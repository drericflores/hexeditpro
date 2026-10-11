#include "HexBuffer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

void set_error(std::string* error, const std::string& message) {
    if (error) *error = message;
}

std::string errno_message(const std::string& what) {
    return what + ": " + std::strerror(errno);
}

// Writes the whole range, retrying on short writes and EINTR.
bool write_all(int fd, const std::uint8_t* p, std::size_t n) {
    while (n > 0) {
        ssize_t w = ::write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += w;
        n -= static_cast<std::size_t>(w);
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------- File I/O

bool HexBuffer::load(const std::string& path, std::string* error) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
        set_error(error, errno_message("Cannot open file"));
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        set_error(error, "Not a regular file");
        return false;
    }

    const auto size = static_cast<std::size_t>(st.st_size);
    Bytes data(size);
    if (size > 0) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            set_error(error, errno_message("Cannot open file"));
            return false;
        }
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (static_cast<std::size_t>(file.gcount()) != size) {
            set_error(error, "Could not read the whole file");
            return false;
        }
    }

    m_data = std::move(data);
    m_path = path;
    m_undo.clear();
    m_redo.clear();
    m_clean_index = 0;
    return true;
}

bool HexBuffer::save(const std::string& path, std::string* error) {
    // Follow symlinks so we replace the real file, not the link itself.
    std::error_code ec;
    fs::path target = fs::weakly_canonical(fs::path(path), ec);
    if (ec) target = fs::path(path);

    fs::path dir = target.parent_path();
    if (dir.empty()) dir = ".";

    std::string tmpl = (dir / ".efhex-XXXXXX").string();
    std::vector<char> tmp_name(tmpl.begin(), tmpl.end());
    tmp_name.push_back('\0');

    int fd = ::mkstemp(tmp_name.data());
    if (fd < 0) {
        set_error(error, errno_message("Cannot create temporary file in " + dir.string()));
        return false;
    }

    auto fail = [&](const std::string& what) {
        const int saved = errno;
        ::close(fd);
        ::unlink(tmp_name.data());
        errno = saved;
        set_error(error, errno_message(what));
        return false;
    };

    // mkstemp creates the file 0600. Keep the original's permissions, or use
    // the normal default for a new file.
    struct stat st{};
    mode_t mode;
    if (::stat(target.c_str(), &st) == 0) {
        mode = st.st_mode & 07777;
    } else {
        const mode_t mask = ::umask(0);
        ::umask(mask);
        mode = 0666 & ~mask;
    }
    if (::fchmod(fd, mode) != 0) return fail("Cannot set file permissions");

    if (!write_all(fd, m_data.data(), m_data.size())) return fail("Write failed");
    if (::fsync(fd) != 0) return fail("Flush to disk failed");
    if (::close(fd) != 0) {
        const int saved = errno;
        ::unlink(tmp_name.data());
        errno = saved;
        set_error(error, errno_message("Close failed"));
        return false;
    }

    if (::rename(tmp_name.data(), target.c_str()) != 0) {
        const int saved = errno;
        ::unlink(tmp_name.data());
        errno = saved;
        set_error(error, errno_message("Cannot replace " + target.string()));
        return false;
    }

    m_path = path;
    m_clean_index = m_undo.size();
    return true;
}

void HexBuffer::clear() {
    m_data.clear();
    m_path.clear();
    m_undo.clear();
    m_redo.clear();
    m_clean_index = 0;
}

// ----------------------------------------------------------------- Editing

void HexBuffer::apply(std::size_t pos, std::size_t remove_count, const Bytes& with) {
    if (remove_count == with.size()) {
        std::copy(with.begin(), with.end(), m_data.begin() + static_cast<std::ptrdiff_t>(pos));
        return;
    }
    auto first = m_data.begin() + static_cast<std::ptrdiff_t>(pos);
    m_data.erase(first, first + static_cast<std::ptrdiff_t>(remove_count));
    m_data.insert(m_data.begin() + static_cast<std::ptrdiff_t>(pos), with.begin(), with.end());
}

