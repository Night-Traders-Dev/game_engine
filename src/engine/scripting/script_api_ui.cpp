#include "engine/scripting/script_engine.h"
#include "game/game.h"
#include "engine/graphics/renderer.h"
#include "engine/core/debug_log.h"
#include "engine/resource/file_io.h"

extern "C" {
#include "interpreter.h"
#include "env.h"
#include "value.h"
}

#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <string>

namespace eb {
extern ScriptEngine* s_active_engine;

// =============== UI API ===============

// ui_label(id, text, x, y, r, g, b, a)
static Value native_ui_label(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string text = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float x = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float y = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float r=1,g=1,b=1,a=1;
    if (argc > 4 && args[4].type == VAL_NUMBER) r = (float)args[4].as.number;
    if (argc > 5 && args[5].type == VAL_NUMBER) g = (float)args[5].as.number;
    if (argc > 6 && args[6].type == VAL_NUMBER) b = (float)args[6].as.number;
    if (argc > 7 && args[7].type == VAL_NUMBER) a = (float)args[7].as.number;
    for (auto& l : gs->script_ui.labels) {
        if (l.id == id) { l.text = text; l.position = {x,y}; l.color = {r,g,b,a}; return val_nil(); }
    }
    gs->script_ui.labels.push_back({id, text, {x,y}, {r,g,b,a}});
    return val_nil();
}

// ui_bar(id, value, max, x, y, w, h, r, g, b, a)
static Value native_ui_bar(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float val = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float mx = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 100;
    float x = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float y = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float w = (argc > 5 && args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 100;
    float h = (argc > 6 && args[6].type == VAL_NUMBER) ? (float)args[6].as.number : 12;
    float r=0.2f,g=0.8f,b=0.2f,a=1;
    if (argc > 7 && args[7].type == VAL_NUMBER) r = (float)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_NUMBER) g = (float)args[8].as.number;
    if (argc > 9 && args[9].type == VAL_NUMBER) b = (float)args[9].as.number;
    if (argc > 10 && args[10].type == VAL_NUMBER) a = (float)args[10].as.number;
    for (auto& bar : gs->script_ui.bars) {
        if (bar.id == id) { bar.value=val; bar.max_value=mx; bar.position={x,y}; bar.width=w; bar.height=h; bar.color={r,g,b,a}; return val_nil(); }
    }
    gs->script_ui.bars.push_back({id, val, mx, {x,y}, w, h, {r,g,b,a}});
    return val_nil();
}

// ui_remove(id)
static Value native_ui_remove(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto& labels = gs->script_ui.labels;
    labels.erase(std::remove_if(labels.begin(), labels.end(), [&](auto& l){return l.id==id;}), labels.end());
    auto& bars = gs->script_ui.bars;
    bars.erase(std::remove_if(bars.begin(), bars.end(), [&](auto& b){return b.id==id;}), bars.end());
    auto& panels = gs->script_ui.panels;
    panels.erase(std::remove_if(panels.begin(), panels.end(), [&](auto& p){return p.id==id;}), panels.end());
    auto& images = gs->script_ui.images;
    images.erase(std::remove_if(images.begin(), images.end(), [&](auto& i){return i.id==id;}), images.end());
    return val_nil();
}

// ui_notify(text, duration)
static Value native_ui_notify(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string text = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float dur = (argc > 1 && args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 3.0f;
    s_active_engine->game_state_->script_ui.notifications.push_back({text, dur, 0.0f});
    return val_nil();
}

// ui_panel(id, x, y, w, h, sprite_region)
static Value native_ui_panel(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float w = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 100;
    float h = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 60;
    std::string region = (argc > 5 && args[5].type == VAL_STRING) ? args[5].as.string : "panel_hud_wide";
    for (auto& p : gs->script_ui.panels) {
        if (p.id == id) { p.position={x,y}; p.width=w; p.height=h; p.sprite_region=region; return val_nil(); }
    }
    gs->script_ui.panels.push_back({id, {x,y}, w, h, region});
    return val_nil();
}

// ui_image(id, x, y, w, h, icon_name)
static Value native_ui_image(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 6) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float w = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 32;
    float h = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 32;
    std::string icon = (args[5].type == VAL_STRING) ? args[5].as.string : "";
    for (auto& img : gs->script_ui.images) {
        if (img.id == id) { img.position={x,y}; img.width=w; img.height=h; img.icon_name=icon; return val_nil(); }
    }
    gs->script_ui.images.push_back({id, {x,y}, w, h, icon});
    return val_nil();
}

// ui_set(id, property, value) — modify any UI component's properties
static Value native_ui_set(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* prop = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float nv = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    bool bv = (args[2].type == VAL_BOOL) ? args[2].as.boolean : (nv != 0);
    const char* sv = (args[2].type == VAL_STRING) ? args[2].as.string : "";

    // Search labels
    for (auto& l : gs->script_ui.labels) {
        if (l.id != id) continue;
        if (std::strcmp(prop, "x") == 0) l.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) l.position.y = nv;
        else if (std::strcmp(prop, "scale") == 0) l.scale = nv;
        else if (std::strcmp(prop, "text") == 0) l.text = sv;
        else if (std::strcmp(prop, "visible") == 0) l.visible = bv;
        else if (std::strcmp(prop, "r") == 0) l.color.x = nv;
        else if (std::strcmp(prop, "g") == 0) l.color.y = nv;
        else if (std::strcmp(prop, "b") == 0) l.color.z = nv;
        else if (std::strcmp(prop, "a") == 0) l.color.w = nv;
        else if (std::strcmp(prop, "opacity") == 0) l.opacity = nv;
        else if (std::strcmp(prop, "rotation") == 0) l.rotation = nv;
        else if (std::strcmp(prop, "layer") == 0) l.layer = (int)nv;
        else if (std::strcmp(prop, "on_click") == 0) l.on_click = sv;
        return val_nil();
    }
    // Search bars
    for (auto& b : gs->script_ui.bars) {
        if (b.id != id) continue;
        if (std::strcmp(prop, "x") == 0) b.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) b.position.y = nv;
        else if (std::strcmp(prop, "w") == 0) b.width = nv;
        else if (std::strcmp(prop, "h") == 0) b.height = nv;
        else if (std::strcmp(prop, "value") == 0) b.value = nv;
        else if (std::strcmp(prop, "max") == 0) b.max_value = nv;
        else if (std::strcmp(prop, "visible") == 0) b.visible = bv;
        else if (std::strcmp(prop, "r") == 0) b.color.x = nv;
        else if (std::strcmp(prop, "g") == 0) b.color.y = nv;
        else if (std::strcmp(prop, "b") == 0) b.color.z = nv;
        else if (std::strcmp(prop, "a") == 0) b.color.w = nv;
        else if (std::strcmp(prop, "rotation") == 0) b.rotation = nv;
        else if (std::strcmp(prop, "opacity") == 0) b.opacity = nv;
        else if (std::strcmp(prop, "layer") == 0) b.layer = (int)nv;
        else if (std::strcmp(prop, "show_text") == 0) b.show_text = bv;
        else if (std::strcmp(prop, "bg_r") == 0) b.bg_color.x = nv;
        else if (std::strcmp(prop, "bg_g") == 0) b.bg_color.y = nv;
        else if (std::strcmp(prop, "bg_b") == 0) b.bg_color.z = nv;
        else if (std::strcmp(prop, "bg_a") == 0) b.bg_color.w = nv;
        return val_nil();
    }
    // Search panels
    for (auto& p : gs->script_ui.panels) {
        if (p.id != id) continue;
        if (std::strcmp(prop, "x") == 0) p.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) p.position.y = nv;
        else if (std::strcmp(prop, "w") == 0) p.width = nv;
        else if (std::strcmp(prop, "h") == 0) p.height = nv;
        else if (std::strcmp(prop, "sprite") == 0) p.sprite_region = sv;
        else if (std::strcmp(prop, "visible") == 0) p.visible = bv;
        else if (std::strcmp(prop, "rotation") == 0) p.rotation = nv;
        else if (std::strcmp(prop, "opacity") == 0) p.opacity = nv;
        else if (std::strcmp(prop, "scale") == 0) p.scale = nv;
        else if (std::strcmp(prop, "layer") == 0) p.layer = (int)nv;
        else if (std::strcmp(prop, "on_click") == 0) p.on_click = sv;
        else if (std::strcmp(prop, "r") == 0) p.color.x = nv;
        else if (std::strcmp(prop, "g") == 0) p.color.y = nv;
        else if (std::strcmp(prop, "b") == 0) p.color.z = nv;
        else if (std::strcmp(prop, "a") == 0) p.color.w = nv;
        return val_nil();
    }
    // Search images
    for (auto& img : gs->script_ui.images) {
        if (img.id != id) continue;
        if (std::strcmp(prop, "x") == 0) img.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) img.position.y = nv;
        else if (std::strcmp(prop, "w") == 0) img.width = nv;
        else if (std::strcmp(prop, "h") == 0) img.height = nv;
        else if (std::strcmp(prop, "icon") == 0) img.icon_name = sv;
        else if (std::strcmp(prop, "visible") == 0) img.visible = bv;
        else if (std::strcmp(prop, "opacity") == 0) img.opacity = nv;
        else if (std::strcmp(prop, "scale") == 0) img.scale = nv;
        else if (std::strcmp(prop, "rotation") == 0) img.rotation = nv;
        else if (std::strcmp(prop, "flip_h") == 0) img.flip_h = bv;
        else if (std::strcmp(prop, "flip_v") == 0) img.flip_v = bv;
        else if (std::strcmp(prop, "layer") == 0) img.layer = (int)nv;
        else if (std::strcmp(prop, "on_click") == 0) img.on_click = sv;
        else if (std::strcmp(prop, "r") == 0) img.tint.x = nv;
        else if (std::strcmp(prop, "g") == 0) img.tint.y = nv;
        else if (std::strcmp(prop, "b") == 0) img.tint.z = nv;
        else if (std::strcmp(prop, "a") == 0) img.tint.w = nv;
        return val_nil();
    }
    return val_nil();
}

