// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScreenEffects.h"

#include "BlendTypes.h"
#include "BlendedEffect.h"
#include "BlendNode.h"

#include "GameCVars.h"

#include <IActorSystem.h>
#include <IMovementController.h>


//-------------------------------------------------------------------------
void CScreenEffects::ProcessExplosionEffect(float blurRadius, const Vec3& explosionPos)
{
	//Enable radial-blur postFX on near explosions
	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Radius", blurRadius);
	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Amount", 1.0f);

	IBlendedEffect *pBlur	= CBlendedEffect<CPostProcessEffect>::Create(CPostProcessEffect("FilterRadialBlurring_Amount", 0.0f));
	IBlendType   *pLinear = CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
	StartBlend(pBlur, pLinear, 1.0f, CScreenEffects::eSFX_GID_RBlur);
	SetUpdateCoords("FilterRadialBlurring_ScreenPosX","FilterRadialBlurring_ScreenPosY", explosionPos);
}

//--------------------------------------------------------------------------
void CScreenEffects::ProcessZoomInEffect()
{
	//Disable some blend groups while zoomed-in
	EnableBlends(false, CScreenEffects::eSFX_GID_ZoomIn);
	EnableBlends(false, CScreenEffects::eSFX_GID_ZoomOut);
	EnableBlends(false, CScreenEffects::eSFX_GID_HitReaction);
}

//-----------------------------------------------------------------------
void CScreenEffects::ProcessZoomOutEffect()
{
	//Renable blend groups after zooming out
	ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomOut);
	ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomIn);
	ResetBlendGroup(CScreenEffects::eSFX_GID_HitReaction);
	EnableBlends(true, CScreenEffects::eSFX_GID_ZoomIn);
	EnableBlends(true, CScreenEffects::eSFX_GID_ZoomOut);
	EnableBlends(true, CScreenEffects::eSFX_GID_HitReaction);
}

//-----------------------------------------------------------
void CScreenEffects::ProcessSlidingFX()
{
	//Enable radial-blur postFX when entering sliding
	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Radius", g_pGameCVars->cl_slidingBlurRadius);
	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Amount", g_pGameCVars->cl_slidingBlurAmount);
	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_ScreenPosX", 0.5f);
	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_ScreenPosY", 0.5f);

	IBlendedEffect *pBlur	= CBlendedEffect<CPostProcessEffect>::Create(CPostProcessEffect("FilterRadialBlurring_Amount", 0.0f));
	IBlendType   *pLinear = CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
	StartBlend(pBlur, pLinear, g_pGameCVars->cl_slidingBlurBlendSpeed, CScreenEffects::eSFX_GID_RBlur);

	//Add also some view shake
	//CamShake(Vec3(0.02f, 0.03f, 0.007f), Vec3(0.15f, 0.15f, -0.1f), 0.15f, 0.8f);
}

//-----------------------------------------------------------
void CScreenEffects::ProcessSprintingFX(bool sprinting, bool isInAir)
{
	ClearBlendGroup(CScreenEffects::eSFX_GID_MotionBlur);

	if(sprinting)
	{
		//Blend towards sprint scale
		IBlendedEffect *pBlur	= CBlendedEffect<CPostProcessEffect>::Create(CPostProcessEffect("MotionBlur_VectorsScale", g_pGameCVars->cl_motionBlurVectorScaleSprint));
		IBlendType   *pLinear = CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
		StartBlend(pBlur, pLinear, 1.2f, CScreenEffects::eSFX_GID_MotionBlur);

		//Zoom in effect
		ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomIn);
		ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomOut);
		{
			const float fovScale = g_pGame->GetPowerSprintTargetFov() / g_pGame->GetFOV();

			IBlendType   *blend = CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
			IBlendedEffect *fovEffect	= CBlendedEffect<CFOVEffect>::Create(CFOVEffect(fovScale));

			StartBlend(fovEffect, blend, 1.0f, CScreenEffects::eSFX_GID_ZoomIn);
		}
	}
	else
	{
		//Blend back to default value
		IBlendedEffect *pBlur	= CBlendedEffect<CPostProcessEffect>::Create(CPostProcessEffect("MotionBlur_VectorsScale", g_pGameCVars->cl_motionBlurVectorScale));
		IBlendType   *pLinear = CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
		StartBlend(pBlur, pLinear, 1.2f, CScreenEffects::eSFX_GID_MotionBlur);

		//Zoom out effect
		ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomIn);
		ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomOut);

		const float effectSpeed = isInAir ? 0.5f : 2.5f;

		IBlendedEffect *fov	= CBlendedEffect<CFOVEffect>::Create(CFOVEffect(1.0f));
		IBlendType *blend		= CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
		StartBlend(fov, blend, effectSpeed, CScreenEffects::eSFX_GID_ZoomOut);
	}

}

void CScreenEffects::ResetGameEffectPools()
{
	CBlendedEffect<CPostProcessEffect>::FreePool();
	CBlendedEffect<CFOVEffect>::FreePool();
	CBlendType<CLinearBlend>::FreePool();
}
