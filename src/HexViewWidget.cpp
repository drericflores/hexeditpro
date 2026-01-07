#include "HexViewWidget.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

HexViewWidget::HexViewWidget() : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {
    m_paned_outer.add1(m_scroll_addr);
    m_paned_outer.add2(m_paned_inner);

    m_paned_inner.add1(m_scroll_hex);
    m_paned_inner.add2(m_scroll_ascii);

    m_scroll_addr.add(m_addr_view);
    m_scroll_hex.add(m_hex_view);
    m_scroll_ascii.add(m_ascii_view);

    m_scroll_addr.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_scroll_hex.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_scroll_ascii.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    setup_view(m_addr_view, false);
    setup_view(m_hex_view, true);
    setup_view(m_ascii_view, true);

    m_paned_outer.set_position(140);
    m_paned_inner.set_position(700);

    pack_start(m_paned_outer, Gtk::PACK_EXPAND_WIDGET);

    setup_sync();
    show_all_children();
}

void HexViewWidget::setup_view(Gtk::TextView& tv, bool editable) {
    tv.set_wrap_mode(Gtk::WRAP_NONE);
    tv.set_monospace(true);
    tv.set_left_margin(6);
    tv.set_right_margin(6);
    tv.set_editable(editable);
    tv.set_cursor_visible(editable);
}

void HexViewWidget::setup_sync() {
    m_hex_view.get_buffer()->signal_mark_set().connect(
        sigc::mem_fun(*this, &HexViewWidget::on_mark_set_hex));
    m_ascii_view.get_buffer()->signal_mark_set().connect(
        sigc::mem_fun(*this, &HexViewWidget::on_mark_set_ascii));
}

void HexViewWidget::on_mark_set_hex(const Gtk::TextBuffer::iterator&,
                                    const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if (!mark || mark->get_name() != "insert") return;
    sync_cursors_from(&m_hex_view);
}

void HexViewWidget::on_mark_set_ascii(const Gtk::TextBuffer::iterator&,
                                      const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if (!mark || mark->get_name() != "insert") return;
    sync_cursors_from(&m_ascii_view);
}

void HexViewWidget::sync_cursors_from(Gtk::TextView* source) {
    if (m_syncing) return;
    m_syncing = true;

    auto src_buf = source->get_buffer();
    auto it = src_buf->get_iter_at_mark(src_buf->get_insert());
    int line = std::max(0, it.get_line());
    int col  = std::max(0, it.get_line_offset());

    std::size_t byte_in_line = 0;
    if (source == &m_hex_view) {
        byte_in_line = std::min<std::size_t>(kBytesPerLine - 1, static_cast<std::size_t>(col) / 3);
    } else {
        byte_in_line = std::min<std::size_t>(kBytesPerLine - 1, static_cast<std::size_t>(col));
    }

    if (source == &m_hex_view) {
        auto abuf = m_ascii_view.get_buffer();
        auto ait = abuf->get_iter_at_line_offset(line, static_cast<int>(byte_in_line));
        abuf->place_cursor(ait);
        m_ascii_view.scroll_to(ait);
    } else {
        auto hbuf = m_hex_view.get_buffer();
        auto hit = hbuf->get_iter_at_line_offset(line, static_cast<int>(byte_in_line * 3));
        hbuf->place_cursor(hit);
        m_hex_view.scroll_to(hit);
    }

    m_syncing = false;
}

Gtk::TextView* HexViewWidget::focused_editor() const {
    if (m_hex_view.has_focus()) return const_cast<Gtk::TextView*>(&m_hex_view);
    if (m_ascii_view.has_focus()) return const_cast<Gtk::TextView*>(&m_ascii_view);
    return const_cast<Gtk::TextView*>(&m_hex_view);
}

void HexViewWidget::set_edit_mode(bool enable) {
    m_hex_view.set_editable(enable);
    m_ascii_view.set_editable(enable);
    m_hex_view.set_cursor_visible(enable);
    m_ascii_view.set_cursor_visible(enable);
}