// ui_get(id, property) -> value — read any UI component property
static Value native_ui_get(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* prop = (args[1].type == VAL_STRING) ? args[1].as.string : "";

    for (auto& l : gs->script_ui.labels) {
        if (l.id != id) continue;
        if (std::strcmp(prop, "x") == 0) return val_number(l.position.x);
        if (std::strcmp(prop, "y") == 0) return val_number(l.position.y);
        if (std::strcmp(prop, "scale") == 0) return val_number(l.scale);
        if (std::strcmp(prop, "text") == 0) return val_string(l.text.c_str());
        if (std::strcmp(prop, "visible") == 0) return val_bool(l.visible);
        if (std::strcmp(prop, "opacity") == 0) return val_number(l.opacity);
        if (std::strcmp(prop, "layer") == 0) return val_number(l.layer);
        return val_nil();
    }
    for (auto& b : gs->script_ui.bars) {
        if (b.id != id) continue;
        if (std::strcmp(prop, "x") == 0) return val_number(b.position.x);
        if (std::strcmp(prop, "y") == 0) return val_number(b.position.y);
        if (std::strcmp(prop, "value") == 0) return val_number(b.value);
        if (std::strcmp(prop, "max") == 0) return val_number(b.max_value);
        if (std::strcmp(prop, "w") == 0) return val_number(b.width);
        if (std::strcmp(prop, "h") == 0) return val_number(b.height);
        if (std::strcmp(prop, "rotation") == 0) return val_number(b.rotation);
        if (std::strcmp(prop, "visible") == 0) return val_bool(b.visible);
        return val_nil();
    }
    for (auto& p : gs->script_ui.panels) {
        if (p.id != id) continue;
        if (std::strcmp(prop, "x") == 0) return val_number(p.position.x);
        if (std::strcmp(prop, "y") == 0) return val_number(p.position.y);
        if (std::strcmp(prop, "w") == 0) return val_number(p.width);
        if (std::strcmp(prop, "h") == 0) return val_number(p.height);
        if (std::strcmp(prop, "rotation") == 0) return val_number(p.rotation);
        if (std::strcmp(prop, "visible") == 0) return val_bool(p.visible);
        if (std::strcmp(prop, "scale") == 0) return val_number(p.scale);
        return val_nil();
    }
    for (auto& img : gs->script_ui.images) {
        if (img.id != id) continue;
        if (std::strcmp(prop, "x") == 0) return val_number(img.position.x);
        if (std::strcmp(prop, "y") == 0) return val_number(img.position.y);
        if (std::strcmp(prop, "w") == 0) return val_number(img.width);
        if (std::strcmp(prop, "h") == 0) return val_number(img.height);
        if (std::strcmp(prop, "visible") == 0) return val_bool(img.visible);
        if (std::strcmp(prop, "scale") == 0) return val_number(img.scale);
        if (std::strcmp(prop, "rotation") == 0) return val_number(img.rotation);
        return val_nil();
    }
    return val_nil();
}

