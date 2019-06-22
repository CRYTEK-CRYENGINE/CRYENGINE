// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehiclePart.h"

#include "Viewport.h"
#include "Util/MFCUtil.h"
#include "VehiclePrototype.h"
#include "VehicleHelperObject.h"
#include "VehicleSeat.h"
#include "VehicleData.h"
#include "VehicleEditorDialog.h"
#include "Util/Math.h"

#include <Cry3DEngine/I3DEngine.h>

REGISTER_CLASS_DESC(CVehiclePartClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehiclePart, CBaseObject)

#define RADIUS 0.05f

//////////////////////////////////////////////////////////////////////////
CVehiclePart::CVehiclePart()
	: m_pVehicle(nullptr)
	, m_pParent(nullptr)
	, m_isMain(false)
	, m_pYawSpeed(nullptr)
	, m_pYawLimits(nullptr)
	, m_pPitchSpeed(nullptr)
	, m_pPitchLimits(nullptr)
	, m_pHelper(nullptr)
	, m_pPartClass(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePart::Init(CBaseObject* prev, const string& file)
{
	//SetColor( RGB(0,0,255) );
	bool res = CBaseObject::Init(prev, file);

	return res;
}

void CVehiclePart::DrawRotationLimits(SDisplayContext& dc, IVariable* pSpeed, IVariable* pLimits, IVariable* pHelper, CLevelEditorSharedState::Axis axis)
{
	if (pSpeed)
	{
		float speed = 0;
		pSpeed->Get(speed);

		if (speed > 0)
		{
			// get limits
			float min = 0, max = 0;
			if (pLimits && pLimits->GetNumVariables() > 1)
			{
				pLimits->GetVariable(0)->Get(min);
				pLimits->GetVariable(1)->Get(max);

				if (min < -180)
					min = 0;
				if (max > 180)
					max = 0;
			}

			// get draw position: either vehicle pos or helper pos, if available
			Vec3 sourcePos = GetWorldPos();
			if (pHelper)
			{
				string name;
				pHelper->Get(name);

				if (CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(name))
				{
					if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
						sourcePos = pObj->GetWorldPos();
				}
			}

			// draw limits
			if (min || max)
			{
				const static Vec3 line(0, 2, 0);
				const static float step = DEG2RAD(10);
				double angleMin = 0, angleMax = 0, angle = 0;
				Matrix34 matMin, matMax;
				Vec3 p0, p1;
				const float radius = 0.85 * line.y;
				float upper = 0;
				int i = 0;

				ColorF color(0, 1, 0, 1);
				if (min > max)
					color = ColorF(1, 0, 0, 1);

				switch (axis)
				{
				case CLevelEditorSharedState::Axis::X:
					angleMin = DEG2RAD(max > 0 ? max : 360 + max); // angles for x-rotation
					angleMax = DEG2RAD(min < 0 ? 360 + min : min);

					matMin = m_pVehicle->GetWorldTM().CreateRotationX(angleMin);
					matMax = m_pVehicle->GetWorldTM().CreateRotationX(angleMax);

					p0.y = sourcePos.y + radius * cos(angleMax - step);
					p0.z = sourcePos.z + radius * sin(angleMax - step);
					p0.x = p1.x = sourcePos.x;

					break;

				case CLevelEditorSharedState::Axis::Z:
					angleMin = DEG2RAD(min < 0 ? -min : 360 - min); // angles for z-rotation
					angleMax = DEG2RAD(max > 0 ? 360 - max : -max);

					matMin = m_pVehicle->GetWorldTM().CreateRotationZ(angleMin);
					matMax = m_pVehicle->GetWorldTM().CreateRotationZ(angleMax);

					p0.x = sourcePos.x + radius * -sin(angleMax - step);
					p0.y = sourcePos.y + radius * cos(angleMax - step);
					p0.z = p1.z = sourcePos.z;

					break;
				}

				upper = angleMin + step;
				if (upper >= gf_PI2)
					upper = upper - gf_PI2;

				angle = angleMax;

				while (++i < gf_PI2 / step)
				{
					switch (axis)
					{
					case CLevelEditorSharedState::Axis::X:
						p1.y = sourcePos.y + radius * cos(angle);
						p1.z = sourcePos.z + radius * sin(angle);
						break;
					case CLevelEditorSharedState::Axis::Z:
						p1.x = sourcePos.x + radius * -sin(angle);
						p1.y = sourcePos.y + radius * cos(angle);
						break;
					}
					dc.DrawLine(p0, p1, color, color);
					p0 = p1;

					if (angle > upper - 0.001 && angle < upper + 0.001)
						break;

					if ((angle >= angleMin || (angle < step && angleMin + step > g_PI2)) && (angle <= upper || (angleMin + step > g_PI2 && angle + step > g_PI2)))
						angle = upper;
					else
					{
						angle += step;
						if (angle > g_PI2)
							angle = angle - g_PI2;
					}
				}

				Vec3 destPos = sourcePos + matMin * line;
				dc.DrawLine(sourcePos, destPos, color, color);

				destPos = sourcePos + matMax * line;
				dc.DrawLine(sourcePos, destPos, color, color);

			}

		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::Display(CObjectRenderHelper& objRenderHelper)
{
	// only draw if selected
	if (!IsSelected())
		return;

	SDisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	COLORREF wireColor, solidColor;
	float alpha = 0.4f;
	wireColor = dc.GetSelectedColor();
	solidColor = CMFCUtils::ColorBToColorRef(GetColor());

	// mass box rendering
	if (GetPartClass() == PARTCLASS_MASS)
	{
		dc.PushMatrix(GetWorldTM());
		AABB box;
		GetLocalBounds(box);
		dc.SetColor(solidColor, alpha);
		dc.DepthWriteOff();
		dc.DrawSolidBox(box.min, box.max);
		dc.DepthWriteOn();

		dc.SetColor(wireColor, 1);
		dc.SetLineWidth(3.0f);
		dc.DrawWireBox(box.min, box.max);
		dc.SetLineWidth(0);
		dc.PopMatrix();
	}
	else if (GetPartClass() == PARTCLASS_WHEEL)
	{
		// use statobj here
		IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity();

		for (int i = 0; i < pEnt->GetSlotCount(); ++i)
		{
			IStatObj* pStatObj = pEnt->GetStatObj(i);

			if (pStatObj && 0 == strcmp(pStatObj->GetGeoName(), GetName()))
			{
				//pEnt->GetSlotWorldTM(i);
				AABB box = pStatObj->GetAABB();

				dc.SetColor(wireColor, 0.85f);

				dc.PushMatrix(pEnt->GetSlotWorldTM(i));
				dc.DrawWireBox(box.min, box.max);
				dc.PopMatrix();
			}
		}
	}
	else
	{
		// draw rotation limits, if available
		DrawRotationLimits(dc, m_pYawSpeed, m_pYawLimits, m_pHelper, CLevelEditorSharedState::Axis::Z);
		DrawRotationLimits(dc, m_pPitchSpeed, m_pPitchLimits, m_pHelper, CLevelEditorSharedState::Axis::X);
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePart::HitTest(HitContext& hc)
{
	if (!IsSelected())
		return false;

	// select on edges
	Vec3 p;
	Matrix34 invertWTM = GetWorldTM();
	Vec3 worldPos = invertWTM.GetTranslation();
	invertWTM.Invert();

	Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
	Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir).GetNormalized();

	float epsilonDist = max(.1f, hc.view->GetScreenScaleFactor(worldPos) * 0.01f);
	epsilonDist *= max(0.0001f, min(invertWTM.GetColumn0().GetLength(), min(invertWTM.GetColumn1().GetLength(), invertWTM.GetColumn2().GetLength())));
	float hitDist;

	float tr = hc.distanceTolerance / 2 + 1;
	AABB box, mbox;
	GetLocalBounds(mbox);
	box.min = mbox.min - Vec3(tr + epsilonDist, tr + epsilonDist, tr + epsilonDist);
	box.max = mbox.max + Vec3(tr + epsilonDist, tr + epsilonDist, tr + epsilonDist);
	if (Intersect::Ray_AABB(xformedRaySrc, xformedRayDir, box, p))
	{
		if (Intersect::Ray_AABBEdge(xformedRaySrc, xformedRayDir, mbox, epsilonDist, hitDist, p))
		{
			hc.dist = xformedRaySrc.GetDistance(p);
			hc.object = this;
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::GetBoundBox(AABB& box)
{
	// Transform local bounding box into world space.
	GetLocalBounds(box);
	box.SetTransformedAABB(GetWorldTM(), box);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::GetLocalBounds(AABB& box)
{
	// if box class, return actual size
	// todo: geometries
	IVariable* pClass, * pSubVar, * pSize;
	pClass = GetChildVar(m_pVar, "class");

	if (pClass)
	{
		string classname;
		pClass->Get(classname);
		if (classname == PARTCLASS_MASS)
		{
			if (pSubVar = GetChildVar(m_pVar, PARTCLASS_MASS))
			{
				if (pSize = GetChildVar(pSubVar, "size"))
				{
					Vec3 size;
					pSize->Get(size);
					box.min = -size;
					box.max = size;
					return;
				}
			}
		}
	}

	// return vehicle bbox
	if (m_pVehicle)
		m_pVehicle->GetLocalBounds(box);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::UpdateObjectFromVar()
{
	if (!m_pVar)
	{
		// create default var here
		m_pVar = CreateDefaultChildOf("Parts");

		// remove default helper, don't want it here
		if (IVariable* pHelpers = GetChildVar(m_pVar, "Helpers"))
			pHelpers->DeleteAllVariables();
	}

	// check if part has a name, otherwise set to new
	string sName("NewPart");
	if (IVariable* pName = GetChildVar(m_pVar, "name"))
	{
		pName->Get(sName);
	}
	SetName(sName);

	// place at vehicle origin
	if (IVariable* pPos = GetChildVar(m_pVar, "position"))
	{
		pPos->RemoveOnSetCallback(functor(*this, &CVehiclePart::OnSetPos));
		pPos->AddOnSetCallback(functor(*this, &CVehiclePart::OnSetPos));
		OnSetPos(pPos);
	}

	// store pointers to variables that are needed for displaying etc.
	m_pYawSpeed = GetChildVar(m_pVar, "yawSpeed");
	m_pYawLimits = GetChildVar(m_pVar, "yawLimits");
	m_pPitchSpeed = GetChildVar(m_pVar, "pitchSpeed");
	m_pPitchLimits = GetChildVar(m_pVar, "pitchLimits");

	m_pHelper = GetChildVar(m_pVar, "helper");
	if (!m_pHelper && GetParent())
	{
		// if not found, lookup in parent
		if (IVeedObject* pVO = IVeedObject::GetVeedObject(GetParent()))
		{
			m_pHelper = GetChildVar(pVO->GetVariable(), "helper");
		}
	}

	if (m_pPartClass != NULL)
	{
		m_pPartClass->RemoveOnSetCallback(functor(*this, &CVehiclePart::OnSetClass));
	}
	m_pPartClass = GetChildVar(m_pVar, "class");
	if (m_pPartClass != NULL)
	{
		m_pPartClass->AddOnSetCallback(functor(*this, &CVehiclePart::OnSetClass));
		OnSetClass(m_pPartClass);
	}

	SetModified(false, false);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnSetPos(IVariable* pVar)
{
	if (!m_pVehicle)
		return;

	Vec3 pos(ZERO);
	pVar->Get(pos);
	if (GetParent())
		SetPos(pos);
	else if (IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity())
		SetPos(pEnt->GetWorldTM().TransformPoint(pos));
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnSetClass(IVariable* pVar)
{
	if (GetPartClass() == PARTCLASS_MASS)
	{
		SetHidden(false);
	}

	bool isWheel = (GetPartClass() == PARTCLASS_WHEEL);
	SetFrozen(isWheel);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::RemoveChild(CBaseObject* node)
{
	CBaseObject::RemoveChild(node);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::SetVariable(IVariable* pVar)
{
	m_pVar = pVar;
	if (m_pVar != NULL)
	{
		UpdateObjectFromVar();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::AddPart(CVehiclePart* pPart)
{
	AttachChild(pPart, false);

	IVariable* pParts = GetOrCreateChildVar(m_pVar, "Parts");
	if (pParts)
		pParts->AddVariable(pPart->GetVariable());
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::Done()
{
	VeedLog("[CVehiclePart:Done] <%s>", GetName());

	// remove callbacks
	if (IVariable* pPos = GetChildVar(m_pVar, "position"))
		pPos->RemoveOnSetCallback(functor(*this, &CVehiclePart::OnSetPos));

	int nChildCount = GetChildCount();
	VeedLog("[CVehiclePart]: deleting %i children..", nChildCount);

	for (int i = nChildCount - 1; i >= 0; --i)
	{
		CBaseObject* pChild = GetChild(i);
		VeedLog("[CVehiclePart]: deleting %s", pChild->GetName());
		GetIEditor()->DeleteObject(GetChild(i));
	}

	// here Listeners are notified of deletion
	// ie. here parents must erase child's variable ptr, not before
	CBaseObject::Done();

}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::AttachChild(CBaseObject* child, bool bKeepPos, bool bInvalidateTM)
{
	child->signalChanged.Connect(this, &CVehiclePart::OnObjectEvent);

	CBaseObject::AttachChild(child, bKeepPos, bInvalidateTM);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnObjectEvent(const CBaseObject* pObject, const CObjectEvent& event)
{
	if (event.m_type == OBJECT_ON_DELETE)
	{
		VeedLog("[CVehiclePart]: ON_DELETE for %s", pObject->GetName());
		// delete variable
		if (IVeedObject* pVO = IVeedObject::GetVeedObject(const_cast<CBaseObject*>(pObject)))
		{
			if (pVO->DeleteVar())
			{
				if (m_pVar && !m_pVar->DeleteVariable(pVO->GetVariable(), true))
				{
					// if not found, delete in top-level, because not all children attached
					// to the part have to keep their variable beneath it
					m_pVehicle->GetVariable()->DeleteVariable(pVO->GetVariable(), true);
				}
				pVO->SetVariable(0);
				VeedLog("[CVehiclePart] deleting var for %s", pObject->GetName());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::UpdateVarFromObject()
{
	if (!m_pVar)
		return;

	IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity();
	if (!pEnt)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[CVehiclePart::UpdateVariable] pEnt is null, returning");
		return;
	}

	IVariable* pVar = GetChildVar(m_pVar, "name");
	if (!pVar)
		CryLog("ChildVar <name> not found in Part!");
	else
		pVar->Set(GetName());

	// update pos
	if (IVariable* pVar = GetChildVar(m_pVar, "position"))
	{
		Vec3 pos = pEnt->GetWorldTM().GetInvertedFast().TransformPoint(GetWorldPos());
		pVar->Set(pos);
	}

	// update size
	if (GetPartClass() == PARTCLASS_MASS)
	{
		if (IVariable* pSize = GetChildVar(m_pVar, "size", true))
		{
			AABB box;
			GetLocalBounds(box);
			pSize->Set(box.max.CompMul(GetScale()));
			SetScale(Vec3(1, 1, 1));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CVehiclePart::GetIconIndex()
{
	if (GetPartClass() == PARTCLASS_WHEEL)
		return VEED_WHEEL_ICON;
	else
		return VEED_PART_ICON;
}

//////////////////////////////////////////////////////////////////////////
string CVehiclePart::GetPartClass()
{
	string name("");

	if (!m_pPartClass)
		m_pPartClass = GetChildVar(m_pVar, "class");

	if (m_pPartClass)
		m_pPartClass->Get(name);

	return name;
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePart::IsLeaf()
{
	string partClass = GetPartClass();

	if (partClass == PARTCLASS_WHEEL
	    || partClass == PARTCLASS_LIGHT
	    || partClass == PARTCLASS_TREAD
	    || partClass == PARTCLASS_MASS)
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::UpdateScale(float scale)
{
	Vec3 vec;

	if (IVariable* pVar = GetChildVar(m_pVar, "position"))
	{
		pVar->Get(vec);
		pVar->Set(vec *= scale);
	}
	if (IVariable* pVar = GetChildVar(m_pVar, "positionOffset"))
	{
		pVar->Get(vec);
		pVar->Set(vec *= scale);
	}

	if (GetPartClass() == PARTCLASS_MASS)
	{
		IVariable* pSub = GetChildVar(m_pVar, "MassBox");
		if (IVariable* pVar = GetChildVar(pSub, "size"))
		{
			pVar->Get(vec);
			pVar->Set(vec *= scale);
		}
	}

	UpdateObjectFromVar();
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePart::OnTreeSelection()
{
}
