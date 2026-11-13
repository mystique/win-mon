#include "support/CoreFixtures.h"

using namespace winmon;
using namespace test;

TEST_CASE("Menu layout and representative action tokens match the operator contract", "[unit][menu]")
{
    WinMonCore core;
    core.ObserveNetwork({{NamedNic("a", L"Alpha", L"A desc")}, true});
    auto menu = core.BeginOperatorMenu({false, true, true});
    REQUIRE(menu.has_value());
    const std::vector<std::wstring> expectedLabels{L"Monitored Network",
                                                   L"",
                                                   L"Show Taskbar Text",
                                                   L"Taskbar Display Content",
                                                   L"Taskbar Display Position",
                                                   L"Enable Taskbar Context Menu",
                                                   L"",
                                                   L"Show Tray Icon",
                                                   L"",
                                                   L"Show Floating Display",
                                                   L"Show Floating Text",
                                                   L"Show Floating Charts",
                                                   L"Floating Display Opacity",
                                                   L"",
                                                   L"Start at Sign-in",
                                                   L"",
                                                   L"Exit"};
    REQUIRE(menu->size() == expectedLabels.size());
    for (size_t i = 0; i < expectedLabels.size(); ++i)
    {
        REQUIRE((*menu)[i].label == expectedLabels[i]);
        REQUIRE((*menu)[i].separator == expectedLabels[i].empty());
    }
    REQUIRE(!FindItem(*menu, L"Start at Sign-in").checked);
    REQUIRE(FindItem(*menu, L"Enable Taskbar Context Menu").checked);
    REQUIRE(FindItem(*menu, L"Show Floating Display").checked);
    REQUIRE(FindItem(*menu, L"Show Floating Text").checked);
    REQUIRE(FindItem(*menu, L"Show Floating Charts").checked);
    REQUIRE(core.CompleteOperatorMenu(FindItem(*menu, L"Show Floating Charts").choiceToken).kind ==
            OperatorActionKind::HideFloatingCharts);
    menu = core.BeginOperatorMenu({false, true, true, false, true, false});
    REQUIRE(menu.has_value());
    REQUIRE(!FindItem(*menu, L"Show Floating Charts").checked);
    const auto& opacity = FindItem(*menu, L"Floating Display Opacity");
    REQUIRE(opacity.children.size() == 10);
    for (size_t i = 0; i < opacity.children.size(); ++i)
    {
        const auto& level = opacity.children[i];
        REQUIRE((level.label == std::to_wstring((i + 1) * 10) + L"%" && level.radio));
        REQUIRE(level.checked == (i == 9));
    }
    REQUIRE(core.CompleteOperatorMenu(opacity.children[3].choiceToken).opacity == 40);
    menu = core.BeginOperatorMenu({false, true, true, false, true, false, 40});
    REQUIRE(menu.has_value());
    const auto& selectedOpacity = FindItem(*menu, L"Floating Display Opacity");
    REQUIRE((selectedOpacity.children[3].checked && !selectedOpacity.children[9].checked));
    REQUIRE(core.CompleteOperatorMenu(selectedOpacity.children[0].choiceToken).kind ==
            OperatorActionKind::SetFloatingDisplayOpacity);
    menu = core.BeginOperatorMenu({false, true, true, false, true, false, 40});
    REQUIRE(menu.has_value());
    const auto& startAtSignIn = FindItem(*menu, L"Start at Sign-in");
    REQUIRE(core.CompleteOperatorMenu(startAtSignIn.choiceToken).kind == OperatorActionKind::EnableStartAtSignIn);

    menu = core.BeginOperatorMenu({true, true, true});
    REQUIRE(menu.has_value());
    const auto& taskbarContextMenu = FindItem(*menu, L"Enable Taskbar Context Menu");
    REQUIRE(core.CompleteOperatorMenu(taskbarContextMenu.choiceToken).kind ==
            OperatorActionKind::DisableTaskbarContextMenu);

    menu = core.BeginOperatorMenu({false, false, true});
    REQUIRE(menu.has_value());
    const auto& floatingDisplay = FindItem(*menu, L"Show Floating Display");
    REQUIRE(core.CompleteOperatorMenu(floatingDisplay.choiceToken).kind == OperatorActionKind::HideFloatingDisplay);

    menu = core.BeginOperatorMenu({.floatingDisplayEnabled = true});
    REQUIRE(menu.has_value());
    REQUIRE(core.CompleteOperatorMenu(FindItem(*menu, L"Show Floating Text").choiceToken).kind ==
            OperatorActionKind::HideFloatingText);
    menu = core.BeginOperatorMenu({false, false, true, false, false});
    REQUIRE(menu.has_value());
    REQUIRE(!FindItem(*menu, L"Show Floating Text").checked);
    REQUIRE(core.CompleteOperatorMenu(FindItem(*menu, L"Show Floating Text").choiceToken).kind ==
            OperatorActionKind::ShowFloatingText);
    menu = core.BeginOperatorMenu({});
    REQUIRE(menu.has_value());
    core.CompleteOperatorMenu(0);

    menu = core.BeginOperatorMenu({});
    REQUIRE(menu.has_value());
    const auto& exit = FindItem(*menu, L"Exit");
    REQUIRE(core.CompleteOperatorMenu(exit.choiceToken).kind == OperatorActionKind::Exit);
}

