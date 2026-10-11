#include "MainWindow.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

#include "HexUtil.hpp"
#include "Version.hpp"
#include "menu.h"

namespace {

constexpr int kResponseFindPrevious = 1;

std::string with_commas(std::size_t n) {
    std::string s = std::to_string(n);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
    return s;
}

std::string offset_text(std::size_t off) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "0x%08zX", off);
    return buf;
}

}  // namespace

// ------------------------------------------------------------ Construction

MainWindow::MainWindow() {
    set_default_size(1200, 800);

    m_accel = Gtk::AccelGroup::create();
    add_accel_group(m_accel);
    m_status_context = m_statusbar.get_context_id("hexpro");

    m_view.set_buffer(&m_buffer);
    m_view.signal_edited().connect([this] {
        update_title();
        update_info();
    });
    m_view.signal_cursor_changed().connect([this] {
        update_info();
        sync_menus();
    });
    m_view.signal_notice().connect([this](const std::string& message) { status(message); });

    register_actions();
    build_menus();

    m_info.set_margin_start(12);
    m_info.set_margin_end(12);
    m_status_row.pack_start(m_statusbar, Gtk::PACK_EXPAND_WIDGET);
    m_status_row.pack_end(m_info, Gtk::PACK_SHRINK);

    m_vbox.pack_start(m_menu_bar, Gtk::PACK_SHRINK);
    m_vbox.pack_start(m_view, Gtk::PACK_EXPAND_WIDGET);
    m_vbox.pack_start(m_status_row, Gtk::PACK_SHRINK);
    add(m_vbox);

    apply_theme();
    update_title();
    update_info();
    sync_menus();
    show_all_children();
    m_view.focus_canvas();
    status("Ready. Open a file (Ctrl+O), or start typing to build a new one.");
}

MainWindow::~MainWindow() = default;

void MainWindow::register_actions() {
    m_actions = {
        // File
        {"file_new", [this] { action_new(); }},
        {"file_open", [this] { action_open(false); }},
        {"file_open_readonly", [this] { action_open(true); }},
        {"file_save", [this] { save_current(); }},
        {"file_save_as", [this] { save_as(); }},
        {"file_export", [this] { action_export(); }},
        {"file_quit", [this] { close(); }},

        // Edit
        {"edit_undo", [this] { action_undo(); }},
        {"edit_redo", [this] { action_redo(); }},
        {"edit_cut", [this] { m_view.cut_selection(); }},
        {"edit_copy", [this] { m_view.copy_selection(); }},
        {"edit_paste_insert", [this] { m_view.paste(true); }},
        {"edit_paste_overwrite", [this] { m_view.paste(false); }},
        {"edit_fill", [this] { action_fill(); }},
        {"edit_zero", [this] { m_view.fill_selection(0x00); }},
        {"edit_select_all", [this] {
             m_view.select_all();
             status("Selected all.");
         }},
        {"edit_mode", [this] {
             const bool on = m_checks.at("edit_mode")->get_active();
             m_view.set_edit_mode(on);
             status(on ? "Edit mode on." : "Read-only: the file cannot be changed.");
         }},
        {"insert_mode", [this] {
             const bool on = m_checks.at("insert_mode")->get_active();
             m_view.set_insert_mode(on);
             status(on ? "Insert mode: typing adds bytes." : "Overwrite mode: typing replaces bytes.");
         }},

        // View
        {"view_hex_ascii", [this] {
             if (m_radios.at("view_hex_ascii")->get_active()) m_view.set_layout(HexView::Layout::HexAndAscii);
         }},
        {"view_hex_only", [this] {
             if (m_radios.at("view_hex_only")->get_active()) m_view.set_layout(HexView::Layout::HexOnly);
         }},
        {"view_ascii_only", [this] {
             if (m_radios.at("view_ascii_only")->get_active()) m_view.set_layout(HexView::Layout::AsciiOnly);
         }},
        {"view_dark", [this] {
             m_dark_mode = m_checks.at("view_dark")->get_active();
             apply_theme();
             status(m_dark_mode ? "Dark mode enabled." : "Light mode enabled.");
         }},

        // Search
        {"find_bytes", [this] { action_find(-1); }},
        {"find_ascii", [this] { action_find(static_cast<int>(FindMode::Ascii)); }},
        {"find_hex", [this] { action_find(static_cast<int>(FindMode::Hex)); }},
        {"find_next", [this] { find_again(true); }},
        {"find_prev", [this] { find_again(false); }},
        {"goto_offset", [this] { action_goto(); }},

        // Help
        {"help_shortcuts", [this] { action_shortcuts(); }},
        {"help_about", [this] { action_about(); }},
    };
}