void HexViewWidget::update_display(const HexBuffer& buffer) {
    std::ostringstream addr_ss, hex_ss, ascii_ss;

    const auto& data = buffer.data;
    const std::size_t n = data.size();
    const std::size_t lines = (n + kBytesPerLine - 1) / kBytesPerLine;

    for (std::size_t line = 0; line < lines; ++line) {
        std::size_t base = line * kBytesPerLine;

        addr_ss << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << base << "\n";

        for (std::size_t i = 0; i < kBytesPerLine; ++i) {
            std::size_t idx = base + i;
            if (idx < n) {
                hex_ss << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
                       << static_cast<unsigned>(data[idx]);
            } else {
                hex_ss << "  ";
            }
            if (i != kBytesPerLine - 1) hex_ss << ' ';
        }
        hex_ss << "\n";

        for (std::size_t i = 0; i < kBytesPerLine; ++i) {
            std::size_t idx = base + i;
            if (idx < n) {
                unsigned char c = data[idx];
                ascii_ss << (std::isprint(c) ? static_cast<char>(c) : '.');
            } else {
                ascii_ss << ' ';
            }
        }
        ascii_ss << "\n";
    }

    m_addr_view.get_buffer()->set_text(addr_ss.str());
    m_hex_view.get_buffer()->set_text(hex_ss.str());
    m_ascii_view.get_buffer()->set_text(ascii_ss.str());

    scroll_to_byte(0);
}

void HexViewWidget::clear_display() {
    m_addr_view.get_buffer()->set_text("");
    m_hex_view.get_buffer()->set_text("");
    m_ascii_view.get_buffer()->set_text("");
}

void HexViewWidget::scroll_to_byte(std::size_t byte_index) {
    std::size_t line = byte_index / kBytesPerLine;
    std::size_t in_line = byte_index % kBytesPerLine;

    auto hbuf = m_hex_view.get_buffer();
    int max_line = std::max(0, hbuf->get_line_count() - 1);
    if (static_cast<int>(line) > max_line) line = static_cast<std::size_t>(max_line);

    auto hit = hbuf->get_iter_at_line_offset(static_cast<int>(line), static_cast<int>(in_line * 3));
    hbuf->place_cursor(hit);
    m_hex_view.scroll_to(hit);

    auto abuf = m_ascii_view.get_buffer();
    auto ait = abuf->get_iter_at_line_offset(static_cast<int>(line), static_cast<int>(in_line));
    abuf->place_cursor(ait);
    m_ascii_view.scroll_to(ait);
}

std::size_t HexViewWidget::iter_to_byte_offset(const Gtk::TextView& tv,
                                               const Gtk::TextBuffer::iterator& it) const {
    int line = std::max(0, it.get_line());
    int col  = std::max(0, it.get_line_offset());

    std::size_t byte_in_line = 0;
    if (&tv == &m_hex_view) {
        byte_in_line = std::min<std::size_t>(kBytesPerLine - 1, static_cast<std::size_t>(col) / 3);
    } else {
        byte_in_line = std::min<std::size_t>(kBytesPerLine - 1, static_cast<std::size_t>(col));
    }

    return static_cast<std::size_t>(line) * kBytesPerLine + byte_in_line;
}

bool HexViewWidget::get_selected_byte_range(std::size_t& start, std::size_t& end) const {
    Gtk::TextView* tv = focused_editor();
    auto buf = tv->get_buffer();

    Gtk::TextBuffer::iterator s, e;
    if (!buf->get_selection_bounds(s, e)) return false;

    std::size_t a = iter_to_byte_offset(*tv, s);
    std::size_t b = iter_to_byte_offset(*tv, e);

    if (a == b) return false;
    if (a > b) std::swap(a, b);

    start = a;
    end = b;
    return true;
}

bool HexViewWidget::parse_hex_text(const std::string& text, std::vector<unsigned char>& out) {
    out.clear();
    std::string cleaned;
    cleaned.reserve(text.size());
    for (unsigned char c : text) {
        if (std::isxdigit(c)) cleaned.push_back(static_cast<char>(c));
    }
    if (cleaned.empty() || (cleaned.size() % 2) != 0) return false;

    for (std::size_t i = 0; i < cleaned.size(); i += 2) {
        unsigned int v = 0;
        std::istringstream iss(cleaned.substr(i, 2));
        iss >> std::hex >> v;
        out.push_back(static_cast<unsigned char>(v & 0xFFu));
    }
    return !out.empty();
}

std::string HexViewWidget::bytes_to_hex_string(const std::vector<unsigned char>& bytes) {
    std::ostringstream ss;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        ss << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
           << static_cast<unsigned>(bytes[i]);
        if (i + 1 != bytes.size()) ss << ' ';
    }
    return ss.str();
}