TEST_CASE("Menu visibility and execution guards protect the last available entry", "[unit][menu]")
{
    for (int flags = 0; flags < 64; ++flags)
    {
        CAPTURE(flags);
        const bool text = (flags & 1) != 0;
        const bool context = (flags & 2) != 0;
        const bool tray = (flags & 4) != 0;
        const bool floating = (flags & 8) != 0;
        const bool taskbarAvailable = (flags & 16) != 0;
        const bool floatingAvailable = (flags & 32) != 0;
        winmon::OperatorMenuToggles toggles{.taskbarContextMenuEnabled = context,
                                            .floatingDisplayEnabled = floating,
                                            .taskbarTextVisible = text,
                                            .trayIconVisible = tray,
                                            .taskbarDisplayAvailable = taskbarAvailable,
                                            .floatingDisplayAvailable = floatingAvailable};
        WinMonCore core;
        auto menu = core.BeginOperatorMenu(toggles);
        REQUIRE(menu.has_value());
        const auto present = [&](const wchar_t* label) {
            return std::any_of(menu->begin(), menu->end(), [&](const auto& item) { return item.label == label; });
        };
        REQUIRE((present(L"Taskbar Display Position") == text && present(L"Enable Taskbar Context Menu") == text));
        for (const auto* label : {L"Show Floating Text", L"Show Floating Charts", L"Floating Display Opacity"})
            REQUIRE(present(label) == floating);
        const bool taskbarEntry = text && context && taskbarAvailable;
        const bool floatingEntry = floating && floatingAvailable;
        const auto check = [&](const wchar_t* label, bool checked, bool allowed) {
            const auto& item = FindItem(*menu, label);
            REQUIRE((item.checked == checked && item.enabled == allowed));
            if (!allowed)
                REQUIRE(item.choiceToken == 0);
        };
        check(L"Show Taskbar Text", text, !text || tray || floatingEntry);
        if (text)
            check(L"Enable Taskbar Context Menu", context, !context || tray || floatingEntry);
        check(L"Show Tray Icon", tray, !tray || taskbarEntry || floatingEntry);
        check(L"Show Floating Display", floating, !floating || tray || taskbarEntry);
        core.CancelOperatorMenu();
        for (const auto kind : {OperatorActionKind::HideTaskbarText, OperatorActionKind::DisableTaskbarContextMenu,
                                OperatorActionKind::HideTrayIcon, OperatorActionKind::HideFloatingDisplay})
        {
            const bool expected = kind == OperatorActionKind::HideTrayIcon          ? taskbarEntry || floatingEntry
                                  : kind == OperatorActionKind::HideFloatingDisplay ? tray || taskbarEntry
                                                                                    : tray || floatingEntry;
            REQUIRE(CanApplyOperatorAction(kind, toggles) == expected);
        }
        REQUIRE(CanApplyOperatorAction(OperatorActionKind::Exit, toggles));
        REQUIRE(winmon::HasConfiguredMenuAccess(toggles) == (tray || floating || (text && context)));
        REQUIRE(winmon::ShouldShowTrayIcon(toggles) == (tray || (!taskbarEntry && !floatingEntry)));
    }
    WinMonCore core;
    auto menu = core.BeginOperatorMenu({});
    REQUIRE(menu.has_value());
    REQUIRE(core.CompleteOperatorMenu(FindItem(*menu, L"Show Taskbar Text").choiceToken).kind ==
            OperatorActionKind::HideTaskbarText);
    menu = core.BeginOperatorMenu({.taskbarContextMenuEnabled = true});
    REQUIRE(menu.has_value());
    REQUIRE(core.CompleteOperatorMenu(FindItem(*menu, L"Show Tray Icon").choiceToken).kind ==
            OperatorActionKind::HideTrayIcon);
    menu = core.BeginOperatorMenu({.taskbarTextVisible = false, .trayIconVisible = false});
    REQUIRE(menu.has_value());
    REQUIRE(core.CompleteOperatorMenu(FindItem(*menu, L"Show Taskbar Text").choiceToken).kind ==
            OperatorActionKind::ShowTaskbarText);
    menu = core.BeginOperatorMenu({.trayIconVisible = false});
    REQUIRE(menu.has_value());
    REQUIRE(core.CompleteOperatorMenu(FindItem(*menu, L"Show Tray Icon").choiceToken).kind ==
            OperatorActionKind::ShowTrayIcon);
}

