#pragma once
#include <string>
namespace cg {
std::string exePath();
std::string exeDir();
std::string resourcePath(const std::string& relative);
std::string userDataDir();
std::string userDataPath(const std::string& relative);
bool ensureDir(const std::string& dir);
}
