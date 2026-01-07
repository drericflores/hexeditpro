#include "menu.h"
#include <iostream>

// Helper for features not yet linked to UI logic
static void notImplemented(const std::string& name) {
    std::cout << "[TODO] " << name << " is not yet implemented.\n";
}

std::vector<Menu> buildHexEditorMenus() {
    return {
        { "File", {
            { "New", []{ /* Handled by MainWindow override */ } },
            { "Open...", []{ /* Handled by MainWindow override */ } },
            { "Open Read-Only", []{ notImplemented("Open Read-Only"); } },
            { "Save", []{ /* Handled by MainWindow override */ } },
            { "Save As...", []{ /* Handled by MainWindow override */ } },
            { "Export Selection...", []{ notImplemented("Export Selection"); } },
            { "", nullptr, true },
            { "Close", []{ notImplemented("Close"); } },
            { "Quit", []{ /* Handled by MainWindow override */ } },
        }},

        { "Edit", {
            { "Undo", []{ notImplemented("Undo"); } },
            { "Redo", []{ notImplemented("Redo"); } },
            { "", nullptr, true },
            { "Cut Bytes", []{ notImplemented("Cut Bytes"); } },
            { "Copy Bytes", []{ notImplemented("Copy Bytes"); } },
            { "Paste Insert", []{ notImplemented("Paste Insert"); } },
            { "Paste Overwrite", []{ notImplemented("Paste Overwrite"); } },
            { "", nullptr, true },
            { "Fill Selection...", []{ notImplemented("Fill Selection"); } },
            { "Zero Selection", []{ notImplemented("Zero Selection"); } },
        }},

        { "View", {
            { "Hex + ASCII", []{ notImplemented("Hex + ASCII"); } },
            { "Hex Only", []{ notImplemented("Hex Only"); } },
            { "ASCII Only", []{ notImplemented("ASCII Only"); } },
            { "", nullptr, true },
            { "Little Endian", []{ notImplemented("Little Endian"); } },
            { "Big Endian", []{ notImplemented("Big Endian"); } },
        }},

        { "Search", {
            { "Find Bytes...", []{ notImplemented("Find Bytes"); } },
            { "Find ASCII...", []{ notImplemented("Find ASCII"); } },
            { "Find Hex Pattern...", []{ notImplemented("Find Hex Pattern"); } },
            { "Go To Offset...", []{ notImplemented("Go To Offset"); } },
        }},

        { "Analysis", {
            { "Byte Frequency", []{ notImplemented("Byte Frequency"); } },
            { "Entropy", []{ notImplemented("Entropy"); } },
            { "CRC32", []{ notImplemented("CRC32"); } },
            { "SHA-256", []{ notImplemented("SHA-256"); } },
        }},

        { "Help", {
            { "Keyboard Shortcuts", []{ notImplemented("Shortcuts"); } },
            { "About", []{ /* Handled by MainWindow override */ } },
        }},
    };
}
