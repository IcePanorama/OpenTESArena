#include "Renderer.h"
#include "RenderTextureUtils.h"

#include "components/debug/Debug.h"

LockedTexture::LockedTexture(void *texels, bool isTrueColor)
{
	this->texels = texels;
	this->isTrueColor = isTrueColor;
}

bool LockedTexture::isValid()
{
	return this->texels != nullptr;
}

ObjectMaterial::ObjectMaterial(ObjectTextureID id0, ObjectTextureID id1)
{
	this->init(id0, id1);
}

ObjectMaterial::ObjectMaterial(ObjectTextureID id)
{
	this->init(id);
}

ObjectMaterial::ObjectMaterial()
	: ObjectMaterial(-1, -1) { }

void ObjectMaterial::init(ObjectTextureID id0, ObjectTextureID id1)
{
	this->id0 = id0;
	this->id1 = id1;
}

void ObjectMaterial::init(ObjectTextureID id)
{
	this->init(id, -1);
}

ScopedObjectTextureRef::ScopedObjectTextureRef(ObjectTextureID id, Renderer &renderer)
{
	DebugAssert(id >= 0);
	this->id = id;
	this->renderer = &renderer;
	this->setDims();
}

ScopedObjectTextureRef::ScopedObjectTextureRef()
{
	this->id = -1;
	this->renderer = nullptr;
	this->width = -1;
	this->height = -1;
}

ScopedObjectTextureRef::ScopedObjectTextureRef(ScopedObjectTextureRef &&other)
{
	this->id = other.id;
	this->renderer = other.renderer;
	this->width = other.width;
	this->height = other.height;
	other.id = -1;
	other.renderer = nullptr;
	other.width = -1;
	other.height = -1;
}

ScopedObjectTextureRef::~ScopedObjectTextureRef()
{
	this->destroy();
}

ScopedObjectTextureRef &ScopedObjectTextureRef::operator=(ScopedObjectTextureRef &&other)
{
	if (this != &other)
	{
		if (this->id >= 0)
		{
			this->destroy();
		}

		this->id = other.id;
		this->renderer = other.renderer;
		this->width = other.width;
		this->height = other.height;
		other.id = -1;
		other.renderer = nullptr;
		other.width = -1;
		other.height = -1;
	}

	return *this;
}

void ScopedObjectTextureRef::init(ObjectTextureID id, Renderer &renderer)
{
	DebugAssert(this->id == -1);
	DebugAssert(this->renderer == nullptr);
	DebugAssert(id >= 0);
	this->id = id;
	this->renderer = &renderer;
	this->setDims();
}

void ScopedObjectTextureRef::setDims()
{
	const std::optional<Int2> dims = this->renderer->tryGetObjectTextureDims(this->id);
	if (!dims.has_value())
	{
		DebugCrash("Couldn't get object texture dimensions (ID " + std::to_string(this->id) + ").");
	}

	this->width = dims->x;
	this->height = dims->y;
}

ObjectTextureID ScopedObjectTextureRef::get() const
{
	return this->id;
}

int ScopedObjectTextureRef::getWidth() const
{
	return this->width;
}

int ScopedObjectTextureRef::getHeight() const
{
	return this->height;
}

LockedTexture ScopedObjectTextureRef::lockTexels()
{
	return this->renderer->lockObjectTexture(this->id);
}

void ScopedObjectTextureRef::unlockTexels()
{
	this->renderer->unlockObjectTexture(this->id);
}

void ScopedObjectTextureRef::destroy()
{
	if (this->renderer != nullptr)
	{
		this->renderer->freeObjectTexture(this->id);
		this->renderer = nullptr;
		this->id = -1;
		this->width = -1;
		this->height = -1;
	}
}

ScopedObjectMaterialRef::ScopedObjectMaterialRef(ObjectMaterialID id, Renderer &renderer)
{
	DebugAssert(id >= 0);
	this->id = id;
	this->renderer = &renderer;
}

