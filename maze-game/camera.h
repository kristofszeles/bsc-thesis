#pragma once

#include <glm/glm.hpp>

class Camera {
private:
    int mode, targetYaw;
    float yaw, pitch, zVel, zoom;
    const float MIN_ZOOM_AMOUNT = 8;
    const float MAX_ZOOM_AMOUNT = 32;
    float getX() const {
        float x = cosf(glm::radians(pitch)) * sinf(glm::radians(yaw));
        if (mode == 1) x *= zoom;
        return x;
    }
    float getY() const {
        float y = sinf(glm::radians(pitch));
        if (mode == 1) y *= zoom;
        return y;
    }
    float getZ() const {
        float z = cosf(glm::radians(pitch)) * cosf(glm::radians(yaw));
        if (mode == 1) z *= zoom;
        return z;
    }
public:
    Camera() {
        reset();
    }
    void setMode(int mode) { this->mode = mode; }
    void setYaw(float value) { this->yaw = value; }
    void setPitch(float value) { this->pitch = value; }
    void adjustYaw(float value) { this->yaw += value; }
    void adjustPitch(float value) { this->pitch += value; }
    void zoomIn() { zVel -= 0.5f; }
    void zoomOut() { zVel += 0.5f; }
    /** TPS (mode 1): apply pinch directly to zoom (not through zVel). */
    void addZoomPinch(float delta) {
        if (mode != 1) return;
        zoom += delta;
        if (zoom < MIN_ZOOM_AMOUNT) zoom = MIN_ZOOM_AMOUNT;
        if (zoom > MAX_ZOOM_AMOUNT) zoom = MAX_ZOOM_AMOUNT;
    }
    void rotateTo(int value) { targetYaw = value; }
    void reset() {
        mode = 0;
        targetYaw = -1;
        yaw = pitch = 0;
        zVel = 0;
        zoom = MIN_ZOOM_AMOUNT;
    }
    void resetTargetYaw() { targetYaw = -1; }
    int getMode() const { return mode; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    glm::vec3 getPosition() const { return glm::vec3(getX(), getY(), getZ()); }
    void update() {
        if (targetYaw != -1) {
            if (yaw < targetYaw) {
                if (std::abs(yaw - targetYaw) <= 180) yaw += 5;
                else yaw -= 5;
            }
            if (yaw > targetYaw) {
                if (std::abs(yaw - targetYaw) <= 180) yaw -= 5;
                else yaw += 5;
            }
        }
        if (yaw < 0) yaw = 360 + yaw;
        if (yaw >= 360) yaw = 0;
        if (pitch > 89) pitch = 89;
        if (pitch < -89) pitch = -89;
        if (mode == 1) {
            if (pitch < 30) pitch = 30;
            zoom += zVel;
            if (zoom < MIN_ZOOM_AMOUNT) {
                zoom = MIN_ZOOM_AMOUNT;
                if (zVel < 0) zVel = 0;
            }
            if (zoom > MAX_ZOOM_AMOUNT) {
                zoom = MAX_ZOOM_AMOUNT;
                if (zVel > 0) zVel = 0;
            }
            if (zVel > 0) zVel -= 0.02f;
            if (zVel < 0) zVel += 0.02f;
            if (std::abs(zVel) < 0.05) zVel = 0;
            if (std::abs(zVel) > 2) zVel = ((zVel > 0) - (zVel < 0)) * 2.0f;
        }
    }
};