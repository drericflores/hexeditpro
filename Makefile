# Plain Makefile build. CMake (see CMakeLists.txt) works too.
CXX      ?= g++
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Wextra -MMD -MP -I./include

GTK_CFLAGS := $(shell pkg-config --cflags gtkmm-3.0)
GTK_LIBS   := $(shell pkg-config --libs gtkmm-3.0)

# The data engine has no GTK dependency, so it is built (and tested) on its own.
CORE_OBJ = src/HexBuffer.o src/HexUtil.o
GUI_OBJ  = src/main.o src/MainWindow.o src/HexView.o src/menu.o
TARGET   = efhex
TEST     = test_core

all: $(TARGET)

$(TARGET): $(GUI_OBJ) $(CORE_OBJ)
	$(CXX) -o $@ $^ $(GTK_LIBS)

$(TEST): tests/test_core.o $(CORE_OBJ)
	$(CXX) -o $@ $^

check: $(TEST)
	./$(TEST)

$(GUI_OBJ): CXXFLAGS += $(GTK_CFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o src/*.d tests/*.o tests/*.d $(TARGET) $(TEST)

.PHONY: all check clean

-include $(wildcard src/*.d tests/*.d)