TEST_CASE("Taskbar position menu reflects and selects Left and Right", "[unit][menu]")
{
    WinMonCore core;
    auto menu = core.BeginOperatorMenu();
    REQUIRE(menu.has_value());
    const auto& position = FindItem(*menu, L"Taskbar Display Position");
    REQUIRE(position.children.size() == 2);
    const auto& left = FindItem(position.children, L"Left");
    const auto& right = FindItem(position.children, L"Right");
    REQUIRE((left.radio && !left.checked && right.radio && right.checked));
    REQUIRE(core.CompleteOperatorMenu(left.choiceToken).kind == OperatorActionKind::SetTaskbarDisplayLeft);
    menu = core.BeginOperatorMenu({false, false, false, true});
    REQUIRE(menu.has_value());
    REQUIRE(FindItem(FindItem(*menu, L"Taskbar Display Position").children, L"Left").checked);
    REQUIRE(
        core.CompleteOperatorMenu(FindItem(FindItem(*menu, L"Taskbar Display Position").children, L"Right").choiceToken)
            .kind == OperatorActionKind::SetTaskbarDisplayRight);
}

TEST_CASE("Taskbar memory menu toggles the current setting", "[unit][menu]")
{
    WinMonCore core;
    for (bool visible : {false, true})
    {
        auto menu = core.BeginOperatorMenu({.taskbarMemoryVisible = visible});
        REQUIRE(menu);
        const auto& items = FindItem(*menu, L"Taskbar Display Content").children;
        const auto& memory = FindItem(items, L"Memory");
        CHECK(FindItem(items, L"Network Speed").enabled == visible);
        CHECK(memory.checked == visible);
        const auto action = core.CompleteOperatorMenu(memory.choiceToken);
        CHECK(action.kind == (visible ? OperatorActionKind::HideTaskbarMemory
                                      : OperatorActionKind::ShowTaskbarMemory));
    }
}

TEST_CASE("Memory-only selection keeps the last content item checked", "[unit][menu]")
{
    WinMonCore core;
    auto menu = core.BeginOperatorMenu({.taskbarMemoryVisible = true, .taskbarRatesVisible = false});
    REQUIRE(menu);
    const auto& items = FindItem(*menu, L"Taskbar Display Content").children;
    const auto& memory = FindItem(items, L"Memory");
    CHECK(memory.checked);
    CHECK_FALSE(memory.enabled);
    CHECK(memory.choiceToken == 0);
    const auto& rates = FindItem(items, L"Network Speed");
    CHECK_FALSE(rates.checked);
    CHECK(core.CompleteOperatorMenu(rates.choiceToken).kind == OperatorActionKind::ShowTaskbarRates);
}

TEST_CASE("GPU is offered only after a successful probe and can be the only group", "[unit][menu]")
{
    WinMonCore core;
    for (bool available : {false, true})
    {
        auto menu = core.BeginOperatorMenu({.taskbarMemoryVisible = false, .taskbarRatesVisible = !available,
            .gpuAvailable = available, .gpuVisible = available});
        REQUIRE(menu);
        const auto& content = FindItem(*menu, L"Taskbar Display Content").children;
        REQUIRE(content.size() == (available ? 3 : 2));
        CHECK(content.front().label == L"Network Speed");
        CHECK(content.back().label == L"Memory");
        if (available)
        {
            CHECK(content[1].label == L"GPU");
            CHECK(content[1].checked);
            CHECK_FALSE(content[1].enabled);
        }
        core.CancelOperatorMenu();
    }
}
