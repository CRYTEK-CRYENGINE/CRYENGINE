// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include "ListenerInfo.h"
#include <IObject.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CEventInstance;

enum class EObjectFlags : EnumFlagsType
{
	None                  = 0,
	MovingOrDecaying      = BIT(0),
	TrackAbsoluteVelocity = BIT(1),
	TrackRelativeVelocity = BIT(2),
	UpdateVirtualStates   = BIT(3),
	IsVirtual             = BIT(4),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EObjectFlags);

class CObject final : public IObject, public CPoolObject<CObject, stl::PSyncNone>
{
public:

	CObject() = delete;
	CObject(CObject const&) = delete;
	CObject(CObject&&) = delete;
	CObject& operator=(CObject const&) = delete;
	CObject& operator=(CObject&&) = delete;

	explicit CObject(
		AkGameObjectID const id,
		CTransformation const& transformation,
		ListenerInfos const& listenerInfos,
		char const* const szName);
	virtual ~CObject() override;

	// CryAudio::Impl::IObject
	virtual void                   Update(float const deltaTime) override;
	virtual void                   SetTransformation(CTransformation const& transformation) override;
	virtual CTransformation const& GetTransformation() const override { return m_transformation; }
	virtual void                   SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners) override;
	virtual void                   SetOcclusionType(EOcclusionType const occlusionType) override;
	virtual void                   StopAllTriggers() override;
	virtual ERequestStatus         SetName(char const* const szName) override;
	virtual void                   AddListener(IListener* const pIListener) override;
	virtual void                   RemoveListener(IListener* const pIListener) override;
	virtual void                   ToggleFunctionality(EObjectFunctionality const type, bool const enable) override;

	// Below data is only used when CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE is defined!
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter) override;
	// ~CryAudio::Impl::IObject

	AkGameObjectID GetId() const { return m_id; }

	void           AddEventInstance(CEventInstance* const pEventInstance);
	void           StopEvent(AkUniqueID const eventId);

	void           SetAuxSendValues();
	void           SetAuxBusSend(AkAuxBusID const busId, float const amount);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	char const* GetName() const          { return m_name.c_str(); }
	char const* GetListenerNames() const { return m_listenerNames.c_str(); }
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

private:

	void UpdateVelocities(float const deltaTime);
	void TryToSetRelativeVelocity(float const relativeVelocity);
	void SetDistanceToListener();
	void SetListeners();

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	void UpdateListenerNames();
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	AkGameObjectID const m_id;
	EObjectFlags         m_flags;
	bool                 m_needsToUpdateAuxSends;
	float                m_shortestDistanceToListener;
	float                m_previousRelativeVelocity;
	float                m_previousAbsoluteVelocity;
	CTransformation      m_transformation;
	Vec3                 m_position;
	Vec3                 m_previousPosition;
	Vec3                 m_velocity;
	ListenerInfos        m_listenerInfos;
	EventInstances       m_eventInstances;

	using AuxSendValues = std::vector<AkAuxSendValue>;
	AuxSendValues m_auxSendValues;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CryFixedStringT<MaxObjectNameLength> m_name;
	CryFixedStringT<MaxMiscStringLength> m_listenerNames;
	std::map<char const* const, float>   m_parameterInfo;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
