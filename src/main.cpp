#include <gtkmm.h>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "com.eric.hexpro");
    MainWindow win;
    return app->run(win);
}
