// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TriggerInstance.h"
#include "Common/PoolObject.h"
#include <CryAudio/IObject.h>

#if defined(CRY_AUDIO_USE_OCCLUSION)
	#include "PropagationProcessor.h"
#endif // CRY_AUDIO_USE_OCCLUSION

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Debug.h"

struct IRenderAuxGeom;
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
enum class EObjectFlags : EnumFlagsType
{
	None                  = 0,
	InUse                 = BIT(0),
	Active                = BIT(1),
	Virtual               = BIT(2),
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CanRunOcclusion       = BIT(3),
	TrackAbsoluteVelocity = BIT(4),
	TrackRelativeVelocity = BIT(5),
	IgnoreDrawDebugInfo   = BIT(6),
#endif // CRY_AUDIO_USE_DEBUG_CODE
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

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CObject(CTransformation const& transformation, char const* const szName)
		: m_pIObject(nullptr)
		, m_flags(EObjectFlags::InUse)
		, m_transformation(transformation)
		, m_numPendingSyncCallbacks(0)
	#if defined(CRY_AUDIO_USE_OCCLUSION)
		, m_propagationProcessor(*this)
	#endif // CRY_AUDIO_USE_OCCLUSION
		, m_maxRadius(0.0f)
		, m_name(szName)
	{}
#else
	explicit CObject(CTransformation const& transformation)
		: m_pIObject(nullptr)
		, m_flags(EObjectFlags::InUse)
		, m_transformation(transformation)
		, m_numPendingSyncCallbacks(0)
	#if defined(CRY_AUDIO_USE_OCCLUSION)
		, m_propagationProcessor(*this)
	#endif // CRY_AUDIO_USE_OCCLUSION
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	void HandleSetTransformation(CTransformation const& transformation);
	void HandleAddListener(ListenerId const id);
	void HandleRemoveListener(ListenerId const id);

	void Init(Impl::IObject* const pIObject, Listeners const& listeners);
	void Destruct();

	void StopAllTriggers();

#if defined(CRY_AUDIO_USE_OCCLUSION)
	void SetOcclusion(float const occlusion);
	void HandleSetOcclusionType(EOcclusionType const calcType);
	void ProcessPhysicsRay(CRayInfo& rayInfo);
	void HandleSetOcclusionRayOffset(float const offset);
	void ReleasePendingRays();
#endif // CRY_AUDIO_USE_OCCLUSION

	Impl::IObject*         GetImplData() const       { return m_pIObject; }
	Listeners const&       GetListeners() const      { return m_listeners; }
	CTransformation const& GetTransformation() const { return m_transformation; }

	bool                   IsPlaying() const;
	bool                   HasPendingCallbacks() const;

	EObjectFlags           GetFlags() const { return m_flags; }
	void                   SetFlag(EObjectFlags const flag);
	void                   RemoveFlag(EObjectFlags const flag);

	void                   Update(float const deltaTime);

	void                   IncrementSyncCallbackCounter() { CryInterlockedIncrement(&m_numPendingSyncCallbacks); }
	void                   DecrementSyncCallbackCounter() { CRY_ASSERT(m_numPendingSyncCallbacks >= 1); CryInterlockedDecrement(&m_numPendingSyncCallbacks); }

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	void ConstructTriggerInstance(
		ControlId const triggerId,
		EntityId const entityId,
		uint16 const numPlayingConnectionInstances,
		uint16 const numPendingConnectionInstances,
		ERequestFlags const flags,
		void* const pOwner,
		void* const pUserData,
		void* const pUserDataOwner,
		float const radius);
#else
	void ConstructTriggerInstance(
		ControlId const triggerId,
		EntityId const entityId,
		uint16 const numPlayingConnectionInstances,
		uint16 const numPendingConnectionInstances,
		ERequestFlags const flags,
		void* const pOwner,
		void* const pUserData,
		void* const pUserDataOwner);
#endif // CRY_AUDIO_USE_DEBUG_CODE

	void ReportStartedTriggerInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result);
	void ReportFinishedTriggerInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result);

private:

	// CryAudio::IObject
	virtual void ExecuteTrigger(ControlId const triggerId, EntityId const entityId = INVALID_ENTITYID, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetTransformation(CTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetEnvironment(EnvironmentId const environmentId, float const amount, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetCurrentEnvironments(EntityId const entityToIgnore = INVALID_ENTITYID, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetOcclusionRayOffset(float const offset, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void AddListener(ListenerId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void RemoveListener(ListenerId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void ToggleAbsoluteVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void ToggleRelativeVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	// ~CryAudio::IObject

	void SetActive();

	TriggerInstances m_triggerInstances;
	Impl::IObject*   m_pIObject;
	EObjectFlags     m_flags;
	CTransformation  m_transformation;
	volatile int     m_numPendingSyncCallbacks;

#if defined(CRY_AUDIO_USE_OCCLUSION)
	CPropagationProcessor m_propagationProcessor;
#endif // CRY_AUDIO_USE_OCCLUSION

	Listeners m_listeners;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
public:

	void           Release();
	void           SetImplData(Impl::IObject* const pIObject);
	#if defined(CRY_AUDIO_USE_OCCLUSION)
	void           ResetObstructionRays() { m_propagationProcessor.ResetRayData(); }
	#endif // CRY_AUDIO_USE_OCCLUSION
	void           ForceImplementationRefresh();

	char const*    GetName() const { return m_name.c_str(); }
	ERequestStatus HandleSetName(char const* const szName);

	void           StoreParameterValue(ControlId const id, float const value);
	void           StoreSwitchValue(ControlId const switchId, SwitchStateId const switchStateId);
	void           StoreEnvironmentValue(ControlId const id, float const value);

	void           UpdateMaxRadius(float const radius);

	void           DrawDebugInfo(
		IRenderAuxGeom& auxGeom,
		bool const isTextFilterDisabled,
		CryFixedStringT<MaxControlNameLength> const& lowerCaseSearchString);

private:

	ParameterValues                      m_parameters;
	SwitchStateIds                       m_switchStates;
	EnvironmentValues                    m_environments;
	float                                m_maxRadius;
	CryFixedStringT<MaxObjectNameLength> m_name;
	Debug::StateDrawInfo                 m_stateDrawInfo;
#endif // CRY_AUDIO_USE_DEBUG_CODE
};
} // namespace CryAudio
