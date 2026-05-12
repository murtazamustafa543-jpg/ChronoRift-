#pragma once

#include "ui.h"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include"animations.h"

using namespace std;
using namespace sf;

// main sfml renderer
class UIRenderer {
public:
void drawDropNotification(const SharedData& sd);
    UIRenderer() {}
    ~UIRenderer() { if (m_window.isOpen()) m_window.close(); }
   
    bool init();
    bool isOpen() const;
    void close();

    bool processEvents();

    void renderSplash     (float elapsedSec);
    void renderMainMenu   ();
    void renderPlayerSelect(int selectedCount);
    void renderGame       (const SharedData& sd);
    void renderGameOver   (const SharedData& sd);

    Screen currentScreen = Screen::SPLASH;

    function<void(const string&)>                    onMenuAction;
    function<void(int)>                              onPlayerCountConfirmed;
    function<void(int action, int targetIdx, int weaponUid)> onActionSelected;
    function<void(int weaponUniqueId)>               onWeaponSelected;
    function<void(int weaponUniqueId)>               onStorageWeaponSelected;
    function<void()>                                 onQuitRequested;

    RenderWindow m_window;
    Font         m_fontBody;
    Clock        m_clock;

    Texture      m_splashTexture;
    Sprite       m_splashSprite;
    Texture      m_bgTexture;
    Sprite       m_bgSprite;
    Texture   m_heroTexture[4];
    Sprite       m_heroSprite[4];
Texture   m_heroTexture1[4];
Animation m_heroAnim[4]{
    Animation(20, 36, 6, 0.15f),
    Animation(21, 43, 6, 0.15f),
    Animation(19, 27, 6, 0.15f),
    Animation(27, 41, 6, 0.20f)
};

Texture   m_enemyTexture[9];
Animation m_enemyAnim[9]{
    Animation(38, 45, 5, 0.12f),
    Animation(60, 72, 4, 0.12f),
    Animation(30, 35, 1, 0.12f),
    Animation(48, 53, 2, 0.12f),
    Animation(34, 39, 3, 0.12f),
    Animation(32, 39, 5, 0.12f),
    Animation(38, 38, 3, 0.12f),
    Animation(85.5, 92, 2, 0.12f),
    Animation(32, 34, 4, 0.12f)
};


float m_totalTime = 0.f;

    int  m_hoveredAction   = -1;
    int  m_selectedTarget  = -1;
    int  m_selectedWeapon  = -1;
    int  m_pendingAction   = ACTION_NONE;
    int  m_playerSelectVal = 1;
    bool m_confirmHover       = false;
bool m_awaitingTarget     = false;
bool m_awaitingWeapon     = false;
int  m_swapActiveUid      = -1;
int  m_swapStorageUid     = -1;
    
    const SharedData* m_sd = nullptr;

    void drawPanel(const FloatRect& r, Color bg, Color border, float radius = 6.f);
    void drawBar  (Vector2f pos, Vector2f size, float pct,
                   Color fill, Color bg, const string& label = "");
    void drawText (const string& s, float x, float y,
                   unsigned size, Color col, bool bold = false);
    void drawIcon (const string& emoji, float x, float y, unsigned size);

    void drawplayer(RenderWindow& win, Sprite sprite,
                    float x = 0, float y = 0, float f = 1.0f, float f2 = 1.0f) {
        win.clear();
        sprite.setPosition(x, y);
        sprite.setScale(f, f2);
        win.draw(sprite);
        win.display();
    }

    static const char* weaponName (int id);
    static Color       weaponColor(int id);

    bool     mouseOver(const FloatRect& r) const;
    Vector2f mousePos() const;

    void drawPlayerPanel   (const SharedData& sd);
    void drawEnemyPanel    (const SharedData& sd);
    void drawArena(const SharedData& sd, float dt);
    void drawActionBar     (const SharedData& sd);
    void drawInventoryPanel(const SharedData& sd);
    void drawLogPanel      (const SharedData& sd);
    void drawArtifactRow   (const SharedData& sd);
    void drawTurnIndicator (const SharedData& sd);

    void drawEntityCard(const EntityData& e, FloatRect r,
                        bool isPlayer, bool isActive, bool isTargeted, int index);
   
    void drawStorageList  (const EntityData& player, FloatRect r);

    void handleActionBarClick (const SharedData* sd, Vector2f mp);
    void handleEnemyClick     (const SharedData* sd, Vector2f mp);
    void handleInventoryClick (const SharedData* sd, Vector2f mp);
};
bool UIRenderer::init() {
    VideoMode vm(Layout::WIN_W, Layout::WIN_H);
    m_window.create(vm, "Chrono Rift", Style::Titlebar | Style::Close);
    m_window.setFramerateLimit(60);

    if (!m_fontBody.loadFromFile("Data/ArchivoBlack-Regular.ttf")) {
    }

    if (m_splashTexture.loadFromFile("Data/splash.png")) {
        m_splashSprite.setTexture(m_splashTexture);
    } else {
        printf("Warning: Failed to load splash image. Continuing without it.\n");
    }
    if (m_bgTexture.loadFromFile("Data/bg.png")) {
    m_bgSprite.setTexture(m_bgTexture);
    FloatRect bounds = m_bgSprite.getLocalBounds();
    m_bgSprite.setScale(
        Layout::WIN_W / bounds.width,
        Layout::WIN_H / bounds.height
    );
} else {
    printf("Warning: Failed to load background image.\n");
}

    static const char* heroFiles1[] = {
        "Data/alya.png",
        "Data/chrono.png",
        "Data/frog.png",
        "Data/magnus.png"
    };
    for (int i = 0; i < 4; i++) {
        if (m_heroTexture1[i].loadFromFile(heroFiles1[i])) {
            m_heroSprite[i].setTexture(m_heroTexture1[i]);
        } else {
            printf("Warning: Failed to load hero sprite: %s\n", heroFiles1[i]);
        }
    }
static const char* heroFiles[] = {
    "Data/alya1.png", "Data/chrono1.png",
    "Data/frog1.png", "Data/magnus1.png"
};
for (int i = 0; i < 4; i++) {
    if (m_heroTexture[i].loadFromFile(heroFiles[i])) {
        m_heroAnim[i].setex(m_heroTexture[i]);
    } else {
        printf("Warning: Failed to load hero sprite: %s\n", heroFiles[i]);
    }
}

static const char* enemyFiles[] = {
    "Data/blob.png", "Data/cybot.png", "Data/free1.png",
    "Data/ghost.png", "Data/imp.png", "Data/jinn.png",
    "Data/mage.png", "Data/mother.png", "Data/son.png"
};
for (int i = 0; i < 9; i++) {
    if (m_enemyTexture[i].loadFromFile(enemyFiles[i])) {
        m_enemyAnim[i].setex(m_enemyTexture[i]);
    } else {
        
        if (i > 0 && m_enemyTexture[0].getNativeHandle())
            m_enemyAnim[i].setex(m_enemyTexture[0]);
        printf("Warning: Failed to load enemy sprite: %s\n", enemyFiles[i]);
    }
}

    return true;
}

