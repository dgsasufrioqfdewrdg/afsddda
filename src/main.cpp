// Dynamic Input Buffer – Geode mod (GD 2.2074 / Geode 4.10.2)
// Hooks:
//   GJBaseGameLayer::handleButton   -> every jump press/release
//   GJBaseGameLayer::processCommands -> once per physics step (tick clock)
//   PlayLayer::init / onQuit / resetLevel -> overlay + state
//   CCKeyboardDispatcher            -> F6 cycles mode
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

namespace {
    constexpr int MODES[] = {0, 2, 4, 8, 16};
    constexpr int MODE_COUNT = 5;

    struct BufferState {
        int modeIdx = 0;
        int tick = 0;
        bool pending = false;
        bool pendingP1 = true;
        int pendingSince = 0;
        int pendingButton = 1; // PlayerButton::Jump
        int lastSaved = 0;
        bool replaying = false;

        int window() const { return MODES[modeIdx]; }
        void clear() { pending = false; }
    };

    BufferState g;
    CCLabelBMFont* g_label = nullptr;

    std::string modeText() {
        if (g.window() == 0) return "Off";
        return fmt::format("+{} Ticks", g.window());
    }

    void refreshOverlay() {
        if (!g_label) return;
        g_label->setString(fmt::format(
            "Buffer: {}\nSaved: {} ticks", modeText(), g.lastSaved
        ).c_str());
    }

    bool canAct(PlayerObject* p) {
        return p && (p->m_isOnGround || p->m_isOnGround2);
    }
}

class $modify(DBGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g.replaying || g.window() == 0 || button != 1) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        if (down) {
            auto* p = isPlayer1 ? m_player1 : m_player2;
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            if (!canAct(p)) {
                g.pending = true;
                g.pendingP1 = isPlayer1;
                g.pendingSince = g.tick;
                g.pendingButton = button;
            } else {
                g.clear();
            }
        } else {
            g.clear();
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }
    }

    void processCommands(float dt) {
        GJBaseGameLayer::processCommands(dt);
        g.tick++;

        if (!g.pending || g.window() == 0) return;

        int age = g.tick - g.pendingSince;
        if (age > g.window()) {
            g.clear();
            return;
        }

        auto* p = g.pendingP1 ? m_player1 : m_player2;
        if (canAct(p)) {
            g.replaying = true;
            GJBaseGameLayer::handleButton(false, g.pendingButton, g.pendingP1);
            GJBaseGameLayer::handleButton(true, g.pendingButton, g.pendingP1);
            g.replaying = false;
            g.lastSaved = age;
            g.clear();
            refreshOverlay();
        }
    }
};

class $modify(DBPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g.tick = 0;
        g.lastSaved = 0;
        g.clear();

        g_label = CCLabelBMFont::create("", "chatFont.fnt");
        g_label->setAnchorPoint({0.f, 1.f});
        g_label->setScale(0.6f);
        g_label->setOpacity(180);
        g_label->setAlignment(kCCTextAlignmentLeft);
        auto win = CCDirector::sharedDirector()->getWinSize();
        g_label->setPosition({8.f, win.height - 8.f});
        g_label->setZOrder(1000);
        if (this->getParent()) {
            this->getParent()->addChild(g_label);
        } else {
            this->addChild(g_label);
        }
        refreshOverlay();
        return true;
    }

    void onQuit() {
        g_label = nullptr;
        g.clear();
        PlayLayer::onQuit();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g.clear();
    }
};

// F6 cycles: Off -> +2 -> +4 -> +8 -> +16 -> Off
class $modify(DBKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
        if (isKeyDown && !isKeyRepeat && key == KEY_F6) {
            if (PlayLayer::get()) {
                g.modeIdx = (g.modeIdx + 1) % MODE_COUNT;
                g.clear();
                refreshOverlay();
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);
    }
};
