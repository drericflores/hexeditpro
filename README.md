# **Pop!Hex Editor Pro (v1.1.2)**

### **<span style="color:red">The Application Works - Still needs Some Minor Details</span>**

---

## **1. Project Overview**

**Pop!Hex Editor Pro** is a high-performance, modular hex editor designed for the **Pop!_OS** environment, with forward compatibility for the **COSMIC** desktop. It utilizes a professional three-pane architecture—**Address**, **Hex**, and **ASCII**—to provide an "elastic" and intuitive data manipulation interface.

Unlike a standard text editor, this application is built as a binary analysis tool, treating files as raw byte streams rather than encoded strings.

### **1.1 Core Capabilities**

* **Elastic Multi-Pane View**: Features resizable dividers between data columns, allowing users to customize the screen real estate for hex or ASCII analysis.
* **Synchronized Selection**: Clicking or moving the cursor in the Hex pane automatically updates the selection in the ASCII pane and vice versa, maintaining a persistent link between raw data and its character representation.
* **Light/Dark Mode**: Native CSS-based theme switching to match Pop!_OS system aesthetics.
* **Professional Edit/View Toggle**: A global state toggle that switches the application between a safe "Read-Only" navigation mode and an interactive "Edit" mode.

### **1.2 Current Limitations**
* **Direct Hex Modification**: While the editor state can be toggled to "Edit," the underlying binary buffer modification via keystrokes is currently in the late-integration phase.
* **Advanced Analysis**: Menu items for CRC32, SHA-256, and Entropy are present in the professional menu structure but currently exist as logical placeholders.

---

## **2. Technical Architecture**

The application is strictly modularized to separate the **Data Engine (Model)** from the **GTK+ Interface (View/Controller)**.

### **2.1 Important Modules**

* **`HexBuffer` (The Engine)**: Manages raw binary memory using `std::vector<unsigned char>` and handles binary file I/O streams (`std::ifstream`/`std::ofstream`).
* **`HexViewWidget` (The Elastic UI)**: Implements the **Coordinate Transformation Logic**. It maps 2D text-buffer positions (lines and columns) back to 1D byte offsets using the formula: `(line * 16) + (column / 3)`.
* **`MainWindow` (The Controller)**: Orchestrates the `gtkmm` event loop, manages the global CSS theme provider, and bridges the custom `Menu` structure to actual GUI signals.

---

## **3. Technology Stack**

* **C++17**: Used for high-speed memory management and binary data manipulation.
* **gtkmm-3.0**: The official C++ wrapper for the GTK+ library, providing native Pop!_OS/GNOME window management and Wayland support.
* **CSS Engines**: Utilized for dynamic theme injection, allowing the application to switch colors without restarting the UI.
* **Wayland/COSMIC Ready**: Designed to work natively with modern Linux display protocols and upcoming COSMIC desktop features.

---

## **4. Installation & Build**

Ensure you have the development headers installed on your Pop!_OS system:

```bash
sudo apt update
sudo apt install libgtkmm-3.0-dev build-essential

```

Compile using the modular source tree:

```bash
g++ src/*.cpp -o hex_pro -I./include `pkg-config --cflags --libs gtkmm-3.0`

```

---

## **5. Support the Developer**

If this tool has helped you in your reverse engineering or binary analysis tasks, consider supporting the project.

**Buy Me a Beer! Send to Paypal: @floreseo**

---

## **6. License**

Copyright 2024-2026, **Dr. Eric O. Flores**.

Licensed under the **MIT License**.
