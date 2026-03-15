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

    log("═══ All API Tests Complete ═══")
    info("TEST SUITE PASSED")
