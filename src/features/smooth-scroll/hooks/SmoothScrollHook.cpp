#include "../services/SmoothScrollController.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// Mouse-wheel smooth scroll only makes sense on desktop. On iOS the
// CCMouseDispatcher::dispatchScrollMSG binding is inlined (not hookable) and
// mobile has no mouse wheel, so the whole hook is desktop-only.
#if defined(GEODE_IS_DESKTOP)

#include <Geode/modify/CCMouseDispatcher.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include <cmath>

namespace {
    bool s_tickScheduled = false;
}

class $modify(PaimonSmoothScrollDispatcher, CCMouseDispatcher) {
    static void onModify(auto& self) {
        (void)self.setHookPriorityPre(
            "cocos2d::CCMouseDispatcher::dispatchScrollMSG",
            geode::Priority::Normal
        );
    }

    void tickSmoothScroll(float dt) {
        auto& ctrl = paimon::smoothscroll::SmoothScrollController::get();
        if (!ctrl.isActive()) {
            this->stopSmoothTick();
            return;
        }

        if (paimon::smoothscroll::shouldBypassSmoothScroll()) {
            this->stopSmoothTick();
            return;
        }

        ctrl.tick(dt, [this](float y, float x) {
            this->dispatchScrollMSG(y, x);
        });

        if (!ctrl.hasMomentum()) {
            this->stopSmoothTick();
        }
    }

    void startSmoothTick() {
        if (s_tickScheduled) return;
        s_tickScheduled = true;
        CCScheduler::get()->scheduleSelector(
            schedule_selector(PaimonSmoothScrollDispatcher::tickSmoothScroll),
            this,
            0.f,
            false
        );
    }

    void stopSmoothTick() {
        if (!s_tickScheduled) {
            paimon::smoothscroll::SmoothScrollController::get().reset();
            return;
        }
        s_tickScheduled = false;
        CCScheduler::get()->unscheduleSelector(
            schedule_selector(PaimonSmoothScrollDispatcher::tickSmoothScroll),
            this
        );
        paimon::smoothscroll::SmoothScrollController::get().reset();
    }

    bool dispatchScrollMSG(float y, float x) {
        auto& ctrl = paimon::smoothscroll::SmoothScrollController::get();

        if (!ctrl.isActive() || ctrl.isReplaying()) {
            return CCMouseDispatcher::dispatchScrollMSG(y, x);
        }

        if (paimon::smoothscroll::shouldBypassSmoothScroll() ||
            !m_pMouseHandlers || m_pMouseHandlers->count() == 0) {
            this->stopSmoothTick();
            return CCMouseDispatcher::dispatchScrollMSG(y, x);
        }

        if (ctrl.queueInput(y, x)) {
            this->startSmoothTick();
            return false;
        }

        this->stopSmoothTick();
        return CCMouseDispatcher::dispatchScrollMSG(y, x);
    }
};

// EditorUI::scrollWheel treats every call as a complete +/-0.1 zoom step and
// ignores the delta magnitude. Smooth replay calls it once per frame, so a
// single notch used to become dozens of full steps. Keep the native wheel path
// (timestamp, focus point and UI updates), but replace the fixed updateZoom
// request with this frame's normalized fraction of one physical wheel step.
class $modify(PaimonFilteredEditorZoom, EditorUI) {
    static void onModify(auto& self) {
        // Normalize the requested zoom before later hooks observe it.
        (void)self.setHookPriorityPre("EditorUI::updateZoom", geode::Priority::VeryEarly);
    }

    void updateZoom(float zoom) {
        auto& ctrl = paimon::smoothscroll::SmoothScrollController::get();
        if (!ctrl.isEditorZoomReplay()) {
            EditorUI::updateZoom(zoom);
            return;
        }

        float const steps = ctrl.replayedZoomSteps();
        if (std::abs(steps) < 0.000001f) return;

        auto* objectLayer = m_editorLayer ? m_editorLayer->m_objectLayer : nullptr;
        if (!objectLayer) return;

        constexpr float kEditorZoomPerStep = 0.1f;
        constexpr float kEditorZoomMin = 0.1f;
        constexpr float kEditorZoomMax = 4.f;
        float const current = objectLayer->getScale();
        float const filtered = std::clamp(
            current + steps * kEditorZoomPerStep,
            kEditorZoomMin,
            kEditorZoomMax
        );
        EditorUI::updateZoom(filtered);
    }
};

#endif // GEODE_IS_DESKTOP

#if defined(GEODE_IS_WINDOWS)
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/cocos/CCDirector.h>

// NOTA: el nombre de la clase $modify DEBE ser unico en todo el binario. Antes
// se llamaba "CaptureView", colisionando con el $modify(CaptureView, CCEGLView)
// de src/hooks/CCEGLView.cpp (ODR violation): el enlazador se quedaba con una
// sola definicion y descartaba la otra, dejando sin aplicar el hook de
// swapBuffers (ejecutor de capturas). Renombrado a SmoothScrollEGLView para que
// ambos $modify coexistan (Geode los combina por nombre unico).
class $modify(SmoothScrollEGLView, CCEGLView) {
    static void onModify(auto& self) {
        (void)self.setHookPriorityPre(
            "cocos2d::CCEGLView::onGLFWMouseScrollCallback",
            geode::Priority::Late
        );
    }

    void onGLFWMouseScrollCallback(GLFWwindow* window, double x, double y) {
        auto& ctrl = paimon::smoothscroll::SmoothScrollController::get();
        if (!ctrl.isActive()) {
            CCEGLView::onGLFWMouseScrollCallback(window, x, y);
            return;
        }

        constexpr double kWinWheelScale = 5.0;
        if (auto* director = CCDirector::get()) {
            if (auto* mouse = director->getMouseDispatcher()) {
                mouse->dispatchScrollMSG(
                    static_cast<float>(-y * kWinWheelScale),
                    static_cast<float>(x * kWinWheelScale)
                );
                return;
            }
        }

        CCEGLView::onGLFWMouseScrollCallback(window, x, y);
    }
};
#endif
