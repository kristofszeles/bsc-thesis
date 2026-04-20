#pragma once

#include <list>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#if !defined(__ANDROID__)
#include <nfd.hpp>
#endif
#include <map>
#include <SDL.h>
#include <nlohmann/json.hpp>

#include "maze.h"
#include "window.h"
#include "drawutils.h"
#include "button.h"

using json = nlohmann::json;

struct Block {
	std::string type;
	float x, y, z, angle;

	Block(const std::string& type, float x, float y, float z, float angle) {
		this->type = type;
		this->x = x;
		this->y = y;
		this->z = z;
		this->angle = angle;
	}
};

class Editor {
private:
	DrawUtils* drawUtils;
	Block* selectedBlock;
	Block* start;
	Block* finish;
	bool quit;
	bool showSelectedBlock;
	bool showInput;
	int mapWidth, mapHeight, blockOffset;
	int cameraX, cameraY;
	float selectX, selectY;
	int selectedType, selectedTileTexture;
	std::string inputText;
	std::string skyboxTexture;
	std::string tileTexture;
	std::list<Block*> blocks;
	std::list<Button*> buttons;
	std::vector<std::string> types;
	std::vector<std::string> skyboxTextures;
	std::vector<std::string> tileTextures;
	const int MAXIMUM_MAZE_SIZE = 100;

	void generateMap();
	void drawBlockTo(const SDL_Rect& position, const std::string& type, float angle);
	void drawMap();
	void drawGui();
	void updateFrame();
	void updateCamera();
	void updateSelectedTileTexture() { selectedTileTexture = std::find(tileTextures.begin(), tileTextures.end(), tileTexture) - tileTextures.begin(); }
	void handleMouse();
	void initButtons();
public:
	Editor(DrawUtils* drawUtils = nullptr);
	~Editor();
	void run();
	void newMap();
	void saveMap();
	void openMap();
	void saveMap(const std::string& fileName);
	void loadMap(const std::string& fileName);
	void loadMap(std::stringstream& data);
	void addBlock(Block* block);
	void moveBlock(Block* block, float x, float y);
	void setSkyboxTexture(const std::string& fileName) { this->skyboxTexture = fileName; }
	void setTileTexture(const std::string& fileName) { this->tileTexture = fileName; updateSelectedTileTexture(); }
	bool removeBlock(Block* block);
	bool selectBlockAt(float x, float y);
	bool getQuit() const { return quit; }
	int handleEvents();
	Block* getSelectedBlock() const { return selectedBlock; }
	std::list<Block*>& getBlocks() { return blocks; }
	SDL_Rect transformMapPosition(float x, float y);
};