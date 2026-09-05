#include "VersusSession.hpp"
#include "VersusGlobed.hpp"
#include "VersusEffects.hpp"
#include "VersusNet.hpp"
#include "VersusRng.hpp"
#include "VersusStore.hpp"
#include "../../../core/RuntimeLifecycle.hpp"
#include "../../../core/modules/ModuleRegistry.hpp"
#include "../../../utils/Localization.hpp"
#include "../../../utils/MainThreadDelay.hpp"
#include "../../../utils/PaimonNotification.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace paimon::versus {

namespace {

// Four a second while running, once every two while waiting in the lobby.
constexpr float kTickInterval = 0.25f;
constexpr float kLobbyPoll = 2.0f;
constexpr float kFoundPoll = 1.0f;

// A rival that stops ticking for this long is treated as gone; the server still
// decides what that costs them.
constexpr float kRivalTimeout = 30.f;

int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

VersusSession& VersusSession::get() {
    static VersusSession instance;
    return instance;
}

void VersusSession::addListener(void const* owner, std::function<void()> listener) {
    removeListener(owner);
    m_listeners.emplace_back(owner, std::move(listener));
}

void VersusSession::removeListener(void const* owner) {
    std::erase_if(m_listeners, [owner](auto const& entry) { return entry.first == owner; });
}

void VersusSession::notifyListeners() {
    // A listener can close a popup, which unregisters it mid-walk, so the list
    // is copied before firing.
    auto const snapshot = m_listeners;
    for (auto const& [owner, listener] : snapshot) {
        if (listener) listener();
    }
}

void VersusSession::setPhase(Phase phase) {
    if (m_phase == phase) return;
    m_phase = phase;
    notifyListeners();
}

void VersusSession::reset() {
    m_pollGeneration++;
    m_phase = Phase::Idle;
    m_match = {};
    m_ticket = {};
    m_own = {};
    m_rival = {};
    m_outcome = Outcome::Pending;
    m_eloDelta = 0;
    m_inLevel = false;
    m_submitted = false;
    m_rivalSeen = false;
    m_levelTime = 0.f;
    m_sinceTick = 0.f;
    m_hillHeld = 0.f;
    m_startsIn = 0.f;
    m_rivalSilence = 0.f;
    m_hand.clear();
    m_milestones.clear();
    m_nextMilestone = 0;
    m_milestoneShift = 0.f;
    m_hourglass = 0.f;
    net::setRival(0);
    net::stopListening();
    gl::restoreVisibility();
    gl::clearShield();
}

void VersusSession::beginQueue(Mode mode, Format format) {
    reset();
    m_match.mode = mode;
    m_match.format = format;
    setPhase(Phase::Queued);

    VersusClient::get().joinQueue(mode, format, [this](bool ok, QueueTicket const& ticket) {
        if (!ok) {
            reset();
            notifyListeners();
            return;
        }
        m_ticket = ticket;
        notifyListeners();
        schedulePoll(kFoundPoll);
    });
}

void VersusSession::cancelQueue() {
    if (m_phase != Phase::Queued) return;
    m_pollGeneration++;
    VersusClient::get().leaveQueue([](bool, std::string const&) {});
    reset();
    notifyListeners();
}

void VersusSession::schedulePoll(float delay) {
    uint64_t const generation = ++m_pollGeneration;
    paimon::scheduleMainThreadDelay(delay, [this, generation]() {
        if (paimon::isRuntimeShuttingDown()) return;
        if (generation != m_pollGeneration) return;
        poll();
    });
}

void VersusSession::poll() {
    if (m_phase == Phase::Idle || m_phase == Phase::Finished) return;

    // Once the level is running the fast channel carries everything; polling
    // the lobby there would only spend requests.
    if (m_phase == Phase::Running) {
        schedulePoll(kLobbyPoll * 3.f);
        return;
    }

    VersusClient::get().pollLobby([this](bool ok, MatchInfo const& info) {
        if (!ok || info.id.empty()) {
            schedulePoll(m_phase == Phase::Queued ? kLobbyPoll : kFoundPoll);
            return;
        }
        applyLobby(info);
        schedulePoll(m_phase == Phase::Queued ? kLobbyPoll : kFoundPoll);
    });
}

void VersusSession::applyLobby(MatchInfo const& info) {
    bool const isNew = m_match.id != info.id;

    // The server closed it without us: a dodge, a void, or a rival that walked
    // before the level opened. Nothing to submit, just drop back to the hub.
    if (info.serverPhase == "done" && m_phase != Phase::Finished) {
        reset();
        notifyListeners();
        return;
    }

    m_match = info;

    if (isNew) {
        net::setRival(info.rival.accountId);
        wireNet();
        setPhase(Phase::Found);
        PaimonNotify::show(Localization::get().getString("versus.match-found").c_str(),
                           NotificationIcon::Info);
        return;
    }

    if (info.serverPhase == "banning" && m_phase == Phase::Found) {
        setPhase(Phase::Banning);
        return;
    }
    if (info.levelId != 0 && (m_phase == Phase::Banning || m_phase == Phase::Found)) {
        setPhase(Phase::Loading);
        return;
    }
    if (info.countdownMs > 0 && m_phase != Phase::Running) {
        m_startsIn = info.countdownMs / 1000.f;
        setPhase(Phase::Countdown);
    }
    notifyListeners();
}

void VersusSession::accept(bool yes) {
    if (m_match.id.empty()) return;

    VersusClient::get().acceptMatch(m_match.id, yes, [this, yes](bool ok, std::string const& message) {
        if (!yes || !ok) {
            if (!message.empty()) PaimonNotify::show(message.c_str(), NotificationIcon::Warning);
            reset();
            notifyListeners();
            return;
        }
        setPhase(Phase::Banning);
    });
}

void VersusSession::ban(int levelId) {
    if (m_match.id.empty()) return;

    VersusClient::get().banLevel(m_match.id, levelId, [this](bool ok, MatchInfo const& info) {
        if (!ok) return;
        applyLobby(info);
    });
}

bool VersusSession::enterLevel() {
    if (m_match.levelId == 0) return false;

    auto* glm = GameLevelManager::get();
    if (!glm) return false;

    // The starting pool is main levels, so nothing has to be downloaded before
    // the countdown. Online levels come with the curated rotation.
    auto* level = glm->getMainLevel(m_match.levelId, false);
    if (!level) level = glm->getSavedLevel(m_match.levelId);
    if (!level) {
        PaimonNotify::show(Localization::get().getString("versus.level-missing").c_str(),
                           NotificationIcon::Error);
        return false;
    }

    VersusClient::get().reportReady(m_match.id, [this](bool ok, MatchInfo const& info) {
        if (ok) applyLobby(info);
    });

    auto* scene = PlayLayer::scene(level, false, false);
    if (!scene) return false;
    CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, scene));
    return true;
}

