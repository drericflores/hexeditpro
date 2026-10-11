#ifndef HEXVIEW_HPP
#define HEXVIEW_HPP

#include <gtkmm.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "HexBuffer.hpp"

// The hex editing widget. It draws the address, hex and ASCII columns itself
// with Cairo/Pango and only renders the rows that are on screen, so file size
// does not matter. It reads straight from the HexBuffer (no text copies that
// can drift out of sync) and edits the buffer directly.
//
// A selection is the range between an anchor and the cursor, both positions
// between bytes. Shift+Right on a byte selects exactly that byte, a drag
// includes the byte where it started and the byte where it ends, and
// double-click selects one byte (the old text-view mapping lost these).
class HexView : public Gtk::Box {
public:
    enum class Pane { Hex, Ascii };
    enum class Layout { HexAndAscii, HexOnly, AsciiOnly };

    HexView();

    // The buffer is owned by the caller and must outlive the view.
    void set_buffer(HexBuffer* buffer);

    // Call after the buffer was changed from outside (undo, redo, load...).
    void refresh();
    // Call after loading or clearing: cursor and scroll back to the start.
    void reset_view();
    void focus_canvas() { m_area.grab_focus(); }

    void set_edit_mode(bool on);
    bool edit_mode() const { return m_edit_mode; }
    void set_insert_mode(bool on);
    bool insert_mode() const { return m_insert_mode; }
    void set_dark_mode(bool on);
    void set_layout(Layout layout);
    Layout layout() const { return m_layout; }
    Pane active_pane() const { return m_pane; }

    // --- Cursor and selection ---------------------------------------------
    std::size_t cursor() const { return m_cursor; }
    // Selected bytes as [start, end). False when nothing is selected.
    bool selection_range(std::size_t& start, std::size_t& end) const;
    void set_cursor(std::size_t pos, bool center = false);
    // Selects [start, end) and scrolls it into view.
    void select_range(std::size_t start, std::size_t end, bool center = true);
    void select_all();

    // --- Operations (each returns false if it could not run) --------------
    // Editing operations refuse to run unless edit mode is on and tell the
    // user why through signal_notice().
    bool copy_selection();
    bool cut_selection();
    bool paste(bool insert);
    bool fill_selection(std::uint8_t value);
    bool delete_selection();

    // The buffer was changed by the user (typing, delete, paste...).
    sigc::signal<void> signal_edited() { return m_signal_edited; }
    // Cursor, selection or mode changed (for the status bar).
    sigc::signal<void> signal_cursor_changed() { return m_signal_cursor; }
    // A short message for the status bar.
    sigc::signal<void, const std::string&> signal_notice() { return m_signal_notice; }

private:
    struct Hit {
        std::size_t index = 0;
        Pane pane = Pane::Hex;
        int nibble = 0;
    };

    // Drawing
    bool on_draw_area(const Cairo::RefPtr<Cairo::Context>& cr);
    void update_metrics();
    void update_scroll_range();
    int visible_rows() const;

    // Input
    bool on_button_press(GdkEventButton* e);
    bool on_button_release(GdkEventButton* e);
    bool on_motion(GdkEventMotion* e);
    bool on_scroll(GdkEventScroll* e);
    bool on_key_press(GdkEventKey* e);
    void on_area_size_allocate(Gtk::Allocation& alloc);

    Hit hit_test(double x, double y) const;
    std::size_t max_cursor() const;
    void move_to(long long target, bool extend);
    void ensure_visible(std::size_t pos);
    void center_on(std::size_t pos);
    void cursor_moved();
    void after_edit(std::size_t new_cursor);
    void collapse_selection_to_start();
    bool require_edit();
    void notice(const std::string& message) { m_signal_notice.emit(message); }

    void type_hex_digit(int digit);
    void type_ascii(char c);
    void delete_forward();
    void delete_backward();

    HexBuffer* m_buf = nullptr;

    Gtk::DrawingArea m_area;
    Glib::RefPtr<Gtk::Adjustment> m_adj;
    Gtk::Scrollbar m_vscroll;

    Pango::FontDescription m_font;
    bool m_metrics_valid = false;
    double m_cw = 9.0;      // width of one character cell
    int m_ch = 18;          // height of one row
    int m_header_h = 22;

    std::size_t m_cursor = 0;
    std::size_t m_anchor = 0;
    std::size_t m_press = 0;     // byte where the current mouse drag began
    bool m_selecting = false;
    bool m_dragging = false;
    int m_nibble = 0;       // 0 = high nibble is next, 1 = low nibble is next
    double m_scroll_accum = 0.0;

    Pane m_pane = Pane::Hex;
    Layout m_layout = Layout::HexAndAscii;
    bool m_edit_mode = true;
    bool m_insert_mode = false;
    bool m_dark = false;

    sigc::signal<void> m_signal_edited;
    sigc::signal<void> m_signal_cursor;
    sigc::signal<void, const std::string&> m_signal_notice;
};

#endif
