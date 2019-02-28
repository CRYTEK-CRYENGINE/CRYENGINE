// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleComp.h"

#include "VehiclePrototype.h"
#include "VehicleData.h"
#include "Viewport.h"
#include "Util/Math.h"

#include <IVehicleSystem.h>

REGISTER_CLASS_DESC(CVehicleComponentClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehicleComponent, CBaseObject)

//////////////////////////////////////////////////////////////////////////
CVehicleComponent::CVehicleComponent()
	: m_pVehicle(NULL)
	, m_pPosition(NULL)
	, m_pSize(NULL)
	, m_pUseBoundsFromParts(NULL)
{
	m_pVar = NULL;
	InitOnTransformCallback(this);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::Display(CObjectRenderHelper& objRenderHelper)
{
	if (!IsSelected())
	{
		return;
	}

	float alpha = 0.4f;
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	COLORREF wireColor = dc.GetSelectedColor();
	COLORREF solidColor = GetColor();

	dc.PushMatrix(GetWorldTM());

	AABB box;
	GetLocalBounds(box);

	dc.SetColor(solidColor, alpha);
	dc.DrawSolidBox(box.min, box.max);

	dc.SetColor(wireColor, 1);
	dc.SetLineWidth(3.0f);
	dc.DrawWireBox(box.min, box.max);
	dc.SetLineWidth(0);

	dc.PopMatrix();

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleComponent::HitTest(HitContext& hc)
{
	if (!IsSelected())
	{
		return false;
	}

	// Select on edges.
	Vec3 p;
	Matrix34 invertWTM = GetWorldTM();
	Vec3 worldPos = invertWTM.GetTranslation();
	invertWTM.Invert();

	Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
	Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir).GetNormalized();

	AABB mbox;
	GetLocalBounds(mbox);

	float epsilonDist = max(0.1f, hc.view->GetScreenScaleFactor(worldPos) * 0.01f);
	epsilonDist *= max(0.0001f, min(invertWTM.GetColumn0().GetLength(), min(invertWTM.GetColumn1().GetLength(), invertWTM.GetColumn2().GetLength())));
	float tr = (hc.distanceTolerance / 2) + 1;
	float offset = tr + epsilonDist;

	AABB box;
	box.min = mbox.min - Vec3(offset);
	box.max = mbox.max + Vec3(offset);

	if (Intersect::Ray_AABB(xformedRaySrc, xformedRayDir, box, p))
	{
		float hitDist;
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
void CVehicleComponent::GetBoundBox(AABB& box)
{
	// Transform local bounding box into world space.
	GetLocalBounds(box);
	box.SetTransformedAABB(GetWorldTM(), box);
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleComponent::HasZeroSize() const
{
	assert(m_pSize != NULL);
	if (m_pSize == NULL)
	{
		return false;
	}

	Vec3 size;
	m_pSize->Get(size);
	bool hasZeroSize = size.IsZero(0.001f);

	return hasZeroSize;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::GetLocalBounds(AABB& box)
{
	bool useBoundsFromParts = GetUseBoundsFromParts();
	if (useBoundsFromParts)
	{
		box.min = Vec3(ZERO);
		box.max = Vec3(ZERO);

		// This is not entirely accurate right now, as it's using the loaded version of
		// the vehicle instead of the one being currently constructed to get the bounds.
		assert(gEnv->pGameFramework);

		IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
		assert(pVehicleSystem != NULL);

		EntityId entityId = m_pVehicle->GetCEntity()->GetEntityId();
		IVehicle* pVehicle = pVehicleSystem->GetVehicle(entityId);
		if (pVehicle == NULL)
		{
			return;
		}

		IVehicleComponent* pComponent = pVehicle->GetComponent(GetName());
		if (pComponent == NULL)
		{
			return;
		}

		box = pComponent->GetBounds();
		return;
	}

	bool hasZeroSize = HasZeroSize();
	if (hasZeroSize && (m_pVehicle != NULL))
	{
		m_pVehicle->GetLocalBounds(box);

		// Relying on the fact that zero scale for the object is invalid and will not be set to match zero size in var...
		Matrix34 invLocal = GetLocalTM();
		invLocal.Invert();
		box.max = invLocal.TransformPoint(box.max);
		box.min = invLocal.TransformPoint(box.min);
		return;
	}

	box.min = Vec3(-0.5f);
	box.max = Vec3(0.5f);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateObjectNameFromVar()
{
	IVariable* pName = GetChildVar(m_pVar, "name");
	if (pName == NULL)
	{
		SetName("NewComponent");
		return;
	}

	string name;
	pName->Get(name);
	SetName(name);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateObjectBoundsFromVar()
{
	if (m_pPosition == NULL)
	{
		return;
	}

	if (m_pSize == NULL)
	{
		return;
	}

	Vec3 position;
	Vec3 scale;

	m_pPosition->Get(position);
	m_pSize->Get(scale);

	const Quat& rotation = GetRotation();
	SetLocalTM(position, rotation, scale);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateObjectFromVar()
{
	if (m_pVar == NULL)
	{
		return;
	}

	UpdateObjectNameFromVar();
	UpdateObjectBoundsFromVar();

	SetModified(false, false);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::SetVariable(IVariable* pVar)
{
	m_pVar = pVar;

	DisableUpdateObjectOnVarChange("position");
	DisableUpdateObjectOnVarChange("size");
	DisableUpdateObjectOnVarChange("name");

	if (m_pVar == NULL)
	{
		m_pVar = CreateDefaultChildOf("Components");
	}

	assert(m_pVar != NULL);
	if (m_pVar == NULL)
	{
		// This means there is probably something wrong in veed_defaults.xml
		return;
	}

	m_pUseBoundsFromParts = GetChildVar(m_pVar, "useBoundsFromParts");

	m_pPosition = GetChildVar(m_pVar, "position");
	m_pSize = GetChildVar(m_pVar, "size");

	bool hasPositionVar = (m_pPosition != NULL);
	bool hasSizeVar = (m_pSize != NULL);
	bool newBoundsStructure = (hasPositionVar && hasSizeVar);

	if (m_pPosition == NULL)
	{
		_smart_ptr<CVariable<Vec3>> pPosition = new CVariable<Vec3>;
		pPosition->SetName("position");
		m_pVar->AddVariable(pPosition);

		m_pPosition = GetChildVar(m_pVar, "position");
	}
	assert(m_pPosition != NULL);

	if (m_pSize == NULL)
	{
		_smart_ptr<CVariable<Vec3>> pSize = new CVariable<Vec3>;
		pSize->SetName("size");
		m_pVar->AddVariable(pSize);

		m_pSize = GetChildVar(m_pVar, "size");
	}
	assert(m_pSize != NULL);

	m_pPosition->SetLimits(-100, 100);
	m_pSize->SetLimits(-10, 10);

	// Extract position and scale from bounds if necessary and remove the vars.
	IVariable* pMaxBound = GetChildVar(m_pVar, "maxBound");
	IVariable* pMinBound = GetChildVar(m_pVar, "minBound");
	if (!newBoundsStructure)
	{
		if (pMaxBound != NULL && pMinBound != NULL)
		{
			Vec3 maxBound;
			pMaxBound->Get(maxBound);

			Vec3 minBound;
			pMinBound->Get(minBound);

			Vec3 size = maxBound - minBound;
			size = size.abs();

			minBound.CheckMin(maxBound);
			Vec3 position = minBound + (size * 0.5f);

			m_pPosition->Set(position);
			m_pSize->Set(size);
		}
	}
	m_pVar->DeleteVariable(pMaxBound);
	m_pVar->DeleteVariable(pMinBound);

	UpdateObjectFromVar();

	EnableUpdateObjectOnVarChange("position");
	EnableUpdateObjectOnVarChange("size");
	EnableUpdateObjectOnVarChange("name");
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::CreateVariable()
{
	SetVariable(NULL);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::ResetPosition()
{
	if (m_pPosition == NULL)
	{
		return;
	}

	m_pPosition->Set(Vec3(0, 0, 0));
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::Done()
{
	VeedLog("[CVehicleComponent:Done] <%s>", GetName());

	// here Listeners are notified of deletion
	// ie. here parents must erase child's variable ptr, not before
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::OnTransform()
{
	Quat identity;
	identity.SetIdentity();
	SetRotation(identity);

	UpdateVarFromObject();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	CBaseObject* pFromParent = pFromObject->GetParent();
	if (pFromParent)
	{
		CBaseObject* pChildParent = ctx.FindClone(pFromParent);
		if (pChildParent)
		{
			pChildParent->AttachChild(this, false);
		}
		else
		{
			// component was cloned and attached to same parent
			if (pFromParent->IsKindOf(RUNTIME_CLASS(CVehiclePrototype)))
			{
				CVehiclePrototype* pFromParentVehicle = static_cast<CVehiclePrototype*>(pFromParent);
				pFromParentVehicle->AddComponent(this);
			}
			else
			{
				pFromParent->AttachChild(this, false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateVarNameFromObject()
{
	IVariable* pName = GetChildVar(m_pVar, "name");
	if (pName == NULL)
	{
		CryLog("ChildVar <name> not found in Component!");
		return;
	}

	pName->Set(GetName());
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleComponent::GetUseBoundsFromParts() const
{
	if (m_pUseBoundsFromParts == NULL)
	{
		return false;
	}

	bool useBoundsFromParts;
	m_pUseBoundsFromParts->Get(useBoundsFromParts);

	return useBoundsFromParts;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateVarBoundsFromObject()
{
	if (m_pVehicle == NULL)
	{
		return;
	}

	if (m_pPosition == NULL)
	{
		return;
	}

	if (m_pSize == NULL)
	{
		return;
	}

	bool useBoundsFromParts = GetUseBoundsFromParts();
	if (useBoundsFromParts)
	{
		return;
	}

	Matrix34 worldTMInv = m_pVehicle->GetWorldTM().GetInverted();
	Vec3 worldPosition = GetWorldTM().GetTranslation();

	Vec3 center = worldTMInv.TransformPoint(worldPosition);
	Vec3 scale = GetScale();
	scale = scale.abs();

	m_pPosition->Set(center);
	m_pSize->Set(scale);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateVarFromObject()
{
	if (m_pVar == NULL)
	{
		return;
	}

	UpdateVarNameFromObject();
	UpdateVarBoundsFromObject();
}

//////////////////////////////////////////////////////////////////////////
int CVehicleComponent::GetIconIndex()
{
	return VEED_COMP_ICON;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleComponent::UpdateScale(float scale)
{
	if (m_pSize == NULL)
	{
		return;
	}

	Vec3 vecScale(scale);
	m_pSize->Set(vecScale);

	UpdateObjectFromVar();
}

