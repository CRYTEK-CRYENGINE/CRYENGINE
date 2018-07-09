// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/VR/IHMDDevice.h>

namespace CryVR
{
namespace Osvr
{
struct Texture
{
	void* pTexture;
	void* pRtView;
};

struct TextureSet
{
	Texture* pTextures;
	uint32   numTextures;
};

struct TextureSwapSet
{
	TextureSet* pTextureSets;
	uint32      numTextureSets;
};

struct RenderParameters
{
	unsigned int width;
	unsigned int height;
};

struct IOsvrDevice : public IHmdDevice
{
public:
	virtual bool InitializeRenderer(void* pD3dDevice, void* pD3dContext) = 0;
	virtual bool RegisterTextureSwapSet(TextureSwapSet* pSwapSet) = 0;
	virtual bool PresentTextureSet(int textureSetIndex) = 0;

	virtual void ShutdownRenderer() = 0;
protected:
	virtual ~IOsvrDevice() {};
};
}       // namespace Osvr
}       // namespace CryVR
