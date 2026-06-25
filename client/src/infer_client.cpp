#include "runpod_ico_classifier/infer_client.hpp"
#include "runpod_ico_classifier/config_io.hpp"

#include <curl/curl.h>
#include <json-glib/json-glib.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace runpod_ico_classifier {
namespace {

struct CurlGlobal {
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *out = static_cast<std::string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string join_url(const std::string &domain, const std::string &path) {
    if (domain.empty())
        return path;
    if (!path.empty() && path.front() == '/')
        return domain + path;
    return domain + "/" + path;
}

bool http_get(const std::string &url, long &http_code, std::string &body,
              std::string &error) {
    static CurlGlobal curl_global;
    CURL *curl = curl_easy_init();
    if (!curl) {
        error = "Failed to initialize libcurl";
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    const CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        error = curl_easy_strerror(code);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    return true;
}

bool http_post_multipart(const std::string &url,
                         const std::vector<InferRequestItem> &items,
                         long &http_code, std::string &body, std::string &error) {
    static CurlGlobal curl_global;
    CURL *curl = curl_easy_init();
    if (!curl) {
        error = "Failed to initialize libcurl";
        return false;
    }

    curl_mime *mime = curl_mime_init(curl);
    std::ostringstream names_field;
    bool first_name = true;

    for (const auto &item : items) {
        std::ifstream file(item.filepath, std::ios::binary);
        if (!file) {
            error = "Failed to open file: " + item.filepath;
            curl_mime_free(mime);
            curl_easy_cleanup(curl);
            return false;
        }

        std::vector<char> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_name(part, "files");
        curl_mime_filename(part, item.label.c_str());
        curl_mime_data(part, data.data(), data.size());

        if (!first_name)
            names_field << ',';
        first_name = false;
        names_field << item.label;
    }

    curl_mimepart *names_part = curl_mime_addpart(mime);
    curl_mime_name(names_part, "names");
    const auto names_value = names_field.str();
    curl_mime_data(names_part, names_value.c_str(), CURL_ZERO_TERMINATED);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    const CURLcode code = curl_easy_perform(curl);
    curl_mime_free(mime);

    if (code != CURLE_OK) {
        error = curl_easy_strerror(code);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    return true;
}

Prediction parse_prediction(JsonObject *item_obj) {
    Prediction prediction;
    if (!item_obj)
        return prediction;

    if (json_object_has_member(item_obj, "ok") &&
        !json_object_get_boolean_member(item_obj, "ok")) {
        prediction.ok = false;
        if (json_object_has_member(item_obj, "error"))
            prediction.error = json_object_get_string_member(item_obj, "error");
        return prediction;
    }

    if (!json_object_has_member(item_obj, "result"))
        return prediction;

    JsonObject *result = json_object_get_object_member(item_obj, "result");
    if (!result)
        return prediction;

    prediction.ok = true;
    if (json_object_has_member(result, "base")) {
        JsonObject *base = json_object_get_object_member(result, "base");
        if (base) {
            if (json_object_has_member(base, "name"))
                prediction.base = json_object_get_string_member(base, "name");
            if (json_object_has_member(base, "confidence"))
                prediction.base_conf =
                    static_cast<float>(json_object_get_double_member(base, "confidence"));
        }
    }

    if (json_object_has_member(result, "modifiers")) {
        JsonArray *mods = json_object_get_array_member(result, "modifiers");
        const guint len = json_array_get_length(mods);
        for (guint i = 0; i < len; ++i) {
            JsonObject *mod = json_array_get_object_element(mods, i);
            if (!mod)
                continue;
            PredictedModifier entry;
            if (json_object_has_member(mod, "name"))
                entry.name = json_object_get_string_member(mod, "name");
            if (json_object_has_member(mod, "prob"))
                entry.prob = static_cast<float>(json_object_get_double_member(mod, "prob"));
            prediction.modifiers.push_back(std::move(entry));
        }
    }

    if (json_object_has_member(result, "top3_base")) {
        JsonArray *top3 = json_object_get_array_member(result, "top3_base");
        const guint len = json_array_get_length(top3);
        for (guint i = 0; i < len; ++i) {
            JsonObject *candidate = json_array_get_object_element(top3, i);
            if (!candidate)
                continue;
            std::string name;
            float prob = 0.f;
            if (json_object_has_member(candidate, "name"))
                name = json_object_get_string_member(candidate, "name");
            if (json_object_has_member(candidate, "prob"))
                prob = static_cast<float>(json_object_get_double_member(candidate, "prob"));
            prediction.top3_base.emplace_back(name, prob);
        }
    }

    return prediction;
}

bool parse_infer_response(const std::string &body,
                          std::vector<InferResponseItem> &results,
                          std::string &error) {
    GError *gerror = nullptr;
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, body.c_str(),
                                    static_cast<gssize>(body.size()), &gerror)) {
        error = gerror ? gerror->message : "Failed to parse JSON";
        if (gerror)
            g_error_free(gerror);
        g_object_unref(parser);
        return false;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || json_node_get_node_type(root) != JSON_NODE_OBJECT) {
        error = "Invalid JSON root";
        g_object_unref(parser);
        return false;
    }

    JsonObject *root_obj = json_node_get_object(root);
    if (!json_object_has_member(root_obj, "results")) {
        error = "Response has no results field";
        g_object_unref(parser);
        return false;
    }

    JsonArray *items = json_object_get_array_member(root_obj, "results");
    const guint len = json_array_get_length(items);
    for (guint i = 0; i < len; ++i) {
        JsonObject *item_obj = json_array_get_object_element(items, i);
        InferResponseItem item;
        if (item_obj && json_object_has_member(item_obj, "file"))
            item.label = json_object_get_string_member(item_obj, "file");
        item.prediction = parse_prediction(item_obj);
        results.push_back(std::move(item));
    }

    g_object_unref(parser);
    return true;
}

} // namespace

bool check_server_health(const std::string &domain, std::string &error) {
    long http_code = 0;
    std::string body;
    const auto url = join_url(normalize_domain(domain), "/health");
    if (!http_get(url, http_code, body, error))
        return false;
    if (http_code < 200 || http_code >= 300) {
        error = "HTTP " + std::to_string(http_code);
        return false;
    }
    return true;
}

bool infer_icons(const std::string &domain,
                 const std::vector<InferRequestItem> &items,
                 std::vector<InferResponseItem> &results, std::string &error) {
    if (items.empty())
        return true;

    long http_code = 0;
    std::string body;
    const auto url = join_url(normalize_domain(domain), "/infer");
    if (!http_post_multipart(url, items, http_code, body, error))
        return false;
    if (http_code < 200 || http_code >= 300) {
        error = "HTTP " + std::to_string(http_code) + ": " + body;
        return false;
    }
    return parse_infer_response(body, results, error);
}

} // namespace runpod_ico_classifier
