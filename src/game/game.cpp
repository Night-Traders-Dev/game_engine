#include "game/game.h"

// ─── Tile hash for deterministic pseudo-random placement ───
static float tile_hash(int x, int y, int seed) {
    int h = (x * 374761393 + y * 668265263 + seed * 1274126177) ^ 0x5bf03635;
    h = ((h >> 13) ^ h) * 1274126177;
    return (float)(h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

// ─── Atlas region helpers ───

void define_tileset_regions(eb::TextureAtlas& atlas) {
    atlas.add_region(126, 96,  63, 53);  atlas.add_region(189, 96,  64, 53);
    atlas.add_region(126, 149, 63, 53);  atlas.add_region(189, 149, 64, 53);
    atlas.add_region(275, 129, 50, 35);  atlas.add_region(325, 129, 48, 35);
    atlas.add_region(275, 164, 50, 38);  atlas.add_region(325, 164, 48, 38);
    atlas.add_region(570, 175, 50, 50);  atlas.add_region(720, 100, 50, 50);
    atlas.add_region(705, 175, 60, 60);  atlas.add_region(570, 100, 55, 55);
    atlas.add_region(830, 100, 55, 55);  atlas.add_region(570, 270, 55, 55);
    atlas.add_region(830, 270, 55, 55);  atlas.add_region(660, 155, 40, 40);
    atlas.add_region(140, 560, 50, 50);  atlas.add_region(200, 530, 50, 50);
    atlas.add_region(250, 510, 60, 50);  atlas.add_region(200, 575, 50, 40);
    atlas.add_region(392, 152, 60, 25);  atlas.add_region(392, 177, 60, 25);
    atlas.add_region(126, 303, 74, 43);  atlas.add_region(200, 303, 74, 43);
    atlas.add_region(126, 346, 74, 43);  atlas.add_region(200, 346, 74, 43);
}

void define_npc_atlas_regions(eb::TextureAtlas& atlas, int cw, int ch) {
    atlas.define_region("idle_down",     0,      0, cw, ch);
    atlas.define_region("walk_down_0",   cw,     0, cw, ch);
    atlas.define_region("walk_down_1",   cw * 2, 0, cw, ch);
    atlas.define_region("idle_up",       0,      ch, cw, ch);
    atlas.define_region("walk_up_0",     cw,     ch, cw, ch);
    atlas.define_region("walk_up_1",     cw * 2, ch, cw, ch);
    atlas.define_region("idle_right",    0,      ch * 2, cw, ch);
    atlas.define_region("walk_right_0",  cw,     ch * 2, cw, ch);
    atlas.define_region("walk_right_1",  cw * 2, ch * 2, cw, ch);
}

// ─── Map generation ───

std::vector<int> generate_town_map(int width, int height) {
    std::vector<int> data(width * height, TILE_GRASS1);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            data[idx] = TILE_GRASS1 + ((x * 7 + y * 13) % 4);
            if (y == 9 || y == 10) data[idx] = TILE_ROAD_H;
            if (y == 8 || y == 11) data[idx] = TILE_SIDEWALK;
            if (x == 15 && y >= 3 && y < 9) data[idx] = TILE_ROAD_V;
            if (x == 15 && (y == 9 || y == 10)) data[idx] = TILE_ROAD_CROSS;
            if ((x == 14 || x == 16) && y >= 3 && y < 9) data[idx] = TILE_SIDEWALK;
            if (x >= 18 && x <= 22 && y >= 5 && y <= 7)
                data[idx] = TILE_DIRT1 + ((x + y) % 4);
            if (x >= 13 && x <= 17 && y >= 5 && y <= 7)
                data[idx] = TILE_STONE1 + ((x + y) % 4);
            if (x >= 22 && x <= 28 && y >= 14 && y <= 18) {
                float dx = x - 25.0f, dy = y - 16.0f;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < 2.5f) data[idx] = TILE_WATER_DEEP;
                else if (d < 3.5f) data[idx] = TILE_WATER_MID;
                else if (d < 4.0f) data[idx] = TILE_SAND;
            }
            if (y == 4 && x >= 5 && x <= 11) data[idx] = TILE_HEDGE1;
            if (y == 4 && x >= 19 && x <= 24) data[idx] = TILE_HEDGE2;
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                data[idx] = TILE_WATER_MID;
        }
    }
    return data;
}

std::vector<int> generate_town_collision(int width, int height) {
    std::vector<int> col(width * height, 0);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                col[y * width + x] = 1;
            if (x >= 22 && x <= 28 && y >= 14 && y <= 18) {
                float dx = x - 25.0f, dy = y - 16.0f;
                if (std::sqrt(dx * dx + dy * dy) < 3.5f) col[y * width + x] = 1;
            }
            if (y == 4 && ((x >= 5 && x <= 11) || (x >= 19 && x <= 24)))
                col[y * width + x] = 1;
        }
    }
    return col;
}

// ─── Object setup ───