void MainWindow::build_menus() {
    std::map<std::string, Gtk::RadioMenuItem::Group> radio_groups;

    auto add_accels = [this](Gtk::MenuItem& mi, const std::string& spec) {
        bool first = true;
        std::size_t start = 0;
        while (start <= spec.size()) {
            std::size_t bar = spec.find('|', start);
            if (bar == std::string::npos) bar = spec.size();
            const std::string one = spec.substr(start, bar - start);
            start = bar + 1;
            if (one.empty()) continue;
            guint key = 0;
            GdkModifierType mods = static_cast<GdkModifierType>(0);
            gtk_accelerator_parse(one.c_str(), &key, &mods);
            if (key == 0) continue;
            // Only the first accelerator is shown next to the menu entry.
            mi.add_accelerator("activate", m_accel, key, static_cast<Gdk::ModifierType>(mods),
                               first ? Gtk::ACCEL_VISIBLE : static_cast<Gtk::AccelFlags>(0));
            first = false;
        }
    };

    for (const auto& menu : buildHexEditorMenus()) {
        auto* top = Gtk::make_managed<Gtk::MenuItem>(menu.title, true);
        auto* submenu = Gtk::make_managed<Gtk::Menu>();

        for (const auto& def : menu.items) {
            if (def.kind == MenuItem::Kind::Separator) {
                submenu->append(*Gtk::make_managed<Gtk::SeparatorMenuItem>());
                continue;
            }

            Gtk::MenuItem* mi = nullptr;
            if (def.kind == MenuItem::Kind::Check) {
                auto* c = Gtk::make_managed<Gtk::CheckMenuItem>(def.label, true);
                m_checks[def.action] = c;
                mi = c;
            } else if (def.kind == MenuItem::Kind::Radio) {
                auto* r = Gtk::make_managed<Gtk::RadioMenuItem>(radio_groups[def.group], def.label, true);
                m_radios[def.action] = r;
                mi = r;
            } else {
                mi = Gtk::make_managed<Gtk::MenuItem>(def.label, true);
            }

            const auto handler = m_actions.find(def.action);
            if (handler == m_actions.end()) {
                mi->set_sensitive(false);   // not implemented yet
            } else {
                const std::function<void()> fn = handler->second;
                mi->signal_activate().connect([this, fn] {
                    if (!m_syncing_menu) fn();
                });
                if (!def.accel.empty()) add_accels(*mi, def.accel);
            }
            submenu->append(*mi);
        }

        top->set_submenu(*submenu);
        m_menu_bar.append(*top);
    }
}

void MainWindow::apply_theme() {
    // Ask GTK for the dark variant of the current theme (menus, dialogs and
    // scrollbars follow), and switch the hex view's own palette to match.
    auto settings = Gtk::Settings::get_default();
    if (settings) settings->property_gtk_application_prefer_dark_theme() = m_dark_mode;
    m_view.set_dark_mode(m_dark_mode);
}

// ---------------------------------------------------------------- UI state

void MainWindow::status(const std::string& message) {
    m_statusbar.pop(m_status_context);
    m_statusbar.push(message, m_status_context);
}

void MainWindow::update_title() {
    const std::string name = m_buffer.path().empty() ? "Untitled" : Glib::path_get_basename(m_buffer.path());
    set_title(std::string(m_buffer.dirty() ? "*" : "") + name + " - HexEdit Pro");
}

