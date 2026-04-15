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
        buttonGroup1.push_back(new Button("New Game", 0, 0, 4, fonts));
        buttonGroup1.push_back(new Button("Continue Game", 0, 52, 4, fonts));
        buttonGroup1.push_back(new Button("Multiplayer", 0, 104, 4, fonts));
        buttonGroup1.push_back(new Button("Map Editor", 0, 156, 4, fonts));
        buttonGroup1.push_back(new Button("Quit Game", 0, 208, 4, fonts));
        buttonGroup2.push_back(new Button("Easy", 0, 0, 4, fonts));
        buttonGroup2.push_back(new Button("Medium", 0, 52, 4, fonts));
        buttonGroup2.push_back(new Button("Hard", 0, 104, 4, fonts));
        buttonGroup2.push_back(new Button("Custom Map...", 0, 156, 4, fonts));
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