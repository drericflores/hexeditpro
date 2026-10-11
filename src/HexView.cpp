#include "HexView.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "HexUtil.hpp"

namespace {

constexpr std::size_t kBytesPerRow = 16;
constexpr std::size_t kGroupSize = 8;      // extra gap after every 8 bytes
constexpr double kPadding = 8.0;
constexpr std::size_t kMaxCopyBytes = 64u * 1024u * 1024u;

struct Rgb {
    double r, g, b;
};

constexpr Rgb rgb(unsigned hex) {
    return {((hex >> 16) & 0xFF) / 255.0, ((hex >> 8) & 0xFF) / 255.0, (hex & 0xFF) / 255.0};
}

struct Palette {
    Rgb bg, header_bg, text, dim, grid, accent;
};

const Palette kLight{rgb(0xFFFFFF), rgb(0xF0F1F3), rgb(0x111111),
                     rgb(0x8A8F98), rgb(0xD5D8DD), rgb(0x2B6CD1)};
const Palette kDark{rgb(0x15161A), rgb(0x1E2024), rgb(0xE6E6E6),
                    rgb(0x7C8088), rgb(0x2E3138), rgb(0x4C8DF0)};

void set_color(const Cairo::RefPtr<Cairo::Context>& cr, const Rgb& c, double alpha = 1.0) {
    cr->set_source_rgba(c.r, c.g, c.b, alpha);
}

// Where each column starts, in pixels. Negative means "not shown".
struct Columns {
    int addr_digits = 8;
    double addr = 0;
    double hex = -1;
    double ascii = -1;
    double sep = -1;
};

Columns make_columns(double cw, std::size_t size, HexView::Layout layout) {
    Columns c;
    c.addr_digits = size > 0xFFFFFFFFull ? 16 : 8;
    c.addr = kPadding;
    double x = c.addr + (c.addr_digits + 2) * cw;
    const double hex_chars = static_cast<double>(kBytesPerRow * 3 - 1 + (kBytesPerRow - 1) / kGroupSize);
    if (layout != HexView::Layout::AsciiOnly) {
        c.hex = x;
        x += (hex_chars + 3) * cw;
    }
    if (layout != HexView::Layout::HexOnly) {
        c.ascii = x;
        if (c.hex >= 0) c.sep = x - 1.5 * cw;
    }
    return c;
}

double hex_x(const Columns& c, double cw, std::size_t i) {
    return c.hex + static_cast<double>(i * 3 + i / kGroupSize) * cw;
}

double ascii_x(const Columns& c, double cw, std::size_t i) {
    return c.ascii + static_cast<double>(i) * cw;
}

const char* kDigits = "0123456789ABCDEF";

void append_separator(std::string& s, std::size_t i) {
    if (i + 1 < kBytesPerRow) {
        s.push_back(' ');
        if ((i + 1) % kGroupSize == 0) s.push_back(' ');
    }
}

std::string hex_header() {
    std::string s;
    for (std::size_t i = 0; i < kBytesPerRow; ++i) {
        s.push_back(kDigits[(i >> 4) & 0xF]);
        s.push_back(kDigits[i & 0xF]);
        append_separator(s, i);
    }
    return s;
}

std::string hex_row(const HexBuffer::Bytes& d, std::size_t base) {
    std::string s;
    s.reserve(kBytesPerRow * 3 + 2);
    for (std::size_t i = 0; i < kBytesPerRow; ++i) {
        const std::size_t idx = base + i;
        if (idx < d.size()) {
            s.push_back(kDigits[d[idx] >> 4]);
            s.push_back(kDigits[d[idx] & 0xF]);
        } else {
            s += "  ";
        }
        append_separator(s, i);
    }
    return s;
}

std::string ascii_row(const HexBuffer::Bytes& d, std::size_t base) {
    std::string s;
    s.reserve(kBytesPerRow);
    for (std::size_t i = 0; i < kBytesPerRow; ++i) {
        const std::size_t idx = base + i;
        s.push_back(idx < d.size() ? hexutil::printable(d[idx]) : ' ');
    }
    return s;
}

int hex_digit_value(unsigned c) {
    if (c >= '0' && c <= '9') return static_cast<int>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<int>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<int>(c - 'A' + 10);
    return -1;
}

}  // namespace

