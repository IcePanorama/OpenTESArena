#include <numeric>
#include <vector>

#include "ArenaRenderUtils.h"
#include "Renderer.h"
#include "RenderSkyManager.h"
#include "../Assets/TextureManager.h"
#include "../Math/Constants.h"
#include "../Math/Vector3.h"
#include "../Sky/SkyDefinition.h"
#include "../Sky/SkyInfoDefinition.h"
#include "../Sky/SkyInstance.h"
#include "../World/MeshUtils.h"

void RenderSkyManager::LoadedGeneralSkyObjectTextureEntry::init(const TextureAsset &textureAsset,
	ScopedObjectTextureRef &&objectTextureRef)
{
	this->textureAsset = textureAsset;
	this->objectTextureRef = std::move(objectTextureRef);
}

void RenderSkyManager::LoadedSmallStarTextureEntry::init(uint8_t paletteIndex, ScopedObjectTextureRef &&objectTextureRef)
{
	this->paletteIndex = paletteIndex;
	this->objectTextureRef = std::move(objectTextureRef);
}

RenderSkyManager::RenderSkyManager()
{
	this->bgVertexBufferID = -1;
	this->bgNormalBufferID = -1;
	this->bgTexCoordBufferID = -1;
	this->bgIndexBufferID = -1;
	this->bgObjectTextureID = -1;

	this->objectVertexBufferID = -1;
	this->objectNormalBufferID = -1;
	this->objectTexCoordBufferID = -1;
	this->objectIndexBufferID = -1;
}