void setup_objects(GameState& game, eb::Texture* tileset_tex) {
    struct ObjSrc { eb::Vec2 sp, ss, rs; };
    ObjSrc srcs[] = {
        {{978, 496},  {103, 111}, {80, 86}},
        {{1099, 499}, {102, 114}, {80, 90}},
        {{1331, 501}, {82, 123},  {48, 72}},
        {{979, 622},  {61, 63},   {48, 50}},
        {{1056, 619}, {57, 64},   {44, 50}},
        {{1271, 617}, {44, 44},   {32, 32}},
        {{1019, 97},  {137, 100}, {128, 96}},
        {{1167, 97},  {137, 103}, {128, 96}},
    };
    float tw = static_cast<float>(tileset_tex->width());
    float th = static_cast<float>(tileset_tex->height());
    for (const auto& s : srcs) {
        game.object_defs.push_back({s.sp, s.ss, s.rs});
        eb::AtlasRegion r;
        r.pixel_x = (int)s.sp.x; r.pixel_y = (int)s.sp.y;
        r.pixel_w = (int)s.ss.x; r.pixel_h = (int)s.ss.y;
        r.uv_min = {s.sp.x / tw, s.sp.y / th};
        r.uv_max = {(s.sp.x + s.ss.x) / tw, (s.sp.y + s.ss.y) / th};
        game.object_regions.push_back(r);
    }
    auto place = [&](int id, float x, float y) {
        game.world_objects.push_back({id, {x * 32.0f, y * 32.0f}});
    };
    place(0, 3, 3); place(1, 8, 2); place(2, 27, 3);
    place(3, 5, 14); place(4, 3, 16); place(0, 10, 15);
    place(5, 7, 13); place(5, 12, 17); place(2, 2, 8);
    place(1, 28, 13);
    place(6, 6, 7.5f); place(7, 20, 7.5f);
}

// ─── NPC setup ───

void setup_npcs(GameState& game) {
    NPC bobby;
    bobby.name = "Bobby"; bobby.position = {6.0f * 32.0f, 12.0f * 32.0f};
    bobby.dir = 0; bobby.sprite_atlas_id = 0;
    bobby.dialogue = {
        {"Bobby", "You idjits better be prepared before heading out."},
        {"Bobby", "I've got some supplies if you need 'em."},
        {"Bobby", "Also, watch your back. Something ain't right in this town."},
    };
    game.npcs.push_back(bobby);

    NPC stranger;
    stranger.name = "???"; stranger.position = {15.0f * 32.0f, 16.0f * 32.0f};
    stranger.dir = 1; stranger.sprite_atlas_id = 1;
    stranger.dialogue = {
        {"???", "You shouldn't be poking around here, hunter."},
        {"???", "I can smell it on you... the stench of righteousness."},
    };
    stranger.has_battle = true;
    stranger.battle_enemy_name = "Shapeshifter";
    stranger.battle_enemy_hp = 45; stranger.battle_enemy_atk = 12;
    game.npcs.push_back(stranger);

    NPC vampire;
    vampire.name = "Vampire"; vampire.position = {24.0f * 32.0f, 12.0f * 32.0f};
    vampire.dir = 2; vampire.sprite_atlas_id = 2;
    vampire.dialogue = {
        {"Vampire", "The night is young, hunter..."},
        {"Vampire", "You reek of dead man's blood. Cute."},
    };
    vampire.has_battle = true;
    vampire.battle_enemy_name = "Vampire";
    vampire.battle_enemy_hp = 60; vampire.battle_enemy_atk = 15;
    game.npcs.push_back(vampire);

    NPC azazel;
    azazel.name = "Azazel"; azazel.position = {10.0f * 32.0f, 16.0f * 32.0f};
    azazel.dir = 0; azazel.sprite_atlas_id = 3;
    azazel.dialogue = {
        {"Azazel", "Well, well... the Winchester boys."},
        {"Azazel", "You have no idea what's coming."},
        {"Azazel", "I've got plans for your brother, Dean."},
        {"Dean", "You son of a bitch."},
    };
    azazel.has_battle = true;
    azazel.battle_enemy_name = "Azazel";
    azazel.battle_enemy_hp = 120; azazel.battle_enemy_atk = 22;
    game.npcs.push_back(azazel);
}

// ─── Sprite lookup ───

eb::AtlasRegion get_character_sprite(eb::TextureAtlas& atlas, int dir, bool moving, int frame) {
    bool flip_h = (dir == 2);
    const char* lookup_dir = flip_h ? "right" :
        (dir == 0 ? "down" : dir == 1 ? "up" : "right");
    const eb::AtlasRegion* sr_ptr = nullptr;
    char name[32];
    if (moving) {
        std::snprintf(name, sizeof(name), "walk_%s_%d", lookup_dir, frame);
        sr_ptr = atlas.find_region(name);
    } else {
        std::snprintf(name, sizeof(name), "idle_%s", lookup_dir);
        sr_ptr = atlas.find_region(name);
    }
    auto sr = sr_ptr ? *sr_ptr : atlas.region(0, 0);
    if (flip_h) std::swap(sr.uv_min.x, sr.uv_max.x);
    return sr;
}

// ─── Game initialization ───