// ------------------------------------------------------------ Construction

HexView::HexView()
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL),
      m_adj(Gtk::Adjustment::create(0, 0, 1, 1, 10, 1)),
      m_vscroll(m_adj, Gtk::ORIENTATION_VERTICAL),
      m_font("Monospace 11") {
    m_area.set_can_focus(true);
    m_area.set_hexpand(true);
    m_area.set_vexpand(true);
    m_area.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON1_MOTION_MASK |
                      Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK | Gdk::KEY_PRESS_MASK |
                      Gdk::FOCUS_CHANGE_MASK);

    m_area.signal_draw().connect(sigc::mem_fun(*this, &HexView::on_draw_area));
    m_area.signal_button_press_event().connect(sigc::mem_fun(*this, &HexView::on_button_press), false);
    m_area.signal_button_release_event().connect(sigc::mem_fun(*this, &HexView::on_button_release), false);
    m_area.signal_motion_notify_event().connect(sigc::mem_fun(*this, &HexView::on_motion), false);
    m_area.signal_scroll_event().connect(sigc::mem_fun(*this, &HexView::on_scroll), false);
    m_area.signal_key_press_event().connect(sigc::mem_fun(*this, &HexView::on_key_press), false);
    m_area.signal_size_allocate().connect(sigc::mem_fun(*this, &HexView::on_area_size_allocate));
    m_area.signal_focus_in_event().connect([this](GdkEventFocus*) {
        m_area.queue_draw();
        return false;
    });
    m_area.signal_focus_out_event().connect([this](GdkEventFocus*) {
        m_area.queue_draw();
        return false;
    });
    m_area.signal_style_updated().connect([this] {
        m_metrics_valid = false;
        m_area.queue_draw();
    });
    m_adj->signal_value_changed().connect([this] { m_area.queue_draw(); });

    pack_start(m_area, Gtk::PACK_EXPAND_WIDGET);
    pack_start(m_vscroll, Gtk::PACK_SHRINK);
    show_all_children();
}

void HexView::set_buffer(HexBuffer* buffer) {
    m_buf = buffer;
    reset_view();
}

void HexView::refresh() {
    m_nibble = 0;
    if (!m_buf) {
        m_cursor = m_anchor = 0;
        m_selecting = false;
    } else {
        m_cursor = std::min(m_cursor, m_selecting ? m_buf->size() : max_cursor());
        m_anchor = std::min(m_anchor, m_buf->size());
        std::size_t s, e;
        if (m_selecting && !selection_range(s, e)) m_selecting = false;
    }
    update_scroll_range();
    m_area.queue_draw();
    m_signal_cursor.emit();
}

void HexView::reset_view() {
    m_cursor = m_anchor = 0;
    m_selecting = false;
    m_dragging = false;
    m_adj->set_value(0);
    refresh();
}

void HexView::set_edit_mode(bool on) {
    m_edit_mode = on;
    refresh();
}

void HexView::set_insert_mode(bool on) {
    m_insert_mode = on;
    m_nibble = 0;
    m_area.queue_draw();
    m_signal_cursor.emit();
}

void HexView::set_dark_mode(bool on) {
    m_dark = on;
    m_area.queue_draw();
}

void HexView::set_layout(Layout layout) {
    m_layout = layout;
    if (layout == Layout::HexOnly) m_pane = Pane::Hex;
    if (layout == Layout::AsciiOnly) m_pane = Pane::Ascii;
    m_nibble = 0;
    m_area.queue_draw();
    m_signal_cursor.emit();
}

// ----------------------------------------------------- Metrics and scrolling

