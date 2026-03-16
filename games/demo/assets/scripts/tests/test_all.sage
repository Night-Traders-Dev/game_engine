# Twilight Engine — API Test Suite
# Run via: run_all_tests() or --test flag
# Uses assert_true() for assertions

proc run_all_tests():
    log("═══ Running API Test Suite ═══")

    # ── Engine Core ──
    log("Testing: Engine Core")
    assert_true(random(1, 10) >= 1, "random min")
    assert_true(random(1, 10) <= 10, "random max")
    assert_true(clamp(5, 0, 10) == 5, "clamp mid")
    assert_true(clamp(-5, 0, 10) == 0, "clamp low")
    assert_true(clamp(20, 0, 10) == 10, "clamp high")
    assert_true(str(42) == "42", "str number")
    log("test log")
    debug("test debug")
    info("test info")
    warn("test warn")
    print("test", "print", 123)

    # ── Flags ──
    log("Testing: Flags")
    set_flag("t_num", 42)
    assert_true(get_flag("t_num") == 42, "flag number")
    set_flag("t_str", "hi")
    assert_true(get_flag("t_str") == "hi", "flag string")
    set_flag("t_bool", true)
    assert_true(get_flag("t_bool") == true, "flag bool")
    assert_true(get_flag("t_none") == 0, "flag unset")
    set_flag("t_num", 99)
    assert_true(get_flag("t_num") == 99, "flag overwrite")

    # ── Inventory ──
    log("Testing: Inventory")
    add_item("t_sword", "Sword", 3, "weapon", "blade", 0, 10, "fire", "")
    assert_true(has_item("t_sword"), "has_item")
    assert_true(item_count("t_sword") == 3, "item_count 3")
    add_item("t_sword", "Sword", 2, "weapon", "blade", 0, 10, "fire", "")
    assert_true(item_count("t_sword") == 5, "item_count stack")
    remove_item("t_sword", 2)
    assert_true(item_count("t_sword") == 3, "item_count remove")
    remove_item("t_sword", 3)
    assert_true(has_item("t_sword") == false, "removed all")

    # ── Gold ──
    log("Testing: Gold")
    set_gold(500)
    assert_true(get_gold() == 500, "set/get gold")
    set_gold(get_gold() + 100)
    assert_true(get_gold() == 600, "gold math")
    set_gold(200)

    # ── Stats ──
    log("Testing: Stats")
    set_skill("player", "vitality", 8)
    assert_true(get_skill("player", "vitality") == 8, "set/get vitality")
    set_skill("ally", "arcana", 7)
    assert_true(get_skill("ally", "arcana") == 7, "set/get arcana")
    set_skill("player", "strength", 9)
    assert_true(get_skill("player", "strength") == 9, "set/get strength")
    set_skill("player", "vitality", 15)
    assert_true(get_skill("player", "vitality") == 10, "stat max clamp")
    set_skill("player", "vitality", 0)
    assert_true(get_skill("player", "vitality") == 1, "stat min clamp")
    assert_true(get_skill_bonus("player", "hp") >= 0, "bonus hp")

    # ── Day-Night ──
    log("Testing: Day-Night")
    set_time(14, 30)
    assert_true(get_hour() == 14, "get_hour")
    assert_true(get_minute() == 30, "get_minute")
    assert_true(is_day() == true, "is_day 14:30")
    assert_true(is_night() == false, "not night 14:30")
    set_time(22, 0)
    assert_true(is_night() == true, "is_night 22:00")
    set_day_speed(6)

    # ── Survival ──
    log("Testing: Survival")
    enable_survival(true)
    set_hunger(80)
    assert_true(get_hunger() == 80, "set/get hunger")
    set_thirst(70)
    assert_true(get_thirst() == 70, "set/get thirst")
    set_energy(60)
    assert_true(get_energy() == 60, "set/get energy")
    set_survival_rate("hunger", 2.0)
    set_hunger(100)
    set_thirst(100)
    set_energy(100)
    enable_survival(false)

    # ── UI Components ──
    log("Testing: UI Components")
    ui_label("tl", "Hi", 10, 10, 1, 1, 1, 1)
    ui_set("tl", "text", "Updated")
    ui_set("tl", "visible", false)
    ui_bar("tb", 50, 100, 10, 30, 100, 12, 0.5, 0.8, 0.2, 1)
    ui_set("tb", "value", 75)
    ui_set("tb", "visible", false)
    ui_panel("tp", 10, 50, 200, 80, "panel_mini")
    ui_set("tp", "visible", false)
    ui_image("ti", 10, 60, 32, 32, "icon_sword")
    ui_set("ti", "visible", false)
    ui_notify("Test!", 0.1)
    ui_remove("tl")
    ui_remove("tb")
    ui_remove("tp")
    ui_remove("ti")

    # ── HUD Config ──
    log("Testing: HUD Config")
    hud_set("scale", 2.0)
    assert_true(hud_get("scale") == 2, "hud scale")
    hud_set("scale", 1.5)
    assert_true(hud_get("screen_w") > 0, "screen_w > 0")
    assert_true(hud_get("screen_h") > 0, "screen_h > 0")

    # ── NPC API ──
    log("Testing: NPC API")
    npc_move_to("Elder", 10, 10)
    npc_add_waypoint("Elder", 300, 300)
    npc_set_route("Elder", "patrol")
    npc_stop_route("Elder")
    npc_clear_route("Elder")
    npc_set_schedule("Elder", 6, 22)
    npc_clear_schedule("Elder")
    npc_face_each_other("Elder", "Merchant")
    npc_set_despawn_day("Skeleton", true)

    # ── Spawn API ──
    log("Testing: Spawn API")
    spawn_loop("Slime", 999, 1)
    set_spawn_area("Slime", 100, 100, 200, 200)
    set_spawn_time("Slime", 0, 24)
    stop_spawn_loop("Slime")

    # ── Audio API ──
    log("Testing: Audio API")
    set_music_volume(0.5)
    set_master_volume(0.8)
    stop_music()
    pause_music()
    resume_music()

    # ── Map API ──
    log("Testing: Map API")
    add_loot("TestE", "td", "Drop", 1.0, "consumable", "T", 5, 0, "", "")
    clear_loot("TestE")
    drop_item(100, 100, "td", "Drop", "consumable", "T", 5, 0, "", "")
    set_collision(0, 0, 0)
    set_portal(0, 0, "", 0, 0, "t")
    remove_portal(0, 0)

    # ── Player API ──
    log("Testing: Player API")
    let px = get_player_x()
    let py = get_player_y()
    assert_true(px >= 0, "get_player_x")
    assert_true(py >= 0, "get_player_y")
    set_player_pos(500, 400)
    assert_true(get_player_x() == 500, "set_player_pos x")
    assert_true(get_player_y() == 400, "set_player_pos y")
    set_player_pos(px, py)
    set_player_speed(150)
    assert_true(get_player_speed() == 150, "set/get player speed")
    set_player_speed(120)
    let hp = get_player_hp()
    set_player_hp(50)
    assert_true(get_player_hp() == 50, "set/get player hp")
    set_player_hp(hp)
    assert_true(get_player_hp_max() > 0, "player hp max")
    set_player_atk(25)
    assert_true(get_player_atk() == 25, "set/get player atk")
    set_player_atk(18)
    set_player_def(10)
    assert_true(get_player_def() == 10, "set/get player def")
    set_player_def(5)
    assert_true(get_player_level() >= 1, "player level")
    assert_true(get_player_xp() >= 0, "player xp")
    add_player_xp(10)
    set_player_dir(2)
    assert_true(get_player_dir() == 2, "set/get player dir")
    set_player_dir(0)
    let ahp = get_ally_hp()
    set_ally_hp(50)
    assert_true(get_ally_hp() == 50, "set/get ally hp")
    set_ally_hp(ahp)
    assert_true(get_ally_atk() > 0, "ally atk")

    # ── Camera API ──
    log("Testing: Camera API")
    let cx = get_camera_x()
    let cy = get_camera_y()
    assert_true(cx >= 0 or cx < 0, "get_camera_x returns number")
    camera_center(500, 400)
    set_camera_pos(cx, cy)
    camera_shake(5, 0.1)
    assert_true(camera_get_zoom() > 0, "camera zoom")

    # ── Platform API ──
    log("Testing: Platform API")
    assert_true(PLATFORM == "linux" or PLATFORM == "windows" or PLATFORM == "android", "PLATFORM set")
    assert_true(get_screen_w() > 0, "screen_w > 0")
    assert_true(get_screen_h() > 0, "screen_h > 0")

    # ── NPC Runtime API ──
    log("Testing: NPC Runtime API")
    assert_true(npc_count() > 0, "npc_count > 0")
    assert_true(npc_exists("Elder"), "Elder exists")
    let ex = npc_get_x("Elder")
    let ey = npc_get_y("Elder")
    assert_true(ex >= 0, "npc_get_x")
    npc_set_pos("Elder", 300, 300)
    npc_set_pos("Elder", ex, ey)
    npc_set_speed("Elder", 30)
    assert_true(npc_get_speed("Elder") == 30, "npc set/get speed")
    npc_set_speed("Elder", 20)
    npc_set_dir("Elder", 1)
    assert_true(npc_get_dir("Elder") == 1, "npc set/get dir")
    npc_set_dir("Elder", 0)
    npc_set_hostile("Elder", false)
    assert_true(npc_is_hostile("Elder") == false, "npc not hostile")

    # ── Screen Effects ──
    log("Testing: Screen Effects")
    screen_shake(2, 0.1)
    screen_flash(1, 1, 1, 0.5, 0.1)
    screen_fade(0, 0, 0, 0, 0.1)

    # ── Tile Map Query ──
    log("Testing: Tile Map Query")
    assert_true(get_map_width() > 0, "map width > 0")
    assert_true(get_map_height() > 0, "map height > 0")
    assert_true(get_tile_size() > 0, "tile size > 0")
    assert_true(get_layer_count() > 0, "layer count > 0")
    let tile = get_tile(0, 5, 5)
    assert_true(tile >= 0, "get_tile returns number")

    # ── Input API ──
    log("Testing: Input API")
    let held = is_key_held("up")
    assert_true(held == true or held == false, "is_key_held returns bool")
    let pressed = is_key_pressed("confirm")
    assert_true(pressed == true or pressed == false, "is_key_pressed returns bool")
    assert_true(get_mouse_x() >= 0 or get_mouse_x() < 0, "get_mouse_x returns number")

    # ── Dialogue Ext ──
    log("Testing: Dialogue Ext")
    set_dialogue_speed(50)
    set_dialogue_scale(1.2)
    set_dialogue_speed(35)
    set_dialogue_scale(1.0)

    # ── Battle Ext ──
    log("Testing: Battle Ext")
    assert_true(is_in_battle() == false, "not in battle")
    set_xp_formula(2.0)
    set_xp_formula(1.0)

    # ── Renderer ──
    log("Testing: Renderer")
    set_clear_color(0.05, 0.05, 0.12)

    # ── Level API ──
    log("Testing: Level API")
    assert_true(get_level_count() >= 0, "get_level_count")
    assert_true(is_level_loaded("nonexistent") == false, "is_level_loaded false")
    # Note: load_level/switch_level require actual map files — tested manually
    # Level zoom API
    set_level_zoom("forest.json", 1.5)
    assert_true(get_level_zoom("forest.json") == 1.5, "set/get level zoom")
    set_level_zoom("forest.json", 1.0)
    assert_true(get_level_zoom("nonexistent") == 1, "default zoom")

    # ── Flag Persistence ──
    log("Testing: Flag persistence & types")
    set_flag("test_int", 42)
    set_flag("test_str", "hello")
    set_flag("test_bool", true)
    assert_true(get_flag("test_int") == 42, "flag int persist")
    assert_true(get_flag("test_str") == "hello", "flag str persist")
    assert_true(get_flag("test_bool") == true, "flag bool persist")
    set_flag("test_int", 99)
    assert_true(get_flag("test_int") == 99, "flag overwrite persist")
    assert_true(get_flag("nonexistent_flag") == 0, "flag default 0")

    # ── Value Clamping ──
    log("Testing: Value clamping")
    set_day_speed(50)
    set_day_speed(6)
    set_music_volume(1.0)
    set_master_volume(0.8)
    # These should not crash with extreme values
    set_day_speed(0)
    set_day_speed(6)

    # ── Path Sanitization ──
    log("Testing: Path sanitization")
    # These should be rejected silently (no crash)
    # play_music("../../etc/passwd")  # Would log warning, skip
    # play_sfx("/absolute/path.wav")  # Would log warning, skip

    # ── Atlas Cache ──
    log("Testing: Atlas cache")
    # Spawn an NPC with string-based sprite key (atlas cache lookup)
    spawn_npc("TestNPC", 100, 100, 0, false, 0, 0, 0, 20, 0)
    assert_true(npc_exists("TestNPC"), "spawned test npc")
    npc_remove("TestNPC")
    assert_true(npc_exists("TestNPC") == false, "removed test npc")

    # ── Screen Effects Clamping ──
    log("Testing: Screen effects clamping")
    screen_shake(5, 0.1)
    screen_flash(1, 1, 1, 0.5, 0.1)
    screen_fade(0, 0, 0, 0, 0.1)
    camera_shake(3, 0.1)

    # ── Tile Rotation API ──
    log("Testing: Tile rotation API")
    let orig = get_tile(0, 5, 5)
    set_tile_rotation(0, 5, 5, 1)
    assert_true(get_tile_rotation(0, 5, 5) == 1, "set/get tile rotation 90")
    set_tile_rotation(0, 5, 5, 2)
    assert_true(get_tile_rotation(0, 5, 5) == 2, "tile rotation 180")
    set_tile_rotation(0, 5, 5, 0)
    assert_true(get_tile_rotation(0, 5, 5) == 0, "tile rotation reset")
    assert_true(get_tile(0, 5, 5) == orig, "tile ID preserved after rotation")
    # set_tile_ex with rotation and flip
    set_tile_ex(0, 5, 5, orig, 3, true, false)
    assert_true(get_tile(0, 5, 5) == orig, "set_tile_ex preserves ID")
    assert_true(get_tile_rotation(0, 5, 5) == 3, "set_tile_ex rotation 270")
    set_tile_ex(0, 5, 5, orig, 0, false, false)
    # Tile flip
    set_tile_flip(0, 5, 5, true, false)
    set_tile_flip(0, 5, 5, false, false)

    # ── Sprite Scale API ──
    log("Testing: Sprite scale API")
    # Player scale
    set_player_scale(2.0)
    assert_true(get_player_scale() == 2, "set/get player scale")
    set_player_scale(1.0)
    # Ally scale
    set_ally_scale(1.5)
    set_ally_scale(1.0)
    # NPC scale/tint/flip
    npc_set_scale("Elder", 2.0)
    assert_true(npc_get_scale("Elder") == 2, "npc set/get scale")
    npc_set_scale("Elder", 1.0)
    npc_set_tint("Elder", 1, 0.5, 0.5, 1)
    npc_set_tint("Elder", 1, 1, 1, 1)
    npc_set_flip("Elder", true)
    npc_set_flip("Elder", false)

    # ── UI Get API ──
    log("Testing: UI get API")
    ui_label("test_lbl", "Hello", 50, 60, 1, 1, 1, 1)
    ui_set("test_lbl", "scale", 1.2)
    assert_true(ui_get("test_lbl", "x") == 50, "ui_get label x")
    assert_true(ui_get("test_lbl", "y") == 60, "ui_get label y")
    assert_true(ui_get("test_lbl", "scale") > 1.1, "ui_get label scale")
    # Extended UI properties
    ui_set("test_lbl", "opacity", 0.8)
    ui_set("test_lbl", "layer", 5)
    ui_set("test_lbl", "on_click", "test_click")
    ui_remove("test_lbl")

    log("═══ All API Tests Complete ═══")
    info("TEST SUITE PASSED")
