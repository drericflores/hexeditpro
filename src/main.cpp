#include <gtkmm.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "MainWindow.hpp"
#include "Version.hpp"

int main(int argc, char* argv[]) {
    std::string initial_file;
    bool read_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("efhex %s\n", EFHEX_VERSION);
            return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::printf(
                "Usage: %s [--read-only] [FILE]\n\n"
                "Opens FILE in efhex (or starts empty).\n"
                "  -r, --read-only   open the file with editing turned off\n"
                "      --version     print the version and exit\n",
                argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "--read-only") == 0 || std::strcmp(argv[i], "-r") == 0) {
            read_only = true;
            continue;
        }
        if (initial_file.empty()) initial_file = argv[i];
    }

    // GTK only needs the program name; the file argument is handled below.
    int app_argc = 1;
    auto app = Gtk::Application::create(app_argc, argv, "com.eric.efhex");

    // Window and taskbar icon: the installed "efhex" theme icon, or the file
    // next to the source tree when running uninstalled from the build folder.
    Gtk::Window::set_default_icon_name("efhex");
    for (const char* p : {"resources/efhex.png", "../resources/efhex.png"}) {
        if (g_file_test(p, G_FILE_TEST_EXISTS)) {
            try { Gtk::Window::set_default_icon_from_file(p); } catch (...) {}
            break;
        }
    }

    MainWindow win;
    if (!initial_file.empty()) {
        // Open once the main loop is running so any error dialog can show.
        Glib::signal_idle().connect_once(
            [&win, initial_file, read_only] { win.open_file(initial_file, read_only); });
    }
    return app->run(win);
}
