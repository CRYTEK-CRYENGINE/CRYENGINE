// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CloudObject.h"
#include "Viewport.h"
#include "Objects/InspectorWidgetCreator.h"

REGISTER_CLASS_DESC(CCloudObjectClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CCloudObject, CBaseObject)

//////////////////////////////////////////////////////////////////////////
CCloudObject::CCloudObject()
{
	m_bNotSharedGeom = false;
	m_bIgnoreNodeUpdate = false;

	m_width = 1;
	m_height = 1;
	m_length = 1;
	mv_spriteRow = -1;
	m_angleVariations = 0.0f;

	mv_sizeSprites = 0;
	mv_randomSizeValue = 0;

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_spriteRow, "SpritesRow", "Sprites Row");
	mv_spriteRow.SetLimits(-1, 64);
	m_pVarObject->AddVariable(m_width, "Width", functor(*this, &CCloudObject::OnSizeChange));
	m_width.SetLimits(0, 99999);
	m_pVarObject->AddVariable(m_height, "Height", functor(*this, &CCloudObject::OnSizeChange));
	m_height.SetLimits(0, 99999);
	m_pVarObject->AddVariable(m_length, "Length", functor(*this, &CCloudObject::OnSizeChange));
	m_length.SetLimits(0, 99999);
	m_pVarObject->AddVariable(m_angleVariations, "AngleVariations", "Angle Variations", functor(*this, &CCloudObject::OnSizeChange), IVariable::DT_ANGLE);
	m_angleVariations.SetLimits(0, 99999);

	m_pVarObject->AddVariable(mv_sizeSprites, "SizeofSprites", "Size of Sprites", functor(*this, &CCloudObject::OnSizeChange));
	mv_sizeSprites.SetLimits(0, 99999);
	m_pVarObject->AddVariable(mv_randomSizeValue, "SizeVariation", "Size Variation", functor(*this, &CCloudObject::OnSizeChange));
	mv_randomSizeValue.SetLimits(0, 99999);

	SetColor(RGB(127, 127, 255));
}

//////////////////////////////////////////////////////////////////////////
void CCloudObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CCloudObject>("Cloud", [](CCloudObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_spriteRow, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_width, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_height, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_length, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_angleVariations, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_sizeSprites, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_randomSizeValue, ar);
	});
}

//////////////////////////////////////////////////////////////////////////
void CCloudObject::OnSizeChange(IVariable* pVar)
{
	InvalidateTM(eObjectUpdateFlags_PositionChanged | eObjectUpdateFlags_RotationChanged | eObjectUpdateFlags_ScaleChanged);
}

//////////////////////////////////////////////////////////////////////////
void CCloudObject::GetLocalBounds(AABB& box)
{
	Vec3 diag(m_width, m_length, m_height);
	box = AABB(-diag / 2, diag / 2);
}

//////////////////////////////////////////////////////////////////////////
void CCloudObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	const Matrix34& wtm = GetWorldTM();
	Vec3 wp = wtm.GetTranslation();

	if (IsSelected())
		dc.SetSelectedColor();
	else if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor(GetColor());

	dc.PushMatrix(wtm);
	AABB box;
	GetLocalBounds(box);
	dc.DrawWireBox(box.min, box.max);
	dc.PopMatrix();

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CCloudObject::HitTest(HitContext& hc)
{
	Vec3 origin = GetWorldPos();

	AABB box;
	GetBoundBox(box);

	float radius = sqrt((box.max.x - box.min.x) * (box.max.x - box.min.x) + (box.max.y - box.min.y) * (box.max.y - box.min.y) + (box.max.z - box.min.z) * (box.max.z - box.min.z)) / 2;

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross(w);
	float d = w.GetLength();

	if (d < radius + hc.distanceTolerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		hc.object = this;
		return true;
	}
	return false;
}

