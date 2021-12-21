#include <algorithm>
#include <cmath>
#include <limits>

#include "ArenaRenderUtils.h"
#include "LegacyRendererUtils.h"
#include "RenderCamera.h"
#include "RendererUtils.h"
#include "RenderFrameSettings.h"
#include "RenderInitSettings.h"
#include "SoftwareRenderer.h"
#include "../Math/Constants.h"
#include "../Math/MathUtils.h"
#include "../Math/Random.h"
#include "../Media/Color.h"
#include "../Media/Palette.h"
#include "../Media/TextureBuilder.h"
#include "../World/ChunkUtils.h"

#include "components/debug/Debug.h"

namespace swConstants
{
	constexpr double NEAR_PLANE = 0.1;
	constexpr double FAR_PLANE = 100.0;
}

namespace swCamera
{
	Double3 GetCameraEye(const RenderCamera &camera)
	{
		// @todo: eventually I think the chunk should be zeroed out and everything should always treat
		// the player's chunk as the origin chunk.
		return VoxelUtils::chunkPointToNewPoint(camera.chunk, camera.point);
	}
}

// Internal geometry types/functions.
namespace swGeometry
{
	struct TriangleClipResult
	{
		int triangleCount = 0;
		RenderTriangle triangles[2];

		static TriangleClipResult zero()
		{
			TriangleClipResult result;
			result.triangleCount = 0;
			return result;
		}

		static TriangleClipResult one(const RenderTriangle &triangle)
		{
			TriangleClipResult result;
			result.triangleCount = 1;
			result.triangles[0] = triangle;
			return result;
		}

		static TriangleClipResult two(const RenderTriangle &triangleA, const RenderTriangle &triangleB)
		{
			TriangleClipResult result;
			result.triangleCount = 2;
			result.triangles[0] = triangleA;
			result.triangles[1] = triangleB;
			return result;
		}
	};

