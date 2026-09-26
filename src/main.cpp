#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "./lib/dexlib.hpp"

int main(int argc, char * argv[])
{
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int lineCount = 0;
    std::string fileName;
    FILE * file;

    dexlib::clearScreen();
    dexlib::printLogo();

    std::cout << "[Input the file name to edit]: " << std::endl;
    std::cin >> fileName;
    std::cin.ignore();

    file = fopen(fileName.c_str(), "a");

    if (file == NULL) {
        fprintf(stderr, T_RED "[FAILED]: Failed to create file: %s\n" T_RESET, fileName.c_str());
        return 1;
    }

    while (lineCount < MAX_LINES && std::fgets(lines[lineCount], MAX_LINE_LENGTH, file) != nullptr) {
        lineCount++;
    }

    std::fclose(file);

    std::cout << T_CYAN << "[Start inputing text (Q for exit)]: " << T_RESET << std::endl;
    while (lineCount < MAX_LINES) {
        std::cout << lineCount + 1 << ": ";

        if (std::fgets(lines[lineCount], sizeof(lines[lineCount]), stdin) == NULL) {
            fprintf(stderr, T_RED "[FAILED]: Failed to read line\n" T_RESET);
            break;
        }

        lines[lineCount][std::strcspn(lines[lineCount], "\n")] = 0;

        if (strlen(lines[lineCount]) == 0 || strcmp(lines[lineCount], "Q") == 0) {
            break;
        }

        std::strcpy(lines[lineCount], lines[lineCount]);
        lineCount++;

        if (lineCount >= MAX_LINES) {
            std::cout << T_YELLOW << "[WARNING]: достигнуто максимальное количество строк. Завершите ввод" << T_RESET << std::endl;
            break;
        }
    }

    file = fopen(fileName.c_str(), "w");
    if (file == NULL) {
        fprintf(stderr, T_RED "[FAILED]: Cannot open file for writing: %s\n" T_RESET, fileName.c_str());
        return 1;
    }

    for (int i = 0; i < lineCount; i++) {
        fprintf(file, "%s\n", lines[i]);
    }

    std::fclose(file);
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
