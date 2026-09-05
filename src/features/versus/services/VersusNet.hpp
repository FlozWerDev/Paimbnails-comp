#pragma once

// The fast channel of a duel, on top of Globed server events.
//
// Four events, all binary, all scoped to the rival with targetPlayers instead
// of the whole session. At the peak that is around 60 bytes a second each way,
// which is less than one player moving, and it keeps us inside the fair use
// Globed asks for.

#include "../data/VersusTypes.hpp"

#include <cstdint>
#include <functional>

namespace paimon::versus::net {

struct Tick {
    float percent = 0.f;
    float levelTime = 0.f;
    int attempt = 1;
    bool alive = true;
    bool practice = false;
    bool shielded = false;
};

enum class StateKind : uint8_t {
    Ready,
    Countdown,
    Death,
    Segment,
    Finish,
    Forfeit,
    Rematch,
};

struct StateMsg {
    StateKind kind = StateKind::Ready;
    uint8_t value = 0;
    uint16_t detail = 0;
    float levelTime = 0.f;
};

struct CardMsg {
    CardId card = CardId::Fog;
    uint8_t milestone = 0;
    bool reflected = false;
    float levelTime = 0.f;
};

using TickHandler  = std::function<void(int from, Tick const&)>;
using CardHandler  = std::function<void(int from, CardMsg const&)>;
using StateHandler = std::function<void(int from, StateMsg const&)>;
using TauntHandler = std::function<void(int from, uint8_t emote)>;

struct Handlers {
    TickHandler onTick;
    CardHandler onCard;
    StateHandler onState;
    TauntHandler onTaunt;
};

// Called once on load. Registration itself waits for Globed internally.
void registerEvents();

// Only events from this account are forwarded, and everything we send goes only
// to them. Zero tears the duel down.
void setRival(int accountId);
int rival();

void listen(Handlers handlers);
void stopListening();

void sendTick(Tick const& tick);
void sendCard(CardMsg const& card);
void sendState(StateMsg const& state);
void sendTaunt(uint8_t emote);

} // namespace paimon::versus::net
