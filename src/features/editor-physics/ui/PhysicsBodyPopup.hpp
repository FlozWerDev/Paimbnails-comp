#pragma once

#include "../services/PhysicsWorkspace.hpp"

#include <Geode/Geode.hpp>

#include <array>
#include <functional>
#include <string>

namespace paimon::editorphysics {

std::string bodyName(std::size_t index);

// Per body and per object materials: what the lab sliders set for everyone, one
// captured body (and one of its objects) can override here.
class PhysicsBodyPopup : public geode::Popup {
public:
    static PhysicsBodyPopup* create(
        std::size_t body,
        int object,
        std::function<void()> onChange
    );

private:
    bool init() override;
    void adjust(int field, int direction);
    void resetOverrides();
    void refreshValues();

    std::size_t m_body = 0;
    int m_object = -1;
    std::function<void()> m_onChange;
    std::array<cocos2d::CCLabelBMFont*, 10> m_valueLabels{};
};

} // namespace paimon::editorphysics
