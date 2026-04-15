#pragma once

struct Position {
    float x, y, z, angleY;
    Position(float x = 0, float y = 0, float z = 0, float angleY = 0) : x(x), y(y), z(z), angleY(angleY) {}
};