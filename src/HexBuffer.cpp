#include "HexBuffer.hpp"
#include <fstream>
#include <iterator>

bool HexBuffer::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    data.assign(std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());

    current_path = path;
    return true;
}

bool HexBuffer::save(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));

    current_path = path;
    return true;
}

void HexBuffer::clear() {
    data.clear();
    current_path.clear();
}
