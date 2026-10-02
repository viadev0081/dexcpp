#ifndef DEX_LIB_HPP
#define DEX_LIB_HPP

#include <iostream>
#include <cstdio>
#include <ncurses.h>
#include <fstream>
#include <string>
#include <vector>

#define T_RED     "\x1b[31m"
#define T_GREEN   "\x1b[32m"
#define T_BLUE    "\x1b[34m"
#define T_YELLOW  "\x1b[33m"
#define T_CYAN    "\x1b[36m"
#define T_RESET   "\x1b[0m"
#define T_MAGENTA "\x1b[35m"

#define MAX_LINES 2048
#define MAX_LINE_LENGTH 2048

namespace dexlib {
    void printLogo();
    void drawCenteredHelloWindow(const std::string& text);
    void saveFile(
        const std::string& fileName, 
        const std::vector<std::string>& lines
    );
    void clearScreen();
    void addFile(const char * fileName);
    void displayContentInFile(const char * fileName);
}

#endif
