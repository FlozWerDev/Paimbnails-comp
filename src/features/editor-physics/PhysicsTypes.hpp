#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace paimon::editorphysics {

// Four corners cover a rotated block and three a slope, but a silhouette hull
// keeps the diagonal faces it traced, so it needs the extra room.
constexpr int kMaxVertices = 8;

struct Vec2 {
    float x = 0.f;
    float y = 0.f;
};

// A fixture is a box unless it says otherwise: `radius` turns it into a circle
// (orbs, rings, coins) and `vertices` into a convex polygon (slopes, silhouette
// hulls, and any object the editor rotated off the axes). `halfSize` always
// holds the local bounding half extents, because mass, inertia and substepping
// only need that. Polygon vertices are relative to `offset` and wound
// counter-clockwise, which is what makes the solver's edge normals point
// outwards. Negative friction or restitution means the body's own value wins,
// so an untouched fixture behaves exactly like the body it belongs to.
struct Fixture {
    Vec2 offset;
    Vec2 halfSize;
    float radius = 0.f;
    int vertexCount = 0;
    Vec2 vertices[kMaxVertices]{};
    float friction = -1.f;
    float restitution = -1.f;
    // Conveyor speed along the contact tangent: friction drags whatever touches
    // the face towards this speed instead of towards a standstill.
    float surfaceVelocity = 0.f;
    // A sensor reports the overlap and lets the other body pass through.
    bool sensor = false;
};

enum class Motion {
    Dynamic,
    Static,
    // Moves exactly where its velocity says and pushes everyone else without
    // ever being pushed back: lifts, saws and moving platforms.
    Kinematic,
};

constexpr std::uint32_t kDefaultCategory = 0x0001u;
constexpr std::uint32_t kAllCategories = 0xFFFFFFFFu;

// Two bodies only touch when each one's category is in the other's mask, so a
// one-sided filter never silently drops half a contact.
struct BodySpec {
    Motion motion = Motion::Static;
    Vec2 position;
    Vec2 velocity;
    float angle = 0.f;
    float angularVelocity = 0.f;
    float mass = 1.f;
    float gravityScale = 1.f;
    float restitution = 0.35f;
    float friction = 0.5f;
    // Negative hands the decision back to the world, so a body that was never
    // touched damps like every other one.
    float linearDamping = -1.f;
    float angularDamping = -1.f;
    // Zero leaves the speed uncapped.
    float maxSpeed = 0.f;
    float maxAngularSpeed = 0.f;
    bool fixedRotation = false;
    bool allowSleep = true;
    std::uint32_t category = kDefaultCategory;
    std::uint32_t mask = kAllCategories;
    std::vector<Fixture> fixtures;
};

constexpr std::size_t kWorldBody = static_cast<std::size_t>(-1);

enum class JointKind {
    Pin,    // both anchors are held on the same point
    Rope,   // the anchors may come closer but never stretch past `length`
    Spring, // soft pull back to `length`
    Weld,   // pin plus a locked relative angle
    Motor,  // drives the relative angle at `motorSpeed`
};

// `bodyB` may be `kWorldBody`, and then `anchorB` is read as a world point: that
// is the nail a pendulum hangs from. A negative `length` is measured from the
// pose the bodies start in, which is what makes anchoring one tap of work.
struct Joint {
    JointKind kind = JointKind::Pin;
    std::size_t bodyA = 0;
    std::size_t bodyB = kWorldBody;
    Vec2 anchorA;
    Vec2 anchorB;
    float length = -1.f;
    float stiffness = 0.5f;
    float damping = 0.2f;
    float motorSpeed = 0.f;
    float maxMotorTorque = 0.f;
    bool collideConnected = false;
};

enum class FieldKind {
    Wind,     // constant push inside the region
    Radial,   // blast away from `position`, or towards it when the strength is negative
    Vortex,   // spins around `position`
    Buoyancy, // the region behaves like water
};

