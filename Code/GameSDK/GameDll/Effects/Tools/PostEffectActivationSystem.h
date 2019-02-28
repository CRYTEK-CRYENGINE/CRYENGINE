// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _POST_EFFECT_ACTIVATION_SYSTEM_
#define _POST_EFFECT_ACTIVATION_SYSTEM_

#pragma once

// Defines
const int MAX_POST_EFFECT_NAME = 64;

//==================================================================================================
// Name: SPostEffectParam
// Desc: Post effect param used in the post effect activation system
// Author: James Chilvers
//==================================================================================================
struct SPostEffectParam
{
	SPostEffectParam()
	{
		name[0] = 0;
		activeValue = 0.0f;
		nonActiveValue = 0.0f;
		forceValue = false;
	}

	void GetMemoryUsage(ICrySizer *pSizer) const {}

	char		name[MAX_POST_EFFECT_NAME];
	float		activeValue;
	float		nonActiveValue;
	bool		forceValue;
};//------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: SPostEffectParamVec
// Desc: Post effect param used in the post effect activation system
// Author: James Chilvers
//==================================================================================================
struct SPostEffectParamVec
{
	SPostEffectParamVec()
	{
		name[0] = 0;
		activeValue = Vec4(0.0f,0.0f,0.0f,0.0f);
		nonActiveValue = Vec4(0.0f,0.0f,0.0f,0.0f);
		forceValue = false;
	}

	void GetMemoryUsage(ICrySizer *pSizer) const {}

	char		name[MAX_POST_EFFECT_NAME];
	Vec4		activeValue;
	Vec4		nonActiveValue;
	bool		forceValue;
};//------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: CPostEffectActivationSystem
// Desc: Simple data driven system to activate post effects 
// Author: James Chilvers
//==================================================================================================
class CPostEffectActivationSystem
{
public:
	CPostEffectActivationSystem(){}
	~CPostEffectActivationSystem(){}

	void Initialise(const IItemParamsNode* postEffectListXmlNode);	// Uses the xml node name for the post effect, and activeValue and nonActiveValue attributes
																																	// eg <Global_User_Brightness activeValue="3.0" nonActiveValue="1.0"/>
	
	void Release();
	
	void SetPostEffectsActive(bool isActive);

	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddContainer(m_postEffectParam);
	}
private:
	PodArray<SPostEffectParam>			m_postEffectParam;
	PodArray<SPostEffectParamVec>		m_postEffectParamVec;
};//------------------------------------------------------------------------------------------------

#endif // _POST_EFFECT_ACTIVATION_SYSTEM_
