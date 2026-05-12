#pragma once
#include <iostream>
#include <fstream>
#include <cmath>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Window.hpp>

using namespace std;
using namespace sf;

class Animation {
private:
    Sprite sprite;
    Texture texture;
    int frameWidth;
    int frameHeight;
    int frameCount;
    float frameTime;      
    float elapsedTime = 0;
    int currentFrame = 0;

    int startPos_x[10];      
    int frame_w[10]; 
    bool useFrame_w = false;

public:
    Animation(int width = 0, int height = 0, int frames = 0, float speed = 0) {
        frameWidth = width;
        frameHeight = height;
        frameCount = frames;
        frameTime = speed;

        sprite.setTextureRect(sf::IntRect(0, 0, frameWidth, frameHeight));
    }

    void setFrame_w(int x[], int widths[]) 
    {
        for (int i = 0; i < frameCount; ++i) {
            startPos_x[i] = x[i];
            frame_w[i] = widths[i];
        }

        useFrame_w = true;
        sprite.setTextureRect(IntRect(startPos_x[0], 0, frame_w[0], 33));
    }

    void setex(Texture& tex) 
    {
        sprite.setTexture(tex);
    }

    void animate(float deltaTime) {
        elapsedTime += deltaTime;

        if (elapsedTime >= frameTime) {
            currentFrame = (currentFrame + 1) % frameCount;
            if (useFrame_w) 
            {
                sprite.setTextureRect(IntRect(startPos_x[currentFrame], 0, frame_w[currentFrame], 33));
            }
            else 
            {
                sprite.setTextureRect(IntRect(currentFrame * frameWidth, 0, frameWidth, frameHeight));
            }
            elapsedTime = 0;
        }
    }

    void setPosition(float x, float y){
        sprite.setPosition(x, y);
    }

    void draw(sf::RenderWindow& window) {
        window.draw(sprite);
    }

    void setScale(float x, float y) {
        sprite.setScale(x, y);
    }

    void setframes(int width = 0, int height = 0, int frames = 0, float speed = 0) {
        frameWidth = width;
        frameHeight = height;
        frameCount = frames;
        frameTime = speed;

        sprite.setTextureRect(IntRect(0, 0, frameWidth, frameHeight));
    }
};
