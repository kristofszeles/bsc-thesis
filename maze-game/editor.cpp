#include <iostream>
#include <memory>

#include "editor.h"

#if defined(__ANDROID__)
#include <SDL_hints.h>
static bool keyIsBackOrEscape(const SDL_Keysym& keysym) {
	return keysym.sym == SDLK_ESCAPE || keysym.sym == SDLK_AC_BACK || keysym.scancode == SDL_SCANCODE_AC_BACK;
}
#else
static bool keyIsBackOrEscape(const SDL_Keysym& keysym) {
	return keysym.sym == SDLK_ESCAPE;
}
#endif

Editor::Editor(DrawUtils* drawUtils) : drawUtils(drawUtils) {
#if defined(__ANDROID__)
	SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
	android.editor = this;
#endif
	selectedBlock = nullptr;
	start = nullptr;
	finish = nullptr;
	quit = false;
	showInput = false;
	blockOffset = 32;
	selectX = selectY = 0;
	cameraX = cameraY = 0;
	mapWidth = mapHeight = 0;
	selectedType = 0;
	showSelectedBlock = true;
	selectedTileTexture = 1;
	types = { "Tile", "Start", "Finish", "NPC", "Item1", "Item2", "Item3", "Item4", "Item5", "Item6" };
	skyboxTextures = { "skybox1.png", "skybox2.png", "skybox3.png", "skybox4.png", "skybox5.png", "skybox6.png", "skybox7.png", "skybox8.png" };
	tileTextures = { "Tile1.png", "Tile2.png", "Tile3.png", "Tile4.png", "Tile5.png", "Tile6.png", "Tile7.png", "Tile8.png", "Tile9.png", "Tile10.png", "Tile11.png", "Tile12.png", "Tile13.png", "Tile14.png", "Tile15.png", "Tile16.png", "Tile17.png", "Tile18.png" };
	newMap();
	initButtons();
}

Editor::~Editor() {
#if defined(__ANDROID__)
	if (SDL_IsTextInputActive()) {
		SDL_StopTextInput();
	}
#endif
	for (auto& block : blocks) delete block;
	for (auto& button : buttons) delete button;
}

void Editor::run() {
	while (!quit) {
		drawMap();
		drawGui();
		int event = handleEvents();
		switch (event) {
		case 1:
			newMap();
			break;
		case 2:
			showInput = true;
			mapWidth = 0;
			mapHeight = 0;
			break;
		case 3:
			openMap();
			break;
		case 4:
			if (start && finish) saveMap();
			else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, nullptr, "You must specify a start and a finish point before saving", nullptr);
			break;
		case 5:
			quit = true;
			break;
		}
		if (!showInput) handleMouse();
		updateFrame();
#if defined(__ANDROID__)
		android.syncTextInputState();
#endif
	}
}

#if defined(__ANDROID__)
void Editor::Android::setTextInputRect() {
	Window* win = editor->drawUtils->getWindow();
	if (!win) return;
	int w = win->getWidth();
	int h = win->getHeight();
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	SDL_Rect r;
	r.x = 0;
	r.y = (int)(h * 0.35f);
	r.w = w;
	r.h = (int)(h * 0.30f);
	SDL_SetTextInputRect(&r);
}

void Editor::Android::requestScreenKeyboardOnTap() {
	if (!editor->showInput) return;
	Window* win = editor->drawUtils->getWindow();
	if (!win || !win->getWindow()) return;
	if (SDL_HasScreenKeyboardSupport() == SDL_TRUE) {
		if (SDL_IsScreenKeyboardShown(win->getWindow()) == SDL_TRUE) {
			return;
		}
	} else {
		if (SDL_IsTextInputActive()) {
			return;
		}
	}
	if (SDL_IsTextInputActive()) {
		SDL_StopTextInput();
	}
	SDL_StartTextInput();
	setTextInputRect();
	SDL_RaiseWindow(win->getWindow());
}

void Editor::Android::syncTextInputState() {
	Window* win = editor->drawUtils->getWindow();
	if (!win || !win->getWindow()) return;
	if (editor->showInput) {
		if (!SDL_IsTextInputActive()) {
			SDL_StartTextInput();
		}
		setTextInputRect();
	} else {
		if (SDL_IsTextInputActive()) {
			SDL_StopTextInput();
		}
	}
}
#endif

void Editor::newMap() {
	mapWidth = 0;
	mapHeight = 0;
	for (auto& block : blocks) {
		delete block;
		block = nullptr;
	}
	start = nullptr;
	finish = nullptr;
	blocks.clear();
	setSkyboxTexture(skyboxTextures.at(rand() % skyboxTextures.size()));
	setTileTexture(tileTextures.at(rand() % tileTextures.size()));
}

