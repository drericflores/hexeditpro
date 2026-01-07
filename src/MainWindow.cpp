#include "MainWindow.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <vector>

MainWindow::MainWindow() {
    set_title("HexEdit Pro");
    set_default_size(1200, 800);

    m_css_provider = Gtk::CssProvider::create();

    setup_complex_menus();

    m_vbox.pack_start(m_menu_bar, Gtk::PACK_SHRINK);

    m_scroll.add(m_hex_display);
    m_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_vbox.pack_start(m_scroll, Gtk::PACK_EXPAND_WIDGET);

    m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);

    add(m_vbox);

    apply_theme();
    show_all_children();
}

MainWindow::~MainWindow() = default;

void MainWindow::status(const std::string& msg) {
    m_statusbar.push(msg);
}

void MainWindow::apply_theme() {
    const char* css_light =
        "window { background: #F5F6F7; }"
        "textview { background: #FFFFFF; color: #111111; }";

    const char* css_dark =
        "window { background: #1E1F22; }"
        "textview { background: #15161A; color: #E6E6E6; }";

    try {
        m_css_provider->load_from_data(m_dark_mode ? css_dark : css_light);
        auto screen = Gdk::Screen::get_default();
        if (screen) {
            Gtk::StyleContext::add_provider_for_screen(
                screen, m_css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }
    } catch (const Glib::Error&) {
        // ignore
    }
}

void MainWindow::setup_complex_menus() {
    auto menu_defs = buildHexEditorMenus();

    for (const auto& m_def : menu_defs) {
        auto* gtk_menu_item = Gtk::make_managed<Gtk::MenuItem>(m_def.title, true);
        auto* gtk_submenu = Gtk::make_managed<Gtk::Menu>();

        for (const auto& i_def : m_def.items) {
            if (i_def.separator) {
                gtk_submenu->append(*Gtk::make_managed<Gtk::SeparatorMenuItem>());
                continue;
            }

            auto* item = Gtk::make_managed<Gtk::MenuItem>(i_def.label, true);

            // File
            if (i_def.label == "New")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_file_new));
            else if (i_def.label == "Open...")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_file_open));
            else if (i_def.label == "Save")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_file_save));
            else if (i_def.label == "Save As...")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_file_save_as));
            else if (i_def.label == "Quit")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_file_quit));

            // Edit
            else if (i_def.label == "Undo")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_undo));
            else if (i_def.label == "Redo")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_redo));
            else if (i_def.label == "Cut Bytes")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_cut_bytes));
            else if (i_def.label == "Copy Bytes")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_copy_bytes));
            else if (i_def.label == "Paste Insert")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_paste_insert));
            else if (i_def.label == "Paste Overwrite")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_paste_overwrite));
            else if (i_def.label == "Fill Selection...")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_fill_selection));
            else if (i_def.label == "Zero Selection")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_zero_selection));
            else if (i_def.label == "Select All")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_edit_select_all));

            // Search / Analysis / Help
            else if (i_def.label == "Find Bytes...")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_search_find_bytes));
            else if (i_def.label == "Byte Frequency")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_analysis_frequency));
            else if (i_def.label == "About")
                item->signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_help_about));
            else {
                item->signal_activate().connect([i_def] {
                    if (i_def.action) i_def.action();
                });
            }

            gtk_submenu->append(*item);
        }

        gtk_menu_item->set_submenu(*gtk_submenu);
        m_menu_bar.append(*gtk_menu_item);
    }

    // Add Dark Mode toggle under View menu
    for (auto* w : m_menu_bar.get_children()) {
        auto* mi = dynamic_cast<Gtk::MenuItem*>(w);
        if (!mi || mi->get_label() != "View") continue;

        auto* view_sub = dynamic_cast<Gtk::Menu*>(mi->get_submenu());
        if (!view_sub) break;

        view_sub->append(*Gtk::make_managed<Gtk::SeparatorMenuItem>());
        auto* dark_item = Gtk::make_managed<Gtk::CheckMenuItem>("Dark Mode");
        dark_item->set_active(m_dark_mode);
        dark_item->signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::on_view_theme_toggle));
        view_sub->append(*dark_item);
        break;
    }
}

// ---------------- File ----------------
void MainWindow::on_file_new() {
    m_buffer.clear();
    m_hex_display.clear_display();
    status("New buffer.");
}

