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
            MoveDirection(0.0f), VisualOffset(0.0f), MoveTimer(0.0f), MoveDuration(0.12f) { }

PlayerObject::PlayerObject(glm::vec2 pos, glm::vec2 size, Texture2D sprite)
        : GameObject(pos, size, sprite, glm::vec3(1.0f), glm::vec2(0.0f, 0.0f)), Radius(0.0f), Stuck(false),
            IsMoving(false), MoveStartPosition(pos), MoveTargetPosition(pos),
            MoveDirection(0.0f), VisualOffset(0.0f), MoveTimer(0.0f), MoveDuration(0.12f)
{ 
}
glm::vec2 PlayerObject::Move(float dt, unsigned int window_width)
{
    // if not stuck to player board
    return this->Position;
}

bool PlayerObject::MoveGrid(int dx, int dy, float stepX, float stepY, std::vector<std::vector<unsigned int>>& levelData, std::vector<GameObject>& bricks, ma_engine* audioEngine)
{
    if (this->IsMoving)
        return false;

    // Calculate current grid position
    int playerGridX = (int)(this->Position.x / stepX);
    int playerGridY = (int)(this->Position.y / stepY);
    
    int targetX = playerGridX + dx;
    int targetY = playerGridY + dy;
    
    // Check bounds
    if (targetY < 0 || targetY >= levelData.size() || 
        targetX < 0 || targetX >= levelData[0].size())
        return false;
    
    unsigned int targetTile = levelData[targetY][targetX];
    
    // Case 1: Empty floor (0) or Target (3) - Player can walk freely
    if (targetTile == 0 || targetTile == 3)
    {
        this->MoveStartPosition = this->Position;
        this->MoveTargetPosition = this->Position + glm::vec2(dx * stepX, dy * stepY);
        this->MoveDirection = glm::vec2((float)dx, (float)dy);
        this->MoveTimer = 0.0f;
        this->IsMoving = true;
    }
    // Case 2: Wall (1) or Border (5) - Block movement
    else if (targetTile == 1 || targetTile == 5)
    {
        // Do nothing - wall blocks movement
    }
    // Case 3: Box (2) - Try to push it
    else if (targetTile == 2)
    {
        // Check the tile BEHIND the box
        int boxNextX = targetX + dx;
        int boxNextY = targetY + dy;
        
        // Check bounds for box destination
        if (boxNextY < 0 || boxNextY >= levelData.size() || 
            boxNextX < 0 || boxNextX >= levelData[0].size())
            return false;
        
        unsigned int boxNextTile = levelData[boxNextY][boxNextX];
        
        // Can only push box if destination is empty (0) or target (3)
        if (boxNextTile == 0 || boxNextTile == 3)
        {
            // Update matrix: Move box
            levelData[boxNextY][boxNextX] = 2; // Place box at new location
            levelData[targetY][targetX] = 0;    // Clear old box position
            
            // Move player
            this->MoveStartPosition = this->Position;
            this->MoveTargetPosition = this->Position + glm::vec2(dx * stepX, dy * stepY);
            this->MoveDirection = glm::vec2((float)dx, (float)dy);
            this->MoveTimer = 0.0f;
            this->IsMoving = true;
            
            // Play box push sound
            if (audioEngine) {
                ma_engine_play_sound(audioEngine, "sounds/placing-cardboard-box.mp3", NULL);
            }
            
            // Update visual representation (find and move the box GameObject)
            for (GameObject& brick : bricks)
            {
                // Find the box at the old position
                if (!brick.IsSolid && 
                    (int)(brick.Position.x / stepX) == targetX && 
                    (int)(brick.Position.y / stepY) == targetY)
                {
                    // Move it to new position
                    brick.Position.x = boxNextX * stepX;
                    brick.Position.y = boxNextY * stepY;
                    break;
                }
            }
        }
    }
    int boxCount = 0;
    int targetCount = 0;
    
    for (const auto& row : levelData)
    {
        for (unsigned int tile : row)
        {
            if (tile == 2) boxCount++;
            if (tile == 3) targetCount++;
        }
    }
    
    // If all targets are covered by boxes, you win!
    // This means: no more standalone target tiles (3) visible
    return (targetCount == 0 && boxCount > 0);


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

    float arc = std::sin(t * 3.14159265f);
    this->VisualOffset.y = -3.0f * arc;
    this->Rotation = this->MoveDirection.x * 8.0f * arc;

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

