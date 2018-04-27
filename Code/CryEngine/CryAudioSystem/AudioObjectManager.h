// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
class CATLAudioObject;

namespace Impl
{
struct SObject3DAttributes;
}

class CObjectManager final
{
public:

	using ConstructedObjects = std::vector<CATLAudioObject*>;

	CObjectManager() = default;
	~CObjectManager();

	CObjectManager(CObjectManager const&) = delete;
	CObjectManager(CObjectManager&&) = delete;
	CObjectManager& operator=(CObjectManager const&) = delete;
	CObjectManager& operator=(CObjectManager&&) = delete;

	void            Init(uint32 const poolSize);
	void            SetImpl(Impl::IImpl* const pIImpl);
	void            Release();
	void            Update(float const deltaTime, Impl::SObject3DAttributes const& listenerAttributes);
	void            RegisterObject(CATLAudioObject* const pObject);

	void            ReportStartedEvent(CATLEvent* const pEvent);
	void            ReportFinishedEvent(CATLEvent* const pEvent, bool const bSuccess);
	void            GetStartedStandaloneFileRequestData(CATLStandaloneFile* const pStandaloneFile, CAudioRequest& request);
	void            ReportFinishedStandaloneFile(CATLStandaloneFile* const pStandaloneFile);
	void            ReleasePendingRays();
	bool            IsActive(CATLAudioObject const* const pAudioObject) const;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	size_t                    GetNumAudioObjects() const;
	size_t                    GetNumActiveAudioObjects() const;
	ConstructedObjects const& GetAudioObjects() const { return m_constructedObjects; }
	void                      DrawPerObjectDebugInfo(
	  IRenderAuxGeom& auxGeom,
	  Vec3 const& listenerPos,
	  AudioTriggerLookup const& triggers,
	  AudioParameterLookup const& parameters,
	  AudioSwitchLookup const& switches,
	  AudioPreloadRequestLookup const& preloadRequests,
	  AudioEnvironmentLookup const& environments) const;
	void DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, float const posX, float posY) const;

#endif // INCLUDE_AUDIO_PRODUCTION_CODE

private:

	bool HasActiveData(CATLAudioObject const* const pAudioObject) const;

	Impl::IImpl*       m_pIImpl = nullptr;
	ConstructedObjects m_constructedObjects;
};
} // namespace CryAudio
