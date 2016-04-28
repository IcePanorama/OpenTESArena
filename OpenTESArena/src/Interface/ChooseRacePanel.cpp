#include <cassert>
#include <iostream>

#include "SDL2\SDL.h"

#include "ChooseRacePanel.h"

#include "Button.h"
#include "ChooseAttributesPanel.h"
#include "ChooseNamePanel.h"
#include "TextBox.h"
#include "../Entities/CharacterClass.h"
#include "../Entities/CharacterGenderName.h"
#include "../Entities/CharacterRaceName.h"
#include "../Game/GameState.h"
#include "../Math/Int2.h"
#include "../Math/Rectangle.h"
#include "../Media/Color.h"
#include "../Media/FontName.h"
#include "../Media/TextureManager.h"
#include "../Media/TextureName.h"
#include "../World/Province.h"
#include "../World/ProvinceName.h"

ChooseRacePanel::ChooseRacePanel(GameState *gameState, CharacterGenderName gender,
	const CharacterClass &charClass, const std::string &name)
	: Panel(gameState)
{
	this->provinceAreas = std::map<ProvinceName, Rectangle>();
	this->parchment = nullptr;
	this->initialTextBox = nullptr;
	this->backToNameButton = nullptr;
	this->gender = nullptr;
	this->charClass = nullptr;

	// Clickable (x, y, width, height) areas for each province.
	this->provinceAreas = std::map<ProvinceName, Rectangle>
	{
		{ ProvinceName::BlackMarsh, Rectangle(216, 144, 55, 12) },
		{ ProvinceName::Elsweyr, Rectangle(148, 127, 37, 11) },
		{ ProvinceName::Hammerfell, Rectangle(72, 75, 50, 11) },
		{ ProvinceName::HighRock, Rectangle(52, 51, 44, 11) },
		{ ProvinceName::ImperialProvince, Rectangle(133, 105, 83, 11) },
		{ ProvinceName::Morrowind, Rectangle(222, 84, 52, 11) },
		{ ProvinceName::Skyrim, Rectangle(142, 44, 34, 11) },
		{ ProvinceName::SummersetIsle, Rectangle(37, 149, 49, 19) },
		{ ProvinceName::Valenwood, Rectangle(106, 147, 49, 10) }
	};

	this->parchment = [gameState]()
	{
		auto *surface = gameState->getTextureManager().getSurface(
			TextureName::ParchmentPopup).getSurface();
		return std::unique_ptr<Surface>(new Surface(surface));
	}();

	this->initialTextBox = [gameState]()
	{
		auto origin = Int2(72, 90);
		auto color = Color(48, 12, 12);
		std::string text = "Choose thy homeland";
		auto fontName = FontName::A;
		return std::unique_ptr<TextBox>(new TextBox(
			origin.getX(),
			origin.getY(),
			color,
			text,
			fontName,
			gameState->getTextureManager()));
	}();

	this->backToNameButton = [gameState, gender, charClass]()
	{
		auto function = [gameState, gender, charClass]()
		{
			auto namePanel = std::unique_ptr<Panel>(new ChooseNamePanel(
				gameState, gender, charClass));
			gameState->setPanel(std::move(namePanel));
		};
		return std::unique_ptr<Button>(new Button(function));
	}();

	// How should the race name be given? "this->getChosenRaceName()"?
	this->acceptButton = [gameState, gender, charClass, name]()
	{
		auto function = [gameState, gender, charClass, name]()
		{
			auto attributesPanel = std::unique_ptr<Panel>(new ChooseAttributesPanel(
				gameState, gender, charClass, name, CharacterRaceName::Nord));
			gameState->setPanel(std::move(attributesPanel));
		};
		return std::unique_ptr<Button>(new Button(function));
	}();

	this->gender = std::unique_ptr<CharacterGenderName>(
		new CharacterGenderName(gender));
	this->charClass = std::unique_ptr<CharacterClass>(
		new CharacterClass(charClass));
	this->name = name;

	// Nine provinces.
	assert(this->provinceAreas.size() == 9);
	assert(this->parchment.get() != nullptr);
	assert(this->initialTextBox.get() != nullptr);
	assert(this->backToNameButton.get() != nullptr);
	assert(this->acceptButton.get() != nullptr);
	assert(this->gender.get() != nullptr);
	assert(*this->gender.get() == gender);
	assert(this->charClass.get() != nullptr);
	assert(this->name.size() > 0);
}

ChooseRacePanel::~ChooseRacePanel()
{

}