void HexBuffer::replace(std::size_t pos, std::size_t count, Bytes with, bool merge) {
    pos = std::min(pos, m_data.size());
    count = std::min(count, m_data.size() - pos);

    Edit e;
    e.pos = pos;
    e.removed.assign(m_data.begin() + static_cast<std::ptrdiff_t>(pos),
                     m_data.begin() + static_cast<std::ptrdiff_t>(pos + count));
    if (e.removed == with) return;  // nothing would change

    apply(pos, count, with);
    e.inserted = std::move(with);

    // If the clean state only exists in the redo history, it is gone for good.
    if (m_clean_index != npos && m_clean_index > m_undo.size()) m_clean_index = npos;
    m_redo.clear();

    e.group = (merge && !m_undo.empty()) ? m_undo.back().group : ++m_next_group;
    m_undo.push_back(std::move(e));
}

void HexBuffer::insert(std::size_t pos, Bytes bytes, bool merge) {
    replace(pos, 0, std::move(bytes), merge);
}

void HexBuffer::erase(std::size_t pos, std::size_t count, bool merge) {
    replace(pos, count, {}, merge);
}

std::size_t HexBuffer::overwrite(std::size_t pos, const Bytes& bytes, bool merge) {
    if (pos >= m_data.size()) return 0;
    const std::size_t n = std::min(bytes.size(), m_data.size() - pos);
    if (n == 0) return 0;
    replace(pos, n, Bytes(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n)), merge);
    return n;
}

void HexBuffer::fill(std::size_t pos, std::size_t count, std::uint8_t value) {
    if (pos >= m_data.size()) return;
    count = std::min(count, m_data.size() - pos);
    if (count == 0) return;
    replace(pos, count, Bytes(count, value));
}

// ------------------------------------------------------------ Undo / redo

std::optional<std::size_t> HexBuffer::undo() {
    if (m_undo.empty()) return std::nullopt;
    const std::size_t group = m_undo.back().group;
    std::size_t where = 0;
    while (!m_undo.empty() && m_undo.back().group == group) {
        Edit e = std::move(m_undo.back());
        m_undo.pop_back();
        apply(e.pos, e.inserted.size(), e.removed);
        where = e.pos;
        m_redo.push_back(std::move(e));
    }
    return where;
}

std::optional<std::size_t> HexBuffer::redo() {
    if (m_redo.empty()) return std::nullopt;
    const std::size_t group = m_redo.back().group;
    std::size_t where = 0;
    while (!m_redo.empty() && m_redo.back().group == group) {
        Edit e = std::move(m_redo.back());
        m_redo.pop_back();
        apply(e.pos, e.removed.size(), e.inserted);
        where = e.pos;
        m_undo.push_back(std::move(e));
    }
    return where;
}

// ------------------------------------------------------------------ Search

// First match whose start lies in [lo, hi] (inclusive), or npos.
std::size_t HexBuffer::find_forward(const Bytes& pat, std::size_t lo, std::size_t hi) const {
    if (lo > hi) return npos;
    auto first = m_data.begin() + static_cast<std::ptrdiff_t>(lo);
    auto last = m_data.begin() + static_cast<std::ptrdiff_t>(hi + pat.size());
    auto it = std::search(first, last, pat.begin(), pat.end());
    return it == last ? npos : static_cast<std::size_t>(it - m_data.begin());
}

// Last match whose start lies in [lo, hi] (inclusive), or npos.
std::size_t HexBuffer::find_backward(const Bytes& pat, std::size_t lo, std::size_t hi) const {
    if (lo > hi) return npos;
    for (std::size_t i = hi + 1; i-- > lo;) {
        if (m_data[i] == pat[0] && std::memcmp(&m_data[i], pat.data(), pat.size()) == 0) return i;
    }
    return npos;
}

std::size_t HexBuffer::find(const Bytes& pattern, std::size_t from, bool forward, bool wrap) const {
    if (pattern.empty() || pattern.size() > m_data.size()) return npos;
    const std::size_t last_start = m_data.size() - pattern.size();

    if (forward) {
        std::size_t r = (from <= last_start) ? find_forward(pattern, from, last_start) : npos;
        if (r == npos && wrap && from > 0) {
            r = find_forward(pattern, 0, std::min(from - 1, last_start));
        }
        return r;
    }

    const std::size_t hi = std::min(from, last_start);
    std::size_t r = find_backward(pattern, 0, hi);
    if (r == npos && wrap && hi < last_start) {
        r = find_backward(pattern, hi + 1, last_start);
    }
    return r;
}
