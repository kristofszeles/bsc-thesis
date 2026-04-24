#pragma once

#include <list>

#include "button.h"

class Menu {
private:
    float vehicleAngle;
    std::list<Button*> buttonGroup1;
    std::list<Button*> buttonGroup2;
public:
    Menu(SDL_Surface** fonts) {
        vehicleAngle = 0;
#if defined(__ANDROID__)
        // Larger touch targets: 2x scale vs desktop; y step 90 for 720p (vs 52 / scale 4 on PC).
        const float bs = 8.0f;
        const float y1 = 90.0f, y2 = 180.0f, y3 = 270.0f, y4 = 360.0f;
#else
        const float bs = 4.0f;
        const float y1 = 52.0f, y2 = 104.0f, y3 = 156.0f, y4 = 208.0f;
#endif
        buttonGroup1.push_back(new Button("New Game", 0, 0, bs, fonts));
        buttonGroup1.push_back(new Button("Continue Game", 0, y1, bs, fonts));
        buttonGroup1.push_back(new Button("Multiplayer", 0, y2, bs, fonts));
        buttonGroup1.push_back(new Button("Map Editor", 0, y3, bs, fonts));
        buttonGroup1.push_back(new Button("Quit Game", 0, y4, bs, fonts));
        buttonGroup2.push_back(new Button("Easy", 0, 0, bs, fonts));
        buttonGroup2.push_back(new Button("Medium", 0, y1, bs, fonts));
        buttonGroup2.push_back(new Button("Hard", 0, y2, bs, fonts));
        buttonGroup2.push_back(new Button("Custom Map...", 0, y3, bs, fonts));
    }
    ~Menu() {
        for (auto& button : buttonGroup1) delete button;
        for (auto& button : buttonGroup2) delete button;
    }
    void run() {
        ++vehicleAngle;
        if (vehicleAngle >= 360) vehicleAngle = 0;
    }
    float getVehicleAngle() const { return vehicleAngle; }
    std::list<Button*>& getButtonGroup1() { return buttonGroup1; }
    std::list<Button*>& getButtonGroup2() { return buttonGroup2; }
};