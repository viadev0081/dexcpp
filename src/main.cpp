#include <ncurses.h>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "./lib/dexlib.hpp"

namespace {

constexpr int kTabStop = 4;
constexpr std::size_t kMaxUndo = 256;
constexpr std::size_t kMaxRecent = 6;
constexpr const char* kVersion = "1.0";

/* ------------------------------------------------------------------ */
/*  Small utilities                                                   */
/* ------------------------------------------------------------------ */
std::string trim(const std::string& s) {
    std::size_t first = 0;
    while (first < s.size() &&
           (s[first] == ' ' || s[first] == '\t' || s[first] == '\r' ||
            s[first] == '\n')) {
        ++first;
    }

    std::size_t last = s.size();
    while (last > first &&
           (s[last - 1] == ' ' || s[last - 1] == '\t' ||
            s[last - 1] == '\r' || s[last - 1] == '\n')) {
        --last;
    }
    return s.substr(first, last - first);
}

std::string lowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    return s;
}

int digitCount(std::size_t value) {
    int digits = 1;
    while (value >= 10) {
        value /= 10;
        ++digits;
    }

    return digits;
}

int clampInt(int value, int low, int high) {
    if (value < low) {
        return low;
    }

    if (value > high) {
        return high;
    }

    return value;
}

/* ------------------------------------------------------------------ */
/*  Recent files                                                      */
/* ------------------------------------------------------------------ */
std::string recentFilePath() {
    const char* home = std::getenv("HOME");

    if (home == nullptr) {
        return ".dexpp_recent";
    }

    return std::string(home) + "/.dexpp_recent";
}

std::vector<std::string> loadRecents() {
    std::vector<std::string> recents;
    std::ifstream in(recentFilePath());
    if (!in.is_open()) {
        return recents;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        bool duplicate = false;
        for (const std::string& existing : recents) {
            if (existing == line) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            recents.push_back(line);
        }
        if (recents.size() >= kMaxRecent) {
            break;
        }
    }
    return recents;
}

