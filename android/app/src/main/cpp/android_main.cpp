#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include "engine/core/engine.h"
#include "engine/platform/platform_android.h"
#include "engine/graphics/renderer.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/texture.h"
#include "engine/graphics/texture_atlas.h"
#include "engine/resource/resource_manager.h"
#include "engine/resource/file_io.h"
#include "game/overworld/camera.h"
#include "game/overworld/tile_map.h"

#include <memory>
#include <cmath>
#include <vector>

#define LOG_TAG "TWEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Tile IDs (must match desktop main.cpp)
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

    std::vector<eb::AtlasRegion> object_regions;
    std::vector<ObjectDef> object_defs;
    std::vector<WorldObject> world_objects;

    eb::Vec2 player_pos = {15.0f * 32.0f, 10.0f * 32.0f};
    float player_speed = 120.0f;
    int player_dir = 0;
    int player_frame = 0;
    float anim_timer = 0.0f;
    bool player_moving = false;
    bool initialized = false;
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

struct AppState {
    struct android_app* app;
    std::unique_ptr<eb::PlatformAndroid> platform;
    std::unique_ptr<eb::Renderer> renderer;
    std::unique_ptr<eb::ResourceManager> resources;
    eb::Timer timer;
    GameState game;
    bool has_focus = false;
    bool has_window = false;
    bool running = true;

    // Vulkan descriptor sets
    VkDescriptorSet tileset_desc = VK_NULL_HANDLE;
    VkDescriptorSet dean_desc = VK_NULL_HANDLE;
};

static bool init_game(AppState& state) {
    if (state.game.initialized) return true;
    if (!state.renderer || !state.resources) return false;

    LOGI("Initializing game...");

    try {
        // Load tileset
        auto* tileset_tex = state.resources->load_texture("assets/textures/tileset.png");
        if (!tileset_tex) {
            LOGE("Failed to load tileset.png");
            return false;
        }
        state.game.tileset_atlas = std::make_unique<eb::TextureAtlas>(tileset_tex);
        define_tileset_regions(*state.game.tileset_atlas);

        // Load Dean sprite sheet
        auto* dean_tex = state.resources->load_texture("assets/textures/dean_sprites.png");
        if (!dean_tex) {
            LOGE("Failed to load dean_sprites.png");
            return false;
        }
        state.game.dean_atlas = std::make_unique<eb::TextureAtlas>(dean_tex, 158, 210);

        // Create map
        const int MAP_W = 30, MAP_H = 20, TILE_SZ = 32;
        state.game.tile_map.create(MAP_W, MAP_H, TILE_SZ);
        state.game.tile_map.set_tileset(state.game.tileset_atlas.get());
        state.game.tile_map.add_layer("ground", generate_town_map(MAP_W, MAP_H));
        state.game.tile_map.set_collision(generate_town_collision(MAP_W, MAP_H));

        // Objects
        setup_objects(state.game, tileset_tex);

        // Camera
        float sw = static_cast<float>(state.platform->get_width());
        float sh = static_cast<float>(state.platform->get_height());
        state.game.camera.set_viewport(sw, sh);
        state.game.camera.set_bounds(0, 0,
            state.game.tile_map.world_width(),
            state.game.tile_map.world_height());
        state.game.camera.set_follow_offset({0.0f, -sh * 0.1f});
        state.game.camera.center_on(state.game.player_pos);

        // Descriptor sets for textures
        state.tileset_desc = state.renderer->get_texture_descriptor(*tileset_tex);
        state.dean_desc = state.renderer->get_texture_descriptor(*dean_tex);

        state.game.initialized = true;
        LOGI("Game initialized successfully");
        return true;

    } catch (const std::exception& e) {
        LOGE("Game init failed: %s", e.what());
        return false;
    }
}

static void update_game(AppState& state, float dt) {
    auto& input = state.platform->input();
    auto& game = state.game;

    // Player movement from touch controls
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

        eb::Vec2 new_pos = game.player_pos;
        new_pos.x += move.x * speed * dt;
        new_pos.y += move.y * speed * dt;

        // Collision check
        float pw = 20.0f, ph = 12.0f;
        float ox = -pw * 0.5f, oy = -ph;
        bool blocked = false;
        blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox, new_pos.y + oy);
        blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox + pw, new_pos.y + oy);
        blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox, new_pos.y);
        blocked = blocked || game.tile_map.is_solid_world(new_pos.x + ox + pw, new_pos.y);

        if (!blocked) game.player_pos = new_pos;

        // Direction
        if (std::abs(move.x) > std::abs(move.y))
            game.player_dir = (move.x < 0) ? 2 : 3;
        else
            game.player_dir = (move.y < 0) ? 1 : 0;

        // Animation
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