// A region of `halfSize` zero covers the whole world, which is what a plain
// wind field wants.
struct ForceField {
    FieldKind kind = FieldKind::Wind;
    Vec2 position;
    Vec2 halfSize;
    Vec2 direction{1.f, 0.f};
    float strength = 0.f;
    float radius = 0.f;
    float drag = 0.f;
};

struct SimulationOptions {
    Vec2 gravity{0.f, -900.f};
    float duration = 3.f;
    float airDrag = 0.08f;
    float angularDrag = 0.25f;
    int fixedRate = 120;
    int sampleRate = 20;
    int solverIterations = 5;
    float maxSpeed = 0.f;
    bool allowSleep = true;
    bool warmStarting = true;
    std::vector<ForceField> fields;
};

struct Pose {
    Vec2 position;
    float angle = 0.f;
};

struct Frame {
    float time = 0.f;
    std::vector<Pose> poses;
};

enum class ContactPhase {
    Begin,
    End,
};

// Enough to drive a sound, a particle or a trigger off the moment two things
// met, which the impact counter alone could never do.
struct ContactEvent {
    float time = 0.f;
    std::size_t bodyA = 0;
    std::size_t bodyB = 0;
    Vec2 point;
    Vec2 normal;
    float impulse = 0.f;
    float approachSpeed = 0.f;
    bool sensor = false;
    ContactPhase phase = ContactPhase::Begin;
};

struct SimulationTrace {
    std::vector<Frame> frames;
    std::vector<ContactEvent> contacts;
    std::size_t impacts = 0;
    float peakImpulse = 0.f;
    // When every dynamic body fell asleep, or negative if some never did. A
    // baked trajectory can be cut here without losing any movement.
    float settleTime = -1.f;
};

struct RayHit {
    bool hit = false;
    std::size_t body = 0;
    std::size_t fixture = 0;
    Vec2 point;
    Vec2 normal;
    float fraction = 1.f;
};

struct Overlap {
    std::size_t body = 0;
    std::size_t fixture = 0;
};

// True when the point is inside the fixture, in the frame the fixture's offset
// lives in. `slack` grows the shape, which is how a finger picks a thin object.
bool fixtureContains(Fixture const& fixture, Vec2 point, float slack = 0.f);

struct WorldData;

// The stateful side of the solver: step it by hand, read the bodies back, push
// them around and ask what is where. `simulate` below is this class run to the
// end of the duration in one call.
class PhysicsWorld {
public:
    PhysicsWorld(
        std::vector<BodySpec> const& bodies,
        SimulationOptions const& options,
        std::vector<Joint> const& joints = {}
    );
    ~PhysicsWorld();
    PhysicsWorld(PhysicsWorld&&) noexcept;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept;

    void step(float dt);

    float time() const;
    std::size_t bodyCount() const;
    Pose pose(std::size_t body) const;
    Vec2 velocity(std::size_t body) const;
    float angularVelocity(std::size_t body) const;
    bool asleep(std::size_t body) const;
    bool settled() const;
    Frame snapshot() const;

    void setVelocity(std::size_t body, Vec2 velocity);
    void setAngularVelocity(std::size_t body, float angularVelocity);
    void applyImpulse(std::size_t body, Vec2 impulse);
    void applyImpulseAt(std::size_t body, Vec2 impulse, Vec2 point);
    void applyAngularImpulse(std::size_t body, float impulse);
    void explode(Vec2 center, float radius, float strength);
    void wake(std::size_t body);

    RayHit raycast(Vec2 from, Vec2 to, std::uint32_t mask = kAllCategories) const;
    std::vector<Overlap> overlapPoint(Vec2 point, float slack = 0.f) const;
    std::vector<Overlap> overlapCircle(Vec2 center, float radius) const;

    // The begin/end pairs recorded by the last `step`.
    std::vector<ContactEvent> const& contacts() const;
    std::size_t impacts() const;
    float peakImpulse() const;

private:
    std::unique_ptr<WorldData> m_data;
};

SimulationTrace simulate(
    std::vector<BodySpec> const& bodies,
    SimulationOptions const& options,
    std::vector<Joint> const& joints = {}
);

} // namespace paimon::editorphysics