bool init_game(GameState& game, eb::Renderer& renderer, eb::ResourceManager& resources,
               float viewport_w, float viewport_h) {
    if (game.initialized) return true;

    try {
        game.white_desc = renderer.default_texture_descriptor();

        // Load tileset
        auto* tileset_tex = resources.load_texture("assets/textures/tileset.png");
        game.tileset_atlas = std::make_unique<eb::TextureAtlas>(tileset_tex);
        define_tileset_regions(*game.tileset_atlas);
        game.tileset_desc = renderer.get_texture_descriptor(*tileset_tex);

        // Load Dean
        auto* dean_tex = resources.load_texture("assets/textures/dean_sprites.png");
        game.dean_atlas = std::make_unique<eb::TextureAtlas>(dean_tex, 158, 210);
        game.dean_atlas->define_region("idle_down",  0*158, 0*210, 158, 210);
        game.dean_atlas->define_region("idle_up",    3*158, 2*210, 158, 210);
        game.dean_atlas->define_region("idle_right", 0*158, 3*210, 158, 210);
        game.dean_atlas->define_region("walk_down_0", 0*158, 0*210, 158, 210);
        game.dean_atlas->define_region("walk_down_1", 2*158, 0*210, 158, 210);
        game.dean_atlas->define_region("walk_up_0", 0*158, 1*210, 158, 210);
        game.dean_atlas->define_region("walk_up_1", 2*158, 1*210, 158, 210);
        game.dean_atlas->define_region("walk_right_0", 3*158, 0*210, 158, 210);
        game.dean_atlas->define_region("walk_right_1", 0*158, 2*210, 158, 210);
        game.dean_desc = renderer.get_texture_descriptor(*dean_tex);

        // Load Sam
        auto* sam_tex = resources.load_texture("assets/textures/sam_sprites.png");
        game.sam_atlas = std::make_unique<eb::TextureAtlas>(sam_tex);
        game.sam_atlas->define_region("idle_down",     44,  92, 140, 190);
        game.sam_atlas->define_region("walk_down_0",   44,  92, 140, 190);
        game.sam_atlas->define_region("walk_down_1",  504,  92, 140, 190);
        game.sam_atlas->define_region("idle_up",      734,  92, 140, 190);
        game.sam_atlas->define_region("walk_up_0",    734,  92, 140, 190);
        game.sam_atlas->define_region("walk_up_1",    275, 350, 140, 190);
        game.sam_atlas->define_region("idle_right",   504, 350, 140, 190);
        game.sam_atlas->define_region("walk_right_0", 504, 350, 140, 190);
        game.sam_atlas->define_region("walk_right_1", 965, 350, 140, 190);
        game.sam_desc = renderer.get_texture_descriptor(*sam_tex);

        // Sam as party member
        PartyMember sam;
        sam.name = "Sam";
        sam.position = {game.player_pos.x, game.player_pos.y + 32.0f};
        sam.dir = 0;
        game.party.push_back(sam);

        // Breadcrumb trail
        game.trail.resize(GameState::TRAIL_SIZE);
        for (auto& r : game.trail) { r.pos = game.player_pos; r.dir = 0; }
        game.trail_head = 0; game.trail_count = 0;

        // Create map
        const int MAP_W = 30, MAP_H = 20, TILE_SZ = 32;
        game.tile_map.create(MAP_W, MAP_H, TILE_SZ);
        game.tile_map.set_tileset(game.tileset_atlas.get());
        game.tile_map.set_tileset_path("assets/textures/tileset.png");
        game.tile_map.add_layer("ground", generate_town_map(MAP_W, MAP_H));
        game.tile_map.set_collision(generate_town_collision(MAP_W, MAP_H));
        game.tile_map.set_animated_tiles(TILE_WATER_DEEP, TILE_WATER_SHORE);

        // Objects & NPCs
        setup_objects(game, tileset_tex);
        setup_npcs(game);

        // Load NPC sprite sheets
        auto load_npc = [&](const char* path, int cw, int ch) {
            auto* tex = resources.load_texture(path);
            auto atlas = std::make_unique<eb::TextureAtlas>(tex);
            define_npc_atlas_regions(*atlas, cw, ch);
            game.npc_descs.push_back(renderer.get_texture_descriptor(*tex));
            game.npc_atlases.push_back(std::move(atlas));
        };
        load_npc("assets/textures/bobby_sprites.png",      123, 174);
        load_npc("assets/textures/stranger_sprites.png",     70, 140);
        load_npc("assets/textures/vampire_sprites.png",     136, 224);
        load_npc("assets/textures/yelloweyes_sprites.png",  134, 187);

        // Dialogue box: background texture and character portraits
        auto* dialog_tex = resources.load_texture("assets/textures/dialog_bg.png");
        if (dialog_tex) {
            game.dialogue.set_background(renderer.get_texture_descriptor(*dialog_tex));
        }
        auto load_portrait = [&](const char* path, const char* speaker) {
            auto* tex = resources.load_texture(path);
            if (tex) game.dialogue.set_portrait(speaker, renderer.get_texture_descriptor(*tex));
        };
        load_portrait("assets/textures/portrait_dean.png", "Dean");
        load_portrait("assets/textures/portrait_sam.png", "Sam");
        load_portrait("assets/textures/portrait_bobby.png", "Bobby");

        // Camera
        game.camera.set_viewport(viewport_w, viewport_h);
        game.camera.set_bounds(0, 0, game.tile_map.world_width(), game.tile_map.world_height());
        game.camera.set_follow_offset(eb::Vec2(0.0f, -viewport_h * 0.1f));
        game.camera.center_on(game.player_pos);

        game.initialized = true;
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Game] Init failed: %s\n", e.what());
        return false;
    }
}

// ─── Battle logic ───

void start_battle(GameState& game, const std::string& enemy, int hp, int atk, bool random, int sprite_id) {
    auto& b = game.battle;
    b.phase = BattlePhase::Intro;
    b.enemy_name = enemy;
    b.enemy_hp_actual = hp; b.enemy_hp_max = hp; b.enemy_atk = atk;
    b.enemy_sprite_id = sprite_id;
    b.player_hp_actual = game.player_hp;
    b.player_hp_max = game.player_hp_max;
    b.player_hp_display = static_cast<float>(game.player_hp);
    b.player_atk = game.player_atk; b.player_def = game.player_def;
    b.menu_selection = 0; b.phase_timer = 0.0f;
    b.message = "A " + enemy + " appeared!";
    b.last_damage = 0; b.random_encounter = random;
}

