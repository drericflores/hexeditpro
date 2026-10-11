// Unit tests for the GUI-free core: HexBuffer and hexutil.
// No test framework needed; a failed CHECK makes the program exit non-zero.

#include "HexBuffer.hpp"
#include "HexUtil.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
using Bytes = HexBuffer::Bytes;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

static fs::path g_dir;

static void write_file(const fs::path& p, const Bytes& b) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

static Bytes read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static HexBuffer loaded(const Bytes& contents, const char* name = "input.bin") {
    const fs::path p = g_dir / name;
    write_file(p, contents);
    HexBuffer b;
    std::string err;
    CHECK(b.load(p.string(), &err));
    return b;
}

// ----------------------------------------------------------------- Editing

static void test_insert_erase_overwrite() {
    HexBuffer b = loaded({1, 2, 3, 4});

    b.insert(2, {9, 9});
    CHECK((b.bytes() == Bytes{1, 2, 9, 9, 3, 4}));

    b.erase(0, 2);
    CHECK((b.bytes() == Bytes{9, 9, 3, 4}));

    CHECK(b.overwrite(1, {7, 8}) == 2);
    CHECK((b.bytes() == Bytes{9, 7, 8, 4}));

    // Overwrite never grows the buffer: extra bytes are dropped.
    CHECK(b.overwrite(3, {5, 6, 7}) == 1);
    CHECK((b.bytes() == Bytes{9, 7, 8, 5}));
    CHECK(b.overwrite(4, {1}) == 0);
    CHECK(b.size() == 4);

    b.fill(1, 2, 0xFF);
    CHECK((b.bytes() == Bytes{9, 0xFF, 0xFF, 5}));

    // Clamping: insert past the end appends, erase past the end is a no-op.
    b.insert(1000, {0xAA});
    CHECK(b.at(b.size() - 1) == 0xAA);
    const std::size_t before = b.size();
    b.erase(1000, 5);
    CHECK(b.size() == before);
}

static void test_empty_buffer_is_editable() {
    HexBuffer b;
    b.insert(0, {1, 2, 3});
    CHECK((b.bytes() == Bytes{1, 2, 3}));
    CHECK(b.dirty());
}

static void test_noop_edits_are_ignored() {
    HexBuffer b = loaded({5, 5, 5});
    b.fill(0, 3, 5);              // same value
    b.overwrite(0, {5});          // same value
    b.erase(1, 0);                // nothing to remove
    CHECK(!b.can_undo());
    CHECK(!b.dirty());
}

// ------------------------------------------------------------- Undo / redo

static void test_undo_redo() {
    HexBuffer b = loaded({1, 2, 3});
    b.insert(3, {4});
    b.overwrite(0, {9});
    b.erase(1, 1);
    CHECK((b.bytes() == Bytes{9, 3, 4}));

    CHECK(b.undo().has_value());
    CHECK((b.bytes() == Bytes{9, 2, 3, 4}));
    CHECK(b.undo().has_value());
    CHECK((b.bytes() == Bytes{1, 2, 3, 4}));
    CHECK(b.undo().has_value());
    CHECK((b.bytes() == Bytes{1, 2, 3}));
    CHECK(!b.undo().has_value());
    CHECK(!b.can_undo());

    CHECK(b.redo().has_value());
    CHECK(b.redo().has_value());
    CHECK(b.redo().has_value());
    CHECK((b.bytes() == Bytes{9, 3, 4}));
    CHECK(!b.redo().has_value());
}

static void test_new_edit_clears_redo() {
    HexBuffer b = loaded({1, 2, 3});
    b.overwrite(0, {9});
    b.undo();
    CHECK(b.can_redo());
    b.overwrite(1, {8});
    CHECK(!b.can_redo());
}

static void test_merged_edits_undo_together() {
    HexBuffer b = loaded({0x00, 0x11});
    // Typing the byte 0xAB: first nibble, then second nibble merged in.
    b.overwrite(0, {0xA0});
    b.overwrite(0, {0xAB}, /*merge=*/true);
    CHECK(b.at(0) == 0xAB);
    b.undo();
    CHECK(b.at(0) == 0x00);
    CHECK(!b.can_undo());
    b.redo();
    CHECK(b.at(0) == 0xAB);
}

static void test_undo_restores_size_changes() {
    HexBuffer b = loaded({1, 2, 3, 4, 5});
    b.erase(1, 3);
    CHECK((b.bytes() == Bytes{1, 5}));
    b.undo();
    CHECK((b.bytes() == Bytes{1, 2, 3, 4, 5}));
    b.insert(2, {7, 7, 7});
    b.undo();
    CHECK((b.bytes() == Bytes{1, 2, 3, 4, 5}));
}

// ------------------------------------------------------------ Dirty state

