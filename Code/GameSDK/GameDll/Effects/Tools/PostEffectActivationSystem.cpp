// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Simple data driven system to activate post effects.

// Includes
#include "StdAfx.h"
#include "PostEffectActivationSystem.h"
#include "IItemSystem.h"

//--------------------------------------------------------------------------------------------------
// Name: Initialise
// Desc: Initialises post effect activation system from data
//       Uses the xml node name for the post effect, and activeValue and nonActiveValue attributes
//--------------------------------------------------------------------------------------------------
void CPostEffectActivationSystem::Initialise(const IItemParamsNode* postEffectListXmlNode)
{
	if(postEffectListXmlNode)
	{
		const IItemParamsNode* postEffectXmlNode = NULL;
		SPostEffectParam* param = NULL;
		int childCount = postEffectListXmlNode->GetChildCount();
		int postEffectCount = childCount;

		const IItemParamsNode* vecsXmlNode = postEffectListXmlNode->GetChild("vecs");

		if(vecsXmlNode)
		{
			postEffectCount--;
		}

		m_postEffectParam.resize(postEffectCount);

		int paramIndex=0;
		for(int c=0; c<childCount; c++)
		{
			postEffectXmlNode = postEffectListXmlNode->GetChild(c);
			if(postEffectXmlNode && postEffectXmlNode != vecsXmlNode)
			{
				param = &m_postEffectParam[paramIndex];
				cry_strcpy(param->name, postEffectXmlNode->GetName());
				postEffectXmlNode->GetAttribute("activeValue",param->activeValue);
				postEffectXmlNode->GetAttribute("nonActiveValue",param->nonActiveValue);

				int forceValue = 0;
				postEffectXmlNode->GetAttribute("forceValue",forceValue);
				param->forceValue = (forceValue) ? true : false;
				paramIndex++;
			}
		}

		if(vecsXmlNode)
		{
			SPostEffectParamVec* paramVec = NULL;
			const int vecCount = vecsXmlNode->GetChildCount();
			m_postEffectParamVec.resize(vecCount);
			for(int i=0; i<vecCount; i++)
			{
				postEffectXmlNode = vecsXmlNode->GetChild(i);

				paramVec = &m_postEffectParamVec[i];
				cry_strcpy(paramVec->name, postEffectXmlNode->GetName());

				Vec3 vecValue(0.0f,0.0f,0.0f);
				float wValue = 0.0f;

				postEffectXmlNode->GetAttribute("activeVec3",vecValue);
				postEffectXmlNode->GetAttribute("activeW",wValue);
				paramVec->activeValue = Vec4(vecValue,wValue);

				postEffectXmlNode->GetAttribute("nonActiveVec3",vecValue);
				postEffectXmlNode->GetAttribute("nonActiveW",wValue);
				paramVec->nonActiveValue = Vec4(vecValue,wValue);

				int forceValue = 0;
				postEffectXmlNode->GetAttribute("forceValue",forceValue);
				paramVec->forceValue = (forceValue) ? true : false;
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Release
// Desc: Releases data used for post effect activation system
//--------------------------------------------------------------------------------------------------
void CPostEffectActivationSystem::Release()
{
	m_postEffectParam.Free();
	m_postEffectParamVec.Free();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetPostEffectsActive
// Desc: Sets active status of post effects
//--------------------------------------------------------------------------------------------------
void CPostEffectActivationSystem::SetPostEffectsActive(bool isActive)
{
	SPostEffectParam* param = NULL;
	float value = 0.0f;
	for(size_t i=0; i<m_postEffectParam.Size(); i++)
	{
		param = &m_postEffectParam[i];
		value = (isActive) ? param->activeValue : param->nonActiveValue;
		gEnv->p3DEngine->SetPostEffectParam( param->name, value, param->forceValue );
	}

	SPostEffectParamVec* paramVec = NULL;
	Vec4* pVec = NULL;
	for(size_t i=0; i<m_postEffectParamVec.Size(); i++)
	{
		paramVec = &m_postEffectParamVec[i];
		pVec = (isActive) ? &paramVec->activeValue : &paramVec->nonActiveValue;
		gEnv->p3DEngine->SetPostEffectParamVec4( paramVec->name, *pVec, paramVec->forceValue );
	}
}//-------------------------------------------------------------------------------------------------
