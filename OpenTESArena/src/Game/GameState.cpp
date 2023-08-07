#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

#include "ArenaClockUtils.h"
#include "Game.h"
#include "GameState.h"
#include "../Assets/ArenaPaletteName.h"
#include "../Assets/ArenaTextureName.h"
#include "../Assets/ExeData.h"
#include "../Assets/INFFile.h"
#include "../Assets/MIFFile.h"
#include "../Assets/RMDFile.h"
#include "../Assets/TextureManager.h"
#include "../Audio/MusicLibrary.h"
#include "../Entities/EntityDefinitionLibrary.h"
#include "../Entities/Player.h"
#include "../GameLogic/MapLogicController.h"
#include "../GameLogic/PlayerLogicController.h"
#include "../Interface/GameWorldUiView.h"
#include "../Math/Constants.h"
#include "../Rendering/RenderCamera.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/RendererUtils.h"
#include "../UI/TextAlignment.h"
#include "../UI/TextBox.h"
#include "../UI/TextRenderUtils.h"
#include "../Voxels/ArenaVoxelUtils.h"
#include "../Weather/ArenaWeatherUtils.h"
#include "../Weather/WeatherUtils.h"
#include "../World/MapType.h"
#include "../WorldMap/ArenaLocationUtils.h"
#include "../WorldMap/LocationDefinition.h"
#include "../WorldMap/LocationInstance.h"

#include "components/debug/Debug.h"
#include "components/utilities/String.h"

GameState::WorldMapLocationIDs::WorldMapLocationIDs(int provinceID, int locationID)
{
	this->provinceID = provinceID;
	this->locationID = locationID;
}

GameState::GameState()
{
	DebugLog("Initializing.");

	this->activeLevelIndex = -1;
	this->nextMapClearsPrevious = false;
	this->nextLevelIndex = -1;

	this->triggerTextRemainingSeconds = 0.0;
	this->actionTextRemainingSeconds = 0.0;
	this->effectTextRemainingSeconds = 0.0;
	this->clearSession();
}

GameState::~GameState()
{
	DebugLog("Closing.");
}

void GameState::init(ArenaRandom &random)
{
	// @todo: might want a clearSession()? Seems weird.

	// Initialize world map definition and instance to default.
	const BinaryAssetLibrary &binaryAssetLibrary = BinaryAssetLibrary::getInstance();
	this->worldMapDef.init(binaryAssetLibrary);
	this->worldMapInst.init(this->worldMapDef);

	// @temp: set main quest dungeons visible for testing.
	for (int i = 0; i < this->worldMapInst.getProvinceCount(); i++)
	{
		ProvinceInstance &provinceInst = this->worldMapInst.getProvinceInstance(i);
		const int provinceDefIndex = provinceInst.getProvinceDefIndex();
		const ProvinceDefinition &provinceDef = this->worldMapDef.getProvinceDef(provinceDefIndex);

		for (int j = 0; j < provinceInst.getLocationCount(); j++)
		{
			LocationInstance &locationInst = provinceInst.getLocationInstance(j);
			const int locationDefIndex = locationInst.getLocationDefIndex();
			const LocationDefinition &locationDef = provinceDef.getLocationDef(locationDefIndex);
			const std::string &locationName = locationInst.getName(locationDef);

			const bool isMainQuestDungeon = locationDef.getType() == LocationDefinitionType::MainQuestDungeon;
			const bool isStartDungeon = isMainQuestDungeon &&
				(locationDef.getMainQuestDungeonDefinition().type == LocationMainQuestDungeonDefinitionType::Start);
			const bool shouldSetVisible = (locationName.size() > 0) &&
				isMainQuestDungeon && !isStartDungeon && !locationInst.isVisible();

			if (shouldSetVisible)
			{
				locationInst.toggleVisibility();
			}
		}
	}

	// Do initial weather update (to set each value to a valid state).
	this->updateWeatherList(random, binaryAssetLibrary.getExeData());

	this->date = Date();
	this->weatherInst = WeatherInstance();
}

void GameState::clearSession()
{
	// @todo: this function doesn't clear everything, i.e. weather state. Might want to revise later.

	// Don't have to clear on-screen text box durations.
	this->provinceIndex = -1;
	this->locationIndex = -1;

	this->isCamping = false;
	this->chasmAnimSeconds = 0.0;

	this->travelData = std::nullopt;
	this->clearMaps();
	
	this->onLevelUpVoxelEnter = std::function<void(Game&)>();

	this->weatherDef.initClear();
}

bool GameState::hasPendingLevelIndexChange() const
{
	return this->nextLevelIndex >= 0;
}