bool UIRenderer::isOpen() const { return m_window.isOpen(); }
void UIRenderer::close()        { m_window.close(); }

bool UIRenderer::processEvents() {
    Event ev;
    while (m_window.pollEvent(ev)) {
        if (ev.type == Event::Closed) {
            if (onQuitRequested) onQuitRequested();
            m_window.close();
            return false;
        }
        if (ev.type == Event::KeyPressed &&
            ev.key.code == Keyboard::Escape) {
            if (onQuitRequested) onQuitRequested();
        }

        if (currentScreen == Screen::MAIN_MENU &&
            ev.type == Event::MouseButtonReleased) {
            Vector2f mp = mousePos();
            FloatRect startBtn(540, 395, 200, 50);
            FloatRect quitBtn (540, 465, 200, 50);
            if (startBtn.contains(mp) && onMenuAction) onMenuAction("start");
            if (quitBtn.contains(mp)  && onMenuAction) onMenuAction("quit");
        }

        if (currentScreen == Screen::PLAYER_SELECT &&
            ev.type == Event::MouseButtonReleased) {
            Vector2f mp = mousePos();
            FloatRect minusBtn(520, 250, 40, 40);
            FloatRect plusBtn (720, 250, 40, 40);
            FloatRect confirmBtn(540, 320, 200, 50);
            if (minusBtn.contains(mp)   && m_playerSelectVal > 1) m_playerSelectVal--;
            if (plusBtn.contains(mp)    && m_playerSelectVal < MAX_PLAYERS) m_playerSelectVal++;
            if (confirmBtn.contains(mp) && onPlayerCountConfirmed)
                onPlayerCountConfirmed(m_playerSelectVal);
        }

        if (currentScreen == Screen::GAME &&
            ev.type == Event::MouseButtonReleased && m_sd) {
            Vector2f mp = mousePos();
      if (m_sd && m_sd->drop_active) {
            
            FloatRect center(440, 250, 400, 200);
            FloatRect yesR(center.left + 80, center.top + 140, 100, 40);
            FloatRect noR (center.left + 220, center.top + 140, 100, 40);

            if (yesR.contains(mp)) {
                // drop popup writes back into shared state
                sem_wait((sem_t*)&m_sd->state_lock);
                SharedData* sd_mut = const_cast<SharedData*>(m_sd);
                sd_mut->drop_choice_ready = 1;
                sd_mut->drop_choice_pick = 1;
                sem_post((sem_t*)&m_sd->state_lock);
                return true;
            }
            if (noR.contains(mp)) {
                sem_wait((sem_t*)&m_sd->state_lock);
                SharedData* sd_mut = const_cast<SharedData*>(m_sd);
                sd_mut->drop_choice_ready = 1;
                sd_mut->drop_choice_pick = 0;
                sem_post((sem_t*)&m_sd->state_lock);
                return true;
            }
        }
            handleActionBarClick(m_sd, mp);
            if (m_awaitingTarget) handleEnemyClick(m_sd, mp);
            if (m_awaitingWeapon) handleInventoryClick(m_sd, mp);
        }

        if (currentScreen == Screen::GAME &&
            ev.type == Event::KeyPressed && m_sd) {
            const int cur = m_sd->current_turn_type;
            if (cur == TURN_PLAYER) {
                switch (ev.key.code) {
                    case Keyboard::Num1: m_pendingAction = ACTION_STRIKE;     m_awaitingTarget = true;  break;
                    case Keyboard::Num2: m_pendingAction = ACTION_EXHAUST;    m_awaitingTarget = true;  break;
                    case Keyboard::Num3: m_pendingAction = ACTION_USE_WEAPON; m_awaitingWeapon = true;  break;
                    case Keyboard::Num4: m_pendingAction = ACTION_SWAP_IN;    m_awaitingWeapon = true;  break;
                    case Keyboard::Num5: if (onActionSelected) onActionSelected(ACTION_HEAL,    -1, -1); break;
                    case Keyboard::Num6: if (onActionSelected) onActionSelected(ACTION_SKIP,    -1, -1); break;
                    case Keyboard::Num7: if (onActionSelected) onActionSelected(ACTION_ULTIMATE, m_selectedTarget, -1); break;
                    case Keyboard::P:if (onActionSelected) onActionSelected(ACTION_PICKUP_ARTIFACT, -1, -1);break;
                    default: break;
                }
            }
        }
    }
    return true;
}

void UIRenderer::renderSplash(float elapsedSec) {
    drawplayer(m_window, m_splashSprite, 0, 0, 0.7f, 0.7f);
}

void UIRenderer::renderMainMenu() {
    float dt = m_clock.restart().asSeconds();
    m_window.clear();

    drawText("CHRONO RIFT",               410, 200, 58, Palette::ARTIFACT_GLOW);
    drawText("Turn-Based Temporal Combat", 500, 272, 18, Palette::TEXT_DIM);

    Vector2f mp = mousePos();

    FloatRect startR(530, 395, 200, 50);
    bool hoverStart = startR.contains(mp);
    drawPanel(startR, hoverStart ? Palette::ACTION_SEL : Palette::BG_PANEL2,
                      hoverStart ? Palette::BORDER_BRIGHT : Palette::BORDER);
    drawText("BEGIN QUEST", 560, 412, 18, hoverStart ? Palette::TEXT_BRIGHT : Palette::TEXT_DIM);

    FloatRect quitR(530, 460, 200, 50);
    bool hoverQuit = quitR.contains(mp);
    drawPanel(quitR, hoverQuit ? Color(60,20,20) : Palette::BG_PANEL2,
                     hoverQuit ? Color(200,80,80) : Palette::BORDER);
    drawText("Quit", 605, 475, 18, hoverQuit ? Color(255,120,100) : Palette::TEXT_DIM);

    drawText("Roll No: 24i0835 , 24i0876", 530, 760, 13, Palette::BORDER);

    m_window.display();
}