void update_battle(GameState& game, float dt, bool confirm, bool up, bool down) {
    auto& b = game.battle;
    b.phase_timer += dt;

    if (b.player_hp_display > b.player_hp_actual) {
        b.player_hp_display -= 40.0f * dt;
        if (b.player_hp_display < b.player_hp_actual)
            b.player_hp_display = static_cast<float>(b.player_hp_actual);
    }

    switch (b.phase) {
    case BattlePhase::Intro:
        if (b.phase_timer > 1.5f || confirm) {
            b.phase = BattlePhase::PlayerTurn; b.phase_timer = 0.0f; b.message = "";
        }
        break;
    case BattlePhase::PlayerTurn:
        if (up && b.menu_selection > 0) b.menu_selection--;
        if (down && b.menu_selection < 2) b.menu_selection++;
        if (confirm) {
            b.phase_timer = 0.0f;
            if (b.menu_selection == 0) {
                int damage = b.player_atk + (game.rng() % 5) - 2;
                if (damage < 1) damage = 1;
                b.enemy_hp_actual -= damage; b.last_damage = damage;
                b.message = "Dean attacks! " + std::to_string(damage) + " damage!";
                b.phase = BattlePhase::PlayerAttack;
            } else if (b.menu_selection == 1) {
                int heal = 10 + game.rng() % 8;
                b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
                b.message = "Dean braces! Recovered " + std::to_string(heal) + " HP.";
                b.phase = BattlePhase::PlayerAttack;
            } else {
                if (b.random_encounter && (game.rng() % 3) != 0) {
                    b.message = "Got away safely!";
                    b.phase = BattlePhase::Victory; b.phase_timer = 0.0f;
                } else {
                    b.message = "Can't escape!";
                    b.phase = BattlePhase::PlayerAttack;
                }
            }
        }
        break;
    case BattlePhase::PlayerAttack:
        if (b.phase_timer > 1.2f || confirm) {
            if (b.enemy_hp_actual <= 0) {
                b.phase = BattlePhase::Victory;
                int xp = b.enemy_hp_max / 2 + b.enemy_atk;
                b.message = "Victory! Gained " + std::to_string(xp) + " XP!";
                game.player_xp += xp;
            } else {
                b.phase = BattlePhase::EnemyTurn;
            }
            b.phase_timer = 0.0f;
        }
        break;
    case BattlePhase::EnemyTurn: {
        int damage = b.enemy_atk + (game.rng() % 5) - 2 - b.player_def / 3;
        if (damage < 1) damage = 1;
        b.player_hp_actual -= damage; b.last_damage = damage;
        b.message = b.enemy_name + " attacks! " + std::to_string(damage) + " damage!";
        b.phase = BattlePhase::EnemyAttack; b.phase_timer = 0.0f;
        break;
    }
    case BattlePhase::EnemyAttack:
        if (b.phase_timer > 1.2f || confirm) {
            if (b.player_hp_actual <= 0) {
                b.player_hp_actual = 0;
                b.phase = BattlePhase::Defeat; b.message = "Dean is down!";
            } else {
                b.phase = BattlePhase::PlayerTurn; b.menu_selection = 0; b.message = "";
            }
            b.phase_timer = 0.0f;
        }
        break;
    case BattlePhase::Victory:
        if (b.phase_timer > 2.0f || confirm) {
            game.player_hp = std::max(b.player_hp_actual, 1);
            b.phase = BattlePhase::None;
        }
        break;
    case BattlePhase::Defeat:
        if (b.phase_timer > 2.0f || confirm) {
            game.player_hp = game.player_hp_max / 2;
            b.phase = BattlePhase::None;
        }
        break;
    case BattlePhase::None: break;
    }
}

// ─── Core game update ───