bool GameState::hasPendingMapDefChange() const
{
	return this->nextMapDef.isValid();
}

bool GameState::hasPendingSceneChange() const
{
	return this->hasPendingLevelIndexChange() || this->hasPendingMapDefChange();
}

void GameState::queueLevelIndexChange(int newLevelIndex, const VoxelInt2 &playerStartOffset)
{
	if (this->hasPendingLevelIndexChange())
	{
		DebugLogError("Already queued level index change to level " + std::to_string(this->nextLevelIndex) + ".");
		return;
	}

	if (this->hasPendingMapDefChange())
	{
		DebugLogError("Already changing map definition change to " + std::to_string(static_cast<int>(this->nextMapDef.getMapType())) + " this frame.");
		return;
	}

	this->nextLevelIndex = newLevelIndex;
	this->nextMapPlayerStartOffset = playerStartOffset;
}

void GameState::queueMapDefChange(MapDefinition &&newMapDef, const std::optional<CoordInt2> &startCoord,
	const std::optional<CoordInt3> &returnCoord, const VoxelInt2 &playerStartOffset,
	const std::optional<WorldMapLocationIDs> &worldMapLocationIDs, bool clearPreviousMap,
	const std::optional<WeatherDefinition> &weatherDef)
{
	if (this->hasPendingMapDefChange())
	{
		DebugLogError("Already queued map definition change to " + std::to_string(static_cast<int>(this->nextMapDef.getMapType())) + ".");
		return;
	}

	if (this->hasPendingLevelIndexChange())
	{
		DebugLogError("Already changing level index to " + std::to_string(this->nextLevelIndex) + " this frame.");
		return;
	}

	this->nextMapDef = std::move(newMapDef);
	this->nextMapStartCoord = startCoord;
	this->prevMapReturnCoord = returnCoord;
	this->nextMapPlayerStartOffset = playerStartOffset;
	this->nextMapDefLocationIDs = worldMapLocationIDs;
	this->nextMapClearsPrevious = clearPreviousMap;
	this->nextMapDefWeatherDef = weatherDef;
}

void GameState::queueMapDefPop()
{
	if (this->hasPendingMapDefChange())
	{
		DebugLogError("Already queued map definition change to " + std::to_string(static_cast<int>(this->nextMapDef.getMapType())) + ".");
		return;
	}

	if (this->hasPendingLevelIndexChange())
	{
		DebugLogError("Already changing level index to " + std::to_string(this->nextLevelIndex) + " this frame.");
		return;
	}

	if (!this->isActiveMapNested())
	{
		DebugLogWarning("No exterior map to return to.");
		return;
	}

	if (!this->prevMapReturnCoord.has_value())
	{
		DebugLogWarning("Expected previous map return coord to be set.");
		return;
	}

	this->nextMapDef = std::move(this->prevMapDef);
	this->prevMapDef.clear();

	this->nextMapPlayerStartOffset = VoxelInt2::Zero;
	this->nextMapDefLocationIDs = std::nullopt;

	// Calculate weather.
	const ArenaTypes::WeatherType weatherType = this->getWeatherForLocation(this->provinceIndex, this->locationIndex);
	Random random; // @todo: get from Game
	this->nextMapDefWeatherDef = WeatherDefinition();
	this->nextMapDefWeatherDef->initFromClassic(weatherType, this->date.getDay(), random);

	this->nextMapClearsPrevious = true;
}

void GameState::queueMusicOnSceneChange(const SceneChangeMusicFunc &musicFunc, const SceneChangeMusicFunc &jingleMusicFunc)
{
	if (this->nextMusicFunc || this->nextJingleMusicFunc)
	{
		DebugLogError("Already have music queued on map change.");
		return;
	}

	this->nextMusicFunc = musicFunc;
	this->nextJingleMusicFunc = jingleMusicFunc;
}

MapType GameState::getActiveMapType() const
{
	return this->getActiveMapDef().getMapType();
}

bool GameState::isActiveMapValid() const
{
	return this->activeMapDef.isValid() && (this->activeLevelIndex >= 0);
}

int GameState::getActiveLevelIndex() const
{
	return this->activeLevelIndex;
}

int GameState::getActiveSkyIndex() const
{
	if (!this->isActiveMapValid())
	{
		DebugLogError("No valid map for obtaining active sky index.");
		return -1;
	}

	return this->activeMapDef.getSkyIndexForLevel(this->activeLevelIndex);
}

const MapDefinition &GameState::getActiveMapDef() const
{
	return this->activeMapDef;
}

