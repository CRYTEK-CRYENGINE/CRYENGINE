// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SmartObject.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "IDisplayViewport.h"
#include "IIconManager.h"
#include "SmartObjectsEditorDialog.h"
#include <CryAISystem/IAgent.h>
#include "Util/Math.h"

REGISTER_CLASS_DESC(CSmartObjectClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CSmartObject, CEntityObject)

CSmartObject::CSmartObject()
{
	m_entityClass = "SmartObject";
	m_pStatObj = NULL;
	m_pHelperMtl = NULL;
	m_pClassTemplate = NULL;

	UseMaterialLayersMask(false);
}

CSmartObject::~CSmartObject()
{
	if (m_pStatObj)
		m_pStatObj->Release();
}

//////////////////////////////////////////////////////////////////////////
void CSmartObject::Done()
{
	__super::Done();
}

bool CSmartObject::Init(CBaseObject* prev, const string& file)
{
	SetColor(RGB(255, 255, 0));
	bool res = __super::Init(prev, file);

	return res;
}

float CSmartObject::GetRadius()
{
	return 0.5f;
}

void CSmartObject::Display(DisplayContext& dc)
{
	const Matrix34& wtm = GetWorldTM();

	if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor(GetColor());

	if (!GetIStatObj())
	{
		dc.RenderObject(eStatObject_Anchor, wtm);
	}
	else if (!(dc.flags & DISPLAY_2D))
	{
		float color[4];
		color[0] = dc.GetColor().r * (1.0f / 255.0f);
		color[1] = dc.GetColor().g * (1.0f / 255.0f);
		color[2] = dc.GetColor().b * (1.0f / 255.0f);
		color[3] = dc.GetColor().a * (1.0f / 255.0f);

		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(GetIEditor()->GetSystem()->GetViewCamera());

		Matrix34 tempTm = wtm;
		SRendParams rp;
		rp.pMatrix = &tempTm;
		rp.AmbientColor = ColorF(color[0], color[1], color[2], 1);
		rp.fAlpha = color[3];
		rp.dwFObjFlags |= FOB_TRANS_MASK;
		//rp.nShaderTemplate = EFT_HELPER;
		if (m_pStatObj)
			m_pStatObj->Render(rp, passInfo);
	}

	dc.SetColor(GetColor());

	if (IsSelected() || IsHighlighted())
	{
		dc.PushMatrix(wtm);
		if (!GetIStatObj())
		{
			float r = GetRadius();
			dc.DrawWireBox(-Vec3(r, r, r), Vec3(r, r, r));
		}
		else
		{
			dc.DrawWireBox(m_pStatObj->GetBoxMin(), m_pStatObj->GetBoxMax());
		}
		dc.PopMatrix();

		// this is now done in CEntity::DrawDefault
		//	if ( gEnv->pAISystem )
		//		gEnv->pAISystem->DrawSOClassTemplate( GetIEntity() );
	}

	DrawDefault(dc);
}

bool CSmartObject::HitTest(HitContext& hc)
{
	if (GetIStatObj())
	{
		float hitEpsilon = hc.view->GetScreenScaleFactor(GetWorldPos()) * 0.01f;
		float hitDist;

		float fScale = GetScale().x;
		AABB boxScaled;
		GetLocalBounds(boxScaled);
		boxScaled.min *= fScale;
		boxScaled.max *= fScale;

		Matrix34 invertWTM = GetWorldTM();
		invertWTM.Invert();

		Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
		Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir);
		xformedRayDir.Normalize();

		Vec3 intPnt;
		// Check intersection with bbox edges.
		if (Intersect::Ray_AABBEdge(xformedRaySrc, xformedRayDir, boxScaled, hitEpsilon, hitDist, intPnt))
		{
			hc.dist = xformedRaySrc.GetDistance(intPnt);
			hc.object = this;
			return true;
		}

		return false;
	}

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

