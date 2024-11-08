#include "player.h"
#include <QString>
#include <iostream>

Player::Player(glm::vec3 pos, const Terrain &terrain)
    : Entity(pos), m_velocity(0,0,0), m_acceleration(0,0,0),
      m_camera(pos + glm::vec3(0, 1.5f, 0)), mcr_terrain(terrain),
      mcr_camera(m_camera), flightMode(true), facingBlock(glm::vec3(0.f, 0.f, 0.f)), showB(false),
      pitch(0.f), maxPitch(90.f)
{}

Player::~Player()
{}

void Player::tick(float dT, InputBundle &input) {
    dT = glm::clamp(dT, 0.f, 100.f);
    processInputs(input);
    computePhysics(dT, mcr_terrain);
    updateFacingBlock();
}

void Player::updateFacingBlock() {
    glm::vec3 rayOrigin = m_camera.mcr_position;
    glm::vec3 rayDirection = m_forward;

    glm::ivec3 blockHit;
    float dist;
    if (gridMarch(rayOrigin, rayDirection * 3.f, mcr_terrain, &dist, &blockHit)) {
        facingBlock = glm::vec3(blockHit.x, blockHit.y, blockHit.z);
        showB = true;
    } else {
        showB = false;
    }
}

void Player::processInputs(InputBundle &inputs) {
    // TODO: Update the Player's velocity and acceleration based on the
    // state of the inputs.
    rotateOnUpGlobal(-inputs.mouseX / 4.5f);
    rotateOnRightLocal(-inputs.mouseY / 4.5f);
    inputs.mouseX = 0.f;
    inputs.mouseY = 0.f;

    m_acceleration = glm::vec3(0.f);

    if (flightMode) {
        if (inputs.wPressed) {
            m_acceleration += m_forward;
        }
        if (inputs.sPressed) {
            m_acceleration -= m_forward;
        }
        if (inputs.dPressed) {
            m_acceleration += m_right;
        }
        if (inputs.aPressed) {
            m_acceleration -= m_right;
        }
        if (inputs.ePressed) {
            m_acceleration += m_up;
        }
        if (inputs.qPressed) {
            m_acceleration -= m_up;
        }
        m_acceleration *= 0.2;
    } else {
        if (inputs.wPressed) {
            glm::vec3 forward = m_forward;
            forward.y = 0.f;
            forward = glm::normalize(forward);
            m_acceleration += forward;
        }
        if (inputs.sPressed) {
            glm::vec3 forward = m_forward;
            forward.y = 0.f;
            forward = glm::normalize(forward);
            m_acceleration -= forward;
        }
        if (inputs.dPressed) {
            glm::vec3 right = m_right;
            right.y = 0.f;
            right = glm::normalize(right);
            m_acceleration += right;
        }
        if (inputs.aPressed) {
            glm::vec3 right = m_right;
            right.y = 0.f;
            right = glm::normalize(right);
            m_acceleration -= right;
        }
        if (inputs.spacePressed && m_velocity.y == 0.f) {
            m_acceleration += glm::vec3(0.f, 80.f, 0.f);
            inputs.spacePressed = false;
        }
        m_acceleration *= 0.1;
    }

    if (inputs.fPressed) {
        flightMode = !flightMode;
        inputs.fPressed = false;
    }
}

void Player::computePhysics(float dT, const Terrain &terrain) {
    // TODO: Update the Player's position based on its acceleration
    // and velocity, and also perform collision detection.
    m_velocity *= 0.9f;
    m_velocity += m_acceleration * dT;

    if (!flightMode) {
        m_velocity.y -= 0.3f * 9.81f * dT;
    }

    glm::vec3 movement = m_velocity * 0.0001f * dT;
    if (flightMode) {
        moveAlongVector(movement * 1.2f);
        return;
    }

    glm::ivec3 blockHit;
    float dist;

    glm::vec3 vertices[12] = {
        glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 0.0f, -0.5f),
        glm::vec3(0.5f, 0.0f, 0.5f),  glm::vec3(-0.5f, 0.0f, 0.5f),
        glm::vec3(-0.5f, 1.0f, -0.5f), glm::vec3(0.5f, 1.0f, -0.5f),
        glm::vec3(0.5f, 1.0f, 0.5f),  glm::vec3(-0.5f, 1.0f, 0.5f),
        glm::vec3(-0.5f, 2.0f, -0.5f), glm::vec3(0.5f, 2.0f, -0.5f),
        glm::vec3(0.5f, 2.0f, 0.5f),  glm::vec3(-0.5f, 2.0f, 0.5f)
    };

    glm::vec3 axes[3] = {
        glm::vec3(movement.x, 0, 0),
        glm::vec3(0, movement.y, 0),
        glm::vec3(0, 0, movement.z)
    };

    for (const auto& axisMovement : axes) {
        bool hit = false;
        for (const auto& vertex : vertices) {
            glm::vec3 rayOrigin = m_position + vertex;
            if (gridMarch(rayOrigin, axisMovement, terrain, &dist, &blockHit)) {
                hit = true;
                if (axisMovement.y != 0) {
                    m_velocity.y = 0.f;
                    movement.y = 0.f;
                    if (glm::floor(m_position.y) != blockHit.y + 1) {
                        moveUpGlobal(-1.f);
                    }
                }
                break;
            }
        }
        if (!hit) {
            moveAlongVector(axisMovement);
        }
    }
}

