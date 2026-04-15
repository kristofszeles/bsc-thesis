#include <algorithm>

#include "entity.h"

void Entity::setAngle(float angle) {
    position.angleY = angle;
    if (position.angleY < 0) position.angleY += 360;
    if (position.angleY >= 360) position.angleY = 0;
}

int Entity::checkCollision(Entity* entity) const {
    if (entity == this) return 0;
    
    HitBox hitBox1{};
    HitBox hitBox2{};
    
    float entity1Size = 1.0f;
    float entity2Size = 1.0f;
    if (this->getMesh()) entity1Size = std::max({ this->getMesh()->getDimensions().width, this->getMesh()->getDimensions().height, this->getMesh()->getDimensions().depth });
    if (entity->getMesh()) entity2Size = std::max({ entity->getMesh()->getDimensions().width, entity->getMesh()->getDimensions().height, entity->getMesh()->getDimensions().depth });

    hitBox1.front = position.z + entity1Size / 2;  // Front
    hitBox1.back = position.z - entity1Size / 2;   // Back
    hitBox1.right = position.x + entity1Size / 2;  // Right
    hitBox1.left = position.x - entity1Size / 2;   // Left
    hitBox1.top = position.y + entity1Size / 2;    // Top
    hitBox1.bottom = position.y - entity1Size / 2; // Bottom
    hitBox2.front = entity->getPosition().z + entity2Size / 2;  // Front
    hitBox2.back = entity->getPosition().z - entity2Size / 2;   // Back
    hitBox2.right = entity->getPosition().x + entity2Size / 2;  // Right
    hitBox2.left = entity->getPosition().x - entity2Size / 2;   // Left
    hitBox2.top = entity->getPosition().y + entity2Size / 2;    // Top
    hitBox2.bottom = entity->getPosition().y - entity2Size / 2; // Bottom
    
    std::vector<int> collidingSides;
    if (hitBox1.bottom <= hitBox2.top && hitBox1.top >= hitBox2.bottom) {
        if (hitBox1.back <= hitBox2.front && position.z >= hitBox2.front) {
            if (hitBox1.right >= hitBox2.left && hitBox1.left <= hitBox2.right) {
                collidingSides.push_back(1);
            }
        }
        if (hitBox1.front >= hitBox2.back && position.z <= hitBox2.back) {
            if (hitBox1.right >= hitBox2.left && hitBox1.left <= hitBox2.right) {
                collidingSides.push_back(2);
            }
        }
        if (hitBox1.right >= hitBox2.left && position.x <= hitBox2.left) {
            if (hitBox1.front >= hitBox2.back && hitBox1.back <= hitBox2.front) {
                collidingSides.push_back(3);
            }
        }
        if (hitBox1.left <= hitBox2.right && position.x >= hitBox2.right) {
            if (hitBox1.front >= hitBox2.back && hitBox1.back <= hitBox2.front) {
                collidingSides.push_back(4);
            }
        }
    }
    if (collidingSides.size() == 2) {
        if (collidingSides[0] == 1 && collidingSides[1] == 3) {
            if (std::abs(hitBox1.back - hitBox2.front) < std::abs(hitBox1.right - hitBox2.left))
                return 1;
            else if (std::abs(hitBox1.back - hitBox2.front) > std::abs(hitBox1.right - hitBox2.left))
                return 3;
        } else if (collidingSides[0] == 1 && collidingSides[1] == 4) {
            if (std::abs(hitBox1.back - hitBox2.front) < std::abs(hitBox1.left - hitBox2.right))
                return 1;
            else if (std::abs(hitBox1.back - hitBox2.front) > std::abs(hitBox1.left - hitBox2.right))
                return 4;
        } else if (collidingSides[0] == 2 && collidingSides[1] == 3) {
            if (std::abs(hitBox1.front - hitBox2.back) < std::abs(hitBox1.right - hitBox2.left))
                return 2;
            else if (std::abs(hitBox1.front - hitBox2.back) > std::abs(hitBox1.right - hitBox2.left))
                return 3;
        } else if (collidingSides[0] == 2 && collidingSides[1] == 4) {
            if (std::abs(hitBox1.front - hitBox2.back) < std::abs(hitBox1.left - hitBox2.right))
                return 2;
            else if (std::abs(hitBox1.front - hitBox2.back) > std::abs(hitBox1.left - hitBox2.right))
                return 4;
        }
    } else if (collidingSides.size() == 1) {
        return collidingSides[0];
    }
    return 0;
}

