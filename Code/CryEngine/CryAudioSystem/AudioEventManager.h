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

class CEventManager final
{
public:

	CEventManager() = default;
	~CEventManager();

	CEventManager(CEventManager const&) = delete;
	CEventManager& operator=(CEventManager const&) = delete;

	void           Init(uint32 const poolSize);
	void           SetImpl(Impl::IImpl* const pIImpl);
	void           Release();

	CATLEvent*     ConstructEvent();
	void           DestructEvent(CATLEvent* const pEvent);

	size_t         GetNumConstructed() const;

private:

	Impl::IImpl*            m_pIImpl = nullptr;
	std::vector<CATLEvent*> m_constructedEvents;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, float const posX, float posY) const;

#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