	TriangleClipResult ClipTriangle(const RenderTriangle &triangle, const Double3 &planePoint, const Double3 &planeNormal)
	{
		std::array<const Double3*, 3> insidePoints, outsidePoints;
		std::array<const Double2*, 3> insideUVs, outsideUVs;
		int insidePointCount = 0;
		int outsidePointCount = 0;

		const std::array<const Double3*, 3> vertexPtrs = { &triangle.v0, &triangle.v1, &triangle.v2 };
		const std::array<const Double2*, 3> uvPtrs = { &triangle.uv0, &triangle.uv1, &triangle.uv2 };

		// Determine which vertices are in the positive half-space of the clipping plane.
		for (int i = 0; i < static_cast<int>(vertexPtrs.size()); i++)
		{
			const Double3 *vertexPtr = vertexPtrs[i];
			const double dist = MathUtils::distanceToPlane(*vertexPtr, planePoint, planeNormal);
			if (dist >= 0.0)
			{
				insidePoints[insidePointCount] = vertexPtr;
				insideUVs[insidePointCount] = uvPtrs[i];
				insidePointCount++;
			}
			else
			{
				outsidePoints[outsidePointCount] = vertexPtr;
				outsideUVs[outsidePointCount] = uvPtrs[i];
				outsidePointCount++;
			}
		}

		// Clip triangle depending on the inside/outside vertex case.
		const bool isCompletelyOutside = insidePointCount == 0;
		const bool isCompletelyInside = insidePointCount == 3;
		const bool becomesSmallerTriangle = insidePointCount == 1;
		const bool becomesQuad = insidePointCount == 2;
		if (isCompletelyOutside)
		{
			return TriangleClipResult::zero();
		}
		else if (isCompletelyInside)
		{
			return TriangleClipResult::one(triangle);
		}
		else if (becomesSmallerTriangle)
		{
			const Double3 &insidePoint = *insidePoints[0];
			const Double2 &insideUV = *insideUVs[0];
			const Double3 &outsidePoint0 = *outsidePoints[0];
			const Double3 &outsidePoint1 = *outsidePoints[1];

			// @todo: replace ray-plane intersection with one that get T value internally
			Double3 newInsidePoint1, newInsidePoint2;
			MathUtils::rayPlaneIntersection(insidePoint, (outsidePoint0 - insidePoint).normalized(),
				planePoint, planeNormal, &newInsidePoint1);
			MathUtils::rayPlaneIntersection(insidePoint, (outsidePoint1 - insidePoint).normalized(),
				planePoint, planeNormal, &newInsidePoint2);

			const double t0 = (newInsidePoint1 - insidePoint).length();
			const double t1 = (newInsidePoint2 - insidePoint).length();

			const Double2 outsideUV0 = *outsideUVs[0];
			const Double2 outsideUV1 = *outsideUVs[1];
			const Double2 newInsideUV0 = insideUV.lerp(outsideUV0, t0);
			const Double2 newInsideUV1 = insideUV.lerp(outsideUV1, t1);

			return TriangleClipResult::one(RenderTriangle(insidePoint, newInsidePoint1, newInsidePoint2,
				insideUV, newInsideUV0, newInsideUV1, Color::Blue.toARGB()));
		}
		else if (becomesQuad)
		{
			const Double3 &insidePoint0 = *insidePoints[0];
			const Double3 &insidePoint1 = *insidePoints[1];
			const Double3 &outsidePoint0 = *outsidePoints[0];
			const Double2 &insideUV0 = *insideUVs[0];
			const Double2 &insideUV1 = *insideUVs[1];
			const Double2 &outsideUV0 = *outsideUVs[0];

			const Double3 &newTriangle0V0 = insidePoint0;
			const Double3 &newTriangle0V1 = insidePoint1;
			const Double2 &newTriangle0UV0 = insideUV0;
			const Double2 &newTriangle0UV1 = insideUV1;

			// @todo: replace ray-plane intersection with one that get T value internally
			Double3 newTriangle0V2;
			MathUtils::rayPlaneIntersection(newTriangle0V0, (outsidePoint0 - newTriangle0V0).normalized(),
				planePoint, planeNormal, &newTriangle0V2);
			const double newTriangle0T = (newTriangle0V2 - newTriangle0V0).length();
			const Double2 newTriangle0UV2 = newTriangle0UV0.lerp(outsideUV0, newTriangle0T);

			const Double3 &newTriangle1V0 = insidePoint1;
			const Double3 &newTriangle1V1 = newTriangle0V2;
			const Double2 &newTriangle1UV0 = insideUV1;
			const Double2 &newTriangle1UV1 = newTriangle0UV2;

			// @todo: replace ray-plane intersection with one that get T value internally
			Double3 newTriangle1V2;
			MathUtils::rayPlaneIntersection(newTriangle1V0, (outsidePoint0 - newTriangle1V0).normalized(),
				planePoint, planeNormal, &newTriangle1V2);
			const double newTriangle1T = (newTriangle1V2 - newTriangle1V0).length();
			const Double2 newTriangle1UV2 = newTriangle1UV0.lerp(outsideUV0, newTriangle1T);

			const RenderTriangle newTriangle0(
				newTriangle0V0, newTriangle0V1, newTriangle0V2,
				newTriangle0UV0, newTriangle0UV1, newTriangle0UV2,
				Color::Red.toARGB());
			const RenderTriangle newTriangle1(
				newTriangle1V0, newTriangle1V1, newTriangle1V2,
				newTriangle1UV0, newTriangle1UV1, newTriangle1UV2,
				Color::Green.toARGB());

			return TriangleClipResult::two(newTriangle0, newTriangle1);
		}
		else
		{
			DebugUnhandledReturnMsg(TriangleClipResult, "Unhandled triangle clip case (inside: " +
				std::to_string(insidePointCount) + ", outside: " + std::to_string(outsidePointCount) + ").");
		}
	}

