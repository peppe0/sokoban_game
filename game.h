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
#include "heart_3d.h"
#include <cmath>

struct ma_engine;
struct ma_sound;
class SphereMesh;

// Pixel coordinate -> grid index. Rounds instead of truncating: a cell is
// window/columns wide and that need not be exact in binary. With 15 columns it was
// (900/15 = 60), but 900/17 is not, so positions built by repeated addition land a
// millionth of a pixel below the exact multiple - and truncation turns 10.0 into 9.
inline int GridIndex(float position, float cellSize)
{
    return (cellSize > 0.0f) ? static_cast<int>(std::floor(position / cellSize + 0.5f)) : 0;
}

// Represents the current state of the game
enum GameState {
    GAME_ACTIVE,
    GAME_MENU,
    GAME_WIN,
    GAME_OVER
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

    // Mouse-driven menu
    double                  MouseX, MouseY;
    bool                    MouseClicked;   // edge-triggered: true for one frame after a left click
    bool                    MenuLevelsOpen;  // whether the level picker under "New Game" is expanded
    bool                    MenuResultsOpen; // whether the "Results" submenu is expanded
    bool                    ResetArmed;      // first click on Reset only arms it; the second wipes the records
    bool                    RequestQuit;     // set when the Exit button is clicked; main loop closes the window

    // Free camera on the middle mouse button (the wheel pressed down), the way a DCC
    // viewport works: drag to orbit the board, Shift+drag to pan, turn the wheel to
    // zoom, H to snap back. Zero on all four values reproduces the fixed shot the
    // scene used to be drawn through, matrix for matrix.
    bool                    MiddleMouseDown;
    bool                    CameraDragStarted;   // false until the first cursor sample of a drag
    double                  CameraLastX, CameraLastY;
    float                   CameraYaw, CameraPitch;  // degrees, orbit around the board centre
    glm::vec2               CameraPan;               // world units, in the camera's own plane
    float                   CameraZoom;              // added to the default 3.0 orbit distance

    glm::vec3 CameraTargetPoint() const;   // what the camera orbits, pan included
    glm::vec3 GetCameraPosition() const;   // eye in world space, for the lighting uniforms
    glm::mat4 GetViewMatrix() const;       // view matrix every 3D scene pass shares
    void ResetCamera();
    void SetMiddleMouse(bool down);                 // wheel pressed / released
    void MoveCameraWithMouse(double x, double y);   // cursor sample; orbits or pans while held
    void ZoomCamera(float ticks);                   // wheel turned

    TextRenderer            *Text; 
    ma_engine               *AudioEngine;
    // The background track. Unlike the effects, which are fire-and-forget one-shots
    // through the engine, this one is a sound object the game holds on to: it has to
    // loop, sit under the effects in volume, and be stopped on the way out.
    ma_sound                *Music;
    // Every effect is played into this group instead of straight into the engine, which
    // is what gives them a volume of their own: engine volume would drag the music with
    // it, and a fire-and-forget play has nowhere to hang a level.
    // (miniaudio makes a group a sound: typedef ma_sound ma_sound_group.)
    ma_sound                *Effects;
    Model3D                 *Box;
    Model3D                 *Box2;
    Model3D                 *Box3;
    Model3D                 *TileModel;
    Model3D                 *WallModel;
    Model3D                 *player;
    Model3D                 *pillar;
    Model3D                 *chest;
    Model3D                 *key;
    Model3D                 *potion;
    Model3D                 *curse;      // tile 10: same bottle mesh, tinted
    Model3D                 *health;     // tile 16: the same bottle again, red
    Model3D                 *lantern;    // tile 11: its own mesh and texture
    Model3D                 *food;       // tile 12: haste
    Model3D                 *coins;      // tile 13: score bonus
    Model3D                 *staff;      // tile 15: the weapon, picked up not consumed
    Model3D                 *grave;      // drawn on every monster spawn; purely decorative
    Model3D                 *spikes;
    Model3D                 *skeleton;
    unsigned int            PlayerHearts;

    // Grid monsters that chase the player one cell per player move.
    struct Monster {
        int   GridX, GridY;        // logical cell it occupies
        int   SpawnX, SpawnY;      // cell to return to after landing a hit
        glm::vec2 VisualOffset;    // pixel offset used while the step animates
        glm::vec2 StepDelta;       // pixel vector of the step in progress
        float MoveTimer, MoveDuration;
        bool  IsMoving;
        float FacingYaw;           // degrees, matches the player's convention
        float AnimTime;            // own animation clock, so they don't march in lockstep
        // A skeleton shot down is not deleted, it is sent back to its grave: this counts
        // the player moves left before it climbs out again. 0 means it is up and hunting.
        int   RespawnIn;
    };
    std::vector<Monster>    Monsters;

    void SpawnMonsters();                                  // (re)build from the current level
    // Shuffles the level's pickups - the staff included - across the cells marked
    // for them. The set of pickups is exactly what the .lvl file contains: only their
    // positions are drawn at random, so two attempts at the same level always offer
    // the same points and the persisted best scores stay comparable.
    void RandomizePickups();
    void StepMonsters(int playerFromX, int playerFromY,    // one monster turn
                      int playerToX, int playerToY);
    
    float                   GameTime;     // used for animations
    float                   ElapsedTime;  // per-level stopwatch (seconds), display only
    unsigned int            Moves;        // successful steps this attempt; drives the score

