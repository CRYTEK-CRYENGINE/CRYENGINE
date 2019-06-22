// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseTriggerConnection.h"
#include "GlobalData.h"
#include <PoolObject.h>

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
enum class ECueFlags : EnumFlagsType
{
	None           = 0,
	ToBeDestructed = BIT(0),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ECueFlags);

class CCue final : public CBaseTriggerConnection, public CPoolObject<CCue, stl::PSyncNone>
{
public:

	enum class EActionType : EnumFlagsType
	{
		None,
		Start,
		Stop,
		Pause,
		Resume,
	};

	CCue() = delete;
	CCue(CCue const&) = delete;
	CCue(CCue&&) = delete;
	CCue& operator=(CCue const&) = delete;
	CCue& operator=(CCue&&) = delete;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	explicit CCue(
		uint32 const id,
		char const* const szCueName,
		uint32 const acbId,
		EActionType const type,
		char const* const szCueSheetName,
		float const fadeOutTime)
		: CBaseTriggerConnection(EType::Cue, szCueName)
		, m_id(id)
		, m_cueSheetId(acbId)
		, m_actionType(type)
		, m_flags(ECueFlags::None)
		, m_pAcbHandle(nullptr)
		, m_numInstances(0)
		, m_cueSheetName(szCueSheetName)
		, m_fadeOutTime(fadeOutTime)
	{}
#else
	explicit CCue(
		uint32 const id,
		char const* const szCueName,
		uint32 const acbId,
		EActionType const type)
		: CBaseTriggerConnection(EType::Cue, szCueName)
		, m_id(id)
		, m_cueSheetId(acbId)
		, m_actionType(type)
		, m_flags(ECueFlags::None)
		, m_pAcbHandle(nullptr)
		, m_numInstances(0)
	{}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	virtual ~CCue() override = default;

	// CryAudio::Impl::ITriggerConnection
	virtual ETriggerResult Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId) override;
	virtual void           Stop(IObject* const pIObject) override;
	// ~CryAudio::Impl::ITriggerConnection

	uint32         GetId() const                 { return m_id; }
	CriAtomExAcbHn GetAcbHandle() const          { return m_pAcbHandle; }

	void           SetFlag(ECueFlags const flag) { m_flags |= flag; }

	void           IncrementNumInstances()       { ++m_numInstances; }
	void           DecrementNumInstances();

	bool           CanBeDestructed() const { return ((m_flags& ECueFlags::ToBeDestructed) != 0) && (m_numInstances == 0); }

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	float GetFadeOutTime() const { return m_fadeOutTime; }
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

private:

	uint32 const      m_id;
	uint32 const      m_cueSheetId;
	EActionType const m_actionType;
	ECueFlags         m_flags;
	CriAtomExAcbHn    m_pAcbHandle;
	uint16            m_numInstances;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_cueSheetName;
	float const m_fadeOutTime;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
