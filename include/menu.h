#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>

// Menu structure as plain data. Items are identified by a stable action id;
// MainWindow maps ids to handlers, so renaming a label can never break an
// action. An item whose id has no handler is shown greyed out.
struct MenuItem {
    enum class Kind { Normal, Check, Radio, Separator };

    Kind kind = Kind::Normal;
    std::string label;   // underscore marks the mnemonic
    std::string action;  // stable id, empty for separators
    std::string accel;   // GTK accelerator names, alternatives separated by '|'
    std::string group;   // radio items sharing a group exclude each other
};

struct Menu {
    std::string title;
    std::vector<MenuItem> items;
};

std::vector<Menu> buildHexEditorMenus();

#endif
