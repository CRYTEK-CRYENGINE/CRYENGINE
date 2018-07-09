// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Dual character proxy, to play animations on fp character and shadow character

-------------------------------------------------------------------------
History:
- 09-11-2009		Benito G.R. - Extracted from Player

*************************************************************************/

#include "StdAfx.h"
#include "DualCharacterProxy.h"

#include "RecordingSystem.h"
#include "GameCVars.h"

#include <CryString/StringUtils.h>

static float GetTPSpeedMul(IAnimationSet *animSet, int animIDFP, int animIDTP)
{
	float TPSpeedMul = 1.0f;

	if ((animIDFP >= 0) && (animIDTP >= 0))
	{
		float durationFP = animSet->GetDuration_sec(animIDFP);
		float durationTP = animSet->GetDuration_sec(animIDTP);
		if ((durationFP > 0.0f) && (durationTP > 0.0f))
		{
			TPSpeedMul = (durationTP / durationFP);
		}
	}

	return TPSpeedMul;
}

CAnimationProxyDualCharacterBase::NameHashMap CAnimationProxyDualCharacterBase::s_animCrCHashMap;

const char *ANIMPAIR_PATHNAME = "scripts/animPairs.xml";

#if PAIRFILE_GEN_ENABLED

void CAnimationProxyDualCharacterBase::Generate1P3PPairFile()
{
	const uint32 MAX_MODELS = 128;
	IDefaultSkeleton* pIDefaultSkeletons[MAX_MODELS];
	uint32 numModels;
	gEnv->pCharacterManager->GetLoadedModels(NULL, numModels);
	numModels = min(numModels, MAX_MODELS);
	
	gEnv->pCharacterManager->GetLoadedModels(pIDefaultSkeletons, numModels);

	s_animCrCHashMap.clear();

	for (uint32 i=0; i<numModels; ++i)
	{
		const IDefaultSkeleton& rIDefaultSkeleton = (const IDefaultSkeleton&)(*pIDefaultSkeletons[i]);
		uint32 numInstances = gEnv->pCharacterManager->GetNumInstancesPerModel(rIDefaultSkeleton);
		if (numInstances > 0)
		{
			IAnimationSet* animSet = gEnv->pCharacterManager->GetICharInstanceFromModel(rIDefaultSkeleton,0)->GetIAnimationSet();
			uint32 numAnims = animSet->GetAnimationCount();

			for (uint32 anm = 0; anm < numAnims; anm++)
			{
				uint32 animCRC = animSet->GetCRCByAnimID(anm);
				if (s_animCrCHashMap.find(animCRC) == s_animCrCHashMap.end())
				{
					int animID3P = -1;
					const char *name = animSet->GetNameByAnimID(anm);
					if (strlen(name) >= 255)
					{
						CRY_ASSERT_MESSAGE(0, string().Format("[CAnimationProxyDualCharacterBase::Generate1P3PPairFiles] Animname %s overruns buffer", name));
						CryLogAlways("[CAnimationProxyDualCharacterBase::Generate1P3PPairFiles] Animname %s overruns buffer", name);
						continue;
					}
					const char *pos = CryStringUtils::stristr(name, "_1p");
					if (pos)
					{
						char name3P[256];
						cry_strcpy(name3P, name);
						name3P[(int)(TRUNCATE_PTR)pos + 1 - (int)(TRUNCATE_PTR)name] = '3';
						animID3P = animSet->GetAnimIDByName(name3P);

						if (animID3P >= 0)
						{
							uint32 animCRCTP = animSet->GetCRCByAnimID(animID3P);
							s_animCrCHashMap[animCRC] = animCRCTP;
						}
					}
				}
			}
		}
	}


	//--- Save the file
	CryFixedStringT<256> animCrC;
	XmlNodeRef nodePairList	= gEnv->pSystem->CreateXmlNode( "Pairs" );	
	for (NameHashMap::iterator iter = s_animCrCHashMap.begin(); iter != s_animCrCHashMap.end(); ++iter)
	{
		XmlNodeRef nodePair = nodePairList->newChild( "Pair" );
		animCrC.Format("%u", iter->first);
		nodePair->setAttr( "FP", animCrC.c_str());
		animCrC.Format("%u", iter->second);
		nodePair->setAttr( "TP", animCrC.c_str());
	}
	nodePairList->saveToFile(ANIMPAIR_PATHNAME);

	s_animCrCHashMap.clear();
	Load1P3PPairFile();
}

