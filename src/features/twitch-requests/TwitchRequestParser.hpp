#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace paimon::twitch {

struct ParsedRequest {
    int levelID = 0;
    std::string command;
    std::string url;  // video que venia en el mensaje, si habia alguno
};

std::vector<std::string> parseCommands(std::string_view configured);
std::optional<ParsedRequest> parseRequest(
    std::string_view message,
    std::string_view configuredCommands
);

// A video field only counts when it is a bounded HTTP(S) URL without spaces.
// The host is intentionally unrestricted so links from any video provider work.
bool isValidVideoUrl(std::string_view url);
bool isYouTubeUrl(std::string_view url);

} // namespace paimon::twitch
