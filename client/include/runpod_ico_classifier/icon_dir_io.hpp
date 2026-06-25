#pragma once

#include <string>
#include <vector>

namespace runpod_ico_classifier {

struct IconFileEntry {
    std::string filename;
    std::string filepath;
};

std::vector<IconFileEntry> list_icon_files(const std::string &directory,
                                           std::string &error);

} // namespace runpod_ico_classifier
