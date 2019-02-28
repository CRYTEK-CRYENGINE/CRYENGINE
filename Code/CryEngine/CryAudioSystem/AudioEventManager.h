// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include <CryMath/Cry_Math.h>
struct IRenderAuxGeom;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
class CATLEvent;

namespace Impl
{
struct IImpl;
} // namespace Impl

class CAudioEventManager final
{
public:

	CAudioEventManager() = default;
	~CAudioEventManager();

	CAudioEventManager(CAudioEventManager const&) = delete;
	CAudioEventManager& operator=(CAudioEventManager const&) = delete;

	void                Init(Impl::IImpl* const pIImpl);
	void                Release();

	CATLEvent*          ConstructAudioEvent();
	void                ReleaseEvent(CATLEvent* const pEvent);

	size_t              GetNumConstructed() const;

private:

	std::list<CATLEvent*> m_constructedAudioEvents;
	Impl::IImpl*          m_pIImpl = nullptr;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, float const posX, float posY) const;

#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