void saveRecents(const std::vector<std::string>& recents) {
    std::ofstream out(recentFilePath(), std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    for (const std::string& path : recents) {
        out << path << '\n';
    }
}

void addRecent(
    std::vector<std::string>& recents,
    const std::string& path
) {
    if (path.empty()) {
        return;
    }

    for (auto it = recents.begin(); it != recents.end(); ++it) {
        if (*it == path) {
            recents.erase(it);
            break;
        }
    }
    recents.insert(recents.begin(), path);
    if (recents.size() > kMaxRecent) {
        recents.resize(kMaxRecent);
    }

    saveRecents(recents);
}

void setMessage(
    App& app,
    const std::string& text,
    int style = 0
) {
    app.message = text;
    app.messageStyle = style;
}

int messageAttr(const App& app) {
    const dexlib::Theme& t = dexlib::theme();
    if (app.messageStyle == 1) {
        return t.error;
    }
    if (app.messageStyle == 2) {
        return t.success;
    }

    return t.command;
}

/* ------------------------------------------------------------------ */
/*  Editing operations                                                */
/* ------------------------------------------------------------------ */
void pushUndo(Editor& ed) {
    Snapshot snap;
    snap.lines = ed.lines;
    snap.cy = ed.cy;
    snap.cx = ed.cx;
    snap.modified = ed.modified;
    ed.undo.push_back(std::move(snap));
    if (ed.undo.size() > kMaxUndo) {
        ed.undo.erase(ed.undo.begin());
    }
}

bool undoLast(Editor& ed) {
    if (ed.undo.empty()) {
        return false;
    }

    Snapshot snap = std::move(ed.undo.back());
    ed.undo.pop_back();
    ed.lines = std::move(snap.lines);
    ed.cy = snap.cy;
    ed.cx = snap.cx;
    ed.modified = snap.modified;
    ed.desiredCol = -1;
    return true;
}

void normalizeCursor(Editor& ed) {
    if (ed.lines.empty()) {
        ed.lines.push_back("");
    }

    if (ed.cy < 0) {
        ed.cy = 0;
    }
    if (ed.cy >= static_cast<int>(ed.lines.size())) {
        ed.cy = static_cast<int>(ed.lines.size()) - 1;
    }

    if (ed.cx > ed.lines[ed.cy].size()) {
        ed.cx = ed.lines[ed.cy].size();
    }
}

void resetView(Editor& ed) {
    ed.cy = 0;
    ed.cx = 0;
    ed.scrollX = 0;
    ed.scrollY = 0;
    ed.desiredCol = -1;
    ed.undo.clear();
    normalizeCursor(ed);
}

void insertText(Editor& ed, const std::string& text) {
    if (text.empty()) {
        return;
    }

    pushUndo(ed);

    if (ed.lines.empty()) {
        ed.lines.push_back("");
    }

    normalizeCursor(ed);
    ed.lines[ed.cy].insert(ed.cx, text);
    ed.cx += text.size();
    ed.modified = true;
    ed.desiredCol = -1;
}

void insertNewline(Editor& ed) {
    pushUndo(ed);
    if (ed.lines.empty()) {
        ed.lines.push_back("");
    }

    normalizeCursor(ed);

    std::string tail = ed.lines[ed.cy].substr(ed.cx);
    ed.lines[ed.cy].erase(ed.cx);
    ed.lines.insert(ed.lines.begin() + ed.cy + 1, tail);
    ++ed.cy;
    ed.cx = 0;
    ed.modified = true;
    ed.desiredCol = -1;
}

void backspace(Editor& ed) {
    normalizeCursor(ed);
    if (ed.cx > 0) {
        pushUndo(ed);
        std::size_t prev = dexlib::utf8Prev(ed.lines[ed.cy], ed.cx);
        ed.lines[ed.cy].erase(prev, ed.cx - prev);
        ed.cx = prev;
        ed.modified = true;
        ed.desiredCol = -1;
    }
    else if (ed.cy > 0) {
        pushUndo(ed);
        std::size_t joinAt = ed.lines[ed.cy - 1].size();
        ed.lines[ed.cy - 1] += ed.lines[ed.cy];
        ed.lines.erase(ed.lines.begin() + ed.cy);
        --ed.cy;
        ed.cx = joinAt;
        ed.modified = true;
        ed.desiredCol = -1;
    }
}

void deleteForward(Editor& ed) {
    normalizeCursor(ed);
    if (ed.cx < ed.lines[ed.cy].size()) {
        pushUndo(ed);
        std::size_t next = dexlib::utf8Next(ed.lines[ed.cy], ed.cx);
        ed.lines[ed.cy].erase(ed.cx, next - ed.cx);
        ed.modified = true;
        ed.desiredCol = -1;
    }
    else if (ed.cy + 1 < static_cast<int>(ed.lines.size())) {
        pushUndo(ed);
        ed.lines[ed.cy] += ed.lines[ed.cy + 1];
        ed.lines.erase(ed.lines.begin() + ed.cy + 1);
        ed.modified = true;
        ed.desiredCol = -1;
    }
}

void deleteCharAtCursor(Editor& ed) {
    normalizeCursor(ed);
    if (ed.cx >= ed.lines[ed.cy].size()) {
        return;
    }

    pushUndo(ed);
    std::size_t next = dexlib::utf8Next(ed.lines[ed.cy], ed.cx);
    ed.lines[ed.cy].erase(ed.cx, next - ed.cx);
    ed.modified = true;
    ed.desiredCol = -1;
}

void moveLeft(Editor& ed) {
    normalizeCursor(ed);
    if (ed.cx > 0) {
        ed.cx = dexlib::utf8Prev(ed.lines[ed.cy], ed.cx);
    }
    else if (ed.cy > 0) {
        --ed.cy;
        ed.cx = ed.lines[ed.cy].size();
    }

    ed.desiredCol = -1;
}

void moveRight(Editor& ed) {
    normalizeCursor(ed);
    if (ed.cx < ed.lines[ed.cy].size()) {
        ed.cx = dexlib::utf8Next(ed.lines[ed.cy], ed.cx);
    }
    else if (ed.cy + 1 < static_cast<int>(ed.lines.size())) {
        ++ed.cy;
        ed.cx = 0;
    }

    ed.desiredCol = -1;
}

void moveVertical(Editor& ed, int delta) {
    normalizeCursor(ed);
    if (ed.desiredCol < 0) {
        ed.desiredCol = dexlib::columnAt(ed.lines[ed.cy], ed.cx, kTabStop);
    }

    int target = ed.cy + delta;
    if (target < 0) {
        target = 0;
    }

    if (target >= static_cast<int>(ed.lines.size())) {
        target = static_cast<int>(ed.lines.size()) - 1;
    }

    ed.cy = target;
    ed.cx = dexlib::byteAtColumn(ed.lines[ed.cy], ed.desiredCol, kTabStop);
}

void moveHome(Editor& ed) {
    ed.cx = 0;
    ed.desiredCol = -1;
}

void moveEnd(Editor& ed) {
    ed.cx = ed.lines[ed.cy].size();
    ed.desiredCol = -1;
}

/* ------------------------------------------------------------------ */
/*  File actions                                                      */
/* ------------------------------------------------------------------ */
bool doSave(App& app) {
    if (app.ed.path.empty()) {
        setMessage(app, "No file name — use :w <file>", 1);
        return false;
    }

    if (!dexlib::saveFile(app.ed.path, app.ed.lines)) {
        setMessage(app, "Could not write: " + app.ed.path, 1);
        app.afterSaveQuit = false;
        return false;
    }

    app.ed.modified = false;
    addRecent(app.recents, app.ed.path);
    setMessage(app, "Saved " + dexlib::baseName(app.ed.path), 2);

    if (app.afterSaveQuit) {
        app.afterSaveQuit = false;
        app.running = false;
    }

    return true;
}

void doSaveAs(
    App& app,
    const std::string& rawPath
) {
    std::string path = dexlib::absolutePath(trim(rawPath));
    if (path.empty()) {
        setMessage(app, "No file name given", 1);
        app.afterSaveQuit = false;
        return;
    }

    std::string oldPath = app.ed.path;
    app.ed.path = path;
    if (!doSave(app)) {
        app.ed.path = oldPath;
    }
}

bool doOpen(App& app, const std::string& rawPath) {
    std::string path = dexlib::absolutePath(trim(rawPath));
    if (path.empty()) {
        setMessage(app, "No file name given", 1);
        return false;
    }

    std::vector<std::string> lines;
    if (!dexlib::readFileLines(path, lines)) {
        setMessage(app, "Could not open: " + dexlib::baseName(path), 1);
        return false;
    }

    app.ed.path = path;
    app.ed.lines = std::move(lines);
    app.ed.modified = false;
    app.mode = Mode::Insert;
    resetView(app.ed);
    addRecent(app.recents, path);
    setMessage(app, "Opened " + dexlib::baseName(path), 2);
    return true;
}

bool doNew(App& app, const std::string& rawName) {
    std::string name = trim(rawName);
    if (name.empty()) {
        name = "untitled.txt";
    }

    std::string path = dexlib::absolutePath(name);
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        setMessage(app, "File already exists — press o to open it", 1);
        return false;
    }

    app.ed.path = path;
    app.ed.lines.assign(1, std::string());
    app.ed.modified = false;
    app.mode = Mode::Insert;
    resetView(app.ed);
    setMessage(app, "New file " + dexlib::baseName(path), 2);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Prompts and commands                                              */
/* ------------------------------------------------------------------ */
void beginPrompt(
    App& app,
    PromptKind kind,
    const std::string& label
) {
    app.returnMode = app.mode;
    app.prompting = true;
    app.promptKind = kind;
    app.promptLabel = label;
    app.input.clear();
    app.message.clear();
}

void cancelPrompt(App& app) {
    if (app.promptKind == PromptKind::SaveAs) {
        app.afterSaveQuit = false;
    }

    app.prompting = false;
    app.promptKind = PromptKind::None;
    app.input.clear();
    app.mode = app.returnMode;
    setMessage(app, "");
}

std::pair<std::string, std::string> splitCommand(const std::string& line) {
    std::string text = trim(line);
    std::size_t space = 0;
    while (space < text.size() && text[space] != ' ' && text[space] != '\t') {
        ++space;
    }

    std::string name = text.substr(0, space);
    std::string rest = space < text.size() ? trim(text.substr(space + 1)) : "";

    return {
        lowerAscii(name),
        rest
    };
}

void executeCommand(
    App& app,
    const std::string& commandLine
) {
    auto parts = splitCommand(commandLine);
    const std::string& name = parts.first;
    const std::string& rest = parts.second;

    if (name.empty()) {
        return;
    }

    if (name == "w" || name == "write") {
        if (!rest.empty()) {
            doSaveAs(app, rest);
        }
        else if (app.ed.path.empty()) {
            beginPrompt(app, PromptKind::SaveAs, "Save as: ");
        }
        else {
            doSave(app);
        }

        return;
    }

    if (name == "wq" || name == "x") {
        if (name == "x" && !app.ed.modified && rest.empty()) {
            app.running = false;
            return;
        }

        app.afterSaveQuit = true;
        if (!rest.empty()) {
            doSaveAs(app, rest);
        }
        else if (app.ed.path.empty()) {
            beginPrompt(app, PromptKind::SaveAs, "Save as: ");
        }
        else {
            doSave(app);
        }

        return;
    }

    if (name == "q" || name == "quit") {
        if (app.ed.modified) {
            setMessage(app, "Unsaved changes — use :q! to force quit", 1);
        }
        else {
            app.running = false;
        }

        return;
    }

    if (name == "q!") {
        app.running = false;
        return;
    }

    if (name == "e" || name == "edit" || name == "e!") {
        if (rest.empty()) {
            beginPrompt(app, PromptKind::Open, "Open file: ");
        }
        else if (app.ed.modified && name != "e!") {
            setMessage(app, "Unsaved changes — save first or use :e!", 1);
        }
        else if (doOpen(app, rest)) {
            app.dashboard = false;
        }

        return;
    }

    if (name == "n" || name == "new") {
        beginPrompt(app, PromptKind::New, "New file: ");
        return;
    }

    /* Recently Files Window */
    if (name == "rf") {
        dexlib::recentlyFilesWindow(app.recents);
        return;
    }

    if (name == "help" || name == "h") {
        setMessage(app,
                   "Keys: arrows move · i/a/o insert · Esc normal · :w save · "
                   ":q quit · ^S save · ^O open · ^Z undo",
                   0
        );

        return;
    }

    setMessage(app, "Unknown command: " + name, 1);
}

void submitPrompt(App& app, bool dashboardPrompt) {
    PromptKind kind = app.promptKind;
    std::string value = app.input;
    Mode previousMode = app.returnMode;

    app.prompting = false;
    app.promptKind = PromptKind::None;
    app.input.clear();
    app.mode = previousMode;

    switch (kind) {
        case PromptKind::Command:
            executeCommand(app, value);
        break;

        case PromptKind::Open:
            if (doOpen(app, value)) {
                app.dashboard = false;
                app.mode = Mode::Insert;
            }
        break;

        case PromptKind::New:
            if (doNew(app, value)) {
                app.dashboard = false;
                app.mode = Mode::Insert;
            }
        break;

        case PromptKind::SaveAs:
            doSaveAs(app, value);
        break;

        case PromptKind::None:
        break;
    }

    (void)dashboardPrompt;
}

void deleteInputChar(std::string& input) {
    if (!input.empty()) {
        input.erase(dexlib::utf8Prev(input, input.size()));
    }
}

/* ------------------------------------------------------------------ */
/*  Rendering helpers                                                 */
/* ------------------------------------------------------------------ */
void drawTitleBar(
        int width,
        const std::string& title,
        const std::string& right,
        int titleAttr,
        int rightAttr
) {
    if (width < 8) {
        return;
    }

    int rightWidth = dexlib::textWidth(right);
    int maxTitle = width - 4 - rightWidth - 1;
    if (maxTitle > 0) {
        dexlib::putClipped(0, 2, maxTitle, title, titleAttr);
    }

    if (rightWidth < width - 3) {
        dexlib::putRight(0, 2, width - 3, right, rightAttr);
    }
}

std::string modeLabel(const App& app) {
    if (app.prompting) {
        return app.promptKind == PromptKind::Command ? "COMMAND" : "PROMPT";
    }

    return app.mode == Mode::Insert ? "INSERT" : "NORMAL";
}

void drawEditor(App& app) {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    erase();

    const dexlib::Theme& t = dexlib::theme();
    dexlib::drawFrame(0, 0, height, width, t);

    if (height < 8 || width < 26) {
        dexlib::putCentered(height / 2, 0, width, "Terminal too small", t.error);
        refresh();
        return;
    }

    const std::string name =
        app.ed.path.empty() ? "[No Name]" : dexlib::baseName(app.ed.path);

    const std::string title =
        " DexCPP · " + name + (app.ed.modified ? " [+]" : "") + " ";

    const std::string right = " " + modeLabel(app) + " ";
    drawTitleBar(width, title, right, t.accent, t.command);

    const int contentTop = 1;
    const int contentRows = height - 4;
    const int statusY = height - 3;
    const int commandY = height - 2;

    int gutter = digitCount(app.ed.lines.size());
    if (gutter < 3) {
        gutter = 3;
    }

    const int textX = gutter + 3;
    int textCols = width - textX - 1;
    if (textCols < 1) {
        textCols = 1;
    }

    normalizeCursor(app.ed);

    if (app.ed.cy < app.ed.scrollY) {
        app.ed.scrollY = app.ed.cy;
    }

    if (app.ed.cy >= app.ed.scrollY + contentRows) {
        app.ed.scrollY = app.ed.cy - contentRows + 1;
    }

    if (app.ed.scrollY < 0) {
        app.ed.scrollY = 0;
    }

    int cursorCol = dexlib::columnAt(app.ed.lines[app.ed.cy], app.ed.cx, kTabStop);
    if (cursorCol < app.ed.scrollX) {
        app.ed.scrollX = cursorCol;
    }

    if (cursorCol >= app.ed.scrollX + textCols) {
        app.ed.scrollX = cursorCol - textCols + 1;
    }

    if (app.ed.scrollX < 0) {
        app.ed.scrollX = 0;
    }

    const std::string marker = dexlib::unicodeBox() ? "▌" : ">";

    for (int row = 0; row < contentRows; ++row) {
        int index = app.ed.scrollY + row;
        int y = contentTop + row;

        if (index >= static_cast<int>(app.ed.lines.size())) {
            dexlib::putClipped(y, 2, width - 4, "~", t.dim);
            continue;
        }

        bool current = index == app.ed.cy;
        int numberAttr = current ? t.accent : t.gutter;

        if (current) {
            dexlib::putClipped(y, 1, 1, marker, t.accent);
        }

        attrset(numberAttr);
        mvprintw(y, 2, "%*d", gutter, index + 1);
        attrset(0);

        std::string expanded = dexlib::expandTabs(app.ed.lines[index], kTabStop);

        std::string visible = dexlib::sliceColumns(
                    expanded,
                    app.ed.scrollX,
                    textCols
        );

        dexlib::putClipped(y, textX, textCols, visible, t.text);
    }

    /* -------------------------------------------------------------- */
    /*  Status bar                                                     */
    /* -------------------------------------------------------------- */
    dexlib::fillRow(statusY, 1, width - 2, t.status);

    int percent = static_cast<int>(app.ed.lines.size()) == 0
        ? 100
        : static_cast<int>((app.ed.cy + 1) * 100 /
            static_cast<int>(app.ed.lines.size()));

    int shownCol = dexlib::columnAt(app.ed.lines[app.ed.cy], app.ed.cx, kTabStop) + 1;

    std::string rightStatus = " Ln " + std::to_string(app.ed.cy + 1) +
                              ", Col " + std::to_string(shownCol) +
                              "   UTF-8   " + std::to_string(percent) + "% ";

    int rightWidth = dexlib::textWidth(rightStatus);
    if (rightWidth > width - 8) {
        rightStatus = " Ln " + std::to_string(app.ed.cy + 1) + ", Col " +
                      std::to_string(shownCol) + " ";

        rightWidth = dexlib::textWidth(rightStatus);
    }
    if (rightWidth > width - 6) {
        rightStatus.clear();
        rightWidth = 0;
    }

    std::string leftStatus = " " + modeLabel(app) + "  " + name +
                             (app.ed.modified ? " +" : "");

    int leftMax = width - 3 - rightWidth;

    if (leftMax > 0) {
        dexlib::putClipped(statusY, 2, leftMax, leftStatus, t.status);
    }
    if (!rightStatus.empty()) {
        dexlib::putRight(statusY, 1, width - 2, rightStatus, t.status);
    }

    /* -------------------------------------------------------------- */
    /*  Command / hint line                                            */
    /* -------------------------------------------------------------- */
    int cursorY = contentTop;
    int cursorX = textX;

    if (app.prompting) {
        std::string line = app.promptLabel + app.input;
        dexlib::putClipped(commandY, 2, width - 4, line, t.command);
        cursorY = commandY;
        cursorX = 2 + dexlib::textWidth(line);
    }
    else if (!app.message.empty()) {
        dexlib::putClipped(commandY, 2, width - 4, app.message,
                           messageAttr(app));
        cursorY = contentTop + (app.ed.cy - app.ed.scrollY);
        cursorX = textX + (cursorCol - app.ed.scrollX);
    }
    else {
        std::string hint =
            app.mode == Mode::Insert
                ? " Esc normal · ^S/F2 save · ^O/F3 open · ^P command · "
                  "^Q/F10 quit"
                : " i insert · a append · o new line · : command · ^S save · "
                  "^Q quit · u undo";

        dexlib::putClipped(commandY, 2, width - 4, hint, t.dim);
        cursorY = contentTop + (app.ed.cy - app.ed.scrollY);
        cursorX = textX + (cursorCol - app.ed.scrollX);
    }

    if (app.prompting) {
        curs_set(1);
        move(cursorY, clampInt(cursorX, 2, width - 2));
    }
    else {
        curs_set(1);
        int screenY = clampInt(contentTop + (app.ed.cy - app.ed.scrollY),
                               contentTop, contentTop + contentRows - 1);
        int screenX = clampInt(textX + (cursorCol - app.ed.scrollX), textX,
                               width - 2);
        move(screenY, screenX);
    }

    refresh();
}

/* ------------------------------------------------------------------ */
/*  Dashboard                                                         */
/* ------------------------------------------------------------------ */
void drawDashboard(App& app) {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    erase();

    const dexlib::Theme& t = dexlib::theme();
    dexlib::drawFrame(0, 0, height, width, t);

    if (height < 9 || width < 34) {
        dexlib::putCentered(
                height / 2,
                0,
                width,
                "Terminal too small for dashboard",
                t.error
        );

        refresh();
        return;
    }

    const std::string sep = dexlib::unicodeBox() ? " · " : " | ";
    drawTitleBar(width, " DexCPP ", " START ", t.accent, t.gutter);

    const int contentTop = 1;
    const int contentRows = height - 4;  /* rows 1 .. height-4 */
    const int statusY = height - 3;
    const int commandY = height - 2;

    const bool bigLogo = width >= 66 && contentRows >= 20;
    const bool smallLogo = !bigLogo && contentRows >= 10 && width >= 40;
    const bool showTagline = contentRows >= 13;
    const bool canShowRecents = contentRows >= 17 && !app.recents.empty();

    std::vector<DashRow> rows;

    auto addText = [&](const std::string& text, int attr) {
        DashRow row;
        row.kind = DashRow::Kind::Text;
        row.text = text;
        row.attr = attr;
        rows.push_back(row);
    };

    auto addMenu = [&](const std::string& key, const std::string& label) {
        DashRow row;
        row.kind = DashRow::Kind::Menu;
        row.key = key;
        row.label = label;
        rows.push_back(row);
    };

    if (bigLogo) {
        static const char * logo[6] = {
            "██████╗ ███████╗██╗  ██╗ ██████╗██████╗ ██████╗ ",
            "██╔══██╗██╔════╝╚██╗██╔╝██╔════╝██╔══██╗██╔══██╗",
            "██║  ██║█████╗   ╚███╔╝ ██║     ██████╔╝██████╔╝",
            "██║  ██║██╔══╝   ██╔██╗ ██║     ██╔═══╝ ██╔═══╝ ",
            "██████╔╝███████╗██╔╝ ██╗╚██████╗██║     ██║     ",
            "╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝╚═╝     ╚═╝     "};
        for (const char * line : logo) {
            addText(line, t.logo);
        }
    }
    else if (smallLogo) {
        addText("D e x C P P", t.logo);
    }

    if (showTagline) {
        addText("", 0);
        addText("a tiny terminal text editor", t.dim);
        addText("", 0);
    }

    addMenu("[ n ]", "New file");
    addMenu("[ o ]", "Open file");
    addMenu("[ q ]", "Quit");
    addMenu("[ r ]", "Recently Files");

    if (canShowRecents) {
        int room = contentRows - static_cast<int>(rows.size()) - 2;
        int count = static_cast<int>(app.recents.size());
        if (count > 4) {
            count = 4;
        }
        if (room < count) {
            count = room;
        }

        if (count > 0) {
            addText("", 0);
            addText("recent files", t.dim);
            for (int i = 0; i < count; ++i) {
                addMenu(
                    "[ " + std::to_string(i + 1) + " ]",
                    dexlib::baseName(app.recents[i])
                );
            }
        }
    }

    /* If even the essential block is too tall, trim spare blank rows. */
    while (static_cast<int>(rows.size()) > contentRows) {
        bool removed = false;
        for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
            if (it->kind == DashRow::Kind::Text && it->text.empty()) {
                rows.erase(std::next(it).base());
                removed = true;
                break;
            }
        }
        if (!removed) {
            break;
        }
    }

    int startY = contentTop;
    if (static_cast<int>(rows.size()) < contentRows) {
        startY += (contentRows - static_cast<int>(rows.size())) / 2;
    }

    /* Keep every menu row on the same left edge. */
    int menuBlock = 0;
    for (const DashRow& row : rows) {
        if (row.kind == DashRow::Kind::Menu) {
            int total = dexlib::textWidth(row.key) + 3 + dexlib::textWidth(row.label);

            if (total > menuBlock) {
            menuBlock = total;
            }
        }
    }

    int menuStartX = 1 + (width - 2 - menuBlock) / 2;
    if (menuStartX < 1) {
        menuStartX = 1;
    }

    int y = startY;
    for (const DashRow& row : rows) {
        if (y >= contentTop + contentRows) {
            break;
        }

        if (row.kind == DashRow::Kind::Text) {
            dexlib::putCentered(y, 1, width - 2, row.text, row.attr);
        }
        else {
            int keyWidth = dexlib::textWidth(row.key);
            dexlib::putClipped(
                y,
                menuStartX,
                width - menuStartX - 1,
                row.key,
                t.accent
            );

            int labelX = menuStartX + keyWidth + 3;
            if (labelX < width - 1) {
                dexlib::putClipped(
                    y,
                    labelX,
                    width - labelX - 1,
                    row.label,
                    t.text
                );
            }
        }
        ++y;
    }

    /* -------------------------------------------------------------- */
    /*  Status bar                                                     */
    /* -------------------------------------------------------------- */
    dexlib::fillRow(statusY, 1, width - 2, t.status);
    dexlib::putClipped(statusY, 2, width - 4, " READY ", t.status);

    std::string rightStatus = " UTF-8" + sep + "ncurses" + sep + "v" + std::string(kVersion) + " ";
    dexlib::putRight(statusY, 1, width - 2, rightStatus, t.status);

    /* -------------------------------------------------------------- */
    /*  Prompt / hint line                                             */
    /* -------------------------------------------------------------- */
    if (app.prompting) {
        std::string line = app.promptLabel + app.input;
        dexlib::putClipped(commandY, 2, width - 4, line, t.command);
        curs_set(1);
        move(commandY,
             clampInt(2 + dexlib::textWidth(line),
                2, width - 2
             )
        );
    }
    else if (!app.message.empty()) {
        dexlib::putClipped(
            commandY,
            2,
            width - 4,
            app.message,
            messageAttr(app)
        );

        curs_set(0);
    }
    else {
        std::string hint = " n new" + sep + "o open" + sep + "r recently files";
        if (!app.recents.empty()) {
            hint += sep + "1-6 recent";
        }

        hint += sep + "q quit";
        dexlib::putClipped(commandY, 2, width - 4, hint, t.dim);
        curs_set(0);
    }

    refresh();
}