void Editor::saveMap() {
#if defined(__ANDROID__)
	return;
#else
	NFD::Guard nfdGuard;
	NFD::UniquePath outPath;
	nfdfilteritem_t filterItem[2] = { { "MAP file", "map" } };
	nfdresult_t result = NFD::SaveDialog(outPath, filterItem, 1);
	if (result == NFD_OKAY) {
		std::string fileName(outPath.get());
		if (fileName.substr(fileName.size() - 4) != ".map") fileName += ".map"; // add file extension if not exists
		saveMap(fileName);
	} else if (result == NFD_CANCEL) {
		return;
	} else {
		std::cout << "Error: " << NFD::GetError() << std::endl;
	}
#endif
}

void Editor::saveMap(const std::string& fileName) {
	std::ofstream output;
	output.open(fileName);
	output << skyboxTexture << std::endl;
	output << tileTextures.at(selectedTileTexture) << std::endl;
	for (auto& block : blocks) {
		if (block) output << block->type << " " << block->x * 3 << " " << 0 << " " << block->y * 3 << " " << block->angle << std::endl;
	}
	output.close();
}

void Editor::openMap() {
#if defined(__ANDROID__)
	return;
#else
	NFD::Guard nfdGuard;
	NFD::UniquePath fileName;
	nfdfilteritem_t filterItem[2] = { { "MAP file", "map" } };
	nfdresult_t result = NFD::OpenDialog(fileName, filterItem, 1);
	if (result == NFD_OKAY) {
		loadMap(fileName.get());
		updateCamera();
	} else if (result == NFD_CANCEL) {
		return;
	} else {
		std::cout << "Error: " << NFD::GetError() << std::endl;
	}
#endif
}

void Editor::loadMap(const std::string& fileName) {
	if (!blocks.empty()) newMap();
	std::ifstream input;
	input.open(fileName);
	float x, y, z, angle;
	std::string type;
	input >> skyboxTexture;
	input >> tileTexture;
	updateSelectedTileTexture();
	while (input >> type >> x >> y >> z >> angle) {
		x /= 3;
		z /= 3;
		addBlock(new Block(type, x, z, 0, angle));
		if (x + 1 > mapWidth) mapWidth = (int)x + 1;
		if (z + 1 > mapHeight) mapHeight = (int)z + 1;
	}
	input.close();
}

void Editor::loadMap(std::stringstream& data) {
	if (!blocks.empty()) newMap();
	selectedTileTexture = rand() % tileTextures.size();
	std::string type;
	float x, y, z, angle;
	while (data >> type >> x >> y >> z >> angle) {
		x /= 3;
		z /= 3;
		addBlock(new Block(type, x, z, 0, angle));
		if (x + 1 > mapWidth) mapWidth = (int)x + 1;
		if (z + 1 > mapHeight) mapHeight = (int)z + 1;
	}
}

void Editor::initButtons() {
	if (!drawUtils) return;
	buttons.push_back(new Button("New Map", 0, 0, 3, drawUtils->getFonts()));
	buttons.push_back(new Button("Generate Map", buttons.back()->getX() + buttons.back()->getTexture()->getWidth() * 3 + 32, 0, 3, drawUtils->getFonts()));
	buttons.push_back(new Button("Load Map", buttons.back()->getX() + buttons.back()->getTexture()->getWidth() * 3 + 32, 0, 3, drawUtils->getFonts()));
	buttons.push_back(new Button("Save Map", buttons.back()->getX() + buttons.back()->getTexture()->getWidth() * 3 + 32, 0, 3, drawUtils->getFonts()));
	buttons.push_back(new Button("Exit to menu", buttons.back()->getX() + buttons.back()->getTexture()->getWidth() * 3 + 32, 0, 3, drawUtils->getFonts()));
}

void Editor::updateCamera() {
	cameraX = drawUtils->getWindow()->getViewportWidth() / 2 - mapWidth * blockOffset / 2;
	cameraY = drawUtils->getWindow()->getViewportHeight() / 2 - mapHeight * blockOffset / 2;
}

void Editor::generateMap() {
	drawUtils->drawBackground2D(drawUtils->getTextures()->at("background"));
	std::unique_ptr<Texture> texture(renderText("Generating...", drawUtils->getFonts()));
	drawUtils->drawTexture2D(texture.get(), drawUtils->getWindow()->getWidth() / 2 - texture->getWidth() * 4 / 2, drawUtils->getWindow()->getHeight() / 2 - texture->getHeight() * 4 / 2, 4);
	updateFrame();
	Maze maze(mapWidth, mapHeight);
	loadMap(maze.getData());
	updateCamera();
}

