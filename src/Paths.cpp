#include "Paths.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <shlobj.h>
#elif defined(__APPLE__)
  #include <mach-o/dyld.h>
  #include <unistd.h>
#else
  #include <unistd.h>
  #include <limits.h>
#endif
namespace cg {
namespace {
const char* kAppName = "ClaudeGame";
std::string parentDir(const std::string& p) {
    auto pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return std::string();
    return p.substr(0, pos);
}
std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    if (a.back() == '/' || a.back() == '\\') return a + b;
    return a + sep + b;
}
}
std::string exePath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0) return std::string();
    return std::string(buf, n);
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1, 0);
    if (_NSGetExecutablePath(buf.data(), &size) != 0) return std::string();
    char real[PATH_MAX];
    if (realpath(buf.data(), real)) return std::string(real);
    return std::string(buf.data());
#else
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string();
    buf[n] = '\0';
    return std::string(buf);
#endif
}
std::string exeDir() {
    return parentDir(exePath());
}
std::string resourcePath(const std::string& relative) {
    std::string dir = exeDir();
    std::string stripped = relative;
    const std::string assetsPrefix = "assets/";
    if (stripped.size() > assetsPrefix.size() &&
        stripped.compare(0, assetsPrefix.size(), assetsPrefix) == 0) {
        stripped = stripped.substr(assetsPrefix.size());
    }
    std::vector<std::string> candidates;
#ifdef __APPLE__
    std::string macOsTail = "/Contents/MacOS";
    if (dir.size() >= macOsTail.size() &&
        dir.compare(dir.size() - macOsTail.size(), macOsTail.size(), macOsTail) == 0) {
        std::string contents = dir.substr(0, dir.size() - std::string("/MacOS").size());
        std::string resDir = joinPath(contents, "Resources");
        candidates.push_back(joinPath(resDir, stripped));
        candidates.push_back(joinPath(resDir, relative));
    }
#endif
    candidates.push_back(joinPath(dir, relative));
    candidates.push_back(joinPath(dir, stripped));
    std::string parentExeDir = parentDir(dir);
    if (!parentExeDir.empty()) {
        candidates.push_back(joinPath(parentExeDir, relative));
        candidates.push_back(joinPath(joinPath(parentExeDir, "assets"), stripped));
    }
    candidates.push_back(relative);
    candidates.push_back(joinPath("assets", stripped));
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    return relative;
}
std::string userDataDir() {
#ifdef _WIN32
    PWSTR pw = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pw))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, pw, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string s(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, pw, -1, s.data(), len, nullptr, nullptr);
            out = joinPath(s, kAppName);
        }
        CoTaskMemFree(pw);
    }
    if (out.empty()) {
        const char* a = std::getenv("APPDATA");
        if (a && *a) out = joinPath(a, kAppName);
    }
    ensureDir(out);
    return out;
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    std::string base = home ? home : "";
    std::string dir = joinPath(joinPath(base, "Library/Application Support"), kAppName);
    ensureDir(dir);
    return dir;
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::string base;
    if (xdg && *xdg) {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = std::string(home ? home : "") + "/.local/share";
    }
    std::string dir = joinPath(base, kAppName);
    ensureDir(dir);
    return dir;
#endif
}
std::string userDataPath(const std::string& relative) {
    return joinPath(userDataDir(), relative);
}
bool ensureDir(const std::string& dir) {
    if (dir.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return !ec;
}
}