static void test_dirty_tracking() {
    HexBuffer b = loaded({1, 2, 3});
    CHECK(!b.dirty());

    b.overwrite(0, {9});
    CHECK(b.dirty());
    b.undo();
    CHECK(!b.dirty());     // back to the loaded state
    b.redo();
    CHECK(b.dirty());

    std::string err;
    CHECK(b.save((g_dir / "dirty.bin").string(), &err));
    CHECK(!b.dirty());
    b.undo();
    CHECK(b.dirty());      // differs from what was saved
    b.redo();
    CHECK(!b.dirty());
}

static void test_dirty_never_falsely_clean() {
    // Save, undo past the save point, make a different edit: the saved state
    // is unreachable, so the buffer must stay dirty even at the same depth.
    HexBuffer b = loaded({1, 2, 3});
    b.overwrite(0, {9});
    std::string err;
    CHECK(b.save((g_dir / "falsely.bin").string(), &err));
    CHECK(!b.dirty());
    b.undo();
    b.overwrite(1, {8});
    CHECK(b.dirty());
    b.undo();
    CHECK(b.dirty());
}

// -------------------------------------------------------------------- I/O

static void test_save_roundtrip_and_no_temp_files() {
    const fs::path dir = g_dir / "roundtrip";
    fs::create_directory(dir);
    HexBuffer b;
    b.insert(0, {0, 1, 2, 0xFF, 0x80});
    std::string err;
    CHECK(b.save((dir / "out.bin").string(), &err));
    CHECK((read_file(dir / "out.bin") == Bytes{0, 1, 2, 0xFF, 0x80}));
    CHECK(b.path() == (dir / "out.bin").string());

    int entries = 0;
    for (const auto& e : fs::directory_iterator(dir)) { (void)e; ++entries; }
    CHECK(entries == 1);   // only out.bin, no leftover temp file
}

static void test_save_keeps_permissions() {
    const fs::path p = g_dir / "perm.bin";
    write_file(p, {1, 2, 3});
    ::chmod(p.c_str(), 0640);
    HexBuffer b;
    CHECK(b.load(p.string()));
    b.overwrite(0, {9});
    CHECK(b.save(p.string()));
    struct stat st{};
    CHECK(::stat(p.c_str(), &st) == 0);
    CHECK((st.st_mode & 07777) == 0640);
    CHECK((read_file(p) == Bytes{9, 2, 3}));
}

static void test_save_follows_symlink() {
    const fs::path real = g_dir / "real.bin";
    const fs::path link = g_dir / "link.bin";
    write_file(real, {1, 2, 3});
    fs::create_symlink(real, link);
    HexBuffer b;
    CHECK(b.load(link.string()));
    b.overwrite(0, {9});
    CHECK(b.save(link.string()));
    CHECK(fs::is_symlink(link));
    CHECK((read_file(real) == Bytes{9, 2, 3}));
}

static void test_failed_save_changes_nothing() {
    HexBuffer b = loaded({1, 2, 3}, "failsave.bin");
    const std::string original_path = b.path();
    b.overwrite(0, {9});
    std::string err;
    CHECK(!b.save((g_dir / "no-such-dir" / "x.bin").string(), &err));
    CHECK(!err.empty());
    CHECK(b.path() == original_path);
    CHECK(b.dirty());
    CHECK((read_file(original_path) == Bytes{1, 2, 3}));
}

static void test_load_errors_leave_buffer_untouched() {
    HexBuffer b = loaded({1, 2, 3}, "keep.bin");
    std::string err;
    CHECK(!b.load((g_dir / "missing.bin").string(), &err));
    CHECK(!err.empty());
    CHECK(!b.load(g_dir.string(), &err));    // a directory
    CHECK(err == "Not a regular file");
    CHECK((b.bytes() == Bytes{1, 2, 3}));
}

static void test_load_empty_file() {
    HexBuffer b = loaded({}, "empty.bin");
    CHECK(b.empty());
    CHECK(!b.dirty());
}

static void test_load_resets_history() {
    HexBuffer b = loaded({1, 2, 3}, "h1.bin");
    b.overwrite(0, {9});
    write_file(g_dir / "h2.bin", {7});
    CHECK(b.load((g_dir / "h2.bin").string()));
    CHECK(!b.can_undo());
    CHECK(!b.can_redo());
    CHECK(!b.dirty());
}

// ------------------------------------------------------------------ Search

