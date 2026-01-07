/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#ifndef GAME_H
#define GAME_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "game_level.h"
#include "text_render.h" 

struct ma_engine;

// Represents the current state of the game
enum GameState {
    GAME_ACTIVE,
    GAME_MENU,
    GAME_WIN
};

// Game holds all game-related state and functionality.
// Combines all game-related data into a single class for
// easy access to each of the components and manageability.
class Game
{
public:
    // game state
    GameState               State;
    std::vector<GameLevel> Levels;
    unsigned int           Level;
    bool                    Keys[1024];
    bool                    KeysProcessed[1024];
    unsigned int            Width, Height;

    TextRenderer            *Text; 
    ma_engine               *AudioEngine;

    // Lighting system
    bool                    LightingEnabled;
    struct Light {
        glm::vec2 position;
        glm::vec3 color;
        float radius;
    };
    std::vector<Light>      Lights;

    // constructor/destructor
    Game(unsigned int width, unsigned int height);
    ~Game();
    // initialize game state (load all shaders/textures/levels)
    void Init();
    // game loop
    void ProcessInput(float dt);
    void Update(float dt);
    void Render();
};

#endif
