/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#ifndef GAMELEVEL_H
#define GAMELEVEL_H
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "game_object.h"
#include "sprite_renderer.h"
#include "resource_manager.h"


/// GameLevel holds all Tiles as part of a Breakout level and 
/// hosts functionality to Load/render levels from the harddisk.
class GameLevel
{
public:
    // level state
    std::vector<GameObject> Bricks;
    glm::vec2 PlayerStartPos;
    // Tile code 9 marks a monster spawn. It is stripped out of TileData during load
    // (the cell becomes plain floor) so the box-pushing and win-condition logic,
    // which both scan TileData, never sees a monster.
    std::vector<glm::vec2> MonsterSpawns;
    // Tile code 14 marks a cell where a pickup MAY appear. Like the spawns it is stripped
    // to floor on load; Game shuffles the level's pickups across these cells plus the
    // cells the pickups already occupy.
    std::vector<glm::vec2> PickupSlots;

    std::vector<std::vector<unsigned int>> TileData;

    // constructor
    GameLevel() { }
    // loads level from file
    void Load(const char *file, unsigned int levelWidth, unsigned int levelHeight);
    // render level
    void Draw(SpriteRenderer &renderer);
    // check if the level is completed (all non-solid tiles are destroyed)
    bool IsCompleted();
private:
    // initialize level from tile data
    void init(std::vector<std::vector<unsigned int>> tileData, unsigned int levelWidth, unsigned int levelHeight);
};

#endif