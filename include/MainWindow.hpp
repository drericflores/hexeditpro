#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <gtkmm.h>

#include <functional>
#include <map>
#include <string>

#include "HexBuffer.hpp"
#include "HexView.hpp"

class MainWindow : public Gtk::Window {
public:
    MainWindow();
    ~MainWindow() override;

    // Opens a file (asks about unsaved changes first). Shows an error dialog
    // and returns false if it cannot be read.
    bool open_file(const std::string& path, bool read_only = false);

protected:
    bool on_delete_event(GdkEventAny* event) override;

private:
    // Setup
    void register_actions();
    void build_menus();
    void apply_theme();

    // UI state
    void status(const std::string& message);
    void update_title();
    void update_info();
    void sync_menus();
    void show_error(const std::string& title, const std::string& detail);

    // Files
    bool confirm_discard();
    bool save_current();
    bool save_as();
    bool save_to(const std::string& path);
    bool choose_file(const std::string& title, Gtk::FileChooserAction action, std::string& path);
    void action_new();
    void action_open(bool read_only);
    void action_export();

    // Edit
    void action_undo();
    void action_redo();
    void action_fill();

    // Search
    enum class FindMode { Hex = 0, Ascii = 1 };
    void action_find(int mode);   // mode: -1 = keep the last one
    void find_again(bool forward);
    void action_goto();

    // Help
    void action_shortcuts();
    void action_about();

    // Widgets
    Gtk::Box m_vbox{Gtk::ORIENTATION_VERTICAL};
    Gtk::MenuBar m_menu_bar;
    HexView m_view;
    Gtk::Box m_status_row{Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Statusbar m_statusbar;
    Gtk::Label m_info;
    guint m_status_context = 0;

    Glib::RefPtr<Gtk::AccelGroup> m_accel;

    // Menu plumbing
    std::map<std::string, std::function<void()>> m_actions;
    std::map<std::string, Gtk::CheckMenuItem*> m_checks;
    std::map<std::string, Gtk::RadioMenuItem*> m_radios;
    bool m_syncing_menu = false;

    // Document and settings
    HexBuffer m_buffer;
    bool m_dark_mode = false;

    // Last search
    HexBuffer::Bytes m_find_bytes;
    std::string m_find_text;
    FindMode m_find_mode = FindMode::Hex;
    bool m_find_wrap = true;
};

#endif