double GameState::getActiveCeilingScale() const
{
	if (!this->isActiveMapValid())
	{
		DebugLogError("No valid map for obtaining ceiling scale.");
		return 0.0;
	}

	BufferView<const LevelInfoDefinition> levelInfoDefs = this->activeMapDef.getLevelInfos();
	const LevelInfoDefinition &levelInfoDef = levelInfoDefs[this->activeLevelIndex];
	return levelInfoDef.getCeilingScale();
}

bool GameState::isActiveMapNested() const
{
	return this->prevMapDef.isValid();
}

WorldMapInstance &GameState::getWorldMapInstance()
{
	return this->worldMapInst;
}

const WorldMapDefinition &GameState::getWorldMapDefinition() const
{
	return this->worldMapDef;
}

const ProvinceDefinition &GameState::getProvinceDefinition() const
{
	return this->worldMapDef.getProvinceDef(this->provinceIndex);
}

const LocationDefinition &GameState::getLocationDefinition() const
{
	const ProvinceDefinition &provinceDef = this->getProvinceDefinition();
	return provinceDef.getLocationDef(this->locationIndex);
}

ProvinceInstance &GameState::getProvinceInstance()
{
	return this->worldMapInst.getProvinceInstance(this->provinceIndex);
}

LocationInstance &GameState::getLocationInstance()
{
	ProvinceInstance &provinceInst = this->getProvinceInstance();
	return provinceInst.getLocationInstance(this->locationIndex);
}

const ProvinceMapUiModel::TravelData *GameState::getTravelData() const
{
	return this->travelData.has_value() ? &(*this->travelData) : nullptr;
}

BufferView<const ArenaTypes::WeatherType> GameState::getWorldMapWeathers() const
{
	return this->worldMapWeathers;
}

ArenaTypes::WeatherType GameState::getWeatherForLocation(int provinceIndex, int locationIndex) const
{
	const BinaryAssetLibrary &binaryAssetLibrary = BinaryAssetLibrary::getInstance();
	const ProvinceDefinition &provinceDef = this->worldMapDef.getProvinceDef(provinceIndex);
	const LocationDefinition &locationDef = provinceDef.getLocationDef(locationIndex);
	const Int2 localPoint(locationDef.getScreenX(), locationDef.getScreenY());
	const Int2 globalPoint = ArenaLocationUtils::getGlobalPoint(localPoint, provinceDef.getGlobalRect());
	const int quarterIndex = ArenaLocationUtils::getGlobalQuarter(globalPoint, binaryAssetLibrary.getCityDataFile());
	DebugAssertIndex(this->worldMapWeathers, quarterIndex);
	ArenaTypes::WeatherType weatherType = this->worldMapWeathers[quarterIndex];

	if (locationDef.getType() == LocationDefinitionType::City)
	{
		// Filter the possible weathers (in case it's trying to have snow in a desert).
		const LocationCityDefinition &locationCityDef = locationDef.getCityDefinition();
		const ArenaTypes::ClimateType climateType = locationCityDef.climateType;
		weatherType = ArenaWeatherUtils::getFilteredWeatherType(weatherType, climateType);
	}

	return weatherType;
}

Date &GameState::getDate()
{
	return this->date;
}

Clock &GameState::getClock()
{
	return this->clock;
}

double GameState::getDaytimePercent() const
{
	return this->clock.getPreciseTotalSeconds() / static_cast<double>(Clock::SECONDS_IN_A_DAY);
}

double GameState::getChasmAnimPercent() const
{
	const double percent = this->chasmAnimSeconds / ArenaVoxelUtils::CHASM_ANIM_SECONDS;
	return std::clamp(percent, 0.0, Constants::JustBelowOne);
}

const WeatherDefinition &GameState::getWeatherDefinition() const
{
	return this->weatherDef;
}

const WeatherInstance &GameState::getWeatherInstance() const
{
	return this->weatherInst;
}

std::function<void(Game&)> &GameState::getOnLevelUpVoxelEnter()
{
	return this->onLevelUpVoxelEnter;
}

bool GameState::triggerTextIsVisible() const
{
	return this->triggerTextRemainingSeconds > 0.0;
}

bool GameState::actionTextIsVisible() const
{
	return this->actionTextRemainingSeconds > 0.0;
}

bool GameState::effectTextIsVisible() const
{
	return this->effectTextRemainingSeconds > 0.0;
}

void GameState::setIsCamping(bool isCamping)
{
	this->isCamping = isCamping;
}

void GameState::setTravelData(std::optional<ProvinceMapUiModel::TravelData> travelData)
{
	this->travelData = std::move(travelData);
}

void GameState::setTriggerTextDuration(const std::string_view &text)
{
	this->triggerTextRemainingSeconds = GameWorldUiView::getTriggerTextSeconds(text);
}

