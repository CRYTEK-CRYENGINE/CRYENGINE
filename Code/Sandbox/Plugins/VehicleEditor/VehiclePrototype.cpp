// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehiclePrototype.h"

#include "Viewport.h"
#include <CryEntitySystem/IEntitySystem.h>

#include <CryScriptSystem/IScriptSystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntitySystem.h>

#include "VehicleXMLLoader.h"
//#include "VehicleXMLSaver.h"
#include "VehiclePart.h"
#include "VehicleData.h"
#include "VehicleComp.h"
#include "VehicleHelperObject.h"
#include "Objects/DisplayContext.h"
#include "Objects/ObjectLoader.h"

REGISTER_CLASS_DESC(CVehiclePrototypeClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehiclePrototype, CBaseObject)

#define VEHICLE_RADIUS 1.f

//////////////////////////////////////////////////////////////////////////
CVehiclePrototype::CVehiclePrototype()
	: m_pVehicleData(0)
{
	m_name = "";
	m_vehicleEntity = 0;
}

void CVehiclePrototype::DeleteThis()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePrototype::Init(CBaseObject* prev, const string& file)
{
	string name = m_name;

	if (file.GetLength() > 0)
	{
		// use file when created from file
		string filename = PathUtil::GetFile(file);
		name = PathUtil::GetFileName(filename);
	}
	else if (prev)
	{
		// use classname when cloned
		//name = ((CEntity*)prev)->GetEntityClass();
		return false;
	}

	if (name.GetLength() == 0)
	{
		CryLog("Creation of VehiclePrototype failed, no filename given");
		return false;
	}

	const XmlNodeRef& xmlDef = CVehicleData::GetXMLDef();
	if (xmlDef == 0)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Loading definition file %s failed.. returning", VEHICLE_XML_DEF);
		return false;
	}

	string fullPath = VEHICLE_XML_PATH + string(name) + ".xml";

	// get entity name from xml
	XmlNodeRef vehicleXml = GetISystem()->LoadXmlFromFile(fullPath);

	if (vehicleXml == 0)
	{
		CryLog("Loading of %s failed, returning..", fullPath);
		return false;
	}

	if (!vehicleXml->haveAttr("name"))
	{
		CryLog("%s has no 'name' attribute [file %s], returning..", name, fullPath);
		return false;
	}

	// load data
	m_pVehicleData = VehicleDataLoad(xmlDef, vehicleXml);

	// reset clone
	if (m_pClone)
		m_pClone->Release();
	m_pClone = 0;

	if (!m_pVehicleData)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "VehicleXmlLoad for %s failed.", name);
		return false;
	}

	// init entity (creates game object)
	bool res = CBaseObject::Init(prev, vehicleXml->getAttr("name"));

	// save data again for diff checks
	//string targetFile = Path::GamePathToFullPath( VEHICLE_XML_PATH ) + name + string("_saved.xml");
	//if ( ! VehicleXmlSave( VEHICLE_XML_DEF, targetFile, m_pVehicleData ) )
	//{
	//  Warning( "VehicleXmlSave for %s failed.", targetFile );
	//}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePrototype::ReloadEntity()
{
	// respawn entity w. script reload
	GetCEntity()->Reload(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::AddComponent(CVehicleComponent* pComp)
{
	AttachChild(pComp);
	if (IVariable* pComps = GetChildVar(GetVariable(), "Components"))
	{
		pComps->AddVariable(pComp->GetVariable());
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::AttachChild(CBaseObject* child, bool bKeepPos, bool bInvalidateTM)
{
	child->AddEventListener(functor(*this, &CVehiclePrototype::OnObjectEvent));

	CBaseObject::AttachChild(child, bKeepPos, bInvalidateTM);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::OnObjectEvent(CBaseObject* node, int event)
{
	if (event == OBJECT_ON_DELETE)
	{
		VeedLog("[CVehiclePrototype]: ON_DELETE for %s", node->GetName());
		// when child deleted, remove its variable
		if (IVeedObject* pVO = IVeedObject::GetVeedObject(node))
		{
			if (pVO->DeleteVar())
			{
				if (GetVariable())
				{
					bool del = GetVariable()->DeleteVariable(pVO->GetVariable(), true);
					VeedLog("[CVehiclePrototype] deleting var for %s: %i", node->GetName(), del);
				}
				pVO->SetVariable(0);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::RemoveChild(CBaseObject* node)
{
	CBaseObject::RemoveChild(node);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::AddPart(CVehiclePart* pPart)
{
	AttachChild(pPart, true);

	IVariable* pParts = GetChildVar(GetVariable(), "Parts");
	assert(pParts);

	pParts->AddVariable(pPart->GetVariable());
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::Done()
{
	int nChildCount = GetChildCount();
	VeedLog("[CVehiclePrototype]: deleting %i children..", nChildCount);

	for (int i = nChildCount - 1; i >= 0; --i)
	{
		CBaseObject* pChild = GetChild(i);
		VeedLog("[CVehiclePrototype]: deleting %s", pChild->GetName());

		GetIEditor()->DeleteObject(pChild);
	}

	//CloseVeedDialog();

	CBaseObject::Done();

}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::Display(CObjectRenderHelper& objRenderHelper)
{
	// don't display anything, we're attached to vehicle now
	return;

	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	Matrix34 wtm = GetWorldTM();

	if (hasEntity())
	{
		// check if dc is 2d view, then render
		if (dc.flags & DISPLAY_2D)
		{
			IRenderNode* pRenderNode = GetCEntity()->GetIEntity()->GetRenderNode();
			if (!pRenderNode)
				return;

			SRendParams rp;
			rp.dwFObjFlags |= FOB_TRANS_MASK;
			rp.AmbientColor = ColorF(1, 1, 1, 1);
			rp.pMatrix = &wtm;
			//rp.nDLightMask = GetIEditor()->Get3DEngine()->GetLightMaskFromPosition(wtm.GetTranslation(),1.f) & 0xFFFF;
			//rp.pMaterial = GetIEditor()->GetIconManager()->GetHelperMaterial();

			SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(GetIEditor()->GetSystem()->GetViewCamera(), SRenderingPassInfo::DEFAULT_FLAGS, true, dc.GetDisplayContextKey());
			pRenderNode->Render(rp, passInfo);
		}

		// display CEntity stuff
		DrawDefault(dc, GetColor());
	}
	else
	{
		// if no entity spawned, display some helper
		float fHelperScale = 1;
		Vec3 dir = wtm.TransformVector(Vec3(0, fHelperScale, 0));
		Vec3 wp = wtm.GetTranslation();
		if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor(1, 1, 0);
		dc.DrawArrow(wp, wp + dir * 2, fHelperScale);

		if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor(GetColor(), 0.8f);

		dc.DrawBall(wp, VEHICLE_RADIUS);

		if (IsSelected())
		{
			dc.SetSelectedColor(0.6f);
			dc.DrawBall(wp, VEHICLE_RADIUS);
		}
		DrawDefault(dc);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVehiclePrototype::HitTest(HitContext& hc)
{
	// not to hit any more
	return false;

	if (hasEntity())
	{
		return GetCEntity()->HitTest(hc);
	}
	else
	{
		// todo
		Vec3 origin = GetWorldPos();
		float radius = VEHICLE_RADIUS;

		Vec3 w = origin - hc.raySrc;
		w = hc.rayDir.Cross(w);
		float d = w.GetLengthSquared();

		if (d < radius * radius + hc.distanceTolerance)
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
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::GetBoundBox(AABB& box)
{
	if (hasEntity())
	{
		GetCEntity()->GetBoundBox(box);
	}
	else
	{
		// todo
		Vec3 pos = GetWorldPos();
		float r = VEHICLE_RADIUS;
		box.min = pos - Vec3(r, r, r);
		box.max = pos + Vec3(r, r, r);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::GetLocalBounds(AABB& box)
{
	if (hasEntity())
	{
		GetCEntity()->GetLocalBounds(box);
	}
	else
	{
		// return local bounds
		float r = VEHICLE_RADIUS;
		box.min = -Vec3(r, r, r);
		box.max = Vec3(r, r, r);
	}
}

//////////////////////////////////////////////////////////////////////////
IVariable* CVehiclePrototype::GetVariable()
{
	if (!m_pClone)
		m_pClone = m_pVehicleData->GetRoot()->Clone(true);

	return m_pClone;
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::ApplyClonedData()
{
	ReplaceChildVars(m_pClone, m_pVehicleData->GetRoot());
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::Serialize(CObjectArchive& ar)
{
	// don't serialize the prototype
	return;

	CBaseObject::Serialize(ar);

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		// loading
		xmlNode->getAttr("veedVehicleName", m_name);
	}
	else
	{
		// saving
		xmlNode->setAttr("veedVehicleName", m_name);
	}
}

//////////////////////////////////////////////////////////////////////////
IVehicleData* CVehiclePrototype::GetVehicleData()
{
	return m_pVehicleData;
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::SetVehicleEntity(CEntityObject* pEnt)
{
	assert(pEnt);
	m_vehicleEntity = pEnt;

	SetWorldTM(pEnt->GetWorldTM());
	pEnt->AttachChild(this);
}

//////////////////////////////////////////////////////////////////////////
const CVehicleHelper* CVehiclePrototype::GetHelper(const string& name) const
{
	for (int i = 0; i < GetChildCount(); ++i)
	{
		CBaseObject* child = GetChild(i);
		if (child->IsKindOf(RUNTIME_CLASS(CVehicleHelper)) && child->GetName() == name)
		{
			return (CVehicleHelper*)child;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePrototype::AddHelper(CVehicleHelper* pHelper, IVariable* pHelperVar)
{
	IVariable* pMyVar = GetVariable();
	if (!pMyVar)
		return;

	// create "Helpers" array if not present [TODO: clean this]
	IVariable* pHelpers = GetChildVar(pMyVar, "Helpers");
	if (!pHelpers)
	{
		pHelpers = new CVariableArray;
		pHelpers->SetName("Helpers");
		pMyVar->AddVariable(pHelpers);
	}

	IVariable* pVar = pHelperVar;
	if (!pVar && !pHelper->IsFromGeometry())
	{
		// Geometry helpers aren't editable.
		// if not present, create variable for helper and insert into variable tree [TODO: generalize that]
		pVar = CreateDefaultChildOf("Helpers");
		pHelpers->AddVariable(pVar);
	}
	pHelper->SetVariable(pVar);
	pHelper->SetVehicle(this);

	AttachChild(pHelper, true);
}

