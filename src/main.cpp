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
            std::printf("HexEdit Pro %s\n", HEXPRO_VERSION);
            return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::printf(
                "Usage: %s [--read-only] [FILE]\n\n"
                "Opens FILE in the hex editor (or starts empty).\n"
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
    auto app = Gtk::Application::create(app_argc, argv, "com.eric.hexpro");

    MainWindow win;
    if (!initial_file.empty()) {
        // Open once the main loop is running so any error dialog can show.
        Glib::signal_idle().connect_once(
            [&win, initial_file, read_only] { win.open_file(initial_file, read_only); });
    }
    return app->run(win);
}