void HexView::update_metrics() {
    auto layout = m_area.create_pango_layout("0000000000");
    layout->set_font_description(m_font);
    int w = 0, h = 0;
    layout->get_size(w, h);
    m_cw = static_cast<double>(w) / PANGO_SCALE / 10.0;
    m_ch = static_cast<int>(std::ceil(static_cast<double>(h) / PANGO_SCALE)) + 2;
    m_header_h = m_ch + 4;
    m_metrics_valid = true;
}

int HexView::visible_rows() const {
    const int h = m_area.get_allocated_height();
    return std::max(1, (h - m_header_h) / std::max(1, m_ch));
}

void HexView::update_scroll_range() {
    if (!m_metrics_valid) update_metrics();
    const std::size_t size = m_buf ? m_buf->size() : 0;
    const double rows = static_cast<double>(size / kBytesPerRow + 1);
    const double visible = visible_rows();
    const double upper = std::max(rows, visible);
    const double value = std::min(m_adj->get_value(), std::max(0.0, upper - visible));
    m_adj->configure(value, 0, upper, 1, std::max(1.0, visible - 1), visible);
}

void HexView::on_area_size_allocate(Gtk::Allocation&) {
    update_scroll_range();
}

void HexView::ensure_visible(std::size_t pos) {
    const double row = static_cast<double>(pos / kBytesPerRow);
    const double first = std::floor(m_adj->get_value());
    const double visible = visible_rows();
    if (row < first) {
        m_adj->set_value(row);
    } else if (row >= first + visible) {
        m_adj->set_value(row - visible + 1);
    }
}

void HexView::center_on(std::size_t pos) {
    const double row = static_cast<double>(pos / kBytesPerRow);
    m_adj->set_value(std::max(0.0, row - visible_rows() / 2));
}

// ----------------------------------------------------------------- Drawing

