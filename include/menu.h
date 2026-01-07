#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>
#include <functional>

struct MenuItem {
    std::string label;
    std::function<void()> action;
    bool separator = false;
};

struct Menu {
    std::string title;
    std::vector<MenuItem> items;
};

std::vector<Menu> buildHexEditorMenus();

#endif
