#pragma once

#include <string>
#include <SDL.h>
#include <list>
#include <map>

#include "gl_core.h"
#include "position.h"
#include "texture.h"
#include "window.h"

class Button;

class DrawUtils {
private:
	Window* window;
	SDL_Surface** fonts;
	std::map<std::string, Texture*>* textures;
public:
	DrawUtils(Window* window, SDL_Surface** fonts, std::map<std::string, Texture*>* textures) : window(window), fonts(fonts), textures(textures) {}
	void setGLES2DOrtho(float width, float height);
	void drawTexture2D(Texture* texture, float x, float y, float scale, float width = 0, float height = 0, float rotationRadians = 0.0f);
	void drawRectangle(SDL_Color color, float x, float y, float width, float height);
	void drawBackground2D(Texture* texture);
	void drawText(const std::string& text, float x, float y, float scale);
	void drawTextInput(const std::string& message, const std::string& input);
	void drawLabel(Texture* texture, float scale = 6.0f);
	void drawLogo(Texture* texture);
	void drawAuthor(Texture* texture);
	void drawButtonGroup(const std::list<Button*>& buttons);
	Window* getWindow() { return window; }
	SDL_Surface** getFonts() { return fonts; }
	std::map<std::string, Texture*>* getTextures() { return textures; }
};

SDL_Surface* loadImage(const std::string& fileName);
Texture* createTextureFromSurface(SDL_Surface* surface);
Texture* createTextureFromImage(const std::string& fileName);
Texture* renderText(const std::string& text, SDL_Surface** fonts);
// specials.png row: ! ? . : - = > < ^ v  (id 0..9); v is id 9 (not ASCII 'v', which uses lowercase)
Texture* renderSpecialsGlyph(int id, SDL_Surface** fonts);