// =============== HUD Config API ===============

// hud_set(property, value) — configure HUD dimensions and visibility
static Value native_hud_set(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto& H = s_active_engine->game_state_->hud;
    const char* prop = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float v = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    bool bv = (args[1].type == VAL_BOOL) ? args[1].as.boolean : (v != 0);

    if (std::strcmp(prop, "scale") == 0)          H.scale = v;
    else if (std::strcmp(prop, "player_x") == 0)  H.player_x = v;
    else if (std::strcmp(prop, "player_y") == 0)  H.player_y = v;
    else if (std::strcmp(prop, "player_w") == 0)  H.player_w = v;
    else if (std::strcmp(prop, "player_h") == 0)  H.player_h = v;
    else if (std::strcmp(prop, "hp_bar_w") == 0)  H.hp_bar_w = v;
    else if (std::strcmp(prop, "hp_bar_h") == 0)  H.hp_bar_h = v;
    else if (std::strcmp(prop, "text_scale") == 0) H.text_scale = v;
    else if (std::strcmp(prop, "time_w") == 0)    H.time_w = v;
    else if (std::strcmp(prop, "time_h") == 0)    H.time_h = v;
    else if (std::strcmp(prop, "time_text_scale") == 0) H.time_text_scale = v;
    else if (std::strcmp(prop, "inv_slot_size") == 0) H.inv_slot_size = v;
    else if (std::strcmp(prop, "inv_padding") == 0)   H.inv_padding = v;
    else if (std::strcmp(prop, "inv_max_slots") == 0) H.inv_max_slots = (int)v;
    else if (std::strcmp(prop, "inv_y_offset") == 0)  H.inv_y_offset = v;
    else if (std::strcmp(prop, "surv_bar_w") == 0) H.surv_bar_w = v;
    else if (std::strcmp(prop, "surv_bar_h") == 0) H.surv_bar_h = v;
    else if (std::strcmp(prop, "show_player") == 0)    H.show_player = bv;
    else if (std::strcmp(prop, "show_time") == 0)      H.show_time = bv;
    else if (std::strcmp(prop, "show_inventory") == 0)  H.show_inventory = bv;
    else if (std::strcmp(prop, "show_survival") == 0)   H.show_survival = bv;
    else if (std::strcmp(prop, "show_minimap") == 0)   H.show_minimap = bv;
    else if (std::strcmp(prop, "minimap_size") == 0)   H.minimap_size = v;
    return val_nil();
}