    // Scoring system
    int                     Score;              // current live score
    int                     BonusPoints;        // collected this attempt (coin stacks)
    std::vector<int>        BestScores;         // best score per level (persisted)

    int  ComputeScore() const;   // calculate score from current state
    void LoadBestScores();       // load from file
    void SaveBestScores();       // save to file

    // Timed effects. Vision is measured in seconds, the freeze in player turns: the
    // skeletons only ever act on a move, so counting their pause in seconds would make
    // it worth more or less depending on how fast you happen to be playing.
    float                   VisionModifier;     // added to the reveal radius: positive from a potion, negative from a curse
    float                   VisionTimer;        // seconds left on whichever vision effect is active
    int                     FreezeTurns;        // player moves the skeletons still have to sit out
    int                     HasteTurns;         // player moves left at double speed
    bool                    HasteSkipNext;      // alternates while hasted, halving the skeletons' rate

    // Death. Losing the last heart no longer flips straight to the game-over panel: the
    // mage plays the rig's own death clip first, and the overlay waits for it to land.
    bool                    PlayerDying;
    float                   DeathTimer;         // seconds into the death clip
    void StartDeath(const char *cause);

    // The staff and what it fires. Picking it up is permanent for the attempt: it is a
    // weapon, not a consumable, so it stays out of the inventory and answers the mouse
    // directly. Only levels three and four carry one.
    bool                    HasStaff;
    float                   CastCooldown;       // seconds before another bolt is allowed
    float                   CastTimer;          // seconds left of the cast animation
    // A click does not fire, it starts the throw: the bolt is held here until the
    // animation reaches the frame the mage actually lets go on.
    bool                    CastPending;
    glm::vec2               CastDirection;      // aimed at click time, fired at release
    struct Bolt {
        glm::vec2 Position;    // board pixels: the space Player->Position lives in
        glm::vec2 Velocity;    // board pixels per second
        float     Life;        // seconds before it fizzles out on its own
        float     Age;         // drives the glow's flicker
    };
    std::vector<Bolt>       Bolts;
    // The bolt's geometry. Every other mesh in the game comes out of a model
    // file through Assimp; this one is generated vertex by vertex at start-up
    // from the parametric equation of the sphere, and owns its own VAO and its
    // own pair of shaders (shaders/orb.vs, shaders/orb.frag).
    SphereMesh              *BoltMesh;

    // Window pixel -> point on the board, through the real camera. False when the cursor
    // is not over the tile plane at all.
    bool ScreenToBoard(double screenX, double screenY, glm::vec2 &board) const;
    void FireBolt(double screenX, double screenY);
    void ReleaseBolt();                 // the throw reached its release frame
    void UpdateBolts(float dt);
    void DrawBolts();

    // Inventory. The bonuses are not spent where they lie: stepping on one stores it,
    // and the effect only fires when the player picks it out of the inventory and uses
    // it. Two things stay out of here on purpose - coins, which are score rather than
    // something you carry, and the curse, which is a trap: nobody would ever choose to
    // drink it, so letting it be carried made it harmless. Both resolve on contact.
    enum ItemSlot { ITEM_POTION = 0, ITEM_LANTERN, ITEM_FOOD, ITEM_HEALTH, ITEM_SLOTS };
    unsigned int            Inventory[ITEM_SLOTS];
    int                     SelectedItem;    // slot the picker's cursor sits on
    bool                    InventoryOpen;   // whether the picker is unfolded (I)

    static int  ItemFromTile(unsigned int tile);  // -1 for tiles that are not carryable
    const char *ItemName(int slot) const;
    const char *ItemEffect(int slot) const;       // one-line "what it does", for the panel
    glm::vec3   ItemColor(int slot) const;        // same ink the debug overlay uses
    bool        UseSelectedItem();                // false when the slot is empty
    void        SelectItem(int slot);
    void        DrawInventory();
    void        DrawInventoryIcons(float centreX, float firstRowY, float rowH);

    // Player facing direction (persists across frames so animations don't reset it)
    float                   PlayerFacingYaw;    // degrees, Y-axis rotation for top-down view
    // Lighting system
    bool                    LightingEnabled;
    // Debug overlay (G): draws the logical TileData grid on top of the scene.
    bool                    ShowGrid;
    void DrawDebugGrid();
    struct Light {
        glm::vec2 position;
        glm::vec3 color;
        float radius;
    };
    std::vector<Light>      Lights;

    // A clickable/hoverable menu entry. action: -2 = New Game (unfolds the level
    // spinner), -3 = Exit, -4 = toggle Results submenu, -5/-6 = previous/next
    // level, -7 = start the selected level, -8 = arm/confirm wiping the records,
    // -100 = informational (not clickable, e.g. a results line or the spinner's
    // own "LEVEL n" readout).
    struct MenuButton {
        std::string label;
        float x, y, w, h;           // hover/click rect (top-left, size)
        float textX, textY, scale;  // where/how to draw the label
        glm::vec3 color;            // label ink colour
        int   action;
    };
    std::vector<MenuButton> BuildMenuButtons(); // shared layout used by both input and render
    float TextWidth(const std::string& txt, float scale) const;

    // Cell size in screen pixels, taken from the current level's own dimensions. The grid
    // used to be hard-coded as 15x8 in four separate places, so changing a level's size
    // silently desynchronised movement, monsters and rendering from one another.
    float CellWidth() const;
    float CellHeight() const;

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