bool HexView::on_draw_area(const Cairo::RefPtr<Cairo::Context>& cr) {
    if (!m_metrics_valid) {
        update_metrics();
        update_scroll_range();
    }
    const Palette& pal = m_dark ? kDark : kLight;
    const int W = m_area.get_allocated_width();
    const int H = m_area.get_allocated_height();

    set_color(cr, pal.bg);
    cr->paint();
    if (!m_buf) return true;

    const HexBuffer::Bytes& data = m_buf->bytes();
    const std::size_t size = data.size();
    const Columns cols = make_columns(m_cw, size, m_layout);
    const bool show_hex = cols.hex >= 0;
    const bool show_ascii = cols.ascii >= 0;

    auto layout = m_area.create_pango_layout("");
    layout->set_font_description(m_font);
    auto draw_text = [&](const std::string& s, double x, double y, const Rgb& c) {
        set_color(cr, c);
        layout->set_text(s);
        cr->move_to(x, y);
        layout->show_in_cairo_context(cr);
    };

    // Header row with the column numbers.
    set_color(cr, pal.header_bg);
    cr->rectangle(0, 0, W, m_header_h);
    cr->fill();
    set_color(cr, pal.grid);
    cr->set_line_width(1.0);
    cr->move_to(0, m_header_h - 0.5);
    cr->line_to(W, m_header_h - 0.5);
    cr->stroke();
    const double header_y = (m_header_h - m_ch) / 2.0 + 1;
    draw_text("Offset", cols.addr, header_y, pal.dim);
    if (show_hex) draw_text(hex_header(), cols.hex, header_y, pal.dim);
    if (show_ascii) draw_text("Decoded text", cols.ascii, header_y, pal.dim);

    // Line between the hex and ASCII columns.
    if (cols.sep >= 0) {
        set_color(cr, pal.grid);
        cr->move_to(cols.sep, m_header_h);
        cr->line_to(cols.sep, H);
        cr->stroke();
    }

    std::size_t sel_start = 0, sel_end = 0;
    const bool has_sel = selection_range(sel_start, sel_end);
    const bool focused = m_area.has_focus();
    const std::size_t first_row = static_cast<std::size_t>(std::floor(m_adj->get_value()));

    for (int r = 0;; ++r) {
        const double y = m_header_h + static_cast<double>(r) * m_ch;
        if (y >= H) break;
        const std::size_t row = first_row + static_cast<std::size_t>(r);
        const std::size_t base = row * kBytesPerRow;
        if (base > size) break;   // base == size is the empty row after the last byte

        // Selection.
        if (has_sel && sel_start < base + kBytesPerRow && sel_end > base) {
            const std::size_t a = std::max(sel_start, base) - base;
            const std::size_t b = std::min(sel_end, base + kBytesPerRow) - base - 1;
            if (show_hex) {
                set_color(cr, pal.accent, m_pane == Pane::Hex ? 0.35 : 0.18);
                const double x0 = hex_x(cols, m_cw, a);
                const double x1 = hex_x(cols, m_cw, b) + 2 * m_cw;
                cr->rectangle(x0, y, x1 - x0, m_ch);
                cr->fill();
            }
            if (show_ascii) {
                set_color(cr, pal.accent, m_pane == Pane::Ascii ? 0.35 : 0.18);
                const double x0 = ascii_x(cols, m_cw, a);
                const double x1 = ascii_x(cols, m_cw, b + 1);
                cr->rectangle(x0, y, x1 - x0, m_ch);
                cr->fill();
            }
        }

        // Cursor.
        if (m_cursor / kBytesPerRow == row && (m_edit_mode || m_cursor < size)) {
            const std::size_t i = m_cursor % kBytesPerRow;
            auto block = [&](double x, double w, bool active) {
                if (active && focused) {
                    if (m_edit_mode && m_insert_mode) {
                        set_color(cr, pal.accent);            // thin bar = insert
                        cr->rectangle(x, y, 2, m_ch);
                    } else {
                        set_color(cr, pal.accent, 0.6);       // block = overwrite / view
                        cr->rectangle(x, y, w, m_ch);
                    }
                    cr->fill();
                } else {
                    set_color(cr, pal.accent, active ? 0.8 : 0.6);
                    cr->set_line_width(1.0);
                    cr->rectangle(x + 0.5, y + 0.5, w - 1, m_ch - 1);
                    cr->stroke();
                }
            };
            if (show_hex) {
                const bool active = m_pane == Pane::Hex;
                if (active && m_edit_mode) {
                    block(hex_x(cols, m_cw, i) + (m_nibble ? m_cw : 0.0), m_cw, true);
                } else {
                    block(hex_x(cols, m_cw, i), 2 * m_cw, active);
                }
            }
            if (show_ascii) {
                block(ascii_x(cols, m_cw, i), m_cw, m_pane == Pane::Ascii);
            }
        }

        // Text.
        const double ty = y + 1;
        char addr[32];
        std::snprintf(addr, sizeof addr, "%0*llX", cols.addr_digits, static_cast<unsigned long long>(base));
        draw_text(addr, cols.addr, ty, pal.dim);
        if (show_hex) draw_text(hex_row(data, base), cols.hex, ty, pal.text);
        if (show_ascii) draw_text(ascii_row(data, base), cols.ascii, ty, pal.text);
    }
    return true;
}

// -------------------------------------------------------- Cursor / selection

std::size_t HexView::max_cursor() const {
    if (!m_buf || m_buf->empty()) return 0;
    return m_edit_mode ? m_buf->size() : m_buf->size() - 1;
}

bool HexView::selection_range(std::size_t& start, std::size_t& end) const {
    if (!m_selecting || !m_buf) return false;
    const std::size_t s = std::min(m_anchor, m_cursor);
    const std::size_t e = std::min(std::max(m_anchor, m_cursor), m_buf->size());
    if (s >= e) return false;
    start = s;
    end = e;
    return true;
}

void HexView::cursor_moved() {
    m_area.queue_draw();
    m_signal_cursor.emit();
}

