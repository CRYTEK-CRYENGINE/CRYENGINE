// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CrySpatialFXAttachmentParams.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
//////////////////////////////////////////////////////////////////////////
CrySpatialFXAttachmentParams::CrySpatialFXAttachmentParams(const CrySpatialFXAttachmentParams& in_rParams)
	: m_rtpc(in_rParams.m_rtpc)
	, m_nonRtpc(in_rParams.m_nonRtpc)
{
	m_paramChangeHandler.SetAllParamChanges();
}

//////////////////////////////////////////////////////////////////////////
AK::IAkPluginParam* CrySpatialFXAttachmentParams::Clone(AK::IAkPluginMemAlloc* in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CrySpatialFXAttachmentParams(*this));
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFXAttachmentParams::Init(AK::IAkPluginMemAlloc* in_pAllocator, const void* in_pParamsBlock, AkUInt32 in_ulBlockSize)
{
	if (in_ulBlockSize == 0)
	{
		// Initialize default parameters here
		m_paramChangeHandler.SetAllParamChanges();
		return AK_Success;
	}

	return SetParamsBlock(in_pParamsBlock, in_ulBlockSize);
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFXAttachmentParams::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
	AK_PLUGIN_DELETE(in_pAllocator, this);
	return AK_Success;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFXAttachmentParams::SetParamsBlock(const void* in_pParamsBlock, AkUInt32 in_ulBlockSize)
{
	AKRESULT result = AK_Success;
	AkUInt8 const* pParamsBlock = static_cast<AkUInt8 const*>(in_pParamsBlock);

	// Read bank data here
	CHECKBANKDATASIZE(in_ulBlockSize, result);
	m_paramChangeHandler.SetAllParamChanges();

	return result;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFXAttachmentParams::SetParam(AkPluginParamID in_paramID, const void* in_pValue, AkUInt32 in_ulParamSize)
{
	AKRESULT result = AK_Success;

	// Handle parameter change here
	switch (in_paramID)
	{
	case g_attachmentParamIDDummy:
		{
			m_paramChangeHandler.SetParamChange(g_attachmentParamIDDummy);
			break;
		}
	default:
		{
			result = AK_InvalidParameter;
			break;
		}
	}

	return result;
}
}// namespace Plugins
}// namespace Wwise
}// namespace Impl
}// namespace CryAudio