void RenderSkyManager::init(Renderer &renderer)
{
	std::vector<double> bgVertices;
	std::vector<double> bgNormals;
	std::vector<double> bgTexCoords;
	std::vector<int32_t> bgIndices;

	const double pointDistance = 1000.0; // @todo: this is a hack while the sky is using naive depth testing w/o any occlusion culling, etc.

	constexpr int zenithVertexIndex = 0;
	constexpr int nadirVertexIndex = 1;
	const Double3 zenithPoint(0.0, 1.0 * pointDistance, 0.0);
	const Double3 nadirPoint(0.0, -1.0 * pointDistance, 0.0);
	bgVertices.emplace_back(zenithPoint.x);
	bgVertices.emplace_back(zenithPoint.y);
	bgVertices.emplace_back(zenithPoint.z);
	bgVertices.emplace_back(nadirPoint.x);
	bgVertices.emplace_back(nadirPoint.y);
	bgVertices.emplace_back(nadirPoint.z);

	const Double3 zenithNormal = -zenithPoint.normalized();
	const Double3 nadirNormal = -nadirPoint.normalized();
	bgNormals.emplace_back(zenithNormal.x);
	bgNormals.emplace_back(zenithNormal.y);
	bgNormals.emplace_back(zenithNormal.z);
	bgNormals.emplace_back(nadirNormal.x);
	bgNormals.emplace_back(nadirNormal.y);
	bgNormals.emplace_back(nadirNormal.z);

	const Double2 zenithTexCoord(0.50, 0.0);
	const Double2 nadirTexCoord(0.50, 1.0);
	bgTexCoords.emplace_back(zenithTexCoord.x);
	bgTexCoords.emplace_back(zenithTexCoord.y);
	bgTexCoords.emplace_back(nadirTexCoord.x);
	bgTexCoords.emplace_back(nadirTexCoord.y);

	constexpr int bgAboveHorizonTriangleCount = 16; // Arbitrary number of triangles, increases smoothness of cone shape.
	for (int i = 0; i < bgAboveHorizonTriangleCount; i++)
	{
		// Generate two triangles: one above horizon, one below.
		const double percent = static_cast<double>(i) / static_cast<double>(bgAboveHorizonTriangleCount);
		const double nextPercent = static_cast<double>(i + 1) / static_cast<double>(bgAboveHorizonTriangleCount);
		const double period = percent * Constants::TwoPi;
		const double nextPeriod = nextPercent * Constants::TwoPi;

		const Double3 point(std::cos(period) * pointDistance, 0.0, std::sin(period) * pointDistance);
		const Double3 nextPoint(std::cos(nextPeriod) * pointDistance, 0.0, std::sin(nextPeriod) * pointDistance);

		bgVertices.emplace_back(point.x);
		bgVertices.emplace_back(point.y);
		bgVertices.emplace_back(point.z);
		bgVertices.emplace_back(nextPoint.x);
		bgVertices.emplace_back(nextPoint.y);
		bgVertices.emplace_back(nextPoint.z);

		// Normals point toward the player.
		const Double3 normal = -point.normalized();
		const Double3 nextNormal = -nextPoint.normalized();
		bgNormals.emplace_back(normal.x);
		bgNormals.emplace_back(normal.y);
		bgNormals.emplace_back(normal.z);
		bgNormals.emplace_back(nextNormal.x);
		bgNormals.emplace_back(nextNormal.y);
		bgNormals.emplace_back(nextNormal.z);

		const Double2 texCoord(1.0, 1.0);
		const Double2 nextTexCoord(0.0, 1.0);
		bgTexCoords.emplace_back(texCoord.x);
		bgTexCoords.emplace_back(texCoord.y);
		bgTexCoords.emplace_back(nextTexCoord.x);
		bgTexCoords.emplace_back(nextTexCoord.y);

		// Above-horizon winding: next -> cur -> zenith
		const int32_t vertexIndex = static_cast<int32_t>((bgVertices.size() / 3) - 2);
		const int32_t nextVertexIndex = static_cast<int32_t>((bgVertices.size() / 3) - 1);
		bgIndices.emplace_back(nextVertexIndex);
		bgIndices.emplace_back(vertexIndex);
		bgIndices.emplace_back(zenithVertexIndex);

		// Below-horizon winding: cur -> next -> nadir
		bgIndices.emplace_back(vertexIndex);
		bgIndices.emplace_back(nextVertexIndex);
		bgIndices.emplace_back(nadirVertexIndex);
	}

	constexpr int positionComponentsPerVertex = MeshUtils::POSITION_COMPONENTS_PER_VERTEX;
	constexpr int normalComponentsPerVertex = MeshUtils::NORMAL_COMPONENTS_PER_VERTEX;
	constexpr int texCoordComponentsPerVertex = MeshUtils::TEX_COORDS_PER_VERTEX;

	const int bgVertexCount = static_cast<int>(bgVertices.size()) / 3;
	if (!renderer.tryCreateVertexBuffer(bgVertexCount, positionComponentsPerVertex, &this->bgVertexBufferID))
	{
		DebugLogError("Couldn't create vertex buffer for sky background mesh ID.");
		return;
	}

	if (!renderer.tryCreateAttributeBuffer(bgVertexCount, normalComponentsPerVertex, &this->bgNormalBufferID))
	{
		DebugLogError("Couldn't create normal attribute buffer for sky background mesh ID.");
		this->freeBgBuffers(renderer);
		return;
	}

	if (!renderer.tryCreateAttributeBuffer(bgVertexCount, texCoordComponentsPerVertex, &this->bgTexCoordBufferID))
	{
		DebugLogError("Couldn't create tex coord attribute buffer for sky background mesh ID.");
		this->freeBgBuffers(renderer);
		return;
	}

	if (!renderer.tryCreateIndexBuffer(static_cast<int>(bgIndices.size()), &this->bgIndexBufferID))
	{
		DebugLogError("Couldn't create index buffer for sky background mesh ID.");
		this->freeBgBuffers(renderer);
		return;
	}

	renderer.populateVertexBuffer(this->bgVertexBufferID, bgVertices);
	renderer.populateAttributeBuffer(this->bgNormalBufferID, bgNormals);
	renderer.populateAttributeBuffer(this->bgTexCoordBufferID, bgTexCoords);
	renderer.populateIndexBuffer(this->bgIndexBufferID, bgIndices);

	BufferView<const uint8_t> bgPaletteIndices = ArenaRenderUtils::PALETTE_INDICES_SKY_COLOR_MORNING;
	constexpr int bgTextureWidth = 1;
	const int bgTextureHeight = bgPaletteIndices.getCount(); // @todo: figure out sky background texture coloring; probably lock+update the main world palette in an update() with DAYTIME.COL indices as times goes on?
	constexpr int bgBytesPerTexel = 1;
	if (!renderer.tryCreateObjectTexture(bgTextureWidth, bgTextureHeight, bgBytesPerTexel, &this->bgObjectTextureID))
	{
		DebugLogError("Couldn't create object texture for sky background texture ID.");
		this->freeBgBuffers(renderer);
		return;
	}

	LockedTexture bgLockedTexture = renderer.lockObjectTexture(this->bgObjectTextureID);
	uint8_t *bgTexels = static_cast<uint8_t*>(bgLockedTexture.texels);
	std::copy(bgPaletteIndices.begin(), bgPaletteIndices.end(), bgTexels);
	renderer.unlockObjectTexture(this->bgObjectTextureID);

	this->bgDrawCall.position = Double3::Zero;
	this->bgDrawCall.preScaleTranslation = Double3::Zero;
	this->bgDrawCall.rotation = Matrix4d::identity();
	this->bgDrawCall.scale = Matrix4d::identity();
	this->bgDrawCall.vertexBufferID = this->bgVertexBufferID;
	this->bgDrawCall.normalBufferID = this->bgNormalBufferID;
	this->bgDrawCall.texCoordBufferID = this->bgTexCoordBufferID;
	this->bgDrawCall.indexBufferID = this->bgIndexBufferID;
	this->bgDrawCall.textureIDs[0] = this->bgObjectTextureID;
	this->bgDrawCall.textureIDs[1] = std::nullopt;
	this->bgDrawCall.textureSamplingType0 = TextureSamplingType::Default;
	this->bgDrawCall.textureSamplingType1 = TextureSamplingType::Default;
	this->bgDrawCall.vertexShaderType = VertexShaderType::Voxel; // @todo: SkyBackground?
	this->bgDrawCall.pixelShaderType = PixelShaderType::Opaque; // @todo?
	this->bgDrawCall.pixelShaderParam0 = 0.0;

	// Initialize sky object mesh buffers shared with all sky objects.
	// @todo: to be more accurate, land/air vertices could rest on the horizon, while star/planet/sun vertices would sit halfway under the horizon, etc., and these would be separate buffers for the draw calls to pick from.
	constexpr int objectMeshVertexCount = 4;
	constexpr int objectMeshIndexCount = 6;
	if (!renderer.tryCreateVertexBuffer(objectMeshVertexCount, positionComponentsPerVertex, &this->objectVertexBufferID))
	{
		DebugLogError("Couldn't create vertex buffer for sky object mesh ID.");
		return;
	}

	if (!renderer.tryCreateAttributeBuffer(objectMeshVertexCount, normalComponentsPerVertex, &this->objectNormalBufferID))
	{
		DebugLogError("Couldn't create normal attribute buffer for sky object mesh def.");
		this->freeObjectBuffers(renderer);
		return;
	}

	if (!renderer.tryCreateAttributeBuffer(objectMeshVertexCount, texCoordComponentsPerVertex, &this->objectTexCoordBufferID))
	{
		DebugLogError("Couldn't create tex coord attribute buffer for sky object mesh def.");
		this->freeObjectBuffers(renderer);
		return;
	}

	if (!renderer.tryCreateIndexBuffer(objectMeshIndexCount, &this->objectIndexBufferID))
	{
		DebugLogError("Couldn't create index buffer for sky object mesh def.");
		this->freeObjectBuffers(renderer);
		return;
	}

	constexpr std::array<double, objectMeshVertexCount * positionComponentsPerVertex> objectVertices =
	{
		0.0, 1.0, -0.50,
		0.0, 0.0, -0.50,
		0.0, 0.0, 0.50,
		0.0, 1.0, 0.50
	};

	constexpr std::array<double, objectMeshVertexCount * normalComponentsPerVertex> objectNormals =
	{
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0
	};

	constexpr std::array<double, objectMeshVertexCount * texCoordComponentsPerVertex> objectTexCoords =
	{
		0.0, 0.0,
		0.0, 1.0,
		1.0, 1.0,
		1.0, 0.0
	};

	constexpr std::array<int32_t, objectMeshIndexCount> objectIndices =
	{
		0, 1, 2,
		2, 3, 0
	};

	renderer.populateVertexBuffer(this->objectVertexBufferID, objectVertices);
	renderer.populateAttributeBuffer(this->objectNormalBufferID, objectNormals);
	renderer.populateAttributeBuffer(this->objectTexCoordBufferID, objectTexCoords);
	renderer.populateIndexBuffer(this->objectIndexBufferID, objectIndices);
}

