#include <iostream>
#include <memory>

#include "editor.h"
#if defined(__ANDROID__)
#include "android_picker.h"
namespace maze_picker = maze_android;
#elif defined(__EMSCRIPTEN__)
#include "web_support.h"
namespace maze_picker = maze_web;
#endif

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
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
		{
			std::string picked;
			if (maze_picker::consumePickedMap(picked)) {
				std::stringstream ss(picked);
				loadMapWithHeader(ss);
				updateCamera();
			}
		}
#endif
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
	if (keyboardDismissedByUser) return;
	const bool shown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
		&& (SDL_IsScreenKeyboardShown(win->getWindow()) == SDL_TRUE);
	if (shown) return;
	// Force a re-show: gesture-nav back can hide the IME without flipping SDL_IsTextInputActive,
	// so a plain SDL_StartTextInput would be a no-op. Stop first to clear any stale state.
	if (SDL_IsTextInputActive()) {
		SDL_StopTextInput();
	}
	SDL_StartTextInput();
	setTextInputRect();
	prevTextInputActive = true;
	prevKeyboardShown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
		&& (SDL_IsScreenKeyboardShown(win->getWindow()) == SDL_TRUE);
	SDL_RaiseWindow(win->getWindow());
}

void Editor::Android::syncTextInputState() {
	Window* win = editor->drawUtils->getWindow();
	if (!win || !win->getWindow()) return;
	if (editor->showInput) {
		const bool active = SDL_IsTextInputActive();
		const bool shown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
			&& (SDL_IsScreenKeyboardShown(win->getWindow()) == SDL_TRUE);
		// Detect dismissal two ways: (a) SDL flipped IsTextInputActive off — happens when the
		// IME path through onKeyPreIme fires SDL_StopTextInput; (b) the IME visibility flipped
		// off while SDL still thinks text input is active — happens with Android's gesture-nav
		// back, which hides the IME without delivering KEYCODE_BACK to the app.
		if ((prevTextInputActive && !active) || (prevKeyboardShown && !shown)) {
			keyboardDismissedByUser = true;
		}
		if (!active && !keyboardDismissedByUser) {
			SDL_StartTextInput();
			setTextInputRect();
			prevTextInputActive = SDL_IsTextInputActive();
		} else if (active) {
			setTextInputRect();
			prevTextInputActive = true;
		} else {
			prevTextInputActive = false;
		}
		prevKeyboardShown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
			&& (SDL_IsScreenKeyboardShown(win->getWindow()) == SDL_TRUE);
	} else {
		if (SDL_IsTextInputActive()) {
			SDL_StopTextInput();
		}
		keyboardDismissedByUser = false;
		prevTextInputActive = false;
		prevKeyboardShown = false;
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
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
	// Build the same file content saveMap(fileName) would write, then hand it over:
	// on Android MainActivity.java prompts for a destination URI via SAF; on the
	// web a browser save dialog asks for the destination (or downloads the bytes
	// where the File System Access API is unavailable).
	std::stringstream ss;
	ss << skyboxTexture << "\n";
	ss << tileTextures.at(selectedTileTexture) << "\n";
	for (auto& block : blocks) {
		if (block) {
			ss << block->type << " " << block->x * 3 << " " << 0 << " " << block->y * 3 << " " << block->angle << "\n";
		}
	}
	maze_picker::launchSaveMapPicker(ss.str());
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
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
	// Async on Android and the web: launch the picker and return. The editor's
	// main loop polls maze_picker::consumePickedMap() each iteration.
	maze_picker::launchOpenMapPicker();
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

void Editor::loadMapWithHeader(std::stringstream& data) {
	if (!blocks.empty()) newMap();
	data >> skyboxTexture;
	data >> tileTexture;
	updateSelectedTileTexture();
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
		if (mapWidth < 2) drawUtils->drawTextInput("Maze width: ", inputText, 4.0f, 32.0f);
		else if (mapHeight < 2) drawUtils->drawTextInput("Maze height: ", inputText, 4.0f, 32.0f);
	}
}

void Editor::updateFrame() {
	SDL_GL_SwapWindow(drawUtils->getWindow()->getWindow());
#if defined(__EMSCRIPTEN__)
	// The editor is a nested loop; yield here so the browser can present the
	// frame and deliver input events (the editor runs with vsync off natively,
	// so pace it to ~60 Hz explicitly).
	maze_web::frameYield();
#endif
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
				const float w = (float)drawUtils->getWindow()->getViewportWidth();
				const float h = (float)drawUtils->getWindow()->getViewportHeight();
				const float px = event.tfinger.x * w;
				const float py = event.tfinger.y * h;
				// Android reserves a strip on the left and right edges for the system back
				// gesture; a swipe starting there must not be misread as a tap-to-reshow —
				// otherwise the back gesture that should close this prompt re-raises the
				// keyboard instead. The bottom 59px strip is the menu bar (see drawGui);
				// tapping it is not a request to type either.
				const float edgeInset = fmaxf(48.0f, w * 0.05f);
				if (px > edgeInset && px < w - edgeInset && py < h - 59.0f) {
					android.keyboardTap.active = true;
					android.keyboardTap.fingerId = event.tfinger.fingerId;
					android.keyboardTap.x = px;
					android.keyboardTap.y = py;
					android.keyboardTap.startTicks = SDL_GetTicks();
				} else {
					android.keyboardTap.active = false;
				}
			}
			break;
		case SDL_FINGERMOTION:
			if (android.keyboardTap.active && event.tfinger.fingerId == android.keyboardTap.fingerId) {
				const float w = (float)drawUtils->getWindow()->getViewportWidth();
				const float h = (float)drawUtils->getWindow()->getViewportHeight();
				const float px = event.tfinger.x * w;
				const float py = event.tfinger.y * h;
				if (std::hypot(px - android.keyboardTap.x, py - android.keyboardTap.y) > 32.0f) {
					android.keyboardTap.active = false;
				}
			}
			break;
		case SDL_FINGERUP:
			if (showInput && android.keyboardTap.active && event.tfinger.fingerId == android.keyboardTap.fingerId) {
				const float w = (float)drawUtils->getWindow()->getViewportWidth();
				const float h = (float)drawUtils->getWindow()->getViewportHeight();
				const float px = event.tfinger.x * w;
				const float py = event.tfinger.y * h;
				const float edgeInset = fmaxf(48.0f, w * 0.05f);
				const bool liftedAtEdge = (px <= edgeInset || px >= w - edgeInset);
				const float d = std::hypot(px - android.keyboardTap.x, py - android.keyboardTap.y);
				if (!liftedAtEdge && d <= 32.0f && (float)(SDL_GetTicks() - android.keyboardTap.startTicks) <= 450.0f) {
					android.keyboardDismissedByUser = false;
					android.requestScreenKeyboardOnTap();
				}
			}
			android.keyboardTap.active = false;
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
				// Touches arrive again as synthetic mouse events (SDL_TOUCH_MOUSEID); those
				// are handled by the FINGERDOWN/FINGERUP tap tracker above. Only a real
				// mouse click may re-raise the keyboard directly.
				if (event.button.which != SDL_TOUCH_MOUSEID
					&& event.button.y < drawUtils->getWindow()->getViewportHeight() - 59.0f) {
					android.keyboardDismissedByUser = false;
					android.requestScreenKeyboardOnTap();
				}
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