void UIRenderer::renderPlayerSelect(int /*selectedCount*/) {
    float dt = m_clock.restart().asSeconds();
    m_window.clear();

    drawText("ASSEMBLE YOUR PARTY",                  390, 100, 40, Palette::PLAYER_ACCENT);
    drawText("Choose how many heroes enter the Rift", 480, 180, 17, Palette::TEXT_DIM);

    FloatRect minusR(520, 240, 40, 40);
    FloatRect plusR (720, 240, 40, 40);
    Vector2f mp = mousePos();

    drawPanel(minusR, minusR.contains(mp) ? Palette::ACTION_SEL : Palette::BG_PANEL2, Palette::BORDER_BRIGHT);
    drawText("-", 536, 245, 22, Palette::TEXT_BRIGHT);

    string cnt = to_string(m_playerSelectVal);
    drawText(cnt, 635, 230, 42, Palette::GOLD);

    drawPanel(plusR, plusR.contains(mp) ? Palette::ACTION_SEL : Palette::BG_PANEL2, Palette::BORDER_BRIGHT);
    drawText("+", 732, 245, 22, Palette::TEXT_BRIGHT);

    float cardW = 250.f, cardH = 300.f;
    float totalW = m_playerSelectVal * cardW + (m_playerSelectVal - 1) * 20.f;
    float startX = 640 - totalW / 2.f;

    for (int i = 0; i < m_playerSelectVal; i++) {
        FloatRect cr(startX + i * (cardW + 20), 400, cardW, cardH);
        drawPanel(cr, Palette::BG_PANEL2, Palette::BORDER_BRIGHT);

        Sprite& spr = m_heroSprite[i];
        FloatRect tb = spr.getLocalBounds();
        if (tb.width > 0) {
            float maxW   = cardW - 20.f;
            float maxH   = cardH - 70.f;
            float scaleX = maxW / tb.width;
            float scaleY = maxH / tb.height;
            float scale  = min(scaleX, scaleY);
            spr.setScale(scale, scale);
            spr.setPosition(
                cr.left + (cardW - tb.width * scale) / 2.f,
                cr.top + 10.f
            );
            m_window.draw(spr);
        }

        drawText(heroNames[i], cr.left + cardW / 2 - 30, cr.top + 240, 20, Palette::TEXT_BRIGHT);
    }

    FloatRect confR(545, 318, 200, 50);
    bool hoverConf = confR.contains(mp);
    drawPanel(confR, hoverConf ? Palette::ACTION_SEL : Palette::BG_PANEL,
                     hoverConf ? Palette::BORDER_BRIGHT : Palette::BORDER);
    drawText("ENTER THE RIFT", 562, 327.5f, 17, hoverConf ? Palette::TEXT_BRIGHT : Palette::TEXT_DIM);

    m_window.display();
}

void UIRenderer::renderGameOver(const SharedData& sd) {
    float dt = m_clock.restart().asSeconds();
    m_window.clear(Palette::BG_DARK);

    bool won = (sd.winner == WINNER_PLAYERS);
    Color titleCol = won ? Palette::GOLD : Palette::HP_LOW;
    string titleStr    = won ? "VICTORY" : (sd.winner == WINNER_QUIT ? "ABANDONED" : "DEFEATED");
    string subtitleStr = won
        ? "The Rift has been sealed. Heroes prevail."
        : (sd.winner == WINNER_QUIT
           ? "You stepped away from the Rift."
           : "The darkness claims the Rift.");

    drawText(titleStr,    460, 200, 60, titleCol);
    drawText(subtitleStr, 460, 300, 20, Palette::TEXT_DIM);

    int totalKills = 0;
    for (int i = 0; i < sd.player_count; i++)
        totalKills += sd.players[i].total_kills;

    string statsStr = "Enemies Slain: " + to_string(sd.enemies_killed_total);
    drawText(statsStr, 550, 400, 16, Palette::TEXT_DIM);

    float cx = 600 - (sd.player_count * 110.f) / 2.f;
    for (int i = 0; i < sd.player_count; i++) {
        const EntityData& p = sd.players[i];
        FloatRect cr(cx + i * 120, 470, 100, 120);
        drawText(p.name,                    cr.left + 50, cr.top + 8,  11, p.alive ? Palette::TEXT_BRIGHT : Palette::TEXT_DEAD);
        drawText(p.alive ? "SURVIVED" : "FALLEN", cr.left + 50, cr.top + 28, 10, p.alive ? Palette::HP_FULL : Palette::HP_LOW);
        drawText("Kills: " + to_string(p.total_kills), cr.left + 50, cr.top + 50, 11, Palette::TEXT_DIM);
        drawText("Turns: " + to_string(p.total_turns), cr.left + 50, cr.top + 68, 11, Palette::TEXT_DIM);
    }

    drawText("Press ESC to exit", 550, 660, 15, Palette::BORDER);
    m_window.display();
}

void UIRenderer::renderGame(const SharedData& sd) {
    m_sd = &sd;
    float dt = m_clock.restart().asSeconds();
    m_totalTime += dt;
    m_window.clear(Palette::BG_DARK);

    m_window.draw(m_bgSprite);

    drawArtifactRow  (sd);
    drawPlayerPanel  (sd);
    drawEnemyPanel   (sd);
    drawArena        (sd, dt);
    drawActionBar    (sd);
    drawInventoryPanel(sd);
    drawLogPanel     (sd);
    drawTurnIndicator(sd);
if (sd.drop_active) {
    drawDropNotification(sd);
}
m_window.display();
}
void UIRenderer::drawPlayerPanel(const SharedData& sd) {
    FloatRect panelR = Layout::PLAYER_PANEL;
    drawPanel(panelR, Palette::BG_PANEL, Palette::BORDER);

    drawText("PARTY", panelR.left + 10, panelR.top + 8, 13, Palette::PLAYER_ACCENT, true);

    float cardH = (panelR.height - 30.f) / MAX_PLAYERS;
    for (int i = 0; i < sd.player_count; i++) {
        FloatRect cr(panelR.left + 5, panelR.top + 28 + i * cardH,
                     panelR.width - 10, cardH - 6);
        bool isActive = (sd.current_turn_type == TURN_PLAYER &&
                         sd.current_turn_index == i &&
                         sd.players[i].alive);
        drawEntityCard(sd.players[i], cr, true, isActive, false, i);
    }
}

void UIRenderer::drawEnemyPanel(const SharedData& sd) {
    FloatRect panelR = Layout::ENEMY_PANEL;
    drawPanel(panelR, Palette::BG_PANEL, Palette::BORDER);

    drawText("ENEMIES", panelR.left + 10, panelR.top + 8, 13, Palette::ENEMY_ACCENT, true);

    int alive = 0;
    for (int i = 0; i < sd.enemy_count; i++)
        if (sd.enemies[i].alive) alive++;
    string cntStr = to_string(alive) + " alive";
    drawText(cntStr, panelR.left + panelR.width - 70, panelR.top + 8, 12, Palette::TEXT_DIM);

    float cardH = (panelR.height - 30.f) / MAX_ENEMIES;
    for (int i = 0; i < sd.enemy_count; i++) {
        FloatRect cr(panelR.left + 5, panelR.top + 28 + i * cardH,
                     panelR.width - 10, cardH - 4);
        bool isActive   = (sd.current_turn_type == TURN_ENEMY &&
                           sd.current_turn_index == i &&
                           sd.enemies[i].alive);
        bool isTargeted = (m_selectedTarget == i && m_awaitingTarget);
        drawEntityCard(sd.enemies[i], cr, false, isActive, isTargeted, i);
    }
}

