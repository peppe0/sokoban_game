/******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#include "player_object.h"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>


 PlayerObject::PlayerObject() 
        : GameObject(), Radius(12.5f), Stuck(true),
            IsMoving(false), MoveStartPosition(0.0f), MoveTargetPosition(0.0f),
            MoveDirection(0.0f), VisualOffset(0.0f), MoveTimer(0.0f), MoveDuration(STEP_DURATION) { }

PlayerObject::PlayerObject(glm::vec2 pos, glm::vec2 size, Texture2D sprite)
        : GameObject(pos, size, sprite, glm::vec3(1.0f), glm::vec2(0.0f, 0.0f)), Radius(0.0f), Stuck(false),
            IsMoving(false), MoveStartPosition(pos), MoveTargetPosition(pos),
            MoveDirection(0.0f), VisualOffset(0.0f), MoveTimer(0.0f), MoveDuration(STEP_DURATION)
{ 
}
glm::vec2 PlayerObject::Move(float dt, unsigned int window_width)
{
    // if not stuck to player board
    return this->Position;
}

bool PlayerObject::MoveGrid(int dx, int dy, float stepX, float stepY, std::vector<std::vector<unsigned int>>& levelData, std::vector<GameObject>& bricks, ma_engine* audioEngine, ma_sound* audioGroup)
{
    if (this->IsMoving)
        return false;

    bool keyInsertedInChest = false;

    // Calculate current grid position
    int playerGridX = GridIndex(this->Position.x, stepX);
    int playerGridY = GridIndex(this->Position.y, stepY);
    
    int targetX = playerGridX + dx;
    int targetY = playerGridY + dy;
    
    // Check bounds
    if (targetY < 0 || targetY >= levelData.size() || 
        targetX < 0 || targetX >= levelData[0].size())
        return false;
    
    unsigned int targetTile = levelData[targetY][targetX];
    
    // Case 1: floor (0), spikes (6) and every pickup (8, 10, 11, 12, 13, 15, 16)
    // - Player can walk freely; pickups are consumed by Game once the step is accepted.
    if (targetTile == 0 || targetTile == 6 || targetTile == 8 ||
        targetTile == 10 || targetTile == 11 || targetTile == 12 || targetTile == 13 ||
        targetTile == 15 || targetTile == 16)
    {
        this->MoveStartPosition = this->Position;
        this->MoveTargetPosition = this->Position + glm::vec2(dx * stepX, dy * stepY);
        this->MoveDirection = glm::vec2((float)dx, (float)dy);
        this->MoveTimer = 0.0f;
        this->IsMoving = true;
    }
    // Case 2: Wall (1), Border (5) or Chest (3) - Block movement.
    // The chest is a solid object standing on its cell, not a floor marking: walking over
    // the treasure was possible only because tile 3 sat in the walkable list above.
    else if (targetTile == 1 || targetTile == 5 || targetTile == 3)
    {
        // Do nothing - wall blocks movement
    }
    // Case 3: Box (2) or Key (7) - Try to push it
    else if (targetTile == 2 || targetTile == 7)
    {
        const unsigned int movableTile = targetTile;
        // Check the tile BEHIND the box
        int boxNextX = targetX + dx;
        int boxNextY = targetY + dy;
        
        // Check bounds for box destination
        if (boxNextY < 0 || boxNextY >= levelData.size() || 
            boxNextX < 0 || boxNextX >= levelData[0].size())
            return false;
        
        unsigned int boxNextTile = levelData[boxNextY][boxNextX];
        
        // Destination must be empty (0) or spikes (6). The chest (3) accepts the key and
        // nothing else: a crate pushed onto it used to overwrite the chest tile, wiping
        // out the level's objective.
        const bool chestAcceptsThis = (boxNextTile == 3 && movableTile == 7);
        if (boxNextTile == 0 || boxNextTile == 6 || chestAcceptsThis)
        {
            const bool keyConsumedByChest = (movableTile == 7 && boxNextTile == 3);
            if (keyConsumedByChest)
                keyInsertedInChest = true;

            // Update matrix:
            // - key + chest: chest stays, key disappears
            // - otherwise: move object into destination
            if (!keyConsumedByChest)
                levelData[boxNextY][boxNextX] = movableTile;
            levelData[targetY][targetX] = 0;
            
            // Move player
            this->MoveStartPosition = this->Position;
            this->MoveTargetPosition = this->Position + glm::vec2(dx * stepX, dy * stepY);
            this->MoveDirection = glm::vec2((float)dx, (float)dy);
            this->MoveTimer = 0.0f;
            this->IsMoving = true;
            
            // Play box push sound
            if (audioEngine) {
                // Into the game's effects group, so it follows the same volume as the rest.
                ma_engine_play_sound(audioEngine, "sounds/placing-cardboard-box.mp3", audioGroup);
            }
            
            // Update visual representation (find moved object, or consume key on chest).
            for (GameObject& brick : bricks)
            {
                // Find the box at the old position
                if (!brick.IsSolid &&
                    GridIndex(brick.Position.x, stepX) == targetX &&
                    GridIndex(brick.Position.y, stepY) == targetY)
                {
                    if (keyConsumedByChest) {
                        brick.Destroyed = true;
                    } else {
                        // Move it to new position
                        brick.Position.x = boxNextX * stepX;
                        brick.Position.y = boxNextY * stepY;
                    }
                    break;
                }
            }
        }
    }
    // The level is won only by putting the key in the chest.
    //
    // A legacy "targetCount == 0 && boxCount > 0" rule used to sit here as well, meant to
    // mean "every target is covered". It never did: the code has no separate tile for a
    // covered target, so the test really read "no chest anywhere on the map", which is
    // true the moment a chest is destroyed, and true from the very first move on any
    // level that has no chest at all.
    return keyInsertedInChest;


}

void PlayerObject::UpdateAnimation(float dt)
{
    if (!this->IsMoving)
    {
        this->VisualOffset = glm::vec2(0.0f);
        this->Rotation = 0.0f;
        return;
    }

    this->MoveTimer += dt;
    float t = std::min(this->MoveTimer / std::max(this->MoveDuration, 0.001f), 1.0f);
    float eased = t * t * (3.0f - 2.0f * t);

    this->Position = this->MoveStartPosition + (this->MoveTargetPosition - this->MoveStartPosition) * eased;

    // No procedural hop or tilt any more: the skinned walk cycle supplies the vertical
    // motion, and adding a second bounce on top of it only reads as a stutter.
    this->VisualOffset = glm::vec2(0.0f);
    this->Rotation = 0.0f;

    if (t >= 1.0f)
    {
        this->Position = this->MoveTargetPosition;
        this->VisualOffset = glm::vec2(0.0f);
        this->Rotation = 0.0f;
        this->IsMoving = false;
    }
}

bool PlayerObject::IsAnimatingMovement() const
{
    return this->IsMoving;
}

void PlayerObject::Draw(SpriteRenderer &renderer)
{
    renderer.DrawSprite(this->Sprite, this->Position + this->VisualOffset, this->Size, this->Rotation, this->Color);
}