void Editor::drawBlockTo(const SDL_Rect& position, const std::string& type, float angle) {
	float x = (float)position.x;
	float y = (float)position.y;
	std::string fileName = type + ".png";;
	if (type == "Tile") fileName = tileTextures.at(selectedTileTexture);
	if (type == "Start" && angle == 0) fileName = "arrow_up.png";
	else if (type == "Start" && angle == 90) fileName = "arrow_left.png";
	else if (type == "Start" && angle == 180) fileName = "arrow_down.png";
	else if (type == "Start" && angle == 270) fileName = "arrow_right.png";
	drawUtils->drawTexture2D(drawUtils->getTextures()->at(fileName), x, y, 1, 32, 32);
}

SDL_Rect Editor::transformMapPosition(float x, float y) {
	SDL_Rect position = { (int)x * blockOffset + cameraX, (int)y * blockOffset + cameraY, blockOffset, blockOffset };
	return position;
}

void Editor::drawMap() {
	glClearColor(0.2f, 0.2f, 0.2f, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	for (auto& block : blocks) {
		if (!block) continue;
		SDL_Rect position = transformMapPosition(block->x, block->y);
		if (position.x >= -blockOffset && position.y >= -blockOffset && position.x < drawUtils->getWindow()->getViewportWidth() && position.y < drawUtils->getWindow()->getViewportHeight()) {
			drawBlockTo(position, block->type, block->angle);
		}
	}
	if (!selectedBlock && showSelectedBlock) drawBlockTo(transformMapPosition(selectX, selectY), types[selectedType], 0);
}

void Editor::drawGui() {
	drawUtils->drawRectangle({ 96, 96, 96 }, 0, drawUtils->getWindow()->getViewportHeight() - 59.0f, (float)drawUtils->getWindow()->getViewportWidth(), 59.0f);
	for (auto& button : buttons) {
		drawUtils->drawTexture2D(button->getTexture(), button->getX() + 16, drawUtils->getWindow()->getViewportHeight() - 43.0f, button->getScale());
	}
	if (showInput) {
		drawUtils->drawRectangle({ 96, 96, 96 }, drawUtils->getWindow()->getWidth() / 2 - 300.0f, drawUtils->getWindow()->getHeight() / 2 - 150.0f, 600.f, 300.0f);
		if (mapWidth < 2) drawUtils->drawTextInput("Maze width: ", inputText);
		else if (mapHeight < 2) drawUtils->drawTextInput("Maze height: ", inputText);
	}
}

void Editor::updateFrame() {
	SDL_GL_SwapWindow(drawUtils->getWindow()->getWindow());
}

int Editor::handleEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			quit = true;
			break;
#if defined(__ANDROID__)
		case SDL_FINGERDOWN:
			if (showInput) {
				android.requestScreenKeyboardOnTap();
			}
			break;
#endif
		case SDL_MOUSEWHEEL:
			if (showInput) break;
			if (event.wheel.y > 0) {
				if (selectedType < (int)types.size() - 1) ++selectedType;
			} else if (event.wheel.y < 0) {
				if (selectedType > 0) --selectedType;
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
#if defined(__ANDROID__)
			if (showInput && event.button.button == SDL_BUTTON_LEFT) {
				android.requestScreenKeyboardOnTap();
				break;
			}
#endif
			if (showInput) break;
			if (event.button.button == SDL_BUTTON_LEFT) {
				int buttonIndex = 1;
				for (auto& button : buttons) {
					float scale = button->getScale();
					float x = button->getX();
					float y = drawUtils->getWindow()->getViewportHeight() - 59.0f;
					float w = button->getWidth() * scale + 16;
					float h = button->getHeight() * scale + 32;
					if (event.button.x >= x && event.button.x <= x + w && event.button.y >= y && event.button.y <= y + h) {
						return buttonIndex;
					}
					++buttonIndex;
				}
				if (selectedBlock && selectedBlock->type == "Start") {
					selectedBlock->angle -= 90;
					if (selectedBlock->angle < 0) selectedBlock->angle = 270;
				}
			}
			break;
		case SDL_KEYDOWN:
			if (showInput) {
				if (keyIsBackOrEscape(event.key.keysym)) {
					showInput = false;
					inputText = "";
					break;
				} else if (event.key.keysym.sym == SDLK_BACKSPACE) {
					if (inputText.size() > 0) inputText.pop_back();
				} else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
					if (showInput && !inputText.empty()) {
						int value = std::stoi(inputText);
						if (value > MAXIMUM_MAZE_SIZE) {
							std::string message = "The width or height of the maze cannot be larger than " + std::to_string(MAXIMUM_MAZE_SIZE) + " units";
							SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, nullptr, message.c_str(), nullptr);
							break;
						}
						if (mapWidth < 2) mapWidth = value;
						else if (mapHeight < 2) {
							mapHeight = value;
							if (mapHeight >= 2) {
								generateMap();
								showInput = false;
							}
						}
						inputText = "";
					}
				}
			} else {
				if (keyIsBackOrEscape(event.key.keysym)) {
					return 5;
				}
				switch (event.key.keysym.sym) {
				case SDLK_F1:
					return 1;
				case SDLK_F2:
					return 2;
				case SDLK_F3:
					return 3;
				case SDLK_F4:
					if (!(SDL_GetModState() & KMOD_ALT)) return 4;
					break;
				case SDLK_F11:
					drawUtils->getWindow()->toggleMaximized();
					break;
				case SDLK_LEFT:
					if (selectedTileTexture > 1) --selectedTileTexture;
					break;
				case SDLK_RIGHT:
					if (selectedTileTexture < (int)tileTextures.size() - 1) ++selectedTileTexture;
					break;
				}
			}
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				drawUtils->getWindow()->setWindowSize(event.window.data1, event.window.data2);
				drawUtils->getWindow()->setViewportSize(event.window.data1, event.window.data2);
				drawUtils->getWindow()->setProjectionMatrixSize(event.window.data1, event.window.data2);
			}
			break;
		case SDL_DROPFILE:
			if (showInput) break;
			loadMap(event.drop.file);
			updateCamera();
			break;
		case SDL_TEXTINPUT:
			if (!showInput) break;
			char character = event.text.text[0];
			if (character >= '0' && character <= '9') {
				if (inputText.size() < 4) inputText += character;
			}
			break;
		}
	}
	return 0;
}

