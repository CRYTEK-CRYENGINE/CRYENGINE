// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERMODIFIABLEVALUES_H__
#define __PLAYERMODIFIABLEVALUES_H__

#include "AutoEnum.h"

struct IEntity;

#ifdef _RELEASE
#define ENABLE_PLAYER_MODIFIABLE_VALUES_DEBUGGING 0
#else
#define ENABLE_PLAYER_MODIFIABLE_VALUES_DEBUGGING 1
#endif

#define PlayerModifiableValueIDList(f)			\
	f(kPMV_WeaponReloadSpeedScale)						\
	f(kPMV_WeaponSelectSpeedScale)						\
	f(kPMV_WeaponZoomTimeScale)								\
	f(kPMV_WeaponAttachment_SelectSpeedScale)	\
	f(kPMV_WeaponAttachment_ZoomTimeScale)		\
	f(kPMV_HeavyWeaponMovementMultiplier)			\
	f(kPMV_HeavyWeaponRipOffSpeedOverride)		\
	f(kPMV_WeaponFireFromSprintTimeScale)			\
	f(kPMV_WeaponSprintToRunBlendTimeScale)		\
	f(kPMV_MovementSpeedMultiplier)						\
	f(kPMV_FlashBangBlindAmountMultiplier)		\
	f(kPMV_FlashBangBlindLengthMultiplier)		\
	f(kPMV_FlashBangStunMultiplier)						\
	f(kPMV_WeaponHipSpreadMultiplier)					\
	f(kPMV_WeaponTimeBetweenShotsMultiplier)	\
	f(kPMV_WeaponSightRecoilMultiplier)				\
	f(kPMV_SandboxInteractionSpeedMultiplier)	\
	f(kPMV_WeaponOverchargeDamageMultiplier)  \

AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(EPlayerModifiableValueID, PlayerModifiableValueIDList, kPMV_Num);

#define MAX_MODIFIERS_PER_VALUE		2

class CSingleModifiedValue
{
	private:
	float m_currentResult;
	const float * m_modifiers[MAX_MODIFIERS_PER_VALUE];

	public:
	CSingleModifiedValue()
		: m_currentResult(1.f)
#if ENABLE_PLAYER_MODIFIABLE_VALUES_DEBUGGING
		, m_dbgName(NULL)
		, m_dbgOwnerEntity(0)
#endif
	{
		memset (m_modifiers, 0, sizeof(m_modifiers));
	}

	ILINE operator float () const
	{
		return m_currentResult;
	}

	void AddModifier(const float * thisModifier);
	void RemoveModifier(const float * thisModifier);
	bool IsModifiedBy(const float* thisModifier);

	void ChangeModifierStateIfRequired(const float * thisModifier, bool onOff);
	
	ILINE void ChangeModifierState(const float * thisModifier, bool onOff)
	{
		if (onOff)
		{
			AddModifier(thisModifier);
		}
		else
		{
			RemoveModifier(thisModifier);
		}
	}

	~CSingleModifiedValue();

#if ENABLE_PLAYER_MODIFIABLE_VALUES_DEBUGGING
	const char  * m_dbgName;
	IEntity     * m_dbgOwnerEntity;
#endif
};

class CPlayerModifiableValues
{
	private:
	CSingleModifiedValue m_modifiableValuesArray[kPMV_Num];

	public:
	ILINE const CSingleModifiedValue & GetValue(EPlayerModifiableValueID valueID) const
	{
		assert (valueID >= 0 && valueID < kPMV_Num);
		return m_modifiableValuesArray[valueID];
	}

	ILINE CSingleModifiedValue & GetValue(EPlayerModifiableValueID valueID)
	{
		assert (valueID >= 0 && valueID < kPMV_Num);
		return m_modifiableValuesArray[valueID];
	}

#if ENABLE_PLAYER_MODIFIABLE_VALUES_DEBUGGING
	void DbgInit(IEntity * owner);
	void DbgTick();
#endif
};

#endif // __PLAYERMODIFIABLEVALUES_H__