/* ------------------------------------------------------------------ */
/*  Input handling                                                    */
/* ------------------------------------------------------------------ */
void openRecent(App& app, int index) {
    if (index < 0 || index >= static_cast<int>(app.recents.size())) {
        return;
    }

    if (doOpen(app, app.recents[index])) {
        app.dashboard = false;
        app.mode = Mode::Insert;
    }
}

void handlePromptChar(App& app, wint_t ch) {
    if (ch == 27) {
        cancelPrompt(app);
        return;
    }
    if (ch == '\n' || ch == '\r') {
        submitPrompt(app, app.dashboard);
        return;
    }
    if (ch == 127 || ch == 8) {
        deleteInputChar(app.input);
        return;
    }
    if (ch == 21) {  /* Ctrl+U */
        app.input.clear();
        return;
    }
    if (ch == 23) {  /* Ctrl+W — delete previous word */
        while (!app.input.empty() && app.input.back() == ' ') {
            app.input.pop_back();
        }

        while (!app.input.empty() && app.input.back() != ' ') {
            app.input.erase(dexlib::utf8Prev(app.input, app.input.size()));
        }
        return;
    }
    if (ch >= 32 && iswprint(ch)) {
        app.input += dexlib::utf8Encode(static_cast<unsigned int>(ch));
    }
}

