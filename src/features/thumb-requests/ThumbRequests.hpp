#pragma once

// Thumbnail requests filed from the Discord bot. The bot writes them to the
// worker (/api/requests/submit) and the team decides there; this side only
// reads /api/requests/list, so the mod always shows what the staff decided
// rather than a second opinion of its own.
//
// Not to be confused with features/twitch-requests, which is the live viewer
// request room: that one is per-stream and dies with the session.

#include <Geode/DefaultInclude.hpp>
#include <string>

namespace paimon::thumbreq {

constexpr char const* kModuleId = "paimbnails.thumbrequests.social";

enum class Status : int { Pending, Sent, Rejected };

struct Request {
    std::string id;
    int levelId = 0;
    std::string levelName;
    std::string creator;
    std::string mode;         // "classic" o "platformer"
    std::string difficulty;   // la que declaro quien pidio
    std::string description;
    std::string video;
    std::string requester;
    Status status = Status::Pending;
    std::string sentDifficulty;  // la que decidio el equipo, solo si Sent
    int sentTier = 0;            // 0 star rate, 1 featured, 2 epic, 3 legendary, 4 mythic
    std::string decidedBy;
    int64_t createdAt = 0;

    // The difficulty the card should draw: what the team decided once they
    // decided, and what the requester asked for until then.
    std::string const& shownDifficulty() const {
        return status == Status::Sent && !sentDifficulty.empty() ? sentDifficulty : difficulty;
    }
};

// Value GJDifficultySprite expects, from the name the server stores. Same table
// as thumb-alerts and as the bot's DIFF_FILE_MAP — the three have to agree or
// the face is wrong.
int difficultyFace(std::string const& name);

} // namespace paimon::thumbreq
