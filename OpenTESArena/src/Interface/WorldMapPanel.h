#ifndef WORLD_MAP_PANEL_H
#define WORLD_MAP_PANEL_H

#include "Panel.h"
#include "ProvinceMapUiModel.h"
#include "WorldMapUiModel.h"
#include "../Math/Vector2.h"
#include "../UI/Button.h"

class Renderer;

class WorldMapPanel : public Panel
{
private:
	Button<Game&> backToGameButton;
	Button<Game&, int, std::unique_ptr<ProvinceMapUiModel::TravelData>> provinceButton;
	Buffer<Int2> provinceNameOffsets; // Yellow province name positions.
	std::unique_ptr<ProvinceMapUiModel::TravelData> travelData;
public:
	WorldMapPanel(Game &game, std::unique_ptr<ProvinceMapUiModel::TravelData> travelData);
	virtual ~WorldMapPanel() = default;

	virtual std::optional<Panel::CursorData> getCurrentCursor() const override;
	virtual void handleEvent(const SDL_Event &e) override;
	virtual void render(Renderer &renderer) override;
};

#endif