void handlePromptKey(App& app, int key) {
    switch (key) {
        case KEY_BACKSPACE:
        case KEY_DC:
            deleteInputChar(app.input);
        break;

        case KEY_ENTER:
            submitPrompt(app, app.dashboard);
        break;

        case KEY_HOME:
        break;

        default:
        break;
    }
}

void requestQuit(App& app) {
    if (app.ed.modified) {
        setMessage(app, "Unsaved changes — use :w then :q, or :q!", 1);
    }
    else {
        app.running = false;
    }
}

void handleInsertChar(App& app, wint_t ch) {
    if (ch == 27) {
        app.mode = Mode::Normal;
        setMessage(app, "");
        return;
    }

    switch (ch) {
        case 3:   /* Ctrl+C */
        case 7:   /* Ctrl+G */
            app.mode = Mode::Normal;
        return;

        case 15:  /* Ctrl+O */
            beginPrompt(app, PromptKind::Open, "Open file: ");
        return;

        case 16:  /* Ctrl+P */
            beginPrompt(app, PromptKind::Command, ":");
        return;

        case 17:  /* Ctrl+Q */
            requestQuit(app);
        return;

        case 19:  /* Ctrl+S */
            if (app.ed.path.empty()) {
                beginPrompt(app, PromptKind::SaveAs, "Save as: ");
            }
            else {
                doSave(app);
            }
        return;

        case 26:  /* Ctrl+Z */
            if (!undoLast(app.ed)) {
                setMessage(app, "Nothing to undo", 0);
            }
        return;

        case '\n':
        case '\r':
            insertNewline(app.ed);
        return;

        case '\t':
            insertText(app.ed, "\t");
        return;

        case 127:
        case 8:
            backspace(app.ed);
        return;

        default:
        break;
    }

    if (ch < 32) {
        return;
    }

    if (iswprint(ch)) {
        insertText(
            app.ed,
            dexlib::utf8Encode(static_cast<unsigned int>(ch))
        );
    }
}

