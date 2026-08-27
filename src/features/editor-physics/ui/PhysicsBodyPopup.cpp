#include "PhysicsBodyPopup.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/GameObject.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <utility>

using namespace geode::prelude;

namespace paimon::editorphysics {

namespace {

constexpr float kPopupWidth = 380.f;
constexpr float kPopupHeight = 320.f;
constexpr float kFirstRowY = 248.f;
constexpr float kRowStep = 21.f;

enum Field {
    FieldMotion = 0,
    FieldMass,
    FieldGravity,
    FieldFriction,
    FieldBounce,
    FieldVelocityX,
    FieldVelocityY,
    FieldSpin,
    FieldObjectFriction,
    FieldObjectBounce,
    FieldCount,
};

char const* const kFieldNames[FieldCount] = {
    "Modo", "Masa", "Escala de gravedad", "Friccion", "Rebote",
    "Velocidad X", "Velocidad Y", "Giro inicial",
    "Friccion del objeto", "Rebote del objeto",
};

// Below zero the value goes back to whatever the lab sliders say, which is the
// state a body starts in.
float stepMaterial(float value, int direction, float step) {
    float const next = (value < 0.f ? (direction > 0 ? 0.f : -1.f) : value + direction * step);
    return next < 0.f ? -1.f : std::min(next, 1.f);
}

std::string materialText(float value) {
    return value < 0.f ? "lab" : fmt::format("{:.2f}", value);
}

} // namespace

std::string bodyName(std::size_t index) {
    if (index == 0) return "A";
    if (index == 1) return "B";
    return fmt::format("Extra {}", index - 1);
}

PhysicsBodyPopup* PhysicsBodyPopup::create(
    std::size_t body,
    int object,
    std::function<void()> onChange
) {
    auto* popup = new PhysicsBodyPopup();
    popup->m_body = body;
    popup->m_object = object;
    popup->m_onChange = std::move(onChange);
    if (popup->init()) {
        popup->autorelease();
        return popup;
    }
    CC_SAFE_DELETE(popup);
    return nullptr;
}

bool PhysicsBodyPopup::init() {
    if (!Popup::init(kPopupWidth, kPopupHeight)) return false;
    auto& workspace = PhysicsWorkspace::get();
    if (!workspace.material(m_body)) return false;

    setID("physics-body-popup"_spr);
    setTitle(fmt::format("Cuerpo {}", bodyName(m_body)));

    auto const& captured = workspace.bodies()[m_body];
    std::string subtitle = fmt::format("{} objetos en el cuerpo", captured.objects.size());
    if (m_object >= 0 && static_cast<std::size_t>(m_object) < captured.objects.size()) {
        auto locked = captured.objects[static_cast<std::size_t>(m_object)].lock();
        subtitle += fmt::format(
            " | elegido el {}{}",
            m_object + 1, locked ? fmt::format(" (ID {})", locked->m_objectID) : ""
        );
    } else {
        subtitle += " | toca un objeto en la vista para afinarlo solo a el";
    }
    auto* subtitleLabel = CCLabelBMFont::create(subtitle.c_str(), "bigFont.fnt");
    subtitleLabel->setScale(0.27f);
    subtitleLabel->setColor({155, 170, 200});
    subtitleLabel->setPosition({kPopupWidth * 0.5f, kFirstRowY + 20.f});
    subtitleLabel->limitLabelWidth(kPopupWidth - 40.f, 0.27f, 0.16f);
    m_mainLayer->addChild(subtitleLabel);

    auto* menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(menu, 5);
    WeakRef<PhysicsBodyPopup> self = this;

    for (int field = 0; field < FieldCount; ++field) {
        float const y = kFirstRowY - field * kRowStep;
        auto* name = CCLabelBMFont::create(kFieldNames[field], "bigFont.fnt");
        name->setAnchorPoint({0.f, 0.5f});
        name->setScale(0.3f);
        name->setPosition({26.f, y});
        m_mainLayer->addChild(name);

        auto* value = CCLabelBMFont::create("-", "bigFont.fnt");
        value->setScale(0.29f);
        value->setColor({255, 220, 110});
        value->setPosition({243.f, y});
        m_mainLayer->addChild(value);
        m_valueLabels[static_cast<std::size_t>(field)] = value;

        for (int direction : {-1, 1}) {
            auto* sprite = ButtonSprite::create(
                direction < 0 ? "-" : "+", "bigFont.fnt", "GJ_button_04.png", 0.8f
            );
            sprite->setScale(0.42f);
            auto* button = CCMenuItemExt::createSpriteExtra(
                sprite, [self, field, direction](CCMenuItemSpriteExtra*) {
                    if (auto popup = self.lock()) popup->adjust(field, direction);
                }
            );
            button->setPosition({direction < 0 ? 296.f : 348.f, y});
            menu->addChild(button);
        }
    }

    auto* resetSprite = ButtonSprite::create(
        "Volver al lab", 120, true, "bigFont.fnt", "GJ_button_06.png", 24.f, 0.55f
    );
    resetSprite->setScale(0.62f);
    auto* resetButton = CCMenuItemExt::createSpriteExtra(
        resetSprite, [self](CCMenuItemSpriteExtra*) {
            if (auto popup = self.lock()) popup->resetOverrides();
        }
    );
    resetButton->setPosition({kPopupWidth * 0.5f, 22.f});
    menu->addChild(resetButton);

    refreshValues();
    return true;
}

void PhysicsBodyPopup::adjust(int field, int direction) {
    auto& workspace = PhysicsWorkspace::get();
    auto* material = workspace.material(m_body);
    if (!material) return;

    auto* objectMaterial = m_object >= 0
        ? workspace.objectMaterial(m_body, static_cast<std::size_t>(m_object))
        : nullptr;

    switch (field) {
        case FieldMotion: {
            auto result = workspace.toggleMotion(m_body);
            if (result.isErr()) return;
            if (result.unwrap() == Motion::Dynamic && material->gravityScale <= 0.f) {
                material->gravityScale = 1.f;
            }
            break;
        }
        case FieldMass:
            material->mass = std::clamp(material->mass + direction * 0.5f, 0.f, 500.f);
            break;
        case FieldGravity:
            material->gravityScale = std::clamp(
                material->gravityScale + direction * 0.1f, -3.f, 3.f
            );
            break;
        case FieldFriction:
            material->friction = stepMaterial(material->friction, direction, 0.05f);
            break;
        case FieldBounce:
            material->restitution = stepMaterial(material->restitution, direction, 0.05f);
            break;
        case FieldVelocityX:
            material->launch.x = std::clamp(material->launch.x + direction * 50.f, -1500.f, 1500.f);
            material->customLaunch = true;
            break;
        case FieldVelocityY:
            material->launch.y = std::clamp(material->launch.y + direction * 50.f, -1500.f, 1500.f);
            material->customLaunch = true;
            break;
        case FieldSpin:
            material->spinDegrees = std::clamp(
                material->spinDegrees + direction * 15.f, -720.f, 720.f
            );
            material->customLaunch = true;
            break;
        case FieldObjectFriction:
            if (!objectMaterial) return;
            objectMaterial->friction = stepMaterial(objectMaterial->friction, direction, 0.05f);
            break;
        case FieldObjectBounce:
            if (!objectMaterial) return;
            objectMaterial->restitution = stepMaterial(objectMaterial->restitution, direction, 0.05f);
            break;
        default:
            return;
    }

    refreshValues();
    if (m_onChange) m_onChange();
}

void PhysicsBodyPopup::resetOverrides() {
    auto& workspace = PhysicsWorkspace::get();
    auto* material = workspace.material(m_body);
    if (!material) return;
    float const gravityScale = material->gravityScale;
    *material = {};
    material->gravityScale = gravityScale;
    if (m_object >= 0) {
        if (auto* objectMaterial =
                workspace.objectMaterial(m_body, static_cast<std::size_t>(m_object))) {
            *objectMaterial = {};
        }
    }
    refreshValues();
    if (m_onChange) m_onChange();
}

void PhysicsBodyPopup::refreshValues() {
    auto& workspace = PhysicsWorkspace::get();
    auto* material = workspace.material(m_body);
    if (!material) return;
    auto* objectMaterial = m_object >= 0
        ? workspace.objectMaterial(m_body, static_cast<std::size_t>(m_object))
        : nullptr;

    std::array<std::string, FieldCount> const values{
        workspace.bodies()[m_body].motion == Motion::Dynamic ? "dinamico" : "fijo",
        material->mass > 0.f ? fmt::format("{:.1f}", material->mass) : "auto",
        fmt::format("x{:.1f}", material->gravityScale),
        materialText(material->friction),
        materialText(material->restitution),
        material->customLaunch ? fmt::format("{:.0f}", material->launch.x) : "lab",
        material->customLaunch ? fmt::format("{:.0f}", material->launch.y) : "lab",
        material->customLaunch ? fmt::format("{:.0f} deg/s", material->spinDegrees) : "lab",
        objectMaterial ? materialText(objectMaterial->friction) : "-",
        objectMaterial ? materialText(objectMaterial->restitution) : "-",
    };
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (m_valueLabels[i]) m_valueLabels[i]->setString(values[i].c_str());
    }
}

} // namespace paimon::editorphysics
