#ifndef WORLD_DATA_H
#define WORLD_DATA_H

#include <cstdint>
#include <string>
#include <vector>

#include "LevelData.h"
#include "../Entities/EntityManager.h"
#include "../Math/Vector2.h"

// This class stores data regarding elements in the game world. It should be constructible
// from a pair of .MIF and .INF files.

class ExeData;
class INFFile;
class MIFFile;
class MiscAssets;
class Renderer;
class TextureManager;

enum class ClimateType;
enum class LocationType;
enum class WeatherType;
enum class WorldType;

class WorldData
{	
private:
	std::vector<LevelData> levels;
	std::vector<Double2> startPoints;
	EntityManager entityManager;
	std::string mifName;
	WorldType worldType;
	int currentLevel;

	// Generates the .INF name for a city given a climate and current weather.
	static std::string generateCityInfName(ClimateType climateType, WeatherType weatherType);

	// Generates the .INF name for the wilderness given a climate and current weather.
	static std::string generateWildernessInfName(ClimateType climateType, WeatherType weatherType);
public:
	WorldData();
	WorldData(WorldData &&worldData) = default;

	WorldData &operator=(WorldData &&worldData) = default;

	// Loads all levels of an interior .MIF file.
	static WorldData loadInterior(const MIFFile &mif, const ExeData &exeData);

	// Loads a set of levels randomly selected from RANDOM1.MIF based on the given seed.
	static WorldData loadDungeon(uint32_t seed, int widthChunks, int depthChunks,
		bool isArtifactDungeon, const ExeData &exeData);

	// Loads a premade exterior city (only used by center province).
	static WorldData loadPremadeCity(const MIFFile &mif, ClimateType climateType,
		WeatherType weatherType, const ExeData &exeData);

	// Loads an exterior city skeleton and its random .MIF chunks.
	static WorldData loadCity(int localCityID, int provinceID, const MIFFile &mif, int cityDim,
		const std::vector<uint8_t> &reservedBlocks, const Int2 &startPosition,
		WeatherType weatherType, const MiscAssets &miscAssets);

	// Loads some wilderness blocks.
	static WorldData loadWilderness(int rmdTR, int rmdTL, int rmdBR, int rmdBL,
		ClimateType climateType, WeatherType weatherType, const ExeData &exeData);

	int getCurrentLevel() const;
	WorldType getWorldType() const;
	const std::string &getMifName() const;
	EntityManager &getEntityManager();
	const EntityManager &getEntityManager() const;
	const std::vector<Double2> &getStartPoints() const;
	std::vector<LevelData> &getLevels();
	const std::vector<LevelData> &getLevels() const;

	// Refreshes texture manager and renderer state using the selected level's data.
	void setLevelActive(int levelIndex, TextureManager &textureManager,
		Renderer &renderer);
};

#endif
