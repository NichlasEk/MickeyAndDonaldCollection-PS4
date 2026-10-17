// The bring-up runner uses fixed ASCII settings paths; the full UI is not linked.
#include "app/sys.h"
#include <algorithm>
static std::string ascii_path(const std::wstring& path) {
    if (std::any_of(path.begin(), path.end(), [](wchar_t c) { return c < 0 || c > 127; })) return {};
    return std::string(path.begin(), path.end());
}
bool read_text(const std::wstring& path, std::string& out) {
    out.clear();
    FILE* f = std::fopen(ascii_path(path).c_str(), "rb");
    if (!f) return false;
    char buffer[4096]; size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f))) {
        if (out.size() + n > 1024 * 1024) { std::fclose(f); return false; }
        out.append(buffer, n);
    }
    bool ok = !std::ferror(f); std::fclose(f); return ok;
}
bool write_file(const std::wstring& path, const void* data, size_t n) {
    FILE* f = std::fopen(ascii_path(path).c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data, 1, n, f) == n;
    return std::fclose(f) == 0 && ok;
}