static void test_find() {
    HexBuffer b = loaded({0, 1, 2, 1, 2, 3, 1, 2});
    const Bytes pat{1, 2};

    CHECK(b.find(pat, 0, true, false) == 1);
    CHECK(b.find(pat, 2, true, false) == 3);
    CHECK(b.find(pat, 4, true, false) == 6);
    CHECK(b.find(pat, 7, true, false) == HexBuffer::npos);
    CHECK(b.find(pat, 7, true, true) == 1);          // wraps

    CHECK(b.find(pat, 7, false, false) == 6);
    CHECK(b.find(pat, 5, false, false) == 3);
    CHECK(b.find(pat, 0, false, false) == HexBuffer::npos);
    CHECK(b.find(pat, 0, false, true) == 6);         // wraps

    CHECK(b.find({9}, 0, true, true) == HexBuffer::npos);
    CHECK(b.find({}, 0, true, true) == HexBuffer::npos);
    CHECK(b.find(Bytes(100, 1), 0, true, true) == HexBuffer::npos);
    CHECK(b.find(pat, 1000, true, false) == HexBuffer::npos);
    CHECK(b.find(pat, 1000, false, false) == 6);     // clamps to the end
    CHECK(b.find(pat, 1000, true, true) == 1);
}

static void test_find_match_at_very_end_and_start() {
    HexBuffer b = loaded({5, 6, 7});
    CHECK(b.find({7}, 0, true, false) == 2);
    CHECK(b.find({5}, 2, false, false) == 0);
    CHECK(b.find({5, 6, 7}, 0, true, false) == 0);
}

// ----------------------------------------------------------------- hexutil

static void test_parse_hex() {
    Bytes out;
    std::string err;
    CHECK(hexutil::parse_hex("DE ad Be eF", out, &err));
    CHECK((out == Bytes{0xDE, 0xAD, 0xBE, 0xEF}));
    CHECK(hexutil::parse_hex("DEADBEEF", out));
    CHECK((out == Bytes{0xDE, 0xAD, 0xBE, 0xEF}));
    CHECK(hexutil::parse_hex("0xDE, 0xAD,0x01", out));
    CHECK((out == Bytes{0xDE, 0xAD, 0x01}));
    CHECK(hexutil::parse_hex("  00\n\tFF  ", out));
    CHECK((out == Bytes{0x00, 0xFF}));

    // Rejected input (the old parser accepted these by skipping characters).
    CHECK(!hexutil::parse_hex("hello", out, &err));
    CHECK(!hexutil::parse_hex("DE AD G1", out, &err));
    CHECK(!hexutil::parse_hex("ABC", out, &err));
    CHECK(!hexutil::parse_hex("0x", out, &err));
    CHECK(!hexutil::parse_hex("", out, &err));
    CHECK(!hexutil::parse_hex("   ", out, &err));
    CHECK(out.empty());
}

static void test_to_hex() {
    CHECK(hexutil::to_hex({}) == "");
    CHECK(hexutil::to_hex({0x00, 0xAB, 0x0F}) == "00 AB 0F");
}

static void test_parse_offset() {
    std::size_t v = 0;
    CHECK(hexutil::parse_offset("0", v) && v == 0);
    CHECK(hexutil::parse_offset("  255 ", v) && v == 255);
    CHECK(hexutil::parse_offset("0xFF", v) && v == 255);
    CHECK(hexutil::parse_offset("0X1f", v) && v == 31);
    CHECK(hexutil::parse_offset("010", v) && v == 10);   // decimal, not octal
    CHECK(!hexutil::parse_offset("", v));
    CHECK(!hexutil::parse_offset("0x", v));
    CHECK(!hexutil::parse_offset("FF", v));              // needs the 0x prefix
    CHECK(!hexutil::parse_offset("12abc", v));
    CHECK(!hexutil::parse_offset("-1", v));
    CHECK(!hexutil::parse_offset("0xFFFFFFFFFFFFFFFFFF", v));   // overflow
}

static void test_printable() {
    CHECK(hexutil::printable('A') == 'A');
    CHECK(hexutil::printable(' ') == ' ');
    CHECK(hexutil::printable(0x00) == '.');
    CHECK(hexutil::printable(0x7F) == '.');
    CHECK(hexutil::printable(0xFF) == '.');
}

int main() {
    char tmpl[] = "/tmp/hexpro-test-XXXXXX";
    if (!::mkdtemp(tmpl)) {
        std::perror("mkdtemp");
        return 2;
    }
    g_dir = tmpl;

    test_insert_erase_overwrite();
    test_empty_buffer_is_editable();
    test_noop_edits_are_ignored();
    test_undo_redo();
    test_new_edit_clears_redo();
    test_merged_edits_undo_together();
    test_undo_restores_size_changes();
    test_dirty_tracking();
    test_dirty_never_falsely_clean();
    test_save_roundtrip_and_no_temp_files();
    test_save_keeps_permissions();
    test_save_follows_symlink();
    test_failed_save_changes_nothing();
    test_load_errors_leave_buffer_untouched();
    test_load_empty_file();
    test_load_resets_history();
    test_find();
    test_find_match_at_very_end_and_start();
    test_parse_hex();
    test_to_hex();
    test_parse_offset();
    test_printable();

    std::error_code ec;
    fs::remove_all(g_dir, ec);

    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
