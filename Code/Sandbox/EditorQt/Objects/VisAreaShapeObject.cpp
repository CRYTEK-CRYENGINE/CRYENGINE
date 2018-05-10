// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VisAreaShapeObject.h"
#include "Objects/InspectorWidgetCreator.h"

#include <Cry3DEngine/I3DEngine.h>

REGISTER_CLASS_DESC(CVisAreaShapeObjectClassDesc);
REGISTER_CLASS_DESC(CPortalShapeObjectClassDesc);
REGISTER_CLASS_DESC(COccluderAreaObjectClassDesc);
REGISTER_CLASS_DESC(COccluderPlaneObjectClassDesc);

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVisAreaShapeObject, CShapeObject)
IMPLEMENT_DYNCREATE(COccluderAreaObject, CVisAreaShapeObject)
IMPLEMENT_DYNCREATE(COccluderPlaneObject, CVisAreaShapeObject)
IMPLEMENT_DYNCREATE(CPortalShapeObject, CVisAreaShapeObject)

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
C3DEngineAreaObjectBase::C3DEngineAreaObjectBase()
{
	m_area = 0;
	mv_closed = true;
	m_bPerVertexHeight = true;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef C3DEngineAreaObjectBase::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = CBaseObject::Export(levelPath, xmlNode);

	// Export Points
	if (!m_points.empty())
	{
		const Matrix34& wtm = GetWorldTM();
		XmlNodeRef points = objNode->newChild("Points");
		for (int i = 0; i < m_points.size(); i++)
		{
			XmlNodeRef pnt = points->newChild("Point");
			pnt->setAttr("Pos", wtm.TransformPoint(m_points[i]));
		}
	}

	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngineAreaObjectBase::Done()
{
	if (m_area)
	{
		// reset the listener vis area in the unlucky case that we are deleting the
		// vis area where the listener is currently in
		REINST("do we still need this?");
		GetIEditorImpl()->Get3DEngine()->DeleteVisArea(m_area);
		m_area = 0;
	}
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool C3DEngineAreaObjectBase::CreateGameObject()
{
	if (!m_area)
	{
		uint64 visGUID = *((uint64*)&GetId());
		m_area = GetIEditorImpl()->Get3DEngine()->CreateVisArea(visGUID);
		m_bAreaModified = true;
		UpdateGameArea();
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// CVisAreaShapeObject
//////////////////////////////////////////////////////////////////////////
CVisAreaShapeObject::CVisAreaShapeObject()
{
	mv_height = 5;
	m_bDisplayFilledWhenSelected = true;
	// Ambient color zeroed out and hidden in Editor, no longer supported in VisAreas/Portals with PBR
	mv_vAmbientColor = Vec3(ZERO);
	mv_bAffectedBySun = false;
	mv_bIgnoreSkyColor = false;
	mv_fViewDistRatio = 100.f;
	mv_bSkyOnly = false;
	mv_bOceanIsVisible = false;
	mv_bIgnoreGI = false;

	SetColor(RGB(255, 128, 0));
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaShapeObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &CVisAreaShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");

	m_pVarObject->AddVariable(mv_bAffectedBySun, "AffectedBySun", functor(*this, &CVisAreaShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bIgnoreSkyColor, "IgnoreSkyColor", functor(*this, &CVisAreaShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bIgnoreGI, "IgnoreGI", functor(*this, &CVisAreaShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_fViewDistRatio, "ViewDistRatio", functor(*this, &CVisAreaShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bSkyOnly, "SkyOnly", functor(*this, &CVisAreaShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bOceanIsVisible, "OceanIsVisible", functor(*this, &CVisAreaShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bIgnoreOutdoorAO, "IgnoreOutdoorAO", functor(*this, &CVisAreaShapeObject::OnShapeChange));
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaShapeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	C3DEngineAreaObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CVisAreaShapeObject>("Vis Area", [](CVisAreaShapeObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_height, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_displayFilled, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_bAffectedBySun, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_bIgnoreSkyColor, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_bIgnoreGI, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_fViewDistRatio, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_bSkyOnly, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_bOceanIsVisible, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_bIgnoreOutdoorAO, ar);
	});
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaShapeObject::SetWorldPos(const Vec3& pos, int flags /*= 0*/)
{
	CBaseObject::SetWorldPos(pos, flags);
	m_bAreaModified = true;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaShapeObject::OnShapeChange(IVariable* pVariable)
{
	CShapeObject::OnShapeChange(pVariable);
	m_bAreaModified = true;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaShapeObject::UpdateGameArea()
{
	if (!m_bAreaModified)
		return;

	if (m_area)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 3)
		{
			const Matrix34& wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = wtm.TransformPoint(GetPoint(i));
			}

			Vec3 vAmbClr = mv_vAmbientColor;

			SVisAreaInfo info;
			info.fHeight = mv_height;
			info.vAmbientColor = vAmbClr;
			info.bAffectedByOutLights = mv_bAffectedBySun;
			info.bIgnoreSkyColor = mv_bIgnoreSkyColor;
			info.bSkyOnly = mv_bSkyOnly;
			info.fViewDistRatio = mv_fViewDistRatio;
			info.bDoubleSide = true;
			info.bUseDeepness = false;
			info.bUseInIndoors = false;
			info.bOceanIsVisible = mv_bOceanIsVisible;
			info.bIgnoreGI = mv_bIgnoreGI;
			info.bIgnoreOutdoorAO = mv_bIgnoreOutdoorAO;
			info.fPortalBlending = -1;

			GetIEditorImpl()->Get3DEngine()->UpdateVisArea(m_area, &points[0], points.size(), GetName(), info, true);
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CPortalShapeObject
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CPortalShapeObject::CPortalShapeObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor(RGB(100, 250, 60));

	mv_bUseDeepness = true;
	mv_bDoubleSide = true;
	mv_bPortalBlending = true;
	mv_fPortalBlendValue = 0.5f;
}

//////////////////////////////////////////////////////////////////////////
void CPortalShapeObject::InitVariables()
{
	CVisAreaShapeObject::InitVariables();

	m_pVarObject->AddVariable(mv_bUseDeepness, "UseDeepness", functor(*this, &CPortalShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bDoubleSide, "DoubleSide", functor(*this, &CPortalShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bPortalBlending, "LightBlending", functor(*this, &CPortalShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_fPortalBlendValue, "LightBlendValue", functor(*this, &CPortalShapeObject::OnShapeChange));

	mv_fPortalBlendValue.SetLimits(0.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////
void CPortalShapeObject::UpdateGameArea()
{
	if (!m_bAreaModified)
		return;

	if (m_area)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 3)
		{
			const Matrix34& wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = wtm.TransformPoint(GetPoint(i));
			}

			string name = string("Portal_") + GetName();

			Vec3 vAmbClr = mv_vAmbientColor;

			SVisAreaInfo info;
			info.fHeight = mv_height;
			info.vAmbientColor = vAmbClr;
			info.bAffectedByOutLights = mv_bAffectedBySun;
			info.bIgnoreSkyColor = mv_bIgnoreSkyColor;
			info.bSkyOnly = mv_bSkyOnly;
			info.fViewDistRatio = mv_fViewDistRatio;
			info.bDoubleSide = true;
			info.bUseDeepness = false;
			info.bUseInIndoors = false;
			info.bOceanIsVisible = mv_bOceanIsVisible;
			info.bIgnoreGI = mv_bIgnoreGI;
			info.fPortalBlending = -1;
			if (mv_bPortalBlending)
				info.fPortalBlending = mv_fPortalBlendValue;

			GetIEditorImpl()->Get3DEngine()->UpdateVisArea(m_area, &points[0], points.size(), name, info, true);
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
COccluderPlaneObject::COccluderPlaneObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor(RGB(200, 128, 60));

	mv_height = 5;
	mv_displayFilled = true;

	mv_bUseInIndoors = false;
	mv_bDoubleSide = true;
	mv_fViewDistRatio = 100.f;
}

//////////////////////////////////////////////////////////////////////////
void COccluderPlaneObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &COccluderPlaneObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");

	m_pVarObject->AddVariable(mv_fViewDistRatio, "CullDistRatio", functor(*this, &COccluderPlaneObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bUseInIndoors, "UseInIndoors", functor(*this, &COccluderPlaneObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bDoubleSide, "DoubleSide", functor(*this, &COccluderPlaneObject::OnShapeChange));
}

//////////////////////////////////////////////////////////////////////////
void COccluderPlaneObject::UpdateGameArea()
{
	if (!m_bAreaModified)
		return;

	if (m_area)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 1)
		{
			const Matrix34& wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = wtm.TransformPoint(GetPoint(i));
			}
			if (!m_points[0].IsEquivalent(m_points[1]))
			{
				string name = string("OcclArea_") + GetName();

				SVisAreaInfo info;
				info.fHeight = mv_height;
				info.vAmbientColor = Vec3(0, 0, 0);
				info.bAffectedByOutLights = false;
				info.bSkyOnly = false;
				info.fViewDistRatio = mv_fViewDistRatio;
				info.bDoubleSide = true;
				info.bUseDeepness = false;
				info.bUseInIndoors = mv_bUseInIndoors;
				info.bOceanIsVisible = false;
				info.fPortalBlending = -1;

				GetIEditorImpl()->Get3DEngine()->UpdateVisArea(m_area, &points[0], points.size(), name, info, false);
			}
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// COccluderAreaObject
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
COccluderAreaObject::COccluderAreaObject()
{
	m_bDisplayFilledWhenSelected = true;
	m_bForce2D = true;
	m_bNoCulling = true;
	SetColor(RGB(200, 128, 60));

	mv_displayFilled = true;

	mv_bUseInIndoors = false;
	mv_fViewDistRatio = 100.f;
}

//////////////////////////////////////////////////////////////////////////
void COccluderAreaObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");

	m_pVarObject->AddVariable(mv_fViewDistRatio, "CullDistRatio", functor(*this, &COccluderAreaObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_bUseInIndoors, "UseInIndoors", functor(*this, &COccluderAreaObject::OnShapeChange));
}

//////////////////////////////////////////////////////////////////////////
void COccluderAreaObject::UpdateGameArea()
{
	if (!m_bAreaModified)
		return;

	if (m_area)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 3)
		{
			const Matrix34& wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = wtm.TransformPoint(GetPoint(i));
			}
			{
				string name = string("OcclArea_") + GetName();

				SVisAreaInfo info;
				info.fHeight = 0;
				info.vAmbientColor = Vec3(0, 0, 0);
				info.bAffectedByOutLights = false;
				info.bSkyOnly = false;
				info.fViewDistRatio = mv_fViewDistRatio;
				info.bDoubleSide = true;
				info.bUseDeepness = false;
				info.bUseInIndoors = mv_bUseInIndoors;
				info.bOceanIsVisible = false;
				info.fPortalBlending = -1;

				GetIEditorImpl()->Get3DEngine()->UpdateVisArea(m_area, &points[0], points.size(), name, info, false);
			}
		}
	}
	m_bAreaModified = false;
}