bool Player::gridMarch(glm::vec3 rayOrigin, glm::vec3 rayDirection, const Terrain &terrain, float *out_dist, glm::ivec3 *out_blockHit) {
    float maxLen = glm::length(rayDirection);
    glm::ivec3 currCell = glm::ivec3(glm::floor(rayOrigin));
    rayDirection = glm::normalize(rayDirection);

    float curr_t = 0.f;
    while(curr_t < maxLen) {
        float min_t = glm::sqrt(3.f);
        float interfaceAxis = -1;
        for(int i = 0; i < 3; ++i) {
            if(rayDirection[i] != 0) {
                float offset = glm::max(0.f, glm::sign(rayDirection[i]));
                if(currCell[i] == rayOrigin[i] && offset == 0.f) {
                    offset = -1.f;
                }
                int nextIntercept = currCell[i] + offset;
                float axis_t = (nextIntercept - rayOrigin[i]) / rayDirection[i];
                axis_t = glm::min(axis_t, maxLen);
                if(axis_t < min_t) {
                    min_t = axis_t;
                    interfaceAxis = i;
                }
            }
        }
        if(interfaceAxis == -1) {
            throw std::out_of_range("interfaceAxis was -1 after the for loop in gridMarch!");
        }
        curr_t += min_t;
        rayOrigin += rayDirection * min_t;
        glm::ivec3 offset = glm::ivec3(0,0,0);
        offset[interfaceAxis] = glm::min(0.f, glm::sign(rayDirection[interfaceAxis]));
        currCell = glm::ivec3(glm::floor(rayOrigin)) + offset;
        BlockType cellType = terrain.getGlobalBlockAt(currCell.x, currCell.y, currCell.z);
        if(cellType != EMPTY) {
            *out_blockHit = currCell;
            *out_dist = glm::min(maxLen, curr_t);
            return true;
        }
    }
    *out_dist = glm::min(maxLen, curr_t);
    return false;
}



void Player::setCameraWidthHeight(unsigned int w, unsigned int h) {
    m_camera.setWidthHeight(w, h);
}

void Player::moveAlongVector(glm::vec3 dir) {
    Entity::moveAlongVector(dir);
    m_camera.moveAlongVector(dir);
}
void Player::moveForwardLocal(float amount) {
    Entity::moveForwardLocal(amount);
    m_camera.moveForwardLocal(amount);
}
void Player::moveRightLocal(float amount) {
    Entity::moveRightLocal(amount);
    m_camera.moveRightLocal(amount);
}
void Player::moveUpLocal(float amount) {
    Entity::moveUpLocal(amount);
    m_camera.moveUpLocal(amount);
}
void Player::moveForwardGlobal(float amount) {
    Entity::moveForwardGlobal(amount);
    m_camera.moveForwardGlobal(amount);
}
void Player::moveRightGlobal(float amount) {
    Entity::moveRightGlobal(amount);
    m_camera.moveRightGlobal(amount);
}
void Player::moveUpGlobal(float amount) {
    Entity::moveUpGlobal(amount);
    m_camera.moveUpGlobal(amount);
}
void Player::rotateOnForwardLocal(float degrees) {
    Entity::rotateOnForwardLocal(degrees);
    m_camera.rotateOnForwardLocal(degrees);
}
void Player::rotateOnRightLocal(float degrees) {
    float newPitch = pitch + degrees;
    if (newPitch > maxPitch) {
        degrees = maxPitch - pitch;
    } else if (newPitch < -maxPitch) {
        degrees = -maxPitch - pitch;
    }

    pitch += degrees;

    Entity::rotateOnRightLocal(degrees);
    m_camera.rotateOnRightLocal(degrees);
}
void Player::rotateOnUpLocal(float degrees) {
    Entity::rotateOnUpLocal(degrees);
    m_camera.rotateOnUpLocal(degrees);
}
void Player::rotateOnForwardGlobal(float degrees) {
    Entity::rotateOnForwardGlobal(degrees);
    m_camera.rotateOnForwardGlobal(degrees);
}
void Player::rotateOnRightGlobal(float degrees) {
    Entity::rotateOnRightLocal(degrees);
    m_camera.rotateOnRightLocal(degrees);
}
void Player::rotateOnUpGlobal(float degrees) {
    Entity::rotateOnUpGlobal(degrees);
    m_camera.rotateOnUpGlobal(degrees);
}

QString Player::posAsQString() const {
    std::string str("( " + std::to_string(m_position.x) + ", " + std::to_string(m_position.y) + ", " + std::to_string(m_position.z) + ")");
    return QString::fromStdString(str);
}
QString Player::velAsQString() const {
    std::string str("( " + std::to_string(m_velocity.x) + ", " + std::to_string(m_velocity.y) + ", " + std::to_string(m_velocity.z) + ")");
    return QString::fromStdString(str);
}
QString Player::accAsQString() const {
    std::string str("( " + std::to_string(m_acceleration.x) + ", " + std::to_string(m_acceleration.y) + ", " + std::to_string(m_acceleration.z) + ")");
    return QString::fromStdString(str);
}
QString Player::lookAsQString() const {
    std::string str("( " + std::to_string(m_forward.x) + ", " + std::to_string(m_forward.y) + ", " + std::to_string(m_forward.z) + ")");
    return QString::fromStdString(str);
}