void VersusSession::wireNet() {
    net::Handlers handlers;

    handlers.onTick = [this](int, net::Tick const& tick) {
        m_rivalSeen = true;
        m_rivalSilence = 0.f;
        m_rival.percent = tick.percent;
        m_rival.bestPercent = std::max(m_rival.bestPercent, tick.percent);
        m_rival.attempt = tick.attempt;
        m_rival.alive = tick.alive;
        m_rival.shielded = tick.shielded;
        notifyListeners();
    };

    handlers.onState = [this](int, net::StateMsg const& state) {
        switch (state.kind) {
            case net::StateKind::Death:
                m_rival.alive = false;
                m_rival.deaths = state.detail;
                break;
            case net::StateKind::Segment:
                m_rival.segments |= static_cast<uint8_t>(1u << std::min<uint8_t>(state.value, 3));
                break;
            case net::StateKind::Finish:
                m_rival.finished = true;
                m_rival.percent = 100.f;
                m_rival.finishTime = state.levelTime;
                break;
            case net::StateKind::Forfeit:
                m_rival.finished = true;
                m_rival.alive = false;
                m_rival.percent = 0.f;
                break;
            default:
                break;
        }
        evaluate();
        notifyListeners();
    };

    handlers.onCard = [this](int, net::CardMsg const& msg) {
        receiveCard(msg.card, msg.reflected);
        notifyListeners();
    };

    handlers.onTaunt = [](int, uint8_t) {};

    net::listen(std::move(handlers));
}