void handleNormalChar(App& app, wint_t ch) {
    if (ch == 27) {
        setMessage(app, "");
        return;
    }

    if (ch == ':') {
        beginPrompt(app, PromptKind::Command, ":");
        return;
    }

    switch (ch) {
        case 15:  /* Ctrl+O */
            beginPrompt(app, PromptKind::Open, "Open file: ");
            return;
        case 16:  /* Ctrl+P */
            beginPrompt(app, PromptKind::Command, ":");
            return;
        case 17:  /* Ctrl+Q */
            requestQuit(app);
            return;
        case 19:  /* Ctrl+S */
            if (app.ed.path.empty())
                beginPrompt(app, PromptKind::SaveAs, "Save as: ");
            else
                doSave(app);
            return;
        case 26:  /* Ctrl+Z */
            if (!undoLast(app.ed))
                setMessage(app, "Nothing to undo", 0);
            return;
        case 'i':
            app.mode = Mode::Insert;
            return;
        case 'a':
            if (app.ed.cx < app.ed.lines[app.ed.cy].size()) {
                app.ed.cx = dexlib::utf8Next(app.ed.lines[app.ed.cy],
                                             app.ed.cx);
            }
            app.ed.desiredCol = -1;
            app.mode = Mode::Insert;
            return;
        case 'I':
            moveHome(app.ed);
            app.mode = Mode::Insert;
            return;
        case 'A':
            moveEnd(app.ed);
            app.mode = Mode::Insert;
            return;
        case 'o':
            moveEnd(app.ed);
            insertNewline(app.ed);
            app.mode = Mode::Insert;
            return;
        case 'O':
            moveHome(app.ed);
            insertNewline(app.ed);
            moveVertical(app.ed, -1);
            app.mode = Mode::Insert;
            return;
        case 'h':
            moveLeft(app.ed);
            return;
        case 'j':
            moveVertical(app.ed, 1);
            return;
        case 'k':
            moveVertical(app.ed, -1);
            return;
        case 'l':
            moveRight(app.ed);
            return;
        case 'x':
            deleteCharAtCursor(app.ed);
            return;
        case '0':
            moveHome(app.ed);
            return;
        case '$':
            moveEnd(app.ed);
            return;
        case 'g':
            app.ed.cy = 0;
            app.ed.cx = 0;
            app.ed.desiredCol = -1;
            return;
        case 'G':
            app.ed.cy = static_cast<int>(app.ed.lines.size()) - 1;
            app.ed.cx = app.ed.lines[app.ed.cy].size();
            app.ed.desiredCol = -1;
            return;
        case 'u':
            if (!undoLast(app.ed)) setMessage(app, "Nothing to undo", 0);
            return;
        default:
            return;
    }
}

