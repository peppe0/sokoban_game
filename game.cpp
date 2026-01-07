/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "game.h"
#include "sprite_renderer.h"
#include "resource_manager.h"
#include "game_object.h"
#include "player_object.h"
#include <iostream>

SpriteRenderer  *Renderer;
PlayerObject    *Player;


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
    delete Text;
    if (AudioEngine) {
        ma_engine_uninit(AudioEngine);
        delete AudioEngine;
    }
}

void Game::Init()
{
    // Initialize audio engine
    AudioEngine = new ma_engine();
    ma_result result = ma_engine_init(NULL, AudioEngine);
    if (result != MA_SUCCESS) {
        std::cout << "Failed to initialize audio engine." << std::endl;
    }

    // Initialize lighting
    LightingEnabled = true;

  ResourceManager::LoadShader("shaders/sprite.vs", "shaders/sprite.frag", nullptr, "sprite");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width), 
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    
    // Setup lighting
    ResourceManager::GetShader("sprite").SetInteger("enableLighting", LightingEnabled);
    ResourceManager::GetShader("sprite").SetFloat("ambientStrength", 0.1f); // Darker ambient
    
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
    GameLevel two; two.Load("levels/two.lvl", this->Width, this->Height);
    GameLevel three; three.Load("levels/three.lvl", this->Width, this->Height);
    GameLevel four; four.Load("levels/four.lvl", this->Width, this->Height);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Levels.push_back(three);
    this->Levels.push_back(four);
    this->Level = 0;
   
    glm::vec2 playerPos = this->Levels[this->Level].PlayerStartPos; // Assuming 50x50 tiles
    glm::vec2 playerSize = glm::vec2(this->Width / 15.0f, this->Height / 8.0f);
    Player = new PlayerObject(playerPos, playerSize, ResourceManager::GetTexture("face"));
     Text = new TextRenderer(this->Width, this->Height);
    Text->Load("fonts/Vollkorn-Black.ttf", 24);

}

void Game::Update(float dt)
{
    // Update lights
    Lights.clear();
    
    // Light follows player
    Light playerLight;
    playerLight.position = Player->Position + glm::vec2(Player->Size.x / 2.0f, Player->Size.y / 2.0f);
    playerLight.color = glm::vec3(0.8f, 0.7f, 0.5f); // Dimmer warm white
    playerLight.radius = 180.0f;
    Lights.push_back(playerLight);
    
    // Add a static light in the center
    Light centerLight;
    centerLight.position = glm::vec2(this->Width / 2.0f, this->Height / 2.0f);
    centerLight.color = glm::vec3(0.8f, 0.8f, 1.0f); // Cool white
    centerLight.radius = 150.0f;
    Lights.push_back(centerLight);

    if (this->State == GAME_WIN)
    {
        // Rotate the player continuously (360 degrees per second)
        Player->Rotation += 360.0f * dt;
        
        // Keep rotation between 0-360
        if (Player->Rotation >= 360.0f)
            Player->Rotation -= 360.0f;
    }

}