void GameState::setActionTextDuration(const std::string_view &text)
{
	this->actionTextRemainingSeconds = GameWorldUiView::getActionTextSeconds(text);
}

void GameState::setEffectTextDuration(const std::string_view &text)
{
	// @todo
	DebugNotImplemented();
}

void GameState::resetTriggerTextDuration()
{
	this->triggerTextRemainingSeconds = 0.0;
}

void GameState::resetActionTextDuration()
{
	this->actionTextRemainingSeconds = 0.0;
}

void GameState::resetEffectTextDuration()
{
	this->effectTextRemainingSeconds = 0.0;
}

void GameState::clearMaps()
{
	this->activeMapDef.clear();
	this->activeLevelIndex = -1;
	this->prevMapDef.clear();
	this->prevMapReturnCoord = std::nullopt;
	this->nextMapDef.clear();
	this->nextMapPlayerStartOffset = VoxelInt2::Zero;
	this->nextMapDefLocationIDs = std::nullopt;
	this->nextMapDefWeatherDef = std::nullopt;
	this->nextMapClearsPrevious = false;
	this->nextLevelIndex = -1;
	this->nextMusicFunc = SceneChangeMusicFunc();
	this->nextJingleMusicFunc = SceneChangeMusicFunc();
}

void GameState::updateWeatherList(ArenaRandom &random, const ExeData &exeData)
{
	const int seasonIndex = this->date.getSeason();

	const size_t weatherCount = std::size(this->worldMapWeathers);
	const auto &climates = exeData.locations.climates;
	DebugAssert(climates.size() == weatherCount);

	for (size_t i = 0; i < weatherCount; i++)
	{
		const int climateIndex = climates[i];
		const int variantIndex = [&random]()
		{
			// 40% for 2, 20% for 1, 20% for 3, 10% for 0, and 10% for 4.
			const int val = random.next() % 100;

			if (val >= 60)
			{
				return 2;
			}
			else if (val >= 40)
			{
				return 1;
			}
			else if (val >= 20)
			{
				return 3;
			}
			else if (val >= 10)
			{
				return 0;
			}
			else
			{
				return 4;
			}
		}();

		const int weatherTableIndex = (climateIndex * 20) + (seasonIndex * 5) + variantIndex;
		const auto &weatherTable = exeData.locations.weatherTable;
		DebugAssertIndex(weatherTable, weatherTableIndex);
		this->worldMapWeathers[i] = static_cast<ArenaTypes::WeatherType>(weatherTable[weatherTableIndex]);
	}
}

