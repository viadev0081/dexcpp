#include "./dexlib.hpp"
#include <iostream>
#include <cstdio>
#include <ncurses.h>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace dexlib {
    void printLogo() {
        std::cout << T_MAGENTA << "██████████    ███████████    ██     ██    █████████  ███████   ███████ " << T_RESET << std::endl;
        std::cout << T_MAGENTA << "██       ██  ██         ██   ██     ██   ██          ██    ██  ██    ██" << T_RESET << std::endl;
        std::cout << T_MAGENTA << "██       ██  ██         ██   ███   ███   ██          ██    ██  ██    ██" << T_RESET << std::endl;
        std::cout << T_MAGENTA << "██       ██  ██         ██    ███████    ██          ███████   ███████ " << T_RESET << std::endl;
        std::cout << T_MAGENTA << "██       ██  █████████████   ███   ███   ██          ██        ██      " << T_RESET << std::endl;
        std::cout << T_MAGENTA << "██       ██  ██              ██     ██   ██          ██        ██      " << T_RESET << std::endl;
        std::cout << T_MAGENTA << "██████████    ████████████   ██     ██    █████████  ██        ██      " << T_RESET << std::endl;
        std::cout << "Welcome to the DexCPP Editor!" << std::endl;
    }

    void drawCenteredHelloWindow(const std::string& text) {
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        int winHeight = 5;
        int winWidth = text.length() + 4;
        int start_y = (max_y - winHeight) / 2;
        int start_x = (max_x - winWidth) / 2;

        WINDOW * win = newwin(winHeight, winWidth, start_y, start_x);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, text.c_str());
        wrefresh(win);
        napms(1500); 
        delwin(win);
    }

    void saveFile(
        const std::string& fileName, 
        const std::vector<std::string>& lines
    ) {
        std::ofstream file(fileName);
        if (file.is_open()) {
            for (const auto& line : lines) {
                file << line << std::endl;
            }
            file.close();
        }
    }

    void clearScreen() {
        #ifdef _WIN32
            HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
            COORD coord = {0, 0};
            DWORD count;
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(hStdOut, &csbi);
            FillConsoleOutputCharacter(hStdOut, ' ', csbi.dwSize.X * csbi.dwSize.Y, coord, &count);
            SetConsoleCursorPosition(hStdOut, coord);
        #else
            std::cout << "\033[2J\033[1;1H";
        #endif
    }

    void addFile(const char * fileName) {
        int is_created = 0;
        FILE * file = fopen(fileName, "a");

        if (file == NULL) {
            std::cout << T_RED << "[FAILED]: Failed to create file!!" << T_RESET << std::endl;
            is_created = 0;
            return;
        }
        else {
            fclose(file);
            is_created = 1;
        }
    }

    void displayContentInFile(const char * fileName) {
        int is_displaying = 0;
        char line[2048];
        FILE * file = fopen(fileName, "r");

        if (file == NULL) {
            std::cout << T_RED << "[FAILED]: Failed to open file for read!!" << T_RESET << std::endl;
            is_displaying = 0;
            return;
        }
        else {
            is_displaying = 1;
            while (fgets(line, sizeof(line), file) != NULL) {
                fputs(line, stdout);
            }

            fclose(file);
            is_displaying = 0;
        }
    }
}
