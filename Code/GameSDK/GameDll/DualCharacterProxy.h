// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Dual character proxy, to play animations on fp character and shadow character

-------------------------------------------------------------------------
History:
- 09-11-2009		Benito G.R. - Extracted from Player

*************************************************************************/

#pragma once

#ifndef _DUALCHARACTER_PROXY_H_
#define _DUALCHARACTER_PROXY_H_

#include <IAnimationGraph.h>

#if !defined(_RELEASE)

	#define PAIRFILE_GEN_ENABLED 1

#endif //!_RELEASE 

class CAnimationProxyDualCharacterBase : public CAnimationPlayerProxy
{
public:

	#if PAIRFILE_GEN_ENABLED
		static void Generate1P3PPairFile();
	#endif //PAIRFILE_GEN_ENABLED

	static void Load1P3PPairFile();
	static void ReleaseBuffers();

	CAnimationProxyDualCharacterBase();
	virtual bool StartAnimation(IEntity *entity, const char* szAnimName, const CryCharAnimationParams& Params, float speedMultiplier = 1.0f);

	virtual void OnReload();

protected:

	struct SPlayParams
	{
		int animIDFP;
		int animIDTP;
		float speedFP;
		float speedTP;
	};

	void GetPlayParams(int animID, float speedMul, IAnimationSet *animSet, SPlayParams &params);

	static int Get3PAnimID(IAnimationSet *animSet, int animID);


	typedef std::map<uint32, uint32> NameHashMap;
	static NameHashMap s_animCrCHashMap;

	int m_characterMain;
	int m_characterShadow;
	bool m_firstPersonMode;
};

class CAnimationProxyDualCharacter : public CAnimationProxyDualCharacterBase
{
public:

	CAnimationProxyDualCharacter();

	virtual bool StartAnimationById(IEntity *entity, int animId, const CryCharAnimationParams& Params, float speedMultiplier = 1.0f);
	virtual bool StopAnimationInLayer(IEntity *entity, int32 nLayer, f32 BlendOutTime);
	virtual bool RemoveAnimationInLayer(IEntity *entity, int32 nLayer, uint32 token);
	virtual const CAnimation *GetAnimation(IEntity *entity, int32 layer);
	virtual CAnimation *GetAnimation(IEntity *entity, int32 layer, uint32 token);

	virtual void OnReload();

	void SetFirstPerson(bool FP)
	{
		m_firstPersonMode = FP;
	}

	void SetKillMixInFirst(bool killMix)
	{
		m_killMixInFirst = killMix;
	}

	void SetCanMixUpperBody(bool canMix)
	{
		m_allowsMix = canMix;
	}

	bool CanMixUpperBody() const
	{
		return m_allowsMix;
	}

private:
	bool m_killMixInFirst;
	bool m_allowsMix;
};

class CAnimationProxyDualCharacterUpper : public CAnimationProxyDualCharacterBase
{
public:

	CAnimationProxyDualCharacterUpper();

	virtual bool StartAnimationById(IEntity *entity, int animId, const CryCharAnimationParams& Params, float speedMultiplier = 1.0f);
	virtual bool StopAnimationInLayer(IEntity *entity, int32 nLayer, f32 BlendOutTime);
	virtual bool RemoveAnimationInLayer(IEntity *entity, int32 nLayer, uint32 token);

	virtual void OnReload();

	void SetFirstPerson(bool FP)
	{
		m_firstPersonMode = FP;
	}

private:
	bool m_killMixInFirst;
};

#endif //_DUALCHARACTER_PROXY_H_