#pragma once
#include <string>
#include <vector>
namespace i18n {
enum class Lang { EN, ZH };
void init();
void save();
Lang current();
void setLanguage(Lang l);
void toggleLanguage();
const char* tr(const char* key);
const std::vector<int>& codepointsForCurrent();
const char* langToggleLabel();
}