// hud_get(property) -> number
static Value native_hud_get(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_number(0);
    auto& H = s_active_engine->game_state_->hud;
    const char* prop = (args[0].type == VAL_STRING) ? args[0].as.string : "";

    if (std::strcmp(prop, "scale") == 0)          return val_number(H.scale);
    if (std::strcmp(prop, "player_x") == 0)       return val_number(H.player_x);
    if (std::strcmp(prop, "player_y") == 0)       return val_number(H.player_y);
    if (std::strcmp(prop, "player_w") == 0)       return val_number(H.player_w);
    if (std::strcmp(prop, "player_h") == 0)       return val_number(H.player_h);
    if (std::strcmp(prop, "hp_bar_w") == 0)       return val_number(H.hp_bar_w);
    if (std::strcmp(prop, "hp_bar_h") == 0)       return val_number(H.hp_bar_h);
    if (std::strcmp(prop, "text_scale") == 0)     return val_number(H.text_scale);
    if (std::strcmp(prop, "time_w") == 0)         return val_number(H.time_w);
    if (std::strcmp(prop, "time_h") == 0)         return val_number(H.time_h);
    if (std::strcmp(prop, "inv_slot_size") == 0)  return val_number(H.inv_slot_size);
    if (std::strcmp(prop, "inv_max_slots") == 0)  return val_number(H.inv_max_slots);
    if (std::strcmp(prop, "show_player") == 0)    return val_bool(H.show_player);
    if (std::strcmp(prop, "show_time") == 0)      return val_bool(H.show_time);
    if (std::strcmp(prop, "show_inventory") == 0) return val_bool(H.show_inventory);
    if (std::strcmp(prop, "show_survival") == 0)  return val_bool(H.show_survival);
    if (std::strcmp(prop, "show_minimap") == 0)  return val_bool(H.show_minimap);
    if (std::strcmp(prop, "minimap_size") == 0)  return val_number(H.minimap_size);
    if (std::strcmp(prop, "screen_w") == 0)     return val_number(H.screen_w);
    if (std::strcmp(prop, "screen_h") == 0)     return val_number(H.screen_h);
    return val_number(0);
}

