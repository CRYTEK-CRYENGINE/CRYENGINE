// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Common/Textures/DepthTexture.h>
#include <Common/ResourcePool.h>


struct STempDepthTexture : _i_reference_target<uint32_t> 
{
	SDepthTexture texture;
	int lastAccessFrameID = 0;

	STempDepthTexture(SDepthTexture &&texture) noexcept : texture(std::move(texture)) {}
	~STempDepthTexture();

	STempDepthTexture(const STempDepthTexture&) = delete;
	STempDepthTexture &operator=(const STempDepthTexture&) = delete;
	STempDepthTexture(STempDepthTexture&&) = delete;
	STempDepthTexture &operator=(STempDepthTexture&&) = delete;
};
