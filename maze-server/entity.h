#pragma once

#include <string>

class Entity {
protected:
	std::string type;
	float x, y, z, angle;
public:
	Entity(const std::string& type, float x, float y, float z, float angle) : type(type), x(x), y(y), z(z), angle(angle) {};
	virtual ~Entity() {}
	std::string getType() const { return type; }
	float getX() const { return x; }
	float getY() const { return y; }
	float getZ() const { return z; }
	float getAngle() const { return angle; }
};

class Tile : public Entity {
public:
	Tile(float x, float y, float z, float angle) : Entity("Tile", x, y, z, angle) {};
};

class Item : public Entity {
public:
	Item(const std::string& type, float x, float y, float z, float angle) : Entity(type, x, y, z, angle) {};
};

class Emerald : public Item {
public:
	Emerald(float x, float y, float z, float angle) : Item("Item1", x, y, z, angle) {};
};

class Gem : public Item {
public:
	Gem(float x, float y, float z, float angle) : Item("Item2", x, y, z, angle) {};
};

class Ruby : public Item {
public:
	Ruby(float x, float y, float z, float angle) : Item("Item3", x, y, z, angle) {};
};

class Gold : public Item {
public:
	Gold(float x, float y, float z, float angle) : Item("Item4", x, y, z, angle) {};
};

class FastPotion : public Item {
public:
	FastPotion(float x, float y, float z, float angle) : Item("Item5", x, y, z, angle) {};
};

class SlowPotion : public Item {
public:
	SlowPotion(float x, float y, float z, float angle) : Item("Item6", x, y, z, angle) {};
};

class Start : public Item {
public:
	Start(float x, float y, float z, float angle) : Item("Start", x, y, z, angle) {};
};

class Finish : public Item {
public:
	Finish(float x, float y, float z, float angle) : Item("Finish", x, y, z, angle) {};
};