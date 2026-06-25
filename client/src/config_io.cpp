#include "runpod_ico_classifier/config_io.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>

namespace runpod_ico_classifier {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
                value.end());
    return value;
}

} // namespace

std::string default_config_path() {
    return Glib::build_filename(Glib::get_user_config_dir(),
                                "ico-classifier.ini");
}

std::string normalize_domain(std::string domain) {
    domain = trim(domain);
    while (!domain.empty() && domain.back() == '/')
        domain.pop_back();

    if (domain.empty())
        return domain;

    const auto scheme_pos = domain.find("://");
    if (scheme_pos != std::string::npos) {
        const std::string scheme = domain.substr(0, scheme_pos);
        const std::string authority = domain.substr(scheme_pos + 3);
        if (scheme == "http" || scheme == "https")
            return domain;
        // localhost://0.0.0.0:8000 and other pseudo-schemes → plain HTTP.
        return "http://" + authority;
    }

    // host, host:port, 127.0.0.1:7860 — default to HTTP for local development.
    return "http://" + domain;
}

bool load_config(AppConfig &config, std::string &error) {
    const auto path = default_config_path();
    if (!std::filesystem::exists(path))
        return true;

    Glib::KeyFile keyfile;
    try {
        keyfile.load_from_file(path);
        config.domain = normalize_domain(keyfile.get_string("Main", "Domain"));
        if (keyfile.has_group("Main") && keyfile.has_key("Main", "IcoDir"))
            config.icons_directory = keyfile.get_string("Main", "IcoDir");
    } catch (const Glib::Error &ex) {
        error = ex.what();
        return false;
    }
    return true;
}

bool save_config(const AppConfig &config, std::string &error) {
    const auto path = default_config_path();
    Glib::KeyFile keyfile;
    if (std::filesystem::exists(path)) {
        try {
            keyfile.load_from_file(path);
        } catch (const Glib::Error &) {
            // overwrite a corrupted file
        }
    }

    keyfile.set_string("Main", "Domain", normalize_domain(config.domain));
    if (!config.icons_directory.empty())
        keyfile.set_string("Main", "icons_directory", config.icons_directory);

    try {
        const auto dir = Glib::path_get_dirname(path);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        keyfile.save_to_file(path);
    } catch (const Glib::Error &ex) {
        error = ex.what();
        return false;
    }
    return true;
}

} // namespace runpod_ico_classifier
