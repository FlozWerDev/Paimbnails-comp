#pragma once

// REST against paimon-versus. Everything slow, durable or contested lives on
// the server: the queue, Elo, history, seasons and the level pool. The fast
// channel inside a level is VersusNet.

#include "../data/VersusTypes.hpp"
#include "VersusStore.hpp"

#include <Geode/Geode.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace paimon::versus {

struct LevelOffer {
    int levelId = 0;
    std::string name;
    std::string author;
    int difficulty = 0;
    int length = 0;
    bool banned = false;
};

struct SeasonInfo {
    int number = 0;
    int daysLeft = 0;
    std::vector<std::string> mutators;
};

struct MatchInfo {
    std::string id;
    // Straight from the server, so the client can react to a match that ended
    // without it: a dodge, a void, a rival that walked.
    std::string serverPhase;
    PlayerRef rival;
    Mode mode = Mode::Classic;
    Format format = Format::Race;
    int levelId = 0;
    uint64_t seed = 0;
    int countdownMs = 0;
    bool catchUp = true;
    std::vector<std::string> mutators;
    std::vector<LevelOffer> offers;
};

struct QueueTicket {
    std::string id;
    int waiting = 0;
    int estimateSeconds = 0;
};

struct LeaderboardRow {
    int rank = 0;
    int accountId = 0;
    std::string name;
    int elo = 0;
    int wins = 0;
    int losses = 0;
};

class VersusClient {
public:
    using OkCallback     = geode::CopyableFunction<void(bool ok, std::string const& message)>;
    using AuthCallback   = geode::CopyableFunction<void(bool ok, std::string const& message)>;
    using QueueCallback  = geode::CopyableFunction<void(bool ok, QueueTicket const& ticket)>;
    using MatchCallback  = geode::CopyableFunction<void(bool ok, MatchInfo const& match)>;
    using BoardCallback  = geode::CopyableFunction<void(bool ok, std::vector<LeaderboardRow> const& rows)>;
    using PoolCallback   = geode::CopyableFunction<void(bool ok, std::vector<LevelOffer> const& levels)>;
    // Someone else's profile never touches the local store: that cache is for
    // the player's own rank and overwriting it from a profile visit would show
    // them a stranger's ladder.
    using ProfileCallback = geode::CopyableFunction<void(bool ok, ModeProfile const& classic,
                                                         ModeProfile const& platformer)>;

    static VersusClient& get();

    std::string baseUrl() const;
    bool authenticated() const;
    SeasonInfo const& season() const { return m_season; }

    // Trades the mod-code for a session token and fills both mode profiles.
    void authenticate(AuthCallback cb);

    void joinQueue(Mode mode, Format format, QueueCallback cb);
    void leaveQueue(OkCallback cb);

    // One poll of the lobby channel. The server answers immediately with the
    // current phase, so a dropped connection costs one tick, not the match.
    void pollLobby(MatchCallback cb);

    void acceptMatch(std::string const& matchId, bool accept, OkCallback cb);
    void banLevel(std::string const& matchId, int levelId, MatchCallback cb);
    void reportReady(std::string const& matchId, MatchCallback cb);
    void submitResult(std::string const& matchId, SideState const& own,
                      SideState const& rival, Outcome outcome, OkCallback cb);
    void forfeit(std::string const& matchId, OkCallback cb);

    void challenge(std::string const& username, Mode mode, Format format, MatchCallback cb);

    void fetchProfile(int accountId, ProfileCallback cb);
    void fetchLeaderboard(Mode mode, std::string const& scope, BoardCallback cb);
    void fetchPool(Mode mode, PoolCallback cb);
    void reportPlayer(std::string const& matchId, std::string const& note, OkCallback cb);

private:
    VersusClient() = default;

    struct ProfileCacheEntry {
        ModeProfile classic;
        ModeProfile platformer;
        int64_t fetchedAt = 0;
    };

    // Both the versus chip and the progression chip want the same numbers when
    // a profile opens; without this every visit costs two identical requests.
    static constexpr int64_t kProfileTtlSeconds = 60;
    std::unordered_map<int, ProfileCacheEntry> m_profileCache;
    std::unordered_map<int, std::vector<ProfileCallback>> m_profileWaiters;

    void send(std::string const& method, std::string const& path,
              matjson::Value const& body,
              geode::CopyableFunction<void(bool ok, matjson::Value const& json,
                                           std::string const& message)> cb);

    static MatchInfo parseMatch(matjson::Value const& v);

    std::string m_token;
    SeasonInfo m_season;
    bool m_authenticated = false;
};

} // namespace paimon::versus