void update_game(GameState& game, const eb::InputState& input, float dt) {
    game.game_time += dt;

    // Battle mode
    if (game.battle.phase != BattlePhase::None) {
        update_battle(game, dt,
                      input.is_pressed(eb::InputAction::Confirm),
                      input.is_pressed(eb::InputAction::MoveUp),
                      input.is_pressed(eb::InputAction::MoveDown));
        return;
    }

    // Dialogue mode
    if (game.dialogue.is_active()) {
        int result = game.dialogue.update(dt,
            input.is_pressed(eb::InputAction::Confirm),
            input.is_pressed(eb::InputAction::MoveUp),
            input.is_pressed(eb::InputAction::MoveDown));
        if (result >= 0 && game.pending_battle_npc >= 0) {
            auto& npc = game.npcs[game.pending_battle_npc];
            start_battle(game, npc.battle_enemy_name,
                         npc.battle_enemy_hp, npc.battle_enemy_atk, false,
                         npc.sprite_atlas_id);
            game.pending_battle_npc = -1;
        }
        return;
    }

    // Player movement
    eb::Vec2 move = {0.0f, 0.0f};
    if (input.is_held(eb::InputAction::MoveUp))    move.y -= 1.0f;
    if (input.is_held(eb::InputAction::MoveDown))  move.y += 1.0f;
    if (input.is_held(eb::InputAction::MoveLeft))  move.x -= 1.0f;
    if (input.is_held(eb::InputAction::MoveRight)) move.x += 1.0f;

    game.player_moving = (move.x != 0.0f || move.y != 0.0f);

    if (game.player_moving) {
        float len = std::sqrt(move.x * move.x + move.y * move.y);
        if (len > 0.0f) { move.x /= len; move.y /= len; }

        float speed = game.player_speed;
        if (input.is_held(eb::InputAction::Run)) speed *= 1.8f;

        float pw = 20.0f, ph = 12.0f;
        float ox = -pw * 0.5f, oy = -ph;

        float new_x = game.player_pos.x + move.x * speed * dt;
        bool bx = game.tile_map.is_solid_world(new_x + ox, game.player_pos.y + oy)
               || game.tile_map.is_solid_world(new_x + ox + pw, game.player_pos.y + oy)
               || game.tile_map.is_solid_world(new_x + ox, game.player_pos.y)
               || game.tile_map.is_solid_world(new_x + ox + pw, game.player_pos.y);
        if (!bx) game.player_pos.x = new_x;

        float new_y = game.player_pos.y + move.y * speed * dt;
        bool by = game.tile_map.is_solid_world(game.player_pos.x + ox, new_y + oy)
               || game.tile_map.is_solid_world(game.player_pos.x + ox + pw, new_y + oy)
               || game.tile_map.is_solid_world(game.player_pos.x + ox, new_y)
               || game.tile_map.is_solid_world(game.player_pos.x + ox + pw, new_y);
        if (!by) game.player_pos.y = new_y;

        if (std::abs(move.x) > std::abs(move.y))
            game.player_dir = (move.x < 0) ? 2 : 3;
        else
            game.player_dir = (move.y < 0) ? 1 : 0;

        game.anim_timer += dt;
        if (game.anim_timer >= 0.2f) {
            game.anim_timer -= 0.2f;
            game.player_frame = 1 - game.player_frame;
        }

        // Breadcrumb trail for party followers
        game.trail_step_accum += speed * dt;
        const float TRAIL_STEP = 4.0f;
        while (game.trail_step_accum >= TRAIL_STEP) {
            game.trail_step_accum -= TRAIL_STEP;
            game.trail[game.trail_head] = {game.player_pos, game.player_dir};
            game.trail_head = (game.trail_head + 1) % GameState::TRAIL_SIZE;
            if (game.trail_count < GameState::TRAIL_SIZE) game.trail_count++;
        }

        // Update party followers
        for (int pi = 0; pi < (int)game.party.size(); pi++) {
            int delay = GameState::FOLLOW_DISTANCE * (pi + 1);
            if (game.trail_count >= delay) {
                int idx = (game.trail_head - delay + GameState::TRAIL_SIZE) % GameState::TRAIL_SIZE;
                auto& pm = game.party[pi];
                auto& target = game.trail[idx];
                float dx = target.pos.x - pm.position.x;
                float dy = target.pos.y - pm.position.y;
                float d = std::sqrt(dx*dx + dy*dy);
                pm.moving = (d > 2.0f);
                if (pm.moving) {
                    float lerp_speed = speed * 1.1f;
                    if (d <= lerp_speed * dt) { pm.position = target.pos; }
                    else { pm.position.x += (dx/d)*lerp_speed*dt; pm.position.y += (dy/d)*lerp_speed*dt; }
                    if (std::abs(dx) > std::abs(dy)) pm.dir = (dx < 0) ? 2 : 3;
                    else pm.dir = (dy < 0) ? 1 : 0;
                    pm.anim_timer += dt;
                    if (pm.anim_timer >= 0.2f) { pm.anim_timer -= 0.2f; pm.frame = 1 - pm.frame; }
                } else {
                    pm.dir = target.dir; pm.frame = 0; pm.anim_timer = 0.0f;
                }
            }
        }

        // Random encounters on grass
        game.steps_since_encounter += speed * dt;
        if (game.steps_since_encounter > 200.0f) {
            game.steps_since_encounter = 0.0f;
            int tx = (int)(game.player_pos.x / 32.0f);
            int ty = (int)(game.player_pos.y / 32.0f);
            int tile = game.tile_map.tile_at(0, tx, ty);
            if (tile >= TILE_GRASS1 && tile <= TILE_GRASS4) {
                if ((game.rng() % 100) < 12) {
                    const char* enemies[] = {"Ghost", "Ghoul", "Demon", "Vengeful Spirit"};
                    int ei = game.rng() % 4;
                    start_battle(game, enemies[ei], 25 + (int)(game.rng() % 30),
                                 8 + (int)(game.rng() % 8), true);
                }
            }
        }
    } else {
        game.player_frame = 0; game.anim_timer = 0.0f;
        for (auto& pm : game.party) { pm.moving = false; pm.frame = 0; pm.anim_timer = 0.0f; }
    }

    // NPC interaction
    if (input.is_pressed(eb::InputAction::Confirm)) {
        for (int i = 0; i < (int)game.npcs.size(); i++) {
            auto& npc = game.npcs[i];
            float dx = game.player_pos.x - npc.position.x;
            float dy = game.player_pos.y - npc.position.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < npc.interact_radius) {
                game.dialogue.start(npc.dialogue);
                game.pending_battle_npc = npc.has_battle ? i : -1;
                break;
            }
        }
    }

    game.camera.follow(game.player_pos, 4.0f);
    game.camera.update(dt);
}

// ─── Grass overlay ───