void UIRenderer::drawEntityCard(const EntityData& e, FloatRect r,
                                bool isPlayer, bool isActive, bool isTargeted,
                                int index) {
    Color border = isActive   ? (isPlayer ? Palette::PLAYER_ACCENT : Palette::ENEMY_ACCENT)
                 : isTargeted ? Palette::GOLD
                 : e.alive    ? Palette::BORDER
                              : Color(40, 35, 55);
    Color bg     = isActive   ? (isPlayer ? Color(20, 28, 50) : Color(50, 20, 20))
                 : e.alive    ? Palette::BG_PANEL2
                              : Color(15, 13, 20);

    drawPanel(r, bg, border);

    if (!e.alive) {
        drawText("X " + string(e.name), r.left + 8, r.top + 6, 12, Palette::TEXT_DEAD);
        return;
    }

    if (isActive) {
        RectangleShape strip({4.f, r.height - 8.f});
        strip.setFillColor(isPlayer ? Palette::PLAYER_ACCENT : Palette::ENEMY_ACCENT);
        strip.setPosition(r.left + 2, r.top + 4);
        m_window.draw(strip);
    }

    string prefix  = e.stunned ? "* " : "";
    Color nameCol  = e.stunned ? Palette::STUN_COLOR
                               : (isPlayer ? Palette::PLAYER_ACCENT : Palette::ENEMY_ACCENT);
    drawText(prefix + string(e.name), r.left + 10, r.top + 5, 12, nameCol, true);

    float barW = r.width - 18.f;
    float barX = r.left + 9.f;

    float hpPct  = (e.max_hp > 0) ? (float)e.hp / e.max_hp : 0.f;
    Color hpCol  = hpPct > 0.5f ? Palette::HP_FULL : hpPct > 0.25f ? Palette::HP_MID : Palette::HP_LOW;
    string hpStr = to_string(e.hp) + "/" + to_string(e.max_hp);
    drawBar({barX, r.top + 22.f}, {barW, 10.f}, hpPct, hpCol, Color(30, 20, 20), hpStr);

    float stPct  = (e.max_stamina > 0) ? (float)e.stamina / e.max_stamina : 0.f;
    string stStr = to_string(e.stamina) + "/" + to_string(e.max_stamina);
    drawBar({barX, r.top + 38.f}, {barW, 8.f}, stPct,
            Palette::STAMINA_COLOR, Palette::STAMINA_EMPTY, stStr);

    float ax = r.left + r.width - 10;
    if (e.has_eclipse_relic) { ax -= 14; drawText("<>", ax, r.top + 4, 11, Palette::ARTIFACT_GLOW); }
    if (e.has_lunar_blade)   { ax -= 14; drawText("()", ax, r.top + 4, 11, Color(180, 200, 255));   }
    if (e.has_solar_core)    { ax -= 14; drawText("*",  ax, r.top + 4, 11, Palette::GOLD);           }

    if (e.stunned) {
        time_t now = time(nullptr);
        int remaining = (int)(e.stun_end_time - now);
        if (remaining > 0)
            drawText("STUNNED " + to_string(remaining) + "s", r.left + 10, r.top + 52, 10, Palette::STUN_COLOR);
    }

    string statStr = "SPD:" + to_string(e.speed) + "  DMG:" + to_string(e.damage);
    drawText(statStr, r.left + 10, r.top + r.height - 14, 10, Palette::TEXT_DIM);

    if (isTargeted) {
        RectangleShape outline({r.width, r.height});
        outline.setPosition(r.left, r.top);
        outline.setFillColor(Color::Transparent);
        outline.setOutlineColor(Palette::GOLD);
        outline.setOutlineThickness(2.f);
        m_window.draw(outline);
    }
}

void UIRenderer::drawArena(const SharedData& sd, float dt) {
    FloatRect r = Layout::ARENA;

    // hand tuned positions
    float heroY = r.top + 420.f;
    float enemyY = r.top + 430.f;

    float heroX[4] = {
        r.left + 100.f,
        r.left + 220.f,
        r.left + 160.f,
        r.left + 280.f
    };
    float heroYpos[4] = {
        heroY,
        heroY,
        heroY - 30.f,
        heroY - 15.f
    };
    
    int pi = 0;
    for (int i = 0; i < sd.player_count; i++) {
        if (!sd.players[i].alive) continue;
        
        float cx = heroX[pi];
        float cy = heroYpos[pi];
        bool isActive = (sd.current_turn_type == TURN_PLAYER && sd.current_turn_index == i);

        if (isActive) {
            float pulse = 0.5f + 0.5f * std::sin(m_totalTime * 5.f);
            RectangleShape glow({40.f, 6.f});
            glow.setFillColor(Color(120, 180, 255, (uint8_t)(80 + 120 * pulse)));
            glow.setPosition(cx - 20.f, cy + 80.f);
            m_window.draw(glow);
        }

        m_heroAnim[i].animate(dt);
        m_heroAnim[i].setScale(1.4f, 1.4f);
        m_heroAnim[i].setPosition(cx - 24.f, cy);
        m_heroAnim[i].draw(m_window);

        float stPct = sd.players[i].max_stamina > 0
                    ? (float)sd.players[i].stamina / sd.players[i].max_stamina : 0.f;
        drawBar({cx - 25.f, cy - 8.f}, {50.f, 4.f}, stPct,
                Palette::STAMINA_COLOR, Color(0,0,0,160));

        RectangleShape nameBg({60.f, 12.f});
        nameBg.setPosition(cx - 30.f, cy + 82.f);
        nameBg.setFillColor(Color(0, 0, 0, 140));
        m_window.draw(nameBg);
        drawText(sd.players[i].name, cx - 25.f, cy + 83.f, 9, Palette::PLAYER_ACCENT);

        pi++;
    }

      float enemyX[9] = {
        r.left + 400.f,
        r.left + 530.f,
        r.left + 660.f,
        r.left + 430.f,
        r.left + 560.f,
        r.left + 690.f,
        r.left + 460.f,
        r.left + 590.f,
        r.left + 720.f
    };
    float enemyYpos[9] = {
        enemyY - 90.f,
        enemyY - 90.f,
        enemyY - 90.f,
        enemyY - 30.f,
        enemyY - 30.f,
        enemyY - 30.f,
        enemyY + 40.f,
        enemyY + 40.f,
        enemyY + 40.f
    };
    
    int ei = 0;
    for (int i = 0; i < sd.enemy_count; i++) {
        if (!sd.enemies[i].alive) continue;

        float cx = enemyX[ei];
        float cy = enemyYpos[ei];
        bool isActive   = (sd.current_turn_type == TURN_ENEMY && sd.current_turn_index == i);
        bool isTargeted = (m_selectedTarget == i && m_awaitingTarget);

        if (isActive || isTargeted) {
            float pulse = 0.5f + 0.5f * std::sin(m_totalTime * 5.f);
            Color glowCol = isTargeted
                ? Color(255, 200, 60, (uint8_t)(80 + 120 * pulse))
                : Color(255, 100, 80, (uint8_t)(80 + 120 * pulse));
            RectangleShape glow({40.f, 6.f});
            glow.setFillColor(glowCol);
            glow.setPosition(cx - 20.f, cy + 80.f);
            m_window.draw(glow);
        }

        int texIdx = i % 9;
        m_enemyAnim[texIdx].animate(dt);
        float scale = 1.4f;
        if (texIdx == 7) scale = 0.55f;
        m_enemyAnim[texIdx].setScale(-scale, scale);
        m_enemyAnim[texIdx].setPosition(cx + 20.f, cy);
        m_enemyAnim[texIdx].draw(m_window);

        float stPct = sd.enemies[i].max_stamina > 0
                    ? (float)sd.enemies[i].stamina / sd.enemies[i].max_stamina : 0.f;
        drawBar({cx - 25.f, cy - 8.f}, {50.f, 4.f}, stPct,
                Color(200,100,80), Color(0,0,0,160));

        Color nameCol = isTargeted ? Palette::GOLD : Palette::ENEMY_ACCENT;
        RectangleShape nameBg({60.f, 12.f});
        nameBg.setPosition(cx - 30.f, cy + 82.f);
        nameBg.setFillColor(Color(0, 0, 0, 140));
        m_window.draw(nameBg);
        drawText(sd.enemies[i].name, cx - 25.f, cy + 83.f, 9, nameCol);

        ei++;
    }

    RectangleShape statsBg({200.f, 18.f});
    statsBg.setPosition(r.left + 10.f, r.top + r.height - 20.f);
    statsBg.setFillColor(Color(0, 0, 0, 160));
    m_window.draw(statsBg);
    drawText("Kills: " + to_string(sd.enemies_killed_total) + " / 10",
             r.left + 14, r.top + r.height - 18, 11, Palette::GOLD);
    drawText("Time: " + to_string(sd.elapsed_seconds) + "s",
             r.left + r.width - 80, r.top + r.height - 18, 11, Palette::TEXT_DIM);
}
void UIRenderer::drawArtifactRow(const SharedData& sd) {
    FloatRect r = Layout::ARTIFACT_ROW;
    drawPanel(r, Color(20, 15, 35), Palette::BORDER);

    drawText("ARTIFACTS:", r.left + 8, r.top + 8, 12, Palette::TEXT_DIM, true);

    static const char* artifactNames[] = {"", "Solar Core", "Lunar Blade", "Eclipse Relic"};
    static Color artifactCols[] = {Palette::BG_DARK, Palette::GOLD,
                                   Color(180, 200, 255), Palette::ARTIFACT_GLOW};

    float ax = r.left + 110;
    bool anyFree = false;
    bool playerTurn = (sd.current_turn_type == TURN_PLAYER);

    for (int i = 0; i < ARTIFACT_COUNT; i++) {
        const ArtifactData& art = sd.artifacts[i];
        if (!art.present) continue;

        Color col = artifactCols[art.artifact_id];
        string status;
        
        if (art.owner_type != TYPE_NONE) {
            status = " [held]";
        } else if (art.locked_by_type != TYPE_NONE) {
            status = " [locked]";
        } else {
            status = " [FREE]";
            anyFree = true;
            col = Palette::GOLD;
        }

        drawText(string(artifactNames[art.artifact_id]) + status, ax, r.top + 8, 12, col);
        ax += 200;
    }

    if (!sd.eclipse_present) {
        drawText("Eclipse Relic: not yet spawned", ax, r.top + 8, 12, Palette::TEXT_DIM);
    }
}

