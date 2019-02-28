// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12SamplerState.hpp"

namespace NCryDX12
{

//---------------------------------------------------------------------------------------------------------------------
CSamplerState::CSamplerState()
	: CRefCounted()
	, m_DescriptorHandle(INVALID_CPU_DESCRIPTOR_HANDLE)
{
	// clear before use
	memset(&m_unSamplerDesc, 0, sizeof(m_unSamplerDesc));
}

//---------------------------------------------------------------------------------------------------------------------
CSamplerState::~CSamplerState()
{

}

}
