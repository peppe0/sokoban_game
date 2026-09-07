/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#define MINIAUDIO_IMPLEMENTATION
#include "game.h"
#include "game_object.h"
#include "heart_3d.h"
#include "miniaudio.h"
#include "player_object.h"
#include "resource_manager.h"
#include "sphere_mesh.h"
#include "sprite_renderer.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <utility>

SpriteRenderer *Renderer;
PlayerObject *Player;

// Mesh heights in model units, measured from the assets. Used to sit a
// character's midpoint on the tile plane, since both are laid flat toward the
// camera.
static const float MAGE_MESH_HEIGHT = 2.65f;
static const float SKELETON_MESH_HEIGHT = 2.17f;
// The crate is the one tile model drawn unrotated, so its height stays on the Y
// axis and its base-anchored origin lifts it off the cell centre.
static const float CRATE_MESH_HEIGHT = 1.50f;
static const float GRAVE_MESH_HEIGHT = 2.106f;

// The death animation comes from the rig the mage already loads, so it binds to
// its bones by name like every other clip. Death_B is the long collapse (2.6s);
// Death_A is the same fall in 0.8s if the retry loop ever needs to be tighter.
static const char *const DEATH_CLIP = "Death_B";

// The staff. KayKit's weapon pack exports at 100x the character scale (this
// mesh is 215 units long against the mage's 2.65 tall), hence a scale two
// orders of magnitude below every other pickup's. Its origin is not its middle
// either - the shaft runs from -90 to +125 along its own length - so the centre
// is subtracted when it is laid out on a cell, or it sits a few pixels off it.
static const float STAFF_SCALE = 0.0014f;
static const float STAFF_CENTRE_X = 4.27f;
static const float STAFF_CENTRE_Y = 17.69f;
static const float STAFF_LIE_ANGLE =
    45.0f; // degrees about Z: it lies on the floor
// Held, the staff inherits the mage's own scale through his model matrix, so
// all that is left is the 100x unit difference between the weapon pack and the
// character - and one quarter turn. The hand slot's own axes are not the
// mesh's: the bone sends its local +Y forward and its +Z up, so an uncorrected
// staff is held out flat like a lance. Tipping it 90 degrees about X stands it
// up, butt at 0.15 and tip at 2.30 on a mage 2.65 tall.
static const float STAFF_HELD_SCALE = 0.01f;
static const float STAFF_HELD_TILT = 90.0f;
// The bolts. Speed is in board pixels per second - about ten cells - and the
// hit radius is half a cell, comfortably more than the ~9px a bolt covers
// between two frames, so nothing tunnels through a skeleton.
static const float BOLT_SPEED = 520.0f;
static const float BOLT_LIFETIME = 2.0f;
// The bolt is a real sphere now, so its size is in world units rather than in
// screen pixels: a fraction of a cell, which the draw converts using the same
// 5.5-world-units-per-board-width mapping the tiles are laid out with. The halo
// is the same mesh again, blown up and dimmed.
static const unsigned int BOLT_SPHERE_STACKS = 18;
static const unsigned int BOLT_SPHERE_SECTORS = 36;
static const float BOLT_CORE_CELLS = 0.23f;
static const float BOLT_HALO_SCALE = 2.10f;
// The throw is played faster than its own 1.367s, which is what makes casting
// feel quick without going back to firing during the wind-up: the release point
// moves with the clip. At 1.6x the bolt leaves 0.44s after the click instead of
// 0.70s.
static const float CAST_PLAYBACK_SPEED = 1.6f;
static const char *const CAST_CLIP = "Throw";
// Where in the Throw clip the mage actually lets go. Measured off the animation
// rather than guessed: handslot.r winds back and up for the first half (hand
// speed ~2.6), then whips forward through a peak of 13.6 at 0.63s and reaches
// full extension at 0.80s. The release is the moment just past that peak, 0.70s
// into a 1.367s clip.
static const float CAST_RELEASE_FRACTION = 0.51f;
// The bolt leaves from the staff rather than the middle of the mage, this far
// along the firing direction, in cells.
static const float CAST_MUZZLE_CELLS = 0.40f;
// A skeleton put down by a bolt climbs back out of its grave this many player
// moves later. Counted in moves, not seconds, for the reason the freeze is: the
// skeletons only ever act on a move, so a stopwatch would make the reprieve
// worth more or less depending on how fast the level happens to be played.
static const int MONSTER_RESPAWN_TURNS = 10;
// Spawn cells are drawn per attempt, but never within this many cells of where the
// player starts: a skeleton placed on his doorstep reaches him before he has had a
// turn. Measured as Manhattan distance, which is never longer than the real path, so
// it is a floor on how far the thing actually has to walk.
static const int MONSTER_SPAWN_MIN_DISTANCE = 5;
// Background music level. The effects play at full volume through the engine,
// so the track has to sit well below them to stay background.
static const float MUSIC_VOLUME = 0.25f;
// And the level of everything else: pickups, the victory sting, the staff, the
// crates. 1.0 is the volume they have always played at; lower it to sit them
// nearer the music.
static const float EFFECTS_VOLUME = 0.7f;

Game::Game(unsigned int width, unsigned int height)
    : State(GAME_MENU), Keys(), Width(width), Height(height), MouseX(0.0),
      MouseY(0.0), MouseClicked(false), MenuLevelsOpen(false),
      MenuResultsOpen(false), ResetArmed(false), RequestQuit(false),
      Text(nullptr), AudioEngine(nullptr), Music(nullptr), Effects(nullptr),
      Box(nullptr), Box2(nullptr), Box3(nullptr), TileModel(nullptr),
      WallModel(nullptr), player(nullptr), pillar(nullptr), chest(nullptr),
      key(nullptr), potion(nullptr), curse(nullptr), health(nullptr),
      lantern(nullptr), food(nullptr), coins(nullptr), staff(nullptr),
      grave(nullptr), spikes(nullptr), skeleton(nullptr), PlayerHearts(3),
      GameTime(0.0f), ElapsedTime(0.0f), Moves(0), Score(0), BonusPoints(0),
      VisionModifier(0.0f), VisionTimer(0.0f), FreezeTurns(0), HasteTurns(0),
      HasteSkipNext(false), PlayerFacingYaw(0.0f) {
  for (int i = 0; i < 1024; ++i)
    this->KeysProcessed[i] = false;
  for (int i = 0; i < ITEM_SLOTS; ++i)
    this->Inventory[i] = 0;
  this->SelectedItem = 0;
  this->InventoryOpen = false;
  this->PlayerDying = false;
  this->DeathTimer = 0.0f;
  this->HasStaff = false;
  this->CastCooldown = 0.0f;
  this->CastTimer = 0.0f;
  this->CastPending = false;
  this->CastDirection = glm::vec2(0.0f);
  this->BoltMesh = nullptr; // built in Init(), once there is a GL context
  this->ResetCamera();
  this->MiddleMouseDown = false;
  this->CameraDragStarted = false;
  this->CameraLastX = this->CameraLastY = 0.0;
  LoadBestScores();
}

// --- Free camera
// -----------------------------------------------------------------
//
// The scene was drawn through a hard-coded view, translate(-2, 1, -3): an eye
// parked at (2, -1, 3) staring down -Z at the middle of the board. That stays
// the rest pose. Yaw, pitch, pan and zoom all at zero make lookAt() produce
// exactly that matrix, so an untouched camera frames the level the way it
// always did.
static const glm::vec3 CAMERA_TARGET(2.0f, -1.0f, 0.0f);
static const float CAMERA_DISTANCE = 3.0f;
static const float CAMERA_FOV = 45.0f;
// Pitch stops short of straight up/down: at the poles the up vector collapses
// and lookAt() degenerates. The board is a plane of flat-laid sprites, so
// grazing angles are already the least useful part of the range.
static const float CAMERA_PITCH_LIMIT = 85.0f;
static const float CAMERA_MIN_DISTANCE = 1.0f;
static const float CAMERA_MAX_DISTANCE = 12.0f;
static const float CAMERA_ORBIT_SPEED = 0.30f; // degrees per pixel dragged

// Direction from the orbit target to the eye. Zero yaw and pitch give +Z, the
// axis the original fixed camera sat on.
static glm::vec3 CameraOffsetDir(float yawDeg, float pitchDeg) {
  const float yaw = glm::radians(yawDeg);
  const float pitch = glm::radians(pitchDeg);
  return glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                   std::cos(pitch) * std::cos(yaw));
}

// Camera-plane axes, used to pan in the direction the cursor actually moves.
static void CameraAxes(float yawDeg, float pitchDeg, glm::vec3 &right,
                       glm::vec3 &up) {
  const glm::vec3 front = -CameraOffsetDir(yawDeg, pitchDeg);
  right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
  up = glm::cross(right, front);
}

glm::vec3 Game::CameraTargetPoint() const {
  glm::vec3 right, up;
  CameraAxes(this->CameraYaw, this->CameraPitch, right, up);
  return CAMERA_TARGET + right * this->CameraPan.x + up * this->CameraPan.y;
}

glm::vec3 Game::GetCameraPosition() const {
  const float distance = CAMERA_DISTANCE + this->CameraZoom;
  return this->CameraTargetPoint() +
         CameraOffsetDir(this->CameraYaw, this->CameraPitch) * distance;
}

glm::mat4 Game::GetViewMatrix() const {
  return glm::lookAt(this->GetCameraPosition(), this->CameraTargetPoint(),
                     glm::vec3(0.0f, 1.0f, 0.0f));
}

void Game::ResetCamera() {
  this->CameraYaw = 0.0f;
  this->CameraPitch = 0.0f;
  this->CameraPan = glm::vec2(0.0f);
  this->CameraZoom = 0.0f;
}

void Game::SetMiddleMouse(bool down) {
  this->MiddleMouseDown = down;
  // Each press starts a fresh drag: the first cursor sample only seeds the
  // origin, otherwise the camera would jump by however far the mouse travelled
  // unpressed.
  this->CameraDragStarted = false;
}

void Game::MoveCameraWithMouse(double x, double y) {
  if (!this->MiddleMouseDown)
    return;
  if (!this->CameraDragStarted) {
    this->CameraLastX = x;
    this->CameraLastY = y;
    this->CameraDragStarted = true;
    return;
  }

  const float dx = static_cast<float>(x - this->CameraLastX);
  const float dy = static_cast<float>(y - this->CameraLastY);
  this->CameraLastX = x;
  this->CameraLastY = y;

  const bool panning =
      this->Keys[GLFW_KEY_LEFT_SHIFT] || this->Keys[GLFW_KEY_RIGHT_SHIFT];
  if (panning) {
    // World units per pixel at the target's depth, so the board tracks the
    // cursor instead of sliding faster or slower as you zoom.
    const float distance = CAMERA_DISTANCE + this->CameraZoom;
    const float unitsPerPixel = 2.0f * distance *
                                std::tan(glm::radians(CAMERA_FOV) * 0.5f) /
                                static_cast<float>(this->Height);
    this->CameraPan.x -= dx * unitsPerPixel; // drag right, board follows right
    this->CameraPan.y += dy * unitsPerPixel;
  } else {
    this->CameraYaw -= dx * CAMERA_ORBIT_SPEED;
    this->CameraPitch += dy * CAMERA_ORBIT_SPEED;
    if (this->CameraPitch > CAMERA_PITCH_LIMIT)
      this->CameraPitch = CAMERA_PITCH_LIMIT;
    if (this->CameraPitch < -CAMERA_PITCH_LIMIT)
      this->CameraPitch = -CAMERA_PITCH_LIMIT;
    if (this->CameraYaw > 180.0f)
      this->CameraYaw -= 360.0f;
    if (this->CameraYaw < -180.0f)
      this->CameraYaw += 360.0f;
  }
}

void Game::ZoomCamera(float ticks) {
  const float distance = CAMERA_DISTANCE + this->CameraZoom - ticks * 0.25f;
  const float clamped = (distance < CAMERA_MIN_DISTANCE)   ? CAMERA_MIN_DISTANCE
                        : (distance > CAMERA_MAX_DISTANCE) ? CAMERA_MAX_DISTANCE
                                                           : distance;
  this->CameraZoom = clamped - CAMERA_DISTANCE;
}

Game::~Game() {
  delete Renderer;
  delete Player;
  delete Text;
  delete Box;
  delete Box2;
  delete Box3;
  delete TileModel;
  delete WallModel;
  delete player;
  delete pillar;
  delete chest;
  delete key;
  delete potion;
  delete curse;
  delete health;
  delete lantern;
  delete staff;
  delete grave;
  delete food;
  delete coins;
  delete spikes;
  delete skeleton;
  delete BoltMesh;

  // Both go before the engine they are attached to.
  if (Effects) {
    ma_sound_group_uninit(Effects);
    delete Effects;
  }
  if (Music) {
    ma_sound_uninit(Music);
    delete Music;
  }
  if (AudioEngine) {
    ma_engine_uninit(AudioEngine);
    delete AudioEngine;
  }
}

// ---------------------------------------------------------------------------
// Scoring helpers
// ---------------------------------------------------------------------------

// Formula:
//   base = 10000
//   move penalty = 50 points per move actually taken
//   heart bonus  = 800 per heart remaining
//   collected    = coin stacks picked up this attempt
//   minimum score = 0
//
// A heart was worth 2000, i.e. 40 moves, which made avoiding damage the right
// answer to every question and left no room for a real gamble. At 800 it is
// worth 16 moves, less than a coin stack, so a risky detour can now pay off.
//
// Scoring is on efficiency, not speed: a Sokoban is solved by thinking, so the
// clock keeps running for display only and never costs points. Bumping into a
// wall is not a move and is not charged.
int Game::ComputeScore() const {
  const int base = 10000;
  const int movePenalty = static_cast<int>(Moves) * 50;
  const int heartBonus = static_cast<int>(PlayerHearts) * 800;
  return std::max(0, base - movePenalty + heartBonus + BonusPoints);
}

void Game::LoadBestScores() {
  BestScores.assign(4, 0); // one slot per level (0-3)
  std::ifstream f("scores.dat");
  if (!f.is_open())
    return;
  for (int i = 0; i < 4; ++i) {
    int val = 0;
    if (f >> val)
      BestScores[i] = val;
  }
}

void Game::SaveBestScores() {
  std::ofstream f("scores.dat");
  if (!f.is_open())
    return;
  for (int i = 0; i < 4; ++i)
    f << BestScores[i] << "\n";
}

// ---------------------------------------------------------------------------

void Game::Init() {
  // Initialize audio engine
  AudioEngine = new ma_engine();
  ma_result result = ma_engine_init(NULL, AudioEngine);
  if (result != MA_SUCCESS) {
    std::cout << "Failed to initialize audio engine." << std::endl;
  }

  // One group for every effect, so they can be levelled without touching the
  // music.
  if (result == MA_SUCCESS) {
    Effects = new ma_sound(); // a group is a sound in miniaudio
    if (ma_sound_group_init(AudioEngine, 0, NULL, Effects) == MA_SUCCESS) {
      ma_sound_group_set_volume(Effects, EFFECTS_VOLUME);
    } else {
      std::cout << "Could not create the effects group." << std::endl;
      delete Effects;
      Effects = nullptr;
    }
  }

  // Background music. Streamed rather than decoded up front - it is 2.4MB of
  // mp3 against the few kilobytes of the effects - and looped for the life of
  // the process, menu included. A quarter volume keeps it under the effects
  // instead of over them.
  if (result == MA_SUCCESS) {
    Music = new ma_sound();
    if (ma_sound_init_from_file(AudioEngine, "sounds/background.mp3",
                                MA_SOUND_FLAG_STREAM, NULL, NULL,
                                Music) == MA_SUCCESS) {
      ma_sound_set_looping(Music, MA_TRUE);
      ma_sound_set_volume(Music, MUSIC_VOLUME);
      ma_sound_start(Music);
    } else {
      std::cout << "Could not open sounds/background.mp3" << std::endl;
      delete Music;
      Music = nullptr;
    }
  }

  // Initialize lighting
  LightingEnabled = true;
  ShowGrid = false;

  ResourceManager::LoadShader("shaders/sprite.vs", "shaders/sprite.frag",
                              nullptr, "sprite");
  // configure shaders
  glm::mat4 projection =
      glm::ortho(0.0f, static_cast<float>(this->Width),
                 static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
  ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
  ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);

  // Setup lighting
  ResourceManager::GetShader("sprite").SetInteger("enableLighting",
                                                  LightingEnabled);
  ResourceManager::GetShader("sprite").SetFloat("ambientStrength",
                                                0.1f); // Darker ambient

  // set render-specific controls
  Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"));
  // load textures
  ResourceManager::LoadTexture("textures/wood.jpg", false, "background");
  ResourceManager::LoadTexture("textures/awesomeface.png", true, "face");
  ResourceManager::LoadTexture("textures/block.png", false, "block");
  ResourceManager::LoadTexture("textures/block_solid.png", false,
                               "block_solid");
  ResourceManager::LoadTexture("textures/paddle.png", true, "paddle");
  ResourceManager::LoadTexture("textures/player.png", false, "player");
  ResourceManager::LoadTexture("textures/vignette.png", true, "vignette");
  ResourceManager::LoadTexture("textures/Menu.png", true, "menu_bg");
  // Menu button plaques, sliced into left cap / stretchable middle / right cap
  // so a button keeps crisp rounded corners at any width.
  ResourceManager::LoadTexture("textures/btn_l.png", true, "btn_l");
  ResourceManager::LoadTexture("textures/btn_m.png", true, "btn_m");
  ResourceManager::LoadTexture("textures/btn_r.png", true, "btn_r");
  ResourceManager::LoadTexture("textures/btn_hover_l.png", true, "btn_hover_l");
  ResourceManager::LoadTexture("textures/btn_hover_m.png", true, "btn_hover_m");
  ResourceManager::LoadTexture("textures/btn_hover_r.png", true, "btn_hover_r");
  // Win/over overlay: a dimming scrim plus a parchment panel matching the menu.
  ResourceManager::LoadTexture("textures/panel.png", true, "panel");
  ResourceManager::LoadTexture("textures/scrim.png", true, "scrim");
  ResourceManager::LoadTexture("textures/debug_px.png", true,
                               "debug_px"); // flat white, tinted per use
  ResourceManager::LoadTexture("textures/magic_orb.png", true,
                               "magic_orb"); // radial glow, tinted per bolt

  // Keep background tiles crisp to avoid sampling artifacts at tile edges.
  Texture2D backgroundTexture = ResourceManager::GetTexture("background");
  glBindTexture(GL_TEXTURE_2D, backgroundTexture.ID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  // load levels
  GameLevel one;
  one.Load("levels/one.lvl", this->Width, this->Height);
  GameLevel two;
  two.Load("levels/two.lvl", this->Width, this->Height);
  GameLevel three;
  three.Load("levels/three.lvl", this->Width, this->Height);
  GameLevel four;
  four.Load("levels/four.lvl", this->Width, this->Height);
  this->Levels.push_back(one);
  this->Levels.push_back(two);
  this->Levels.push_back(three);
  this->Levels.push_back(four);
  this->Level = 0;

  glm::vec2 playerPos =
      this->Levels[this->Level].PlayerStartPos; // Assuming 50x50 tiles
  glm::vec2 playerSize = glm::vec2(CellWidth(), CellHeight());
  Player = new PlayerObject(playerPos, playerSize,
                            ResourceManager::GetTexture("player"));
  Text = new TextRenderer(this->Width, this->Height);
  Text->Load("fonts/GeorgiaBold.ttf", 24);

  ResourceManager::LoadShader("shaders/box.vs", "shaders/box.frag", nullptr,
                              "box");
  // The magic bolt: its own shader pair and its own mesh, the one piece of
  // geometry in the game that is computed instead of loaded.
  ResourceManager::LoadShader("shaders/orb.vs", "shaders/orb.frag", nullptr,
                              "orb");
  BoltMesh = new SphereMesh(BOLT_SPHERE_STACKS, BOLT_SPHERE_SECTORS);
  std::cout << "Built procedural bolt sphere: " << BoltMesh->VertexCount()
            << " vertices, " << BoltMesh->IndexCount() / 3 << " triangles"
            << std::endl;
  Box = new Model3D("resources/love_heart.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.09f), glm::vec3(1.0f, 0.1f, 0.3f));
  Box2 = new Model3D("resources/love_heart.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                     glm::vec3(0.09f), glm::vec3(1.0f, 0.1f, 0.3f));
  Box3 = new Model3D("resources/love_heart.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                     glm::vec3(0.09f), glm::vec3(1.0f, 0.1f, 0.3f));
  TileModel =
      new Model3D("resources/box_large.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.12f), glm::vec3(1.0f));
  WallModel = new Model3D("resources/wall.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                          glm::vec3(0.18f), glm::vec3(1.0f));
  // The .glb is the rigged mage; mago.obj is the same character but OBJ cannot
  // carry bones, so it could never be animated.
  player = new Model3D("resources/Mage.glb", glm::vec3(0.0f, 0.0f, 0.0f),
                       glm::vec3(0.15f), glm::vec3(1.0f));
  pillar = new Model3D("resources/pillar.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                       glm::vec3(0.28f), glm::vec3(1.0f));
  chest = new Model3D("resources/chest.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                      glm::vec3(0.12f), glm::vec3(1.0f));
  key = new Model3D("resources/key.obj", glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.12f), glm::vec3(1.0f));
  potion = new Model3D("resources/bottle_A_labeled_green.obj",
                       glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.12f),
                       glm::vec3(1.0f));
  // The curse shares the potion's bottle mesh, told apart by tint.
  curse = new Model3D("resources/bottle_A_labeled_green.obj", glm::vec3(0.0f),
                      glm::vec3(0.12f), glm::vec3(0.62f, 0.28f, 0.95f));
  // The health potion is the same bottle a third time, red, so the three read
  // as one family of flasks told apart by colour.
  health = new Model3D("resources/bottle_A_labeled_green.obj", glm::vec3(0.0f),
                       glm::vec3(0.12f), glm::vec3(1.0f, 0.30f, 0.35f));
  // The lantern has a mesh and texture of its own, so it renders untinted.
  lantern = new Model3D("resources/lantern_standing.obj", glm::vec3(0.0f),
                        glm::vec3(0.20f), glm::vec3(1.0f));
  grave = new Model3D("resources/grave_A_destroyed.obj", glm::vec3(0.0f),
                      glm::vec3(0.115f), glm::vec3(1.0f));
  food = new Model3D("resources/plate_food_A.obj", glm::vec3(0.0f),
                     glm::vec3(0.18f), glm::vec3(1.0f));
  coins = new Model3D("resources/coin_stack_large.obj", glm::vec3(0.0f),
                      glm::vec3(0.12f), glm::vec3(1.0f));
  // Textured from textures/mage_texture.png: the .fbx points at the exporting
  // machine's Windows path, which the loader falls back from to the bare file
  // name.
  staff = new Model3D("resources/staff.fbx", glm::vec3(0.0f),
                      glm::vec3(STAFF_SCALE), glm::vec3(1.0f));
  spikes = new Model3D("resources/floor_tile_big_spikes_spikes.obj",
                       glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.12f),
                       glm::vec3(1.0f));
  // Textured from textures/skeleton_texture.png (the .fbx points at the
  // exporting machine's absolute path, so the loader falls back to the bare
  // file name).
  skeleton = new Model3D("resources/Skeleton_Minion.fbx", glm::vec3(0.0f),
                         glm::vec3(0.14f), glm::vec3(1.0f));
  // The mesh ships without clips; KayKit keeps them in shared Rig_Medium files
  // that drive the very same 23 bones, so they bind to this skeleton by name.
  skeleton->LoadAnimations("resources/Rig_Medium_MovementBasic.fbx");
  skeleton->LoadAnimations("resources/Rig_Medium_General.fbx");
  skeleton->SetBindPose();
  // The Adventurers and Skeletons packs share the very same Rig_Medium bone
  // names, so the mage drives off the same two clip files.
  player->LoadAnimations("resources/Rig_Medium_MovementBasic.fbx");
  player->LoadAnimations("resources/Rig_Medium_General.fbx");
  player->SetBindPose();
  ResourceManager::LoadShader("shaders/skinned.vs", "shaders/box.frag", nullptr,
                              "skinned");
  player->RotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);
  player->Rotation = 90.0f;

  // Pickups first: SpawnMonsters only takes cells that are still plain floor, so it
  // has to see the board the attempt will actually be played on.
  RandomizePickups();
  SpawnMonsters();
}

// One generator for the whole game, seeded once, so every level start draws a
// fresh layout instead of repeating the same "random" one.
static std::mt19937 &PickupRng() {
  static std::mt19937 rng(static_cast<unsigned int>(
      std::chrono::steady_clock::now().time_since_epoch().count()));
  return rng;
}

void Game::RandomizePickups() {
  if (this->Level >= this->Levels.size())
    return;
  GameLevel &level = this->Levels[this->Level];
  auto &tiles = level.TileData;
  const int rows = static_cast<int>(tiles.size());
  const int cols = rows > 0 ? static_cast<int>(tiles[0].size()) : 0;
  if (rows == 0 || cols == 0)
    return;

  const float stepX = CellWidth();
  const float stepY = CellHeight();

  // The staff (15) rides along with the consumables. It is not an inventory
  // item - it arms the mouse for the rest of the attempt - but as far as this
  // function is concerned it is one more thing lying on the floor, and drawing
  // its cell at random keeps the two levels that carry it from opening the same
  // way every time.
  auto isPickup = [](unsigned int t) {
    return t == 8 || t == 10 || t == 11 || t == 12 || t == 13 || t == 15 ||
           t == 16;
  };

  // What the level ships with, and where those pickups currently sit.
  std::vector<unsigned int> pickups;
  std::vector<std::pair<int, int>> occupied;
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      if (isPickup(tiles[y][x])) {
        pickups.push_back(tiles[y][x]);
        occupied.push_back({x, y});
        tiles[y][x] = 0;
      }
    }
  }
  if (pickups.empty())
    return;

  // Candidate cells: the ones they came from, plus the slots marked with
  // tile 14.
  std::vector<std::pair<int, int>> pool = occupied;
  for (const glm::vec2 &slot : level.PickupSlots)
    pool.push_back({GridIndex(slot.x, stepX), GridIndex(slot.y, stepY)});

  // Retire the bricks left on the old cells; new ones follow for the chosen
  // cells.
  for (GameObject &brick : level.Bricks) {
    if (brick.IsSolid || brick.Destroyed)
      continue;
    const std::pair<int, int> cell{GridIndex(brick.Position.x, stepX),
                                   GridIndex(brick.Position.y, stepY)};
    if (std::find(occupied.begin(), occupied.end(), cell) != occupied.end())
      brick.Destroyed = true;
  }

  std::shuffle(pool.begin(), pool.end(), PickupRng());
  std::shuffle(pickups.begin(), pickups.end(), PickupRng());

  const size_t placed = std::min(pickups.size(), pool.size());
  for (size_t i = 0; i < placed; ++i) {
    const int x = pool[i].first, y = pool[i].second;
    if (x < 0 || y < 0 || x >= cols || y >= rows || tiles[y][x] != 0)
      continue; // a slot overlapping something solid is simply skipped
    tiles[y][x] = pickups[i];
    level.Bricks.push_back(
        GameObject(glm::vec2(x * stepX, y * stepY), glm::vec2(stepX, stepY),
                   ResourceManager::GetTexture("block"), glm::vec3(1.0f)));
  }
}

void Game::SpawnMonsters() {
  Monsters.clear();
  if (this->Level >= this->Levels.size())
    return;

  GameLevel &level = this->Levels[this->Level];
  const float stepX = CellWidth();
  const float stepY = CellHeight();
  const auto &tiles = level.TileData;
  const int rows = static_cast<int>(tiles.size());
  const int cols = rows > 0 ? static_cast<int>(tiles[0].size()) : 0;

  // The 9s in the level file are only a head count now; which cells the skeletons come
  // out of is drawn fresh each attempt, the way the pickups already are. Two conditions
  // make a cell eligible, and both matter: far enough from the player to give him a
  // turn, and actually connected to him, so a spawn can never be sealed inside a pocket
  // where the skeleton would sit out the whole level.
  const int playerX = GridIndex(level.PlayerStartPos.x, stepX);
  const int playerY = GridIndex(level.PlayerStartPos.y, stepY);

  // Flood fill from the player over the cells a skeleton may walk - the same set
  // StepMonsters uses, so reachable here means reachable in play.
  std::vector<bool> reachable(
      static_cast<size_t>(rows * cols > 0 ? rows * cols : 0), false);
  if (cols > 0 && playerX >= 0 && playerX < cols && playerY >= 0 &&
      playerY < rows) {
    auto monsterWalkable = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= cols || y >= rows)
        return false;
      const unsigned int t = tiles[y][x];
      return t == 0 || t == 6 || t == 8;
    };
    std::vector<std::pair<int, int>> frontier{{playerX, playerY}};
    reachable[static_cast<size_t>(playerY * cols + playerX)] = true;
    const int dirX[4] = {1, -1, 0, 0};
    const int dirY[4] = {0, 0, 1, -1};
    while (!frontier.empty()) {
      const std::pair<int, int> cell = frontier.back();
      frontier.pop_back();
      for (int d = 0; d < 4; ++d) {
        const int nx = cell.first + dirX[d], ny = cell.second + dirY[d];
        if (!monsterWalkable(nx, ny))
          continue;
        const size_t idx = static_cast<size_t>(ny * cols + nx);
        if (reachable[idx])
          continue;
        reachable[idx] = true;
        frontier.push_back({nx, ny});
      }
    }
  }

  std::vector<std::pair<int, int>> pool;
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      if (tiles[y][x] != 0)
        continue; // plain floor only: never under a crate, a pickup or the chest
      if (!reachable[static_cast<size_t>(y * cols + x)])
        continue;
      const int dx = (x > playerX) ? x - playerX : playerX - x;
      const int dy = (y > playerY) ? y - playerY : playerY - y;
      if (dx + dy < MONSTER_SPAWN_MIN_DISTANCE)
        continue;
      pool.push_back({x, y});
    }
  }
  std::shuffle(pool.begin(), pool.end(), PickupRng());

  size_t nextCell = 0;
  for (const glm::vec2 &spawn : level.MonsterSpawns) {
    Monster m;
    if (nextCell < pool.size()) {
      m.GridX = m.SpawnX = pool[nextCell].first;
      m.GridY = m.SpawnY = pool[nextCell].second;
      ++nextCell;
    } else {
      // Nothing eligible left (a tiny or heavily walled level): fall back to the
      // cell the file asked for, so a level never loses a skeleton to the shuffle.
      m.GridX = m.SpawnX = GridIndex(spawn.x, stepX);
      m.GridY = m.SpawnY = GridIndex(spawn.y, stepY);
    }
    m.VisualOffset = glm::vec2(0.0f);
    m.StepDelta = glm::vec2(0.0f);
    m.MoveTimer = 0.0f;
    // Kept in step with the player's own 0.28s: the skeletons move on the same
    // turn he does, and a slide that ends early just leaves them standing there
    // idling while he is still walking.
    m.MoveDuration = 0.25f;
    m.IsMoving = false;
    m.FacingYaw = 0.0f;
    m.AnimTime =
        0.37f * Monsters.size(); // stagger the idle so they look independent
    m.RespawnIn = 0;             // alive
    Monsters.push_back(m);
  }
}

void Game::StepMonsters(int playerFromX, int playerFromY, int playerToX,
                        int playerToY) {
  // Graves count down before anything else, and whatever the living are doing:
  // a lantern freezes the skeletons on their feet, it does not hold the dead
  // ones under.
  for (Monster &m : Monsters) {
    if (m.RespawnIn <= 0)
      continue;
    if (m.RespawnIn == 1) {
      // Never let one climb out from under the player's own feet, or on top of
      // a skeleton that is already standing there. It just waits another turn.
      bool blocked = (m.SpawnX == playerToX && m.SpawnY == playerToY);
      for (const Monster &other : Monsters)
        if (&other != &m && other.RespawnIn == 0 && other.GridX == m.SpawnX &&
            other.GridY == m.SpawnY)
          blocked = true;
      if (blocked)
        continue;
    }
    if (--m.RespawnIn == 0) {
      m.GridX = m.SpawnX;
      m.GridY = m.SpawnY;
      m.IsMoving = false;
      m.VisualOffset = glm::vec2(0.0f);
      m.StepDelta = glm::vec2(0.0f);
    }
  }

  // The lantern buys turns, not seconds: burn one and let the skeletons stand
  // still.
  if (this->FreezeTurns > 0) {
    this->FreezeTurns--;
    return;
  }
  // Food does not speed the player up, it slows the skeletons to every other
  // turn, which is the same thing on a grid and keeps every move worth exactly
  // one step.
  if (this->HasteTurns > 0) {
    this->HasteTurns--;
    this->HasteSkipNext = !this->HasteSkipNext;
    if (this->HasteSkipNext)
      return;
  }
  if (Monsters.empty() || this->Level >= this->Levels.size())
    return;

  const auto &tiles = this->Levels[this->Level].TileData;
  const int rows = static_cast<int>(tiles.size());
  const int cols = rows > 0 ? static_cast<int>(tiles[0].size()) : 0;
  if (rows == 0 || cols == 0)
    return;

  // Monsters walk on floor, spikes and potions; walls, boxes, the chest and the
  // key block them.
  auto walkable = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= cols || y >= rows)
      return false;
    unsigned int t = tiles[y][x];
    return t == 0 || t == 6 || t == 8;
  };
  // The player's own cell is always enterable, whatever it holds: standing on
  // the chest (tile 3) or on the untouched spawn tile (4) must not make you
  // untouchable.
  auto enterable = [&](int x, int y) {
    return walkable(x, y) || (x == playerToX && y == playerToY);
  };

  // Breadth-first flood fill outward from the player: dist[y][x] is the number
  // of steps to reach the player, so a monster just walks downhill. Over 15x8
  // cells this costs nothing and, unlike a greedy chase, it never gets stuck
  // behind a wall.
  const int UNREACHABLE = std::numeric_limits<int>::max();
  std::vector<int> dist(static_cast<size_t>(rows * cols), UNREACHABLE);
  std::vector<std::pair<int, int>> frontier, nextFrontier;
  if (playerToX >= 0 && playerToX < cols && playerToY >= 0 &&
      playerToY < rows) {
    dist[static_cast<size_t>(playerToY * cols + playerToX)] = 0;
    frontier.push_back({playerToX, playerToY});
  }
  const int dirX[4] = {1, -1, 0, 0};
  const int dirY[4] = {0, 0, 1, -1};
  int depth = 0;
  while (!frontier.empty()) {
    ++depth;
    nextFrontier.clear();
    for (const auto &cell : frontier) {
      for (int d = 0; d < 4; ++d) {
        int nx = cell.first + dirX[d], ny = cell.second + dirY[d];
        if (!walkable(nx, ny))
          continue;
        size_t idx = static_cast<size_t>(ny * cols + nx);
        if (dist[idx] != UNREACHABLE)
          continue;
        dist[idx] = depth;
        nextFrontier.push_back({nx, ny});
      }
    }
    frontier.swap(nextFrontier);
  }

  // Cells already claimed this turn, so two monsters never stack on one tile.
  std::vector<bool> claimed(static_cast<size_t>(rows * cols), false);
  for (const Monster &m : Monsters)
    if (m.RespawnIn == 0)
      claimed[static_cast<size_t>(m.GridY * cols + m.GridX)] = true;

  const float stepX = CellWidth();
  const float stepY = CellHeight();
  bool playerHit = false;

  for (Monster &m : Monsters) {
    if (m.RespawnIn > 0)
      continue; // in its grave: it neither moves nor catches
    const int fromX = m.GridX, fromY = m.GridY;

    // Pick the neighbouring cell closest to the player.
    int bestX = fromX, bestY = fromY;
    int bestDist = dist[static_cast<size_t>(fromY * cols + fromX)];
    for (int d = 0; d < 4; ++d) {
      int nx = fromX + dirX[d], ny = fromY + dirY[d];
      if (!enterable(nx, ny))
        continue;
      size_t idx = static_cast<size_t>(ny * cols + nx);
      if (claimed[idx])
        continue;
      if (dist[idx] < bestDist) {
        bestDist = dist[idx];
        bestX = nx;
        bestY = ny;
      }
    }

    if (bestX != fromX || bestY != fromY) {
      claimed[static_cast<size_t>(fromY * cols + fromX)] = false;
      claimed[static_cast<size_t>(bestY * cols + bestX)] = true;
      m.GridX = bestX;
      m.GridY = bestY;
      m.StepDelta = glm::vec2((bestX - fromX) * stepX, (bestY - fromY) * stepY);
      m.VisualOffset = -m.StepDelta; // start behind and slide into place
      m.MoveTimer = 0.0f;
      m.IsMoving = true;
      m.FacingYaw = glm::degrees(std::atan2(static_cast<float>(bestX - fromX),
                                            static_cast<float>(bestY - fromY)));
    }

    // Landing on the player counts as a hit, and so does crossing straight
    // through them: without the swap test a head-on monster passes harmlessly.
    const bool landedOnPlayer = (m.GridX == playerToX && m.GridY == playerToY);
    const bool swappedWithPlayer =
        (fromX == playerToX && fromY == playerToY && m.GridX == playerFromX &&
         m.GridY == playerFromY);
    if (landedOnPlayer || swappedWithPlayer) {
      playerHit = true;
      m.GridX = m.SpawnX; // send it home so it can't drain every heart at once
      m.GridY = m.SpawnY;
      m.IsMoving = false;
      m.VisualOffset = glm::vec2(0.0f);
    }
  }

  if (playerHit && this->PlayerHearts > 0) {
    this->PlayerHearts -= 1;
    if (this->PlayerHearts == 0)
      this->StartDeath("Caught by a skeleton.");
  }
}

// Losing the last heart starts the animation instead of ending the run on the
// spot. State stays GAME_ACTIVE while the clip plays - that is what keeps the
// scene drawn and the mage posed - and Update flips to GAME_OVER once it lands.
void Game::StartDeath(const char *cause) {
  if (this->PlayerDying)
    return;
  this->PlayerDying = true;
  this->DeathTimer = 0.0f;
  this->CastPending = false; // a throw interrupted by dying never lands
  this->CastTimer = 0.0f;
  if (Player) {
    // Land the step that killed him before collapsing. Left animating, the mage
    // would keep sliding toward the next cell underneath the death clip.
    Player->Position = Player->MoveTargetPosition;
    Player->VisualOffset = glm::vec2(0.0f);
    Player->IsMoving = false;
  }
  std::cout << "GAME OVER! " << cause << std::endl;
}

void Game::Update(float dt) {
  // Update game time for animations
  GameTime += dt;

  // Update per-level stopwatch only while playing
  if (this->State == GAME_ACTIVE && this->PlayerDying) {
    // The stopwatch and the timed effects stop the moment the last heart goes:
    // the death animation is not play time. The panel waits for the clip to
    // finish.
    this->DeathTimer += dt;
    const float duration =
        player ? player->AnimationDuration(DEATH_CLIP) : 0.0f;
    if (this->DeathTimer >= duration)
      this->State = GAME_OVER;
  } else if (this->State == GAME_ACTIVE) {
    ElapsedTime += dt;
    if (this->CastCooldown > 0.0f)
      this->CastCooldown = std::max(0.0f, this->CastCooldown - dt);
    if (this->CastTimer > 0.0f)
      this->CastTimer = std::max(0.0f, this->CastTimer - dt);
    if (this->CastPending && player) {
      const float castLength =
          player->AnimationDuration(CAST_CLIP) / CAST_PLAYBACK_SPEED;
      const float elapsed =
          castLength - this->CastTimer; // CastTimer counts down
      if (castLength <= 0.0f || elapsed >= castLength * CAST_RELEASE_FRACTION)
        ReleaseBolt();
    }
    UpdateBolts(dt);
    // Tick vision-potion buff countdown
    if (VisionTimer > 0.0f) {
      VisionTimer -= dt;
      if (VisionTimer <= 0.0f) {
        VisionTimer = 0.0f;
        VisionModifier = 0.0f;
      }
    }
  }

  if (Player) {
    Player->UpdateAnimation(dt);
  }

  // Slide monsters from their previous cell into the one they just claimed.
  for (Monster &m : Monsters) {
    m.AnimTime += dt;
    if (!m.IsMoving)
      continue;
    m.MoveTimer += dt;
    float t = (m.MoveDuration > 0.001f)
                  ? std::min(m.MoveTimer / m.MoveDuration, 1.0f)
                  : 1.0f;
    m.VisualOffset = -m.StepDelta * (1.0f - t);
    if (t >= 1.0f) {
      m.IsMoving = false;
      m.VisualOffset = glm::vec2(0.0f);
    }
  }

  // Rotate the 3D hearts
  if (Box) {
    Box->Rotation += 50.0f * dt; // Rotate 50 degrees per second
  }
  if (Box2) {
    Box2->Rotation += 50.0f * dt; // Rotate faster
  }
  if (Box3) {
    Box3->Rotation += 50.0f * dt; // Rotate even faster
  }
  if (player) {
    // We control all rotation via RotationEuler; disable the legacy Rotation
    // field.
    player->Rotation = 0.0f;
  }

  if (player && Player) {
    float centerX = Player->Position.x + Player->Size.x * 0.5f;
    float centerY = Player->Position.y + Player->Size.y * 0.5f;

    // Interpolated visual position (uses VisualOffset from PlayerObject
    // animation)
    float visualX = centerX + Player->VisualOffset.x;
    float visualY = centerY + Player->VisualOffset.y;

    float normalizedX = visualX / static_cast<float>(this->Width);
    float normalizedY = visualY / static_cast<float>(this->Height);

    // Must use the very same mapping as the level tiles (offsets included),
    // otherwise the mage does not sit on the grid it is logically standing on.
    player->Position.x = (normalizedX - 0.5f) * 5.5f + 2.00f;
    player->Position.y = (0.5f - normalizedY) * 3.0f - 1.10f;

    // The rig now carries the motion, so the old procedural breathing, hop and
    // squash/stretch are gone: layering them on a real walk cycle just fights
    // it. Position and facing stay here, they are grid logic rather than
    // animation.
    const float playerScale =
        0.116f; // see tileScale: rescaled with the 17x10 grid
    // RotationEuler.x = 90 lays the mage toward the camera, so its height
    // becomes depth. Anchoring the feet at z = 0 left the body entirely in
    // front of the tile plane, and perspective then shifted it by up to 18% of
    // a cell, with the sign flipping between the top and bottom rows. Sitting
    // the body's midpoint on the tile plane keeps the mage centred on its cell
    // at every row.
    const float tilePlaneZ =
        -0.20f; // where non-solid tiles, boxes included, sit
    player->Position.z = tilePlaneZ - (MAGE_MESH_HEIGHT * playerScale) * 0.5f;
    player->Size = glm::vec3(playerScale);
    player->RotationEuler = glm::vec3(90.0f, PlayerFacingYaw, 0.0f);

    if (this->PlayerDying) {
      // Clamped a hair short of the end rather than looped: SetPose wraps with
      // fmod, so an unclamped timer would spring the mage back onto his feet
      // the moment the clip ran out. The last frame is the pose he has to hold
      // under the panel.
      const float duration = player->AnimationDuration(DEATH_CLIP);
      const float held =
          (duration > 0.0f) ? std::min(DeathTimer, duration - 0.001f) : 0.0f;
      player->SetPose(DEATH_CLIP, held);
    } else if (Player->IsAnimatingMovement()) {
      // Drive the clip off the step's own progress rather than wall-clock time,
      // so one cell is exactly one stride (half a walk cycle) whatever
      // MoveDuration is. Alternating on move parity makes the mage lead with
      // the other leg each step.
      const float stride = player->AnimationDuration("Walking_A") * 0.5f;
      const float t = std::min(
          Player->MoveTimer / std::max(Player->MoveDuration, 0.001f), 1.0f);
      player->SetPose("Walking_A",
                      (static_cast<float>(Moves % 2) + t) * stride);
    } else if (this->CastTimer > 0.0f) {
      // Played forwards from its own start, so a second click restarts the
      // throw instead of resuming it half way, and sped up by the same factor
      // the release timing uses. Walking outranks it: a step that begins
      // mid-cast should look like walking, not like a stuck arm.
      const float castLength =
          player->AnimationDuration(CAST_CLIP) / CAST_PLAYBACK_SPEED;
      const float elapsed = std::max(0.0f, castLength - this->CastTimer);
      player->SetPose(CAST_CLIP, elapsed * CAST_PLAYBACK_SPEED);
    } else {
      // Idle_B rather than Idle_A: it is the far broader of the two idles (the
      // torso travels ~6x as much), so the mage visibly breathes while you
      // think.
      player->SetPose("Idle_B", GameTime);
    }
  }

  // Update lights
  Lights.clear();

  // Light follows player
  Light playerLight;
  playerLight.position = Player->Position + glm::vec2(Player->Size.x / 2.0f,
                                                      Player->Size.y / 2.0f);
  playerLight.color = glm::vec3(0.8f, 0.7f, 0.5f); // Dimmer warm white
  playerLight.radius = 180.0f;
  Lights.push_back(playerLight);

  // Add a static light in the center
  Light centerLight;
  centerLight.position = glm::vec2(this->Width / 2.0f, this->Height / 2.0f);
  centerLight.color = glm::vec3(0.8f, 0.8f, 1.0f); // Cool white
  centerLight.radius = 150.0f;
  Lights.push_back(centerLight);

  if (this->State == GAME_WIN) {
    // Rotate the player continuously (360 degrees per second)
    Player->Rotation += 360.0f * dt;

    // Keep rotation between 0-360
    if (Player->Rotation >= 360.0f)
      Player->Rotation -= 360.0f;
  }
}

void Game::ProcessInput(float dt) {
  // H puts the free camera back on its rest pose. Checked before anything else
  // so it still answers while a move animates or the menu is up.
  if (this->Keys[GLFW_KEY_H] && !this->KeysProcessed[GLFW_KEY_H]) {
    this->ResetCamera();
    this->KeysProcessed[GLFW_KEY_H] = true;
  }
  if (!this->Keys[GLFW_KEY_H])
    this->KeysProcessed[GLFW_KEY_H] = false;

  auto reloadSelectedLevel = [this]() {
    const char *levelPath = "levels/one.lvl";
    switch (this->Level) {
    case 1:
      levelPath = "levels/two.lvl";
      break;
    case 2:
      levelPath = "levels/three.lvl";
      break;
    case 3:
      levelPath = "levels/four.lvl";
      break;
    default:
      break;
    }

    this->Levels[this->Level].Load(levelPath, this->Width, this->Height);
    Player->Position = this->Levels[this->Level].PlayerStartPos;
    Player->Rotation = 0.0f;
    this->PlayerHearts = 3;
    this->PlayerDying = false;
    this->DeathTimer = 0.0f;
    this->HasStaff = false; // each attempt has to find the staff again
    this->CastCooldown = 0.0f;
    this->CastTimer = 0.0f;
    this->CastPending = false;
    this->Bolts.clear();
    this->ElapsedTime = 0.0f; // reset stopwatch on level reload
    this->Moves = 0;          // reset move counter
    this->Score = 0;          // reset live score
    this->VisionModifier = 0.0f;
    this->VisionTimer = 0.0f;
    this->FreezeTurns = 0;
    this->HasteTurns = 0;
    this->HasteSkipNext = false;
    this->BonusPoints = 0;
    if (player) {
      player->RotationEuler = glm::vec3(0.0f);
    }
    for (int i = 0; i < ITEM_SLOTS; ++i)
      this->Inventory[i] = 0; // nothing carries over between attempts
    this->SelectedItem = 0;
    this->InventoryOpen = false;
    this->RandomizePickups(); // redraw where the pickups go first, then let the
    this->SpawnMonsters();    // monsters draw from what is still plain floor
    this->ResetCamera();      // a new attempt starts from the default shot
  };

  if (this->State == GAME_MENU) {
    auto startSelectedLevel = [this, &reloadSelectedLevel]() {
      reloadSelectedLevel();
      this->State = GAME_ACTIVE;
      this->MenuLevelsOpen = false;
      this->MenuResultsOpen = false;
    };
    const unsigned int levelCount = (unsigned int)this->Levels.size();

    if (this->MouseClicked) {
      bool hitButton = false;
      for (const MenuButton &b : BuildMenuButtons()) {
        bool hovered = MouseX >= b.x && MouseX <= b.x + b.w && MouseY >= b.y &&
                       MouseY <= b.y + b.h;
        if (!hovered)
          continue;
        hitButton = true;

        // Anything other than Reset itself cancels a pending confirmation, so
        // an armed button can never be triggered by a later, unrelated click.
        if (b.action != -8)
          this->ResetArmed = false;

        if (b.action == -2) {
          // New Game only unfolds the level spinner; START begins the game.
          this->MenuLevelsOpen = !this->MenuLevelsOpen;
          this->MenuResultsOpen = false;
        } else if (b.action == -3) {
          this->RequestQuit = true;
        } else if (b.action == -4) {
          this->MenuResultsOpen = !this->MenuResultsOpen;
          this->MenuLevelsOpen = false;
        } else if (b.action == -5 && levelCount > 0) {
          this->Level = (this->Level + levelCount - 1) % levelCount;
        } else if (b.action == -6 && levelCount > 0) {
          this->Level = (this->Level + 1) % levelCount;
        } else if (b.action == -7) {
          startSelectedLevel();
        } else if (b.action == -8) {
          if (!this->ResetArmed) {
            this->ResetArmed = true; // first click: ask for confirmation
          } else {
            this->BestScores.assign(4, 0);
            this->SaveBestScores();
            this->ResetArmed = false;
            std::cout << "Best scores erased." << std::endl;
          }
        }
        break;
      }
      if (!hitButton) // clicking empty parchment also cancels
        this->ResetArmed = false;
      this->MouseClicked = false;
    }

    // Keyboard shortcuts mirroring the spinner while it is open
    if (this->MenuLevelsOpen) {
      if (this->Keys[GLFW_KEY_LEFT] && !this->KeysProcessed[GLFW_KEY_LEFT] &&
          levelCount > 0) {
        this->Level = (this->Level + levelCount - 1) % levelCount;
        this->KeysProcessed[GLFW_KEY_LEFT] = true;
      }
      if (this->Keys[GLFW_KEY_RIGHT] && !this->KeysProcessed[GLFW_KEY_RIGHT] &&
          levelCount > 0) {
        this->Level = (this->Level + 1) % levelCount;
        this->KeysProcessed[GLFW_KEY_RIGHT] = true;
      }
      if (this->Keys[GLFW_KEY_ENTER] && !this->KeysProcessed[GLFW_KEY_ENTER]) {
        this->KeysProcessed[GLFW_KEY_ENTER] = true;
        startSelectedLevel();
      }
      if (!this->Keys[GLFW_KEY_LEFT])
        this->KeysProcessed[GLFW_KEY_LEFT] = false;
      if (!this->Keys[GLFW_KEY_RIGHT])
        this->KeysProcessed[GLFW_KEY_RIGHT] = false;
      if (!this->Keys[GLFW_KEY_ENTER])
        this->KeysProcessed[GLFW_KEY_ENTER] = false;
    }
    return;
  }

  if (this->State == GAME_ACTIVE) {
    // Nothing to drive while the mage is collapsing: no steps, no items. The
    // camera keys are handled above, so the death is still watchable from any
    // angle.
    if (this->PlayerDying)
      return;

    // The staff answers the mouse. Consuming the click here also stops it from
    // sitting in the flag until the menu next opens, where it would spend
    // itself on whatever button happened to be under the pointer.
    if (this->MouseClicked) {
      this->MouseClicked = false;
      this->FireBolt(this->MouseX, this->MouseY);
    }

    // Inventory first: it has to keep answering while a step animates, and none
    // of its keys overlap the movement set.
    if (this->Keys[GLFW_KEY_I] && !this->KeysProcessed[GLFW_KEY_I]) {
      this->InventoryOpen = !this->InventoryOpen;
      this->KeysProcessed[GLFW_KEY_I] = true;
    }
    if (!this->Keys[GLFW_KEY_I])
      this->KeysProcessed[GLFW_KEY_I] = false;

    // 1-4 pick a slot outright and unfold the panel, so a known item is one
    // keypress away; the arrows walk the list for anyone who would rather
    // browse it.
    for (int slot = 0; slot < ITEM_SLOTS; ++slot) {
      const int digit = GLFW_KEY_1 + slot;
      if (this->Keys[digit] && !this->KeysProcessed[digit]) {
        this->SelectItem(slot);
        this->InventoryOpen = true;
        this->KeysProcessed[digit] = true;
      }
      if (!this->Keys[digit])
        this->KeysProcessed[digit] = false;
    }
    if (this->InventoryOpen) {
      const int prevKeys[2] = {GLFW_KEY_UP, GLFW_KEY_LEFT};
      const int nextKeys[2] = {GLFW_KEY_DOWN, GLFW_KEY_RIGHT};
      for (int i = 0; i < 2; ++i) {
        if (this->Keys[prevKeys[i]] && !this->KeysProcessed[prevKeys[i]]) {
          this->SelectedItem =
              (this->SelectedItem + ITEM_SLOTS - 1) % ITEM_SLOTS;
          this->KeysProcessed[prevKeys[i]] = true;
        }
        if (!this->Keys[prevKeys[i]])
          this->KeysProcessed[prevKeys[i]] = false;
        if (this->Keys[nextKeys[i]] && !this->KeysProcessed[nextKeys[i]]) {
          this->SelectedItem = (this->SelectedItem + 1) % ITEM_SLOTS;
          this->KeysProcessed[nextKeys[i]] = true;
        }
        if (!this->Keys[nextKeys[i]])
          this->KeysProcessed[nextKeys[i]] = false;
      }
    }
    // E spends the selected item, panel open or not.
    if (this->Keys[GLFW_KEY_E] && !this->KeysProcessed[GLFW_KEY_E]) {
      this->UseSelectedItem();
      this->KeysProcessed[GLFW_KEY_E] = true;
    }
    if (!this->Keys[GLFW_KEY_E])
      this->KeysProcessed[GLFW_KEY_E] = false;

    if (Player && Player->IsAnimatingMovement()) {
      return;
    }

    float stepX = CellWidth();
    float stepY = CellHeight();
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

    if (!this->Keys[GLFW_KEY_A])
      this->KeysProcessed[GLFW_KEY_A] = false;
    if (!this->Keys[GLFW_KEY_D])
      this->KeysProcessed[GLFW_KEY_D] = false;
    if (!this->Keys[GLFW_KEY_W])
      this->KeysProcessed[GLFW_KEY_W] = false;
    if (!this->Keys[GLFW_KEY_S])
      this->KeysProcessed[GLFW_KEY_S] = false;

    // Toggle lighting with L key
    if (this->Keys[GLFW_KEY_L] && !this->KeysProcessed[GLFW_KEY_L]) {
      LightingEnabled = !LightingEnabled;
      this->KeysProcessed[GLFW_KEY_L] = true;
      std::cout << "Lighting: " << (LightingEnabled ? "ON" : "OFF")
                << std::endl;
    }
    if (!this->Keys[GLFW_KEY_L])
      this->KeysProcessed[GLFW_KEY_L] = false;

    // Toggle the debug grid overlay with G
    if (this->Keys[GLFW_KEY_G] && !this->KeysProcessed[GLFW_KEY_G]) {
      ShowGrid = !ShowGrid;
      this->KeysProcessed[GLFW_KEY_G] = true;
      std::cout << "Debug grid: " << (ShowGrid ? "ON" : "OFF") << std::endl;
    }
    if (!this->Keys[GLFW_KEY_G])
      this->KeysProcessed[GLFW_KEY_G] = false;

    if (dx != 0 || dy != 0) {
      bool steppingOnSpikes = false;
      unsigned int steppedTile = 0;
      int playerGridX = GridIndex(Player->Position.x, stepX);
      int playerGridY = GridIndex(Player->Position.y, stepY);
      int targetX = playerGridX + dx;
      int targetY = playerGridY + dy;
      auto &mutableTileData = this->Levels[this->Level].TileData;
      if (targetY >= 0 && targetY < static_cast<int>(mutableTileData.size()) &&
          targetX >= 0 &&
          targetX < static_cast<int>(mutableTileData[0].size())) {
        steppedTile = mutableTileData[targetY][targetX];
        steppingOnSpikes = (steppedTile == 6);
      }

      if (player) {
        // Compute and persist the facing yaw so Update() animation block can
        // read it
        PlayerFacingYaw = glm::degrees(
            std::atan2(static_cast<float>(dx), static_cast<float>(dy)));
      }

      bool won = Player->MoveGrid(
          dx, dy, stepX, stepY, this->Levels[this->Level].TileData,
          this->Levels[this->Level].Bricks, this->AudioEngine, this->Effects);

      // Count the step here, before the win check, so the winning move is
      // charged too. IsMoving is only set when the move was legal, so walking
      // into a wall costs nothing.
      if (Player->IsMoving)
        this->Moves++;

      // Pickups: potion (8), curse (10), lantern (11) and ration (12) go into
      // the inventory and only fire when the player spends them; the coin stack
      // (13) is score, so it still lands on contact. Everything here is taken
      // off the board, so each pickup counts once, and only if the step was
      // actually taken.
      const int pickedItem = ItemFromTile(steppedTile);
      const bool isStaffPickup = (steppedTile == 15);
      const bool isCursePickup = (steppedTile == 10);
      const bool isPickup = (pickedItem >= 0 || steppedTile == 13 ||
                             isStaffPickup || isCursePickup);
      if (isPickup && Player->IsMoving) {
        if (isStaffPickup) {
          // A weapon, not a consumable: it stays for the rest of the attempt
          // and is spent by clicking, so it never reaches the inventory.
          this->HasStaff = true;
          std::cout << "Staff collected - click to cast." << std::endl;
        } else if (isCursePickup) {
          // The curse is a trap, and a trap you get to choose when to spring is
          // no trap at all: carrying it made it free to pick up. It goes off
          // where it lies, and cancels a vision potion if one is running.
          this->VisionModifier = -0.40f; // reveal radius 1.15 -> 0.75
          this->VisionTimer = 10.0f;
          std::cout << "Cursed - blinded for 10s." << std::endl;
        } else if (pickedItem >= 0) {
          Inventory[pickedItem]++;
          // Move the cursor onto what was just picked up when it was parked on
          // an empty slot, so the common case stays "walk over it, press E".
          if (Inventory[SelectedItem] == 0)
            SelectedItem = pickedItem;
        } else {
          BonusPoints += 1000; // coin stack
        }
        // Its own sound now, not the win jingle: hearing the end-of-level
        // fanfare on every coin made the real one mean nothing. Played into the
        // effects group like the rest, so it follows their volume.
        ma_engine_play_sound(AudioEngine, "sounds/item_pickup.wav", Effects);

        // Remove the pickup's tile and its brick. Skipping the ones already
        // destroyed is the whole point of the test: RandomizePickups retires
        // the brick a pickup was loaded on and pushes a fresh one for the cell
        // it shuffles it to, so a cell routinely holds two entries. Matching
        // the dead one first and breaking left the live one behind, and a live
        // brick on a cell whose tile has gone back to 0 was drawn as a crate -
        // the block that appeared wherever an item had just been collected.
        mutableTileData[targetY][targetX] = 0;
        for (GameObject &brick : this->Levels[this->Level].Bricks) {
          if (!brick.IsSolid && !brick.Destroyed &&
              GridIndex(brick.Position.x, stepX) == targetX &&
              GridIndex(brick.Position.y, stepY) == targetY) {
            brick.Destroyed = true;
            break;
          }
        }
      }

      if (steppingOnSpikes && this->PlayerHearts > 0) {
        this->PlayerHearts -= 1;
        if (this->PlayerHearts == 0) {
          this->StartDeath("Impaled on the spikes.");
          return; // no win check and no monster turn on the move that kills
        }
      }

      if (won) {
        // Compute and persist final score
        Score = ComputeScore();
        if (Score > BestScores[this->Level]) {
          BestScores[this->Level] = Score;
          SaveBestScores();
        }
        this->State = GAME_WIN;
        ma_engine_play_sound(AudioEngine, "sounds/win.mp3", Effects);
        std::cout << "YOU WON! Score: " << Score << std::endl;
      } else if (Player->IsMoving) {
        // One monster turn per committed player move. MoveGrid reports the win
        // condition, not whether a step happened, so IsMoving is the signal
        // that the move was legal: bumping into a wall must not hand out a free
        // turn.
        StepMonsters(playerGridX, playerGridY, targetX, targetY);
      }
    }
  }
  if (this->State == GAME_WIN || this->State == GAME_OVER) {
    if (this->Keys[GLFW_KEY_ENTER] && !this->KeysProcessed[GLFW_KEY_ENTER]) {
      reloadSelectedLevel();
      this->State = GAME_ACTIVE;
      this->KeysProcessed[GLFW_KEY_ENTER] = true;
    }
    if (!this->Keys[GLFW_KEY_ENTER])
      this->KeysProcessed[GLFW_KEY_ENTER] = false;
  }
}

float Game::CellWidth() const {
  if (this->Level < this->Levels.size()) {
    const auto &t = this->Levels[this->Level].TileData;
    if (!t.empty() && !t[0].empty())
      return static_cast<float>(this->Width) / static_cast<float>(t[0].size());
  }
  return static_cast<float>(this->Width) /
         17.0f; // fallback: nominal level width
}

float Game::CellHeight() const {
  if (this->Level < this->Levels.size()) {
    const auto &t = this->Levels[this->Level].TileData;
    if (!t.empty())
      return static_cast<float>(this->Height) / static_cast<float>(t.size());
  }
  return static_cast<float>(this->Height) /
         10.0f; // fallback: nominal level height
}

float Game::TextWidth(const std::string &txt, float scale) const {
  float w = 0.0f;
  for (char c : txt) {
    auto it = Text->Characters.find(c);
    if (it != Text->Characters.end())
      w += (it->second.Advance >> 6) * scale;
  }
  return w;
}

std::vector<Game::MenuButton> Game::BuildMenuButtons() {
  std::vector<MenuButton> buttons;
  const float cx = Width * 0.5f;
  static const std::string diffNames[4] = {"Novice", "Explorer", "Veteran",
                                           "Master"};

  // Rows stack top-to-bottom from a cursor starting at 0; the finished block is
  // re-centred on the parchment panel at the end, so submenus can expand
  // without any other row's position needing to be retuned by hand.
  float y = 0.0f;
  const float rowGap = 30.0f;
  const float subRowGap = 18.0f; // tight enough that Results plus its Reset
                                 // button still fits the panel
  const float padX = 34.0f, padY = 13.0f; // plaque padding around the label

  auto makeButton = [&](const std::string &label, float scale, int action,
                        float centerX) {
    MenuButton b;
    b.label = label;
    b.scale = scale;
    b.action = action;
    float tw = TextWidth(label, scale);
    b.w = tw + padX;
    b.h = 24.0f * scale + padY;
    b.x = centerX - b.w * 0.5f;
    b.y = y - padY * 0.5f; // centres the label vertically inside the plaque
    b.textX = centerX - tw * 0.5f;
    b.textY = y;
    b.color = glm::vec3(0.0f);
    return b;
  };
  auto neutralizeRect =
      [](MenuButton &b) { // informational: never hovered/clicked
        b.action = -100;
        b.x = -10000.0f;
        b.y = -10000.0f;
        b.w = 0.0f;
        b.h = 0.0f;
      };
  auto addButton = [&](const std::string &label, float scale, int action) {
    buttons.push_back(makeButton(label, scale, action, cx));
    y += rowGap;
  };
  auto addInfoLine = [&](const std::string &label, float scale) {
    MenuButton b = makeButton(label, scale, -100, cx);
    neutralizeRect(b);
    buttons.push_back(b);
    y += subRowGap;
  };
  // Two-tone info row: "<left><right>" centred as a whole, each half inked
  // separately.
  auto addInfoPair = [&](const std::string &left, const std::string &right,
                         const glm::vec3 &rightColor, float scale) {
    float wl = TextWidth(left, scale), wr = TextWidth(right, scale);
    float startX = cx - (wl + wr) * 0.5f;
    MenuButton a = makeButton(left, scale, -100, cx);
    neutralizeRect(a);
    a.textX = startX;
    MenuButton b = makeButton(right, scale, -100, cx);
    neutralizeRect(b);
    b.textX = startX + wl;
    b.color = rightColor;
    buttons.push_back(a);
    buttons.push_back(b);
    y += subRowGap;
  };

  // "New Game" unfolds a single-row level spinner rather than a list of levels.
  addButton(MenuLevelsOpen ? "NEW GAME v" : "NEW GAME >", 0.46f, -2);
  if (MenuLevelsOpen) {
    const float selScale = 0.44f;
    const float arrowScale = 0.58f;
    // Arrows are pinned to the widest possible label so they don't shift as the
    // number changes.
    float halfSpan = TextWidth("LEVEL 4", selScale) * 0.5f + 26.0f;
    MenuButton center =
        makeButton("LEVEL " + std::to_string(Level + 1), selScale, -100, cx);
    neutralizeRect(center);
    MenuButton prev = makeButton("<", arrowScale, -5, cx - halfSpan);
    MenuButton next = makeButton(">", arrowScale, -6, cx + halfSpan);
    // The arrows are drawn larger than the label, so lift them to share its
    // optical centre.
    float arrowLift = 24.0f * (arrowScale - selScale) * 0.5f;
    for (MenuButton *a : {&prev, &next}) {
      a->y -= arrowLift;
      a->textY -= arrowLift;
    }
    buttons.push_back(prev);
    buttons.push_back(center);
    buttons.push_back(next);
    y += rowGap;

    if (Level < 4)
      addInfoLine(diffNames[Level], 0.34f);
    addButton("START", 0.44f, -7);
  }

  addButton(MenuResultsOpen ? "RESULTS v" : "RESULTS >", 0.46f, -4);
  if (MenuResultsOpen) {
    const glm::vec3 scoreColor(0.62f, 0.09f,
                               0.05f); // deep crimson: pops off the parchment
    const glm::vec3 emptyColor(0.42f, 0.36f,
                               0.30f); // muted, so blank slots recede
    unsigned int shown = std::min(
        {(unsigned int)Levels.size(), (unsigned int)BestScores.size(), 4u});
    bool anyScore = false;
    for (unsigned int i = 0; i < shown; ++i) {
      int best = BestScores[i];
      bool hasScore = best > 0;
      anyScore = anyScore || hasScore;
      addInfoPair(std::to_string(i + 1) + ". " + diffNames[i] + ": ",
                  hasScore ? std::to_string(best) : std::string("No Record"),
                  hasScore ? scoreColor : emptyColor, 0.34f);
    }
    // Wiping the records cannot be undone, so the first click only arms the
    // button and the second one carries it out. Offered only when there is
    // something to wipe.
    if (anyScore)
      addButton(ResetArmed ? "CONFIRM - ERASE ALL" : "RESET RECORDS", 0.36f,
                -8);
  }

  addButton("EXIT", 0.46f, -3);

  // Centre the block on the parchment panel painted into the menu artwork
  // (its inner area sits at roughly 62% of the window height).
  if (!buttons.empty()) {
    float blockTop = buttons.front().textY;
    float blockBottom = buttons.back().textY + 24.0f * buttons.back().scale;
    float shift = Height * 0.62f - (blockTop + blockBottom) * 0.5f;
    for (MenuButton &b : buttons) {
      b.y += shift;
      b.textY += shift;
    }
  }
  return buttons;
}

// --- The staff
// -------------------------------------------------------------------
//
// Aiming has to survive the free camera, so the cursor is unprojected through
// whatever view is current rather than assumed to line up with the board's 2D
// coordinates. The scene's tiles all sit on one plane, which makes this a
// ray-plane intersection: build the ray through the pixel, cross it with z =
// -0.20, then invert the world mapping the tiles are laid out with to land back
// in the space Player->Position lives in.
bool Game::ScreenToBoard(double screenX, double screenY,
                         glm::vec2 &board) const {
  const float aspect =
      static_cast<float>(this->Width) / static_cast<float>(this->Height);
  const glm::mat4 projection =
      glm::perspective(glm::radians(CAMERA_FOV), aspect, 0.1f, 100.0f);
  const glm::mat4 inverseVP = glm::inverse(projection * this->GetViewMatrix());

  const float ndcX =
      2.0f * static_cast<float>(screenX) / static_cast<float>(this->Width) -
      1.0f;
  const float ndcY = 1.0f - 2.0f * static_cast<float>(screenY) /
                                static_cast<float>(this->Height);

  glm::vec4 nearH = inverseVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
  glm::vec4 farH = inverseVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
  if (std::fabs(nearH.w) < 1e-6f || std::fabs(farH.w) < 1e-6f)
    return false;
  const glm::vec3 a = glm::vec3(nearH) / nearH.w;
  const glm::vec3 b = glm::vec3(farH) / farH.w;

  const float planeZ = -0.20f; // where the tiles and the pickups sit
  const float dz = b.z - a.z;
  if (std::fabs(dz) < 1e-6f)
    return false; // looking along the plane: no aim point
  const float t = (planeZ - a.z) / dz;
  if (t < 0.0f)
    return false; // the plane is behind the camera
  const glm::vec3 hit = a + (b - a) * t;

  const float nx = (hit.x - 2.00f) / 5.5f + 0.5f;
  const float ny = 0.5f - (hit.y + 1.10f) / 3.0f;
  board = glm::vec2(nx * static_cast<float>(this->Width),
                    ny * static_cast<float>(this->Height));
  return true;
}

void Game::FireBolt(double screenX, double screenY) {
  if (!this->HasStaff || this->PlayerDying || this->CastCooldown > 0.0f ||
      !Player)
    return;

  glm::vec2 target;
  if (!ScreenToBoard(screenX, screenY, target))
    return;

  const glm::vec2 origin(Player->Position.x + Player->Size.x * 0.5f,
                         Player->Position.y + Player->Size.y * 0.5f);
  glm::vec2 direction = target - origin;
  const float length = glm::length(direction);
  if (length < 1.0f)
    return; // clicking on his own feet aims nowhere
  direction /= length;

  // The click only starts the throw. ReleaseBolt puts the bolt in the air when
  // the animation gets to the frame the mage lets go on; firing here instead
  // had the bolt leaving during the wind-up, before his arm had even come
  // forward.
  this->CastPending = true;
  this->CastDirection = direction;
  this->CastTimer =
      player ? player->AnimationDuration(CAST_CLIP) / CAST_PLAYBACK_SPEED
             : 0.0f;
  // The cooldown is the wind-up itself, derived rather than written down twice:
  // any shorter and a second click would restart the animation with the first
  // bolt still waiting to leave.
  this->CastCooldown = this->CastTimer * CAST_RELEASE_FRACTION;
  // Turn to the shot, in the same convention the movement keys use for facing.
  this->PlayerFacingYaw = glm::degrees(std::atan2(direction.x, direction.y));
  if (this->CastTimer <= 0.0f)
    this->ReleaseBolt(); // no clip to wait on: fire on the spot
}

// The throw has reached its release frame. The origin is taken now rather than
// at the click, so a bolt thrown while stepping leaves from where the mage
// actually is.
void Game::ReleaseBolt() {
  if (!this->CastPending || !Player)
    return;
  this->CastPending = false;

  glm::vec2 origin(Player->Position.x + Player->Size.x * 0.5f,
                   Player->Position.y + Player->Size.y * 0.5f);
  // Out at the staff, unless that lands it inside a wall - shooting with your
  // back to one should not swallow the shot.
  const glm::vec2 muzzle =
      origin + this->CastDirection * (CellWidth() * CAST_MUZZLE_CELLS);
  if (this->Level < this->Levels.size()) {
    const auto &tiles = this->Levels[this->Level].TileData;
    const int rows = static_cast<int>(tiles.size());
    const int cols = rows > 0 ? static_cast<int>(tiles[0].size()) : 0;
    const int gx = static_cast<int>(std::floor(muzzle.x / CellWidth()));
    const int gy = static_cast<int>(std::floor(muzzle.y / CellHeight()));
    const bool blocked = (gx < 0 || gy < 0 || gx >= cols || gy >= rows) ||
                         tiles[gy][gx] == 1 || tiles[gy][gx] == 5 ||
                         tiles[gy][gx] == 2 || tiles[gy][gx] == 3;
    if (!blocked)
      origin = muzzle;
  }

  Bolt bolt;
  bolt.Position = origin;
  bolt.Velocity = this->CastDirection * BOLT_SPEED;
  bolt.Life = BOLT_LIFETIME;
  bolt.Age = 0.0f;
  this->Bolts.push_back(bolt);

  ma_engine_play_sound(this->AudioEngine, "sounds/whoosh.flac", this->Effects);
}

// Bolts live in board pixels and are stepped continuously, not cell by cell:
// they are the one thing in this game that does not move on the grid's turn.
void Game::UpdateBolts(float dt) {
  if (this->Bolts.empty() || this->Level >= this->Levels.size())
    return;

  const auto &tiles = this->Levels[this->Level].TileData;
  const int rows = static_cast<int>(tiles.size());
  const int cols = rows > 0 ? static_cast<int>(tiles[0].size()) : 0;
  if (rows == 0 || cols == 0)
    return;
  const float stepX = CellWidth();
  const float stepY = CellHeight();

  for (size_t i = 0; i < this->Bolts.size();) {
    Bolt &bolt = this->Bolts[i];
    bolt.Position += bolt.Velocity * dt;
    bolt.Life -= dt;
    bolt.Age += dt;

    bool spent = (bolt.Life <= 0.0f);

    // Walls, the border, crates and the chest stop it; spikes and loose pickups
    // do not, since a bolt flies over the floor rather than along it.
    if (!spent) {
      const int gx = static_cast<int>(std::floor(bolt.Position.x / stepX));
      const int gy = static_cast<int>(std::floor(bolt.Position.y / stepY));
      if (gx < 0 || gy < 0 || gx >= cols || gy >= rows) {
        spent = true;
      } else {
        const unsigned int tile = tiles[gy][gx];
        if (tile == 1 || tile == 5 || tile == 2 || tile == 3)
          spent = true;
      }
    }

    if (!spent) {
      for (Monster &monster : this->Monsters) {
        if (monster.RespawnIn > 0)
          continue; // already down, the bolt flies past
        const glm::vec2 centre(
            (static_cast<float>(monster.GridX) + 0.5f) * stepX +
                monster.VisualOffset.x,
            (static_cast<float>(monster.GridY) + 0.5f) * stepY +
                monster.VisualOffset.y);
        if (glm::distance(centre, bolt.Position) < stepY * 0.5f) {
          // Buried, not deleted: it owes the grave it spawned from a number of
          // turns and then climbs back out of it.
          monster.RespawnIn = MONSTER_RESPAWN_TURNS;
          monster.IsMoving = false;
          monster.VisualOffset = glm::vec2(0.0f);
          monster.StepDelta = glm::vec2(0.0f);
          ma_engine_play_sound(this->AudioEngine,
                               "sounds/placing-cardboard-box.mp3",
                               this->Effects);
          std::cout << "Skeleton down; back in " << MONSTER_RESPAWN_TURNS
                    << " moves." << std::endl;
          spent = true;
          break;
        }
      }
    }

    if (spent)
      this->Bolts.erase(this->Bolts.begin() + i);
    else
      ++i;
  }
}

// The bolt is drawn as an actual sphere in the scene, not as a billboard: the
// mesh is the procedural one built in sphere_mesh.cpp, pushed through its own
// shader pair. Two passes of the same VAO make the glow - a wide dim halo first,
// then a tight bright core - and both are blended additively so overlapping
// bolts brighten one another the way light does.
//
// Depth testing is off at this point in the frame (the heart HUD turned it off),
// which is what keeps a bolt visible over the walls it flies past; back-face
// culling stands in for it inside the sphere itself, so only the hemisphere
// facing the camera is rasterised.
void Game::DrawBolts() {
  if (this->Bolts.empty() || this->BoltMesh == nullptr)
    return;

  const float aspect =
      static_cast<float>(this->Width) / static_cast<float>(this->Height);
  const glm::mat4 projection =
      glm::perspective(glm::radians(CAMERA_FOV), aspect, 0.1f, 100.0f);
  const glm::mat4 view = this->GetViewMatrix();
  const glm::vec3 cameraPos = this->GetCameraPosition();

  // Board pixels -> world space, the same mapping every tile is placed with.
  auto toWorld = [&](const glm::vec2 &boardPos) {
    const float nx = boardPos.x / static_cast<float>(this->Width);
    const float ny = boardPos.y / static_cast<float>(this->Height);
    return glm::vec3((nx - 0.5f) * 5.5f + 2.00f,
                     (0.5f - ny) * 3.0f - 1.10f, -0.20f);
  };

  // A cell is CellWidth() pixels wide, and the whole board spans 5.5 world
  // units, so this is one cell measured in world units.
  const float cellWorld = 5.5f * CellWidth() / static_cast<float>(this->Width);
  const float coreRadius = cellWorld * BOLT_CORE_CELLS;

  Shader orbShader = ResourceManager::GetShader("orb");
  orbShader.Use();
  orbShader.SetMatrix4("projection", projection);
  orbShader.SetMatrix4("view", view);
  orbShader.SetVector3f("viewPos", cameraPos);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive, so it reads as a glow
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  for (const Bolt &bolt : this->Bolts) {
    const glm::vec3 centre = toWorld(bolt.Position);
    const float flicker = 0.85f + 0.15f * std::sin(bolt.Age * 30.0f);

    // Spinning the sphere costs nothing and makes the shader's swirl, which is
    // anchored to the mesh's own UVs, travel across the visible face.
    const glm::mat4 placed =
        glm::rotate(glm::translate(glm::mat4(1.0f), centre), bolt.Age * 2.0f,
                    glm::normalize(glm::vec3(0.30f, 1.0f, 0.20f)));

    orbShader.SetFloat("time", bolt.Age);

    // Halo: the same unit sphere scaled up and turned down.
    orbShader.SetMatrix4(
        "model",
        glm::scale(placed, glm::vec3(coreRadius * BOLT_HALO_SCALE)));
    orbShader.SetVector3f("coreColor", glm::vec3(0.30f, 0.16f, 0.70f) * flicker);
    orbShader.SetVector3f("rimColor", glm::vec3(0.55f, 0.35f, 1.00f) * flicker);
    orbShader.SetFloat("intensity", 0.40f);
    this->BoltMesh->Draw();

    // Core.
    orbShader.SetMatrix4("model", glm::scale(placed, glm::vec3(coreRadius)));
    orbShader.SetVector3f("coreColor", glm::vec3(0.95f, 0.90f, 1.00f) * flicker);
    orbShader.SetVector3f("rimColor", glm::vec3(0.55f, 0.35f, 1.00f) * flicker);
    orbShader.SetFloat("intensity", 1.00f);
    this->BoltMesh->Draw();
  }

  glDisable(GL_CULL_FACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // back to the normal blend
}

// --- Inventory
// -------------------------------------------------------------------
//
// The pickups used to fire the moment you walked over them, which made them
// pure luck: a haste ration found three moves before the exit was wasted, and
// the curse punished you for touching it. They are carried now, and the player
// decides when each one goes off. Coins keep resolving on contact - a score
// bonus is not something you hold.

int Game::ItemFromTile(unsigned int tile) {
  switch (tile) {
  case 8:
    return ITEM_POTION;
  case 11:
    return ITEM_LANTERN;
  case 12:
    return ITEM_FOOD;
  case 16:
    return ITEM_HEALTH;
  default:
    return -1; // 13 (coins) and 10 (curse) resolve on contact instead
  }
}

const char *Game::ItemName(int slot) const {
  switch (slot) {
  case ITEM_POTION:
    return "Potion";
  case ITEM_LANTERN:
    return "Lantern";
  case ITEM_FOOD:
    return "Ration";
  case ITEM_HEALTH:
    return "Elixir";
  default:
    return "";
  }
}

const char *Game::ItemEffect(int slot) const {
  switch (slot) {
  case ITEM_POTION:
    return "sight +15s";
  case ITEM_LANTERN:
    return "freeze 8";
  case ITEM_FOOD:
    return "haste 10";
  case ITEM_HEALTH:
    return "+1 heart";
  default:
    return "";
  }
}

glm::vec3 Game::ItemColor(int slot) const {
  switch (slot) {
  case ITEM_POTION:
    return glm::vec3(0.90f, 0.40f, 1.00f);
  case ITEM_LANTERN:
    return glm::vec3(1.00f, 0.80f, 0.20f);
  case ITEM_FOOD:
    return glm::vec3(0.40f, 1.00f, 0.50f);
  case ITEM_HEALTH:
    return glm::vec3(1.00f, 0.30f, 0.35f);
  default:
    return glm::vec3(0.7f);
  }
}

void Game::SelectItem(int slot) {
  if (slot >= 0 && slot < ITEM_SLOTS)
    this->SelectedItem = slot;
}

bool Game::UseSelectedItem() {
  if (this->SelectedItem < 0 || this->SelectedItem >= ITEM_SLOTS)
    return false;
  if (this->Inventory[this->SelectedItem] == 0)
    return false;
  // Drinking an elixir at full health would throw it away, so the slot simply
  // refuses.
  if (this->SelectedItem == ITEM_HEALTH && this->PlayerHearts >= 3)
    return false;

  // Same effects the tiles used to apply on contact. A potion still shares its
  // timer with the curse: they are opposites, so drinking one cancels the other
  // rather than stacking with it.
  switch (this->SelectedItem) {
  case ITEM_POTION:
    this->VisionModifier = 0.6f; // reveal radius 1.15 -> 1.75
    this->VisionTimer = 15.0f;
    break;
  case ITEM_HEALTH:
    this->PlayerHearts++; // capped at 3 by the guard above
    break;
  case ITEM_LANTERN:
    this->FreezeTurns = 8; // skeletons skip the next 8 of your moves
    break;
  case ITEM_FOOD:
    this->HasteTurns = 10; // skeletons move once every two of yours
    this->HasteSkipNext = false;
    break;
  default:
    return false;
  }

  this->Inventory[this->SelectedItem]--;
  ma_engine_play_sound(this->AudioEngine, "sounds/win.mp3",
                       this->Effects); // reuse a sound
  std::cout << "Used " << ItemName(this->SelectedItem) << std::endl;
  return true;
}

// The picker, bottom left. Closed it is a single line saying what you are
// carrying, so the board stays clear; open it lists every slot with its count
// and what using it does.
void Game::DrawInventory() {
  unsigned int carried = 0;
  for (int i = 0; i < ITEM_SLOTS; ++i)
    carried += this->Inventory[i];

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  Shader hudShader = ResourceManager::GetShader("sprite");
  hudShader.Use();
  hudShader.SetInteger("enableLighting",
                       false); // HUD is artwork, not lit scene

  if (!this->InventoryOpen) {
    const std::string line = "[I] Items " + std::to_string(carried) +
                             (this->HasStaff ? "    [Click] Cast" : "");
    Text->RenderText(line, 12.0f, this->Height - 26.0f, 0.5f,
                     carried > 0 || this->HasStaff
                         ? glm::vec3(1.0f, 0.9f, 0.55f)
                         : glm::vec3(0.6f, 0.58f, 0.52f));
    glDisable(GL_BLEND);
    return;
  }

  const float rowH = 26.0f;
  const float panelW = 320.0f;
  const float panelH = 42.0f + rowH * ITEM_SLOTS + 20.0f;
  const float panelX = 12.0f;
  const float panelY = this->Height - panelH - 12.0f;
  // Four columns: cursor tag, icon, name and count, then what using it does.
  // The icon column is 22px wide, so the name has to start clear of it.
  const float iconX = panelX + 32.0f; // centre of the icon column
  const float textX = panelX + 54.0f;

  Renderer->DrawSprite(ResourceManager::GetTexture("panel"),
                       glm::vec2(panelX, panelY), glm::vec2(panelW, panelH),
                       0.0f, glm::vec3(1.0f));
  Text->RenderText("INVENTORY", panelX + 18.0f, panelY + 16.0f, 0.55f,
                   glm::vec3(1.0f, 0.92f, 0.6f));

  Texture2D px = ResourceManager::GetTexture("debug_px");
  for (int i = 0; i < ITEM_SLOTS; ++i) {
    const float rowY = panelY + 40.0f + rowH * i;
    const bool empty = (this->Inventory[i] == 0);
    const bool selected = (i == this->SelectedItem);

    // An empty slot is still listed, greyed: the layout stays put as things are
    // picked up and spent, so the number keys always mean the same item.
    glm::vec3 ink = empty ? glm::vec3(0.45f, 0.43f, 0.40f) : ItemColor(i);
    if (selected) {
      // A tag in the item's own colour marks the cursor; on an empty slot it is
      // dimmed too, so "selected" never reads as "usable".
      Renderer->DrawSprite(px, glm::vec2(panelX + 10.0f, rowY),
                           glm::vec2(4.0f, rowH - 8.0f), 0.0f, ink);
    }

    const std::string row = std::to_string(i + 1) + " " + ItemName(i) + "  x" +
                            std::to_string(this->Inventory[i]);
    Text->RenderText(row, textX, rowY, 0.45f, ink);
    // The effect column is right-aligned off its measured width rather than
    // parked at a fixed x, so it cannot drift into the name whatever the font
    // metrics are.
    const std::string effect = ItemEffect(i);
    Text->RenderText(effect, panelX + panelW - 18.0f - TextWidth(effect, 0.40f),
                     rowY, 0.40f,
                     empty ? glm::vec3(0.40f, 0.38f, 0.36f) : ink * 0.85f);
  }

  Text->RenderText("1-4 pick   E use   I close", panelX + 18.0f,
                   panelY + panelH - 24.0f, 0.40f,
                   glm::vec3(0.75f, 0.72f, 0.62f));

  DrawInventoryIcons(iconX, panelY + 40.0f, rowH);
  glDisable(GL_BLEND);
}

// The icons are the item meshes themselves, not a second set of 2D artwork: the
// row shows the player exactly the object they walked over. They get their own
// camera - origin, looking down -Z - so the panel keeps its layout whatever the
// free camera is doing, and each icon is pinned to its row in pixels through
// the inverse of the same perspective divide the scene uses.
void Game::DrawInventoryIcons(float centreX, float firstRowY, float rowH) {
  Model3D *icons[ITEM_SLOTS] = {potion, lantern, food, health};

  // Measured from the meshes, which are all base-anchored and centred on X and
  // Z:
  //   bottle 0.368 x 0.886 x 0.368, lantern 0.640 x 0.925 x 0.640,
  //   plate  1.025 x 0.788 x 0.971.
  // Each scale puts that item's largest projected side on about 22px at the
  // tilt below - the board's own scales are tuned against a cell, not against
  // each other in a list, and reusing them here left the bottle a 7px dot next
  // to the plate.
  const float iconScale[ITEM_SLOTS] = {0.144f, 0.122f, 0.117f, 0.144f};
  const float meshHeight[ITEM_SLOTS] = {0.886f, 0.925f, 0.788f, 0.886f};
  // The board lays every pickup flat with a 90 degree X rotation, because the
  // level is seen from above. An icon seen from straight above is a blob: the
  // bottle becomes a circle. These are tipped just far enough to keep a
  // recognisable silhouette, and turned a little on their own axis so they read
  // as objects rather than cutouts.
  const float iconTilt = 28.0f;
  const float iconYaw = 25.0f;
  const float iconDepth = 3.0f;

  Shader iconShader = ResourceManager::GetShader("box");
  iconShader.Use();
  iconShader.SetVector3f("lightColor", 1.0f, 1.0f, 1.0f);
  iconShader.SetVector3f("viewPos", 0.0f, 0.0f, 0.0f);
  iconShader.SetInteger("useLighting", 1);

  const glm::mat4 iconView(1.0f); // camera at the origin, looking down -Z
  const float aspect =
      static_cast<float>(this->Width) / static_cast<float>(this->Height);
  const glm::mat4 iconProjection =
      glm::perspective(glm::radians(CAMERA_FOV), aspect, 0.1f, 100.0f);
  const float tanHalfFov = std::tan(glm::radians(CAMERA_FOV) * 0.5f);
  // Inverse of the perspective divide at the icons' depth: turns a pixel on the
  // panel into the view-space point that lands on it.
  auto place = [&](float sx, float sy) {
    const float ndcX = 2.0f * sx / static_cast<float>(this->Width) - 1.0f;
    const float ndcY = 1.0f - 2.0f * sy / static_cast<float>(this->Height);
    return glm::vec3(ndcX * iconDepth * tanHalfFov * aspect,
                     ndcY * iconDepth * tanHalfFov, -iconDepth);
  };

  // Depth on and cleared: these are solid meshes, and without it their own back
  // faces paint over the front ones. Nothing else is drawn in 3D this late in
  // the frame, so wiping the buffer costs nothing.
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  for (int i = 0; i < ITEM_SLOTS; ++i) {
    Model3D *model = icons[i];
    if (!model)
      continue;

    const float scale = iconScale[i];
    model->Size = glm::vec3(scale);
    model->Rotation = 0.0f;
    model->RotationEuler = glm::vec3(iconTilt, iconYaw, 0.0f);

    // The meshes sit on their base, so their middle is half a height up the
    // model's own Y - which the tilt has already swung out of the screen plane.
    // Subtracting the rotated half-height is what actually centres the icon on
    // its row.
    const float halfHeight = meshHeight[i] * 0.5f * scale;
    const glm::vec3 centreOffset(0.0f,
                                 halfHeight * std::cos(glm::radians(iconTilt)),
                                 halfHeight * std::sin(glm::radians(iconTilt)));
    model->Position =
        place(centreX, firstRowY + rowH * i + 7.0f) - centreOffset;

    // The elixir is the potion bottle told apart by tint, so its icon has to
    // carry that tint too or the two rows would be the same picture. An empty
    // slot is darkened to match its greyed label.
    const glm::vec3 tint =
        (i == ITEM_HEALTH) ? ItemColor(ITEM_HEALTH) : glm::vec3(1.0f);
    model->Color = (this->Inventory[i] == 0) ? tint * 0.30f : tint;

    // A light just in front of each icon, so they are shaded the way they are
    // on the board rather than flat.
    iconShader.SetVector3f("lightPos",
                           model->Position + glm::vec3(0.0f, 0.0f, 1.2f));
    model->Draw(iconShader, iconView, iconProjection);
  }
  glDisable(GL_DEPTH_TEST);
}

// Debug overlay: draws TileData, the logical state, so a box that looks wrong
// on screen can be checked against the cell the game actually believes it is
// in.
//
// The cells are projected through the scene's own camera rather than laid out
// flat in screen space. A flat grid is not the same picture: the perspective
// camera renders cells 55x57px starting inset from the edge, not 60x50 from the
// corner, and the error grows to almost a full cell down the board. Overlaying
// that would report every correctly placed object as off-centre.
void Game::DrawDebugGrid() {
  if (this->Level >= this->Levels.size())
    return;
  const auto &tiles = this->Levels[this->Level].TileData;
  const int rows = static_cast<int>(tiles.size());
  const int cols = rows > 0 ? static_cast<int>(tiles[0].size()) : 0;
  if (rows == 0 || cols == 0)
    return;

  // Mirrors the tile pass: same world mapping, same view matrix, same
  // perspective(45). It runs the points through the actual matrices rather than
  // the closed form it used to: that shortcut assumed a view with no rotation,
  // so the moment the camera is orbited it would keep drawing a straight-on
  // grid over a tilted board.
  const float aspect =
      static_cast<float>(this->Width) / static_cast<float>(this->Height);
  const glm::mat4 viewProjection =
      glm::perspective(glm::radians(CAMERA_FOV), aspect, 0.1f, 100.0f) *
      this->GetViewMatrix();
  auto project = [&](float nx, float ny) {
    const glm::vec4 world((nx - 0.5f) * 5.5f + 2.0f, (0.5f - ny) * 3.0f - 1.10f,
                          -0.20f, // the plane the tiles sit on
                          1.0f);
    glm::vec4 clip = viewProjection * world;
    if (std::fabs(clip.w) < 1e-6f)
      clip.w = 1e-6f;
    return glm::vec2(((clip.x / clip.w) + 1.0f) * 0.5f * this->Width,
                     (1.0f - (clip.y / clip.w)) * 0.5f * this->Height);
  };
  auto corner = [&](int gx, int gy) {
    return project(static_cast<float>(gx) / cols,
                   static_cast<float>(gy) / rows);
  };

  Texture2D px = ResourceManager::GetTexture("debug_px");

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  Shader gridShader = ResourceManager::GetShader("sprite");
  gridShader.Use();
  gridShader.SetInteger("enableLighting", false);

  // Every edge is drawn as a rotated quad between two projected points instead
  // of an axis-aligned rect: a perspective camera maps a straight world line to
  // a straight screen line, so this stays exact at any orbit angle.
  auto segment = [&](const glm::vec2 &a, const glm::vec2 &b, float t,
                     const glm::vec3 &c) {
    const glm::vec2 d = b - a;
    const float length = glm::length(d);
    if (length < 0.01f)
      return;
    const float angle = glm::degrees(std::atan2(d.y, d.x));
    const glm::vec2 size(length, t);
    const glm::vec2 topLeft =
        (a + b) * 0.5f - size * 0.5f; // DrawSprite rotates about the centre
    Renderer->DrawSprite(px, topLeft, size, angle, c);
  };

  // Outline of one whole cell, from its four projected corners.
  auto cellOutline = [&](int gx, int gy, float t, const glm::vec3 &c) {
    const glm::vec2 tl = corner(gx, gy), tr = corner(gx + 1, gy);
    const glm::vec2 br = corner(gx + 1, gy + 1), bl = corner(gx, gy + 1);
    segment(tl, tr, t, c);
    segment(tr, br, t, c);
    segment(br, bl, t, c);
    segment(bl, tl, t, c);
  };

  const glm::vec3 lineCol(0.30f, 0.34f, 0.40f);
  for (int x = 0; x <= cols; ++x)
    segment(corner(x, 0), corner(x, rows), 1.0f, lineCol);
  for (int y = 0; y <= rows; ++y)
    segment(corner(0, y), corner(cols, y), 1.0f, lineCol);

  // One glyph per non-empty cell, coloured by tile code.
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      unsigned int t = tiles[y][x];
      if (t == 0)
        continue;
      glm::vec3 c(0.7f);
      switch (t) {
      case 1:
        c = glm::vec3(0.55f, 0.55f, 0.60f);
        break; // wall
      case 2:
        c = glm::vec3(0.30f, 0.75f, 1.00f);
        break; // box
      case 3:
        c = glm::vec3(0.30f, 1.00f, 0.40f);
        break; // chest / target
      case 4:
        c = glm::vec3(1.00f, 0.95f, 0.35f);
        break; // player start
      case 5:
        c = glm::vec3(0.35f, 0.35f, 0.40f);
        break; // border
      case 6:
        c = glm::vec3(1.00f, 0.35f, 0.30f);
        break; // spikes
      case 7:
        c = glm::vec3(1.00f, 0.70f, 0.20f);
        break; // key
      case 8:
        c = glm::vec3(0.90f, 0.40f, 1.00f);
        break; // potion
      case 10:
        c = glm::vec3(0.62f, 0.28f, 0.95f);
        break; // curse
      case 11:
        c = glm::vec3(1.00f, 0.80f, 0.20f);
        break; // lantern
      case 12:
        c = glm::vec3(0.40f, 1.00f, 0.50f);
        break; // food (haste)
      case 13:
        c = glm::vec3(1.00f, 0.90f, 0.30f);
        break; // coins
      case 15:
        c = glm::vec3(0.55f, 0.35f, 1.00f);
        break; // staff
      case 16:
        c = glm::vec3(1.00f, 0.30f, 0.35f);
        break; // health elixir
      default:
        break;
      }
      if (t == 2) // boxes are what this overlay exists for: give them a frame
        cellOutline(x, y, 2.0f, c);
      const glm::vec2 centre = project((x + 0.5f) / cols, (y + 0.5f) / rows);
      Text->RenderText(std::to_string(t), centre.x - 4.0f, centre.y - 8.0f,
                       0.42f, c);
    }
  }

  // Where the game thinks the player is, straight from the same integer maths
  // that MoveGrid uses to decide a push.
  if (Player) {
    const float logicStepX = static_cast<float>(this->Width) / cols;
    const float logicStepY = static_cast<float>(this->Height) / rows;
    int pgx = GridIndex(Player->Position.x, logicStepX);
    int pgy = GridIndex(Player->Position.y, logicStepY);
    cellOutline(pgx, pgy, 3.0f, glm::vec3(1.0f, 1.0f, 0.2f));
    Text->RenderText("P " + std::to_string(pgx) + "," + std::to_string(pgy),
                     4.0f, 4.0f, 0.42f, glm::vec3(1.0f, 1.0f, 0.2f));
  }
  for (const Monster &m : Monsters)
    if (m.RespawnIn == 0)
      cellOutline(m.GridX, m.GridY, 2.0f, glm::vec3(1.0f, 0.3f, 0.3f));

  glDisable(GL_BLEND);
}

void Game::Render() {
  if (this->State == GAME_MENU) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Menu artwork is fully painted/lit already; skip the in-game ambient
    // dimming (enableLighting/ambientStrength are set once in Init() and
    // otherwise only touched by GAME_ACTIVE, so without this the menu inherits
    // a 10% ambient dim).
    Shader menuShader = ResourceManager::GetShader("sprite");
    menuShader.Use();
    menuShader.SetInteger("enableLighting", false);

    // 1. Full-bleed hand-painted menu artwork ("cover" fit: fills the window,
    // cropped not stretched).
    //    The art already bakes in the "SOKOBAN" logo and an empty parchment
    //    panel we lay our UI on top of.
    Texture2D menuBg = ResourceManager::GetTexture("menu_bg");
    float bgImgW = (menuBg.Width > 0) ? static_cast<float>(menuBg.Width)
                                      : (float)this->Width;
    float bgImgH = (menuBg.Height > 0) ? static_cast<float>(menuBg.Height)
                                       : (float)this->Height;
    float coverScale = std::max(this->Width / bgImgW, this->Height / bgImgH);
    float bgDrawW = bgImgW * coverScale;
    float bgDrawH = bgImgH * coverScale;
    Renderer->DrawSprite(menuBg,
                         glm::vec2((this->Width - bgDrawW) * 0.5f,
                                   (this->Height - bgDrawH) * 0.5f),
                         glm::vec2(bgDrawW, bgDrawH), 0.0f, glm::vec3(1.0f));

    // Subtle vignette to pull focus toward the parchment panel.
    Texture2D vignette = ResourceManager::GetTexture("vignette");
    Renderer->DrawSprite(vignette, glm::vec2(0.0f, 0.0f),
                         glm::vec2(this->Width, this->Height), 0.0f,
                         glm::vec3(1.0f));

    // 2. Mouse-driven buttons: New Game (level spinner) / Results / Exit.
    //    Plaques first, labels second, so no plaque ever paints over a label.
    const glm::vec3 white(1.0f);
    std::vector<MenuButton> menuButtons = BuildMenuButtons();

    auto isHovered = [this](const MenuButton &b) {
      return MouseX >= b.x && MouseX <= b.x + b.w && MouseY >= b.y &&
             MouseY <= b.y + b.h;
    };

    for (const MenuButton &b : menuButtons) {
      if (b.action == -100)
        continue; // info lines carry no plaque
      const char *suffix = isHovered(b) ? "_hover" : "";
      Texture2D capL =
          ResourceManager::GetTexture(std::string("btn") + suffix + "_l");
      Texture2D mid =
          ResourceManager::GetTexture(std::string("btn") + suffix + "_m");
      Texture2D capR =
          ResourceManager::GetTexture(std::string("btn") + suffix + "_r");

      // Caps keep their own aspect ratio; only the middle slice stretches.
      float capW =
          b.h *
          ((capL.Height > 0) ? (float)capL.Width / (float)capL.Height : 0.5f);
      capW = std::min(capW, b.w * 0.5f);
      Renderer->DrawSprite(capL, glm::vec2(b.x, b.y), glm::vec2(capW, b.h),
                           0.0f, white);
      Renderer->DrawSprite(mid, glm::vec2(b.x + capW, b.y),
                           glm::vec2(b.w - 2.0f * capW, b.h), 0.0f, white);
      Renderer->DrawSprite(capR, glm::vec2(b.x + b.w - capW, b.y),
                           glm::vec2(capW, b.h), 0.0f, white);
    }

    for (const MenuButton &b : menuButtons)
      Text->RenderText(b.label, b.textX, b.textY, b.scale, b.color);

    glDisable(GL_BLEND);
    return;
  }

  if (this->State == GAME_ACTIVE || this->State == GAME_WIN ||
      this->State == GAME_OVER)

  {
    // Draw 2D background and sprites first
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
    Texture2D bg = ResourceManager::GetTexture("background");
    float tileSizeX = (bg.Width > 0) ? static_cast<float>(bg.Width) : 64.0f;
    float tileSizeY = (bg.Height > 0) ? static_cast<float>(bg.Height) : 64.0f;
    const float backgroundTileScale = 0.10f;
    tileSizeX *= backgroundTileScale;
    tileSizeY *= backgroundTileScale;
    if (tileSizeX < 16.0f)
      tileSizeX = 16.0f;
    if (tileSizeY < 16.0f)
      tileSizeY = 16.0f;

    for (float y = 0.0f; y < this->Height; y += tileSizeY) {
      for (float x = 0.0f; x < this->Width; x += tileSizeX) {
        Renderer->DrawSprite(bg, glm::vec2(x, y),
                             glm::vec2(tileSizeX, tileSizeY), 0.0f);
      }
    }
    // 2D tile rendering disabled: level blocks are rendered as 3D models.

    // Now render 3D heart on top
    glEnable(GL_DEPTH_TEST);

    if (Box) {
      Shader boxShader = ResourceManager::GetShader("box");

      // Scene camera: the free camera's matrix, which with an untouched camera
      // is the fixed translate(-2, 1, -3) this pass always used.
      glm::mat4 view = this->GetViewMatrix();
      const glm::vec3 cameraPos = this->GetCameraPosition();

      glm::mat4 projection3D = glm::perspective(
          glm::radians(CAMERA_FOV), (float)this->Width / (float)this->Height,
          0.1f, 100.0f);

      boxShader.Use();
      glm::vec3 lightPosition(0.0f, 0.0f, 0.6f);
      if (Player) {
        const float playerCenterX = Player->Position.x + Player->Size.x * 0.5f;
        const float playerCenterY = Player->Position.y + Player->Size.y * 0.5f;
        const float normalizedX =
            playerCenterX / static_cast<float>(this->Width);
        const float normalizedY =
            playerCenterY / static_cast<float>(this->Height);

        // Keep 3D lighting aligned with the level tile world mapping.
        lightPosition = glm::vec3((normalizedX - 0.5f) * 5.5f + 2.0f,
                                  (0.5f - normalizedY) * 3.0f - 1.10f, 0.6f);
      }
      boxShader.SetVector3f("lightPos", lightPosition);
      boxShader.SetVector3f("lightColor", 1.0f, 1.0f, 1.0f);
      boxShader.SetVector3f("viewPos", cameraPos);
      boxShader.SetInteger("useLighting", 1);

      if (TileModel && WallModel) {
        const float tileHorizontalOffset = 2.00f;
        const float tileVerticalOffset = 1.10f;
        // Same scale as the mage, so the two keep the proportions the art pack
        // intends (the crate mesh is 1.5 wide against the mage's 1.94). Growing
        // both together is what actually closes the gap in a push; growing the
        // crate alone only made it outsize and pushed it into the row above.
        // Rescaled by 0.8 when the grid went from 15x8 to 17x10: cells lost 20%
        // of their height, so keeping 0.145 would have left the crate almost
        // touching the wall above it (0.057 of clearance instead of 0.133).
        const float tileScale = 0.116f;
        const float pillarScale = 0.1f;
        const float chestScale = 0.15f;
        const float keyScale = 0.21f;
        const float potionScale = 0.28f;
        // The lantern mesh is 0.64 wide against the bottle's 0.36, so it needs
        // its own scale to end up a comparable size on the cell (40% of its
        // width).
        const float lanternScale = 0.20f;
        const float foodScale = 0.18f; // plate is 1.03 wide -> 57% of a cell
        const float coinScale = 0.12f; // stack is 1.44 wide -> 53% of a cell
        const float spikesScale = 0.09f;
        // The skeleton mesh is 2.17 units tall against the mage's 2.65, so this
        // scale puts the two at roughly the same on-screen height.
        const float skeletonScale = 0.112f;
        const float chestRotationX = 90.0f;
        const float chestRotationY = 270.0f;
        const float chestRotationZ = 0.0f;
        const float keyRotationX = 180.0f;
        const float keyRotationY = 0.0f;
        const float keyRotationZ = 0.0f;
        const float potionRotationX = 90.0f;
        const float potionRotationY = 0.0f;
        const float potionRotationZ = 0.0f;
        const float spikesRotationX = 90.0f;
        const float spikesRotationY = 0.0f;
        const float spikesRotationZ = 0.0f;
        const float worldGridWidth = 5.5f;
        const float worldGridHeight = 3.0f;
        // Dynamic reveal radius: base + active potion bonus with a pulsing glow
        float pulseExtra = 0.0f;
        if (VisionTimer > 0.0f) {
          float pulse = 0.04f * std::sin(GameTime * 6.0f); // fast pulsing
          pulseExtra = VisionModifier + pulse;
        }
        const float revealRadius = 1.15f + pulseExtra;

        const auto &tileData = this->Levels[this->Level].TileData;
        const int rows = static_cast<int>(tileData.size());
        const int cols = rows > 0 ? static_cast<int>(tileData[0].size()) : 0;
        const float stepX = cols > 0 ? static_cast<float>(this->Width) /
                                           static_cast<float>(cols)
                                     : 1.0f;
        const float stepY = rows > 0 ? static_cast<float>(this->Height) /
                                           static_cast<float>(rows)
                                     : 1.0f;
        const float wallWidthScale = 0.10f;
        const float wallHeightScale = 0.10f;
        const float wallThicknessScale = 0.05f;
        const float wallTopBottomRotationX = 90.0f;
        const float wallTopBottomRotationY = 0.0f;
        const float wallTopBottomRotationZ = 0.0f;
        const float wallCenterRotationX = 90.0f;
        const float wallCenterRotationY = 90.0f;
        const float wallCenterRotationZ = 0.0f;
        const float topWallYOffset = -0.16f;
        const float bottomWallYOffset = 0.16f;

        if (rows > 0 && cols > 0) {
          // Graves mark where the skeletons come from, and where they are sent
          // back to after a hit. Purely decorative: the spawn tile is stripped
          // to plain floor at load time, so the cell stays walkable and nothing
          // in the movement or push logic ever sees it.
          if (grave) {
            const float graveScale = 0.115f;
            grave->Size = glm::vec3(graveScale);
            grave->Rotation = 0.0f;
            // Standing means laid toward the camera here: the screen is the
            // ground plane, so a model left unrotated has its height running up
            // the screen and reads as fallen over.
            grave->RotationEuler = glm::vec3(90.0f, 0.0f, 0.0f);
            grave->Color = glm::vec3(1.0f);
            boxShader.SetInteger("useLighting", 1);

            // Taken from the skeletons themselves rather than the level file:
            // the spawn cells are drawn per attempt now, and a grave left on the
            // file's marker would point at a cell nothing ever comes back to.
            for (const Monster &m : this->Monsters) {
              const int gx = m.SpawnX;
              const int gy = m.SpawnY;
              const float gnx = (static_cast<float>(gx) + 0.5f) / cols;
              const float gny = (static_cast<float>(gy) + 0.5f) / rows;

              grave->Position.x =
                  (gnx - 0.5f) * worldGridWidth + tileHorizontalOffset;
              // No base correction on Y any more: once rotated, the mesh's
              // height is depth, and its Z extent (already centred) is what
              // shows vertically.
              grave->Position.y =
                  (0.5f - gny) * worldGridHeight - tileVerticalOffset;
              // Sit the body's midpoint on the tile plane, as the characters
              // do, so perspective does not shift it row by row.
              grave->Position.z =
                  -0.20f - (GRAVE_MESH_HEIGHT * graveScale) * 0.5f;

              const glm::vec2 gravePos2D(grave->Position.x, grave->Position.y);
              const glm::vec2 playerLightPos2D(lightPosition.x,
                                               lightPosition.y);
              if (LightingEnabled &&
                  glm::distance(gravePos2D, playerLightPos2D) > revealRadius)
                continue;

              grave->Draw(boxShader, view, projection3D);
            }
          }

          std::vector<int> brickIndexByCell(static_cast<size_t>(rows * cols),
                                            -1);

          for (size_t i = 0; i < this->Levels[this->Level].Bricks.size(); ++i) {
            const GameObject &brick = this->Levels[this->Level].Bricks[i];
            if (brick.Destroyed)
              continue;

            float centerX = brick.Position.x + brick.Size.x * 0.5f;
            float centerY = brick.Position.y + brick.Size.y * 0.5f;

            int gridX = static_cast<int>(std::floor(centerX / stepX));
            int gridY = static_cast<int>(std::floor(centerY / stepY));
            gridX = std::max(0, std::min(gridX, cols - 1));
            gridY = std::max(0, std::min(gridY, rows - 1));

            if (tileData[gridY][gridX] == 5)
              continue;

            brickIndexByCell[static_cast<size_t>(gridY * cols + gridX)] =
                static_cast<int>(i);
          }

          for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
              const float cellCenterX = (static_cast<float>(x) + 0.5f) * stepX;
              const float cellCenterY = (static_cast<float>(y) + 0.5f) * stepY;
              const float normalizedX =
                  cellCenterX / static_cast<float>(this->Width);
              const float normalizedY =
                  cellCenterY / static_cast<float>(this->Height);

              if (tileData[y][x] == 5) {
                const bool isTopOrBottomRow = (y == 0 || y == rows - 1);
                const float rowYOffset =
                    (y == 0) ? topWallYOffset
                             : ((y == rows - 1) ? bottomWallYOffset : 0.0f);
                const glm::vec3 wallEulerRotation =
                    isTopOrBottomRow
                        ? glm::vec3(wallTopBottomRotationX,
                                    wallTopBottomRotationY,
                                    wallTopBottomRotationZ)
                        : glm::vec3(wallCenterRotationX, wallCenterRotationY,
                                    wallCenterRotationZ);
                WallModel->Position.x = (normalizedX - 0.5f) * worldGridWidth +
                                        tileHorizontalOffset;
                WallModel->Position.y = (0.5f - normalizedY) * worldGridHeight -
                                        tileVerticalOffset + rowYOffset;
                WallModel->Position.z = -0.15f;

                const glm::vec2 wallPos2D(WallModel->Position.x,
                                          WallModel->Position.y);
                const glm::vec2 playerLightPos2D(lightPosition.x,
                                                 lightPosition.y);
                if (LightingEnabled &&
                    glm::distance(wallPos2D, playerLightPos2D) > revealRadius)
                  continue;

                WallModel->Size = glm::vec3(wallWidthScale, wallHeightScale,
                                            wallThicknessScale);
                WallModel->Color = glm::vec3(1.0f);
                WallModel->Rotation = 0.0f;
                WallModel->RotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
                WallModel->RotationEuler = wallEulerRotation;

                boxShader.SetInteger("useLighting", 0);
                WallModel->Draw(boxShader, view, projection3D);
                continue;
              }

              const int brickIndex =
                  brickIndexByCell[static_cast<size_t>(y * cols + x)];
              if (brickIndex < 0)
                continue;

              const GameObject &brick =
                  this->Levels[this->Level]
                      .Bricks[static_cast<size_t>(brickIndex)];
              const bool isChestTile =
                  (tileData[y][x] == 3 && chest != nullptr);
              const bool isKeyTile = (tileData[y][x] == 7 && key != nullptr);
              const bool isPotionTile =
                  (tileData[y][x] == 8 && potion != nullptr);
              const bool isCurseTile =
                  (tileData[y][x] == 10 && curse != nullptr);
              const bool isHealthTile =
                  (tileData[y][x] == 16 && health != nullptr);
              const bool isLanternTile =
                  (tileData[y][x] == 11 && lantern != nullptr);
              const bool isFoodTile = (tileData[y][x] == 12 && food != nullptr);
              const bool isCoinTile =
                  (tileData[y][x] == 13 && coins != nullptr);
              const bool isSpikesTile =
                  (tileData[y][x] == 6 && spikes != nullptr);
              const bool isPillarTile =
                  (tileData[y][x] == 1 && pillar != nullptr);
              const bool isStaffTile =
                  (tileData[y][x] == 15 && staff != nullptr);
              Model3D *modelToDraw =
                  isChestTile
                      ? chest
                      : (isKeyTile
                             ? key
                             : (isPotionTile
                                    ? potion
                                    : (isCurseTile
                                           ? curse
                                           : (isHealthTile
                                                  ? health
                                                  : (isLanternTile
                                                         ? lantern
                                                         : (isFoodTile
                                                                ? food
                                                                : (isCoinTile
                                                                       ? coins
                                                                       : (isSpikesTile
                                                                              ? spikes
                                                                              : (isPillarTile
                                                                                     ? pillar
                                                                                     : (isStaffTile
                                                                                            ? staff
                                                                                            : TileModel))))))))));
              const float pillarRotationX = 90.0f;
              // The crate is the only tile drawn unrotated, so its height
              // stays on Y and its base-anchored origin would leave it sitting
              // 29% of a cell high. Every other model is laid flat by a 90
              // degree X rotation, which puts its height on Z instead.
              // Only an actual crate draws the crate. The model used to be
              // whatever was left when no other tile code matched, so any
              // brick outliving its tile - a collected pickup, a stale entry -
              // silently became a box on the board.
              const bool isCrate = (tileData[y][x] == 2);
              if (!isCrate && modelToDraw == TileModel)
                continue;
              modelToDraw->Position.x =
                  (normalizedX - 0.5f) * worldGridWidth + tileHorizontalOffset;
              modelToDraw->Position.y =
                  (0.5f - normalizedY) * worldGridHeight - tileVerticalOffset -
                  (isCrate ? CRATE_MESH_HEIGHT * tileScale * 0.5f : 0.0f);
              if (isStaffTile) {
                // The shaft is not centred on its origin, so its middle has
                // to be rotated by the same angle and subtracted, otherwise
                // it hangs off the cell.
                const float lie = glm::radians(STAFF_LIE_ANGLE);
                const float cx = STAFF_CENTRE_X * STAFF_SCALE;
                const float cy = STAFF_CENTRE_Y * STAFF_SCALE;
                modelToDraw->Position.x -=
                    cx * std::cos(lie) - cy * std::sin(lie);
                modelToDraw->Position.y -=
                    cx * std::sin(lie) + cy * std::cos(lie);
              }
              modelToDraw->Position.z = brick.IsSolid ? -0.15f : -0.20f;

              const glm::vec2 objectPos2D(modelToDraw->Position.x,
                                          modelToDraw->Position.y);
              const glm::vec2 playerLightPos2D(lightPosition.x,
                                               lightPosition.y);
              if (LightingEnabled &&
                  glm::distance(objectPos2D, playerLightPos2D) > revealRadius)
                continue;

              modelToDraw->Size = glm::vec3(
                  isStaffTile
                      ? STAFF_SCALE
                      : (isPillarTile
                             ? pillarScale
                             : (isChestTile
                                    ? chestScale
                                    : (isKeyTile
                                           ? keyScale
                                           : (isLanternTile
                                                  ? lanternScale
                                                  : (isFoodTile
                                                         ? foodScale
                                                         : (isCoinTile
                                                                ? coinScale
                                                                : ((isPotionTile ||
                                                                    isCurseTile ||
                                                                    isHealthTile)
                                                                       ? potionScale
                                                                       : (isSpikesTile
                                                                              ? spikesScale
                                                                              : tileScale)))))))));
              // The curse and the elixir are tinted (both reuse the potion
              // bottle); the lantern has artwork of its own and everything
              // else renders plain.
              if (!isCurseTile && !isHealthTile)
                modelToDraw->Color = glm::vec3(1.0f);
              modelToDraw->Rotation = 0.0f;
              if (isChestTile) {
                modelToDraw->RotationEuler =
                    glm::vec3(chestRotationX, chestRotationY, chestRotationZ);
              } else if (isKeyTile) {
                modelToDraw->RotationEuler =
                    glm::vec3(keyRotationX, keyRotationY, keyRotationZ);
              } else if (isPotionTile || isCurseTile || isHealthTile ||
                         isLanternTile || isFoodTile || isCoinTile) {
                modelToDraw->RotationEuler = glm::vec3(
                    potionRotationX, potionRotationY, potionRotationZ);
              } else if (isStaffTile) {
                // It lies on the floor, so no X tip: that is what lays the
                // characters and the bottles toward the camera, and on a
                // 215-unit shaft it would push the whole length into depth
                // and leave a stub. A Z turn keeps it across the cell.
                modelToDraw->RotationEuler =
                    glm::vec3(0.0f, 0.0f, STAFF_LIE_ANGLE);
              } else if (isSpikesTile) {
                modelToDraw->RotationEuler = glm::vec3(
                    spikesRotationX, spikesRotationY, spikesRotationZ);
              } else if (isPillarTile) {
                modelToDraw->RotationEuler =
                    glm::vec3(pillarRotationX, 0.0f, 0.0f);
              } else {
                modelToDraw->RotationEuler = glm::vec3(0.0f);
              }

              boxShader.SetInteger("useLighting", 1);
              modelToDraw->Draw(boxShader, view, projection3D);
            }
          }

          // Skeletons ride the same grid mapping as the tiles, so they line up
          // with the cells and stay hidden outside the player's light.
          if (skeleton) {
            // Skinned models need their own vertex shader; the fragment stage
            // is shared, so mirror the lighting uniforms onto it.
            Shader skinShader = ResourceManager::GetShader("skinned");
            skinShader.Use();
            skinShader.SetVector3f("lightPos", lightPosition);
            skinShader.SetVector3f("lightColor", 1.0f, 1.0f, 1.0f);
            skinShader.SetVector3f("viewPos", cameraPos);
            skinShader.SetInteger("useLighting", 1);

            for (const Monster &m : Monsters) {
              if (m.RespawnIn > 0)
                continue; // buried: only its grave is on the board
              const float cellCenterX =
                  (static_cast<float>(m.GridX) + 0.5f) * stepX +
                  m.VisualOffset.x;
              const float cellCenterY =
                  (static_cast<float>(m.GridY) + 0.5f) * stepY +
                  m.VisualOffset.y;
              const float normalizedX =
                  cellCenterX / static_cast<float>(this->Width);
              const float normalizedY =
                  cellCenterY / static_cast<float>(this->Height);

              skeleton->Position.x =
                  (normalizedX - 0.5f) * worldGridWidth + tileHorizontalOffset;
              skeleton->Position.y =
                  (0.5f - normalizedY) * worldGridHeight - tileVerticalOffset;
              // Same depth centring as the mage: laid flat, its height is
              // depth, so anchoring the feet would drift it row by row.
              skeleton->Position.z =
                  -0.20f - (SKELETON_MESH_HEIGHT * skeletonScale) * 0.5f;

              const glm::vec2 monsterPos2D(skeleton->Position.x,
                                           skeleton->Position.y);
              const glm::vec2 playerLightPos2D(lightPosition.x,
                                               lightPosition.y);
              if (LightingEnabled &&
                  glm::distance(monsterPos2D, playerLightPos2D) > revealRadius)
                continue;

              skeleton->Size = glm::vec3(skeletonScale);
              skeleton->Rotation = 0.0f;
              skeleton->RotationEuler = glm::vec3(90.0f, m.FacingYaw, 0.0f);

              // Walk while the step animates, idle between turns. SetPose is
              // per-draw, so each skeleton can sit at its own point in the
              // clip.
              skeleton->SetPose(m.IsMoving ? "Walking_A" : "Idle_A",
                                m.AnimTime);
              skeleton->Draw(skinShader, view, projection3D);
            }
          }
        }
      }

      // Draw the mage through the tiles' own camera. It used to have a separate
      // centred one, which left it 0.10 units (27% of a cell) above everything
      // on the grid: pushing downward looked glued, pushing upward looked
      // remote.
      if (player) {
        // The mage is skinned now, so it needs the bone-aware vertex shader.
        Shader playerShader = ResourceManager::GetShader("skinned");
        playerShader.Use();
        playerShader.SetVector3f("lightPos", lightPosition);
        playerShader.SetVector3f("lightColor", 1.0f, 1.0f, 1.0f);
        playerShader.SetVector3f("viewPos", cameraPos);
        playerShader.SetInteger("useLighting", 1);
        player->Draw(playerShader, view, projection3D);

        // Once collected, the staff rides the mage's own skeleton. The KayKit
        // rig carries a bone named handslot.r for exactly this, so the weapon's
        // model matrix is the mage's own times that bone's posed transform: it
        // follows the walk cycle, the throw and the death without any code of
        // its own.
        if (this->HasStaff && staff) {
          glm::mat4 hand;
          if (player->BoneMatrix("handslot.r", hand)) {
            // The grip is the mesh origin, so the bone's own position needs no
            // help; the units and the slot's axis convention do (see the two
            // constants above).
            glm::mat4 fit =
                glm::rotate(glm::mat4(1.0f), glm::radians(STAFF_HELD_TILT),
                            glm::vec3(1.0f, 0.0f, 0.0f));
            fit = glm::scale(fit, glm::vec3(STAFF_HELD_SCALE));
            Shader staffShader = ResourceManager::GetShader("box");
            staffShader.Use();
            staffShader.SetVector3f("lightPos", lightPosition);
            staffShader.SetVector3f("lightColor", 1.0f, 1.0f, 1.0f);
            staffShader.SetVector3f("viewPos", cameraPos);
            staffShader.SetInteger("useLighting", 1);
            staff->Color = glm::vec3(1.0f);
            staff->DrawWithModel(staffShader,
                                 player->ModelMatrix() * hand * fit, view,
                                 projection3D);
          }
        }
      }
    }

    glDisable(GL_DEPTH_TEST);

    // Separate pass for hearts so they are not occluded by walls.
    if (Box) {
      Shader boxShader = ResourceManager::GetShader("box");
      glm::mat4 projection3D = glm::perspective(
          glm::radians(45.0f), (float)this->Width / (float)this->Height, 0.1f,
          100.0f);

      boxShader.Use();
      glm::vec3 lightPosition(0.0f, 0.0f, 0.6f);
      if (Player) {
        const float playerCenterX = Player->Position.x + Player->Size.x * 0.5f;
        const float playerCenterY = Player->Position.y + Player->Size.y * 0.5f;
        const float normalizedX =
            playerCenterX / static_cast<float>(this->Width);
        const float normalizedY =
            playerCenterY / static_cast<float>(this->Height);
        lightPosition = glm::vec3((normalizedX - 0.5f) * 5.5f + 2.0f,
                                  (0.5f - normalizedY) * 3.0f - 1.10f, 0.6f);
      }
      boxShader.SetVector3f("lightPos", lightPosition);
      boxShader.SetVector3f("lightColor", 1.0f, 1.0f, 1.0f);
      boxShader.SetInteger("useLighting", 1);

      glm::mat4 view = glm::mat4(1.0f);
      view = glm::translate(view, glm::vec3(-2.0f, 1.0f, -3.0f));
      boxShader.SetVector3f("viewPos", -2.0f, 1.0f, -3.0f);
      if (Box3 && this->PlayerHearts >= 1) {
        glm::mat4 view3 = glm::mat4(1.0f);
        view3 = glm::translate(view3, glm::vec3(-2.4f, 1.0f, -3.0f));
        boxShader.SetVector3f("viewPos", -2.4f, 1.0f, -3.0f);
        Box3->Draw(boxShader, view3, projection3D);
      }

      if (this->PlayerHearts >= 2)
        Box->Draw(boxShader, view, projection3D);

      if (Box2 && this->PlayerHearts >= 3) {
        glm::mat4 view2 = glm::mat4(1.0f);
        view2 = glm::translate(view2, glm::vec3(-1.6f, 1.0f, -3.0f));
        boxShader.SetVector3f("viewPos", -1.6f, 1.0f, -3.0f);
        Box2->Draw(boxShader, view2, projection3D);
      }
    }

    DrawBolts();

    if (ShowGrid)
      DrawDebugGrid();

    // --- Live HUD (top-right): timer + live score preview ---
    if (this->State == GAME_ACTIVE) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      int t = static_cast<int>(ElapsedTime);
      char liveBuf[32];
      std::snprintf(liveBuf, sizeof(liveBuf), "%02d:%02d", t / 60, t % 60);
      int liveScore = ComputeScore();
      Text->RenderText(std::string("Time ") + liveBuf, Width - 140.0f, 10.0f,
                       0.55f, glm::vec3(1.0f, 0.95f, 0.6f));
      Text->RenderText(std::string("Moves ") + std::to_string(Moves),
                       Width - 155.0f, 30.0f, 0.55f,
                       glm::vec3(1.0f, 0.85f, 0.55f));
      Text->RenderText(std::string("Score ") + std::to_string(liveScore),
                       Width - 155.0f, 50.0f, 0.55f,
                       glm::vec3(0.6f, 1.0f, 0.8f));

      // Timed effects, stacked under the score. Each says how much is left,
      // since both the vision effects and the freeze are only useful while they
      // last.
      float indicatorY = 70.0f;
      if (VisionTimer > 0.0f) {
        float pulse = 0.5f + 0.5f * std::sin(GameTime * 5.0f);
        int secsLeft = static_cast<int>(std::ceil(VisionTimer));
        if (VisionModifier >= 0.0f) {
          glm::vec3 buffColor = glm::mix(glm::vec3(0.2f, 1.0f, 0.4f),
                                         glm::vec3(0.0f, 0.85f, 1.0f), pulse);
          Text->RenderText(std::string("Vision +") + std::to_string(secsLeft) +
                               "s",
                           Width - 148.0f, indicatorY, 0.52f, buffColor);
        } else {
          glm::vec3 curseColor = glm::mix(glm::vec3(0.75f, 0.35f, 1.0f),
                                          glm::vec3(0.45f, 0.15f, 0.8f), pulse);
          Text->RenderText(std::string("Blinded ") + std::to_string(secsLeft) +
                               "s",
                           Width - 148.0f, indicatorY, 0.52f, curseColor);
        }
        indicatorY += 20.0f;
      }
      if (HasteTurns > 0) {
        float pulse = 0.5f + 0.5f * std::sin(GameTime * 5.0f);
        glm::vec3 hasteColor = glm::mix(glm::vec3(0.45f, 1.0f, 0.55f),
                                        glm::vec3(0.15f, 0.85f, 0.35f), pulse);
        Text->RenderText(std::string("Haste ") + std::to_string(HasteTurns) +
                             " moves",
                         Width - 168.0f, indicatorY, 0.52f, hasteColor);
        indicatorY += 20.0f;
      }
      if (FreezeTurns > 0) {
        float pulse = 0.5f + 0.5f * std::sin(GameTime * 5.0f);
        glm::vec3 freezeColor = glm::mix(glm::vec3(1.0f, 0.85f, 0.35f),
                                         glm::vec3(1.0f, 0.65f, 0.1f), pulse);
        Text->RenderText(std::string("Frozen ") + std::to_string(FreezeTurns) +
                             " moves",
                         Width - 168.0f, indicatorY, 0.52f, freezeColor);
      }
      glDisable(GL_BLEND);

      DrawInventory();
    }

    if (this->State == GAME_WIN) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // The overlay is artwork, not part of the lit scene: the 2D lights would
      // otherwise leave the panel dark wherever the player's torch is not.
      Shader overlayShader = ResourceManager::GetShader("sprite");
      overlayShader.Use();
      overlayShader.SetInteger("enableLighting", false);

      const float cx = this->Width * 0.5f;
      const float panelW = 380.0f, panelH = 220.0f;
      const float panelX = cx - panelW * 0.5f;
      const float panelY = this->Height * 0.5f - panelH * 0.5f;

      Renderer->DrawSprite(ResourceManager::GetTexture("scrim"),
                           glm::vec2(0.0f),
                           glm::vec2((float)this->Width, (float)this->Height),
                           0.0f, glm::vec3(1.0f));
      Renderer->DrawSprite(ResourceManager::GetTexture("panel"),
                           glm::vec2(panelX, panelY), glm::vec2(panelW, panelH),
                           0.0f, glm::vec3(1.0f));

      // Every line is centred from its measured width, so the layout holds
      // however many digits the score has.
      auto centered = [this, cx](const std::string &txt, float y, float scale,
                                 const glm::vec3 &col) {
        Text->RenderText(txt, cx - TextWidth(txt, scale) * 0.5f, y, scale, col);
      };

      const glm::vec3 ink(0.16f, 0.10f, 0.05f);
      const glm::vec3 mutedInk(0.42f, 0.34f, 0.26f);
      const glm::vec3 crimson(0.62f, 0.09f, 0.05f);
      const glm::vec3 green(0.09f, 0.40f, 0.13f);

      int totalSecs = static_cast<int>(ElapsedTime);
      char timeBuf[16];
      std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", totalSecs / 60,
                    totalSecs % 60);

      const int best = (this->Level < (unsigned int)BestScores.size())
                           ? BestScores[this->Level]
                           : 0;
      const bool isNewRecord = (Score > 0 && Score >= best);

      centered("YOU WON!", panelY + 26.0f, 0.80f, crimson);

      // Thin ink rule under the title
      Renderer->DrawSprite(ResourceManager::GetTexture("background"),
                           glm::vec2(cx - 105.0f, panelY + 66.0f),
                           glm::vec2(210.0f, 1.5f), 0.0f,
                           glm::vec3(0.34f, 0.24f, 0.13f));

      centered("SCORE", panelY + 76.0f, 0.32f, mutedInk);
      centered(std::to_string(Score), panelY + 92.0f, 0.78f, crimson);

      if (isNewRecord)
        centered("NEW RECORD", panelY + 134.0f, 0.44f, green);
      else
        centered("Best " + std::to_string(best), panelY + 134.0f, 0.42f, ink);

      // Moves first: it is what the score is made of, the clock is just trivia.
      centered(std::string("Moves ") + std::to_string(Moves) + "    Time " +
                   timeBuf,
               panelY + 158.0f, 0.40f, ink);
      // ESC returns to the menu here, it does not quit the game.
      centered("ENTER to retry    ESC for menu", panelY + 188.0f, 0.32f,
               mutedInk);

      glDisable(GL_BLEND);
    } else if (this->State == GAME_OVER) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      Shader overlayShader = ResourceManager::GetShader("sprite");
      overlayShader.Use();
      overlayShader.SetInteger("enableLighting", false);

      const float cx = this->Width * 0.5f;
      const float panelW = 380.0f, panelH = 220.0f;
      const float panelX = cx - panelW * 0.5f;
      const float panelY = this->Height * 0.5f - panelH * 0.5f;

      Renderer->DrawSprite(ResourceManager::GetTexture("scrim"),
                           glm::vec2(0.0f),
                           glm::vec2((float)this->Width, (float)this->Height),
                           0.0f, glm::vec3(1.0f));
      Renderer->DrawSprite(ResourceManager::GetTexture("panel"),
                           glm::vec2(panelX, panelY), glm::vec2(panelW, panelH),
                           0.0f, glm::vec3(1.0f));

      auto centered = [this, cx](const std::string &txt, float y, float scale,
                                 const glm::vec3 &col) {
        Text->RenderText(txt, cx - TextWidth(txt, scale) * 0.5f, y, scale, col);
      };

      const glm::vec3 ink(0.16f, 0.10f, 0.05f);
      const glm::vec3 mutedInk(0.42f, 0.34f, 0.26f);
      const glm::vec3 crimson(0.62f, 0.09f, 0.05f);

      int totalSecs = static_cast<int>(ElapsedTime);
      char timeBuf[16];
      std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", totalSecs / 60,
                    totalSecs % 60);

      const int best = (this->Level < (unsigned int)BestScores.size())
                           ? BestScores[this->Level]
                           : 0;

      centered("GAME OVER", panelY + 26.0f, 0.80f, crimson);

      Renderer->DrawSprite(ResourceManager::GetTexture("background"),
                           glm::vec2(cx - 105.0f, panelY + 66.0f),
                           glm::vec2(210.0f, 1.5f), 0.0f,
                           glm::vec3(0.34f, 0.24f, 0.13f));

      // No score to show: the run ended, so report how far it got instead.
      centered("You ran out of hearts", panelY + 82.0f, 0.40f, ink);
      centered(std::string("Moves ") + std::to_string(Moves) + "    Time " +
                   timeBuf,
               panelY + 116.0f, 0.40f, ink);
      centered(best > 0 ? ("Best on this level " + std::to_string(best))
                        : std::string("No record on this level yet"),
               panelY + 146.0f, 0.36f, mutedInk);

      // ESC returns to the menu here, it does not quit the game.
      centered("ENTER to retry    ESC for menu", panelY + 188.0f, 0.32f,
               mutedInk);

      glDisable(GL_BLEND);
    }
  }
}