void RenderSkyManager::shutdown(Renderer &renderer)
{
	this->freeBgBuffers(renderer);
	this->bgDrawCall.clear();

	this->freeObjectBuffers(renderer);
	this->objectDrawCalls.clear();
}

ObjectTextureID RenderSkyManager::getGeneralSkyObjectTextureID(const TextureAsset &textureAsset) const
{
	const auto iter = std::find_if(this->generalSkyObjectTextures.begin(), this->generalSkyObjectTextures.end(),
		[&textureAsset](const LoadedGeneralSkyObjectTextureEntry &loadedTexture)
	{
		return loadedTexture.textureAsset == textureAsset;
	});

	if (iter == this->generalSkyObjectTextures.end())
	{
		DebugLogError("Couldn't find loaded sky object texture for \"" + textureAsset.filename + "\".");
		return -1;
	}

	return iter->objectTextureRef.get();
}

ObjectTextureID RenderSkyManager::getSmallStarTextureID(uint8_t paletteIndex) const
{
	const auto iter = std::find_if(this->smallStarTextures.begin(), this->smallStarTextures.end(),
		[paletteIndex](const LoadedSmallStarTextureEntry &loadedTexture)
	{
		return loadedTexture.paletteIndex == paletteIndex;
	});

	if (iter == this->smallStarTextures.end())
	{
		DebugLogError("Couldn't find loaded small star texture with palette index \"" + std::to_string(paletteIndex) + "\".");
		return -1;
	}

	return iter->objectTextureRef.get();
}

