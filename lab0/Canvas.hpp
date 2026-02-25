#pragma once

#include <vector>
#include <algorithm>
#include <iostream>

class Canvas {
public:
    Canvas(int width, int height)
        : width_(width), height_(height), data_(width * height, ' ') {}

    void clear(char fill = ' ') {
        std::fill(data_.begin(), data_.end(), fill);
    }

    void setPixel(int x, int y, char c = '*') {
        if (x >= 0 && x < width_ && y >= 0 && y < height_)
            data_[y * width_ + x] = c;
    }

    char getPixel(int x, int y) const {
        if (x >= 0 && x < width_ && y >= 0 && y < height_)
            return data_[y * width_ + x];
        return ' ';
    }

    int width() const { return width_; }
    int height() const { return height_; }

    void print() const {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x)
                std::cout << data_[y * width_ + x];
            std::cout << '\n';
        }
    }

private:
    int width_, height_;
    std::vector<char> data_;
};

