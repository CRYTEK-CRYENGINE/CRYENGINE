// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CFacialInstance;
class CFacialEffectorsLibrary;
class CFaceState;
class CFaceIdentifierHandle;
class CCharInstance;

#define EYEMOVEMENT_EFFECTOR_AMOUNT 16

class CEyeMovementFaceAnim : public _i_reference_target_t
{
public:
	CEyeMovementFaceAnim(CFacialInstance* pInstance);

	void Update(float fDeltaTimeSec, const QuatTS& rAnimLocationNext, CCharInstance* pCharacter, CFacialEffectorsLibrary* pEffectorsLibrary, CFaceState* pFaceState);
	void OnExpressionLibraryLoad();

	void GetMemoryUsage(ICrySizer* pSizer) const;

private:
	enum DirectionID
	{
		DirectionNONE  = -1,

		DirectionEyeUp = 0,
		DirectionEyeUpRight,
		DirectionEyeRight,
		DirectionEyeDownRight,
		DirectionEyeDown,
		DirectionEyeDownLeft,
		DirectionEyeLeft,
		DirectionEyeUpLeft,

		DirectionCOUNT
	};

	enum EyeID
	{
		EyeLeft,
		EyeRight,

		EyeCOUNT
	};

	enum EffectorID
	{
		EffectorEyeLeftDirectionEyeUp,
		EffectorEyeLeftDirectionEyeUpRight,
		EffectorEyeLeftDirectionEyeRight,
		EffectorEyeLeftDirectionEyeDownRight,
		EffectorEyeLeftDirectionEyeDown,
		EffectorEyeLeftDirectionEyeDownLeft,
		EffectorEyeLeftDirectionEyeLeft,
		EffectorEyeLeftDirectionEyeUpLeft,
		EffectorEyeRightDirectionEyeUp,
		EffectorEyeRightDirectionEyeUpRight,
		EffectorEyeRightDirectionEyeRight,
		EffectorEyeRightDirectionEyeDownRight,
		EffectorEyeRightDirectionEyeDown,
		EffectorEyeRightDirectionEyeDownLeft,
		EffectorEyeRightDirectionEyeLeft,
		EffectorEyeRightDirectionEyeUpLeft,

		EffectorCOUNT
	};

	EffectorID EffectorFromEyeAndDirection(EyeID eye, DirectionID direction)
	{
		assert(eye >= 0 && eye < EyeCOUNT);
		assert(direction >= 0 && direction < DirectionCOUNT);
		return static_cast<EffectorID>(eye * DirectionCOUNT + direction);
	}

	EyeID EyeFromEffector(EffectorID effector)
	{
		assert(effector >= 0 && effector < EffectorCOUNT);
		return static_cast<EyeID>(effector / DirectionCOUNT);
	}

	DirectionID DirectionFromEffector(EffectorID effector)
	{
		assert(effector >= 0 && effector < EffectorCOUNT);
		return static_cast<DirectionID>(effector % DirectionCOUNT);
	}

	void                         InitialiseChannels();
	uint32                       GetChannelForEffector(EffectorID effector);
	uint32                       CreateChannelForEffector(EffectorID effector);
	void                         UpdateEye(const QuatTS& rAnimLocationNext, EyeID eye, const QuatT& additionalRotation);
	DirectionID                  FindEyeDirection(EyeID eye);
	void                         InitialiseBoneIDs();
	void                         FindLookAngleAndStrength(EyeID eye, float& angle, float& strength, const QuatT& additionalRotation);
	void                         DisplayDebugInfoForEye(const QuatTS& rAnimLocationNext, EyeID eye, const string& text);
	void                         CalculateEyeAdditionalRotation(CCharInstance* pCharacter, CFaceState* pFaceState, CFacialEffectorsLibrary* pEffectorsLibrary, QuatT additionalRotation[EyeCOUNT]);
	const CFaceIdentifierHandle* RetrieveEffectorIdentifiers() const;

	CFacialInstance*   m_pInstance;
	static const char* szEyeBoneNames[EyeCOUNT];
	static uint32      ms_eyeBoneNamesCrc[EyeCOUNT];

	class CChannelEntry
	{
	public:
		CChannelEntry() : id(~0), loadingAttempted(false) {}
		int  id;
		bool loadingAttempted;
	};

	CChannelEntry m_channelEntries[EffectorCOUNT];
	int           m_eyeBoneIDs[EyeCOUNT];
	bool          m_bInitialized;
};
