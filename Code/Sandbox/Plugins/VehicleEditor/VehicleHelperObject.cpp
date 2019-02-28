// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleHelperObject.h"
#include "Viewport.h"

#include "VehicleData.h"
#include "VehiclePrototype.h"
#include "VehiclePart.h"

REGISTER_CLASS_DESC(CVehicleHelperClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehicleHelper, CBaseObject)

#define RADIUS                     0.05f

#define VEHICLE_HELPER_COLOR       (RGB(255, 255, 120))
#define VEHICLE_ASSET_HELPER_COLOR (RGB(120, 255, 120))

//////////////////////////////////////////////////////////////////////////
CVehicleHelper::CVehicleHelper()
{
	m_pVar = 0;
	m_fromGeometry = false;
	m_pVehicle = 0;

	ChangeColor(VEHICLE_HELPER_COLOR);

	InitOnTransformCallback(this);
}

CVehicleHelper::~CVehicleHelper()
{
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::SetVariable(IVariable* pVar)
{
	m_pVar = pVar;

	DisableUpdateObjectOnVarChange("position");
	DisableUpdateObjectOnVarChange("direction");

	if (IVariable* pVar = GetChildVar(m_pVar, "position"))
	{
		pVar->SetLimits(-10, 10);
	}

	EnableUpdateObjectOnVarChange("position");
	EnableUpdateObjectOnVarChange("direction");
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::UpdateVarFromObject()
{
	if (IsFromGeometry())
	{
		assert(!m_pVar);
		return;
	}

	assert(m_pVar);
	if (!m_pVar || !m_pVehicle)
		return;

	IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity();
	if (!pEnt)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[CVehicleHelper::UpdateVariable] pEnt is null, returning");
		return;
	}

	if (IVariable* pVar = GetChildVar(m_pVar, "name"))
		pVar->Set(GetName());

	if (IVariable* pVar = GetChildVar(m_pVar, "position"))
	{
		Vec3 pos = pEnt->GetWorldTM().GetInvertedFast().TransformPoint(GetWorldPos());
		pVar->Set(pos);
	}

	IVariable* pVar = GetChildVar(m_pVar, "direction");
	if (!pVar)
	{
		pVar = new CVariable<Vec3>;
		pVar->SetName("direction");
		m_pVar->AddVariable(pVar);
	}

	IVariable* pPartVar = GetChildVar(m_pVar, "part");
	if (!pPartVar)
	{
		pPartVar = new CVariable<string>;
		pPartVar->SetName("part");
		m_pVar->AddVariable(pPartVar);
	}

	Matrix33 relTM = Matrix33(pEnt->GetWorldTM().GetInvertedFast()) * Matrix33(GetWorldTM());
	Vec3 dir = relTM.TransformVector(FORWARD_DIRECTION);
	pVar->Set(dir);

}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::UpdateObjectFromVar()
{
	if (!m_pVar || !m_pVehicle)
		return;

	if (IVariable* pVar = GetChildVar(m_pVar, "position"))
	{
		Vec3 local(ZERO);
		pVar->Get(local);
		Matrix34 tm = GetWorldTM();
		tm.SetTranslation(m_pVehicle->GetCEntity()->GetIEntity()->GetWorldTM().TransformPoint(local));
		SetWorldTM(tm);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::Done()
{
	VeedLog("[CVehicleHelper:Done] <%s>", GetName());
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleHelper::Init(CBaseObject* prev, const string& file)
{
	SetColor(RGB(255, 255, 0));
	bool res = CBaseObject::Init(prev, file);

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	CBaseObject* pFromParent = pFromObject->GetParent();
	if (pFromParent)
	{
		CBaseObject* pChildParent = ctx.FindClone(pFromParent);
		if (pChildParent)
			pChildParent->AttachChild(this, false);
		else
		{
			//       // helper was cloned and attached to same parent
			//       if (pFromParent->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
			//         ((CVehiclePart*)pFromParent)->AddHelper( this, 0 );
			//       else
			//         pFromParent->AttachChild(this, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleHelper::HitTest(HitContext& hc)
{
	Vec3 origin = GetWorldPos();
	float radius = RADIUS;

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross(w);
	float d = w.GetLength();

	if (d < radius + hc.distanceTolerance)
	{
		Vec3 i0;
		if (Intersect::Ray_SphereFirst(Ray(hc.raySrc, hc.rayDir), Sphere(origin, radius), i0))
		{
			hc.dist = hc.raySrc.GetDistance(i0);
			return true;
		}
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	COLORREF color = GetColor();
	float radius = RADIUS;

	//dc.SetColor( color, 0.5f );
	//dc.DrawBall( GetPos(), radius );

	if (IsSelected())
	{
		dc.SetSelectedColor(0.6f);
	}

	AABB box;
	GetLocalBounds(box);
	dc.PushMatrix(GetWorldTM());

	if (!IsHighlighted())
	{
		dc.SetColor(color, 0.8f);
		dc.SetLineWidth(2);
		dc.DrawWireBox(box.min, box.max);
	}

	if (!IsSelected())
	{
		// direction vector
		Vec3 dirEndPos(0, 4 * box.max.y, 0);
		dc.DrawArrow(Vec3(0, box.max.y, 0), dirEndPos, 0.15f);
	}

	dc.PopMatrix();

	// draw label
	if (dc.flags & DISPLAY_HIDENAMES)
	{
		Vec3 p(GetWorldPos());
		DrawLabel(dc, p, RGB(255, 255, 255));
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::GetBoundBox(AABB& box)
{
	// Transform local bounding box into world space.
	GetLocalBounds(box);
	box.SetTransformedAABB(GetWorldTM(), box);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::GetLocalBounds(AABB& box)
{
	// return local bounds
	float r = RADIUS;
	box.min = -Vec3(r, r, r);
	box.max = Vec3(r, r, r);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::GetBoundSphere(Vec3& pos, float& radius)
{
	pos = GetPos();
	radius = RADIUS;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::UpdateScale(float scale)
{
	if (IVariable* pPos = GetChildVar(m_pVar, "position"))
	{
		Vec3 pos;
		pPos->Get(pos);
		pPos->Set(pos *= scale);
		UpdateObjectFromVar();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::SetIsFromGeometry(bool b)
{
	m_fromGeometry = b;
	if (m_fromGeometry)
	{
		ChangeColor(VEHICLE_ASSET_HELPER_COLOR);
		SetFrozen(true);
	}
	else
	{
		ChangeColor(VEHICLE_HELPER_COLOR);
		SetFrozen(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleHelper::OnTransform()
{
	UpdateVarFromObject();
}

