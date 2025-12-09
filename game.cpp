/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#include "game.h"
#include "sprite_renderer.h"
#include "resource_manager.h"
#include "game_object.h"
#include "player_object.h"

#include <iostream>

SpriteRenderer  *Renderer;
GameObject      *Player;

  
PlayerObject     *Ball; 

Game::Game(unsigned int width, unsigned int height)
    : State(GAME_ACTIVE), Keys(), Width(width), Height(height)
{
    for (int i = 0; i < 1024; ++i)
        this->KeysProcessed[i] = false;

}

Game::~Game()
{
    delete Renderer;
    delete Player;
}

void Game::Init()
{
  ResourceManager::LoadShader("shaders/sprite.vs", "shaders/sprite.frag", nullptr, "sprite");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width), 
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    // set render-specific controls
    Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"));
    // load textures
    ResourceManager::LoadTexture("textures/background.jpg", false, "background");
    ResourceManager::LoadTexture("textures/awesomeface.png", true, "face");
    ResourceManager::LoadTexture("textures/block.png", false, "block");
    ResourceManager::LoadTexture("textures/block_solid.png", false, "block_solid");
    ResourceManager::LoadTexture("textures/paddle.png", true, "paddle");
    // load levels
    GameLevel one; one.Load("levels/one.lvl", this->Width, this->Height);
    GameLevel two; two.Load("levels/two.lvl", this->Width, this->Height / 2);
    GameLevel three; three.Load("levels/three.lvl", this->Width, this->Height / 2);
    GameLevel four; four.Load("levels/four.lvl", this->Width, this->Height / 2);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Levels.push_back(three);
    this->Levels.push_back(four);
    this->Level = 0;
    // configure game objects

    glm::vec2 playerPos = this->Levels[this->Level].PlayerStartPos; // Assuming 50x50 tiles
    glm::vec2 playerSize = glm::vec2(this->Width / 15.0f, this->Height / 8.0f);
    //glm::vec2 playerPos = glm::vec2(this->Width / 2.0f - PLAYER_SIZE.x / 2.0f, this->Height - PLAYER_SIZE.y);
    Player = new GameObject(playerPos, playerSize, ResourceManager::GetTexture("face"));
   //glm::vec2 ballPos = playerPos + glm::vec2(PLAYER_SIZE.x / 2.0f - BALL_RADIUS, -BALL_RADIUS * 2.0f);

   // Ball = new PlayerObject(ballPos, BALL_RADIUS, INITIAL_BALL_VELOCITY,
       // ResourceManager::GetTexture("face"));
}

void Game::Update(float dt)
{
        //Ball->Move(dt, this->Width);
}

void Game::ProcessInput(float dt)
{
     float stepX = this->Width / 15.0f; 
     float stepY = this->Height / 8.0f;
     if (this->State == GAME_ACTIVE)
    {
        float velocity = PLAYER_VELOCITY * dt;
        // move playerboard
        if (this->Keys[GLFW_KEY_A] && !this->KeysProcessed[GLFW_KEY_A])
        {
            if (Player->Position.x >= 0.0f)
                Player->Position.x -= stepX;
            this->KeysProcessed[GLFW_KEY_A] = true;
            
        }
        if (this->Keys[GLFW_KEY_D] && !this->KeysProcessed[GLFW_KEY_D])
        {
            if (Player->Position.x <= this->Width - Player->Size.x)
                Player->Position.x += stepX;

            this->KeysProcessed[GLFW_KEY_D] = true; // Mark as processed

            
        }
          if (this->Keys[GLFW_KEY_W] && !this->KeysProcessed[GLFW_KEY_W])
        {
            if (Player->Position.y >= 0.0f)
                Player->Position.y -= stepY;
            this->KeysProcessed[GLFW_KEY_W] = true; 

            
        }
        if (this->Keys[GLFW_KEY_S] && !this->KeysProcessed[GLFW_KEY_S])
        {
            if (Player->Position.y <= this->Height - Player->Size.y)
                Player->Position.y += stepY;
            this->KeysProcessed[GLFW_KEY_S] = true; 

        }
         if (!this->Keys[GLFW_KEY_A]) this->KeysProcessed[GLFW_KEY_A] = false;
        if (!this->Keys[GLFW_KEY_D]) this->KeysProcessed[GLFW_KEY_D] = false;
        if (!this->Keys[GLFW_KEY_W]) this->KeysProcessed[GLFW_KEY_W] = false;
         if (!this->Keys[GLFW_KEY_S]) this->KeysProcessed[GLFW_KEY_S] = false;
    }
}

void Game::Render()
{
     if(this->State == GAME_ACTIVE)
    {
        // draw background
        Renderer->DrawSprite(ResourceManager::GetTexture("background"), glm::vec2(0.0f, 0.0f), glm::vec2(this->Width, this->Height), 0.0f);
        // draw level
        this->Levels[this->Level].Draw(*Renderer);
        // draw player
        Player->Draw(*Renderer);
        //Ball->Draw(*Renderer);
    }
}
