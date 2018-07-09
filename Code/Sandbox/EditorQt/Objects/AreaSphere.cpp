// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AreaSphere.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include <CryEntitySystem/IEntitySystem.h>
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"

REGISTER_CLASS_DESC(CAreaSphereClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAreaSphere, CEntityObject)

//////////////////////////////////////////////////////////////////////////
CAreaSphere::CAreaSphere()
	: m_innerFadeDistance(0.0f)
{
	m_areaId = -1;
	m_edgeWidth = 0;
	m_radius = 3;
	mv_groupId = 0;
	mv_priority = 0;
	m_entityClass = "AreaSphere";

	SetColor(RGB(0, 0, 255));
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(m_innerFadeDistance, "InnerFadeDistance", functor(*this, &CAreaSphere::OnAreaChange));
	m_pVarObject->AddVariable(m_areaId, "AreaId", functor(*this, &CAreaSphere::OnAreaChange));
	m_pVarObject->AddVariable(m_edgeWidth, "FadeInZone", functor(*this, &CAreaSphere::OnSizeChange));
	m_pVarObject->AddVariable(m_radius, "Radius", functor(*this, &CAreaSphere::OnSizeChange));
	m_pVarObject->AddVariable(mv_groupId, "GroupId", functor(*this, &CAreaSphere::OnAreaChange));
	m_pVarObject->AddVariable(mv_priority, "Priority", functor(*this, &CAreaSphere::OnAreaChange));
	m_pVarObject->AddVariable(mv_filled, "Filled", functor(*this, &CAreaSphere::OnAreaChange));
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAreaObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAreaSphere>("Area Sphere", [](CAreaSphere* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->m_innerFadeDistance, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_areaId, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_edgeWidth, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_radius, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_groupId, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_priority, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_filled, ar);
	});
}

//////////////////////////////////////////////////////////////////////////
bool CAreaSphere::CreateGameObject()
{
	bool const bSuccess = __super::CreateGameObject();

	if (bSuccess)
	{
		if (m_pEntity != nullptr)
		{
			IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

			if (pArea)
			{
				pArea->SetSphere(Vec3(ZERO), m_radius);
			}
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::GetLocalBounds(AABB& box)
{
	box.min = Vec3(-m_radius, -m_radius, -m_radius);
	box.max = Vec3(m_radius, m_radius, m_radius);
}

//////////////////////////////////////////////////////////////////////////
bool CAreaSphere::HitTest(HitContext& hc)
{
	Vec3 origin = GetWorldPos();
	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross(w);
	float d = w.GetLength();

	Matrix34 invertWTM = GetWorldTM();
	Vec3 worldPos = invertWTM.GetTranslation();
	float epsilonDist = hc.view->GetScreenScaleFactor(worldPos) * 0.01f;
	if ((d < m_radius + epsilonDist) &&
	    (d > m_radius - epsilonDist))
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::Reload(bool bReloadScript /* = false */)
{
	__super::Reload(bReloadScript);

	// During reloading the entity+proxies get completely removed.
	// Hence we need to completely recreate the proxy and update all data to it.
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::OnAreaChange(IVariable* pVar)
{
	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetID(m_areaId);
			pArea->SetGroup(mv_groupId);
			pArea->SetPriority(mv_priority);
			pArea->SetInnerFadeDistance(m_innerFadeDistance);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::OnSizeChange(IVariable* pVar)
{
	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetSphere(Vec3(ZERO), m_radius);
			pArea->SetProximity(m_edgeWidth);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	if (!gViewportDebugPreferences.showAreaObjectHelper)
		return;

	COLORREF wireColor, solidColor;
	float wireOffset = 0;
	float alpha = 0.3f;
	if (IsSelected())
	{
		wireColor = dc.GetSelectedColor();
		solidColor = GetColor();
		wireOffset = -0.1f;
	}
	else
	{
		wireColor = GetColor();
		solidColor = GetColor();
	}

	const Matrix34& tm = GetWorldTM();
	Vec3 pos = GetWorldPos();

	bool bFrozen = IsFrozen();

	if (bFrozen)
		dc.SetFreezeColor();
	//////////////////////////////////////////////////////////////////////////
	if (!bFrozen)
		dc.SetColor(solidColor, alpha);

	if (IsSelected() || (bool)mv_filled)
	{
		dc.CullOff();
		dc.DepthWriteOff();
		//int rstate = dc.ClearStateFlag( GS_DEPTHWRITE );
		dc.DrawBall(pos, m_radius);
		//dc.SetState( rstate );
		dc.DepthWriteOn();
		dc.CullOn();
	}

	if (!bFrozen)
		dc.SetColor(wireColor, 1);
	dc.DrawWireSphere(pos, m_radius);
	if (m_edgeWidth)
		dc.DrawWireSphere(pos, m_radius - m_edgeWidth);
	//////////////////////////////////////////////////////////////////////////

	if (!m_entities.IsEmpty())
	{
		Vec3 vcol = Vec3(1, 1, 1);
		for (int i = 0; i < m_entities.GetCount(); i++)
		{
			CBaseObject* obj = GetEntity(i);
			if (!obj)
				continue;
			dc.DrawLine(GetWorldPos(), obj->GetWorldPos(), ColorF(vcol.x, vcol.y, vcol.z, 0.7f), ColorF(1, 1, 1, 0.7f));
		}
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::Serialize(CObjectArchive& ar)
{
	__super::Serialize(ar);
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		SetAreaId(m_areaId);
	}
	else
	{
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CAreaSphere::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = __super::Export(levelPath, xmlNode);
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::SetAreaId(int nAreaId)
{
	m_areaId = nAreaId;

	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetID(m_areaId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CAreaSphere::GetAreaId()
{
	return m_areaId;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::SetRadius(float fRadius)
{
	m_radius = fRadius;

	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetSphere(Vec3(ZERO), m_radius);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
float CAreaSphere::GetRadius()
{
	return m_radius;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::OnEntityAdded(IEntity const* const pIEntity)
{
	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->AddEntity(pIEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::OnEntityRemoved(IEntity const* const pIEntity)
{
	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->RemoveEntity(pIEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::PostLoad(CObjectArchive& ar)
{
	// After loading Update game structure.
	__super::PostLoad(ar);
	UpdateGameArea();
	UpdateAttachedEntities();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::UpdateGameArea()
{
	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetSphere(Vec3(ZERO), m_radius);
			pArea->SetProximity(m_edgeWidth);
			pArea->SetID(m_areaId);
			pArea->SetGroup(mv_groupId);
			pArea->SetPriority(mv_priority);
			pArea->SetInnerFadeDistance(m_innerFadeDistance);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::UpdateAttachedEntities()
{
	if (m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->RemoveEntities();
			size_t const numEntities = GetEntityCount();

			for (size_t i = 0; i < numEntities; ++i)
			{
				CEntityObject* const pEntity = GetEntity(i);

				if (pEntity != nullptr && pEntity->GetIEntity() != nullptr)
				{
					pArea->AddEntity(pEntity->GetIEntity()->GetId());
				}
			}
		}
	}
}