void Editor::handleMouse() {
	int x, y, deltaX, deltaY;
	Uint32 mouseState = SDL_GetMouseState(&x, &y);
	SDL_GetRelativeMouseState(&deltaX, &deltaY);
	selectX = floor((float)(x - cameraX) / blockOffset);
	selectY = floor((float)(y - cameraY) / blockOffset);
	if (y < drawUtils->getWindow()->getViewportHeight() - 59) {
		selectBlockAt(selectX, selectY);
		showSelectedBlock = true;
	} else {
		showSelectedBlock = false;
	}
	if (mouseState & SDL_BUTTON_MMASK) {
		cameraX += deltaX;
		cameraY += deltaY;
		SDL_SetRelativeMouseMode(SDL_TRUE);
		showSelectedBlock = false;
		return;
	} else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
	if (showSelectedBlock) {
		if (mouseState & SDL_BUTTON_RMASK && !(mouseState & SDL_BUTTON_LMASK)) {
			if (selectedBlock) removeBlock(selectedBlock);
			showSelectedBlock = false;
		}
		if (mouseState & SDL_BUTTON_LMASK && !(mouseState & SDL_BUTTON_RMASK)) {
			if (!selectedBlock) {
				if ((types[selectedType] == "Start" && !start) || (types[selectedType] == "Finish" && !finish) || (types[selectedType] != "Start" && types[selectedType] != "Finish")) {
					addBlock(new Block(types[selectedType], selectX, selectY, 0, 0));
				} else if (types[selectedType] == "Start" && start) {
					moveBlock(start, selectX, selectY);
				} else if (types[selectedType] == "Finish" && finish) {
					moveBlock(finish, selectX, selectY);
				}
			}
		}
	}
}

void Editor::addBlock(Block* block) {
	if (block->type == "Start") start = block;
	else if (block->type == "Finish") finish = block;
	blocks.push_back(block);
}

void Editor::moveBlock(Block* block, float x, float y) {
	block->x = x;
	block->y = y;
	block->angle = 0;
}

bool Editor::removeBlock(Block* block) {
	for (auto& b : blocks) {
		if (b == block) {
			if (block->type == "Start") start = nullptr;
			else if (block->type == "Finish") finish = nullptr;
			delete b;
			b = nullptr;
			return true;
		}
	}
	return false;
}

bool Editor::selectBlockAt(float x, float y) {
	for (auto& b : blocks) {
		if (!b) continue;
		if (b->x == x && b->y == y) {
			selectedBlock = b;
			return true;
		}
	}
	selectedBlock = nullptr;
	return false;
}