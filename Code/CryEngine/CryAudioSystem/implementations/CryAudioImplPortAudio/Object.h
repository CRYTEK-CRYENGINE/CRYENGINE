// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <IObject.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CEventInstance;
class CListener;

class CObject final : public IObject, public CPoolObject<CObject, stl::PSyncNone>
{
public:

	CObject(CObject const&) = delete;
	CObject(CObject&&) = delete;
	CObject& operator=(CObject const&) = delete;
	CObject& operator=(CObject&&) = delete;

	CObject(CListener* const pListener)
		: m_pListener(pListener)
	{}

	virtual ~CObject() override = default;

	// CryAudio::Impl::IObject
	virtual void                   Update(float const deltaTime) override;
	virtual void                   SetTransformation(CTransformation const& transformation) override;
	virtual CTransformation const& GetTransformation() const override { return CTransformation::GetEmptyObject(); }
	virtual void                   SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners) override;
	virtual void                   SetOcclusionType(EOcclusionType const occlusionType) override;
	virtual void                   StopAllTriggers() override;
	virtual ERequestStatus         SetName(char const* const szName) override;
	virtual void                   AddListener(IListener* const pIListener) override;
	virtual void                   RemoveListener(IListener* const pIListener) override;
	virtual void                   ToggleFunctionality(EObjectFunctionality const type, bool const enable) override;

	// Below data is only used when CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE is defined!
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter) override;
	// ~CryAudio::Impl::IObject

	CListener* GetListener() const { return m_pListener; }

	void       StopEvent(uint32 const pathId);
	void       RegisterEventInstance(CEventInstance* const pEventInstance);

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

private:

	CListener*     m_pListener;
	EventInstances m_eventInstances;

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
};
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
