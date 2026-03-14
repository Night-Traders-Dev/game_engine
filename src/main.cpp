#include "engine/core/engine.h"
#include "engine/graphics/renderer.h"
#include "engine/graphics/texture.h"
#include "engine/graphics/texture_atlas.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/resource/resource_manager.h"
#include "engine/platform/platform.h"
#include "engine/platform/input.h"
#include "game/overworld/camera.h"
#include "game/overworld/tile_map.h"
#include "editor/tile_editor.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
#include <cmath>
#include <sys/stat.h>

// Tile IDs (1-indexed for TileMap, 0 = empty)
enum Tile : int {
    TILE_EMPTY = 0,
    TILE_GRASS1 = 1, TILE_GRASS2, TILE_GRASS3, TILE_GRASS4,
    TILE_DIRT1, TILE_DIRT2, TILE_DIRT3, TILE_DIRT4,
    TILE_ROAD_H, TILE_ROAD_V, TILE_ROAD_CROSS,
    TILE_ROAD_TL, TILE_ROAD_TR, TILE_ROAD_BL, TILE_ROAD_BR,
    TILE_SIDEWALK,
    TILE_WATER_DEEP, TILE_WATER_MID, TILE_WATER_SHORE, TILE_SAND,
    TILE_HEDGE1, TILE_HEDGE2,
    TILE_STONE1, TILE_STONE2, TILE_STONE3, TILE_STONE4,
    TILE_COUNT
};

struct WorldObject {
    int sprite_id;
    eb::Vec2 position;
};

struct ObjectDef {
    eb::Vec2 src_pos, src_size, render_size;
};

struct GameState {
    eb::Camera camera;
    eb::TileMap tile_map;
    std::unique_ptr<eb::TextureAtlas> tileset_atlas;
    std::unique_ptr<eb::TextureAtlas> dean_atlas;
    eb::TileEditor editor;

    std::vector<eb::AtlasRegion> object_regions;
    std::vector<ObjectDef> object_defs;
    std::vector<WorldObject> world_objects;

    eb::Vec2 player_pos = {15.0f * 32.0f, 10.0f * 32.0f};
    float player_speed = 120.0f;
    int player_dir = 0;
    int player_frame = 0;
    float anim_timer = 0.0f;
    bool player_moving = false;
};

static void define_tileset_regions(eb::TextureAtlas& atlas) {
    // Grass
    atlas.add_region(126, 96,  63, 53);
    atlas.add_region(189, 96,  64, 53);
    atlas.add_region(126, 149, 63, 53);
    atlas.add_region(189, 149, 64, 53);
    // Dirt
    atlas.add_region(275, 129, 50, 35);
    atlas.add_region(325, 129, 48, 35);
    atlas.add_region(275, 164, 50, 38);
    atlas.add_region(325, 164, 48, 38);
    // Roads
    atlas.add_region(570, 175, 50, 50);
    atlas.add_region(720, 100, 50, 50);
    atlas.add_region(705, 175, 60, 60);
    atlas.add_region(570, 100, 55, 55);
    atlas.add_region(830, 100, 55, 55);
    atlas.add_region(570, 270, 55, 55);
    atlas.add_region(830, 270, 55, 55);
    atlas.add_region(660, 155, 40, 40);
    // Water & beach
    atlas.add_region(140, 560, 50, 50);
    atlas.add_region(200, 530, 50, 50);
    atlas.add_region(250, 510, 60, 50);
    atlas.add_region(200, 575, 50, 40);
    // Hedges
    atlas.add_region(392, 152, 60, 25);
    atlas.add_region(392, 177, 60, 25);
    // Stone path
    atlas.add_region(126, 303, 74, 43);
    atlas.add_region(200, 303, 74, 43);
    atlas.add_region(126, 346, 74, 43);
    atlas.add_region(200, 346, 74, 43);
}