#endif //PAIRFILE_GEN_ENABLED

void CAnimationProxyDualCharacterBase::Load1P3PPairFile()
{
	if (s_animCrCHashMap.empty())
	{
		XmlNodeRef nodeAttachList	= gEnv->pSystem->LoadXmlFromFile(ANIMPAIR_PATHNAME);	

		if (!nodeAttachList)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load anim pair file! %s No 3P animations will play!", ANIMPAIR_PATHNAME);
		}
		else
		{
			int numPairs = nodeAttachList->getChildCount();
			for (int i=0; i<numPairs; i++)
			{
				XmlNodeRef pair = nodeAttachList->getChild(i);
				uint32 animCRCFP, animCRCTP;
				pair->getAttr("FP", animCRCFP);
				pair->getAttr("TP",animCRCTP);		
				s_animCrCHashMap[animCRCFP] = animCRCTP;
			}
		}
	}
}

void CAnimationProxyDualCharacterBase::ReleaseBuffers()
{
	s_animCrCHashMap.clear();
}

int CAnimationProxyDualCharacterBase::Get3PAnimID(IAnimationSet *animSet, int animID)
{
	if (animID >= 0)
	{
		uint32 crc = animSet->GetCRCByAnimID(animID);
		NameHashMap::iterator iter = s_animCrCHashMap.find(crc);
		if (iter != s_animCrCHashMap.end())
		{
			return animSet->GetAnimIDByCRC(iter->second);
		}
	}

	return animID;
}

CAnimationProxyDualCharacterBase::CAnimationProxyDualCharacterBase()
:
	m_characterMain(0),
	m_characterShadow(5),
	m_firstPersonMode(true)
{
}

void CAnimationProxyDualCharacterBase::OnReload()
{
	m_characterMain = 0;
	m_characterShadow = 5;
	m_firstPersonMode = true;
}

void CAnimationProxyDualCharacterBase::GetPlayParams(int animID, float speedMul, IAnimationSet *animSet, SPlayParams &params)
{
	params.animIDFP = animID;
	params.animIDTP = Get3PAnimID(animSet, animID);
	params.speedFP  = speedMul;
	params.speedTP  = speedMul;

	if (params.animIDFP != params.animIDTP)
	{
		params.speedTP *= GetTPSpeedMul(animSet, params.animIDFP, params.animIDTP);
	}
}

bool CAnimationProxyDualCharacterBase::StartAnimation(IEntity *entity, const char* szAnimName, const CryCharAnimationParams& Params, float speedMultiplier)
{
	ICharacterInstance* pICharacter = entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		int animID = pICharacter->GetIAnimationSet()->GetAnimIDByName(szAnimName);

#if !defined(_RELEASE)
		if (g_pGameCVars->g_animatorDebug)
		{
			static const ColorF col (1.0f, 0.0f, 0.0f, 1.0f);
			if (animID < 0) 
			{
				g_pGame->GetIGameFramework()->GetIPersistantDebug()->Add2DText(string().Format("Missing %s", szAnimName).c_str(), 1.0f, col, 10.0f);
			}
		}
#endif //!defined(_RELEASE)

		return StartAnimationById(entity, animID, Params, speedMultiplier);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CAnimationProxyDualCharacter::CAnimationProxyDualCharacter()
: m_killMixInFirst(true)
, m_allowsMix(false)
{

}

void CAnimationProxyDualCharacter::OnReload()
{
	CAnimationProxyDualCharacterBase::OnReload();

	m_killMixInFirst = true;
	m_allowsMix = false;
}

bool CAnimationProxyDualCharacter::StartAnimationById(IEntity *entity, int animId, const CryCharAnimationParams& Params, float speedMultiplier)
{
	ICharacterInstance* pICharacter = entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;
		IAnimationSet* pAnimSet = pICharacter->GetIAnimationSet();

		SPlayParams playParams;
		GetPlayParams(animId, speedMultiplier, pAnimSet, playParams);

		bool shadowLocomotion = !(((Params.m_nFlags & CA_DISABLE_MULTILAYER) || (Params.m_nLayerID != 0)) || !m_killMixInFirst);
		bool bAnimStarted = false;
		bool bShouldPlayShadow = true;
		CryCharAnimationParams localParams = Params;
		if (!m_firstPersonMode || !shadowLocomotion)
		{
			localParams.m_fPlaybackSpeed *= m_firstPersonMode ? playParams.speedFP : playParams.speedTP;
			bAnimStarted = pISkeletonAnim->StartAnimationById(m_firstPersonMode ? playParams.animIDFP : playParams.animIDTP, localParams);
			if (bAnimStarted)
			{
				pISkeletonAnim->SetLayerBlendWeight(Params.m_nLayerID, 1.0f);
			}
			else
				bShouldPlayShadow = false;
		}

		if (bShouldPlayShadow && pIShadowCharacter != NULL)
		{
			ISkeletonAnim* pIShadowSkeletonAnim = pIShadowCharacter->GetISkeletonAnim();
			localParams.m_fPlaybackSpeed = Params.m_fPlaybackSpeed * playParams.speedTP;
			if (pIShadowSkeletonAnim->StartAnimationById(playParams.animIDTP, localParams))
			{
				bAnimStarted = true;

				pIShadowSkeletonAnim->SetLayerBlendWeight(Params.m_nLayerID, 1.0f);
			}
		}

		if (bAnimStarted && (Params.m_nLayerID == 0))
		{
			m_allowsMix = (Params.m_nFlags & CA_DISABLE_MULTILAYER) == 0;
		}

		return bAnimStarted;
	}

	return false;
}

