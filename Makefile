# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++17 `pkg-config --cflags gtkmm-3.0` -I./include
LIBS = `pkg-config --libs gtkmm-3.0`

# Project Files
# Note: This list ensures we only link the intended files, avoiding "multiple definition" errors
OBJ = src/main.o src/HexBuffer.o src/HexViewWidget.o src/MainWindow.o
TARGET = hex_pro

# Build Rules
$(TARGET): $(OBJ)
	$(CXX) -o $(TARGET) $(OBJ) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Utility Rules
clean:
	rm -f src/*.o $(TARGET)