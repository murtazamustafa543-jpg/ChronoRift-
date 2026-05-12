#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <string>
#include <vector>
#include <functional>
#include "../common.h"

// screen ids
enum class Screen {
    SPLASH,
    MAIN_MENU,
    PLAYER_SELECT,
    GAME,
    GAME_OVER
};

// colors
namespace Palette {
    const sf::Color BG_DARK       = sf::Color(10,  10,  18);
    const sf::Color BG_PANEL      = sf::Color(18,  18,  32);
    const sf::Color BG_PANEL2     = sf::Color(24,  24,  42);
    const sf::Color BORDER        = sf::Color(60,  55,  100);
    const sf::Color BORDER_BRIGHT = sf::Color(120, 100, 200);

    const sf::Color HP_FULL       = sf::Color(80,  220, 120);
    const sf::Color HP_MID        = sf::Color(220, 200, 60);
    const sf::Color HP_LOW        = sf::Color(220, 70,  60);

    const sf::Color STAMINA_COLOR = sf::Color(80,  160, 240);
    const sf::Color STAMINA_EMPTY = sf::Color(30,  50,  80);

    const sf::Color PLAYER_ACCENT = sf::Color(120, 180, 255);
    const sf::Color ENEMY_ACCENT  = sf::Color(255, 100, 80);
    const sf::Color GOLD          = sf::Color(255, 200, 60);
    const sf::Color ARTIFACT_GLOW = sf::Color(200, 100, 255);

    const sf::Color TEXT_BRIGHT   = sf::Color(240, 235, 255);
    const sf::Color TEXT_DIM      = sf::Color(140, 130, 170);
    const sf::Color TEXT_DEAD     = sf::Color(80,  70,  90);

    const sf::Color ACTION_HOVER  = sf::Color(60,  55,  100);
    const sf::Color ACTION_SEL    = sf::Color(90,  80,  160);

    const sf::Color STUN_COLOR    = sf::Color(255, 230, 50);
    const sf::Color LOG_BG        = sf::Color(12,  12,  22);
}

// layout
namespace Layout {
    const int WIN_W = 1280;
    const int WIN_H = 800;

    const sf::FloatRect PLAYER_PANEL = { 10,  10, 260, 540};
    const sf::FloatRect ENEMY_PANEL  = {1025, 10, 240, 540};

    const sf::FloatRect ARENA        = {280,  55, 740, 625};

    const sf::FloatRect ACTION_BAR   = {280, 690, 735, 100};

    const sf::FloatRect INVENTORY    = {10,  560, 260, 230};

    const sf::FloatRect LOG_PANEL    = {1025, 560, 240, 230};

    const sf::FloatRect ARTIFACT_ROW = {280,0,720,  50};
}