	// 1) Back-face culling
	// 2) Frustum culling
	// 3) Clipping
	std::vector<RenderTriangle> ProcessTrianglesForRasterization(const BufferView<const RenderTriangle> &triangles, const RenderCamera &camera)
	{
		// @todo: camera frustum planes

		const Double3 eye = swCamera::GetCameraEye(camera);

		// Plane point and normal pairs in camera space.
		// @todo: if clipping at THIS stage in the pipeline, these should be world space, not camera space.
		// - probably need a camera -> world space transform.
		const std::array<std::pair<Double3, Double3>, 1> clipPlanes =
		{
			{
				// Near plane
				std::make_pair(eye + (camera.forward * swConstants::NEAR_PLANE), camera.forward),

				// Far plane
				//std::make_pair(Double3(0.0, 0.0, swConstants::FAR_PLANE), -Double3::UnitZ),

				// Left
				//std::make_pair(Double3::Zero,) // @todo: probably combination of camera fwd/right/up normalized, accounting for fov and aspect

				// Right

				// Bottom

				// Top

			}
		};

		std::vector<RenderTriangle> clippedTriangles;

		for (int i = 0; i < triangles.getCount(); i++)
		{
			const RenderTriangle &triangle = triangles.get(i);
			const Double3 &v0 = triangle.v0;
			const Double3 &v1 = triangle.v1;
			const Double3 &v2 = triangle.v2;

			// Discard back-facing and almost-back-facing.
			const Double3 v0ToEye = eye - v0;
			if (v0ToEye.dot(triangle.normal) < Constants::Epsilon)
			{
				continue;
			}

			// @todo: when adding more clipping planes, probably need some kind of queue here, and then
			// the clippedTriangles.emplace_back() will be in a loop for all the final-clipped triangles
			// from the queue (since a triangle might be clipped multiple times).
			const auto &planePair = clipPlanes[0];
			const Double3 &planePoint = planePair.first;
			const Double3 &planeNormal = planePair.second;
			const TriangleClipResult clipResult = ClipTriangle(triangle, planePoint, planeNormal);
			for (int k = 0; k < clipResult.triangleCount; k++)
			{
				clippedTriangles.emplace_back(clipResult.triangles[k]);
			}
		}

		return clippedTriangles;
	}

	std::vector<RenderTriangle> MakeDebugCube(const Double3 &point)
	{
		std::vector<RenderTriangle> triangles;

		auto p = [&point](double x, double y, double z)
		{
			return point + Double3(x, y, z);
		};

		const Double2 uvTL(0.0, 0.0);
		const Double2 uvTR(1.0, 0.0);
		const Double2 uvBL(0.0, 1.0);
		const Double2 uvBR(1.0, 1.0);

		const uint32_t color = Color::White.toARGB();

		// Cube
		// X=0
		triangles.emplace_back(RenderTriangle(p(0.0, 1.0, 0.0), p(0.0, 0.0, 0.0), p(0.0, 0.0, 1.0), uvTL, uvBL, uvBR, color));
		triangles.emplace_back(RenderTriangle(p(0.0, 0.0, 1.0), p(0.0, 1.0, 1.0), p(0.0, 1.0, 0.0), uvBR, uvTR, uvTL, color));
		// X=1
		triangles.emplace_back(RenderTriangle(p(1.0, 1.0, 1.0), p(1.0, 0.0, 1.0), p(1.0, 0.0, 0.0), uvTL, uvBL, uvBR, color));
		triangles.emplace_back(RenderTriangle(p(1.0, 0.0, 0.0), p(1.0, 1.0, 0.0), p(1.0, 1.0, 1.0), uvBR, uvTR, uvTL, color));
		// Y=0
		triangles.emplace_back(RenderTriangle(p(1.0, 0.0, 1.0), p(0.0, 0.0, 1.0), p(0.0, 0.0, 0.0), uvTL, uvBL, uvBR, color));
		triangles.emplace_back(RenderTriangle(p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(1.0, 0.0, 1.0), uvBR, uvTR, uvTL, color));
		// Y=1
		triangles.emplace_back(RenderTriangle(p(1.0, 1.0, 0.0), p(0.0, 1.0, 0.0), p(0.0, 1.0, 1.0), uvTL, uvBL, uvBR, color));
		triangles.emplace_back(RenderTriangle(p(0.0, 1.0, 1.0), p(1.0, 1.0, 1.0), p(1.0, 1.0, 0.0), uvBR, uvTR, uvTL, color));
		// Z=0
		triangles.emplace_back(RenderTriangle(p(1.0, 1.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 0.0, 0.0), uvTL, uvBL, uvBR, color));
		triangles.emplace_back(RenderTriangle(p(0.0, 0.0, 0.0), p(0.0, 1.0, 0.0), p(1.0, 1.0, 0.0), uvBR, uvTR, uvTL, color));
		// Z=1
		triangles.emplace_back(RenderTriangle(p(0.0, 1.0, 1.0), p(0.0, 0.0, 1.0), p(1.0, 0.0, 1.0), uvTL, uvBL, uvBR, color));
		triangles.emplace_back(RenderTriangle(p(1.0, 0.0, 1.0), p(1.0, 1.0, 1.0), p(0.0, 1.0, 1.0), uvBR, uvTR, uvTL, color));

		return triangles;
	}

