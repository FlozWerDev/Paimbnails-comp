#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace paimon::editorphysics {

// Audited against GeometryDash.bro 2.2081 and GameObject's native
// isEditorSpawnableTrigger list. The catalog intentionally contains the native
// objects useful to a physics compiler, rather than pretending the .bro has
// display names for every editor button.
enum class TriggerCapability : std::uint32_t {
    None = 0,
    Motion = 1u << 0,
    Collision = 1u << 1,
    State = 1u << 2,
    Timing = 1u << 3,
    Dispatch = 1u << 4,
    Player = 1u << 5,
};

constexpr TriggerCapability operator|(TriggerCapability lhs, TriggerCapability rhs) {
    return static_cast<TriggerCapability>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs)
    );
}

struct NativeTriggerInfo {
    int objectID;
    std::string_view name;
    TriggerCapability capabilities;
    bool isTrigger;
};

namespace nativeids {
constexpr int Move = 901;
constexpr int Toggle = 1049;
constexpr int Spawn = 1268;
constexpr int Rotate = 1346;
constexpr int Follow = 1347;
constexpr int Touch = 1595;
constexpr int Count = 1611;
constexpr int Stop = 1616;
constexpr int InstantCount = 1811;
constexpr int Collision = 1815;
constexpr int CollisionBlock = 1816;
constexpr int Pickup = 1817;
constexpr int AdvancedFollow = 3016;
constexpr int KeyframePoint = 3032;
constexpr int KeyframeAnimation = 3033;
constexpr int Event = 3604;
constexpr int Sequence = 3607;
constexpr int InstantCollision = 3609;
constexpr int Time = 3614;
constexpr int TimeEvent = 3615;
constexpr int TimeControl = 3617;
constexpr int Reset = 3618;
constexpr int ItemEdit = 3619;
constexpr int ItemCompare = 3620;
constexpr int CollisionStateBlock = 3640;
constexpr int PersistentItem = 3641;
constexpr int ObjectControl = 3655;
constexpr int AdvancedFollowEdit = 3660;
constexpr int AdvancedFollowRetarget = 3661;
constexpr int LinkVisible = 3662;
} // namespace nativeids

inline constexpr std::array<NativeTriggerInfo, 30> kNativePhysicsCatalog{{
    {nativeids::Move, "Move", TriggerCapability::Motion, true},
    {nativeids::Toggle, "Toggle", TriggerCapability::State, true},
    {nativeids::Spawn, "Spawn", TriggerCapability::Dispatch | TriggerCapability::Timing, true},
    {nativeids::Rotate, "Rotate", TriggerCapability::Motion, true},
    {nativeids::Follow, "Follow", TriggerCapability::Motion, true},
    {nativeids::Touch, "Touch", TriggerCapability::Player | TriggerCapability::Dispatch, true},
    {nativeids::Count, "Count", TriggerCapability::State | TriggerCapability::Dispatch, true},
    {nativeids::Stop, "Stop", TriggerCapability::State | TriggerCapability::Dispatch, true},
    {nativeids::InstantCount, "Instant Count", TriggerCapability::State | TriggerCapability::Dispatch, true},
    {nativeids::Collision, "Collision", TriggerCapability::Collision | TriggerCapability::Dispatch, true},
    {nativeids::CollisionBlock, "Collision Block", TriggerCapability::Collision, false},
    {nativeids::Pickup, "Pickup", TriggerCapability::State, true},
    {nativeids::AdvancedFollow, "Advanced Follow", TriggerCapability::Motion, true},
    {nativeids::KeyframePoint, "Keyframe Point", TriggerCapability::Motion, false},
    {nativeids::KeyframeAnimation, "Keyframe Animation", TriggerCapability::Motion, true},
    {nativeids::Event, "Event", TriggerCapability::Player | TriggerCapability::Dispatch, true},
    {nativeids::Sequence, "Sequence", TriggerCapability::Dispatch, true},
    {nativeids::InstantCollision, "Instant Collision", TriggerCapability::Collision | TriggerCapability::Dispatch, true},
    {nativeids::Time, "Time", TriggerCapability::Timing | TriggerCapability::Dispatch, true},
    {nativeids::TimeEvent, "Time Event", TriggerCapability::Timing | TriggerCapability::Dispatch, true},
    {nativeids::TimeControl, "Time Control", TriggerCapability::Timing | TriggerCapability::State, true},
    {nativeids::Reset, "Reset", TriggerCapability::State, true},
    {nativeids::ItemEdit, "Item Edit", TriggerCapability::State, true},
    {nativeids::ItemCompare, "Item Compare", TriggerCapability::State | TriggerCapability::Dispatch, true},
    {nativeids::CollisionStateBlock, "Collision State Block", TriggerCapability::Collision | TriggerCapability::State, false},
    {nativeids::PersistentItem, "Persistent Item", TriggerCapability::State, true},
    {nativeids::ObjectControl, "Object Control", TriggerCapability::State | TriggerCapability::Dispatch, true},
    {nativeids::AdvancedFollowEdit, "Edit Advanced Follow", TriggerCapability::Motion | TriggerCapability::Dispatch, true},
    {nativeids::AdvancedFollowRetarget, "Re-Target Advanced Follow", TriggerCapability::Motion | TriggerCapability::Dispatch, true},
    {nativeids::LinkVisible, "Link Visible", TriggerCapability::State, true},
}};

constexpr NativeTriggerInfo const* nativeTriggerInfo(int objectID) {
    for (auto const& info : kNativePhysicsCatalog) {
        if (info.objectID == objectID) return &info;
    }
    return nullptr;
}

} // namespace paimon::editorphysics
