#pragma once

#include <list>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
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
#if defined(__ANDROID__)
	struct Android {
		Editor* editor = nullptr;
		// Tap tracker for tap-to-reshow-keyboard: confirming on FINGERUP (short, mostly
		// stationary, away from the screen edges) keeps the system back-gesture swipe —
		// which also arrives as FINGERDOWN/FINGERUP — from being misread as a tap.
		struct KeyboardTap {
			bool active = false;
			SDL_FingerID fingerId = 0;
			float x = 0.0f, y = 0.0f;
			Uint32 startTicks = 0;
		};
		KeyboardTap keyboardTap;
		// Latches when the user dismisses the IME (system back gesture, IME's down arrow). On
		// modern Android the back gesture hides the IME without sending KEYCODE_BACK through
		// onKeyPreIme, so SDL_IsTextInputActive stays TRUE — only SDL_IsScreenKeyboardShown
		// (which queries imm.isAcceptingText) reflects the real visibility. The latch blocks
		// syncTextInputState and requestScreenKeyboardOnTap from re-raising the keyboard until
		// the size prompt closes or the user explicitly taps to ask for it back.
		bool keyboardDismissedByUser = false;
		bool prevTextInputActive = false;
		bool prevKeyboardShown = false;
		void setTextInputRect();
		void requestScreenKeyboardOnTap();
		void syncTextInputState();
	};
	Android android;
#endif
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
	// Same as loadMap(stringstream) but reads the skybox+tile header lines first; used for SAF
	// open results on Android where the picked file is delivered as bytes, not a path.
	void loadMapWithHeader(std::stringstream& data);
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