void handleEditorKey(App& app, int key) {
    app.message.clear();

    if (app.prompting) {
        handlePromptKey(app, key);
        return;
    }

    int page = 10;
    switch (key) {
        case KEY_UP:
            moveVertical(app.ed, -1);
        break;

        case KEY_DOWN:
            moveVertical(app.ed, 1);
        break;

        case KEY_LEFT:
            moveLeft(app.ed);
        break;

        case KEY_RIGHT:
            moveRight(app.ed);
        break;

        case KEY_HOME:
            moveHome(app.ed);
        break;

        case KEY_END:
            moveEnd(app.ed);
        break;

        case KEY_PPAGE:
            for (int i = 0; i < page; ++i) {
                moveVertical(app.ed, -1);
            }
        break;

        case KEY_NPAGE:
            for (int i = 0; i < page; ++i) {
                moveVertical(app.ed, 1);
            }
        break;

        case KEY_BACKSPACE:
            if (app.mode == Mode::Insert) {
                backspace(app.ed);
            }
            else {
                moveLeft(app.ed);
            }
        break;

        case KEY_DC:
            deleteForward(app.ed);
        break;

        case KEY_F(2):
            if (app.ed.path.empty()) {
                beginPrompt(app, PromptKind::SaveAs, "Save as: ");
            }
            else {
                doSave(app);
            }
        break;

        case KEY_F(3):
            beginPrompt(app, PromptKind::Open, "Open file: ");
        break;

        case KEY_F(10):
            requestQuit(app);
        break;

        default:
        break;
    }
}