void VersusSession::onLevelStarted(PlayLayer* layer) {
    m_inLevel = true;
    VersusEffects::get().attach(layer);
    m_levelTime = 0.f;
    m_sinceTick = 0.f;
    m_hillHeld = 0.f;
    m_rivalSilence = 0.f;
    m_own = {};
    m_submitted = false;
    m_hand.clear();
    m_nextMilestone = 0;
    m_milestoneShift = 0.f;
    m_hourglass = 0.f;
    buildMilestones();

    if (m_match.rival.accountId != 0) {
        // A duel in the global room still has to look like a duel.
        gl::isolateRival(m_match.rival.accountId);
    }
    setPhase(m_startsIn > 0.f ? Phase::Countdown : Phase::Running);
}

void VersusSession::onLevelTick(float dt, float percent, int attempt, bool practice) {
    if (!m_inLevel) return;

    if (m_phase == Phase::Countdown) {
        m_startsIn -= dt;
        if (m_startsIn <= 0.f) {
            m_startsIn = 0.f;
            setPhase(Phase::Running);
        }
        return;
    }
    if (m_phase != Phase::Running) return;

    VersusEffects::get().update(dt);

    m_levelTime += dt;
    m_own.percent = percent;
    m_own.bestPercent = std::max(m_own.bestPercent, percent);
    m_own.attempt = attempt;
    m_own.alive = true;

    if (m_rivalSeen) {
        m_rivalSilence += dt;
        if (m_rivalSilence > kRivalTimeout && !m_rival.finished) {
            m_rival.alive = false;
        }
    }

    // Practice runs never count toward a duel, but leaving practice should not
    // end the match either: the tick just stops carrying progress.
    if (practice) return;

    auto const& def = format();
    if (def.id == Format::Ladder) {
        int const segment = segmentForPercent(m_own.percent);
        if (segment >= 0) {
            uint8_t const bit = static_cast<uint8_t>(1u << segment);
            if (!(m_own.segments & bit) && !(m_rival.segments & bit)) {
                m_own.segments |= bit;
                net::sendState({net::StateKind::Segment, static_cast<uint8_t>(segment), 0, m_levelTime});
            }
        }
    } else if (def.id == Format::KingOfTheHill) {
        m_hillHeld = m_own.percent > m_rival.percent ? m_hillHeld + dt : 0.f;
    }

    if (m_hourglass > 0.f) m_hourglass = std::max(0.f, m_hourglass - dt);
    checkMilestones();

    m_sinceTick += dt;
    if (m_sinceTick >= kTickInterval) pushTick(false);

    evaluate();
}

bool VersusSession::hasMutator(std::string const& id) const {
    return std::find(m_match.mutators.begin(), m_match.mutators.end(), id)
        != m_match.mutators.end();
}

bool VersusSession::dealsCards() const {
    return format().cards && paimon::modules::isEnabled("paimbnails.versus.cards");
}

void VersusSession::buildMilestones() {
    m_milestones.clear();
    if (!dealsCards() || m_match.seed == 0) return;
    m_milestones = rollMilestones(m_match.seed);
}

void VersusSession::checkMilestones() {
    if (m_milestones.empty() || m_nextMilestone >= m_milestones.size()) return;

    // Spark pulls the next one closer, the Hourglass halves what is left to
    // every one of them; both only ever move our own copy of the list.
    float threshold = m_milestones[m_nextMilestone] - m_milestoneShift;
    if (m_hourglass > 0.f) {
        threshold = m_own.percent + (threshold - m_own.percent) * 0.5f;
    }
    if (m_own.percent < threshold) return;

    m_nextMilestone++;
    m_milestoneShift = 0.f;
    drawCard();
}

