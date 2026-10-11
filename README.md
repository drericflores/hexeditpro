# **efhex (v1.2.0)**

*efhex = Easy and Flexible Hex editor*

### **<span style="color:red">The Application Works - Still needs Some Minor Details</span>**

---

## **1. Project Overview**

**efhex** (Easy and Flexible Hex editor) is a high-performance, modular hex editor designed for the **Pop!_OS** environment, with forward compatibility for the **COSMIC** desktop. It utilizes a professional three-pane architecture—**Address**, **Hex**, and **ASCII**—to provide an "elastic" and intuitive data manipulation interface.

Unlike a standard text editor, this application is built as a binary analysis tool, treating files as raw byte streams rather than encoded strings.

### **1.1 Core Capabilities**

* **Virtual Hex View**: One custom-drawn view paints only the rows on screen, so file size does not slow it down. A 400 MiB test file opened in about half a second, and scrolling did not increase memory use.
* **Direct Editing**: Type hex digits in the hex column or text in the text column. Overwrite and insert modes (**Ins**), Delete/Backspace, typing past the end appends, and an empty buffer can be built up from scratch.
* **Undo / Redo**: Every edit is recorded. The two nibbles of one typed byte undo as a single step.
* **Safe Saving**: Saves go to a temporary file that replaces the original only after a successful flush, so a failed save never destroys your data. Permissions are kept and symlinks are followed. New, Open, Quit and closing the window ask before discarding unsaved changes.
* **Selection & Clipboard**: Select with Shift+movement, mouse drag or double-click. Copy, cut, paste (insert or overwrite), fill, zero and export the selection to a file.
* **Search**: Find hex bytes or ASCII text with Find Next / Previous and optional wrap-around, and jump with Go To Offset (decimal, or hex with a `0x` prefix).
* **Views**: Hex + ASCII, hex only or ASCII only; light/dark mode; and a read-only mode (**Ctrl+E**, or start with `--read-only`).
* **Status Bar**: Shows the offset, the byte value, selection size, file size, insert/overwrite mode and the active column.

### **1.2 Current Limitations**

* **Advanced Analysis**: Byte Frequency, Entropy, CRC32 and SHA-256 appear in the Analysis menu but are greyed out until they are implemented.
* **Memory**: The whole file is loaded into memory, so RAM use is roughly the file size. Memory-mapped I/O is not implemented yet.
* **Dark Mode**: Switches the GTK theme to its dark variant, so the window chrome only follows if the active theme provides one (Adwaita does). The hex view itself always follows.
* **Copying Text**: Copying from the text column copies what is displayed (non-printable bytes appear as `.`). Copy from the hex column for exact bytes.
* **Layout**: Fixed at 16 bytes per row. There is no Replace, bookmarks or drag-and-drop yet.

### **1.3 Keyboard Shortcuts**

| Action | Keys |
| --- | --- |
| Move / select | Arrow keys, PgUp/PgDn, Home/End, Ctrl+Home/End; add **Shift** to select |
| Switch hex / text column | Tab |
| Insert / overwrite mode | Ins |
| Edit mode on / off | Ctrl+E |
| Delete byte or selection | Del, Backspace |
| Undo / Redo | Ctrl+Z, Ctrl+Y (or Ctrl+Shift+Z) |
| Cut / Copy / Paste insert / Paste overwrite | Ctrl+X, Ctrl+C, Ctrl+V, Ctrl+Shift+V |
| Select all | Ctrl+A |
| Find / Next / Previous | Ctrl+F, F3, Shift+F3 |
| Go To Offset | Ctrl+G |
| New / Open / Save / Save As / Quit | Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S, Ctrl+Q |

---

## **2. Technical Architecture**

The application is strictly modularized to separate the **Data Engine (Model)** from the **GTK+ Interface (View/Controller)**. The engine has no GTK dependency, which is what makes it unit-testable.

### **2.1 Important Modules**

* **`HexBuffer` (The Engine)**: Holds the bytes and applies every edit through one `replace()` operation that is recorded for undo/redo. Also provides dirty tracking, atomic saving and search.
* **`HexUtil`**: Parsing and formatting helpers (hex strings, offsets) shared by the view and the window.
* **`HexView` (The Custom Widget)**: A `Gtk::DrawingArea` that draws the address, hex and text columns with Cairo/Pango and handles the mouse, keyboard and clipboard. It reads straight from the `HexBuffer`, so the display cannot drift out of sync with the data.
* **`MainWindow` (The Controller)**: Builds the menus from the data in `menu.cpp` (items are identified by stable action ids rather than label text), handles files, dialogs, search and the status bar.

### **2.2 Tests**

`tests/test_core.cpp` covers the engine and helpers with no GTK needed: edits, undo/redo, dirty tracking, atomic save, symlinks, load errors, search and the hex/offset parsers. Run it with `ctest` (CMake) or `make check`.

---

## **3. Technology Stack**

* **C++17**: Used for high-speed memory management and binary data manipulation.
* **gtkmm-3.0**: The official C++ wrapper for the GTK+ library, with Cairo and Pango for drawing the hex view.
* **CMake** (or the included Makefile) for building.
* **Wayland/X11**: GTK3 applications run on both. COSMIC has not been tested yet.

---

## **4. Installation & Build**

Ensure you have the development headers installed on your Pop!_OS system:

```bash
sudo apt update
sudo apt install libgtkmm-3.0-dev build-essential cmake
```

Build and run with CMake:

```bash
cmake -S . -B build
cmake --build build
./build/efhex [--read-only] [FILE]
cd build && ctest        # run the unit tests
```

Or with the Makefile:

```bash
make            # builds ./efhex
make check      # builds and runs the unit tests
```

---

## **5. Support the Developer**

If this tool has helped you in your reverse engineering or binary analysis tasks, consider supporting the project.

**Buy Me a Beer! Send to Paypal: @floreseo**

---

## **6. License**

Copyright 2024-2026, **Dr. Eric O. Flores**.

Licensed under the **MIT License**.