void handleEditorChar(App& app, wint_t ch) {
    app.message.clear();

    if (app.prompting) {
        handlePromptChar(app, ch);
        return;
    }

    if (app.mode == Mode::Insert) {
        handleInsertChar(app, ch);
    }
    else {
        handleNormalChar(app, ch);
    }
}

void handleDashboardChar(App& app, wint_t ch) {
    app.message.clear();

    if (app.prompting) {
        handlePromptChar(app, ch);
        return;
    }

    if (ch == 27) return;

    if (ch == 'n' || ch == 'N') {
        beginPrompt(app, PromptKind::New, "New file: ");
        return;
    }
    if (ch == 'o' || ch == 'O') {
        beginPrompt(app, PromptKind::Open, "Open file: ");
        return;
    }
    if (ch == 'q' || ch == 'Q' || ch == 17) {
        app.running = false;
        return;
    }
    if (ch == 'r' || ch == 'R') {
        dexlib::recentlyFilesWindow(app.recents);
        return;
    }
    if (ch >= '1' && ch <= '9') {
        openRecent(app, static_cast<int>(ch - '1'));
        return;
    }
}

void handleDashboardKey(App& app, int key) {
    app.message.clear();

    if (app.prompting) {
        handlePromptKey(app, key);
        return;
    }

    (void)key;
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
void printUsage(const char* program) {
    std::printf(
        "DexCPP %s — tiny terminal text editor\n"
        "\n"
        "Usage: %s [file]\n"
        "\n"
        "Keys:\n"
        "  arrows / hjkl   move\n"
        "  i a o O         insert modes\n"
        "  Esc             normal mode\n"
        "  :w  :q  :wq     save / quit\n"
        "  Ctrl+S / F2     save\n"
        "  Ctrl+O / F3     open file\n"
        "  Ctrl+P          command line\n"
        "  Ctrl+Q / F10    quit\n"
        "  Ctrl+Z          undo\n"
        "\n"
        "Start screen: n new · o open · 1-6 recent · q quit\n",
        kVersion, program);
}

}  // namespace

int main(int argc, char ** argv) {
    std::setlocale(LC_ALL, "");

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h" || arg == "help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(1);

    dexlib::initUi();

    App app;
    app.recents = loadRecents();

    if (argc > 1) {
        if (doOpen(app, argv[1])) {
            app.dashboard = false;
            app.mode = Mode::Insert;
        }
    }

    while (app.running) {
        if (app.dashboard) {
            drawDashboard(app);

            wint_t ch = 0;
            int rc = get_wch(&ch);
            if (rc == ERR) {
                continue;
            }

            if (rc == KEY_CODE_YES) {
                handleDashboardKey(app, static_cast<int>(ch));
            }
            else {
                handleDashboardChar(app, ch);
            }
        }
        else {
            drawEditor(app);

            wint_t ch = 0;
            int rc = get_wch(&ch);
            if (rc == ERR) {
                continue;
            }

            if (rc == KEY_CODE_YES) {
                handleEditorKey(app, static_cast<int>(ch));
            }
            else {
                handleEditorChar(app, ch);
            }
        }
    }

    endwin();
    return 0;
}