void VersusSession::drawCard() {
    uint8_t const modeMask = m_match.mode == Mode::Platformer ? ModePlatformer : ModeClassic;
    float const deficit = std::max(0.f, m_rival.percent - m_own.percent);

    auto const card = rollCard(m_match.seed, static_cast<int>(m_nextMilestone),
                               modeMask, deficit, m_match.catchUp);

    m_hand.push_back(card);
    // A third card pushes the oldest out, which is what makes holding a
    // legendary through a hard section an actual decision.
    if (static_cast<int>(m_hand.size()) > kHandSize) m_hand.erase(m_hand.begin());
    notifyListeners();
}

bool VersusSession::playCard(int slot) {
    if (m_phase != Phase::Running || !m_inLevel) return false;
    if (slot < 0 || slot >= static_cast<int>(m_hand.size())) return false;
    if (VersusEffects::get().cardsLocked()) return false;

    auto const card = m_hand[slot];
    m_hand.erase(m_hand.begin() + slot);

    auto const& def = cardAt(card);
    switch (def.target) {
        case CardTarget::Self:
            VersusEffects::get().apply(card, false);
            break;
        case CardTarget::Rival:
            break;
        case CardTarget::Both:
            VersusEffects::get().apply(card, false);
            if (card == CardId::Hourglass) m_hourglass = def.duration;
            if (card == CardId::Dice) drawCard();
            if (card == CardId::Swap) m_hand.clear();
            break;
    }
    if (card == CardId::Bolt) m_milestoneShift = 4.f;

    net::sendCard({card, static_cast<uint8_t>(m_nextMilestone), false, m_levelTime});
    notifyListeners();
    return true;
}

void VersusSession::receiveCard(CardId card, bool alreadyReflected) {
    auto const& def = cardAt(card);

    if (def.target == CardTarget::Rival) {
        // Rebound sends it straight back instead of eating it, and a card that
        // has already bounced once cannot bounce again.
        if (!alreadyReflected && VersusEffects::get().consumeReflect()) {
            net::sendCard({card, 0, true, m_levelTime});
            return;
        }
        VersusEffects::get().apply(card, true);
        return;
    }

    if (def.target == CardTarget::Both) {
        if (card == CardId::Hourglass) m_hourglass = def.duration;
        if (card == CardId::Dice) drawCard();
        if (card == CardId::Swap) m_hand.clear();
        VersusEffects::get().apply(card, true);
        return;
    }

    // Wraith is cast on themselves but acted on here: we are the ones who have
    // to stop drawing them.
    if (card == CardId::Ghost) VersusEffects::get().apply(card, true);
}

void VersusSession::pushTick(bool force) {
    if (!force && m_sinceTick < kTickInterval) return;
    m_sinceTick = 0.f;

    net::Tick tick;
    tick.percent = m_own.percent;
    tick.levelTime = m_levelTime;
    tick.attempt = m_own.attempt;
    tick.alive = m_own.alive;
    tick.shielded = gl::shieldActive();
    net::sendTick(tick);
}

void VersusSession::onDeath() {
    if (!m_inLevel || m_phase != Phase::Running) return;

    m_own.deaths++;
    m_own.alive = false;
    net::sendState({net::StateKind::Death, 0, static_cast<uint16_t>(m_own.deaths), m_levelTime});

    auto const& def = format();
    if (def.attemptLimit > 0 && m_own.attempt >= def.attemptLimit) {
        evaluate();
        return;
    }
    if (def.id == Format::SuddenDeath) {
        finish(m_rival.alive ? Outcome::Loss : Outcome::Draw);
    }
}

void VersusSession::onComplete() {
    if (!m_inLevel) return;

    m_own.finished = true;
    m_own.percent = 100.f;
    m_own.bestPercent = 100.f;
    m_own.finishTime = m_levelTime;
    net::sendState({net::StateKind::Finish, 0, 0, m_levelTime});
    evaluate();
}

