#include "./dexlib.hpp"

#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace dexlib {

namespace {

Theme g_theme;
bool g_unicode = true;

std::string repeat(
    const std::string& what,
    int count
) {
    std::string out;
    if (count <= 0) {
        return out;
    }
    out.reserve(what.size() * static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        out += what;
    }

    return out;
}

std::string truncateRight(
    const std::string& s,
    int maxCols
) {
    if (maxCols <= 0) {
        return "";
    }
    if (textWidth(s) <= maxCols) {
        return s;
    }

    std::string out;
    int col = 0;
    std::size_t pos = 0;
    while (pos < s.size()) {
        int len = 0;
        unsigned int cp = utf8Decode(s, pos, len);
        int w = (len == 1 && static_cast<unsigned char>(s[pos]) >= 128) ? 1 : cpWidth(cp);

        if (w < 0) {
            w = 1;
        }
        if (col + w > maxCols) {
            break;
        }

        out.append(s, pos, static_cast<std::size_t>(len));
        col += w;
        pos += static_cast<std::size_t>(len);
    }

    return out;
}

}  // namespace

/* ------------------------------------------------------------------ */
/*  Theme                                                             */
/* ------------------------------------------------------------------ */
void initUi() {
    g_unicode = MB_CUR_MAX > 1;

    if (has_colors()) {
        start_color();
        short bg = COLOR_BLACK;
        if (use_default_colors() == OK) {
            bg = -1;
        }

        init_pair(1, COLOR_CYAN, bg);      // frame
        init_pair(2, COLOR_MAGENTA, bg);   // accent / logo
        init_pair(3, COLOR_BLUE, bg);      // gutter
        init_pair(4, COLOR_BLACK, COLOR_CYAN);  // status bar
        init_pair(5, COLOR_YELLOW, bg);    // command line
        init_pair(6, COLOR_RED, bg);       // error
        init_pair(7, COLOR_GREEN, bg);     // success
        init_pair(8, COLOR_WHITE, bg);     // text

        g_theme.border  = COLOR_PAIR(1) | A_BOLD;
        g_theme.accent  = COLOR_PAIR(2) | A_BOLD;
        g_theme.logo    = COLOR_PAIR(2) | A_BOLD;
        g_theme.gutter  = COLOR_PAIR(3) | A_DIM;
        g_theme.status  = COLOR_PAIR(4) | A_BOLD;
        g_theme.command = COLOR_PAIR(5) | A_BOLD;
        g_theme.error   = COLOR_PAIR(6) | A_BOLD;
        g_theme.success = COLOR_PAIR(7) | A_BOLD;
        g_theme.text    = COLOR_PAIR(8);
        g_theme.dim     = A_DIM;
    }
    else {
        g_theme.border  = A_BOLD;
        g_theme.accent  = A_BOLD;
        g_theme.logo    = A_BOLD;
        g_theme.gutter  = A_DIM;
        g_theme.status  = A_REVERSE;
        g_theme.command = A_BOLD;
        g_theme.error   = A_BOLD;
        g_theme.success = A_BOLD;
        g_theme.text    = 0;
        g_theme.dim     = A_DIM;
    }
}

const Theme& theme() { return g_theme; }

bool unicodeBox() { return g_unicode; }

/* ------------------------------------------------------------------ */
/*  UTF-8 helpers                                                     */
/* ------------------------------------------------------------------ */
int utf8SeqLen(unsigned char lead) {
    if ((lead & 0x80) == 0) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0) {
        return 4;
    }

    return 1;
}

unsigned int utf8Decode(
    const std::string& s,
    std::size_t pos,
    int& len
) {
    len = 1;
    if (pos >= s.size()) {
        return 0;
    }

    unsigned char lead = static_cast<unsigned char>(s[pos]);
    int n = utf8SeqLen(lead);
    if (n == 1 || pos + static_cast<std::size_t>(n) > s.size()) {
        return lead;
    }

    unsigned int cp = lead & (0xFFu >> (n + 1));
    for (int i = 1; i < n; ++i) {
        unsigned char b = static_cast<unsigned char>(s[pos + static_cast<std::size_t>(i)]);
        if ((b & 0xC0) != 0x80) return lead;
        cp = (cp << 6) | (b & 0x3F);
    }

    /* Reject overlong / invalid sequences. */
    if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) ||
        (n == 4 && cp < 0x10000) || cp > 0x10FFFF ||
        (cp >= 0xD800 && cp <= 0xDFFF)) {
        return lead;
    }

    len = n;
    return cp;
}

