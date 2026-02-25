#include "Canvas.hpp"
#include "Bresenham.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <string>

void clearScreen() { system("clear"); }

int main() {
    const int W = 60, H = 24;
    const int delayMs = 2000;
    Canvas canvas(W, H);

    int frame = 0;
    for (;;) {
        canvas.clear(' ');

        switch (frame % 3) {
            case 0: {
                bresenhamLine(canvas, 5, 5, 55, 20, '*');
                bresenhamLine(canvas, 55, 5, 5, 20, '*');
                std::cout << "--- Две линии (Брезенхем) ---\n";
                break;
            }
            case 1: {
                bresenhamCircle(canvas, W/2, H/2, std::min(W, H)/2 - 2, '*');
                std::cout << "--- Окружность (Брезенхем) ---\n";
                break;
            }
            case 2: {
                int cx = W/2, cy = H/2, size = std::min(W, H)/3;
                drawTriangle(canvas,
                    cx, cy - size,
                    cx - size, cy + size,
                    cx + size, cy + size,
                    '*');
                std::cout << "--- Треугольник (3 линии Брезенхема) ---\n";
                break;
            }
        }

        clearScreen();
        canvas.print();
        std::cout << "\nСледующий кадр через " << (delayMs/1000) << " сек... (Ctrl+C — выход)\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        ++frame;
    }
    return 0;
}
