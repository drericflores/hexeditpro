#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <gtkmm.h>
#include "HexBuffer.hpp"
#include "HexViewWidget.hpp"
#include "menu.h"

class MainWindow : public Gtk::Window {
public:
    MainWindow();
    ~MainWindow() override;

private:
    Gtk::Box m_vbox{Gtk::ORIENTATION_VERTICAL};
    Gtk::MenuBar m_menu_bar;

    Gtk::ScrolledWindow m_scroll;
    HexViewWidget m_hex_display;

    Gtk::Statusbar m_statusbar;
    HexBuffer m_buffer;

    Glib::RefPtr<Gtk::CssProvider> m_css_provider;
    bool m_dark_mode{false};

    void setup_complex_menus();
    void apply_theme();
    void status(const std::string& msg);

    // File
    void on_file_new();
    void on_file_open();
    void on_file_save();
    void on_file_save_as();
    void on_file_quit();

    // Edit
    void on_edit_undo();
    void on_edit_redo();
    void on_edit_cut_bytes();
    void on_edit_copy_bytes();
    void on_edit_paste_insert();
    void on_edit_paste_overwrite();
    void on_edit_fill_selection();
    void on_edit_zero_selection();
    void on_edit_select_all();

    // View
    void on_view_theme_toggle();

    // Search / Analysis / Help
    void on_search_find_bytes();
    void on_analysis_frequency();
    void on_help_about();
};

#endif