void ChooseRacePanel::handleEvents(bool &running)
{
	auto mousePosition = this->getMousePosition();
	auto mouseOriginalPoint = this->nativePointToOriginal(mousePosition);

	SDL_Event e;
	while (SDL_PollEvent(&e) != 0)
	{
		bool applicationExit = (e.type == SDL_QUIT);
		bool resized = (e.type == SDL_WINDOWEVENT) &&
			(e.window.event == SDL_WINDOWEVENT_RESIZED);

		if (applicationExit)
		{
			running = false;
		}
		if (resized)
		{
			int width = e.window.data1;
			int height = e.window.data2;
			this->getGameState()->resizeWindow(width, height);
		}

		bool leftClick = (e.type == SDL_MOUSEBUTTONDOWN) &&
			(e.button.button == SDL_BUTTON_LEFT);
		bool rightClick = (e.type == SDL_MOUSEBUTTONDOWN) &&
			(e.button.button == SDL_BUTTON_RIGHT);

		bool escapePressed = (e.type == SDL_KEYDOWN) &&
			(e.key.keysym.sym == SDLK_ESCAPE);
		bool enterPressed = (e.type == SDL_KEYDOWN) &&
			((e.key.keysym.sym == SDLK_RETURN) ||
				(e.key.keysym.sym == SDLK_KP_ENTER));
		bool spacePressed = (e.type == SDL_KEYDOWN) &&
			(e.key.keysym.sym == SDLK_SPACE);

		// Context-sensitive input depending on the visibility of the first text box.
		if (this->initialTextBox->isVisible())
		{
			bool hideInitialPopUp = leftClick || rightClick || enterPressed || 
				spacePressed || escapePressed;

			if (hideInitialPopUp)
			{
				// Hide the initial text box.
				this->initialTextBox->setVisibility(false);
			}
		}
		else
		{
			if (escapePressed)
			{
				// Go back to the name panel.
				this->backToNameButton->click();
			}
			else if (leftClick)
			{
				// Listen for map clicks.
				for (const auto &area : this->provinceAreas)
				{
					if (area.second.contains(mouseOriginalPoint))
					{
						// Save the clicked province name...?

						// Go to the attributes panel.
						this->acceptButton->click();
						break;
					}
				}
			}			
		}
	}
}

void ChooseRacePanel::handleMouse(double dt)
{
	static_cast<void>(dt);
}

void ChooseRacePanel::handleKeyboard(double dt)
{
	static_cast<void>(dt);
}

void ChooseRacePanel::tick(double dt, bool &running)
{
	static_cast<void>(dt);

	this->handleEvents(running);
}

void ChooseRacePanel::drawProvinceTooltip(ProvinceName provinceName, SDL_Surface *dst)
{
	auto mouseOriginalPosition = this->nativePointToOriginal(this->getMousePosition());
	const auto raceName = Province(provinceName).getRaceName(true);
	auto tooltip = std::unique_ptr<TextBox>(new TextBox(
		mouseOriginalPosition.getX(), mouseOriginalPosition.getY(),
		Color::White, "Land of the " + raceName, FontName::A,
		this->getGameState()->getTextureManager()));

	const int originalWidth = 320;
	const int originalHeight = 200;
	const int width = tooltip->getWidth() / 2;
	const int height = tooltip->getHeight() / 2;
	const int x = ((tooltip->getX() + width) < originalWidth) ? tooltip->getX() :
		(tooltip->getX() - width);
	const int y = ((tooltip->getY() + height) < originalHeight) ? tooltip->getY() :
		(tooltip->getY() - height);

	this->drawScaledToNative(*tooltip.get(), x, y, width, height, dst);
}

void ChooseRacePanel::render(SDL_Surface *dst, const SDL_Rect *letterbox)
{
	// Clear full screen.
	this->clearScreen(dst);

	// Draw background map.
	const auto &worldMap = this->getGameState()->getTextureManager()
		.getSurface(TextureName::WorldMap);
	this->drawLetterbox(worldMap, dst, letterbox);

	// Draw visible parchments and text.
	this->parchment->setTransparentColor(Color::Magenta);
	if (this->initialTextBox->isVisible())
	{
		const int originalWidth = 320;
		const int parchmentWidth = static_cast<int>(
			static_cast<double>(this->parchment->getWidth()) * 1.25);
		const int parchmentX = (originalWidth / 2) - (parchmentWidth / 2);
		const int parchmentY = 75;
		this->drawScaledToNative(*this->parchment.get(),
			parchmentX,
			parchmentY,
			parchmentWidth,
			this->parchment->getHeight(),
			dst);
		this->drawScaledToNative(*this->initialTextBox.get(), dst);
	}

	// Draw cursor.
	const auto &cursor = this->getGameState()->getTextureManager()
		.getSurface(TextureName::SwordCursor);
	this->drawCursor(cursor, dst);

	// Draw hovered province tooltip.
	if (!this->initialTextBox->isVisible())
	{
		auto mouseOriginalPosition = 
			this->nativePointToOriginal(this->getMousePosition());

		for (const auto &pair : this->provinceAreas)
		{
			// Draw tooltip if the mouse is in the province.
			if (pair.second.contains(mouseOriginalPosition))
			{
				this->drawProvinceTooltip(pair.first, dst);
			}
		}
	}
}