void MainWindow::update_info() {
    const std::size_t cur = m_view.cursor();
    char buf[96];
    std::string s;

    std::snprintf(buf, sizeof buf, "Offset 0x%08zX (%zu)", cur, cur);
    s = buf;
    if (cur < m_buffer.size()) {
        std::snprintf(buf, sizeof buf, "   Value 0x%02X (%u)", m_buffer.at(cur), m_buffer.at(cur));
        s += buf;
    }
    std::size_t a = 0, b = 0;
    if (m_view.selection_range(a, b)) s += "   Selected " + with_commas(b - a);
    s += "   Size " + with_commas(m_buffer.size());

    if (!m_view.edit_mode()) s += "   READ-ONLY";
    else s += m_view.insert_mode() ? "   INS" : "   OVR";
    s += m_view.active_pane() == HexView::Pane::Hex ? "   HEX" : "   TEXT";

    m_info.set_markup("<tt>" + Glib::Markup::escape_text(s) + "</tt>");
}

// Keeps the check and radio items in step with the real state (the Insert key
// and dialogs change it without going through the menu).
void MainWindow::sync_menus() {
    if (m_checks.empty()) return;
    m_syncing_menu = true;

    auto set_check = [this](const char* id, bool value) {
        const auto it = m_checks.find(id);
        if (it != m_checks.end() && it->second->get_active() != value) it->second->set_active(value);
    };
    set_check("edit_mode", m_view.edit_mode());
    set_check("insert_mode", m_view.insert_mode());
    set_check("view_dark", m_dark_mode);

    const char* active = "view_hex_ascii";
    if (m_view.layout() == HexView::Layout::HexOnly) active = "view_hex_only";
    if (m_view.layout() == HexView::Layout::AsciiOnly) active = "view_ascii_only";
    const auto r = m_radios.find(active);
    if (r != m_radios.end() && !r->second->get_active()) r->second->set_active(true);

    m_syncing_menu = false;
}