static void render_game(AppState& state) {
    auto& batch = state.renderer->sprite_batch();
    auto& game = state.game;

    batch.set_projection(game.camera.projection_matrix());

    // Tile map
    batch.set_texture(state.tileset_desc);
    game.tile_map.render(batch, game.camera);

    // Y-sorted objects
    for (const auto& obj : game.world_objects) {
        const auto& def = game.object_defs[obj.sprite_id];
        const auto& region = game.object_regions[obj.sprite_id];
        eb::Vec2 draw_pos = {
            obj.position.x - def.render_size.x * 0.5f,
            obj.position.y - def.render_size.y
        };
        batch.draw_sorted(draw_pos, def.render_size,
                         region.uv_min, region.uv_max,
                         obj.position.y, state.tileset_desc);
    }

    // Player sprite
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
                     game.player_pos.y, state.dean_desc);

    // Flush sorted sprites before drawing UI overlay
    batch.flush_sorted();
    batch.flush();

    // Draw touch controls overlay on top
    batch.set_texture(state.renderer->default_texture_descriptor());
    state.platform->touch_controls().render(batch,
        state.platform->get_width(), state.platform->get_height());
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    auto* state = static_cast<AppState*>(app->userData);
    if (state && state->platform) {
        return state->platform->handle_input(event);
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    auto* state = static_cast<AppState*>(app->userData);
    if (!state) return;

    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            if (app->window) {
                state->has_window = true;
                if (!state->platform) {
                    state->platform = std::make_unique<eb::PlatformAndroid>(app->window);
                    try {
                        state->renderer = std::make_unique<eb::Renderer>(*state->platform, true);
                        state->renderer->set_shader_dir("shaders/");
                        state->renderer->set_clear_color(0.05f, 0.05f, 0.12f);
                        state->resources = std::make_unique<eb::ResourceManager>(
                            state->renderer->vulkan_context());
                        LOGI("Renderer initialized");
                    } catch (const std::exception& e) {
                        LOGE("Failed to initialize renderer: %s", e.what());
                        state->running = false;
                    }
                } else {
                    state->platform->set_window(app->window);
                }
            }
            break;

        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            state->has_window = false;
            state->game.initialized = false;
            state->game.tileset_atlas.reset();
            state->game.dean_atlas.reset();
            state->resources.reset();
            if (state->renderer) {
                state->renderer->vulkan_context().wait_idle();
            }
            state->renderer.reset();
            break;

        case APP_CMD_GAINED_FOCUS:
            state->has_focus = true;
            break;

        case APP_CMD_LOST_FOCUS:
            state->has_focus = false;
            break;

        case APP_CMD_DESTROY:
            state->running = false;
            break;

        case APP_CMD_CONFIG_CHANGED:
            if (state->platform && app->window) {
                int w = ANativeWindow_getWidth(app->window);
                int h = ANativeWindow_getHeight(app->window);
                state->platform->handle_resize(w, h);
                if (state->game.initialized) {
                    state->game.camera.set_viewport(
                        static_cast<float>(w), static_cast<float>(h));
                }
            }
            break;
    }
}

void android_main(struct android_app* app) {
    LOGI("android_main started");

    // Set up asset manager for file I/O before anything else
    eb::FileIO::set_asset_manager(app->activity->assetManager);

    AppState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    state.timer.tick();

    while (state.running) {
        int events;
        struct android_poll_source* source;

        int timeout = state.has_focus ? 0 : -1;
        int result;
        while ((result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&source))) >= 0) {
            if (source) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                state.running = false;
                break;
            }
            timeout = 0;
        }

        if (!state.running) break;

        if (state.has_focus && state.has_window && state.renderer) {
            // Lazy-init game assets
            if (!state.game.initialized) {
                init_game(state);
            }

            float dt = state.timer.tick();
            state.platform->poll_events();

            // Menu/back button exits
            if (state.platform->input().is_pressed(eb::InputAction::Menu)) {
                ANativeActivity_finish(app->activity);
            }

            if (state.game.initialized) {
                update_game(state, dt);

                if (state.renderer->begin_frame()) {
                    render_game(state);
                    state.renderer->end_frame();
                }
            }
        }
    }

    state.resources.reset();
    state.renderer.reset();
    state.platform.reset();

    LOGI("android_main exiting");
}