void HexView::move_to(long long target, bool extend) {
    if (!m_buf) return;
    const std::size_t size = m_buf->size();
    // A selection may reach the very end of the file even in read-only mode.
    const long long maxc = static_cast<long long>(extend ? size : max_cursor());
    target = std::clamp(target, 0LL, maxc);
    if (extend) {
        if (!m_selecting) {
            m_anchor = m_cursor;
            m_selecting = true;
        }
        m_cursor = static_cast<std::size_t>(target);
    } else {
        m_selecting = false;
        m_anchor = m_cursor = static_cast<std::size_t>(target);
    }
    m_nibble = 0;
    ensure_visible(m_cursor);
    cursor_moved();
}

void HexView::set_cursor(std::size_t pos, bool center) {
    if (!m_buf) return;
    m_selecting = false;
    m_anchor = m_cursor = std::min(pos, max_cursor());
    m_nibble = 0;
    if (center) center_on(m_cursor); else ensure_visible(m_cursor);
    cursor_moved();
}

void HexView::select_range(std::size_t start, std::size_t end, bool center) {
    if (!m_buf || start >= m_buf->size()) return;
    end = std::min(end, m_buf->size());
    if (start >= end) return;
    m_anchor = end;
    m_cursor = start;
    m_selecting = true;
    m_nibble = 0;
    if (center) center_on(start); else ensure_visible(m_cursor);
    cursor_moved();
}

void HexView::select_all() {
    if (!m_buf || m_buf->empty()) return;
    m_anchor = 0;
    m_cursor = m_buf->size();
    m_selecting = true;
    m_nibble = 0;
    cursor_moved();
}

void HexView::collapse_selection_to_start() {
    std::size_t s, e;
    if (selection_range(s, e)) m_cursor = m_anchor = s;
    m_selecting = false;
    m_nibble = 0;
}

// ------------------------------------------------------------------- Mouse

HexView::Hit HexView::hit_test(double x, double y) const {
    Hit hit;
    const std::size_t size = m_buf ? m_buf->size() : 0;
    const Columns cols = make_columns(m_cw, size, m_layout);

    const double row_d = std::floor(m_adj->get_value()) + std::floor((y - m_header_h) / m_ch);
    const std::size_t last_row = size / kBytesPerRow;
    const std::size_t row = row_d < 0 ? 0 : std::min(static_cast<std::size_t>(row_d), last_row);

    if (cols.hex < 0) hit.pane = Pane::Ascii;
    else if (cols.ascii < 0) hit.pane = Pane::Hex;
    else hit.pane = x >= cols.sep ? Pane::Ascii : Pane::Hex;

    std::size_t i = 0;
    if (hit.pane == Pane::Ascii) {
        const double c = std::floor((x - cols.ascii) / m_cw);
        i = static_cast<std::size_t>(std::clamp(c, 0.0, static_cast<double>(kBytesPerRow - 1)));
    } else {
        // One group = 8 bytes * 3 characters + 1 gap = 25 characters.
        constexpr int kGroupChars = static_cast<int>(kGroupSize * 3 + 1);
        const double hex_chars = static_cast<double>(kBytesPerRow * 3 - 1 + (kBytesPerRow - 1) / kGroupSize);
        const int c = static_cast<int>(std::clamp(std::floor((x - cols.hex) / m_cw), 0.0, hex_chars - 1));
        const int group = c / kGroupChars;
        const int within = std::min(c % kGroupChars, kGroupChars - 2);   // the gap counts as the last digit
        i = std::min<std::size_t>(static_cast<std::size_t>(group) * kGroupSize + static_cast<std::size_t>(within / 3),
                                  kBytesPerRow - 1);
        hit.nibble = (within % 3 == 0) ? 0 : 1;
    }

    hit.index = row * kBytesPerRow + i;
    const std::size_t maxc = max_cursor();
    if (hit.index > maxc) {
        hit.index = maxc;
        hit.nibble = 0;
    }
    return hit;
}

