// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"

struct SSamplerState;

namespace NCryVulkan
{
class CSampler : public CDeviceObject
{
public:
	CSampler(CDevice* pDevice) : CDeviceObject(pDevice) {}
	CSampler(CSampler&&) = default;
	CSampler& operator=(CSampler&&) = default;
	virtual ~CSampler() override;

	VkResult Init(const SSamplerState& state);

	VkSampler GetHandle() const { return m_sampler; }

private:
	virtual void Destroy() override; // Uses CDevice::DeferDestruction

	CAutoHandle<VkSampler> m_sampler;
};
}
