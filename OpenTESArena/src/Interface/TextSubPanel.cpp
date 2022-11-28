#include "CommonUiView.h"
#include "TextSubPanel.h"
#include "../Assets/ArenaPaletteName.h"
#include "../Assets/ArenaTextureName.h"
#include "../Assets/TextureManager.h"
#include "../Game/Game.h"
#include "../Input/InputActionName.h"
#include "../Math/Rect.h"
#include "../Rendering/Renderer.h"
#include "../UI/CursorAlignment.h"
#include "../UI/CursorData.h"
#include "../UI/FontLibrary.h"
#include "../UI/TextBox.h"

TextSubPanel::TextSubPanel(Game &game)
	: Panel(game) { }

bool TextSubPanel::init(const TextBox::InitInfo &textBoxInitInfo, const std::string_view &text,
	const OnClosedFunction &onClosed, ScopedUiTextureRef &&textureRef, const Int2 &textureCenter)
{
	auto &game = this->getGame();

	if (!this->textBox.init(textBoxInitInfo, text, game.getRenderer()))
	{
		DebugLogError("Couldn't init sub-panel text box.");
		return false;
	}

	this->closeButton = Button<Game&>(
		0,
		0,
		ArenaRenderUtils::SCREEN_WIDTH,
		ArenaRenderUtils::SCREEN_HEIGHT,
		onClosed);

	this->addButtonProxy(MouseButtonType::Left, this->closeButton.getRect(),
		[this, &game]() { this->closeButton.click(game); });
	this->addButtonProxy(MouseButtonType::Right, this->closeButton.getRect(),
		[this, &game]() { this->closeButton.click(game); });

	this->addInputActionListener(InputActionName::Back,
		[this](const InputActionCallbackValues &values)
	{
		if (values.performed)
		{
			this->closeButton.click(values.game);
		}
	});
	
	this->textureRef = std::move(textureRef);

	this->addDrawCall(
		this->textureRef.get(),
		textureCenter,
		Int2(this->textureRef.getWidth(), this->textureRef.getHeight()),
		PivotType::Middle);

	const Rect &textBoxRect = this->textBox.getRect();
	this->addDrawCall(
		this->textBox.getTextureID(),
		textBoxRect.getCenter(),
		Int2(textBoxRect.getWidth(), textBoxRect.getHeight()),
		PivotType::Middle);

	auto &textureManager = game.getTextureManager();
	auto &renderer = game.getRenderer();
	const UiTextureID cursorTextureID = CommonUiView::allocDefaultCursorTexture(textureManager, renderer);
	this->cursorTextureRef.init(cursorTextureID, renderer);
	this->addCursorDrawCall(this->cursorTextureRef.get(), PivotType::TopLeft);

	this->textureCenter = textureCenter;

	return true;
}