void UIRenderer::drawActionBar(const SharedData& sd) {
    FloatRect r = Layout::ACTION_BAR;
    drawPanel(r, Palette::BG_PANEL, Palette::BORDER);

    bool playerTurn = (sd.current_turn_type == TURN_PLAYER);

    if (!playerTurn) {
        string msg = sd.current_turn_type == TURN_ENEMY ? "Enemy is thinking..." : "Awaiting turn...";
        drawText(msg, r.left + r.width / 2, r.top + r.height / 2 - 10, 18, Palette::TEXT_DIM);
        return;
    }

    const EntityData& actor = sd.players[sd.current_turn_index];
    string header = string(actor.name) + "'s Turn - choose action:";
    drawText(header, r.left + 10, r.top + 6, 14, Palette::PLAYER_ACCENT, true);

    if (m_awaitingTarget)
        drawText("Click an enemy to target (or press ESC to cancel)",
                 r.left + 10, r.top + 26, 11, Palette::GOLD);
    if (m_awaitingWeapon)
        drawText("Click a weapon in inventory below",
                 r.left + 10, r.top + 26, 11, Palette::GOLD);

    struct ActionBtn { int action; const char* label; const char* hotkey; };
    ActionBtn btns[] = {
        {ACTION_STRIKE,     "Strike",     "[1]"},
        {ACTION_EXHAUST,    "Exhaust",    "[2]"},
        {ACTION_USE_WEAPON, "Use Wpn",    "[3]"},
        {ACTION_SWAP_IN,    "Swap",       "[4]"},
        {ACTION_HEAL,       "Heal",       "[5]"},
        {ACTION_SKIP,       "Skip",       "[6]"},
        {ACTION_ULTIMATE,   "ULTIMATE",   "[7]"},
    };
    
    int numBtns = 7;
    int cols = 4;
    int rows = 2;
    
    float marginX = 8.f;
    float marginY = 6.f;
    float btnW = (r.width - 20.f - (cols - 1) * marginX) / cols;
    float btnH = (r.height - 38.f - (rows - 1) * marginY) / rows;

    float startY = r.top + 38.f;
    Vector2f mp = mousePos();

    for (int i = 0; i < numBtns; i++) {
        int row = i / cols;
        int col = i % cols;
        
        float rowOffset = 0;
        if (row == 1) {
            int bottomCount = numBtns - cols;
            float bottomWidth = bottomCount * btnW + (bottomCount - 1) * marginX;
            rowOffset = (r.width - 20.f - bottomWidth) / 2.f;
        }
        
        float bx = r.left + 10 + rowOffset + col * (btnW + marginX);
        float by = startY + row * (btnH + marginY);
        FloatRect br(bx, by, btnW, btnH);

        bool isUltimate  = (btns[i].action == ACTION_ULTIMATE);
        bool canUltimate = actor.has_solar_core && actor.has_lunar_blade;
        bool disabled    = isUltimate && !canUltimate;
        bool hover       = !disabled && br.contains(mp);
        bool selected    = (m_pendingAction == btns[i].action);

        Color bgCol = disabled ? Color(20, 18, 28) 
                     : selected ? Palette::ACTION_SEL 
                     : hover ? Palette::ACTION_HOVER 
                     : Palette::BG_PANEL2;
        Color brCol = disabled ? Color(40, 36, 55) 
                     : isUltimate ? Palette::ARTIFACT_GLOW 
                     : selected ? Palette::BORDER_BRIGHT 
                     : Palette::BORDER;

        drawPanel(br, bgCol, brCol);
        
        drawText(btns[i].hotkey, bx + 3, by + 2, 8, Palette::TEXT_DIM);

        Color lblCol = disabled ? Palette::TEXT_DEAD 
                      : isUltimate ? Palette::ARTIFACT_GLOW 
                      : Palette::TEXT_BRIGHT;
        drawText(btns[i].label, bx + btnW / 2, by + btnH / 2 - 5, 11, lblCol);

        if (isUltimate && !canUltimate)
            drawText("Need both", bx + btnW / 2, by + btnH - 10, 7, Palette::TEXT_DEAD);
    }

    bool anyArtifactFree = false;
    for (int a = 0; a < ARTIFACT_COUNT; a++) {
        if (sd.artifacts[a].present && sd.artifacts[a].owner_type == TYPE_NONE && sd.artifacts[a].locked_by_type == TYPE_NONE) {
            anyArtifactFree = true;
            break;
        }
    }
    if (anyArtifactFree) {
        FloatRect artR(r.left + r.width - 90, r.top + 4, 80, 22);
        bool hoverArt = artR.contains(mp);
        drawPanel(artR, hoverArt ? Palette::GOLD : Color(40, 35, 20), Palette::ARTIFACT_GLOW);
        drawText("PICKUP [P]", artR.left + 6, artR.top + 5, 9, hoverArt ? Palette::TEXT_BRIGHT : Palette::GOLD);
    }

    if (m_selectedTarget >= 0 && m_awaitingTarget) {
        string tinfo = "Target: " + string(sd.enemies[m_selectedTarget].name);
        drawText(tinfo, r.left + 10, r.top + r.height - 16, 11, Palette::GOLD);

        FloatRect confR(r.left + r.width - 80, r.top + r.height - 22, 70, 18);
        bool hoverConf = confR.contains(mp);
        drawPanel(confR, hoverConf ? Palette::ACTION_SEL : Palette::BG_PANEL2, Palette::BORDER_BRIGHT);
        drawText("OK", confR.left + confR.width / 2, confR.top + 2, 10, Palette::TEXT_BRIGHT);
    }
} 
void UIRenderer::drawInventoryPanel(const SharedData& sd) {
    FloatRect r = Layout::INVENTORY;
    drawPanel(r, Palette::BG_PANEL, Palette::BORDER);

    bool playerTurn = (sd.current_turn_type == TURN_PLAYER);
    int pi = playerTurn ? sd.current_turn_index : 0;
    const EntityData& p = sd.players[pi];

    drawText("INVENTORY", r.left + 8, r.top + 6, 13, Palette::PLAYER_ACCENT, true);

    string storBadge = "Storage: " + to_string(p.storage_count);
    drawText(storBadge, r.left + r.width - 90, r.top + 6, 11, Palette::TEXT_DIM);

    float slotW = (r.width - 16.f) / INVENTORY_SLOTS;
    float slotH = 18.f;
    float slotY = r.top + 26;

    for (int s = 0; s < INVENTORY_SLOTS; s++) {
        RectangleShape slotRect({slotW - 1, slotH});
        slotRect.setPosition(r.left + 8 + s * slotW, slotY);
        slotRect.setFillColor(Color(25, 22, 40));
        slotRect.setOutlineColor(Palette::BORDER);
        slotRect.setOutlineThickness(1.f);
        m_window.draw(slotRect);
    }

    Vector2f mp = mousePos();
    for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
        const OwnedWeapon& ow = p.weapons[w];
        if (!ow.used || ow.in_storage) continue;

        float wx = r.left + 8 + ow.slot_start * slotW;
        float ww = ow.slot_count * slotW - 2;
        FloatRect wr(wx, slotY, ww, slotH);

        Color wCol = weaponColor(ow.weapon_id);
        bool hov = wr.contains(mp) && m_awaitingWeapon;
bool sel = (m_selectedWeapon == ow.unique_id) || (m_swapActiveUid == ow.unique_id);

        RectangleShape wRect({ww, slotH});
        wRect.setPosition(wx, slotY);
        wRect.setFillColor(Color(wCol.r / 3, wCol.g / 3, wCol.b / 3, hov || sel ? 220 : 160));
        wRect.setOutlineColor(sel ? Palette::GOLD : hov ? Palette::BORDER_BRIGHT : wCol);
        wRect.setOutlineThickness(sel ? 2.f : 1.f);
        m_window.draw(wRect);

        if (ow.slot_count >= 3) {
            string wname = weaponName(ow.weapon_id);
            if ((int)wname.size() > ow.slot_count * 2)
                wname = wname.substr(0, ow.slot_count * 2 - 1);
            drawText(wname, wx + 2, slotY + 3, 9, Palette::TEXT_BRIGHT);
        }
    }

    for (int s = 0; s < INVENTORY_SLOTS; s += 5)
        drawText(to_string(s), r.left + 8 + s * slotW, slotY + slotH + 2, 9, Palette::TEXT_DIM);

    drawText("Weapons:", r.left + 8, slotY + slotH + 18, 11, Palette::TEXT_DIM, true);
    float wy2 = slotY + slotH + 34;
    int wrow = 0;
    for (int w = 0; w < MAX_WEAPON_COPIES && wrow < 4; w++) {
        const OwnedWeapon& ow = p.weapons[w];
        if (!ow.used || ow.in_storage) continue;
        string wstr = "  " + string(weaponName(ow.weapon_id)) +
                      "  [" + to_string(ow.slot_count) + "s]";
        Color wc  = weaponColor(ow.weapon_id);
        bool sel  = (m_selectedWeapon == ow.unique_id);
        drawText(wstr, r.left + 8, wy2 + wrow * 18, 11, sel ? Palette::GOLD : wc);
        wrow++;
    }

    float storageLabelY = wy2 + 4 * 18 + 8;
    if (p.storage_count > 0) {
        drawText("Storage:", r.left + 8, storageLabelY, 11, Palette::TEXT_DIM, true);
        int srow = 0;
        for (int i = 0; i < p.storage_count && srow < 4; i++) {
            for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
                const OwnedWeapon& ow = p.weapons[w];
                if (ow.used && ow.in_storage && ow.unique_id == p.storage_ids[i]) {
                    string sstr = "  " + string(weaponName(ow.weapon_id));
                    bool sel    = (m_swapStorageUid == ow.unique_id);
                    drawText(sstr, r.left + 8, storageLabelY + 16 + srow * 18, 11,
                             sel ? Palette::GOLD : Palette::TEXT_DIM);
                    srow++;
                    break;
                }
            }
        }
    } else {
        drawText("Storage: (empty)", r.left + 8, storageLabelY, 11, Palette::TEXT_DIM);
    }
}


