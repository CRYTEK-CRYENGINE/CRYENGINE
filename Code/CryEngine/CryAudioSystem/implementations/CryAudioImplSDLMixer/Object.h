// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <IObject.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CListener;

using VolumeMultipliers = std::map<SampleId, float>;

class CObject final : public IObject, public CPoolObject<CObject, stl::PSyncNone>
{
public:

	CObject() = delete;
	CObject(CObject const&) = delete;
	CObject(CObject&&) = delete;
	CObject& operator=(CObject const&) = delete;
	CObject& operator=(CObject&&) = delete;

	explicit CObject(CTransformation const& transformation, CListener* const pListener)
		: m_pListener(pListener)
		, m_transformation(transformation)
	{}

	virtual ~CObject() override = default;

	// CryAudio::Impl::IObject
	virtual void                   Update(float const deltaTime) override;
	virtual void                   SetTransformation(CTransformation const& transformation) override                                            { m_transformation = transformation; }
	virtual CTransformation const& GetTransformation() const override                                                                           { return m_transformation; }
	virtual void                   SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners) override {}
	virtual void                   SetOcclusionType(EOcclusionType const occlusionType) override                                                {}
	virtual void                   StopAllTriggers() override;
	virtual ERequestStatus         SetName(char const* const szName) override;
	virtual void                   AddListener(IListener* const pIListener) override;
	virtual void                   RemoveListener(IListener* const pIListener) override;
	virtual void                   ToggleFunctionality(EObjectFunctionality const type, bool const enable) override {}

	// Below data is only used when CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE is defined!
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter) override {}
	// ~CryAudio::Impl::IObject

	CListener* GetListener() const { return m_pListener; }

	void       StopEvent(uint32 const id);
	void       SetVolume(SampleId const sampleId, float const value);

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	EventInstances    m_eventInstances;
	VolumeMultipliers m_volumeMultipliers;

private:

	CListener*      m_pListener;
	CTransformation m_transformation;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio