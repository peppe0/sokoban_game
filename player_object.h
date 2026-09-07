#ifndef PLAYEROBJECT_H
#define PLAYEROBJECT_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "game.h"
#include "game_object.h"
#include "texture.h"

// BallObject holds the state of the Ball object inheriting
// relevant state data from GameObject. Contains some extra
// functionality specific to Breakout's ball object that
// were too specific for within GameObject alone.
class PlayerObject : public GameObject {
public:
  // constructor(s)
  float Radius;
  bool Stuck;

  // How long one cell takes. It is also what sets the pace of the walk cycle,
  // since Update stretches a stride (half of Walking_A) over exactly one step:
  // the clip runs 0.53s at its own speed, so the old 0.12s played it 4.4x too
  // fast to read as walking at all. 0.28s is 1.9x natural: the falcata still
  // reads, with the step back to feeling immediate under the fingers.
  static constexpr float STEP_DURATION = 0.25f;

  // Smooth movement/animation state.
  bool IsMoving;
  glm::vec2 MoveStartPosition;
  glm::vec2 MoveTargetPosition;
  glm::vec2 MoveDirection;
  glm::vec2 VisualOffset;
  float MoveTimer;
  float MoveDuration;

  PlayerObject();
  PlayerObject(glm::vec2 pos, glm::vec2 size, Texture2D sprite);
  // moves the ball, keeping it constrained within the window bounds (except
  // bottom edge); returns new position
  glm::vec2 Move(float dt, unsigned int window_width);
  // resets the ball to original state with given position and velocity
  bool MoveGrid(int dx, int dy, float stepX, float stepY,
                std::vector<std::vector<unsigned int>> &levelData,
                std::vector<GameObject> &bricks,
                struct ma_engine *audioEngine = nullptr,
                struct ma_sound *audioGroup = nullptr);
  void UpdateAnimation(float dt);
  bool IsAnimatingMovement() const;

  void Draw(SpriteRenderer &renderer) override;
};

#endif