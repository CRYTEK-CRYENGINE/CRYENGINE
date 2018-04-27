// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 11:07:2009   Created by Benito G.R.
*************************************************************************/

#include "Stdafx.h"
#include "PostEffectBlender.h"
#include "EngineFacade/EngineFacade.h"

namespace Graphics
{

	CPostEffectBlender::CPostEffectBlender( EngineFacade::IEngineFacade& engine )
		: m_engine(engine)
	{
		m_activeBlends.reserve(8);
	}


	void CPostEffectBlender::AddBlendedEffect( const SBlendEffectParams& effectParams )
	{
		SInternalBlendParams internalParams;
		internalParams.m_blendParams = effectParams;

		if (internalParams.m_blendParams.m_initialValue == INVALID_BLEND_EFFECT_INITIAL_VALUE)
			internalParams.m_blendParams.m_initialValue = m_engine.GetEngine3DEngine().GetPostEffectParameter(internalParams.m_blendParams.m_postEffectName.c_str());

		m_activeBlends.push_back(internalParams);
	}


	void CPostEffectBlender::Update( float frameTime )
	{
		TActiveBlendsVector updatingBlends;
		updatingBlends.swap(m_activeBlends);

		const int updateCount = updatingBlends.size();

		for (int i = 0; i < updateCount; ++i)
		{
			SInternalBlendParams& blendParams = updatingBlends[i];
			blendParams.m_runningTime += frameTime;

			if (blendParams.m_runningTime >= blendParams.m_blendParams.m_blendTime)
			{
				m_engine.GetEngine3DEngine().SetPostProcessEffectParameter(blendParams.m_blendParams.m_postEffectName.c_str(), blendParams.m_blendParams.m_endValue);
			}
			else
			{
				const float effectTime = CLAMP(blendParams.m_blendParams.m_blendTime / blendParams.m_runningTime, 0.0f, 1.0f);
				const float diff = blendParams.m_blendParams.m_endValue - blendParams.m_blendParams.m_initialValue;
				const float postEffectValue = blendParams.m_blendParams.m_initialValue + (diff * effectTime);

				m_engine.GetEngine3DEngine().SetPostProcessEffectParameter(blendParams.m_blendParams.m_postEffectName.c_str(), postEffectValue);
				m_activeBlends.push_back(blendParams);
			}
		}
	}

	void CPostEffectBlender::Reset()
	{
		m_engine.GetEngine3DEngine().ResetPostEffects();
		m_activeBlends.clear();
	}

}
