#pragma once

#include <string>
#include <ctime>

#include "mesh.h"
#include "position.h"
#include "drawutils.h"

struct HitBox {
    float front, back, right, left, top, bottom;
};

enum Direction { NO_DIRECTION = 0, FORWARD = 1, FORWARD_LEFT = 2, LEFT = 3, BACKWARD_LEFT = 4, BACKWARD = 5, BACKWARD_RIGHT = 6, RIGHT = 7, FORWARD_RIGHT = 8 };

class Entity {
protected:
    std::string type;
    Position position;
    Mesh* mesh;
    bool hidden;
    int accelerate;
    int direction;
    float velocityX, velocityY, velocityZ;
    float xVel, yVel, zVel;
    float maxVelocity;
    bool hitBoxSides[6] = { true, true, true, true, true, true };
public:
    Entity(const Position& position, Mesh* mesh) : position(position), mesh(mesh) {
        accelerate = 0;
        direction = 0;
        velocityX = velocityY = velocityZ = xVel = yVel = zVel = 0;
        hidden = false;
        maxVelocity = 0.2f;
    }
    virtual ~Entity() {}
    bool isHidden() const { return hidden; }
    bool hasHitBoxSide(int i) const { return hitBoxSides[i - 1]; }
    int getDirection() const { return direction; }
    int checkCollision(Entity* entity) const;
    float getAngle() const { return position.angleY; }
    float getXVel() const { return xVel; }
    float getZVel() const { return zVel; }
    Position& getPosition() { return position; }
    Mesh* getMesh() const { return mesh; }
    std::string getType() const { return type; }
    void setPositionX(float x) { position.x = x; }
    void setPositionY(float y) { position.y = y; }
    void setPositionZ(float z) { position.z = z; }
    void setAngle(float angle);
    void setMesh(Mesh* mesh) { this->mesh = mesh; }
    void removeHitBoxSide(int i) { hitBoxSides[i - 1] = false; }
    void hide() { hidden = true; }
    void show() { hidden = false; }
    virtual void run() {}
    virtual void setDirection(int direction) { this->direction = direction; }
    virtual void resolveCollision(Entity* entity) {}
    virtual bool isItem() const { return false; }
};

class Player : public Entity {
private:
    int health;
    Direction movingDirection;
    Position* targetPosition;
    std::string potionName;
    time_t potionExpiration;
public:
    Player(const Position& position, Mesh* mesh) : Entity(position, mesh) {
        health = 100;
        movingDirection = Direction::NO_DIRECTION;
        potionExpiration = 0;
        targetPosition = nullptr;
        type = "Player";
    }
    ~Player() {
        if (targetPosition) delete targetPosition;
    }
    void run() override;
    void setDirection(int direction) override;
    void resolveCollision(Entity* entity) override;
    void substractHealth(int amount) {
        health -= amount;
        if (health < 0) health = 0;
    }
    void setHealth(int amount) { health = amount; }
    void gotoPosition(float x, float z) { this->targetPosition = new Position(x, 0, z); }
    void activatePotion(const std::string& potionName, int seconds) {
        this->potionName = potionName;
        this->potionExpiration = time(nullptr) + seconds;
    }
    void setMaxVelocity(float value) { this->maxVelocity = value; }
    void setAcceleration(int accelerate) { this->accelerate = accelerate; }
    int getHealth() const { return health; }
    Direction getMovingDirection() const { return movingDirection; }
    std::string getPotionName() { return potionName; }
    time_t getPotionExpiration() const { return potionExpiration; }
};

class Opponent : public Entity {
private:
    Texture* billboard;
    std::string name;
public:
    Opponent(const Position& position, Mesh* mesh, const std::string& name) : Entity(position, mesh) {
        this->billboard = nullptr;
        this->name = name;
    }
    ~Opponent() {
        if (billboard) delete billboard;
    }
    void setBillboard(Texture* billboard) { this->billboard = billboard; }
    Texture* getBillboard() const { return billboard; }
    std::string getName() const { return name; }
};

class NPC : public Entity {
public:
    NPC(const Position& position, Mesh* mesh) : Entity(position, mesh) {
        setDirection(rand() % 4 + 1);
        velocityX = velocityZ = 0.2f;
        type = "NPC";
    }
    ~NPC() {}
    void setDirection(int direction) override {
        this->direction = direction;
        setAngle((direction - 1) * 90.0f);
    }
    void run() override {
        if (direction == 1) {
            xVel = 0;
            zVel = velocityZ;
        } else if (direction == 2) {
            xVel = velocityX;
            zVel = 0;
        } else if (direction == 3) {
            xVel = 0;
            zVel = -velocityZ;
        } else if (direction == 4) {
            xVel = -velocityX;
            zVel = 0;
        }
        position.x += xVel;
        position.z += zVel;
    }
    void resolveCollision(Entity* entity) override {
        int face;
        while ((face = checkCollision(entity)) != 0) {
            if (face == 1) {
                if (zVel < 0) position.z -= zVel;
                else position.x -= xVel;
            } else if (face == 2) {
                if (zVel > 0) position.z -= zVel;
                else position.x -= xVel;
            } else if (face == 3) {
                if (xVel > 0) position.x -= xVel;
                else position.z -= zVel;
            } else if (face == 4) {
                if (xVel < 0) position.x -= xVel;
                else position.z -= zVel;
            }
        }
    }
};

class Tile : public Entity {
public:
    Tile(const Position& position, Mesh* mesh) : Entity(position, mesh) {
        type = "Tile";
    }
    ~Tile() {
        delete mesh;
    }
};

class Item : public Entity {
private:
    int directionY;
    float originY, shiftY;
public:
    Item(const Position& position, Mesh* mesh) : Entity(position, mesh) {
        this->position.y -= 1;
        int angle = rand() % 360;
        setAngle((float)angle);
        originY = this->position.y;
        shiftY = (rand() % 41) / 100.0f;
        directionY = 1;
    }
    void run() override {
        setAngle(getAngle() + 2);
        if (directionY == 1) {
            if (originY + shiftY < originY + 0.4f)
                shiftY += 0.005f;
            else
                directionY = 2;
        } else if (directionY == 2) {
            if (originY + shiftY > originY)
                shiftY -= 0.005f;
            else
                directionY = 1;
        }
        position.y = originY + shiftY;
    };
    bool isItem() const override { return true; }
};

class Gem : public Item {
public:
    Gem(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Item1";
    }
};

class Emerald : public Item {
public:
    Emerald(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Item2";
    }
};

class Gold : public Item {
public:
    Gold(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Item3";
    }
};

class Ruby : public Item {
public:
    Ruby(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Item4";
    }
};

class FastPotion : public Item {
public:
    FastPotion(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Item5";
    }
};

class SlowPotion : public Item {
public:
    SlowPotion(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Item6";
    }
};

class Start : public Item {
public:
    Start(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Start";
    }
};

class Finish : public Item {
public:
    Finish(const Position& position, Mesh* mesh) : Item(position, mesh) {
        type = "Finish";
    }
};