bool CAnimationProxyDualCharacter::StopAnimationInLayer(IEntity *entity, int32 nLayer, f32 BlendOutTime)
{
	ICharacterInstance* pICharacter = entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;

		if (pIShadowCharacter)
		{
			ISkeletonAnim* pIShadowSkeletonAnim = pIShadowCharacter->GetISkeletonAnim();

			bool stopped = pIShadowSkeletonAnim->StopAnimationInLayer(nLayer, BlendOutTime);
			if (m_allowsMix)
			{
				return stopped;
			}
		}

		return pISkeletonAnim->StopAnimationInLayer(nLayer, BlendOutTime);
	}

	return false;
}

bool CAnimationProxyDualCharacter::RemoveAnimationInLayer(IEntity *entity, int32 nLayer, uint32 token)
{
	ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;
	ICharacterInstance* pICharacter = pIShadowCharacter ? pIShadowCharacter : entity->GetCharacter(m_characterMain);

	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

		if (pIShadowCharacter)
		{
			ISkeletonAnim* pIShadowSkeletonAnim = pIShadowCharacter->GetISkeletonAnim();
			int nAnimsInFIFO = pIShadowSkeletonAnim->GetNumAnimsInFIFO(nLayer);
			for (int i=0; i<nAnimsInFIFO; ++i)
			{
				const CAnimation& anim = pIShadowSkeletonAnim->GetAnimFromFIFO(nLayer, i);
				if (anim.HasUserToken(token))
				{
					pIShadowSkeletonAnim->RemoveAnimFromFIFO(nLayer, i);
				}
			}
		}

		int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(nLayer);
		for (int i=0; i<nAnimsInFIFO; ++i)
		{
			const CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO(nLayer, i);
			if (anim.HasUserToken(token))
			{
				return pISkeletonAnim->RemoveAnimFromFIFO(nLayer, i);
			}
		}
	}

	return false;
}

const CAnimation *CAnimationProxyDualCharacter::GetAnimation(IEntity *entity, int32 layer)
{
	ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;
	ICharacterInstance* pICharacter = pIShadowCharacter ? pIShadowCharacter : entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

		int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(layer);
		if (nAnimsInFIFO > 0)
		{
			return &pISkeletonAnim->GetAnimFromFIFO(layer, nAnimsInFIFO-1);
		}
	}

	return NULL;
}