void Player::run() {
    if (potionExpiration <= time(nullptr)) {
        potionExpiration = 0;
        maxVelocity = 0.2f;
    }
    if (targetPosition) {
        if (movingDirection == Direction::NO_DIRECTION) {
            if (position.x < targetPosition->x) movingDirection = Direction::RIGHT;
            else if (position.x > targetPosition->x) movingDirection = Direction::LEFT;
            else if (position.z < targetPosition->z) movingDirection = Direction::BACKWARD;
            else if (position.z > targetPosition->z) movingDirection = Direction::FORWARD;
        }
        if (movingDirection == Direction::FORWARD) {
            xVel = 0;
            zVel = -maxVelocity/2;
            if (position.z <= targetPosition->z) {
                position.z = targetPosition->z;
                movingDirection = Direction::NO_DIRECTION;
            }
        } else if (movingDirection == Direction::LEFT) {
            xVel = -maxVelocity/2;
            zVel = 0;
            if (position.x <= targetPosition->x) {
                position.x = targetPosition->x;
                movingDirection = Direction::NO_DIRECTION;
            }
        } else if (movingDirection == Direction::BACKWARD) {
            xVel = 0;
            zVel = maxVelocity/2;
            if (position.z >= targetPosition->z) {
                position.z = targetPosition->z;
                movingDirection = Direction::NO_DIRECTION;
            }
        } else if (movingDirection == Direction::RIGHT) {
            xVel = maxVelocity/2;
            zVel = 0;
            if (position.x >= targetPosition->x) {
                position.x = targetPosition->x;
                movingDirection = Direction::NO_DIRECTION;
            }
        }
        if (movingDirection == Direction::NO_DIRECTION) {
            xVel = zVel = 0;
            delete targetPosition;
            targetPosition = nullptr;
        }
    } else {
        if (accelerate == 1) {
            if (velocityX < maxVelocity) velocityX += 0.01f;
            if (velocityZ < maxVelocity) velocityZ += 0.01f;
            if (velocityX > maxVelocity) velocityX = maxVelocity;
            if (velocityZ > maxVelocity) velocityZ = maxVelocity;
        } else {
            if (velocityX > 0) {
                if (accelerate == -1)
                    velocityX -= 0.02f;
                else if (accelerate == 0)
                    velocityX -= 0.005f;
            }
            if (velocityZ > 0) {
                if (accelerate == -1)
                    velocityZ -= 0.02f;
                else if (accelerate == 0)
                    velocityZ -= 0.005f;
            }
            if (velocityX < 0) velocityX = 0;
            if (velocityZ < 0) velocityZ = 0;
        }
        if (direction == Direction::FORWARD) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle()));
            zVel *= cosf(glm::radians(getAngle()));
        } else if (direction == Direction::FORWARD_LEFT) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle() + 45));
            zVel *= cosf(glm::radians(getAngle() + 45));
        } else if (direction == Direction::LEFT) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle() + 90));
            zVel *= cosf(glm::radians(getAngle() + 90));
        } else if (direction == Direction::BACKWARD_LEFT) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle() + 135));
            zVel *= cosf(glm::radians(getAngle() + 135));
        } else if (direction == Direction::BACKWARD) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle() + 180));
            zVel *= cosf(glm::radians(getAngle() + 180));
        } else if (direction == Direction::BACKWARD_RIGHT) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle() + 225));
            zVel *= cosf(glm::radians(getAngle() + 225));
        } else if (direction == Direction::RIGHT) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle() + 270));
            zVel *= cosf(glm::radians(getAngle() + 270));
        } else if (direction == Direction::FORWARD_RIGHT) {
            xVel = -velocityX;
            zVel = -velocityZ;
            xVel *= sinf(glm::radians(getAngle() + 315));
            zVel *= cosf(glm::radians(getAngle() + 315));
        }
    }
    position.x += xVel;
    position.z += zVel;
}

void Player::setDirection(int direction) {
    if (std::abs(this->direction - direction) >= 2) {
        if (!(this->direction == 1 && direction == 8) && !(this->direction == 8 && direction == 1)) {
            if (velocityX == 0 && velocityZ == 0)
                this->direction = direction;
            else
                accelerate = -1;
            return;
        }
    }
    this->direction = direction;
}

void Player::resolveCollision(Entity* entity) {
    int hitBoxSide;
    while ((hitBoxSide = checkCollision(entity)) != 0) {
        if (hitBoxSide == 1) {
            if (zVel < 0 && std::abs(zVel) > 0.000001) {
                position.z -= zVel;
                velocityZ = 0;
            } else {
                position.x -= xVel;
                velocityX = 0;
            }
        } else if (hitBoxSide == 2) {
            if (zVel > 0 && std::abs(zVel) > 0.000001) {
                position.z -= zVel;
                velocityZ = 0;
            } else {
                position.x -= xVel;
                velocityX = 0;
            }
        } else if (hitBoxSide == 3) {
            if (xVel > 0 && std::abs(xVel) > 0.000001) {
                position.x -= xVel;
                velocityX = 0;
            } else {
                position.z -= zVel;
                velocityZ = 0;
            }
        } else if (hitBoxSide == 4) {
            if (xVel < 0 && std::abs(xVel) > 0.000001) {
                position.x -= xVel;
                velocityX = 0;
            } else {
                position.z -= zVel;
                velocityZ = 0;
            }
        }
    }
}