void MainWindow::on_file_open() {
    Gtk::FileChooserDialog dialog(*this, "Open Binary File", Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Open", Gtk::RESPONSE_OK);

    if (dialog.run() != Gtk::RESPONSE_OK) return;

    const auto path = dialog.get_filename();
    if (!m_buffer.load(path)) {
        Gtk::MessageDialog err(*this, "Failed to open file.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        err.set_secondary_text(path);
        err.run();
        return;
    }
    m_hex_display.update_display(m_buffer);
    status("Loaded: " + path);
}

void MainWindow::on_file_save() {
    if (m_buffer.current_path.empty()) {
        on_file_save_as();
        return;
    }
    if (!m_buffer.save(m_buffer.current_path)) {
        Gtk::MessageDialog err(*this, "Failed to save file.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        err.set_secondary_text(m_buffer.current_path);
        err.run();
        return;
    }
    status("Saved: " + m_buffer.current_path);
}

void MainWindow::on_file_save_as() {
    Gtk::FileChooserDialog dialog(*this, "Save Binary File As", Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Save", Gtk::RESPONSE_OK);
    dialog.set_do_overwrite_confirmation(true);

    if (dialog.run() != Gtk::RESPONSE_OK) return;

    const auto path = dialog.get_filename();
    if (!m_buffer.save(path)) {
        Gtk::MessageDialog err(*this, "Failed to save file.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        err.set_secondary_text(path);
        err.run();
        return;
    }
    status("Saved: " + path);
}

void MainWindow::on_file_quit() {
    hide();
}

// ---------------- Edit ----------------
void MainWindow::on_edit_undo() { status("Undo: not implemented yet."); }
void MainWindow::on_edit_redo() { status("Redo: not implemented yet."); }

void MainWindow::on_edit_copy_bytes() {
    if (m_buffer.data.empty()) { status("Nothing to copy."); return; }
    if (!m_hex_display.copy_bytes_to_clipboard(m_buffer)) { status("Copy: no selection."); return; }
    status("Copied bytes.");
}

void MainWindow::on_edit_cut_bytes() {
    if (m_buffer.data.empty()) { status("Nothing to cut."); return; }
    if (!m_hex_display.cut_bytes(m_buffer)) { status("Cut: no selection."); return; }
    m_hex_display.update_display(m_buffer);
    status("Cut bytes.");
}

void MainWindow::on_edit_paste_insert() {
    if (m_buffer.data.empty()) { status("Paste: load a file first."); return; }
    if (!m_hex_display.paste_insert(m_buffer)) { status("Paste Insert failed (clipboard format?)."); return; }
    m_hex_display.update_display(m_buffer);
    status("Paste Insert complete.");
}

void MainWindow::on_edit_paste_overwrite() {
    if (m_buffer.data.empty()) { status("Paste: load a file first."); return; }
    if (!m_hex_display.paste_overwrite(m_buffer)) { status("Paste Overwrite failed (clipboard format?)."); return; }
    m_hex_display.update_display(m_buffer);
    status("Paste Overwrite complete.");
}

void MainWindow::on_edit_zero_selection() {
    if (m_buffer.data.empty()) { status("Nothing to modify."); return; }
    if (!m_hex_display.fill_selection(m_buffer, 0x00)) { status("Zero: no selection."); return; }
    m_hex_display.update_display(m_buffer);
    status("Selection zeroed.");
}

void MainWindow::on_edit_fill_selection() {
    if (m_buffer.data.empty()) { status("Nothing to modify."); return; }

    Gtk::Dialog dlg("Fill Selection", *this);
    dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("Fill", Gtk::RESPONSE_OK);

    auto* box = dlg.get_content_area();
    Gtk::Label label("Enter a byte value (hex), e.g. FF or 0A:");
    label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry entry;
    entry.set_placeholder_text("00..FF");
    entry.set_activates_default(true);

    box->pack_start(label, Gtk::PACK_SHRINK);
    box->pack_start(entry, Gtk::PACK_SHRINK);
    dlg.set_default_response(Gtk::RESPONSE_OK);
    dlg.show_all_children();

    if (dlg.run() != Gtk::RESPONSE_OK) return;

    std::string t = entry.get_text();
    std::string cleaned;
    for (unsigned char c : t) if (std::isxdigit(c)) cleaned.push_back(static_cast<char>(c));
    if (cleaned.size() != 2) { status("Fill: invalid byte value."); return; }

    unsigned int v = 0;
    std::istringstream iss(cleaned);
    iss >> std::hex >> v;
    if (v > 0xFFu) { status("Fill: invalid value."); return; }

    if (!m_hex_display.fill_selection(m_buffer, static_cast<unsigned char>(v))) {
        status("Fill: no selection.");
        return;
    }
    m_hex_display.update_display(m_buffer);
    status("Selection filled.");
}

void MainWindow::on_edit_select_all() {
    m_hex_display.select_all();
    status("Selected all.");
}

// ---------------- View ----------------
void MainWindow::on_view_theme_toggle() {
    m_dark_mode = !m_dark_mode;
    apply_theme();
    status(m_dark_mode ? "Dark mode enabled." : "Light mode enabled.");
}

// ---------------- Search / Analysis / Help (leave your existing versions or re-add) ----------------
void MainWindow::on_search_find_bytes() { status("Search: hook up your existing find logic here."); }
void MainWindow::on_analysis_frequency() { status("Analysis: hook up your existing frequency logic here."); }

void MainWindow::on_help_about() {
    Gtk::AboutDialog dlg;
    dlg.set_transient_for(*this);
    dlg.set_program_name("HexEdit Pro");
    dlg.set_version("0.2");
    dlg.set_comments("GTKmm hex viewer/editor with functional Edit menu operations.");
    dlg.set_authors({"Dr. Eric O. Flores", "ChatGPT-assisted"});
    dlg.run();
}
