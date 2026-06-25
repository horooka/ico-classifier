#include "runpod_ico_classifier/name_match.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace runpod_ico_classifier {
namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string strip_extension(const std::string &filename) {
    const auto dot = filename.find_last_of('.');
    if (dot == std::string::npos)
        return filename;
    return filename.substr(0, dot);
}

std::vector<std::string> split_dash(const std::string &stem) {
    std::vector<std::string> parts;
    std::stringstream stream(stem);
    std::string part;
    while (std::getline(stream, part, '-')) {
        part = to_lower(part);
        if (!part.empty())
            parts.push_back(part);
    }
    return parts;
}

std::vector<std::string> modifier_names(const Prediction &prediction) {
    std::vector<std::string> names;
    names.reserve(prediction.modifiers.size());
    for (const auto &mod : prediction.modifiers)
        names.push_back(to_lower(mod.name));
    std::sort(names.begin(), names.end());
    return names;
}

bool base_in_top3(const ExpectedName &expected, const Prediction &prediction) {
    for (const auto &candidate : prediction.top3_base) {
        if (to_lower(candidate.first) == expected.base)
            return true;
    }
    return false;
}

} // namespace

ExpectedName parse_expected_from_filename(const std::string &filename) {
    ExpectedName expected;
    const auto parts = split_dash(strip_extension(filename));
    if (parts.empty())
        return expected;
    expected.base = parts.front();
    if (parts.size() > 1)
        expected.mods.assign(parts.begin() + 1, parts.end());
    std::sort(expected.mods.begin(), expected.mods.end());
    return expected;
}

std::string format_expected_label(const ExpectedName &expected) {
    if (expected.base.empty())
        return {};
    if (expected.mods.empty())
        return expected.base;
    std::ostringstream out;
    out << expected.base;
    for (const auto &mod : expected.mods)
        out << " + " << mod;
    return out.str();
}

std::string format_predicted_label(const Prediction &prediction) {
    if (!prediction.ok)
        return "—";
    std::ostringstream out;
    out << prediction.base;
    for (const auto &mod : prediction.modifiers)
        out << " + " << mod.name;
    return out.str();
}

MatchResult compare_expected_vs_prediction(const ExpectedName &expected,
                                           const Prediction &prediction) {
    MatchResult result;
    result.predicted_label = format_predicted_label(prediction);

    if (!prediction.ok) {
        result.verdict = MatchVerdict::Fail;
        result.confidence_text = "—";
        result.details = prediction.error.empty() ? "Inference error" : prediction.error;
        return result;
    }

    result.top3_base = prediction.top3_base;

    std::ostringstream conf;
    conf.setf(std::ios::fixed);
    conf.precision(1);
    conf << (prediction.base_conf * 100.f) << '%';
    result.confidence_text = conf.str();

    const auto predicted_mods = modifier_names(prediction);
    const bool base_match = expected.base == to_lower(prediction.base);
    const bool mods_match = expected.mods == predicted_mods;

    if (base_match && mods_match) {
        result.verdict =
            prediction.base_conf < 0.7f ? MatchVerdict::Warn : MatchVerdict::Ok;
        result.details = result.verdict == MatchVerdict::Ok
                             ? "Filename matches prediction"
                             : "Match found but confidence is below 70%";
        return result;
    }

    if (base_match && !mods_match) {
        result.verdict = MatchVerdict::Warn;
        result.details = "Base icon matches but modifiers differ";
        return result;
    }

    if (!base_match && base_in_top3(expected, prediction)) {
        result.verdict = MatchVerdict::Warn;
        result.details = "Expected base is in top-3 but not ranked first";
        return result;
    }

    result.verdict = MatchVerdict::Fail;
    result.details = "Filename does not match the image";
    return result;
}

} // namespace runpod_ico_classifier
