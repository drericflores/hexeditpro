#ifndef HEXBUFFER_HPP
#define HEXBUFFER_HPP

#include <vector>
#include <string>

class HexBuffer {
public:
    std::vector<unsigned char> data;
    std::string current_path;

    bool load(const std::string& path);
    bool save(const std::string& path);
    void clear();
};

#endif
