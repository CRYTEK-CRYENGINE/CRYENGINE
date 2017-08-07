// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
struct IRenderAuxGeom;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
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
	CAudioEventManager(CAudioEventManager&&) = delete;
	CAudioEventManager& operator=(CAudioEventManager const&) = delete;
	CAudioEventManager& operator=(CAudioEventManager&&) = delete;

	void                Init(Impl::IImpl* const pIImpl);
	void                Release();
	void                Update(float const deltaTime);

	CATLEvent*          ConstructAudioEvent();
	void                ReleaseEvent(CATLEvent* const pEvent);

	size_t              GetNumConstructed() const;

private:

	std::list<CATLEvent*> m_constructedAudioEvents;
	Impl::IImpl*          m_pIImpl = nullptr;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, float const posX, float posY) const;

#endif //INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