void GameState::applyPendingSceneChange(Game &game, double dt)
{
	Player &player = game.getPlayer();

	const VoxelDouble2 startOffset(
		static_cast<SNDouble>(this->nextMapPlayerStartOffset.x),
		static_cast<WEDouble>(this->nextMapPlayerStartOffset.y));

	if (this->hasPendingMapDefChange())
	{
		if (!this->nextMapClearsPrevious)
		{
			this->prevMapDef = std::move(this->activeMapDef);
		}

		this->activeMapDef.clear();

		const bool shouldPopReturnCoord = this->prevMapReturnCoord.has_value() && this->nextMapClearsPrevious;
		this->nextMapClearsPrevious = false;

		if (this->nextMapDefLocationIDs.has_value())
		{
			this->provinceIndex = this->nextMapDefLocationIDs->provinceID;
			this->locationIndex = this->nextMapDefLocationIDs->locationID;
			this->nextMapDefLocationIDs = std::nullopt;
		}

		const std::optional<int> &nextMapStartLevelIndex = this->nextMapDef.getStartLevelIndex();
		this->activeLevelIndex = nextMapStartLevelIndex.value_or(0);

		this->activeMapDef = std::move(this->nextMapDef);
		this->nextMapDef.clear();

		if (this->nextMapDefWeatherDef.has_value())
		{
			this->weatherDef = std::move(*this->nextMapDefWeatherDef);
			this->nextMapDefWeatherDef = std::nullopt;
		}

		CoordDouble2 startCoord;
		if (this->nextMapStartCoord.has_value())
		{
			const VoxelInt2 startVoxelXZ(
				this->nextMapStartCoord->voxel.x,
				this->nextMapStartCoord->voxel.y);
			startCoord = CoordDouble2(this->nextMapStartCoord->chunk, VoxelUtils::getVoxelCenter(startVoxelXZ));
			this->nextMapStartCoord = std::nullopt;
		}
		else if (shouldPopReturnCoord)
		{
			const VoxelInt2 returnVoxelXZ(
				this->prevMapReturnCoord->voxel.x,
				this->prevMapReturnCoord->voxel.z);
			startCoord = CoordDouble2(this->prevMapReturnCoord->chunk, VoxelUtils::getVoxelCenter(returnVoxelXZ));
			this->prevMapReturnCoord = std::nullopt;
		}
		else if (this->activeMapDef.getStartPointCount() > 0)
		{
			const WorldDouble2 startPoint = this->activeMapDef.getStartPoint(0);
			startCoord = VoxelUtils::worldPointToCoord(startPoint);
		}
		else
		{
			DebugLogWarning("No valid start coord for map definition change.");
		}

		const double ceilingScale = this->getActiveCeilingScale();
		const CoordDouble3 newPlayerPos(
			startCoord.chunk,
			VoxelDouble3(startCoord.point.x + startOffset.x, ceilingScale + Player::HEIGHT, startCoord.point.y + startOffset.y));

		player.teleport(newPlayerPos);

		this->nextMapPlayerStartOffset = VoxelInt2::Zero;
	}
	else if (this->hasPendingLevelIndexChange())
	{
		this->activeLevelIndex = this->nextLevelIndex;
		this->nextLevelIndex = -1;

		const double ceilingScale = this->getActiveCeilingScale();

		const CoordDouble3 oldPlayerPos = player.getPosition(); // The player should be inside the transition voxel.
		const VoxelInt3 oldPlayerVoxel = VoxelUtils::pointToVoxel(oldPlayerPos.point);
		const VoxelDouble3 oldPlayerCenteredPoint = VoxelUtils::getVoxelCenter(oldPlayerVoxel);
		const CoordDouble3 newPlayerPos(
			oldPlayerPos.chunk,
			VoxelDouble3(oldPlayerCenteredPoint.x + startOffset.x, ceilingScale + Player::HEIGHT, oldPlayerCenteredPoint.z + startOffset.y));

		player.teleport(newPlayerPos);
		player.lookAt(newPlayerPos + VoxelDouble3(startOffset.x, 0.0, startOffset.y));

		this->nextMapPlayerStartOffset = VoxelInt2::Zero;
	}
	else
	{
		DebugNotImplementedMsg("Unhandled scene change case.");
	}

	player.setVelocityToZero();

	TextureManager &textureManager = game.getTextureManager();
	Renderer &renderer = game.getRenderer();
	SceneManager &sceneManager = game.getSceneManager();

	const CoordDouble3 &playerCoord = player.getPosition();

	// Clear and re-populate scene immediately so it's ready for rendering this frame (otherwise we get a black frame).
	const Options &options = game.getOptions();
	ChunkManager &chunkManager = sceneManager.chunkManager;
	chunkManager.clear();
	chunkManager.update(playerCoord.chunk, options.getMisc_ChunkDistance());

	sceneManager.voxelChunkManager.recycleAllChunks();
	sceneManager.entityChunkManager.clear();
	sceneManager.collisionChunkManager.recycleAllChunks();
	sceneManager.renderChunkManager.unloadScene(renderer);
	
	sceneManager.skyInstance.clear();
	sceneManager.renderSkyManager.unloadScene(renderer);
	sceneManager.renderWeatherManager.unloadScene();

	const MapType activeMapType = this->getActiveMapType();
	const int activeSkyIndex = this->getActiveSkyIndex();
	const SkyDefinition &activeSkyDef = this->activeMapDef.getSky(activeSkyIndex);
	const SkyInfoDefinition &activeSkyInfoDef = this->activeMapDef.getSkyInfoForSky(activeSkyIndex);

	sceneManager.skyInstance.init(activeSkyDef, activeSkyInfoDef, this->date.getDay(), textureManager);
	sceneManager.renderSkyManager.loadScene(activeSkyInfoDef, textureManager, renderer);
	sceneManager.renderWeatherManager.loadScene();

	const BinaryAssetLibrary &binaryAssetLibrary = BinaryAssetLibrary::getInstance();
	this->weatherInst.init(this->weatherDef, this->clock, binaryAssetLibrary.getExeData(), game.getRandom(), textureManager);

	this->tickVoxels(0.0, game);
	this->tickEntities(0.0, game);
	this->tickCollision(0.0, game);
	this->tickSky(0.0, game);
	this->tickRendering(game);

	if (this->nextMusicFunc)
	{
		const MusicDefinition *musicDef = this->nextMusicFunc(game);
		const MusicDefinition *jingleMusicDef = nullptr;
		if (this->nextJingleMusicFunc)
		{
			jingleMusicDef = this->nextJingleMusicFunc(game);
		}

		AudioManager &audioManager = game.getAudioManager();
		audioManager.setMusic(musicDef, jingleMusicDef);

		this->nextMusicFunc = SceneChangeMusicFunc();
		this->nextJingleMusicFunc = SceneChangeMusicFunc();
	}
}

