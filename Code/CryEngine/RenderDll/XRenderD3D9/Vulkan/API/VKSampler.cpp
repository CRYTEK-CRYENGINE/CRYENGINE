// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKSampler.hpp"
#include "VKDevice.hpp"
#include "VKInstance.hpp"

namespace NCryVulkan
{
static VkFilter MapFilter(int filter, float& impliedAniso)
{
	float aniso;
	switch (filter)
	{
	default:
		VK_ASSERT(false && "Unknown filter value");
		// Fall through
	case FILTER_NONE:
	case FILTER_POINT:
		return VK_FILTER_NEAREST;
	case FILTER_LINEAR:
	case FILTER_BILINEAR:
	case FILTER_TRILINEAR:
		return VK_FILTER_LINEAR;
	case FILTER_ANISO2X:
		aniso = 2.0f;
		break;
	case FILTER_ANISO4X:
		aniso = 4.0f;
		break;
	case FILTER_ANISO8X:
		aniso = 8.0f;
		break;
	case FILTER_ANISO16X:
		aniso = 16.0f;
		break;
	}
	if (aniso > impliedAniso)
	{
		impliedAniso = aniso;
	}
	return VK_FILTER_LINEAR;
}

static VkSamplerMipmapMode MapMipFilter(int filter)
{
	return filter < FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

static VkSamplerAddressMode MapAddressMode(ESamplerAddressMode mode)
{
	switch (mode)
	{
	case eSamplerAddressMode_Wrap:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case eSamplerAddressMode_Mirror:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	default:
		VK_ASSERT("Unknown address-mode expression");
	case eSamplerAddressMode_Clamp:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case eSamplerAddressMode_Border:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	}
}

static float MapAnisotropy(float specifiedAniso, float impliedAniso)
{
	return specifiedAniso < 1.0f ? impliedAniso : specifiedAniso;
}

static VkBorderColor MapBorderColor(DWORD color)
{
	switch (color)
	{
	default:
		VK_ASSERT("Unknown border color expression");
	case 0x00000000:
		return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	case 0x000000FF:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	case 0xFFFFFFFF:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	}
}

CSampler::~CSampler()
{
	m_sampler.Destroy(vkDestroySampler, GetDevice()->GetVkDevice());
}

VkResult CSampler::Init(const SSamplerState& state)
{
	float impliedAniso = 1.0f;

	VkSamplerCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.magFilter = MapFilter(state.m_nMagFilter, impliedAniso);
	info.minFilter = MapFilter(state.m_nMinFilter, impliedAniso);
	info.mipmapMode = MapMipFilter(state.m_nMipFilter);
	info.addressModeU = MapAddressMode(static_cast<ESamplerAddressMode>(state.m_nAddressU));
	info.addressModeV = MapAddressMode(static_cast<ESamplerAddressMode>(state.m_nAddressV));
	info.addressModeW = MapAddressMode(static_cast<ESamplerAddressMode>(state.m_nAddressW));
	info.mipLodBias = state.m_fMipLodBias;
	info.maxAnisotropy = MapAnisotropy(state.m_nAnisotropy, impliedAniso);
	info.anisotropyEnable = info.maxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
	info.compareEnable = state.m_bComparison ? VK_TRUE : VK_FALSE;
	info.compareOp = VK_COMPARE_OP_LESS;
	info.minLod = 0.0f;
	info.maxLod = 14.0f;
	info.borderColor = MapBorderColor(state.m_dwBorderColor);
	info.unnormalizedCoordinates = VK_FALSE;

	// Limit anisotropy
	const VkPhysicalDeviceFeatures& features = GetDevice()->GetPhysicalDeviceInfo()->deviceFeatures;
	const VkPhysicalDeviceLimits& limits = GetDevice()->GetPhysicalDeviceInfo()->deviceProperties.limits;
	const float maxAnisotropy = features.samplerAnisotropy == VK_TRUE ? limits.maxSamplerAnisotropy : 1.0f;
	if (info.maxAnisotropy > maxAnisotropy)
	{
		info.maxAnisotropy = maxAnisotropy;

		static bool bOnce = false;
		VK_ASSERT(bOnce && "Clamped anisotropy to device limit, output may not be as expected");
		bOnce = true;
	}
	else if (!(info.maxAnisotropy >= 1.0f)) // Paranoid check to catch NaN, some drivers confirmed to crash when passed value outside [1.0f, limits.maxAniso]
	{
		info.maxAnisotropy = 1.0f;
	}

	// Limit LoD
	const float maxLod = limits.maxSamplerLodBias;
	if (info.maxLod > maxLod)
	{
		info.maxLod = maxLod;

		static bool bOnce = false;
		VK_ASSERT(bOnce && "Clamped max LoD to device limit, output may not be as expected");
		bOnce = true;
	}

	return vkCreateSampler(GetDevice()->GetVkDevice(), &info, nullptr, &m_sampler);
}

void CSampler::Destroy()
{
	GetDevice()->DeferDestruction(std::move(*this));
	CRefCounted::Destroy();
}

}
