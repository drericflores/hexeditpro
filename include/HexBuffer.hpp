#ifndef HEXBUFFER_HPP
#define HEXBUFFER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The data engine. Holds the file contents, applies edits, and keeps the
// undo/redo history. It has no GUI dependencies so it can be unit tested.
//
// All edits go through replace(): remove `count` bytes at `pos` and put
// `with` in their place. insert/erase/overwrite/fill are thin wrappers.
// Every edit is recorded so it can be undone.
class HexBuffer {
public:
    using Bytes = std::vector<std::uint8_t>;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    // --- File I/O ---------------------------------------------------------
    // On failure the buffer is left unchanged and `error` (if given) says why.
    bool load(const std::string& path, std::string* error = nullptr);

    // Writes to a temporary file next to `path`, flushes it to disk and then
    // renames it over `path`, so a failed save never destroys the original.
    // Symlinks are followed (the link target is replaced, not the link).
    bool save(const std::string& path, std::string* error = nullptr);

    // Drops the contents, the path and the whole undo history.
    void clear();

    // --- Read access ------------------------------------------------------
    const Bytes& bytes() const { return m_data; }
    std::size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }
    std::uint8_t at(std::size_t i) const { return m_data[i]; }
    const std::string& path() const { return m_path; }

    // --- Editing ----------------------------------------------------------
    // `merge` joins the edit to the previous undo step (used for the two
    // nibbles of one typed byte so Undo removes the whole byte at once).
    // Positions and counts are clamped to the buffer; no-op edits are ignored.
    void replace(std::size_t pos, std::size_t count, Bytes with, bool merge = false);
    void insert(std::size_t pos, Bytes bytes, bool merge = false);
    void erase(std::size_t pos, std::size_t count, bool merge = false);

    // Overwrites in place without changing the size; bytes that would land
    // past the end are dropped. Returns how many bytes were written.
    std::size_t overwrite(std::size_t pos, const Bytes& bytes, bool merge = false);

    void fill(std::size_t pos, std::size_t count, std::uint8_t value);

    // --- Undo / redo ------------------------------------------------------
    bool can_undo() const { return !m_undo.empty(); }
    bool can_redo() const { return !m_redo.empty(); }
    // Each returns the offset of the change, or nullopt if nothing to do.
    std::optional<std::size_t> undo();
    std::optional<std::size_t> redo();

    // True when the contents differ from what was last loaded or saved.
    bool dirty() const { return m_undo.size() != m_clean_index; }

    // --- Search -----------------------------------------------------------
    // Forward: first match starting at or after `from`.
    // Backward: last match starting at or before `from`.
    // With `wrap` the search continues from the other end of the buffer.
    // Returns npos when there is no match (or the pattern is empty).
    std::size_t find(const Bytes& pattern, std::size_t from, bool forward, bool wrap) const;

private:
    struct Edit {
        std::size_t pos = 0;
        Bytes removed;
        Bytes inserted;
        std::size_t group = 0;
    };

    void apply(std::size_t pos, std::size_t remove_count, const Bytes& with);
    std::size_t find_forward(const Bytes& pat, std::size_t lo, std::size_t hi) const;
    std::size_t find_backward(const Bytes& pat, std::size_t lo, std::size_t hi) const;

    Bytes m_data;
    std::string m_path;

    std::vector<Edit> m_undo;
    std::vector<Edit> m_redo;
    std::size_t m_next_group = 0;
    // Undo depth at which the buffer matches the file on disk, or npos when
    // that state can no longer be reached.
    std::size_t m_clean_index = 0;
};

#endif