void GameState::tickGameClock(double dt, Game &game)
{
	DebugAssert(dt >= 0.0);

	// Tick the game clock.
	const Clock prevClock = this->clock;
	const double timeScale = GameState::GAME_TIME_SCALE * (this->isCamping ? 250.0 : 1.0);
	this->clock.tick(dt * timeScale);

	// Check if the hour changed.
	const int prevHour = prevClock.getHours24();
	const int newHour = this->clock.getHours24();
	if (newHour != prevHour)
	{
		// Update the weather list that's used for selecting the current one.
		const auto &exeData = BinaryAssetLibrary::getInstance().getExeData();
		this->updateWeatherList(game.getArenaRandom(), exeData);
	}

	// Check if the clock hour looped back around.
	if (newHour < prevHour)
	{
		// Increment the day.
		this->date.incrementDay();
	}

	// See if the clock passed the boundary between night and day, and vice versa.
	const double oldClockTime = prevClock.getPreciseTotalSeconds();
	const double newClockTime = this->clock.getPreciseTotalSeconds();
	const double lamppostActivateTime = ArenaClockUtils::LamppostActivate.getPreciseTotalSeconds();
	const double lamppostDeactivateTime = ArenaClockUtils::LamppostDeactivate.getPreciseTotalSeconds();
	const bool activateNightLights = (oldClockTime < lamppostActivateTime) && (newClockTime >= lamppostActivateTime);
	const bool deactivateNightLights = (oldClockTime < lamppostDeactivateTime) && (newClockTime >= lamppostDeactivateTime);

	if (activateNightLights)
	{
		MapLogicController::handleNightLightChange(game, true);
	}
	else if (deactivateNightLights)
	{
		MapLogicController::handleNightLightChange(game, false);
	}

	// Check for changes in exterior music depending on the time.
	const MapDefinition &activeMapDef = this->getActiveMapDef();
	const MapType activeMapType = activeMapDef.getMapType();
	if ((activeMapType == MapType::City) || (activeMapType == MapType::Wilderness))
	{
		AudioManager &audioManager = game.getAudioManager();
		const MusicLibrary &musicLibrary = MusicLibrary::getInstance();
		const double dayMusicStartTime = ArenaClockUtils::MusicSwitchToDay.getPreciseTotalSeconds();
		const double nightMusicStartTime = ArenaClockUtils::MusicSwitchToNight.getPreciseTotalSeconds();
		const bool changeToDayMusic = (oldClockTime < dayMusicStartTime) && (newClockTime >= dayMusicStartTime);
		const bool changeToNightMusic = (oldClockTime < nightMusicStartTime) && (newClockTime >= nightMusicStartTime);
		
		const MusicDefinition *musicDef = nullptr;
		if (changeToDayMusic)
		{
			musicDef = musicLibrary.getRandomMusicDefinitionIf(MusicDefinition::Type::Weather, game.getRandom(),
				[this](const MusicDefinition &def)
			{
				DebugAssert(def.getType() == MusicDefinition::Type::Weather);
				const auto &weatherMusicDef = def.getWeatherMusicDefinition();
				return weatherMusicDef.weatherDef == this->weatherDef;
			});

			if (musicDef == nullptr)
			{
				DebugLogWarning("Missing weather music.");
			}
		}
		else if (changeToNightMusic)
		{
			musicDef = musicLibrary.getRandomMusicDefinition(MusicDefinition::Type::Night, game.getRandom());

			if (musicDef == nullptr)
			{
				DebugLogWarning("Missing night music.");
			}
		}

		if (musicDef != nullptr)
		{
			audioManager.setMusic(musicDef);
		}
	}
}

void GameState::tickChasmAnimation(double dt)
{
	this->chasmAnimSeconds += dt;
	if (this->chasmAnimSeconds >= ArenaVoxelUtils::CHASM_ANIM_SECONDS)
	{
		this->chasmAnimSeconds = std::fmod(this->chasmAnimSeconds, ArenaVoxelUtils::CHASM_ANIM_SECONDS);
	}
}

void GameState::tickSky(double dt, Game &game)
{
	SceneManager &sceneManager = game.getSceneManager();
	const LocationDefinition &locationDef = this->getLocationDefinition();

	SkyInstance &skyInst = sceneManager.skyInstance;
	skyInst.update(dt, locationDef.getLatitude(), this->getDaytimePercent(), this->weatherInst, game.getRandom());
}