void UIRenderer::drawLogPanel(const SharedData& sd) {
    FloatRect r = Layout::LOG_PANEL;
    drawPanel(r, Palette::LOG_BG, Palette::BORDER);

    drawText("BATTLE LOG", r.left + 8, r.top + 6, 12, Palette::TEXT_DIM, true);

    int maxVisible = 7;
    int start      = max(0, sd.log_count - maxVisible);
    float lineH    = (r.height - 28.f) / maxVisible;

    for (int i = start; i < sd.log_count; i++) {
        float ly = r.top + 24 + (i - start) * lineH;
        string line = sd.log_lines[i];

        sf::Text text(line, m_fontBody, 10);
        text.setFillColor(Palette::TEXT_DIM);
        text.setPosition(r.left + 8, ly);

        const float maxWidth = r.width - 18.f;

        while (text.getLocalBounds().width > maxWidth && !line.empty()) {
            if (line.length() > 3) {
                line = line.substr(0, line.length() - 4) + "...";
            } else {
                line = "...";
                break;
            }
            text.setString(line);
        }

        Color col = Palette::TEXT_DIM;
        if      (line.find("defeated") != string::npos || line.find("killed") != string::npos) 
            col = Palette::HP_LOW;
        else if (line.find("stunned")  != string::npos)  col = Palette::STUN_COLOR;
        else if (line.find("Ultimate") != string::npos)  col = Palette::ARTIFACT_GLOW;
        else if (line.find("healed")   != string::npos)  col = Palette::HP_FULL;
        else if (line.find("Player")   != string::npos)  col = Palette::PLAYER_ACCENT;

        text.setFillColor(col);
        m_window.draw(text);
    }
}