void RenderSkyManager::freeBgBuffers(Renderer &renderer)
{
	if (this->bgVertexBufferID >= 0)
	{
		renderer.freeVertexBuffer(this->bgVertexBufferID);
		this->bgVertexBufferID = -1;
	}

	if (this->bgNormalBufferID >= 0)
	{
		renderer.freeAttributeBuffer(this->bgNormalBufferID);
		this->bgNormalBufferID = -1;
	}

	if (this->bgTexCoordBufferID >= 0)
	{
		renderer.freeAttributeBuffer(this->bgTexCoordBufferID);
		this->bgTexCoordBufferID = -1;
	}

	if (this->bgIndexBufferID >= 0)
	{
		renderer.freeIndexBuffer(this->bgIndexBufferID);
		this->bgIndexBufferID = -1;
	}

	if (this->bgObjectTextureID >= 0)
	{
		renderer.freeObjectTexture(this->bgObjectTextureID);
		this->bgObjectTextureID = -1;
	}
}

void RenderSkyManager::freeObjectBuffers(Renderer &renderer)
{
	if (this->objectVertexBufferID >= 0)
	{
		renderer.freeVertexBuffer(this->objectVertexBufferID);
		this->objectVertexBufferID = -1;
	}

	if (this->objectNormalBufferID >= 0)
	{
		renderer.freeAttributeBuffer(this->objectNormalBufferID);
		this->objectNormalBufferID = -1;
	}

	if (this->objectTexCoordBufferID >= 0)
	{
		renderer.freeAttributeBuffer(this->objectTexCoordBufferID);
		this->objectTexCoordBufferID = -1;
	}

	if (this->objectIndexBufferID >= 0)
	{
		renderer.freeIndexBuffer(this->objectIndexBufferID);
		this->objectIndexBufferID = -1;
	}

	this->generalSkyObjectTextures.clear();
	this->smallStarTextures.clear();
}

RenderDrawCall RenderSkyManager::getBgDrawCall() const
{
	return this->bgDrawCall;
}

BufferView<const RenderDrawCall> RenderSkyManager::getObjectDrawCalls() const
{
	return this->objectDrawCalls;
}

