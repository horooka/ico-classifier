#include "runpod_ico_classifier/icon_dir_io.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace runpod_ico_classifier {
namespace {

bool is_image_extension(const std::string &ext_lower) {
    return ext_lower == ".png" || ext_lower == ".jpg" || ext_lower == ".jpeg" ||
           ext_lower == ".webp" || ext_lower == ".bmp" || ext_lower == ".gif" ||
           ext_lower == ".ico";
}

} // namespace

std::vector<IconFileEntry> list_icon_files(const std::string &directory,
                                           std::string &error) {
    std::vector<IconFileEntry> entries;
    std::error_code ec;
    const std::filesystem::path root(directory);
    if (!std::filesystem::is_directory(root, ec)) {
        error = "Directory not found: " + directory;
        return entries;
    }

    for (const auto &item : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            error = ec.message();
            return entries;
        }
        if (!item.is_regular_file())
            continue;

        std::string ext = item.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (!is_image_extension(ext))
            continue;

        IconFileEntry entry;
        entry.filename = item.path().filename().string();
        entry.filepath = item.path().string();
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(),
              [](const IconFileEntry &a, const IconFileEntry &b) {
                  return a.filename < b.filename;
              });
    return entries;
}

} // namespace runpod_ico_classifier