CAnimation *CAnimationProxyDualCharacter::GetAnimation(IEntity *entity, int32 layer, uint32 token)
{
	ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;
	ICharacterInstance* pICharacter = pIShadowCharacter ? pIShadowCharacter : entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

		int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(layer);
		if (nAnimsInFIFO == 0)
		{
			return NULL;
		}
		if (token == INVALID_ANIMATION_TOKEN)
		{
			return &pISkeletonAnim->GetAnimFromFIFO(layer, 0);
		}
		else
		{
			return pISkeletonAnim->FindAnimInFIFO(token, layer);
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CAnimationProxyDualCharacterUpper::CAnimationProxyDualCharacterUpper()
: m_killMixInFirst(true)
{

}

void CAnimationProxyDualCharacterUpper::OnReload()
{
	CAnimationProxyDualCharacterBase::OnReload();

	m_killMixInFirst = true;
}

bool CAnimationProxyDualCharacterUpper::StartAnimationById(IEntity *entity, int animId, const CryCharAnimationParams& Params, float speedMultiplier)
{
	ICharacterInstance* pICharacter = entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;
		IAnimationSet* pAnimSet = pICharacter->GetIAnimationSet();

		SPlayParams playParams;
		GetPlayParams(animId, speedMultiplier, pAnimSet, playParams);

		CryCharAnimationParams localParams = Params;

		bool startedMain = false;
		if (m_firstPersonMode)
		{
			localParams.m_fPlaybackSpeed *= playParams.speedFP;
			startedMain = pISkeletonAnim->StartAnimationById(playParams.animIDFP, localParams);
		}
		else
		{
			localParams.m_fPlaybackSpeed *= playParams.speedTP;
			startedMain = pISkeletonAnim->StartAnimationById(playParams.animIDTP, localParams);
		}

		if (startedMain)
		{
			pISkeletonAnim->SetLayerBlendWeight(Params.m_nLayerID, 1.0f);
		}

		if (pIShadowCharacter != NULL && (Params.m_nLayerID != 0))
		{
			ISkeletonAnim* pIShadowSkeletonAnim = pIShadowCharacter->GetISkeletonAnim();

			localParams.m_fPlaybackSpeed = Params.m_fPlaybackSpeed * playParams.speedTP;

			if (pIShadowSkeletonAnim->StartAnimationById(playParams.animIDTP, localParams))
			{
				pIShadowSkeletonAnim->SetLayerBlendWeight(Params.m_nLayerID, 1.0f);
			}
		}

		return startedMain;
	}

	return false;
}

bool CAnimationProxyDualCharacterUpper::StopAnimationInLayer(IEntity *entity, int32 nLayer, f32 BlendOutTime)
{
	ICharacterInstance* pICharacter = entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;

		if (pIShadowCharacter)
		{
			ISkeletonAnim* pIShadowSkeletonAnim = pIShadowCharacter->GetISkeletonAnim();

			pIShadowSkeletonAnim->StopAnimationInLayer(nLayer, BlendOutTime);
		}

		return pISkeletonAnim->StopAnimationInLayer(nLayer, BlendOutTime);
	}

	return false;
}

bool CAnimationProxyDualCharacterUpper::RemoveAnimationInLayer(IEntity *entity, int32 nLayer, uint32 token)
{
	ICharacterInstance* pIShadowCharacter = m_firstPersonMode ? entity->GetCharacter(m_characterShadow) : NULL;
	ICharacterInstance* pICharacter = pIShadowCharacter ? pIShadowCharacter : entity->GetCharacter(m_characterMain);
	if (pICharacter)
	{
		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

		if (pIShadowCharacter)
		{
			ISkeletonAnim* pIShadowSkeletonAnim = pIShadowCharacter->GetISkeletonAnim();
			int nAnimsInFIFO = pIShadowSkeletonAnim->GetNumAnimsInFIFO(nLayer);
			for (int i=0; i<nAnimsInFIFO; ++i)
			{
				const CAnimation& anim = pIShadowSkeletonAnim->GetAnimFromFIFO(nLayer, i);
				if (anim.HasUserToken(token))
				{
					pIShadowSkeletonAnim->RemoveAnimFromFIFO(nLayer, i);
				}
			}
		}

		int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(nLayer);
		for (int i=0; i<nAnimsInFIFO; ++i)
		{
			const CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO(nLayer, i);
			if (anim.HasUserToken(token))
			{
				return pISkeletonAnim->RemoveAnimFromFIFO(nLayer, i);
			}
		}
	}

	return false;
}