static void render_grass_overlay(const GameState& game, eb::SpriteBatch& batch, float time) {
    eb::Rect view = game.camera.visible_area();
    float ts = (float)game.tile_map.tile_size();
    int sx = std::max(0, (int)std::floor(view.x / ts));
    int sy = std::max(0, (int)std::floor(view.y / ts));
    int ex = std::min(game.tile_map.width(), (int)std::ceil((view.x + view.w) / ts) + 1);
    int ey = std::min(game.tile_map.height(), (int)std::ceil((view.y + view.h) / ts) + 1);
    batch.set_texture(game.white_desc);
    const float margin = 6.0f;
    for (int ty = sy; ty < ey; ty++) {
        for (int tx = sx; tx < ex; tx++) {
            int tile = game.tile_map.tile_at(0, tx, ty);
            if (tile < TILE_GRASS1 || tile > TILE_GRASS4) continue;
            auto is_grass = [&](int x, int y) {
                if (x < 0 || x >= game.tile_map.width() || y < 0 || y >= game.tile_map.height()) return false;
                int t = game.tile_map.tile_at(0, x, y);
                return t >= TILE_GRASS1 && t <= TILE_GRASS4;
            };
            if (!is_grass(tx-1,ty)||!is_grass(tx+1,ty)||!is_grass(tx,ty-1)||!is_grass(tx,ty+1)) continue;
            float x_min = margin, x_max = ts - margin, y_min = margin, y_max = ts - margin;
            int tuft_count = 2 + (int)(tile_hash(tx, ty, 0) * 2.0f);
            for (int t = 0; t < tuft_count; t++) {
                float fx = tile_hash(tx,ty,t*5+1), fy = tile_hash(tx,ty,t*5+2);
                float base_x = tx*ts + x_min + fx*(x_max-x_min);
                float base_y = ty*ts + y_min + fy*(y_max-y_min);
                float wind = std::sin(time*2.0f + tx*0.7f + ty*0.5f + t*1.3f) * 1.5f;
                int blades = 2 + (int)(tile_hash(tx,ty,t*5+3)*1.5f);
                for (int b = 0; b < blades; b++) {
                    float blade_h = 2.0f + tile_hash(tx,ty,t*5+b+10)*2.0f;
                    float spread = (b-(blades-1)*0.5f)*1.5f;
                    float blade_sway = wind * (0.7f + tile_hash(tx,ty,t*5+b+20)*0.6f);
                    float shade = 0.7f + tile_hash(tx,ty,t*5+b+30)*0.3f;
                    batch.draw_quad({base_x+spread+blade_sway, base_y-blade_h},
                                    {1.0f, blade_h}, {0,0},{1,1},
                                    {0.2f*shade, 0.55f*shade, 0.12f*shade, 0.8f});
                }
            }
        }
    }
}

// ─── Leaf overlay ───

static void render_leaf_overlay(const GameState& game, eb::SpriteBatch& batch, float time) {
    batch.set_texture(game.white_desc);
    for (int oi = 0; oi < (int)game.world_objects.size(); oi++) {
        const auto& obj = game.world_objects[oi];
        if (obj.sprite_id > 4) continue;
        const auto& def = game.object_defs[obj.sprite_id];
        float canopy_w = def.render_size.x * 0.8f, canopy_h = def.render_size.y * 0.5f;
        float canopy_cx = obj.position.x, canopy_cy = obj.position.y - def.render_size.y * 0.7f;
        int leaf_count = 6 + (int)(tile_hash(oi, 0, 99) * 6.0f);
        for (int l = 0; l < leaf_count; l++) {
            float lx_f = tile_hash(oi,l,10)-0.5f, ly_f = tile_hash(oi,l,20)-0.5f;
            float leaf_x = canopy_cx + lx_f*canopy_w, leaf_y = canopy_cy + ly_f*canopy_h;
            float leaf_sz = 2.0f + tile_hash(oi,l,30)*2.5f;
            float wind_phase = time*1.8f + oi*2.1f + l*0.9f;
            float sway_x = std::sin(wind_phase)*2.0f, sway_y = std::cos(wind_phase*0.7f)*1.0f;
            float flutter = std::sin(time*4.0f + l*3.7f);
            if (flutter > 0.7f) { sway_x *= 2.0f; sway_y *= 1.5f; }
            float shade = 0.6f + tile_hash(oi,l,40)*0.4f;
            float r = 0.15f + tile_hash(oi,l,50)*0.15f;
            batch.draw_sorted({leaf_x+sway_x-leaf_sz*0.5f, leaf_y+sway_y-leaf_sz*0.5f},
                              {leaf_sz, leaf_sz}, {0,0},{1,1},
                              obj.position.y - 0.05f, game.white_desc,
                              {r*shade, 0.55f*shade, 0.1f*shade, 0.75f});
        }
    }
}

// ─── Render battle ───