static std::vector<int> generate_town_map(int width, int height) {
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

static std::vector<int> generate_town_collision(int width, int height) {
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

static void setup_objects(GameState& game, eb::Texture* tileset_tex) {
    struct ObjSrc { eb::Vec2 sp, ss, rs; };
    ObjSrc srcs[] = {
        {{978, 496},  {103, 111}, {80, 86}},   // tree large 1
        {{1099, 499}, {102, 114}, {80, 90}},   // tree large 2
        {{1331, 501}, {82, 123},  {48, 72}},   // pine
        {{979, 622},  {61, 63},   {48, 50}},   // small 1
        {{1056, 619}, {57, 64},   {44, 50}},   // small 2
        {{1271, 617}, {44, 44},   {32, 32}},   // bush
        {{1019, 97},  {137, 100}, {128, 96}},  // shop
        {{1167, 97},  {137, 103}, {128, 96}},  // motel
    };

    float tw = static_cast<float>(tileset_tex->width());
    float th = static_cast<float>(tileset_tex->height());

    for (const auto& s : srcs) {
        game.object_defs.push_back({s.sp, s.ss, s.rs});
        eb::AtlasRegion r;
        r.pixel_x = static_cast<int>(s.sp.x);
        r.pixel_y = static_cast<int>(s.sp.y);
        r.pixel_w = static_cast<int>(s.ss.x);
        r.pixel_h = static_cast<int>(s.ss.y);
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

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        eb::EngineConfig config;
        config.title = "Twilight Engine - Town Demo (Tab = Editor)";
        config.width = 960;
        config.height = 720;
        config.vsync = true;

        eb::Engine engine(config);
        engine.renderer().set_shader_dir("shaders/");
        engine.renderer().set_clear_color(0.05f, 0.05f, 0.12f);

        GameState game;

        // Load tileset
        auto* tileset_tex = engine.resources().load_texture("assets/textures/tileset.png");
        game.tileset_atlas = std::make_unique<eb::TextureAtlas>(tileset_tex);
        define_tileset_regions(*game.tileset_atlas);

        // Load Dean sprite sheet
        auto* dean_tex = engine.resources().load_texture("assets/textures/dean_sprites.png");
        game.dean_atlas = std::make_unique<eb::TextureAtlas>(dean_tex, 158, 210);

        // Create map
        const int MAP_W = 30, MAP_H = 20, TILE_SZ = 32;
        game.tile_map.create(MAP_W, MAP_H, TILE_SZ);
        game.tile_map.set_tileset(game.tileset_atlas.get());
        game.tile_map.set_tileset_path("assets/textures/tileset.png");
        game.tile_map.add_layer("ground", generate_town_map(MAP_W, MAP_H));
        game.tile_map.set_collision(generate_town_collision(MAP_W, MAP_H));

        // Objects
        setup_objects(game, tileset_tex);

        // Camera
        game.camera.set_viewport(static_cast<float>(config.width),
                                  static_cast<float>(config.height));
        game.camera.set_bounds(0, 0, game.tile_map.world_width(), game.tile_map.world_height());
        game.camera.set_follow_offset({0.0f, -config.height * 0.1f});
        game.camera.center_on(game.player_pos);

        // Descriptor sets
        VkDescriptorSet tileset_desc = engine.renderer().get_texture_descriptor(*tileset_tex);
        VkDescriptorSet dean_desc = engine.renderer().get_texture_descriptor(*dean_tex);

        // Setup editor
        game.editor.set_map(&game.tile_map);
        game.editor.set_tileset(game.tileset_atlas.get(), tileset_desc);

        // Create maps directory
#ifdef _WIN32
        mkdir("assets/maps");
#else
        mkdir("assets/maps", 0755);
#endif

        // --- Update ---
        engine.on_update = [&](float dt) {
            auto& input = engine.platform().input();

            // Tab toggles editor
            if (input.key_pressed(GLFW_KEY_TAB)) {
                game.editor.toggle();
                if (game.editor.is_active()) {
                    // Enter editor: stop following, clear bounds for free camera
                    game.camera.clear_bounds();
                } else {
                    // Exit editor: restore game camera
                    game.camera.set_bounds(0, 0,
                        game.tile_map.world_width(), game.tile_map.world_height());
                    game.camera.set_follow_offset({0.0f, -config.height * 0.1f});
                    game.camera.follow(game.player_pos, 100.0f);
                }
            }

            // ESC quits only in game mode; in editor it clears selection
            if (input.is_pressed(eb::InputAction::Menu) && !game.editor.is_active()) {
                engine.quit();
                return;
            }

            if (game.editor.is_active()) {
                game.editor.update(input, game.camera, dt,
                                   engine.platform().get_width(),
                                   engine.platform().get_height());
                game.camera.update(dt);
            } else {
                // Game mode: player movement
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

                    eb::Vec2 new_pos = game.player_pos;
                    new_pos.x += move.x * speed * dt;
                    new_pos.y += move.y * speed * dt;

                    float pw = 20.0f, ph = 12.0f;
                    float ox = -pw * 0.5f, oy = -ph;
                    bool blocked = false;
                    blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox, new_pos.y + oy);
                    blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox + pw, new_pos.y + oy);
                    blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox, new_pos.y);
                    blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox + pw, new_pos.y);

                    if (!blocked) game.player_pos = new_pos;

                    if (std::abs(move.x) > std::abs(move.y))
                        game.player_dir = (move.x < 0) ? 2 : 3;
                    else
                        game.player_dir = (move.y < 0) ? 1 : 0;

                    game.anim_timer += dt;
                    if (game.anim_timer >= 0.2f) {
                        game.anim_timer -= 0.2f;
                        game.player_frame = 1 - game.player_frame;
                    }
                } else {
                    game.player_frame = 0;
                    game.anim_timer = 0.0f;
                }

                game.camera.follow(game.player_pos, 4.0f);
                game.camera.update(dt);
            }
        };

        // --- Render ---
        engine.on_render = [&]() {
            auto& batch = engine.renderer().sprite_batch();

            batch.set_projection(game.camera.projection_matrix());

            // Tile map
            batch.set_texture(tileset_desc);
            game.tile_map.render(batch, game.camera);

            // Y-sorted objects + player
            for (const auto& obj : game.world_objects) {
                const auto& def = game.object_defs[obj.sprite_id];
                const auto& region = game.object_regions[obj.sprite_id];
                eb::Vec2 draw_pos = {
                    obj.position.x - def.render_size.x * 0.5f,
                    obj.position.y - def.render_size.y
                };
                batch.draw_sorted(draw_pos, def.render_size,
                                 region.uv_min, region.uv_max,
                                 obj.position.y, tileset_desc);
            }

            if (!game.editor.is_active()) {
                int col = 0, row = 0;
                if (game.player_moving) {
                    switch (game.player_dir) {
                        case 0: row = 0; col = game.player_frame;     break;
                        case 1: row = 0; col = 2 + game.player_frame; break;
                        case 2: row = 1; col = game.player_frame;     break;
                        case 3: row = 1; col = 2 + game.player_frame; break;
                    }
                } else {
                    row = 2; col = game.player_dir;
                }
                auto sr = game.dean_atlas->region(col, row);
                float rw = 48.0f, rh = 64.0f;
                eb::Vec2 dp = {game.player_pos.x - rw * 0.5f,
                               game.player_pos.y - rh + 4.0f};
                batch.draw_sorted(dp, {rw, rh}, sr.uv_min, sr.uv_max,
                                 game.player_pos.y, dean_desc);
            }

            // Editor overlay (drawn last, on top)
            if (game.editor.is_active()) {
                // Flush sorted sprites first so editor draws on top
                batch.flush_sorted();
                batch.flush();

                // Editor needs to use the default (white) texture for colored quads
                batch.set_texture(engine.renderer().default_texture_descriptor());

                game.editor.render(batch, game.camera, tileset_desc,
                                   engine.platform().get_width(),
                                   engine.platform().get_height());
            }
        };

        std::printf("[Main] Starting engine... (Tab = toggle editor)\n");
        std::printf("[Editor] Controls:\n");
        std::printf("  Tab       - Toggle editor\n");
        std::printf("  P/E/F/I/R/C - Paint/Erase/Fill/Eyedrop/Select/Collision tool\n");
        std::printf("  G         - Toggle grid\n");
        std::printf("  V         - Toggle collision overlay\n");
        std::printf("  1-9       - Switch layer\n");
        std::printf("  LClick    - Use tool / select palette tile\n");
        std::printf("  RClick    - Erase tile\n");
        std::printf("  MidDrag   - Pan camera\n");
        std::printf("  Ctrl+C/V  - Copy/Paste selection\n");
        std::printf("  Ctrl+S    - Save map\n");
        std::printf("  Ctrl+L    - Load map\n");
        std::printf("  Delete    - Clear selection\n");
        engine.run();

    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Fatal] %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