ScopedObjectMaterialRef::ScopedObjectMaterialRef()
{
	this->id = -1;
	this->renderer = nullptr;
}

ScopedObjectMaterialRef::ScopedObjectMaterialRef(ScopedObjectMaterialRef &&other)
{
	this->id = other.id;
	this->renderer = other.renderer;
	other.id = -1;
	other.renderer = nullptr;
}

ScopedObjectMaterialRef::~ScopedObjectMaterialRef()
{
	this->destroy();
}

ScopedObjectMaterialRef &ScopedObjectMaterialRef::operator=(ScopedObjectMaterialRef &&other)
{
	if (this != &other)
	{
		if (this->id >= 0)
		{
			this->destroy();
		}

		this->id = other.id;
		this->renderer = other.renderer;
		other.id = -1;
		other.renderer = nullptr;
	}

	return *this;
}

void ScopedObjectMaterialRef::init(ObjectMaterialID id, Renderer &renderer)
{
	DebugAssert(this->id == -1);
	DebugAssert(this->renderer == nullptr);
	DebugAssert(id >= 0);
	this->id = id;
	this->renderer = &renderer;
}

ObjectMaterialID ScopedObjectMaterialRef::get() const
{
	return this->id;
}

void ScopedObjectMaterialRef::destroy()
{
	if (this->renderer != nullptr)
	{
		this->renderer->freeObjectMaterial(this->id);
		this->renderer = nullptr;
		this->id = -1;
	}
}

ScopedUiTextureRef::ScopedUiTextureRef(UiTextureID id, Renderer &renderer)
{
	DebugAssert(id >= 0);
	this->id = id;
	this->renderer = &renderer;
	this->setDims();
}

ScopedUiTextureRef::ScopedUiTextureRef()
{
	this->id = -1;
	this->renderer = nullptr;
	this->width = -1;
	this->height = -1;
}

ScopedUiTextureRef::ScopedUiTextureRef(ScopedUiTextureRef &&other)
{
	this->id = other.id;
	this->renderer = other.renderer;
	this->width = other.width;
	this->height = other.height;
	other.id = -1;
	other.renderer = nullptr;
	other.width = -1;
	other.height = -1;
}

ScopedUiTextureRef::~ScopedUiTextureRef()
{
	if (this->renderer != nullptr)
	{
		this->renderer->freeUiTexture(this->id);
	}
}

ScopedUiTextureRef &ScopedUiTextureRef::operator=(ScopedUiTextureRef &&other)
{
	if (this != &other)
	{
		this->id = other.id;
		this->renderer = other.renderer;
		this->width = other.width;
		this->height = other.height;
		other.id = -1;
		other.renderer = nullptr;
		other.width = -1;
		other.height = -1;
	}

	return *this;
}

void ScopedUiTextureRef::init(UiTextureID id, Renderer &renderer)
{
	DebugAssert(this->id == -1);
	DebugAssert(this->renderer == nullptr);
	DebugAssert(id >= 0);
	this->id = id;
	this->renderer = &renderer;
	this->setDims();
}

void ScopedUiTextureRef::setDims()
{
	const std::optional<Int2> dims = this->renderer->tryGetUiTextureDims(this->id);
	if (!dims.has_value())
	{
		DebugCrash("Couldn't get UI texture dimensions (ID " + std::to_string(this->id) + ").");
	}

	this->width = dims->x;
	this->height = dims->y;
}

UiTextureID ScopedUiTextureRef::get() const
{
	return this->id;
}

int ScopedUiTextureRef::getWidth() const
{
	return this->width;
}

int ScopedUiTextureRef::getHeight() const
{
	return this->height;
}

uint32_t *ScopedUiTextureRef::lockTexels()
{
	return this->renderer->lockUiTexture(this->id);
}

void ScopedUiTextureRef::unlockTexels()
{
	this->renderer->unlockUiTexture(this->id);
}
