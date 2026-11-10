#ifndef DEX_LIB_HPP
#define DEX_LIB_HPP

#include <ncurses.h>

#include <cstddef>
#include <string>
#include <vector>

/* ------------------------------------------------------------------ */
/*  Editor state                                                      */
/* ------------------------------------------------------------------ */
struct Snapshot {
    std::vector<std::string> lines;
    int cy = 0;
    std::size_t cx = 0;
    bool modified = false;
};

struct Editor {
    std::string path;
    std::vector<std::string> lines{std::string()};
    int cy = 0;
    std::size_t cx = 0;
    int scrollY = 0;
    int scrollX = 0;
    int desiredCol = -1;
    bool modified = false;
    std::vector<Snapshot> undo;
};

enum class Mode { Insert, Normal };

enum class PromptKind {
    None,
    Command,
    Open,
    New,
    SaveAs,
};

struct App {
    bool running = true;
    bool dashboard = true;

    Editor ed;
    Mode mode = Mode::Insert;
    Mode returnMode = Mode::Insert;

    bool prompting = false;
    PromptKind promptKind = PromptKind::None;
    std::string promptLabel;
    std::string input;

    std::string message;
    int messageStyle = 0;  /* 0 info, 1 error, 2 success */

    bool afterSaveQuit = false;
    std::vector<std::string> recents;
};

struct DashRow {
    enum class Kind { Text, Menu } kind = Kind::Text;
    std::string text;
    std::string key;
    std::string label;
    int attr = 0;
};

namespace dexlib {

/* ------------------------------------------------------------------ */
/*  Theme                                                             */
/* ------------------------------------------------------------------ */
struct Theme {
    int border  = 0;
    int accent  = 0;
    int logo    = 0;
    int gutter  = 0;
    int status  = 0;
    int command = 0;
    int error   = 0;
    int success = 0;
    int text    = 0;
    int dim     = 0;
};

void initUi();
const Theme& theme();

bool unicodeBox();

/* ------------------------------------------------------------------ */
/*  UTF-8 helpers                                                     */
/* ------------------------------------------------------------------ */
int utf8SeqLen(unsigned char lead);

unsigned int utf8Decode(
    const std::string& s, 
    std::size_t pos, 
    int& len
);

std::string utf8Encode(unsigned int cp);

/* Move between code-point boundaries. */
std::size_t utf8Prev(
    const std::string& s, 
    std::size_t pos
);

std::size_t utf8Next(
    const std::string& s, 
    std::size_t pos
);

int cpWidth(unsigned int cp);

int textWidth(const std::string& s);

std::string expandTabs(
    const std::string& s, 
    int tabstop = 4
);

int columnAt(
    const std::string& s, 
    std::size_t bytePos, 
    int tabstop = 4
);

std::size_t byteAtColumn(
    const std::string& s, 
    int column, 
    int tabstop = 4
);

std::string sliceColumns(
    const std::string& s, 
    int startCol, 
    int maxCols
);

std::string ellipsizeLeft(
    const std::string& s, 
    int maxCols
);

/* ------------------------------------------------------------------ */
/*  Drawing                                                           */
/* ------------------------------------------------------------------ */
void drawFrame(
    int y, 
    int x, 
    int height, 
    int width, 
    const Theme& t
);

void putClipped(
    int y, 
    int x, 
    int maxCols, 
    const std::string& s, 
    int attr
);

void putRight(
    int y, 
    int x, 
    int width, 
    const std::string& s, 
    int attr
);

void putCentered(
    int y, 
    int x, 
    int width, 
    const std::string& s, 
    int attr
);

void fillRow(
    int y, 
    int x, 
    int width, 
    int attr
);

/* ------------------------------------------------------------------ */
/*  Files                                                             */
/* ------------------------------------------------------------------ */
bool readFileLines(
    const std::string& path, 
    std::vector<std::string>& lines
);

bool saveFile(
    const std::string& path, 
    const std::vector<std::string>& lines
);

std::string baseName(const std::string& path);
std::string expandUser(const std::string& path);
std::string absolutePath(const std::string& path);

void recentlyFilesWindow(const std::vector<std::string>& recents);

}  // namespace dexlib

#endif  // DEX_LIB_HPP