void UIRenderer::drawTurnIndicator(const SharedData& sd) {
    RectangleShape bar({(float)Layout::WIN_W, 8.f});
    if      (sd.current_turn_type == TURN_PLAYER) bar.setFillColor(Palette::PLAYER_ACCENT);
    else if (sd.current_turn_type == TURN_ENEMY)  bar.setFillColor(Palette::ENEMY_ACCENT);
    else                                           bar.setFillColor(Palette::BORDER);
    bar.setPosition(0, 0);
    m_window.draw(bar);
}


void UIRenderer::drawDropNotification(const SharedData& sd) {
    FloatRect center(440, 250, 400, 200);
    drawPanel(center, Color(20, 15, 35), Palette::GOLD, 8.f);
    drawText("WEAPON DROP!", center.left + 120, center.top + 20, 24, Palette::GOLD, true);
    drawText(weaponName(sd.drop_weapon_id), center.left + 160, center.top + 60, 20, Palette::TEXT_BRIGHT);
    drawText("Pick it up?", center.left + 140, center.top + 100, 16, Palette::TEXT_DIM);
    
    FloatRect yesR(center.left + 80, center.top + 140, 100, 40);
    bool hoverYes = yesR.contains(mousePos());
    drawPanel(yesR, hoverYes ? Palette::ACTION_SEL : Palette::BG_PANEL2, Palette::GOLD);
    drawText("YES", yesR.left + 35, yesR.top + 10, 16, Palette::TEXT_BRIGHT);
    
    FloatRect noR(center.left + 220, center.top + 140, 100, 40);
    bool hoverNo = noR.contains(mousePos());
    drawPanel(noR, hoverNo ? Color(80,20,20) : Palette::BG_PANEL2, Color(200,80,80));
    drawText("NO", noR.left + 40, noR.top + 10, 16, Color(255,120,100));
}

void UIRenderer::drawPanel(const FloatRect& r, Color bg, Color border, float /*radius*/) {
    RectangleShape rect({r.width, r.height});
    rect.setPosition(r.left, r.top);
    rect.setFillColor(bg);
    rect.setOutlineColor(border);
    rect.setOutlineThickness(1.f);
    m_window.draw(rect);
}

void UIRenderer::drawBar(Vector2f pos, Vector2f size, float pct,
                         Color fill, Color bg, const string& label) {
    pct = max(0.f, min(1.f, pct));

    RectangleShape bgRect(size);
    bgRect.setPosition(pos);
    bgRect.setFillColor(bg);
    m_window.draw(bgRect);

    if (pct > 0.f) {
        RectangleShape fillRect({size.x * pct, size.y});
        fillRect.setPosition(pos);
        fillRect.setFillColor(fill);
        m_window.draw(fillRect);
    }

    if (!label.empty() && size.y >= 10.f) {
        Text txt;
        txt.setFont(m_fontBody);
        txt.setString(label);
        txt.setCharacterSize(8);
        txt.setFillColor(Color(10, 10, 10));
        FloatRect tb = txt.getLocalBounds();
        txt.setPosition(pos.x + size.x / 2 - tb.width / 2,
                        pos.y + size.y / 2 - tb.height / 2 - 1);
        m_window.draw(txt);
    }
}

void UIRenderer::drawText(const string& s, float x, float y,
                          unsigned size, Color col, bool bold) {
    Text txt;
    txt.setFont(m_fontBody);
    txt.setString(s);
    txt.setCharacterSize(size);
    txt.setFillColor(col);
    if (bold) txt.setStyle(Text::Bold);
    txt.setPosition(x, y);
    m_window.draw(txt);
}

const char* UIRenderer::weaponName(int id) {
    switch (id) {
        case WEAPON_SOLAR_CORE:     return "Solar Core";
        case WEAPON_LUNAR_BLADE:    return "Lunar Blade";
        case WEAPON_IRON_HALBERD:   return "Iron Halberd";
        case WEAPON_VENOM_DAGGER:   return "Venom Dagger";
        case WEAPON_THUNDERSTAFF:   return "Thunderstaff";
        case WEAPON_OBSIDIAN_AXE:   return "Obsidian Axe";
        case WEAPON_FROSTBOW:       return "Frostbow";
        case WEAPON_SPLINTER_STICK: return "Splinter Stick";
        case WEAPON_ECLIPSE_RELIC: return "Eclipse Relic";
        default:                    return "Unknown";
    }
}

Color UIRenderer::weaponColor(int id) {
    switch (id) {
        case WEAPON_SOLAR_CORE:     return Palette::GOLD;
        case WEAPON_LUNAR_BLADE:    return Color(180, 200, 255);
        case WEAPON_IRON_HALBERD:   return Color(160, 160, 180);
        case WEAPON_VENOM_DAGGER:   return Color(100, 220, 100);
        case WEAPON_THUNDERSTAFF:   return Color(200, 180, 80);
        case WEAPON_OBSIDIAN_AXE:   return Color(120, 80,  160);
        case WEAPON_FROSTBOW:       return Color(100, 200, 230);
        case WEAPON_SPLINTER_STICK: return Color(160, 130, 90);
        case WEAPON_ECLIPSE_RELIC: return Color(148, 0, 211);
        default:                    return Palette::TEXT_DIM;
    }
}

bool UIRenderer::mouseOver(const FloatRect& r) const {
    return r.contains(mousePos());
}

Vector2f UIRenderer::mousePos() const {
    Vector2i mp = Mouse::getPosition(m_window);
    return {(float)mp.x, (float)mp.y};
}