void GameState::tickWeather(double dt, Game &game)
{
	const Renderer &renderer = game.getRenderer();
	const double windowAspect = renderer.getWindowAspect();
	this->weatherInst.update(dt, this->clock, windowAspect, game.getRandom(), game.getAudioManager());
}

void GameState::tickUiMessages(double dt)
{
	if (this->triggerTextIsVisible())
	{
		this->triggerTextRemainingSeconds -= dt;
	}

	if (this->actionTextIsVisible())
	{
		this->actionTextRemainingSeconds -= dt;
	}

	if (this->effectTextIsVisible())
	{
		this->effectTextRemainingSeconds -= dt;
	}
}

void GameState::tickPlayer(double dt, Game &game)
{
	auto &player = game.getPlayer();
	const CoordDouble3 oldPlayerCoord = player.getPosition();
	player.tick(game, dt);
	const CoordDouble3 newPlayerCoord = player.getPosition();

	// Handle input for the player's attack.
	const auto &inputManager = game.getInputManager();
	const Int2 mouseDelta = inputManager.getMouseDelta();
	PlayerLogicController::handlePlayerAttack(game, mouseDelta);

	// See if the player changed voxels in the XZ plane. If so, trigger text and sound events,
	// and handle any level transition.
	const double ceilingScale = this->getActiveCeilingScale();
	const CoordInt3 oldPlayerVoxelCoord(oldPlayerCoord.chunk, VoxelUtils::pointToVoxel(oldPlayerCoord.point, ceilingScale));
	const CoordInt3 newPlayerVoxelCoord(newPlayerCoord.chunk, VoxelUtils::pointToVoxel(newPlayerCoord.point, ceilingScale));
	if (newPlayerVoxelCoord != oldPlayerVoxelCoord)
	{
		TextBox *triggerTextBox = game.getTriggerTextBox();
		DebugAssert(triggerTextBox != nullptr);
		MapLogicController::handleTriggers(game, newPlayerVoxelCoord, *triggerTextBox);

		const MapType activeMapType = this->getActiveMapType();
		if (activeMapType == MapType::Interior)
		{
			MapLogicController::handleLevelTransition(game, oldPlayerVoxelCoord, newPlayerVoxelCoord);
		}
	}
}

void GameState::tickVoxels(double dt, Game &game)
{
	SceneManager &sceneManager = game.getSceneManager();
	const ChunkManager &chunkManager = sceneManager.chunkManager;

	const Player &player = game.getPlayer();

	const MapDefinition &mapDef = this->getActiveMapDef();
	const int levelIndex = this->getActiveLevelIndex();
	const BufferView<const LevelDefinition> levelDefs = mapDef.getLevels();
	const BufferView<const int> levelInfoDefIndices = mapDef.getLevelInfoIndices();
	const BufferView<const LevelInfoDefinition> levelInfoDefs = mapDef.getLevelInfos();
	const LevelDefinition &levelDef = levelDefs[levelIndex];
	const int levelInfoIndex = levelInfoDefIndices[levelIndex];
	const LevelInfoDefinition &levelInfoDef = levelInfoDefs[levelInfoIndex];
	const MapSubDefinition &mapSubDef = mapDef.getSubDefinition();

	VoxelChunkManager &voxelChunkManager = sceneManager.voxelChunkManager;
	voxelChunkManager.update(dt, chunkManager.getNewChunkPositions(), chunkManager.getFreedChunkPositions(),
		player.getPosition(), &levelDef, &levelInfoDef, mapSubDef, levelDefs, levelInfoDefIndices, levelInfoDefs,
		this->getActiveCeilingScale(), game.getAudioManager());
}