std::string utf8Encode(unsigned int cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::size_t utf8Prev(
    const std::string& s,
    std::size_t pos
) {
    if (pos == 0 || s.empty()) {
        return 0;
    }
    if (pos > s.size()) {
        pos = s.size();
    }
    std::size_t p = pos - 1;

    while (p > 0 && (static_cast<unsigned char>(s[p]) & 0xC0) == 0x80) {
        --p;
    }

    return p;
}

std::size_t utf8Next(
    const std::string& s,
    std::size_t pos
) {
    if (pos >= s.size()) {
        return s.size();
    }

    int n = utf8SeqLen(static_cast<unsigned char>(s[pos]));
    std::size_t next = pos + static_cast<std::size_t>(n);
    return next > s.size() ? s.size() : next;
}

int cpWidth(unsigned int cp) {
    if (cp == 0) {
        return 0;
    }
    if (cp < 32 || cp == 127) {
        return 1;  /* rendered as '?' */
    }

    int w = wcwidth(static_cast<wchar_t>(cp));
    if (w > 0) {
        return w;
    }
    if (w == 0) {
        return 0;
    }

    return 1;
}

int textWidth(const std::string& s) {
    int col = 0;
    std::size_t pos = 0;
    while (pos < s.size()) {
        int len = 0;
        unsigned int cp = utf8Decode(s, pos, len);
        if (len == 1 && static_cast<unsigned char>(s[pos]) >= 128) {
            ++col;
        }
        else {
            col += cpWidth(cp);
        }
        pos += static_cast<std::size_t>(len);
    }
    return col;
}

std::string expandTabs(
    const std::string& s,
    int tabstop
) {
    if (tabstop <= 0) {
        tabstop = 4;
    }

    std::string out;
    int col = 0;
    std::size_t pos = 0;
    while (pos < s.size()) {
        unsigned char lead = static_cast<unsigned char>(s[pos]);
        if (lead == '\t') {
            int spaces = tabstop - (col % tabstop);
            out.append(static_cast<std::size_t>(spaces), ' ');
            col += spaces;
            ++pos;
            continue;
        }

        int len = 0;
        unsigned int cp = utf8Decode(s, pos, len);
        bool invalid = (len == 1 && lead >= 128) || cp < 32 || cp == 127;
        if (invalid) {
            out.push_back('?');
            ++col;
        }
        else {
            out.append(s, pos, static_cast<std::size_t>(len));
            col += cpWidth(cp);
        }
        pos += static_cast<std::size_t>(len);
    }
    return out;
}

int columnAt(
    const std::string& s,
    std::size_t bytePos,
    int tabstop
) {
    if (tabstop <= 0) {
        tabstop = 4;
    }
    if (bytePos > s.size()) {
        bytePos = s.size();
    }

    int col = 0;
    std::size_t pos = 0;
    while (pos < bytePos) {
        unsigned char lead = static_cast<unsigned char>(s[pos]);
        if (lead == '\t') {
            col += tabstop - (col % tabstop);
            ++pos;
            continue;
        }

        int len = 0;
        unsigned int cp = utf8Decode(s, pos, len);
        if (len == 1 && lead >= 128) {
            ++col;
        }
        else {
            col += cpWidth(cp);
        }
        pos += static_cast<std::size_t>(len);
    }
    return col;
}

std::size_t byteAtColumn(
    const std::string& s,
    int column,
    int tabstop
) {
    if (tabstop <= 0) {
        tabstop = 4;
    }
    if (column <= 0) {
        return 0;
    }

    int col = 0;
    std::size_t pos = 0;
    while (pos < s.size()) {
        unsigned char lead = static_cast<unsigned char>(s[pos]);
        if (lead == '\t') {
            int next = col + (tabstop - (col % tabstop));
            if (next > column) {
                return pos;
            }

            col = next;
            ++pos;
            continue;
        }

        int len = 0;
        unsigned int cp = utf8Decode(s, pos, len);
        int w = (len == 1 && lead >= 128) ? 1 : cpWidth(cp);
        if (col + w > column) {
            return pos;
        }

        col += w;
        pos += static_cast<std::size_t>(len);
    }
    return s.size();
}

std::string sliceColumns(
    const std::string& s,
    int startCol,
    int maxCols
) {
    if (maxCols <= 0) {
        return "";
    }
    if (startCol < 0) {
        startCol = 0;
    }

    std::string out;
    int col = 0;
    std::size_t pos = 0;
    const int endCol = startCol + maxCols;

    while (pos < s.size() && col < endCol) {
        unsigned char lead = static_cast<unsigned char>(s[pos]);
        if (lead == '\t') {
            int next = col + (4 - (col % 4));
            int from = col < startCol ? startCol : col;
            int to = next < endCol ? next : endCol;
            if (to > from) out.append(static_cast<std::size_t>(to - from), ' ');
            col = next;
            ++pos;
            continue;
        }

        std::size_t start = pos;
        int len = 0;
        unsigned int cp = utf8Decode(s, pos, len);
        int w = (len == 1 && lead >= 128) ? 1 : cpWidth(cp);
        if (w < 0) {
            w = 1;
        }

        if (col >= startCol && col + w <= endCol) {
            out.append(s, start, static_cast<std::size_t>(len));
        }

        col += w;
        pos += static_cast<std::size_t>(len);
    }

    return out;
}

std::string ellipsizeLeft(const std::string& s, int maxCols) {
    if (maxCols <= 0) {
        return "";
    }

    if (textWidth(s) <= maxCols) {
        return s;
    }

    const std::string dots = g_unicode ? "…" : "...";
    int dotsWidth = textWidth(dots);
    if (dotsWidth >= maxCols) {
        return truncateRight(dots, maxCols);
    }

    std::string out = s;
    int budget = maxCols - dotsWidth;
    while (!out.empty() && textWidth(out) > budget) {
        out.erase(0, utf8Next(out, 0));
    }
    return dots + out;
}

void drawFrame(
    int y,
    int x,
    int height,
    int width,
    const Theme& t
) {
    if (height < 2 || width < 2) {
        return;
    }

    const std::string tl = g_unicode ? "╭" : "+";
    const std::string tr = g_unicode ? "╮" : "+";
    const std::string bl = g_unicode ? "╰" : "+";
    const std::string br = g_unicode ? "╯" : "+";
    const std::string hz = g_unicode ? "─" : "-";
    const std::string vt = g_unicode ? "│" : "|";

    attrset(t.border);

    if (width > 2) {
        const std::string line = repeat(hz, width - 2);
        mvaddstr(y, x + 1, line.c_str());
        mvaddstr(y + height - 1, x + 1, line.c_str());
    }
    for (int row = 1; row < height - 1; ++row) {
        mvaddstr(y + row, x, vt.c_str());
        mvaddstr(y + row, x + width - 1, vt.c_str());
    }

    mvaddstr(y, x, tl.c_str());
    mvaddstr(y, x + width - 1, tr.c_str());
    mvaddstr(y + height - 1, x, bl.c_str());
    mvaddstr(y + height - 1, x + width - 1, br.c_str());

    attrset(0);
}

void putClipped(
    int y,
    int x,
    int maxCols,
    const std::string& s,
    int attr
) {
    if (maxCols <= 0) {
        return;
    }
    std::string visible = truncateRight(s, maxCols);
    attrset(attr);
    mvaddstr(y, x, visible.c_str());
    attrset(0);
}

void putRight(
    int y,
    int x,
    int width,
    const std::string& s,
    int attr
) {
    if (width <= 0) {
        return;
    }

    std::string text = s;

    if (textWidth(text) > width) {
        text = ellipsizeLeft(text, width);
    }

    int start = x + width - textWidth(text);
    putClipped(y, start, width, text, attr);
}

void putCentered(
    int y,
    int x,
    int width,
    const std::string& s,
    int attr
) {
    if (width <= 0) {
        return;
    }

    std::string text = s;

    if (textWidth(text) > width) {
        text = ellipsizeLeft(text, width);
    }

    int start = x + (width - textWidth(text)) / 2;

    putClipped(y, start, width, text, attr);
}

void fillRow(
    int y,
    int x,
    int width,
    int attr
) {
    if (width <= 0) {
        return;
    }

    attrset(attr);
    mvhline(y, x, ' ', width);
    attrset(0);
}

std::string expandUser(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }

    const char* home = std::getenv("HOME");

    if (home == nullptr) {
        return path;
    }

    if (path.size() == 1) {
        return std::string(home);
    }

    if (path[1] == '/') {
        return std::string(home) + path.substr(1);
    }

    return path;
}

std::string absolutePath(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::error_code ec;
    std::filesystem::path p(expandUser(path));
    std::filesystem::path abs = std::filesystem::absolute(p, ec);

    if (ec) {
        return path;
    }

    return abs.lexically_normal().string();
}

std::string baseName(const std::string& path) {
    if (path.empty()) {
        return "[No Name]";
    }

    std::size_t slash = path.find_last_of("/\\");

    if (slash == std::string::npos) {
        return path;
    }

    if (slash + 1 >= path.size()) {
        return path;
    }

    return path.substr(slash + 1);
}

bool readFileLines(
    const std::string& path,
    std::vector<std::string>& lines
) {
    std::ifstream in(expandUser(path), std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    lines.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        lines.push_back(line);
    }

    if (lines.empty()) {
        lines.push_back("");
    }

    return true;
}

bool saveFile(
    const std::string& path,
    const std::vector<std::string>& lines
) {
    if (path.empty()) {
        return false;
    }

    std::ofstream out(expandUser(path), std::ios::binary | std::ios::trunc);

    if (!out.is_open()) {
        return false;
    }

    for (const std::string& line : lines) {
        out << line << '\n';
    }

    return out.good();
}

}  // namespace dexlib
