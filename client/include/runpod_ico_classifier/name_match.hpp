#pragma once

#include <string>
#include <vector>

namespace runpod_ico_classifier {

enum class MatchVerdict { Pending, Ok, Warn, Fail };

struct ExpectedName {
    std::string base;
    std::vector<std::string> mods;
};

struct PredictedModifier {
    std::string name;
    float prob = 0.f;
};

struct Prediction {
    bool ok = false;
    std::string error;
    std::string base;
    float base_conf = 0.f;
    std::vector<PredictedModifier> modifiers;
    std::vector<std::pair<std::string, float>> top3_base;
};

struct MatchResult {
    MatchVerdict verdict = MatchVerdict::Pending;
    std::string predicted_label;
    std::string confidence_text;
    std::string details;
    std::vector<std::pair<std::string, float>> top3_base;
};

ExpectedName parse_expected_from_filename(const std::string &filename);
std::string format_expected_label(const ExpectedName &expected);
std::string format_predicted_label(const Prediction &prediction);
MatchResult compare_expected_vs_prediction(const ExpectedName &expected,
                                           const Prediction &prediction);

} // namespace runpod_ico_classifier