void GameState::tickEntities(double dt, Game &game)
{
	SceneManager &sceneManager = game.getSceneManager();
	const ChunkManager &chunkManager = sceneManager.chunkManager;
	const VoxelChunkManager &voxelChunkManager = sceneManager.voxelChunkManager;

	const Player &player = game.getPlayer();

	const MapDefinition &mapDef = this->getActiveMapDef();
	const MapType mapType = mapDef.getMapType();
	const int levelIndex = this->getActiveLevelIndex();
	const BufferView<const LevelDefinition> levelDefs = mapDef.getLevels();
	const BufferView<const int> levelInfoDefIndices = mapDef.getLevelInfoIndices();
	const BufferView<const LevelInfoDefinition> levelInfoDefs = mapDef.getLevelInfos();
	const LevelDefinition &levelDef = levelDefs[levelIndex];
	const int levelInfoIndex = levelInfoDefIndices[levelIndex];
	const LevelInfoDefinition &levelInfoDef = levelInfoDefs[levelInfoIndex];
	const MapSubDefinition &mapSubDef = mapDef.getSubDefinition();

	EntityGeneration::EntityGenInfo entityGenInfo;
	entityGenInfo.init(ArenaClockUtils::nightLightsAreActive(this->clock));

	const ProvinceDefinition &provinceDef = this->getProvinceDefinition();
	const LocationDefinition &locationDef = this->getLocationDefinition();
	const std::optional<CitizenUtils::CitizenGenInfo> citizenGenInfo = CitizenUtils::tryMakeCitizenGenInfo(
		mapType, provinceDef.getRaceID(), locationDef);

	const double ceilingScale = this->getActiveCeilingScale();

	EntityChunkManager &entityChunkManager = sceneManager.entityChunkManager;
	entityChunkManager.update(dt, chunkManager.getActiveChunkPositions(), chunkManager.getNewChunkPositions(),
		chunkManager.getFreedChunkPositions(), player, &levelDef, &levelInfoDef, mapSubDef, levelDefs, levelInfoDefIndices,
		levelInfoDefs, entityGenInfo, citizenGenInfo, ceilingScale, game.getRandom(), voxelChunkManager, game.getAudioManager(),
		game.getTextureManager(), game.getRenderer());
}

void GameState::tickCollision(double dt, Game &game)
{
	SceneManager &sceneManager = game.getSceneManager();
	const ChunkManager &chunkManager = sceneManager.chunkManager;
	const VoxelChunkManager &voxelChunkManager = sceneManager.voxelChunkManager;

	CollisionChunkManager &collisionChunkManager = sceneManager.collisionChunkManager;
	collisionChunkManager.update(dt, chunkManager.getActiveChunkPositions(), chunkManager.getNewChunkPositions(),
		chunkManager.getFreedChunkPositions(), voxelChunkManager);
}

void GameState::tickRendering(Game &game)
{
	SceneManager &sceneManager = game.getSceneManager();
	const ChunkManager &chunkManager = sceneManager.chunkManager;
	const VoxelChunkManager &voxelChunkManager = sceneManager.voxelChunkManager;
	const EntityChunkManager &entityChunkManager = sceneManager.entityChunkManager;
	const SkyInstance &skyInst = sceneManager.skyInstance;

	const double ceilingScale = this->getActiveCeilingScale();
	const double chasmAnimPercent = this->getChasmAnimPercent();

	const Player &player = game.getPlayer();
	const CoordDouble3 &playerCoord = player.getPosition();
	const CoordDouble2 playerCoordXZ(playerCoord.chunk, VoxelDouble2(playerCoord.point.x, playerCoord.point.z));
	const Double2 playerDirXZ = player.getGroundDirection();

	TextureManager &textureManager = game.getTextureManager();
	Renderer &renderer = game.getRenderer();

	RenderChunkManager &renderChunkManager = sceneManager.renderChunkManager;
	renderChunkManager.updateActiveChunks(chunkManager.getActiveChunkPositions(), chunkManager.getNewChunkPositions(),
		chunkManager.getFreedChunkPositions(), voxelChunkManager, renderer);
	renderChunkManager.updateVoxels(chunkManager.getActiveChunkPositions(), chunkManager.getNewChunkPositions(), ceilingScale, chasmAnimPercent,
		voxelChunkManager, textureManager, renderer);
	renderChunkManager.updateEntities(chunkManager.getActiveChunkPositions(), chunkManager.getNewChunkPositions(), playerCoordXZ, playerDirXZ, ceilingScale,
		voxelChunkManager, entityChunkManager, textureManager, renderer);
	renderChunkManager.updateLights(playerCoord, renderer);

	const double ambientPercent = ArenaRenderUtils::getAmbientPercent(this->clock, this->getActiveMapType());
	const double distantAmbientPercent = ArenaRenderUtils::getDistantAmbientPercent(ambientPercent);

	RenderSkyManager &renderSkyManager = sceneManager.renderSkyManager;
	renderSkyManager.update(skyInst, playerCoord, distantAmbientPercent, renderer);

	const WeatherInstance &weatherInst = game.getGameState().getWeatherInstance();
	const auto &options = game.getOptions();
	const RenderCamera renderCamera = RendererUtils::makeCamera(playerCoord.chunk, playerCoord.point, player.getDirection(),
		options.getGraphics_VerticalFOV(), renderer.getViewAspect(), options.getGraphics_TallPixelCorrection());

	RenderWeatherManager &renderWeatherManager = sceneManager.renderWeatherManager;
	renderWeatherManager.update(weatherInst, renderCamera);
}