void VersusSession::evaluate() {
    if (m_phase != Phase::Running || m_submitted) return;

    auto const& def = format();

    if (m_own.finished && !m_rival.finished) {
        finish(Outcome::Win);
        return;
    }
    if (m_rival.finished && !m_own.finished) {
        // Time attack is the one format where finishing second can still win.
        if (def.id != Format::TimeAttack) finish(Outcome::Loss);
        return;
    }
    if (m_own.finished && m_rival.finished) {
        if (def.id == Format::TimeAttack || def.id == Format::Race) {
            float const gap = m_rival.finishTime - m_own.finishTime;
            if (std::fabs(gap) < 0.05f) finish(Outcome::Draw);
            else finish(gap > 0.f ? Outcome::Win : Outcome::Loss);
        } else {
            finish(Outcome::Draw);
        }
        return;
    }

    if (def.id == Format::Ladder) {
        int own = 0, rival = 0;
        for (int i = 0; i < kLadderSegments; i++) {
            if (m_own.segments & (1u << i)) own++;
            if (m_rival.segments & (1u << i)) rival++;
        }
        if (own >= kLadderToWin) finish(Outcome::Win);
        else if (rival >= kLadderToWin) finish(Outcome::Loss);
        return;
    }

    if (def.id == Format::KingOfTheHill && m_hillHeld >= kHillSeconds) {
        finish(Outcome::Win);
        return;
    }

    if (def.id == Format::SuddenDeath && !m_rival.alive && m_own.alive) {
        finish(Outcome::Win);
    }
}

void VersusSession::finish(Outcome outcome) {
    if (m_submitted) return;
    m_submitted = true;
    m_outcome = outcome;
    setPhase(Phase::Finished);

    int const before = VersusStore::get().profile(m_match.mode).elo;

    VersusClient::get().submitResult(m_match.id, m_own, m_rival, outcome,
        [this, before](bool ok, std::string const& message) {
            if (!ok) {
                log::warn("[Versus][Session] Result rejected: {}", message);
                return;
            }
            m_eloDelta = VersusStore::get().profile(m_match.mode).elo - before;

            MatchRecord record;
            record.id = m_match.id;
            record.rival = m_match.rival.name;
            record.levelId = m_match.levelId;
            record.mode = m_match.mode;
            record.format = m_match.format;
            record.outcome = m_outcome;
            record.eloDelta = m_eloDelta;
            record.ownPercent = m_own.bestPercent;
            record.rivalPercent = m_rival.bestPercent;
            record.playedAt = nowSeconds();
            VersusStore::get().pushRecord(record);

            notifyListeners();
        });
}

void VersusSession::forfeit() {
    if (m_match.id.empty() || m_submitted) return;

    net::sendState({net::StateKind::Forfeit, 0, 0, m_levelTime});
    m_submitted = true;
    m_outcome = Outcome::Loss;
    setPhase(Phase::Finished);

    VersusClient::get().forfeit(m_match.id, [this](bool, std::string const&) {
        notifyListeners();
    });
}

void VersusSession::onLevelLeft() {
    if (!m_inLevel) return;
    m_inLevel = false;

    VersusEffects::get().detach();
    gl::restoreVisibility();
    gl::clearShield();

    // Walking out of a running duel is a forfeit; the server would rule it one
    // anyway once the rival submits.
    if (m_phase == Phase::Running && !m_submitted) forfeit();
}

float VersusSession::countdownLeft() const {
    return std::max(0.f, m_startsIn);
}

std::string VersusSession::statusLine() const {
    auto& loc = Localization::get();
    switch (m_phase) {
        case Phase::Idle:      return {};
        case Phase::Queued:    return loc.getString("versus.status.queued");
        case Phase::Found:     return loc.getString("versus.status.found");
        case Phase::Banning:   return loc.getString("versus.status.banning");
        case Phase::Loading:   return loc.getString("versus.status.loading");
        case Phase::Countdown: return loc.getString("versus.status.countdown");
        case Phase::Running:   return loc.getString("versus.status.running");
        case Phase::Finished:  return loc.getString("versus.status.finished");
    }
    return {};
}

} // namespace paimon::versus
