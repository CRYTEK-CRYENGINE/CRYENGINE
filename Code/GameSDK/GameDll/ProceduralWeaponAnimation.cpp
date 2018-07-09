// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProceduralWeaponAnimation.h"
#include "Player.h"
#include "PlayerInput.h"


namespace
{



	void DrawCircle(float aspectRatio, float offsetX, float widthMult, float radius, ColorB color)
	{
		const int numSegments = 24;
		Vec3 points[numSegments];
		vtx_idx indices[numSegments*2];

		for (int i = 0; i < numSegments; ++i)
		{
			float t = (i / float(numSegments)) * gf_PI * 2.0f;
			float x = cos_tpl(t) * radius + 0.5f;
			float y = sin_tpl(t) * radius * aspectRatio + 0.5f;
			points[i] = Vec3(x, y, 0.0f);
			indices[i*2] = i;
			indices[i*2+1] = i+1;
		}
		indices[(numSegments-1)*2+1] = 0;

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLines(
			points, numSegments+1,
			indices, numSegments*2, color);
	}



	void DrawGoldenRatio(float aspectRatio, float offsetX, float widthMult)
	{
		const float invAspectRatio = 1.0f / aspectRatio;
		const float oneThird = 1.0f / 3.0f;
		const ColorB gold = ColorB(192, 128, 0);
		const ColorB lightGold = ColorB(255, 255, 196);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(offsetX, 0.0f, 0.0f), gold,
			Vec3(offsetX, 1.0f, 0.0f), gold);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(oneThird*widthMult+offsetX, 0.0f, 0.0f), gold,
			Vec3(oneThird*widthMult+offsetX, 1.0f, 0.0f), gold);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(oneThird*2.0f*widthMult+offsetX, 0.0f, 0.0f), gold,
			Vec3(oneThird*2.0f*widthMult+offsetX, 1.0f, 0.0f), gold);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(widthMult+offsetX, 0.0f, 0.0f), gold,
			Vec3(widthMult+offsetX, 1.0f, 0.0f), gold);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(offsetX, oneThird, 0.0f), gold,
			Vec3(widthMult+offsetX, oneThird, 0.0f), gold);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(offsetX, oneThird*2.0f, 0.0f), gold,
			Vec3(widthMult+offsetX, oneThird*2.0f, 0.0f), gold);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(0.5f-invAspectRatio*0.5f, 0.0f, 0.0f), lightGold,
			Vec3(0.5f+invAspectRatio*0.5f, 1.0f, 0.0f), lightGold);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(0.5f-invAspectRatio*0.5f, 1.0f, 0.0f), lightGold,
			Vec3(0.5f+invAspectRatio*0.5f, 0.0f, 0.0f), lightGold);
	}



	void DrawSights(float aspectRatio, float offsetX, float widthMult)
	{
		const float oneThird = 1.0f / 3.0f;
		const ColorB scopeColor = ColorB(96, 128, 192);
		const ColorB reflexColor = ColorB(192, 128, 96);
		const ColorB ironSightColor = ColorB(0, 0, 0);
		const ColorB markupColor = ColorB(0, 255, 128);
		const float sniperScopeRadius = 0.28f;
		const float assaultScopeRadius = 0.19f;
		const float reflexRadius = 0.08f;
		const float ironsightSize = 0.025f;

		DrawCircle(aspectRatio, offsetX, widthMult, sniperScopeRadius, scopeColor);
		DrawCircle(aspectRatio, offsetX, widthMult, assaultScopeRadius, scopeColor);
		DrawCircle(aspectRatio, offsetX, widthMult, reflexRadius, reflexColor);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(oneThird*widthMult+offsetX, 0.5f, 0.0f), ironSightColor,
			Vec3(oneThird*2.0f*widthMult+offsetX, 0.5f, 0.0f), ironSightColor);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(0.5f, 0.5f, 0.0f), ironSightColor,
			Vec3(0.5f, 1.0f, 0.0f), ironSightColor);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(0.5f, 0.48f, 0.0f), markupColor,
			Vec3(0.5f, 0.52f, 0.0f), markupColor);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(0.5f-ironsightSize, 0.48f, 0.0f), markupColor,
			Vec3(0.5f-ironsightSize, 0.52f, 0.0f), markupColor);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			Vec3(0.5f+ironsightSize, 0.48f, 0.0f), markupColor,
			Vec3(0.5f+ironsightSize, 0.52f, 0.0f), markupColor);
	}



	void DrawWeaponFireDirection(CPlayer* pPlayer)
	{
		CItem* pCurrentWeapon = static_cast<CItem*>(pPlayer->GetCurrentItem());
		if (!pCurrentWeapon)
			return;

		const float infinity = 1000.0f;
		const float sphereRadius = 5.0f;
		const ColorB startColor = ColorB(0, 255, 0);
		const ColorB endColor = ColorB(0, 128, 0);
		const ColorB sphereColor = ColorB(255, 0, 0);
		
		Vec3 pos = pCurrentWeapon->GetSlotHelperPos(eIGS_FirstPerson, "weapon_term", true);
		Matrix33 rotation = pCurrentWeapon->GetSlotHelperRotation(eIGS_FirstPerson, "weapon_term", true);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
			pos, startColor,
			pos + rotation.GetColumn1()*infinity, endColor);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(
			pos + rotation.GetColumn1()*infinity, sphereRadius, sphereColor);
	}



	void DrawCrossHairGuideLines()
	{
		CPlayer* pLocalPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (!pLocalPlayer)
			return;

		IMovementController* pMC = pLocalPlayer->GetMovementController();
		if (!pMC)
			return;

		SAuxGeomRenderFlags currentFlags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = currentFlags;
		newFlags.SetDepthTestFlag(e_DepthTestOff);
		newFlags.SetMode2D3DFlag(e_Mode2D);
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

		int vpW = gEnv->pRenderer->GetOverlayWidth();
		int vpH = gEnv->pRenderer->GetOverlayHeight();

		const float desireAspectRatio = 16.0f / 9.0f;
		const float currentAspectRatio = vpW / float(vpH);
		const float widthMult = desireAspectRatio / currentAspectRatio;
		const float offsetX = (1.0f-widthMult) * 0.5f;

		SMovementState info;
		pMC->GetMovementState(info);
		Vec3 dir = info.fireDirection;

		DrawGoldenRatio(currentAspectRatio, offsetX, widthMult);
		DrawSights(currentAspectRatio, offsetX, widthMult);

		newFlags.SetMode2D3DFlag(e_Mode3D);
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);
		DrawWeaponFireDirection(pLocalPlayer);

		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(currentFlags);
	}



}