bool HexView::on_button_press(GdkEventButton* e) {
    if (e->button != 1) return false;
    if (e->type != GDK_BUTTON_PRESS && e->type != GDK_2BUTTON_PRESS) return false;
    m_area.grab_focus();
    if (!m_buf) return true;

    const Hit h = hit_test(e->x, e->y);
    m_pane = h.pane;
    m_nibble = 0;
    const std::size_t size = m_buf->size();

    if (e->type == GDK_2BUTTON_PRESS) {
        // Double-click selects the byte under the pointer.
        m_dragging = false;
        if (h.index < size) {
            m_anchor = h.index + 1;
            m_cursor = h.index;
            m_selecting = true;
            cursor_moved();
        }
        return true;
    }

    m_dragging = true;
    if ((e->state & GDK_SHIFT_MASK) && size > 0) {
        // Extend from the current anchor to include the clicked byte.
        if (!m_selecting) {
            m_anchor = m_cursor;
            m_selecting = true;
        }
        const std::size_t b = std::min(h.index, size - 1);
        m_press = std::min(m_anchor, size - 1);
        m_cursor = (b >= m_anchor) ? b + 1 : b;
    } else {
        m_selecting = false;
        m_anchor = m_cursor = h.index;
        m_press = h.index;
        if (m_edit_mode && h.pane == Pane::Hex) m_nibble = h.nibble;
    }
    cursor_moved();
    return true;
}

bool HexView::on_button_release(GdkEventButton* e) {
    if (e->button == 1) m_dragging = false;
    return false;
}

bool HexView::on_motion(GdkEventMotion* e) {
    if (!m_dragging || !m_buf || m_buf->empty()) return false;
    const Hit h = hit_test(e->x, e->y);
    const std::size_t size = m_buf->size();
    const std::size_t idx = std::min(h.index, size - 1);
    if (idx != m_press || m_selecting) {
        // Both the byte where the drag started and the byte under the
        // pointer are included, whichever way the drag goes.
        m_selecting = true;
        if (idx >= m_press) {
            m_anchor = m_press;
            m_cursor = std::min(idx + 1, size);
        } else {
            m_anchor = std::min(m_press + 1, size);
            m_cursor = idx;
        }
        m_nibble = 0;
        ensure_visible(idx);
        cursor_moved();
    }
    return true;
}

bool HexView::on_scroll(GdkEventScroll* e) {
    double rows = 0;
    switch (e->direction) {
        case GDK_SCROLL_UP:
            rows = -3;
            break;
        case GDK_SCROLL_DOWN:
            rows = 3;
            break;
        case GDK_SCROLL_SMOOTH:
            m_scroll_accum += e->delta_y * 3.0;
            rows = std::trunc(m_scroll_accum);
            m_scroll_accum -= rows;
            break;
        default:
            return false;
    }
    if (rows != 0) m_adj->set_value(m_adj->get_value() + rows);
    return true;
}

// ---------------------------------------------------------------- Keyboard