	std::vector<RenderTriangle> MakeDebugMesh1()
	{
		return MakeDebugCube(Double3::Zero);
	}

	std::vector<RenderTriangle> MakeDebugMesh2()
	{
		std::vector<RenderTriangle> triangles;

		for (int y = 0; y < 3; y += 2)
		{
			const Double3 point(
				24.0,
				static_cast<double>(y),
				0.0);
			std::vector<RenderTriangle> cubeTriangles = MakeDebugCube(point);
			triangles.insert(triangles.end(), cubeTriangles.begin(), cubeTriangles.end());
		}

		return triangles;
	}

	std::vector<RenderTriangle> MakeDebugMesh3()
	{
		std::vector<RenderTriangle> triangles;

		for (int z = 0; z < 64; z += 2)
		{
			for (int x = 0; x < 64; x += 2)
			{
				const Double3 point(
					static_cast<double>(x),
					0.0,
					static_cast<double>(z));
				std::vector<RenderTriangle> cubeTriangles = MakeDebugCube(point);
				triangles.insert(triangles.end(), cubeTriangles.begin(), cubeTriangles.end());
			}
		}

		return triangles;
	}
}

// Rendering functions, per-pixel work.
namespace swRender
{
	void DrawDebugRGB(const RenderCamera &camera, BufferView2D<uint32_t> &colorBuffer)
	{
		const int frameBufferWidth = colorBuffer.getWidth();
		const int frameBufferHeight = colorBuffer.getHeight();
		uint32_t *colorBufferPtr = colorBuffer.get();

		const Double3 cameraRightScaledDir = camera.right * camera.aspectRatio;

		for (int y = 0; y < frameBufferHeight; y++)
		{
			const double yPercent = (static_cast<double>(y) + 0.50) / static_cast<double>(frameBufferHeight);

			for (int x = 0; x < frameBufferWidth; x++)
			{
				const double xPercent = (static_cast<double>(x) + 0.50) / static_cast<double>(frameBufferWidth);

				const Double3 pixelDir = ((camera.forward - cameraRightScaledDir + camera.up) +
					(cameraRightScaledDir * (xPercent * 2.0)) -
					(camera.up * (yPercent * 2.0))).normalized();

				const Double3 pixelDirClamped(
					std::max(pixelDir.x, 0.0),
					std::max(pixelDir.y, 0.0),
					std::max(pixelDir.z, 0.0));

				const Color color(
					static_cast<uint8_t>(pixelDirClamped.x * 255.0),
					static_cast<uint8_t>(pixelDirClamped.y * 255.0),
					static_cast<uint8_t>(pixelDirClamped.z * 255.0));

				const uint32_t outputColor = color.toARGB();
				const int outputIndex = x + (y * frameBufferWidth);
				colorBufferPtr[outputIndex] = outputColor;
			}
		}
	}

