#include "menu.h"

namespace {

using Kind = MenuItem::Kind;

MenuItem item(const char* label, const char* action, const char* accel = "") {
    return {Kind::Normal, label, action, accel, ""};
}
MenuItem check(const char* label, const char* action, const char* accel = "") {
    return {Kind::Check, label, action, accel, ""};
}
MenuItem radio(const char* label, const char* action, const char* group) {
    return {Kind::Radio, label, action, "", group};
}
MenuItem sep() {
    return {Kind::Separator, "", "", "", ""};
}

}  // namespace

std::vector<Menu> buildHexEditorMenus() {
    return {
        {"_File", {
            item("_New", "file_new", "<Control>N"),
            item("_Open...", "file_open", "<Control>O"),
            item("Open Read-_Only...", "file_open_readonly"),
            item("_Save", "file_save", "<Control>S"),
            item("Save _As...", "file_save_as", "<Control><Shift>S"),
            item("_Export Selection...", "file_export"),
            sep(),
            item("_Quit", "file_quit", "<Control>Q"),
        }},

        {"_Edit", {
            item("_Undo", "edit_undo", "<Control>Z"),
            item("_Redo", "edit_redo", "<Control>Y|<Control><Shift>Z"),
            sep(),
            item("Cu_t Bytes", "edit_cut", "<Control>X"),
            item("_Copy Bytes", "edit_copy", "<Control>C"),
            item("_Paste Insert", "edit_paste_insert", "<Control>V"),
            item("Paste _Overwrite", "edit_paste_overwrite", "<Control><Shift>V"),
            sep(),
            item("_Fill Selection...", "edit_fill"),
            item("_Zero Selection", "edit_zero"),
            item("Select _All", "edit_select_all", "<Control>A"),
            sep(),
            check("Edit _Mode", "edit_mode", "<Control>E"),
            check("_Insert Mode (Ins)", "insert_mode"),
        }},

        {"_View", {
            radio("Hex + _ASCII", "view_hex_ascii", "layout"),
            radio("_Hex Only", "view_hex_only", "layout"),
            radio("ASC_II Only", "view_ascii_only", "layout"),
            sep(),
            check("_Dark Mode", "view_dark"),
        }},

        {"_Search", {
            item("_Find Bytes...", "find_bytes", "<Control>F"),
            item("Find _ASCII...", "find_ascii"),
            item("Find _Hex Pattern...", "find_hex"),
            sep(),
            item("Find _Next", "find_next", "F3"),
            item("Find _Previous", "find_prev", "<Shift>F3"),
            sep(),
            item("_Go To Offset...", "goto_offset", "<Control>G"),
        }},

        // Not implemented yet: these have no handler, so they appear greyed out.
        {"_Analysis", {
            item("Byte _Frequency", "analysis_frequency"),
            item("_Entropy", "analysis_entropy"),
            item("_CRC32", "analysis_crc32"),
            item("_SHA-256", "analysis_sha256"),
        }},

        {"_Help", {
            item("_Keyboard Shortcuts", "help_shortcuts", "F1"),
            item("_About", "help_about"),
        }},
    };
}