bool HexView::on_key_press(GdkEventKey* e) {
    if (!m_buf) return false;
    const bool shift = e->state & GDK_SHIFT_MASK;
    const bool ctrl = e->state & GDK_CONTROL_MASK;
    const bool alt = e->state & GDK_MOD1_MASK;
    if (alt) return false;

    const long long row = static_cast<long long>(kBytesPerRow);
    const long long page = std::max(1, visible_rows() - 1) * row;
    const long long cur = static_cast<long long>(m_cursor);

    switch (e->keyval) {
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
            move_to(cur - 1, shift);
            return true;
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            move_to(cur + 1, shift);
            return true;
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            move_to(cur - row, shift);
            return true;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            move_to(cur + row, shift);
            return true;
        case GDK_KEY_Page_Up:
        case GDK_KEY_KP_Page_Up:
            move_to(cur - page, shift);
            return true;
        case GDK_KEY_Page_Down:
        case GDK_KEY_KP_Page_Down:
            move_to(cur + page, shift);
            return true;
        case GDK_KEY_Home:
        case GDK_KEY_KP_Home:
            move_to(ctrl ? 0 : cur - cur % row, shift);
            return true;
        case GDK_KEY_End:
        case GDK_KEY_KP_End:
            move_to(ctrl ? static_cast<long long>(max_cursor()) : cur - cur % row + row - 1, shift);
            return true;
        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
            if (m_layout == Layout::HexAndAscii) {
                m_pane = (m_pane == Pane::Hex) ? Pane::Ascii : Pane::Hex;
                m_nibble = 0;
                cursor_moved();
            }
            return true;
        case GDK_KEY_Insert:
        case GDK_KEY_KP_Insert:
            set_insert_mode(!m_insert_mode);
            return true;
        case GDK_KEY_Escape:
            if (m_selecting) {
                m_selecting = false;
                cursor_moved();
            }
            return true;
        case GDK_KEY_Delete:
        case GDK_KEY_KP_Delete:
            if (ctrl) return false;
            if (m_selecting) delete_selection(); else delete_forward();
            return true;
        case GDK_KEY_BackSpace:
            if (ctrl) return false;
            if (m_selecting) delete_selection(); else delete_backward();
            return true;
        default:
            break;
    }

    if (ctrl) return false;   // shortcuts are handled by the window's menu
    const guint32 uc = gdk_keyval_to_unicode(e->keyval);
    if (uc < 0x20 || uc >= 0x7F) return false;
    if (!require_edit()) return true;

    if (m_pane == Pane::Hex) {
        const int d = hex_digit_value(uc);
        if (d >= 0) type_hex_digit(d);
        else notice("Hex pane: type 0-9 and A-F (Tab switches to the text pane)");
    } else {
        type_ascii(static_cast<char>(uc));
    }
    return true;
}

bool HexView::require_edit() {
    if (m_edit_mode) return true;
    notice("Read-only: turn on Edit Mode (Ctrl+E) to change the file");
    return false;
}

void HexView::after_edit(std::size_t new_cursor) {
    m_selecting = false;
    m_cursor = m_anchor = std::min(new_cursor, max_cursor());
    update_scroll_range();
    ensure_visible(m_cursor);
    m_area.queue_draw();
    m_signal_edited.emit();
    m_signal_cursor.emit();
}

void HexView::type_hex_digit(int digit) {
    if (m_selecting) collapse_selection_to_start();
    const std::size_t pos = m_cursor;
    if (m_nibble == 0) {
        if (pos >= m_buf->size() || m_insert_mode) {
            m_buf->insert(pos, {static_cast<std::uint8_t>(digit << 4)});
        } else {
            const std::uint8_t old = m_buf->at(pos);
            m_buf->overwrite(pos, {static_cast<std::uint8_t>((digit << 4) | (old & 0x0F))});
        }
        m_nibble = 1;
        after_edit(pos);
    } else {
        const std::uint8_t old = pos < m_buf->size() ? m_buf->at(pos) : 0;
        m_buf->overwrite(pos, {static_cast<std::uint8_t>((old & 0xF0) | digit)}, /*merge=*/true);
        m_nibble = 0;
        after_edit(pos + 1);
    }
}

void HexView::type_ascii(char c) {
    if (m_selecting) collapse_selection_to_start();
    const std::size_t pos = m_cursor;
    const auto byte = static_cast<std::uint8_t>(c);
    if (pos >= m_buf->size() || m_insert_mode) {
        m_buf->insert(pos, {byte});
    } else {
        m_buf->overwrite(pos, {byte});
    }
    m_nibble = 0;
    after_edit(pos + 1);
}

void HexView::delete_forward() {
    if (!require_edit()) return;
    if (m_cursor >= m_buf->size()) return;
    const std::size_t pos = m_cursor;
    m_buf->erase(pos, 1);
    m_nibble = 0;
    after_edit(pos);
}