void RenderSkyManager::loadScene(const SkyInfoDefinition &skyInfoDef, TextureManager &textureManager, Renderer &renderer)
{
	auto tryLoadTextureAsset = [this, &textureManager, &renderer](const TextureAsset &textureAsset)
	{
		const auto iter = std::find_if(this->generalSkyObjectTextures.begin(), this->generalSkyObjectTextures.end(),
			[&textureAsset](const LoadedGeneralSkyObjectTextureEntry &loadedTexture)
		{
			return loadedTexture.textureAsset == textureAsset;
		});

		if (iter == this->generalSkyObjectTextures.end())
		{
			const std::optional<TextureBuilderID> textureBuilderID = textureManager.tryGetTextureBuilderID(textureAsset);
			if (!textureBuilderID.has_value())
			{
				DebugLogError("Couldn't get texture builder ID for sky object texture \"" + textureAsset.filename + "\".");
				return;
			}

			const TextureBuilder &textureBuilder = textureManager.getTextureBuilderHandle(*textureBuilderID);
			ObjectTextureID textureID;
			if (!renderer.tryCreateObjectTexture(textureBuilder, &textureID))
			{
				DebugLogError("Couldn't create object texture for sky object texture \"" + textureAsset.filename + "\".");
				return;
			}

			LoadedGeneralSkyObjectTextureEntry loadedEntry;
			loadedEntry.init(textureAsset, ScopedObjectTextureRef(textureID, renderer));
			this->generalSkyObjectTextures.emplace_back(std::move(loadedEntry));
		}
	};

	auto tryLoadPaletteColor = [this, &renderer](uint8_t paletteIndex)
	{
		const auto iter = std::find_if(this->smallStarTextures.begin(), this->smallStarTextures.end(),
			[paletteIndex](const LoadedSmallStarTextureEntry &loadedTexture)
		{
			return loadedTexture.paletteIndex == paletteIndex;
		});

		if (iter == this->smallStarTextures.end())
		{
			constexpr int textureWidth = 1;
			constexpr int textureHeight = textureWidth;
			constexpr int bytesPerTexel = 1;
			ObjectTextureID textureID;
			if (!renderer.tryCreateObjectTexture(textureWidth, textureHeight, bytesPerTexel, &textureID))
			{
				DebugLogError("Couldn't create object texture for sky object texture palette index \"" + std::to_string(paletteIndex) + "\".");
				return;
			}

			LockedTexture lockedTexture = renderer.lockObjectTexture(textureID);
			if (!lockedTexture.isValid())
			{
				DebugLogError("Couldn't lock sky object texture for writing palette index \"" + std::to_string(paletteIndex) + "\".");
				return;
			}

			DebugAssert(lockedTexture.bytesPerTexel == 1);
			uint8_t *dstTexels = static_cast<uint8_t*>(lockedTexture.texels);
			*dstTexels = paletteIndex;
			renderer.unlockObjectTexture(textureID);

			LoadedSmallStarTextureEntry loadedEntry;
			loadedEntry.init(paletteIndex, ScopedObjectTextureRef(textureID, renderer));
			this->smallStarTextures.emplace_back(std::move(loadedEntry));
		}		
	};

	for (int i = 0; i < skyInfoDef.getLandCount(); i++)
	{
		const SkyLandDefinition &landDef = skyInfoDef.getLand(i);
		for (const TextureAsset &textureAsset : landDef.textureAssets)
		{
			tryLoadTextureAsset(textureAsset);
		}
	}

	for (int i = 0; i < skyInfoDef.getAirCount(); i++)
	{
		const SkyAirDefinition &airDef = skyInfoDef.getAir(i);
		tryLoadTextureAsset(airDef.textureAsset);
	}

	for (int i = 0; i < skyInfoDef.getStarCount(); i++)
	{
		const SkyStarDefinition &starDef = skyInfoDef.getStar(i);
		switch (starDef.type)
		{
		case SkyStarType::Small:
		{
			const SkySmallStarDefinition &smallStarDef = starDef.smallStar;
			tryLoadPaletteColor(smallStarDef.paletteIndex);
			break;
		}
		case SkyStarType::Large:
		{
			const SkyLargeStarDefinition &largeStarDef = starDef.largeStar;
			tryLoadTextureAsset(largeStarDef.textureAsset);
			break;
		}
		default:
			DebugNotImplementedMsg(std::to_string(static_cast<int>(starDef.type)));
			break;
		}
	}

	for (int i = 0; i < skyInfoDef.getSunCount(); i++)
	{
		const SkySunDefinition &sunDef = skyInfoDef.getSun(i);
		tryLoadTextureAsset(sunDef.textureAsset);
	}

	for (int i = 0; i < skyInfoDef.getMoonCount(); i++)
	{
		const SkyMoonDefinition &moonDef = skyInfoDef.getMoon(i);
		for (const TextureAsset &textureAsset : moonDef.textureAssets)
		{
			tryLoadTextureAsset(textureAsset);
		}
	}

	// @todo: load draw calls for all the sky objects (ideally here, but can be in update() for now if convenient)
}