void MainWindow::show_error(const std::string& title, const std::string& detail) {
    Gtk::MessageDialog dlg(*this, title, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
    dlg.set_secondary_text(detail);
    dlg.run();
}

// ------------------------------------------------------------------- Files

bool MainWindow::confirm_discard() {
    if (!m_buffer.dirty()) return true;

    const std::string name = m_buffer.path().empty() ? "Untitled" : Glib::path_get_basename(m_buffer.path());
    Gtk::MessageDialog dlg(*this, "Save changes to \"" + name + "\"?", false, Gtk::MESSAGE_WARNING,
                           Gtk::BUTTONS_NONE, true);
    dlg.set_secondary_text("Your changes will be lost if you don't save them.");
    dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("Discard Changes", Gtk::RESPONSE_REJECT);
    dlg.add_button("Save", Gtk::RESPONSE_ACCEPT);
    dlg.set_default_response(Gtk::RESPONSE_ACCEPT);

    switch (dlg.run()) {
        case Gtk::RESPONSE_ACCEPT:
            return save_current();
        case Gtk::RESPONSE_REJECT:
            return true;
        default:
            return false;
    }
}

bool MainWindow::on_delete_event(GdkEventAny* event) {
    if (!confirm_discard()) return true;   // stay open
    return Gtk::Window::on_delete_event(event);
}

bool MainWindow::choose_file(const std::string& title, Gtk::FileChooserAction action, std::string& path) {
    Gtk::FileChooserDialog dlg(*this, title, action);
    dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button(action == Gtk::FILE_CHOOSER_ACTION_OPEN ? "Open" : "Save", Gtk::RESPONSE_OK);
    if (action == Gtk::FILE_CHOOSER_ACTION_SAVE) {
        dlg.set_do_overwrite_confirmation(true);
        if (!path.empty()) {
            dlg.set_current_folder(Glib::path_get_dirname(path));
            dlg.set_current_name(Glib::path_get_basename(path));
        }
    }
    if (dlg.run() != Gtk::RESPONSE_OK) return false;
    path = dlg.get_filename();
    return true;
}

bool MainWindow::open_file(const std::string& path, bool read_only) {
    if (!confirm_discard()) return false;

    std::string error;
    if (!m_buffer.load(path, &error)) {
        show_error("Could not open the file", path + "\n\n" + error);
        return false;
    }
    m_view.reset_view();
    m_view.set_edit_mode(!read_only);
    update_title();
    update_info();
    status("Loaded " + path + " (" + with_commas(m_buffer.size()) + " bytes)" +
           (read_only ? " - read-only" : ""));
    return true;
}

void MainWindow::action_open(bool read_only) {
    std::string path;
    if (!choose_file(read_only ? "Open Binary File (Read-Only)" : "Open Binary File",
                     Gtk::FILE_CHOOSER_ACTION_OPEN, path)) {
        return;
    }
    open_file(path, read_only);
}

void MainWindow::action_new() {
    if (!confirm_discard()) return;
    m_buffer.clear();
    m_view.reset_view();
    m_view.set_edit_mode(true);
    update_title();
    update_info();
    status("New empty buffer. Type hex digits to add bytes.");
}

bool MainWindow::save_to(const std::string& path) {
    std::string error;
    if (!m_buffer.save(path, &error)) {
        show_error("Could not save the file", path + "\n\n" + error + "\n\nThe original file was not changed.");
        return false;
    }
    update_title();
    status("Saved " + path);
    return true;
}

bool MainWindow::save_as() {
    std::string path = m_buffer.path();
    if (!choose_file("Save Binary File As", Gtk::FILE_CHOOSER_ACTION_SAVE, path)) return false;
    return save_to(path);
}

bool MainWindow::save_current() {
    if (m_buffer.path().empty()) return save_as();
    return save_to(m_buffer.path());
}

void MainWindow::action_export() {
    std::size_t s = 0, e = 0;
    if (!m_view.selection_range(s, e)) {
        status("Export: select some bytes first.");
        return;
    }
    std::string path;
    if (!choose_file("Export Selection As", Gtk::FILE_CHOOSER_ACTION_SAVE, path)) return;

    const auto& d = m_buffer.bytes();
    HexBuffer out;
    out.insert(0, HexBuffer::Bytes(d.begin() + static_cast<std::ptrdiff_t>(s),
                                   d.begin() + static_cast<std::ptrdiff_t>(e)));
    std::string error;
    if (!out.save(path, &error)) {
        show_error("Could not export the selection", path + "\n\n" + error);
        return;
    }
    status("Exported " + with_commas(e - s) + " bytes to " + path);
}

// -------------------------------------------------------------------- Edit

void MainWindow::action_undo() {
    if (!m_view.edit_mode()) {
        status("Read-only: turn on Edit Mode (Ctrl+E) to undo.");
        return;
    }
    const auto where = m_buffer.undo();
    if (!where) {
        status("Nothing to undo.");
        return;
    }
    m_view.refresh();
    m_view.set_cursor(*where);
    update_title();
    update_info();
    status("Undo.");
}

void MainWindow::action_redo() {
    if (!m_view.edit_mode()) {
        status("Read-only: turn on Edit Mode (Ctrl+E) to redo.");
        return;
    }
    const auto where = m_buffer.redo();
    if (!where) {
        status("Nothing to redo.");
        return;
    }
    m_view.refresh();
    m_view.set_cursor(*where);
    update_title();
    update_info();
    status("Redo.");
}

void MainWindow::action_fill() {
    std::size_t s = 0, e = 0;
    if (!m_view.selection_range(s, e)) {
        status("Fill: select some bytes first.");
        return;
    }

    Gtk::Dialog dlg("Fill Selection", *this, true);
    dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("Fill", Gtk::RESPONSE_OK);
    dlg.set_default_response(Gtk::RESPONSE_OK);

    Gtk::Box* box = dlg.get_content_area();
    box->set_spacing(8);
    box->set_border_width(12);
    Gtk::Label label("Byte value in hex (00 to FF) for " + with_commas(e - s) + " selected byte(s):");
    label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry entry;
    entry.set_max_length(4);
    entry.set_text("00");
    entry.set_activates_default(true);
    Gtk::Label error;
    error.set_halign(Gtk::ALIGN_START);
    box->pack_start(label, Gtk::PACK_SHRINK);
    box->pack_start(entry, Gtk::PACK_SHRINK);
    box->pack_start(error, Gtk::PACK_SHRINK);
    dlg.show_all_children();
    entry.grab_focus();

    while (dlg.run() == Gtk::RESPONSE_OK) {
        HexBuffer::Bytes value;
        std::string err;
        if (!hexutil::parse_hex(entry.get_text(), value, &err) || value.size() != 1) {
            error.set_text(err.empty() ? "Enter exactly one byte, such as FF or 0A." : err);
            continue;
        }
        dlg.hide();
        m_view.fill_selection(value[0]);
        return;
    }
}

// ------------------------------------------------------------------ Search

void MainWindow::action_find(int mode) {
    if (mode >= 0) m_find_mode = (mode == static_cast<int>(FindMode::Ascii)) ? FindMode::Ascii : FindMode::Hex;

    Gtk::Dialog dlg("Find", *this, true);
    dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("Find Previous", kResponseFindPrevious);
    dlg.add_button("Find Next", Gtk::RESPONSE_OK);
    dlg.set_default_response(Gtk::RESPONSE_OK);

    Gtk::Grid grid;
    grid.set_row_spacing(8);
    grid.set_column_spacing(8);
    grid.set_border_width(12);

    Gtk::Label type_label("Search for:");
    type_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText type;
    type.append("Hex bytes (e.g. DE AD BE EF)");
    type.append("ASCII text");
    type.set_active(static_cast<int>(m_find_mode));

    Gtk::Label pattern_label("Pattern:");
    pattern_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry entry;
    entry.set_width_chars(36);
    entry.set_text(m_find_text);
    entry.set_activates_default(true);

    Gtk::CheckButton wrap("Wrap around at the end of the file");
    wrap.set_active(m_find_wrap);
    Gtk::Label error;
    error.set_halign(Gtk::ALIGN_START);

    grid.attach(type_label, 0, 0);
    grid.attach(type, 1, 0);
    grid.attach(pattern_label, 0, 1);
    grid.attach(entry, 1, 1);
    grid.attach(wrap, 0, 2, 2, 1);
    grid.attach(error, 0, 3, 2, 1);
    dlg.get_content_area()->pack_start(grid, Gtk::PACK_SHRINK);
    dlg.show_all_children();
    entry.grab_focus();

    for (;;) {
        const int response = dlg.run();
        if (response != Gtk::RESPONSE_OK && response != kResponseFindPrevious) return;

        const FindMode chosen = type.get_active_row_number() == 1 ? FindMode::Ascii : FindMode::Hex;
        const std::string text = entry.get_text();
        HexBuffer::Bytes bytes;
        std::string err;
        if (chosen == FindMode::Hex) {
            if (!hexutil::parse_hex(text, bytes, &err)) {
                error.set_text(err);
                continue;
            }
        } else {
            if (text.empty()) {
                error.set_text("Enter some text to search for.");
                continue;
            }
            bytes = hexutil::from_text(text);
        }

        m_find_mode = chosen;
        m_find_text = text;
        m_find_bytes = std::move(bytes);
        m_find_wrap = wrap.get_active();
        dlg.hide();
        find_again(response == Gtk::RESPONSE_OK);
        return;
    }
}

void MainWindow::find_again(bool forward) {
    if (m_find_bytes.empty()) {
        action_find(-1);
        return;
    }

    const std::size_t size = m_buffer.size();
    std::size_t s = 0, e = 0;
    const bool has_selection = m_view.selection_range(s, e);

    // Continue after (or before) the current match, or from the cursor.
    std::size_t from = m_view.cursor();
    bool can_search = true;
    if (forward) {
        if (has_selection) from = s + 1;
    } else if (has_selection) {
        if (s > 0) from = s - 1;
        else if (m_find_wrap) from = size;
        else can_search = false;
    }

    const std::size_t pos =
        can_search ? m_buffer.find(m_find_bytes, from, forward, m_find_wrap) : HexBuffer::npos;
    if (pos == HexBuffer::npos) {
        status("Not found: " + m_find_text);
        return;
    }

    const bool wrapped = forward ? pos < from : (pos > from || (has_selection && s == 0));
    m_view.select_range(pos, pos + m_find_bytes.size());
    status("Found at " + offset_text(pos) + (wrapped ? " (wrapped around)" : ""));
}

void MainWindow::action_goto() {
    Gtk::Dialog dlg("Go To Offset", *this, true);
    dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("Go", Gtk::RESPONSE_OK);
    dlg.set_default_response(Gtk::RESPONSE_OK);

    Gtk::Box* box = dlg.get_content_area();
    box->set_spacing(8);
    box->set_border_width(12);
    Gtk::Label label("Offset (decimal, or hex with a 0x prefix, e.g. 0x1F40):");
    label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry entry;
    entry.set_activates_default(true);
    Gtk::Label error;
    error.set_halign(Gtk::ALIGN_START);
    box->pack_start(label, Gtk::PACK_SHRINK);
    box->pack_start(entry, Gtk::PACK_SHRINK);
    box->pack_start(error, Gtk::PACK_SHRINK);
    dlg.show_all_children();
    entry.grab_focus();

    while (dlg.run() == Gtk::RESPONSE_OK) {
        std::size_t off = 0;
        if (!hexutil::parse_offset(entry.get_text(), off)) {
            error.set_text("Not a valid offset. Use decimal, or hex with a 0x prefix.");
            continue;
        }
        if (off > m_buffer.size()) {
            error.set_text("Beyond the end of the file (" + with_commas(m_buffer.size()) + " bytes).");
            continue;
        }
        dlg.hide();
        m_view.set_cursor(off, /*center=*/true);
        status("Moved to " + offset_text(m_view.cursor()));
        return;
    }
}

// -------------------------------------------------------------------- Help

void MainWindow::action_shortcuts() {
    Gtk::MessageDialog dlg(*this, "Keyboard Shortcuts", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_CLOSE, true);
    dlg.set_secondary_text(
        "Moving:  arrows, PgUp/PgDn, Home/End, Ctrl+Home/End\n"
        "Select:  Shift + any move key, or drag with the mouse; Ctrl+A selects all\n"
        "Pane:    Tab switches between the hex and text columns\n"
        "Typing:  hex digits in the hex column, any text in the text column\n"
        "Mode:    Ins toggles insert/overwrite; Ctrl+E toggles edit mode\n"
        "Delete:  Del and Backspace remove bytes (or the selection)\n"
        "Undo:    Ctrl+Z, Ctrl+Y (or Ctrl+Shift+Z)\n"
        "Copy:    Ctrl+C, Ctrl+X, Ctrl+V (insert), Ctrl+Shift+V (overwrite)\n"
        "Search:  Ctrl+F find, F3 next, Shift+F3 previous, Ctrl+G go to offset\n"
        "File:    Ctrl+N new, Ctrl+O open, Ctrl+S save, Ctrl+Shift+S save as, Ctrl+Q quit");
    dlg.run();
}

void MainWindow::action_about() {
    Gtk::AboutDialog dlg;
    dlg.set_transient_for(*this);
    dlg.set_program_name("HexEdit Pro");
    dlg.set_version(HEXPRO_VERSION);
    dlg.set_comments("Hex viewer and editor for Pop!_OS, built with gtkmm.");
    dlg.set_copyright("Copyright 2024-2026 Dr. Eric O. Flores");
    dlg.set_license_type(Gtk::LICENSE_MIT_X11);
    dlg.set_authors({"Dr. Eric O. Flores", "ChatGPT-assisted"});
    dlg.run();
}