void render_battle(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text, float sw, float sh) {
    auto& b = game.battle;
    float sprite_w = 72.0f, sprite_h = 96.0f; // Battle sprite size

    // Background
    batch.set_texture(game.white_desc);
    batch.draw_quad({0,0},{sw,sh},{0,0},{1,1},{0.02f,0.02f,0.08f,1.0f});

    // ── Enemy sprite (top center, facing down/toward player) ──
    float enemy_cx = sw * 0.5f;
    float enemy_y = 30.0f;

    if (b.enemy_sprite_id >= 0 && b.enemy_sprite_id < (int)game.npc_atlases.size()) {
        auto& atlas = *game.npc_atlases[b.enemy_sprite_id];
        auto desc = game.npc_descs[b.enemy_sprite_id];
        auto sr = get_character_sprite(atlas, 0, false, 0); // dir=0 = facing down (toward us)
        float ew = sprite_w * 1.5f, eh = sprite_h * 1.5f; // Enemy drawn larger
        batch.set_texture(desc);
        batch.draw_quad({enemy_cx - ew * 0.5f, enemy_y}, {ew, eh},
                        sr.uv_min, sr.uv_max);
    }

    // Enemy name + HP (below enemy sprite)
    float ebx = sw * 0.5f - 140.0f, eby = enemy_y + sprite_h * 1.5f + 10.0f;
    batch.set_texture(game.white_desc);
    batch.draw_quad({ebx, eby}, {280, 50}, {0,0},{1,1}, {0.1f, 0.08f, 0.15f, 0.8f});
    text.draw_text(batch, game.font_desc, b.enemy_name, {ebx+10, eby+4}, {1,0.4f,0.4f,1}, 1.0f);

    float hp_pct = std::max(0.0f, (float)b.enemy_hp_actual / b.enemy_hp_max);
    batch.set_texture(game.white_desc);
    batch.draw_quad({ebx+10, eby+28}, {200, 14}, {0,0},{1,1}, {0.2f,0.2f,0.2f,1});
    eb::Vec4 hpc = hp_pct>0.5f ? eb::Vec4{0.2f,0.8f,0.2f,1}
                 : hp_pct>0.25f ? eb::Vec4{0.9f,0.7f,0.1f,1}
                 : eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({ebx+10, eby+28}, {200*hp_pct, 14}, {0,0},{1,1}, hpc);
    text.draw_text(batch, game.font_desc,
                   std::to_string(std::max(0,b.enemy_hp_actual))+"/"+std::to_string(b.enemy_hp_max),
                   {ebx+218, eby+26}, {1,1,1,1}, 0.7f);

    // ── Dean + Sam sprites (bottom center, backs facing us = dir 1 = up) ──
    float party_y = sh * 0.45f;
    float party_cx = sw * 0.5f;

    // Dean (left)
    {
        auto sr = get_character_sprite(*game.dean_atlas, 1, false, 0); // dir=1 = facing up (back to us)
        batch.set_texture(game.dean_desc);
        batch.draw_quad({party_cx - sprite_w - 8.0f, party_y}, {sprite_w, sprite_h},
                        sr.uv_min, sr.uv_max);
    }
    // Sam (right)
    {
        auto sr = get_character_sprite(*game.sam_atlas, 1, false, 0);
        batch.set_texture(game.sam_desc);
        batch.draw_quad({party_cx + 8.0f, party_y}, {sprite_w, sprite_h},
                        sr.uv_min, sr.uv_max);
    }

    // ── Player stats (bottom right) ──
    float pbx = sw - 280, pby = sh - 80;
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx, pby}, {260, 70}, {0,0},{1,1}, {0.08f,0.08f,0.18f,0.9f});
    batch.draw_quad({pbx, pby}, {260, 2}, {0,0},{1,1}, {0.5f,0.5f,0.8f,1});
    batch.draw_quad({pbx, pby+68}, {260, 2}, {0,0},{1,1}, {0.5f,0.5f,0.8f,1});
    text.draw_text(batch, game.font_desc, "Dean  Lv."+std::to_string(game.player_level),
                   {pbx+10, pby+6}, {1,1,1,1}, 0.8f);

    float dhp = b.player_hp_display;
    float ppct = std::max(0.0f, dhp / b.player_hp_max);
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx+10, pby+32}, {180, 12}, {0,0},{1,1}, {0.2f,0.2f,0.2f,1});
    eb::Vec4 ppc = ppct>0.5f ? eb::Vec4{0.2f,0.8f,0.2f,1}
                 : ppct>0.25f ? eb::Vec4{0.9f,0.7f,0.1f,1}
                 : eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({pbx+10, pby+32}, {180*ppct, 12}, {0,0},{1,1}, ppc);
    char hps[32]; std::snprintf(hps, sizeof(hps), "HP %d/%d", (int)std::ceil(dhp), b.player_hp_max);
    text.draw_text(batch, game.font_desc, hps, {pbx+198, pby+30}, {1,1,1,1}, 0.6f);

    // ── Battle menu (player turn, bottom left) ──
    if (b.phase == BattlePhase::PlayerTurn) {
        float mx = 20, my = sh - 120;
        batch.set_texture(game.white_desc);
        batch.draw_quad({mx,my},{180,110},{0,0},{1,1},{0.08f,0.05f,0.15f,0.95f});
        batch.draw_quad({mx,my},{180,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        batch.draw_quad({mx,my+108},{180,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        batch.draw_quad({mx,my},{2,110},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        batch.draw_quad({mx+178,my},{2,110},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        const char* opts[] = {"Attack", "Defend", "Run"};
        for (int i = 0; i < 3; i++) {
            eb::Vec4 c = (i==b.menu_selection) ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{0.8f,0.8f,0.8f,1};
            std::string pfx = (i==b.menu_selection) ? "> " : "  ";
            text.draw_text(batch, game.font_desc, pfx+opts[i],
                           {mx+10, my+10+i*32.0f}, c, 1.0f);
        }
    }

    // ── Message box (center) ──
    if (!b.message.empty()) {
        float mw = sw * 0.6f, mh = 40;
        float mx2 = (sw-mw)*0.5f, my2 = sh*0.42f;
        batch.set_texture(game.white_desc);
        batch.draw_quad({mx2,my2},{mw,mh},{0,0},{1,1},{0.05f,0.05f,0.12f,0.9f});
        batch.draw_quad({mx2,my2},{mw,2},{0,0},{1,1},{0.5f,0.5f,0.7f,1});
        text.draw_text(batch, game.font_desc, b.message,
                       {mx2+12, my2+10}, {1,1,1,1}, 0.9f);
    }
}

// ─── Render HUD ───

static void render_hud(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text) {
    float hx=10, hy=10, hw=200, hh=50;
    batch.set_texture(game.white_desc);
    batch.draw_quad({hx,hy},{hw,hh},{0,0},{1,1},{0.03f,0.03f,0.10f,0.85f});
    batch.draw_quad({hx,hy},{hw,2},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    batch.draw_quad({hx,hy+hh-2},{hw,2},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    batch.draw_quad({hx,hy},{2,hh},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    batch.draw_quad({hx+hw-2,hy},{2,hh},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    char ns[64]; std::snprintf(ns,sizeof(ns),"Dean  Lv.%d",game.player_level);
    text.draw_text(batch,game.font_desc,ns,{hx+10,hy+6},{1,1,1,1},0.8f);
    float hp_pct=std::max(0.0f,(float)game.player_hp/game.player_hp_max);
    float bx=hx+10, by=hy+28, bw=130, bh=12;
    batch.set_texture(game.white_desc);
    batch.draw_quad({bx,by},{bw,bh},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 hc=hp_pct>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:hp_pct>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({bx,by},{bw*hp_pct,bh},{0,0},{1,1},hc);
    char hs[32]; std::snprintf(hs,sizeof(hs),"%d/%d",game.player_hp,game.player_hp_max);
    text.draw_text(batch,game.font_desc,hs,{bx+bw+6,by-1},{1,1,1,1},0.6f);
}

// ─── Render world ───

void render_game_world(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text) {
    batch.set_projection(game.camera.projection_matrix());

    // Tile map with water animation
    batch.set_texture(game.tileset_desc);
    game.tile_map.render(batch, game.camera, game.game_time);

    // Grass overlay
    render_grass_overlay(game, batch, game.game_time);

    // Y-sorted objects
    for (const auto& obj : game.world_objects) {
        const auto& def = game.object_defs[obj.sprite_id];
        const auto& region = game.object_regions[obj.sprite_id];
        eb::Vec2 dp = {obj.position.x - def.render_size.x*0.5f, obj.position.y - def.render_size.y};
        batch.draw_sorted(dp, def.render_size, region.uv_min, region.uv_max,
                         obj.position.y, game.tileset_desc);
    }

    // Leaf overlay
    render_leaf_overlay(game, batch, game.game_time);

    // NPCs
    for (const auto& npc : game.npcs) {
        if (npc.sprite_atlas_id >= 0 && npc.sprite_atlas_id < (int)game.npc_atlases.size()) {
            auto& atlas = *game.npc_atlases[npc.sprite_atlas_id];
            auto desc = game.npc_descs[npc.sprite_atlas_id];
            auto sr = get_character_sprite(atlas, npc.dir, false, 0);
            float rw=48, rh=64;
            eb::Vec2 dp = {npc.position.x-rw*0.5f, npc.position.y-rh+4};
            batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, npc.position.y, desc);
        }
    }

    // Party (Sam)
    for (int pi = 0; pi < (int)game.party.size(); pi++) {
        auto& pm = game.party[pi];
        auto sr = get_character_sprite(*game.sam_atlas, pm.dir, pm.moving, pm.frame);
        float rw=48, rh=64;
        float bob = pm.moving ? std::sin(pm.anim_timer*15.0f)*2.0f : 0.0f;
        eb::Vec2 dp = {pm.position.x-rw*0.5f, pm.position.y-rh+4+bob};
        batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, pm.position.y, game.sam_desc);
    }

    // Player (Dean)
    {
        auto sr = get_character_sprite(*game.dean_atlas, game.player_dir, game.player_moving, game.player_frame);
        float rw=48, rh=64;
        float bob = game.player_moving ? std::sin(game.anim_timer*15.0f)*2.0f : 0.0f;
        eb::Vec2 dp = {game.player_pos.x-rw*0.5f, game.player_pos.y-rh+4+bob};
        batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, game.player_pos.y, game.dean_desc);
    }

    batch.flush_sorted();
    batch.flush();
}

// ─── Render UI overlay ───

void render_game_ui(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                    eb::Mat4 screen_proj, float sw, float sh) {
    batch.set_projection(screen_proj);

    // NPC labels
    for (const auto& npc : game.npcs) {
        float dx = game.player_pos.x - npc.position.x;
        float dy = game.player_pos.y - npc.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < npc.interact_radius * 1.5f) {
            eb::Vec2 cam_off = game.camera.offset();
            float sx = npc.position.x + cam_off.x;
            float sy = npc.position.y + cam_off.y - 100.0f;
            float ns = 0.7f;
            auto name_size = text.measure_text(npc.name, ns);
            float lp = 6.0f;
            batch.set_texture(game.white_desc);
            batch.draw_quad({sx-name_size.x*0.5f-lp, sy-lp*0.5f},
                {name_size.x+lp*2, name_size.y+lp}, {0,0},{1,1},{0,0,0,0.6f});
            text.draw_text(batch, game.font_desc, npc.name,
                {sx-name_size.x*0.5f, sy}, {1,1,0.4f,1}, ns);
            if (dist < npc.interact_radius) {
                const char* hint_text =
#ifdef __ANDROID__
                    "[A] Talk";
#else
                    "[Z] Talk";
#endif
                float hs = 0.5f;
                auto hint_size = text.measure_text(hint_text, hs);
                float hy = sy + name_size.y + 8.0f;
                batch.set_texture(game.white_desc);
                batch.draw_quad({sx-hint_size.x*0.5f-4,hy-2},
                    {hint_size.x+8,hint_size.y+4},{0,0},{1,1},{0,0,0,0.5f});
                text.draw_text(batch, game.font_desc, hint_text,
                    {sx-hint_size.x*0.5f, hy}, {0.8f,0.8f,0.8f,0.9f}, hs);
            }
        }
    }

    // HUD
    render_hud(game, batch, text);

    // Dialogue
    if (game.dialogue.is_active()) {
        game.dialogue.render(batch, text, game.font_desc, game.white_desc, sw, sh);
    }
}