	void ClearFrameBuffers(uint32_t clearColor, BufferView2D<uint32_t> &colorBuffer, BufferView2D<double> &depthBuffer)
	{
		colorBuffer.fill(clearColor);
		depthBuffer.fill(std::numeric_limits<double>::infinity());
	}

	// The provided triangles are assumed to be back-face culled and clipped.
	void RasterizeTriangles(const BufferView<const RenderTriangle> &triangles, const RenderCamera &camera,
		BufferView2D<uint32_t> &colorBuffer, BufferView2D<double> &depthBuffer)
	{
		const int frameBufferWidth = colorBuffer.getWidth();
		const int frameBufferHeight = colorBuffer.getHeight();
		const double frameBufferWidthReal = static_cast<double>(frameBufferWidth);
		const double frameBufferHeightReal = static_cast<double>(frameBufferHeight);

		const Double3 eye = swCamera::GetCameraEye(camera);
		const Matrix4d viewMatrix = Matrix4d::view(eye, camera.forward, camera.right, camera.up);
		const Matrix4d perspectiveMatrix = Matrix4d::perspective(camera.fovY, camera.aspectRatio,
			swConstants::NEAR_PLANE, swConstants::FAR_PLANE);

		constexpr double yShear = 0.0;

		uint32_t *colorBufferPtr = colorBuffer.get();
		double *depthBufferPtr = depthBuffer.get();

		for (int i = 0; i < triangles.getCount(); i++)
		{
			const RenderTriangle &triangle = triangles.get(i);
			const Double3 &v0 = triangle.v0;
			const Double3 &v1 = triangle.v1;
			const Double3 &v2 = triangle.v2;

			const Double4 view0 = RendererUtils::worldSpaceToCameraSpace(Double4(v0, 1.0), viewMatrix);
			const Double4 view1 = RendererUtils::worldSpaceToCameraSpace(Double4(v1, 1.0), viewMatrix);
			const Double4 view2 = RendererUtils::worldSpaceToCameraSpace(Double4(v2, 1.0), viewMatrix);

			// Nearest and farthest Z values (note these may be negative - behind the camera).
			const double zMin = std::min(view0.z, std::min(view1.z, view2.z));
			const double zMax = std::max(view0.z, std::max(view1.z, view2.z));

			/*if ((zMin < swConstants::NEAR_PLANE) || (zMin > swConstants::FAR_PLANE) ||
				(zMax < swConstants::NEAR_PLANE) || (zMax > swConstants::FAR_PLANE))
			{
				continue;
			}*/

			const Double4 clip0 = RendererUtils::cameraSpaceToClipSpace(view0, perspectiveMatrix);
			const Double4 clip1 = RendererUtils::cameraSpaceToClipSpace(view1, perspectiveMatrix);
			const Double4 clip2 = RendererUtils::cameraSpaceToClipSpace(view2, perspectiveMatrix);
			const Double3 ndc0 = RendererUtils::clipSpaceToNDC(clip0);
			const Double3 ndc1 = RendererUtils::clipSpaceToNDC(clip1);
			const Double3 ndc2 = RendererUtils::clipSpaceToNDC(clip2);
			const Double3 screenSpace0 = RendererUtils::ndcToScreenSpace(ndc0, yShear, frameBufferWidthReal, frameBufferHeightReal);
			const Double3 screenSpace1 = RendererUtils::ndcToScreenSpace(ndc1, yShear, frameBufferWidthReal, frameBufferHeightReal);
			const Double3 screenSpace2 = RendererUtils::ndcToScreenSpace(ndc2, yShear, frameBufferWidthReal, frameBufferHeightReal);
			const Double2 screenSpace0_2D(screenSpace0.x, screenSpace0.y);
			const Double2 screenSpace1_2D(screenSpace1.x, screenSpace1.y);
			const Double2 screenSpace2_2D(screenSpace2.x, screenSpace2.y);
			const Double2 screenSpace01 = screenSpace1_2D - screenSpace0_2D;
			const Double2 screenSpace12 = screenSpace2_2D - screenSpace1_2D;
			const Double2 screenSpace20 = screenSpace0_2D - screenSpace2_2D;
			const Double2 screenSpace01Perp = screenSpace01.rightPerp();
			const Double2 screenSpace12Perp = screenSpace12.rightPerp();
			const Double2 screenSpace20Perp = screenSpace20.rightPerp();

			// Naive screen-space bounding box around triangle.
			const double xMin = std::min(screenSpace0.x, std::min(screenSpace1.x, screenSpace2.x));
			const double xMax = std::max(screenSpace0.x, std::max(screenSpace1.x, screenSpace2.x));
			const double yMin = std::min(screenSpace0.y, std::min(screenSpace1.y, screenSpace2.y));
			const double yMax = std::max(screenSpace0.y, std::max(screenSpace1.y, screenSpace2.y));
			const int xStart = RendererUtils::getLowerBoundedPixel(xMin, frameBufferWidth);
			const int xEnd = RendererUtils::getUpperBoundedPixel(xMax, frameBufferWidth);
			const int yStart = RendererUtils::getLowerBoundedPixel(yMin, frameBufferHeight);
			const int yEnd = RendererUtils::getUpperBoundedPixel(yMax, frameBufferHeight);

			/*const Double3 color = Double3::fromRGB(triangle.color);
			const double depthPercent = (v0 - eye).length() / 20.0;
			const Double3 shadedColor = color * std::clamp(1.0 - depthPercent, 0.0, 1.0);
			const uint32_t colorInteger = shadedColor.toRGB();*/

			for (int y = yStart; y < yEnd; y++)
			{
				const double yScreenPercent = (static_cast<double>(y) + 0.50) / frameBufferHeightReal;

				for (int x = xStart; x < xEnd; x++)
				{
					const double xScreenPercent = (static_cast<double>(x) + 0.50) / frameBufferWidthReal;
					const Double2 pixelCenter(
						xScreenPercent * frameBufferWidthReal,
						yScreenPercent * frameBufferHeightReal);

					// See if pixel center is inside triangle.
					const bool inHalfSpace0 = MathUtils::isPointInHalfSpace(pixelCenter, screenSpace0_2D, screenSpace01Perp);
					const bool inHalfSpace1 = MathUtils::isPointInHalfSpace(pixelCenter, screenSpace1_2D, screenSpace12Perp);
					const bool inHalfSpace2 = MathUtils::isPointInHalfSpace(pixelCenter, screenSpace2_2D, screenSpace20Perp);
					if (inHalfSpace0 && inHalfSpace1 && inHalfSpace2)
					{
						const Double2 &ss0 = screenSpace01;
						const Double2 ss1 = screenSpace2_2D - screenSpace0_2D;
						const Double2 ss2 = pixelCenter - screenSpace0_2D;

						const double dot00 = ss0.dot(ss0);
						const double dot01 = ss0.dot(ss1);
						const double dot11 = ss1.dot(ss1);
						const double dot20 = ss2.dot(ss0);
						const double dot21 = ss2.dot(ss1);
						const double denominator = (dot00 * dot11) - (dot01 * dot01);

						const double v = ((dot11 * dot20) - (dot01 * dot21)) / denominator;
						const double w = ((dot00 * dot21) - (dot01 * dot20)) / denominator;
						const double u = 1.0 - v - w;

						const Double3 color(u, v, w);
						const uint32_t colorInteger = triangle.color;

						const int outputIndex = x + (y * frameBufferWidth);
						colorBufferPtr[outputIndex] = colorInteger;
					}
				}
			}
		}
	}
}

