// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CharAttachHelper.h"
#include <CryAnimation/ICryAnimation.h>

REGISTER_CLASS_DESC(CCharacterAttachHelperObjectClassDesc);

IMPLEMENT_DYNCREATE(CCharacterAttachHelperObject, CEntityObject)

float CCharacterAttachHelperObject::m_charAttachHelperScale = 1.0f;

//////////////////////////////////////////////////////////////////////////
CCharacterAttachHelperObject::CCharacterAttachHelperObject()
{
	m_entityClass = "CharacterAttachHelper";
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
void CCharacterAttachHelperObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	__super::Display(objRenderHelper);

	dc.SetLineWidth(4.0f);
	float s = 1.0f * GetHelperScale();

	if (m_pEntity)
	{
		Matrix34 tm = m_pEntity->GetWorldTM();

		if (IsSelected())
			dc.SetSelectedColor();
		else
			dc.SetColor(GetColor());

		dc.SetLineWidth(4.0f);
		dc.DrawLine(tm.TransformPoint(Vec3(0, 0, 0)), tm.TransformPoint(Vec3(s, 0, 0)), ColorF(1, 0, 0), ColorF(1, 0, 0));
		dc.DrawLine(tm.TransformPoint(Vec3(0, 0, 0)), tm.TransformPoint(Vec3(0, s, 0)), ColorF(0, 1, 0), ColorF(0, 1, 0));
		dc.DrawLine(tm.TransformPoint(Vec3(0, 0, 0)), tm.TransformPoint(Vec3(0, 0, s)), ColorF(0, 0, 1), ColorF(0, 0, 1));
		dc.SetLineWidth(0);

		dc.SetColor(ColorB(0, 255, 0, 255));
		dc.DrawLine(tm.TransformPoint(Vec3(0, 0, 0)), GetWorldPos());
	}

	const Matrix34& tm = GetWorldTM();

	if (IsSelected())
		dc.SetSelectedColor();
	else
		dc.SetColor(GetColor());

	dc.SetLineWidth(4.0f);
	dc.DrawLine(tm.TransformPoint(Vec3(0, 0, 0)), tm.TransformPoint(Vec3(s, 0, 0)), ColorF(1, 0, 0), ColorF(1, 0, 0));
	dc.DrawLine(tm.TransformPoint(Vec3(0, 0, 0)), tm.TransformPoint(Vec3(0, s, 0)), ColorF(0, 1, 0), ColorF(0, 1, 0));
	dc.DrawLine(tm.TransformPoint(Vec3(0, 0, 0)), tm.TransformPoint(Vec3(0, 0, s)), ColorF(0, 0, 1), ColorF(0, 0, 1));
	dc.SetLineWidth(0);
}

//////////////////////////////////////////////////////////////////////////
void CCharacterAttachHelperObject::SetHelperScale(float scale)
{
	m_charAttachHelperScale = scale;
}

