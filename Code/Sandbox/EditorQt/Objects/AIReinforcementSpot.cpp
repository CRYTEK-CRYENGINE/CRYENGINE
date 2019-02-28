// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIReinforcementSpot.h"
#include "Gizmos/AxisHelper.h"
#include "IIconManager.h"
#include <CryAISystem/IAgent.h>
#include "Objects/DisplayContext.h"

REGISTER_CLASS_DESC(CAIReinforcementSpotClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAIReinforcementSpot, CEntityObject)

//////////////////////////////////////////////////////////////////////////
float CAIReinforcementSpot::m_helperScale = 1;

//////////////////////////////////////////////////////////////////////////
CAIReinforcementSpot::CAIReinforcementSpot()
{
	m_entityClass = "AIReinforcementSpot";
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
void CAIReinforcementSpot::Done()
{
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CAIReinforcementSpot::Init(CBaseObject* prev, const string& file)
{
	SetColor(RGB(255, 196, 0));
	bool res = __super::Init(prev, file);

	return res;
}

//////////////////////////////////////////////////////////////////////////
float CAIReinforcementSpot::GetRadius()
{
	return 0.5f * m_helperScale * gGizmoPreferences.helperScale;
}

//////////////////////////////////////////////////////////////////////////
void CAIReinforcementSpot::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	const Matrix34& wtm = GetWorldTM();

	Vec3 wp = wtm.GetTranslation();

	if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor(GetColor());

	Matrix34 tm(wtm);
	objRenderHelper.Render(tm);

	if (IsSelected())
	{
		dc.SetColor(GetColor());

		dc.PushMatrix(wtm);
		float r = GetRadius();
		dc.DrawWireBox(-Vec3(r, r, r), Vec3(r, r, r));

		float callRadius = 0.0f;
		float avoidRadius = 0.0f;

		if (GetIEntity())
		{
			IScriptTable* pTable = GetIEntity()->GetScriptTable();
			if (pTable)
			{
				SmartScriptTable props;
				if (pTable->GetValue("Properties", props))
				{
					props->GetValue("AvoidWhenTargetInRadius", avoidRadius);
					props->GetValue("radius", callRadius);
				}
			}
		}

		if (callRadius > 0.0f)
		{
			dc.SetColor(GetColor());
			dc.DrawWireSphere(Vec3(0, 0, 0), callRadius);
		}

		if (avoidRadius > 0.0f)
		{
			ColorB col;
			// Blend between red and the selected color
			col.r = (255 + GetRValue(GetColor())) / 2;
			col.g = (0 + GetGValue(GetColor())) / 2;
			col.b = (0 + GetBValue(GetColor())) / 2;
			col.a = 255;
			dc.SetColor(col);
			dc.DrawWireSphere(Vec3(0, 0, 0), avoidRadius);
		}

		dc.PopMatrix();
	}
	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CAIReinforcementSpot::HitTest(HitContext& hc)
{
	Vec3 origin = GetWorldPos();
	float radius = GetRadius();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross(w);
	float d = w.GetLength();

	if (d < radius + hc.distanceTolerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAIReinforcementSpot::GetLocalBounds(AABB& box)
{
	float r = GetRadius();
	box.min = -Vec3(r, r, r);
	box.max = Vec3(r, r, r);
}