void SoftwareRenderer::ObjectTexture::init8Bit(int width, int height)
{
	this->texels.init(width, height);
}

void SoftwareRenderer::ObjectTexture::initPalette(int count)
{
	this->paletteTexels.init(count);
}

void SoftwareRenderer::ObjectTexture::clear()
{
	this->texels.clear();
	this->paletteTexels.clear();
}

SoftwareRenderer::SoftwareRenderer()
{
	this->nextObjectTextureID = -1;
}

SoftwareRenderer::~SoftwareRenderer()
{

}

void SoftwareRenderer::init(const RenderInitSettings &settings)
{
	this->depthBuffer.init(settings.width, settings.height);
	this->nextObjectTextureID = 0;
}

void SoftwareRenderer::shutdown()
{
	this->depthBuffer.clear();
	this->objectTextures.clear();
	this->freedObjectTextureIDs.clear();
	this->nextObjectTextureID = -1;
}

bool SoftwareRenderer::isInited() const
{
	return this->depthBuffer.isValid();
}

void SoftwareRenderer::resize(int width, int height)
{
	this->depthBuffer.init(width, height);
	this->depthBuffer.fill(std::numeric_limits<double>::infinity());
}

bool SoftwareRenderer::tryCreateObjectTexture(int width, int height, bool isPalette, ObjectTextureID *outID)
{
	ObjectTexture texture;
	if (!isPalette)
	{
		texture.init8Bit(width, height);
		texture.texels.fill(0);
	}
	else
	{
		texture.initPalette(width * height);
		texture.paletteTexels.fill(0);
	}

	if (!this->freedObjectTextureIDs.empty())
	{
		*outID = this->freedObjectTextureIDs.back();
		this->freedObjectTextureIDs.pop_back();
		this->objectTextures[*outID] = std::move(texture);
	}
	else
	{
		*outID = this->nextObjectTextureID;
		this->nextObjectTextureID++;
		this->objectTextures.emplace_back(std::move(texture));
	}

	return true;
}

