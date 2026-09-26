#include "./dexlib.hpp"
#include <iostream>
#include <cstdio>

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
