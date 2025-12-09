#ifndef PLAYEROBJECT_H
#define PLAYEROBJECT_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "game_object.h"
#include "texture.h"
#include "game.h"


// BallObject holds the state of the Ball object inheriting
// relevant state data from GameObject. Contains some extra
// functionality specific to Breakout's ball object that
// were too specific for within GameObject alone.
class PlayerObject : public GameObject
{
public:
 
    // constructor(s)
    float   Radius;
    bool    Stuck;
    
    PlayerObject();
    PlayerObject(glm::vec2 pos, glm::vec2 size, Texture2D sprite);
    // moves the ball, keeping it constrained within the window bounds (except bottom edge); returns new position
    glm::vec2 Move(float dt, unsigned int window_width);
    // resets the ball to original state with given position and velocity
    void MoveGrid(int dx, int dy, float stepX, float stepY, std::vector<std::vector<unsigned int>>& levelData, std::vector<GameObject>& bricks);

};

#endif