// =============== Survival API ===============

// enable_survival(bool)
static Value native_enable_survival(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    bool en = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    s_active_engine->game_state_->survival.enabled = en;
    return val_nil();
}

// get/set hunger, thirst, energy — all follow same pattern
static Value native_get_hunger(int, Value*) { if (!s_active_engine||!s_active_engine->game_state_) return val_number(0); return val_number(s_active_engine->game_state_->survival.hunger); }
static Value native_set_hunger(int argc, Value* args) { if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil(); if(args[0].type==VAL_NUMBER) s_active_engine->game_state_->survival.hunger=(float)args[0].as.number; return val_nil(); }
static Value native_get_thirst(int, Value*) { if (!s_active_engine||!s_active_engine->game_state_) return val_number(0); return val_number(s_active_engine->game_state_->survival.thirst); }
static Value native_set_thirst(int argc, Value* args) { if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil(); if(args[0].type==VAL_NUMBER) s_active_engine->game_state_->survival.thirst=(float)args[0].as.number; return val_nil(); }
static Value native_get_energy(int, Value*) { if (!s_active_engine||!s_active_engine->game_state_) return val_number(0); return val_number(s_active_engine->game_state_->survival.energy); }
static Value native_set_energy(int argc, Value* args) { if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil(); if(args[0].type==VAL_NUMBER) s_active_engine->game_state_->survival.energy=(float)args[0].as.number; return val_nil(); }

// set_survival_rate(stat, rate) — stat: "hunger", "thirst", "energy"
static Value native_set_survival_rate(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    const char* stat = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float rate = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1.0f;
    auto& s = s_active_engine->game_state_->survival;
    if (std::strcmp(stat, "hunger") == 0) s.hunger_rate = rate;
    else if (std::strcmp(stat, "thirst") == 0) s.thirst_rate = rate;
    else if (std::strcmp(stat, "energy") == 0) s.energy_rate = rate;
    return val_nil();
}

// =============== Screen Effects API ===============

static Value native_screen_shake(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    gs->shake_intensity = (args[0].type == VAL_NUMBER) ? std::max(0.0f, std::min(50.0f, (float)args[0].as.number)) : 4.0f;
    gs->shake_timer = (args[1].type == VAL_NUMBER) ? std::max(0.0f, std::min(10.0f, (float)args[1].as.number)) : 0.3f;
    return val_nil();
}

static Value native_screen_flash(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    gs->flash_r = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 1.0f;
    gs->flash_g = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1.0f;
    gs->flash_b = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 1.0f;
    gs->flash_a = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 1.0f;
    gs->flash_timer = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0.2f;
    return val_nil();
}

static Value native_screen_fade(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    gs->fade_r = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    gs->fade_g = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    gs->fade_b = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    gs->fade_target = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 1.0f;
    gs->fade_duration = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 1.0f;
    gs->fade_timer = gs->fade_duration;
    return val_nil();
}

// =============== Dialogue Extension API ===============

static Value native_set_dialogue_speed(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->dialogue.set_chars_per_sec((float)args[0].as.number);
    return val_nil();
}

static Value native_set_dialogue_scale(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->dialogue.set_text_scale((float)args[0].as.number);
    return val_nil();
}

// =============== Battle Extension API ===============