void CSmartObject::GetLocalBounds(AABB& box)
{
	if (GetIStatObj())
	{
		box.min = m_pStatObj->GetBoxMin();
		box.max = m_pStatObj->GetBoxMax();
	}
	else
	{
		float r = GetRadius();
		box.min = -Vec3(r, r, r);
		box.max = Vec3(r, r, r);
	}
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CSmartObject::GetIStatObj()
{
	if (m_pStatObj)
		return m_pStatObj;

	ISmartObjectManager::IStatObjPtr* ppStatObjects = NULL;
	IEntity* pEntity(GetIEntity());

	assert(pEntity);
	if (gEnv->pAISystem && pEntity)
	{
		ISmartObjectManager* piSmartObjManager = gEnv->pAISystem->GetSmartObjectManager();

		uint32 numSOFound = 0;
		piSmartObjManager->GetSOClassTemplateIStatObj(pEntity, NULL, numSOFound);
		if (numSOFound)
		{
			ppStatObjects = new ISmartObjectManager::IStatObjPtr[numSOFound];
			if (piSmartObjManager->GetSOClassTemplateIStatObj(pEntity, ppStatObjects, numSOFound) == 0)
			{
				SAFE_DELETE_ARRAY(ppStatObjects);
			}
		}
	}

	if (ppStatObjects)
	{
		m_pStatObj = ppStatObjects[0];
		m_pStatObj->AddRef();

		SAFE_DELETE_ARRAY(ppStatObjects);

		return m_pStatObj;
	}

	return NULL;

	// Try to load the object specified in the SO class template
	m_pClassTemplate = NULL;
	string classes;
	IVariable* pVar = GetProperties() ? GetProperties()->FindVariable("soclasses_SmartObjectClass") : NULL;
	if (pVar)
	{
		pVar->Get(classes);
		if (classes.IsEmpty())
			return NULL;

		SetStrings setClasses;
		CSOLibrary::String2Classes((const char*) classes, setClasses);

		SetStrings::iterator it, itEnd = setClasses.end();
		for (it = setClasses.begin(); it != itEnd; ++it)
		{
			CSOLibrary::VectorClassData::iterator itClass = CSOLibrary::FindClass(*it);
			if (itClass == CSOLibrary::GetClasses().end())
				continue;
			m_pClassTemplate = itClass->pClassTemplateData;
			if (m_pClassTemplate && !m_pClassTemplate->model.empty())
				break;
		}
	}

	if (m_pClassTemplate && !m_pClassTemplate->model.empty())
	{
		m_pStatObj = GetIEditor()->Get3DEngine()->LoadStatObj("Editor/Objects/" + m_pClassTemplate->model, NULL, NULL, false);
		if (!m_pStatObj)
		{
			CryLog("Error: Load Failed: %s", (const char*) m_pClassTemplate->model);
			return NULL;
		}
		m_pStatObj->AddRef();
		if (GetHelperMaterial())
			m_pStatObj->SetMaterial(GetHelperMaterial());
	}

	return m_pStatObj;
}

#define HELPER_MATERIAL "Editor/Objects/Helper"

//////////////////////////////////////////////////////////////////////////
IMaterial* CSmartObject::GetHelperMaterial()
{
	if (!m_pHelperMtl)
		m_pHelperMtl = GetIEditor()->Get3DEngine()->GetMaterialManager()->LoadMaterial(HELPER_MATERIAL);
	return m_pHelperMtl;
};

//////////////////////////////////////////////////////////////////////////
void CSmartObject::OnPropertyChange(IVariable* var)
{
	if (m_pStatObj)
	{
		IStatObj* pOld = (IStatObj*) GetIStatObj();
		m_pStatObj = NULL;
		GetIStatObj();
		if (pOld)
			pOld->Release();
	}
}

void CSmartObject::GetScriptProperties(XmlNodeRef xmlEntityNode)
{
	__super::GetScriptProperties(xmlEntityNode);

	if (m_pLuaProperties)
	{
		m_pLuaProperties->AddOnSetCallback(functor(*this, &CSmartObject::OnPropertyChange));
	}
}

void CSmartObject::OnEvent(ObjectEvent eventID)
{
	CBaseObject::OnEvent(eventID);

	switch (eventID)
	{
	case EVENT_INGAME:
	case EVENT_OUTOFGAME:
		SAFE_RELEASE(m_pStatObj);
		break;
	}
}