bool HexViewWidget::copy_bytes_to_clipboard(const HexBuffer& buffer) {
    std::size_t start = 0, end = 0;
    if (!get_selected_byte_range(start, end)) return false;
    if (start >= buffer.data.size()) return false;
    end = std::min(end, buffer.data.size());

    Gtk::TextView* tv = focused_editor();
    auto clip = Gtk::Clipboard::get();

    if (tv == &m_ascii_view) {
        std::string s;
        s.reserve(end - start);
        for (std::size_t i = start; i < end; ++i) s.push_back(static_cast<char>(buffer.data[i]));
        clip->set_text(s);
    } else {
        std::vector<unsigned char> bytes(buffer.data.begin() + static_cast<long>(start),
                                         buffer.data.begin() + static_cast<long>(end));
        clip->set_text(bytes_to_hex_string(bytes));
    }
    return true;
}

bool HexViewWidget::cut_bytes(HexBuffer& buffer) {
    std::size_t start = 0, end = 0;
    if (!get_selected_byte_range(start, end)) return false;
    if (start >= buffer.data.size()) return false;
    end = std::min(end, buffer.data.size());

    copy_bytes_to_clipboard(buffer);

    buffer.data.erase(buffer.data.begin() + static_cast<long>(start),
                      buffer.data.begin() + static_cast<long>(end));
    return true;
}

bool HexViewWidget::paste_insert(HexBuffer& buffer) {
    auto clip = Gtk::Clipboard::get();
    auto text = clip->wait_for_text();
    if (text.empty()) return false;

    Gtk::TextView* tv = focused_editor();
    std::vector<unsigned char> bytes;

    if (tv == &m_ascii_view) {
        bytes.assign(text.begin(), text.end());
    } else {
        if (!parse_hex_text(text, bytes)) return false;
    }

    std::size_t start = 0, end = 0;
    if (!get_selected_byte_range(start, end)) {
        auto buf = tv->get_buffer();
        auto it = buf->get_iter_at_mark(buf->get_insert());
        start = iter_to_byte_offset(*tv, it);
        end = start;
    } else {
        if (start > end) std::swap(start, end);
    }

    start = std::min(start, buffer.data.size());
    end   = std::min(end, buffer.data.size());

    if (end > start) {
        buffer.data.erase(buffer.data.begin() + static_cast<long>(start),
                          buffer.data.begin() + static_cast<long>(end));
    }
    buffer.data.insert(buffer.data.begin() + static_cast<long>(start), bytes.begin(), bytes.end());
    return true;
}

bool HexViewWidget::paste_overwrite(HexBuffer& buffer) {
    auto clip = Gtk::Clipboard::get();
    auto text = clip->wait_for_text();
    if (text.empty()) return false;

    Gtk::TextView* tv = focused_editor();
    std::vector<unsigned char> bytes;

    if (tv == &m_ascii_view) {
        bytes.assign(text.begin(), text.end());
    } else {
        if (!parse_hex_text(text, bytes)) return false;
    }

    std::size_t start = 0, end = 0;
    if (!get_selected_byte_range(start, end)) {
        auto buf = tv->get_buffer();
        auto it = buf->get_iter_at_mark(buf->get_insert());
        start = iter_to_byte_offset(*tv, it);
    } else {
        if (start > end) std::swap(start, end);
    }

    start = std::min(start, buffer.data.size());
    if (start >= buffer.data.size()) return false;

    std::size_t max_write = std::min<std::size_t>(bytes.size(), buffer.data.size() - start);
    for (std::size_t i = 0; i < max_write; ++i) buffer.data[start + i] = bytes[i];
    return true;
}

bool HexViewWidget::fill_selection(HexBuffer& buffer, unsigned char value) {
    std::size_t start = 0, end = 0;
    if (!get_selected_byte_range(start, end)) return false;
    if (start >= buffer.data.size()) return false;
    end = std::min(end, buffer.data.size());

    for (std::size_t i = start; i < end; ++i) buffer.data[i] = value;
    return true;
}

bool HexViewWidget::select_all() {
    Gtk::TextView* tv = focused_editor();
    auto buf = tv->get_buffer();
    buf->select_range(buf->begin(), buf->end());
    return true;
}