static Value native_start_battle(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    gs->battle.enemy_name = (args[0].type == VAL_STRING) ? args[0].as.string : "Enemy";
    gs->battle.enemy_hp_actual = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 10;
    gs->battle.enemy_hp_max = gs->battle.enemy_hp_actual;
    gs->battle.enemy_atk = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 3;
    if (args[3].type == VAL_STRING) gs->battle.enemy_sprite_key = args[3].as.string;
    else gs->battle.enemy_sprite_id = (args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 0;
    gs->battle.phase = BattlePhase::Intro;
    gs->battle.phase_timer = 0;
    gs->battle.player_hp_actual = gs->player_hp;
    gs->battle.player_hp_max = gs->player_hp_max;
    gs->battle.player_atk = gs->player_atk;
    gs->battle.player_def = gs->player_def;
    gs->battle.sam_hp_actual = gs->sam_hp;
    gs->battle.sam_hp_max = gs->sam_hp_max;
    gs->battle.sam_atk = gs->sam_atk;
    return val_nil();
}

static Value native_is_in_battle(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    return val_bool(s_active_engine->game_state_->battle.phase != BattlePhase::None);
}

static Value native_set_xp_formula(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->xp_multiplier = (float)args[0].as.number;
    return val_nil();
}

// =============== Renderer API ===============

static Value native_set_clear_color(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* renderer = s_active_engine->game_state_->renderer;
    if (!renderer) return val_nil();
    float r = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float g = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float b = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    renderer->set_clear_color(r, g, b);
    return val_nil();
}

// =============== Register Methods ===============

void ScriptEngine::register_ui_api() {
    if (!env_) return;
    env_define(env_, "ui_label", 8, val_native(native_ui_label));
    env_define(env_, "ui_bar", 6, val_native(native_ui_bar));
    env_define(env_, "ui_remove", 9, val_native(native_ui_remove));
    env_define(env_, "ui_notify", 9, val_native(native_ui_notify));
    env_define(env_, "ui_panel", 8, val_native(native_ui_panel));
    env_define(env_, "ui_image", 8, val_native(native_ui_image));
    env_define(env_, "ui_set", 6, val_native(native_ui_set));
    env_define(env_, "ui_get", 6, val_native(native_ui_get));
    env_define(env_, "hud_set", 7, val_native(native_hud_set));
    env_define(env_, "hud_get", 7, val_native(native_hud_get));
    std::printf("[ScriptEngine] UI API registered\n");
}

void ScriptEngine::register_survival_api() {
    if (!env_) return;
    env_define(env_, "enable_survival", 15, val_native(native_enable_survival));
    env_define(env_, "get_hunger", 10, val_native(native_get_hunger));
    env_define(env_, "set_hunger", 10, val_native(native_set_hunger));
    env_define(env_, "get_thirst", 10, val_native(native_get_thirst));
    env_define(env_, "set_thirst", 10, val_native(native_set_thirst));
    env_define(env_, "get_energy", 10, val_native(native_get_energy));
    env_define(env_, "set_energy", 10, val_native(native_set_energy));
    env_define(env_, "set_survival_rate", 17, val_native(native_set_survival_rate));
    std::printf("[ScriptEngine] Survival API registered\n");
}

void ScriptEngine::register_effects_api() {
    if (!env_) return;
    env_define(env_, "screen_shake", 12, val_native(native_screen_shake));
    env_define(env_, "screen_flash", 12, val_native(native_screen_flash));
    env_define(env_, "screen_fade", 11, val_native(native_screen_fade));
    std::printf("[ScriptEngine] Effects API registered\n");
}

void ScriptEngine::register_dialogue_ext_api() {
    if (!env_) return;
    env_define(env_, "set_dialogue_speed", 18, val_native(native_set_dialogue_speed));
    env_define(env_, "set_dialogue_scale", 18, val_native(native_set_dialogue_scale));
    std::printf("[ScriptEngine] Dialogue Extension API registered\n");
}

void ScriptEngine::register_battle_ext_api() {
    if (!env_) return;
    env_define(env_, "start_battle", 12, val_native(native_start_battle));
    env_define(env_, "is_in_battle", 12, val_native(native_is_in_battle));
    env_define(env_, "set_xp_formula", 14, val_native(native_set_xp_formula));
    std::printf("[ScriptEngine] Battle Extension API registered\n");
}

// ═══════════════ Renderer API ═══════════════

void ScriptEngine::register_renderer_api() {
    if (!env_) return;
    env_define(env_, "set_clear_color", 15, val_native(native_set_clear_color));
    std::printf("[ScriptEngine] Renderer API registered\n");
}

} // namespace eb