bool SoftwareRenderer::tryCreateObjectTexture(const TextureBuilder &textureBuilder, ObjectTextureID *outID)
{
	const int width = textureBuilder.getWidth();
	const int height = textureBuilder.getHeight();
	if (!this->tryCreateObjectTexture(width, height, false, outID))
	{
		DebugLogWarning("Couldn't create " + std::to_string(width) + "x" + std::to_string(height) + " object texture.");
		return false;
	}

	DebugAssertIndex(this->objectTextures, *outID);
	ObjectTexture &texture = this->objectTextures[*outID];

	const TextureBuilder::Type textureBuilderType = textureBuilder.getType();
	if (textureBuilderType == TextureBuilder::Type::Paletted)
	{
		const TextureBuilder::PalettedTexture &palettedTexture = textureBuilder.getPaletted();
		std::copy(palettedTexture.texels.get(), palettedTexture.texels.end(), texture.texels.get());
	}
	else if (textureBuilderType == TextureBuilder::Type::TrueColor)
	{
		DebugLogWarning("True color texture (dimensions " + std::to_string(width) + "x" + std::to_string(height) + ") not supported.");
		texture.texels.fill(0);
	}
	else
	{
		DebugUnhandledReturnMsg(bool, std::to_string(static_cast<int>(textureBuilderType)));
	}

	return true;
}

LockedTexture SoftwareRenderer::lockObjectTexture(ObjectTextureID id)
{
	DebugAssertIndex(this->objectTextures, id);
	ObjectTexture &texture = this->objectTextures[id];
	if (texture.texels.isValid())
	{
		return LockedTexture(texture.texels.get(), false);
	}
	else if (texture.paletteTexels.isValid())
	{
		return LockedTexture(texture.paletteTexels.get(), true);
	}
	else
	{
		DebugNotImplemented();
		return LockedTexture(nullptr, false);
	}
}

void SoftwareRenderer::unlockObjectTexture(ObjectTextureID id)
{
	// Do nothing; any writes are already in RAM.
	static_cast<void>(id);
}

void SoftwareRenderer::freeObjectTexture(ObjectTextureID id)
{
	DebugAssertIndex(this->objectTextures, id);
	ObjectTexture &texture = this->objectTextures[id];
	texture.clear();
	this->freedObjectTextureIDs.emplace_back(id);
}

