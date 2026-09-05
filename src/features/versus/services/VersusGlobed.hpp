#pragma once

// Everything the duel needs from Globed, behind one façade.
//
// Globed is an optional dependency and its soft-link API is the only one with
// an ABI guarantee, so nothing here links against the mod: if the headers are
// missing at build time or the mod is missing at runtime, every call turns into
// a no-op and the duel falls back to server-relayed progress.

#include <cstdint>
#include <string>
#include <vector>

namespace paimon::versus::gl {

// Headers were available at build time.
bool compiled();
// Globed is installed, enabled, and its API table answered.
bool present();
bool connected();
// In a level with an active session, which is what the fast channel needs.
bool inSession();

uint32_t pingMs();
std::vector<int> sessionPlayers();
bool rivalInSession(int accountId);
std::string rivalName(int accountId);

// Hide everyone in the level except the rival, so a duel in the global room
// still looks like a duel. Undone on level exit.
void isolateRival(int accountId);
void restoreVisibility();
// The Wraith card: the caster asks us to stop drawing them for a few seconds.
void setRivalHidden(bool hidden);

// The Shield card. Survives the next death without desyncing the session.
void grantShield();
bool shieldActive();
void clearShield();

void killSelf(bool fake);
void respawn(bool fullReset);
void cancelRespawn();
void taunt(uint32_t emoteId);

// Room state, read only: the soft-link API cannot create or join one.
bool inRoom();
uint32_t roomId();
int pinnedLevel();

} // namespace paimon::versus::gl