void Game::ProcessInput(float dt)
{

     if (this->State == GAME_ACTIVE)
    {
        float stepX = this->Width / 15.0f; 
        float stepY = this->Height / 8.0f;
        int dx = 0, dy = 0;
        
         if (this->Keys[GLFW_KEY_A] && !this->KeysProcessed[GLFW_KEY_A]) {
            dx = -1;
            this->KeysProcessed[GLFW_KEY_A] = true;
        }
        if (this->Keys[GLFW_KEY_D] && !this->KeysProcessed[GLFW_KEY_D]) {
            dx = 1;
            this->KeysProcessed[GLFW_KEY_D] = true;
        }
        if (this->Keys[GLFW_KEY_W] && !this->KeysProcessed[GLFW_KEY_W]) {
            dy = -1;
            this->KeysProcessed[GLFW_KEY_W] = true;
        }
        if (this->Keys[GLFW_KEY_S] && !this->KeysProcessed[GLFW_KEY_S]) {
            dy = 1;
            this->KeysProcessed[GLFW_KEY_S] = true;
        }

        if (!this->Keys[GLFW_KEY_A]) this->KeysProcessed[GLFW_KEY_A] = false;
        if (!this->Keys[GLFW_KEY_D]) this->KeysProcessed[GLFW_KEY_D] = false;
        if (!this->Keys[GLFW_KEY_W]) this->KeysProcessed[GLFW_KEY_W] = false;
        if (!this->Keys[GLFW_KEY_S]) this->KeysProcessed[GLFW_KEY_S] = false;
        
        // Toggle lighting with L key
        if (this->Keys[GLFW_KEY_L] && !this->KeysProcessed[GLFW_KEY_L]) {
            LightingEnabled = !LightingEnabled;
            this->KeysProcessed[GLFW_KEY_L] = true;
            std::cout << "Lighting: " << (LightingEnabled ? "ON" : "OFF") << std::endl;
        }
        if (!this->Keys[GLFW_KEY_L]) this->KeysProcessed[GLFW_KEY_L] = false;
        
        if (dx != 0 || dy != 0)
        {
           bool won = Player->MoveGrid(dx, dy, stepX, stepY, 
                this->Levels[this->Level].TileData,    
                this->Levels[this->Level].Bricks, this->AudioEngine);
            
            if (won)
            {
                this->State = GAME_WIN;
                ma_engine_play_sound(AudioEngine, "sounds/win.mp3", NULL);
                std::cout << "YOU WON!" << std::endl; // Console message
            }
        }
          
    }
    if (this->State == GAME_WIN)
        {
        if (this->Keys[GLFW_KEY_ENTER])
        {
            this->State = GAME_ACTIVE;
            this->Levels[this->Level].Load("levels/one.lvl", this->Width, this->Height); // Reload level
            Player->Position = this->Levels[this->Level].PlayerStartPos; // Reset player
            Player->Rotation = 0.0f;
        }
         }
}

void Game::Render()
{
     if(this->State == GAME_ACTIVE || this->State == GAME_WIN)
    {
        // Update lighting uniforms
        Shader shader = ResourceManager::GetShader("sprite");
        shader.Use();
        shader.SetInteger("enableLighting", LightingEnabled);
        shader.SetInteger("numLights", std::min((int)Lights.size(), 4));
        
        for (size_t i = 0; i < std::min(Lights.size(), (size_t)4); i++) {
            std::string base = "lightPositions[" + std::to_string(i) + "]";
            shader.SetVector2f(base.c_str(), Lights[i].position);
            base = "lightColors[" + std::to_string(i) + "]";
            shader.SetVector3f(base.c_str(), Lights[i].color);
            base = "lightRadii[" + std::to_string(i) + "]";
            shader.SetFloat(base.c_str(), Lights[i].radius);
        }
        
        // draw background
        Renderer->DrawSprite(ResourceManager::GetTexture("background"), glm::vec2(0.0f, 0.0f), glm::vec2(this->Width, this->Height), 0.0f);
        // draw level
        this->Levels[this->Level].Draw(*Renderer);
        // draw player
        Player->Draw(*Renderer);
        if (this->State == GAME_WIN)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            Text->RenderText("YOU WON!!!", 320.0f, Height / 2.0f - 20.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
            Text->RenderText("Press ENTER to retry or ESC to quit", 130.0f, Height / 2.0f + 20.0f, 0.5f, glm::vec3(1.0f, 1.0f, 0.0f));
            glDisable(GL_BLEND);
            // Option 1: Load a "youwin.png" texture and display it
            // ResourceManager::LoadTexture("textures/youwin.png", true, "wintext");
            // Renderer->DrawSprite(ResourceManager::GetTexture("wintext"), 
            //     glm::vec2(this->Width / 2.0f - 100, this->Height / 2.0f - 50), 
            //     glm::vec2(200, 100), 0.0f);
            
            // Option 2: Just use console for now (already done above)
        }
    }
}