void UIRenderer::handleActionBarClick(const SharedData* sd, Vector2f mp) {
    if (!sd || sd->current_turn_type != TURN_PLAYER) return;

    const EntityData& actor = sd->players[sd->current_turn_index];
    FloatRect r = Layout::ACTION_BAR;
    
    int cols = 4;
    float marginX = 8.f;
    float marginY = 6.f;
    float btnW = (r.width - 20.f - (cols - 1) * marginX) / cols;
    float btnH = (r.height - 38.f - 1 * marginY) / 2;
    float startY = r.top + 38.f;

    int actions[] = {ACTION_STRIKE, ACTION_EXHAUST, ACTION_USE_WEAPON, ACTION_SWAP_IN,
                     ACTION_HEAL, ACTION_SKIP, ACTION_ULTIMATE};

    for (int i = 0; i < 7; i++) {
        int row = i / cols;
        int col = i % cols;
        
        float rowOffset = 0;
        if (row == 1) {
            int bottomCount = 3;
            float bottomWidth = bottomCount * btnW + (bottomCount - 1) * marginX;
            rowOffset = (r.width - 20.f - bottomWidth) / 2.f;
        }
        
        float bx = r.left + 10 + rowOffset + col * (btnW + marginX);
        float by = startY + row * (btnH + marginY);
        FloatRect br(bx, by, btnW, btnH);
        
        if (!br.contains(mp)) continue;

        int act = actions[i];
        if (act == ACTION_ULTIMATE && !(actor.has_solar_core && actor.has_lunar_blade))
            continue;

        if (act == ACTION_HEAL || act == ACTION_SKIP) {
            m_pendingAction  = ACTION_NONE; m_awaitingTarget  = false;
m_awaitingWeapon = false;       m_selectedTarget  = -1;
m_swapActiveUid  = -1;          m_swapStorageUid  = -1;
            if (onActionSelected) onActionSelected(act, -1, -1);
            return;
        }
        if (act == ACTION_SWAP_IN || act == ACTION_USE_WEAPON) {
            m_pendingAction = act;
            m_awaitingWeapon = true;
            m_awaitingTarget = false;
            m_selectedTarget = -1;
            m_selectedWeapon = -1;
            return;
        }
        m_pendingAction = act; m_awaitingTarget = true; m_awaitingWeapon = false;
        m_selectedTarget = -1;
        return;
    }

    bool anyArtifactFree = false;
    for (int a = 0; a < ARTIFACT_COUNT; a++) {
        if (sd->artifacts[a].present && sd->artifacts[a].owner_type == TYPE_NONE && sd->artifacts[a].locked_by_type == TYPE_NONE) {
            anyArtifactFree = true;
            break;
        }
    }
    if (anyArtifactFree) {
        FloatRect artR(r.left + r.width - 90, r.top + 4, 80, 22);
        if (artR.contains(mp)) {
            if (onActionSelected) onActionSelected(ACTION_PICKUP_ARTIFACT, -1, -1);
            return;
        }
    }

    if (m_awaitingTarget && m_selectedTarget >= 0) {
        FloatRect confR(r.left + r.width - 80, r.top + r.height - 22, 70, 18);
        if (confR.contains(mp) && onActionSelected) {
            onActionSelected(m_pendingAction, m_selectedTarget, m_selectedWeapon);
            m_pendingAction = ACTION_NONE; m_awaitingTarget = false; m_selectedTarget = -1;
        }
    }
}
void UIRenderer::handleEnemyClick(const SharedData* sd, Vector2f mp) {
    if (!sd) return;
    FloatRect panelR = Layout::ENEMY_PANEL;
    float cardH = (panelR.height - 30.f) / MAX_ENEMIES;

    for (int i = 0; i < sd->enemy_count; i++) {
        if (!sd->enemies[i].alive) continue;
        FloatRect cr(panelR.left + 5, panelR.top + 28 + i * cardH,
                     panelR.width - 10, cardH - 4);
        if (cr.contains(mp)) {
            m_selectedTarget = i;
            if (m_pendingAction != ACTION_NONE && onActionSelected) {
                if (m_pendingAction == ACTION_USE_WEAPON && m_selectedWeapon < 0) {
                    return;
                }
                onActionSelected(m_pendingAction, m_selectedTarget, m_selectedWeapon);
                m_pendingAction = ACTION_NONE; 
                m_awaitingTarget = false; 
                m_selectedTarget = -1;
                m_selectedWeapon = -1;
            }
            return;
        }
    }
}

void UIRenderer::handleInventoryClick(const SharedData* sd, Vector2f mp) {
    if (!sd || sd->current_turn_type != TURN_PLAYER) return;
    const EntityData& p = sd->players[sd->current_turn_index];
    FloatRect r  = Layout::INVENTORY;
    float slotW  = (r.width - 16.f) / INVENTORY_SLOTS;
    float slotY  = r.top + 26;
    float slotH  = 18.f;

    // keep these in sync with drawInventoryPanel
    float wy2          = slotY + slotH + 34;
    float storageLabelY = wy2 + 4 * 18 + 8;
    float storageItemY  = storageLabelY + 16;

    if (m_pendingAction == ACTION_SWAP_IN) {
        int wrow = 0;
        for (int w = 0; w < MAX_WEAPON_COPIES && wrow < 4; w++) {
                const OwnedWeapon& ow = p.weapons[w];
            if (!ow.used || ow.in_storage) continue;
            FloatRect wr(r.left + 8, wy2 + wrow * 18, r.width - 16, 18.f);
            if (wr.contains(mp)) {
                m_swapActiveUid = ow.unique_id;
                if (m_swapStorageUid >= 0) {
                    if (onStorageWeaponSelected) onStorageWeaponSelected(m_swapStorageUid);
                    m_pendingAction  = ACTION_NONE;
                    m_awaitingWeapon = false;
                    m_selectedWeapon = -1;
                    m_swapActiveUid  = -1;
                    m_swapStorageUid = -1;
                }
                return;
            }
            wrow++;
        }

        int srow = 0;
        for (int i = 0; i < p.storage_count && srow < 4; i++) {
            for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
                const OwnedWeapon& ow = p.weapons[w];
                if (!ow.used || !ow.in_storage || ow.unique_id != p.storage_ids[i]) continue;
                FloatRect sr(r.left + 8, storageItemY + srow * 18, r.width - 16, 18.f);
                if (sr.contains(mp)) {
                    m_swapStorageUid = ow.unique_id;
                    if (m_swapActiveUid >= 0) {
                        if (onStorageWeaponSelected) onStorageWeaponSelected(m_swapStorageUid);
                        m_pendingAction  = ACTION_NONE;
                        m_awaitingWeapon = false;
                        m_selectedWeapon = -1;
                        m_swapActiveUid  = -1;
                        m_swapStorageUid = -1;
                    }
                    return;
                }
                srow++;
                break;
            }
        }
    }

    for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
        const OwnedWeapon& ow = p.weapons[w];
        if (!ow.used || ow.in_storage) continue;
        float wx = r.left + 8 + ow.slot_start * slotW;
        float ww = ow.slot_count * slotW - 2;
        FloatRect wr(wx, slotY, ww, slotH);
        if (wr.contains(mp)) {
            m_selectedWeapon = ow.unique_id;
            if (m_pendingAction == ACTION_USE_WEAPON) {
                m_awaitingWeapon = false;
                m_awaitingTarget = true;
            }
            return;
        }
    }
}