void HexView::delete_backward() {
    if (!require_edit()) return;
    if (m_cursor == 0 || m_buf->empty()) return;
    const std::size_t pos = std::min(m_cursor, m_buf->size()) - 1;
    m_buf->erase(pos, 1);
    m_nibble = 0;
    after_edit(pos);
}

// -------------------------------------------------------- Selection actions

bool HexView::delete_selection() {
    if (!require_edit()) return false;
    std::size_t s, e;
    if (!selection_range(s, e)) {
        notice("Nothing selected");
        return false;
    }
    m_buf->erase(s, e - s);
    m_nibble = 0;
    after_edit(s);
    notice("Deleted " + std::to_string(e - s) + " byte(s)");
    return true;
}

bool HexView::copy_selection() {
    std::size_t s, e;
    if (!selection_range(s, e)) {
        notice("Nothing selected");
        return false;
    }
    if (e - s > kMaxCopyBytes) {
        notice("Selection is too large to copy (limit 64 MiB)");
        return false;
    }
    const auto& d = m_buf->bytes();
    std::string text;
    if (m_pane == Pane::Ascii) {
        text.reserve(e - s);
        for (std::size_t i = s; i < e; ++i) text.push_back(hexutil::printable(d[i]));
    } else {
        text = hexutil::to_hex(HexBuffer::Bytes(d.begin() + static_cast<std::ptrdiff_t>(s),
                                                d.begin() + static_cast<std::ptrdiff_t>(e)));
    }
    Gtk::Clipboard::get()->set_text(text);
    notice("Copied " + std::to_string(e - s) + " byte(s) as " +
           (m_pane == Pane::Ascii ? "text" : "hex"));
    return true;
}

bool HexView::cut_selection() {
    if (!require_edit()) return false;
    if (!copy_selection()) return false;
    std::size_t s, e;
    if (!selection_range(s, e)) return false;
    m_buf->erase(s, e - s);
    m_nibble = 0;
    after_edit(s);
    notice("Cut " + std::to_string(e - s) + " byte(s)");
    return true;
}

bool HexView::paste(bool insert) {
    if (!require_edit()) return false;
    const Glib::ustring text = Gtk::Clipboard::get()->wait_for_text();
    if (text.empty()) {
        notice("The clipboard has no text to paste");
        return false;
    }

    HexBuffer::Bytes bytes;
    if (m_pane == Pane::Ascii) {
        bytes = hexutil::from_text(text.raw());
    } else {
        std::string err;
        if (!hexutil::parse_hex(text.raw(), bytes, &err)) {
            notice("Paste failed: " + err + " (the hex pane pastes hex bytes; Tab switches to text)");
            return false;
        }
    }

    std::size_t s, e;
    std::size_t start, removed = 0;
    if (selection_range(s, e)) {
        start = s;
        removed = e - s;
    } else {
        start = std::min(m_cursor, m_buf->size());
    }

    std::size_t new_cursor;
    if (insert) {
        m_buf->replace(start, removed, bytes);
        new_cursor = start + bytes.size();
    } else {
        const std::size_t n = m_buf->overwrite(start, bytes);
        if (n == 0) {
            notice("Paste Overwrite: there is nothing to overwrite at the cursor");
            return false;
        }
        new_cursor = start + n;
    }
    m_nibble = 0;
    after_edit(new_cursor);
    notice(std::string(insert ? "Inserted " : "Overwrote ") + std::to_string(bytes.size()) + " byte(s)");
    return true;
}

bool HexView::fill_selection(std::uint8_t value) {
    if (!require_edit()) return false;
    std::size_t s, e;
    if (!selection_range(s, e)) {
        notice("Nothing selected");
        return false;
    }
    m_buf->fill(s, e - s, value);
    m_nibble = 0;
    m_area.queue_draw();
    m_signal_edited.emit();
    m_signal_cursor.emit();
    notice("Filled " + std::to_string(e - s) + " byte(s)");
    return true;
}
