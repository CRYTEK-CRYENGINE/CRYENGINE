// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/Plugin/PluginServices/AkFXParameterChangeHandler.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
constexpr AkPluginParamID g_attachmentParamIDDummy = 1000;
constexpr AkUInt32 g_numAttachmentParams = 1;

// These structs are intentionally left empty as they are expected by the audio engine
struct CrySpatialAttachmentRTPCParams final
{
};

// These structs are intentionally left empty as they are expected by the audio engine
struct CrySpatialAttachmentNonRTPCParams final
{
};

class CrySpatialFXAttachmentParams final : public AK::IAkPluginParam
{
public:

	CrySpatialFXAttachmentParams() = default;

	explicit CrySpatialFXAttachmentParams(CrySpatialFXAttachmentParams const& in_rParams);

	~CrySpatialFXAttachmentParams() = default;

	// Create a duplicate of the parameter node instance in its current state.
	IAkPluginParam* Clone(AK::IAkPluginMemAlloc* in_pAllocator) override;

	// Initialize the plug-in parameter node interface.
	// Initializes the internal parameter structure to default values or with the provided parameter block if it is valid.
	AKRESULT Init(AK::IAkPluginMemAlloc* in_pAllocator, void const* in_pParamsBlock, AkUInt32 in_ulBlockSize) override;

	// Called by the sound engine when a parameter node is terminated.
	AKRESULT Term(AK::IAkPluginMemAlloc* in_pAllocator) override;

	// Set all plug-in parameters at once using a parameter block.
	AKRESULT SetParamsBlock(void const* in_pParamsBlock, AkUInt32 in_ulBlockSize) override;

	// Update a single parameter at a time and perform the necessary actions on the parameter changes.
	AKRESULT SetParam(AkPluginParamID in_paramID, void const* in_pValue, AkUInt32 in_ulParamSize) override;

	AK::AkFXParameterChangeHandler<g_numAttachmentParams> m_paramChangeHandler;

	CrySpatialAttachmentRTPCParams                        m_rtpc;
	CrySpatialAttachmentNonRTPCParams                     m_nonRtpc;
}

AK_ALIGN_DMA;
} // namespace Plugins
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio