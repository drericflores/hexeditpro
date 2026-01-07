#ifndef HEXVIEWWIDGET_HPP
#define HEXVIEWWIDGET_HPP

#include <gtkmm.h>
#include "HexBuffer.hpp"
#include <cstddef>
#include <vector>
#include <string>

class HexViewWidget : public Gtk::Box {
public:
    HexViewWidget();

    void update_display(const HexBuffer& buffer);
    void clear_display();

    void set_edit_mode(bool enable);

    // --- Byte-level operations against HexBuffer, based on current selection ---
    bool get_selected_byte_range(std::size_t& start, std::size_t& end) const; // [start,end)
    bool copy_bytes_to_clipboard(const HexBuffer& buffer);
    bool cut_bytes(HexBuffer& buffer);
    bool paste_insert(HexBuffer& buffer);
    bool paste_overwrite(HexBuffer& buffer);
    bool fill_selection(HexBuffer& buffer, unsigned char value);
    bool select_all();

    void scroll_to_byte(std::size_t byte_index);

private:
    static constexpr std::size_t kBytesPerLine = 16;

    Gtk::Paned m_paned_outer{Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Paned m_paned_inner{Gtk::ORIENTATION_HORIZONTAL};

    Gtk::ScrolledWindow m_scroll_addr;
    Gtk::ScrolledWindow m_scroll_hex;
    Gtk::ScrolledWindow m_scroll_ascii;

    Gtk::TextView m_addr_view;
    Gtk::TextView m_hex_view;
    Gtk::TextView m_ascii_view;

    bool m_syncing{false};

    void setup_view(Gtk::TextView& tv, bool editable);
    void setup_sync();

    void on_mark_set_hex(const Gtk::TextBuffer::iterator&,
                         const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark);
    void on_mark_set_ascii(const Gtk::TextBuffer::iterator&,
                           const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark);

    Gtk::TextView* focused_editor() const;

    std::size_t iter_to_byte_offset(const Gtk::TextView& tv,
                                    const Gtk::TextBuffer::iterator& it) const;

    static bool parse_hex_text(const std::string& text, std::vector<unsigned char>& out);
    static std::string bytes_to_hex_string(const std::vector<unsigned char>& bytes);

    void sync_cursors_from(Gtk::TextView* source);
};

#endif