void RenderSkyManager::update(const SkyInstance &skyInst, const CoordDouble3 &cameraCoord, const Renderer &renderer)
{
	const WorldDouble3 cameraPos = VoxelUtils::coordToWorldPoint(cameraCoord);

	// Keep the sky centered on the player.
	this->bgDrawCall.position = cameraPos;

	// @temp fix for Z ordering. Later I think we should just not do depth testing in the sky?
	constexpr double landDistance = 250.0;
	constexpr double airDistance = landDistance + 20.0;
	constexpr double moonDistance = airDistance + 20.0;
	constexpr double sunDistance = moonDistance + 20.0;
	constexpr double starDistance = sunDistance + 20.0;

	auto addDrawCall = [this, &renderer, &cameraPos](const Double3 &direction, double width, double height, ObjectTextureID textureID,
		double arbitraryDistance, PixelShaderType pixelShaderType)
	{
		RenderDrawCall drawCall;
		drawCall.position = cameraPos + (direction * arbitraryDistance);
		drawCall.preScaleTranslation = Double3::Zero;
		
		const Radians xzRotationRadians = MathUtils::fullAtan2(Double2(direction.z, direction.x).normalized()) + Constants::Pi;
		drawCall.rotation = Matrix4d::yRotation(xzRotationRadians);
		// @todo: need to combine with a rotation that turns it towards the player from above and below

		const double scaledWidth = width * arbitraryDistance;
		const double scaledHeight = height * arbitraryDistance;
		drawCall.scale = Matrix4d::scale(1.0, scaledHeight, scaledWidth);

		drawCall.vertexBufferID = this->objectVertexBufferID;
		drawCall.normalBufferID = this->objectNormalBufferID;
		drawCall.texCoordBufferID = this->objectTexCoordBufferID;
		drawCall.indexBufferID = this->objectIndexBufferID;
		drawCall.textureIDs[0] = textureID;
		drawCall.textureIDs[1] = std::nullopt;
		drawCall.textureSamplingType0 = TextureSamplingType::Default;
		drawCall.textureSamplingType1 = TextureSamplingType::Default;
		drawCall.vertexShaderType = VertexShaderType::SlidingDoor; // @todo: make a sky object vertex shader
		drawCall.pixelShaderType = pixelShaderType;
		drawCall.pixelShaderParam0 = 0.0; // @todo: maybe use for full-bright distant objects like volcanoes?
		this->objectDrawCalls.emplace_back(std::move(drawCall));
	};

	// @todo: create draw calls in loadScene() as an optimization
	// @todo: update sky object draw call transforms if they are affected by planet rotation

	this->objectDrawCalls.clear(); // @todo: don't clear every frame, just change their transforms/animation texture ID

	for (int i = skyInst.landStart; i < skyInst.landEnd; i++)
	{
		const SkyObjectInstance &skyObjectInst = skyInst.getSkyObjectInst(i);
		const SkyObjectTextureType textureType = skyObjectInst.textureType;
		DebugAssertMsg(textureType == SkyObjectTextureType::TextureAsset, "Expected all sky land objects to use TextureAsset texture type.");

		const SkyObjectTextureAssetEntry &textureAssetEntry = skyInst.getTextureAssetEntry(skyObjectInst.textureAssetEntryID);
		const BufferView<const TextureAsset> textureAssets = textureAssetEntry.textureAssets;
		const int textureCount = textureAssets.getCount();

		int textureAssetIndex = 0;
		const int animIndex = skyObjectInst.animIndex;
		if (animIndex >= 0)
		{
			const SkyObjectAnimationInstance &animInst = skyInst.getAnimInst(animIndex);
			const double animPercent = animInst.percentDone;
			textureAssetIndex = std::clamp(static_cast<int>(static_cast<double>(textureCount) * animPercent), 0, textureCount - 1);
		}

		const TextureAsset &textureAsset = textureAssets.get(textureAssetIndex);
		const ObjectTextureID textureID = this->getGeneralSkyObjectTextureID(textureAsset);

		addDrawCall(skyObjectInst.transformedDirection, skyObjectInst.width, skyObjectInst.height, textureID, landDistance, PixelShaderType::AlphaTested);
	}

	for (int i = skyInst.airStart; i < skyInst.airEnd; i++)
	{
		const SkyObjectInstance &skyObjectInst = skyInst.getSkyObjectInst(i);
		const SkyObjectTextureType textureType = skyObjectInst.textureType;
		DebugAssertMsg(textureType == SkyObjectTextureType::TextureAsset, "Expected all sky air objects to use TextureAsset texture type.");

		const SkyObjectTextureAssetEntry &textureAssetEntry = skyInst.getTextureAssetEntry(skyObjectInst.textureAssetEntryID);
		const TextureAsset &textureAsset = textureAssetEntry.textureAssets.get(0);
		const ObjectTextureID textureID = this->getGeneralSkyObjectTextureID(textureAsset);

		addDrawCall(skyObjectInst.transformedDirection, skyObjectInst.width, skyObjectInst.height, textureID, airDistance, PixelShaderType::AlphaTestedWithLightLevelTransparency);
	}

	for (int i = skyInst.moonStart; i < skyInst.moonEnd; i++)
	{
		const SkyObjectInstance &skyObjectInst = skyInst.getSkyObjectInst(i);
		const SkyObjectTextureType textureType = skyObjectInst.textureType;
		DebugAssertMsg(textureType == SkyObjectTextureType::TextureAsset, "Expected all sky moon objects to use TextureAsset texture type.");

		const SkyObjectTextureAssetEntry &textureAssetEntry = skyInst.getTextureAssetEntry(skyObjectInst.textureAssetEntryID);
		const TextureAsset &textureAsset = textureAssetEntry.textureAssets.get(0);
		const ObjectTextureID textureID = this->getGeneralSkyObjectTextureID(textureAsset);

		addDrawCall(skyObjectInst.transformedDirection, skyObjectInst.width, skyObjectInst.height, textureID, moonDistance, PixelShaderType::AlphaTestedWithLightLevelTransparency);
	}

	for (int i = skyInst.sunStart; i < skyInst.sunEnd; i++)
	{
		const SkyObjectInstance &skyObjectInst = skyInst.getSkyObjectInst(i);
		const SkyObjectTextureType textureType = skyObjectInst.textureType;
		DebugAssertMsg(textureType == SkyObjectTextureType::TextureAsset, "Expected all sky sun objects to use TextureAsset texture type.");

		const SkyObjectTextureAssetEntry &textureAssetEntry = skyInst.getTextureAssetEntry(skyObjectInst.textureAssetEntryID);
		const TextureAsset &textureAsset = textureAssetEntry.textureAssets.get(0);
		const ObjectTextureID textureID = this->getGeneralSkyObjectTextureID(textureAsset);

		addDrawCall(skyObjectInst.transformedDirection, skyObjectInst.width, skyObjectInst.height, textureID, sunDistance, PixelShaderType::AlphaTested);
	}

	for (int i = skyInst.starStart; i < skyInst.starEnd; i++)
	{
		const SkyObjectInstance &skyObjectInst = skyInst.getSkyObjectInst(i);
		const SkyObjectTextureType textureType = skyObjectInst.textureType;

		ObjectTextureID textureID = -1;
		if (textureType == SkyObjectTextureType::TextureAsset)
		{
			const SkyObjectTextureAssetEntry &textureAssetEntry = skyInst.getTextureAssetEntry(skyObjectInst.textureAssetEntryID);
			const TextureAsset &textureAsset = textureAssetEntry.textureAssets.get(0);
			textureID = this->getGeneralSkyObjectTextureID(textureAsset);
		}
		else if (textureType == SkyObjectTextureType::PaletteIndex)
		{
			const SkyObjectPaletteIndexEntry &paletteIndexEntry = skyInst.getPaletteIndexEntry(skyObjectInst.paletteIndexEntryID);
			const uint8_t paletteIndex = paletteIndexEntry.paletteIndex;
			textureID = this->getSmallStarTextureID(paletteIndex);
		}
		else
		{
			DebugNotImplementedMsg(std::to_string(static_cast<int>(textureType)));
		}

		addDrawCall(skyObjectInst.transformedDirection, skyObjectInst.width, skyObjectInst.height, textureID, starDistance, PixelShaderType::AlphaTested);
	}
}

void RenderSkyManager::unloadScene(Renderer &renderer)
{
	this->generalSkyObjectTextures.clear();
	this->smallStarTextures.clear();
	this->objectDrawCalls.clear();
}
