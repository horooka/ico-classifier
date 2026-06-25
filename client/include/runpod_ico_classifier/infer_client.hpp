#pragma once

#include "runpod_ico_classifier/name_match.hpp"

#include <string>
#include <vector>

namespace runpod_ico_classifier {

struct InferRequestItem {
    std::string filepath;
    std::string label;
};

struct InferResponseItem {
    std::string label;
    Prediction prediction;
};

bool check_server_health(const std::string &domain, std::string &error);
bool infer_icons(const std::string &domain,
                 const std::vector<InferRequestItem> &items,
                 std::vector<InferResponseItem> &results,
                 std::string &error);

} // namespace runpod_ico_classifier
