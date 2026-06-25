#pragma once

#include <string>

namespace runpod_ico_classifier {

struct AppConfig {
    std::string domain;
    std::string icons_directory;
};

std::string default_config_path();
bool load_config(AppConfig &config, std::string &error);
bool save_config(const AppConfig &config, std::string &error);
std::string normalize_domain(std::string domain);

} // namespace runpod_ico_classifier