std::optional<Int2> SoftwareRenderer::tryGetObjectTextureDims(ObjectTextureID id) const
{
	DebugAssertIndex(this->objectTextures, id);
	const ObjectTexture &texture = this->objectTextures[id];
	return Int2(texture.texels.getWidth(), texture.texels.getHeight());
}

bool SoftwareRenderer::tryGetEntitySelectionData(const Double2 &uv, ObjectTextureID textureID, bool pixelPerfect, bool *outIsSelected) const
{
	if (pixelPerfect)
	{
		// Get the texture list from the texture group at the given animation state and angle.
		DebugAssertIndex(this->objectTextures, textureID);
		const ObjectTexture &texture = this->objectTextures[textureID];
		const int textureWidth = texture.texels.getWidth();
		const int textureHeight = texture.texels.getHeight();

		const int textureX = static_cast<int>(uv.x * static_cast<double>(textureWidth));
		const int textureY = static_cast<int>(uv.y * static_cast<double>(textureHeight));

		if ((textureX < 0) || (textureX >= textureWidth) ||
			(textureY < 0) || (textureY >= textureHeight))
		{
			// Outside the texture; out of bounds.
			return false;
		}

		// Check if the texel is non-transparent.
		const uint8_t texel = texture.texels.get(textureX, textureY);
		*outIsSelected = texel != 0;
		return true;
	}
	else
	{
		// The entity's projected rectangle is hit if the texture coordinates are valid.
		const bool withinEntity = (uv.x >= 0.0) && (uv.x <= 1.0) && (uv.y >= 0.0) && (uv.y <= 1.0);
		*outIsSelected = withinEntity;
		return true;
	}
}

Double3 SoftwareRenderer::screenPointToRay(double xPercent, double yPercent, const Double3 &cameraDirection,
	Degrees fovY, double aspect) const
{
	return LegacyRendererUtils::screenPointToRay(xPercent, yPercent, cameraDirection, fovY, aspect);
}

RendererSystem3D::ProfilerData SoftwareRenderer::getProfilerData() const
{
	const int renderWidth = this->depthBuffer.getWidth();
	const int renderHeight = this->depthBuffer.getHeight();

	// @todo
	const int threadCount = 1;
	const int potentiallyVisFlatCount = 0;
	const int visFlatCount = 0;
	const int visLightCount = 0;

	return ProfilerData(renderWidth, renderHeight, threadCount, potentiallyVisFlatCount, visFlatCount, visLightCount);
}

void SoftwareRenderer::submitFrame(const RenderCamera &camera, const RenderFrameSettings &settings, uint32_t *outputBuffer)
{
	const int frameBufferWidth = this->depthBuffer.getWidth();
	const int frameBufferHeight = this->depthBuffer.getHeight();
	BufferView2D<uint32_t> colorBufferView(outputBuffer, frameBufferWidth, frameBufferHeight);
	BufferView2D<double> depthBufferView(this->depthBuffer.get(), frameBufferWidth, frameBufferHeight);

	// Palette for 8-bit -> 32-bit color conversion.
	//const ObjectTexture &paletteTexture = this->objectTextures[settings.paletteTextureID];

	const uint32_t clearColor = Color::Gray.toARGB();
	swRender::ClearFrameBuffers(clearColor, colorBufferView, depthBufferView);
	swRender::DrawDebugRGB(camera, colorBufferView);

	const std::vector<RenderTriangle> triangles = swGeometry::MakeDebugMesh2();
	const BufferView<const RenderTriangle> trianglesView(triangles.data(), static_cast<int>(triangles.size()));

	const std::vector<RenderTriangle> clippedTriangles = swGeometry::ProcessTrianglesForRasterization(trianglesView, camera);
	const BufferView<const RenderTriangle> clippedTrianglesView(clippedTriangles.data(), static_cast<int>(clippedTriangles.size()));
	swRender::RasterizeTriangles(clippedTrianglesView, camera, colorBufferView, depthBufferView);
}

void SoftwareRenderer::present()
{
	// Do nothing for now, might change later.
}
