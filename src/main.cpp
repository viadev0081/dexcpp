#include <ncurses.h>
#include <locale.h>
#include "./lib/dexlib.hpp"

int main(int argc, char ** argv) 
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    dexlib::drawCenteredHelloWindow("Welcome to the DexCPP editor!");

    echo();
    printw("\nEnter file name: ");
    refresh();

    char fileName[256];
    getstr(fileName);
    noecho();

    std::ofstream out_file(fileName);
    if (!out_file.is_open()) {
        endwin();
        std::cout << T_RED << "[ERROR]: Could not open file " << T_RESET << fileName << std::endl;
        return 1;
    }

    std::vector<std::string> lines;
    int ch;
    int lineNum = 1;
    int currentLine = 0;

    while (true) {
        clear();
        printw("Editing: %s (Alt+S to save, Alt+Q to exit\n", fileName);
        printw("---------------------------------------------------\n");

        for (size_t i = 0; i < lines.size(); ++i) {
            printw("%d: %s\n", static_cast<int>(i + 1), lines[i].c_str());
        } 

        move(currentLine + 2, 3);
        clrtoeol();

        refresh();

        ch = getch();

        if (ch == 27) {
            ch = getch();
            if (ch == 's') {
                dexlib::saveFile(fileName, lines);
                printw("\nFile saved! Press any key to exit...");
                refresh();
                getch();
                break;
            } 
            else if (ch == 'q') {
                break;
            }
        } 
        else if (ch == '\n') {
            lines.push_back("");
            currentLine++;
        } 
        else if (ch == KEY_BACKSPACE || ch == 127) { // Backspace
            if (!lines[currentLine].empty()) {
                lines[currentLine].pop_back();
            }
        } 
        else if (ch == KEY_UP && currentLine > 0) { // Стрелка вверх
            currentLine--;
        } 
        else if (ch == KEY_DOWN && currentLine < static_cast<int>(lines.size()) - 1) { // Стрелка вниз
            currentLine++;
        } 
        else if (ch >= 32 && ch <= 126) { // Печатаемые символы
            if (lines.empty()) {
                lines.push_back("");
            }    
            lines[currentLine] += static_cast<char>(ch);
        }
    }

    endwin();
    return 0;
}