CProceduralWeaponAnimation::CProceduralWeaponAnimation()
	:	m_customOffset(IDENTITY)
	,	m_rightOffset(IDENTITY)
	,	m_leftOffset(IDENTITY)
	,	m_weaponOffsetInput(CWeaponOffsetInput::Get())
	,	m_debugInput(false)
{
}



void CProceduralWeaponAnimation::Update(float deltaTime)
{
	UpdateDebugState();
	ComputeOffsets(deltaTime);
	ResetCustomOffset();
}



void CProceduralWeaponAnimation::UpdateDebugState()
{
	if ((g_pGameCVars->g_debugWeaponOffset==2) != m_debugInput)
	{
		IActionMapManager* pAMMgr = gEnv->pGameFramework->GetIActionMapManager();

		if (g_pGameCVars->g_debugWeaponOffset == 2)
		{
			if (gEnv->pInput)
				gEnv->pInput->AddEventListener(m_weaponOffsetInput.get());
			if (pAMMgr)
				pAMMgr->Enable(false);
			m_weaponOffsetInput->SetRightDebugOffset(SWeaponOffset(m_rightOffset));
			m_weaponOffsetInput->SetLeftDebugOffset(SWeaponOffset(m_leftOffset));
		}
		else
		{
			if (gEnv->pInput)
				gEnv->pInput->RemoveEventListener(m_weaponOffsetInput.get());
			if (pAMMgr)
				pAMMgr->Enable(true);
		}
		m_debugInput = (g_pGameCVars->g_debugWeaponOffset == 2);
	}

	if (g_pGameCVars->g_debugWeaponOffset)
	{
		DrawCrossHairGuideLines();
	}
	if (m_debugInput)
	{
		m_weaponOffsetInput->Update();
	}
}



void CProceduralWeaponAnimation::ComputeOffsets(float deltaTime)
{
	if (!m_debugInput)
	{
		m_rightOffset =
			m_weaponZoomOffset.Compute(deltaTime) *
			m_lookOffset.Compute(deltaTime) *
			m_strafeOffset.Compute(deltaTime) *
			m_recoilOffset.Compute(deltaTime) *
			m_bumpOffset.Compute(deltaTime) *
			m_customOffset;

		m_leftOffset = m_weaponZoomOffset.GetLeftHandOffset(deltaTime);
	}
	else
	{
		m_rightOffset = ToQuatT(m_weaponOffsetInput->GetRightDebugOffset());
		m_leftOffset = ToQuatT(m_weaponOffsetInput->GetLeftDebugOffset());
	}
}




void CProceduralWeaponAnimation::ResetCustomOffset()
{
	m_customOffset = QuatT(IDENTITY);
}



void CProceduralWeaponAnimation::AddCustomOffset(const QuatT& offset)
{
	m_customOffset = m_customOffset * offset;
}

