// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaterWaveObject.h"
#include "Material/Material.h"
#include "Viewport.h"
#include "EntityObject.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "Util/MFCUtil.h"

//////////////////////////////////////////////////////////////////////////
// CWaterWaveObject implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CWaterWaveObject, CRoadObject)

//////////////////////////////////////////////////////////////////////////
CWaterWaveObject::CWaterWaveObject()
{
	//m_entityClass = "AreaWaterWave";
	SetColor(CMFCUtils::Vec2Rgb(Vec3(0, 0, 1)));

	m_nID = -1;

	UseMaterialLayersMask(false);

	m_pRenderNode = 0;

	mv_ratioViewDist = 35;
}

//////////////////////////////////////////////////////////////////////////
bool CWaterWaveObject::Init(CBaseObject* prev, const string& file)
{
	bool res = __super::Init(prev, file);

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::Done()
{
	m_points.clear();
	m_sectors.clear();

	if (m_pRenderNode)
	{
		GetIEditorImpl()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}

	__super::Done();

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::InitVariables()
{
	__super::InitVariables();

	mv_surfaceUScale = 1.0f;
	mv_surfaceVScale = 1.0f;

	mv_speed = 5.0f;
	mv_speedVar = 2.0f;

	mv_lifetime = 8.0f;
	mv_lifetimeVar = 2.0f;

	mv_height = 0.5f;
	mv_heightVar = 0.5f;

	mv_posVar = 5.0f;

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_surfaceUScale, "UScale", "UScale", functor(*this, &CWaterWaveObject::OnParamChange));
	m_pVarObject->AddVariable(mv_surfaceVScale, "VScale", "VScale", functor(*this, &CWaterWaveObject::OnParamChange));

	m_pVarObject->AddVariable(mv_speed, "Speed", "Speed", functor(*this, &CWaterWaveObject::OnParamChange));
	m_pVarObject->AddVariable(mv_speedVar, "SpeedVar", "Speed variation", functor(*this, &CWaterWaveObject::OnParamChange));

	m_pVarObject->AddVariable(mv_lifetime, "Lifetime", "Lifetime", functor(*this, &CWaterWaveObject::OnParamChange));
	m_pVarObject->AddVariable(mv_lifetimeVar, "LifetimeVar", "Lifetime variation", functor(*this, &CWaterWaveObject::OnParamChange));

	m_pVarObject->AddVariable(mv_height, "Height", "Height", functor(*this, &CWaterWaveObject::OnParamChange));
	m_pVarObject->AddVariable(mv_heightVar, "HeightVar", "Height variation", functor(*this, &CWaterWaveObject::OnParamChange));

	m_pVarObject->AddVariable(mv_posVar, "posVar", "Position variation", functor(*this, &CWaterWaveObject::OnParamChange));
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CRoadObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CWaterWaveObject>("Water Wave", [](CWaterWaveObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_surfaceUScale, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_surfaceVScale, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_speed, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_speedVar, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_lifetime, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_lifetimeVar, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_height, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_heightVar, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_posVar, ar);
	});
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::OnWaterWaveParamChange(IVariable* var)
{
	// make me
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::Physicalize()
{
	// make me
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::OnWaterWavePhysicsParamChange(IVariable* var)
{
	Physicalize();
}

//////////////////////////////////////////////////////////////////////////

bool CWaterWaveObject::IsValidSector(const CRoadSector& pSector)
{
	return pSector.points[0] != pSector.points[1] &&
	       pSector.points[0] != pSector.points[2] &&
	       pSector.points[1] != pSector.points[2] &&
	       pSector.points[2] != pSector.points[3] &&
	       pSector.points[1] != pSector.points[3];
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::UpdateSectors()
{
	if (m_nID == -1)
	{
		m_nID = GetId().hipart;
		assert(m_nID);
	}

	std::vector<Vec3> pVertices;

	// Merge all positions a single list
	for (int i = 0; i < m_sectors.size(); i++)
	{
		CRoadSector& pSector = m_sectors[i];
		if (IsValidSector(pSector))
		{
			// copy vertex data
			for (int v(0); v < pSector.points.size(); ++v)
			{
				pVertices.push_back(pSector.points[v]);   // don't care about sector uvs
			}
		}
	}

	// Assign and create waves render node

	if (!m_pRenderNode)
	{
		m_pRenderNode = GetIEditorImpl()->Get3DEngine()->CreateRenderNode(eERType_WaterWave);
	}

	IWaterWaveRenderNode* pWaterWaveRN(static_cast<IWaterWaveRenderNode*>(m_pRenderNode));
	if (pWaterWaveRN)
	{
		pWaterWaveRN->SetRndFlags(0);
		pWaterWaveRN->SetViewDistRatio(mv_ratioViewDist);

		pWaterWaveRN->SetMinSpec(GetMinSpec());
		pWaterWaveRN->SetMaterialLayers(GetMaterialLayersMask());

		SWaterWaveParams pParams;

		pParams.m_fSpeed = mv_speed;
		pParams.m_fSpeedVar = mv_speedVar;
		pParams.m_fLifetime = mv_lifetime;
		pParams.m_fLifetimeVar = mv_lifetimeVar;
		pParams.m_fHeight = mv_height;
		pParams.m_fHeightVar = mv_heightVar;
		pParams.m_fPosVar = mv_posVar;

		pWaterWaveRN->SetParams(pParams);

		const Matrix34& pWorldTM(GetWorldTM());
		const Matrix34& pLocalTM(GetLocalTM());

		CMaterial* pMat((CMaterial*)GetMaterial());
		if (pMat)
		{
			pMat->AssignToEntity(pWaterWaveRN);
		}
		else
		{
			pWaterWaveRN->SetMaterial(0);
		}

		if (!pVertices.empty())
		{
			pWaterWaveRN->Create(m_nID,
			                     &pVertices[0], pVertices.size(),
			                     Vec2(mv_surfaceUScale, mv_surfaceVScale),
			                     pWorldTM);
		}
	}

	Physicalize();
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::Serialize(CObjectArchive& ar)
{
	XmlNodeRef xmlNode(ar.node);
	if (ar.bLoading)
	{
		float fVar(1.0f);

		xmlNode->getAttr("UScale", fVar);
		mv_surfaceUScale = fVar;

		xmlNode->getAttr("VScale", fVar);
		mv_surfaceVScale = fVar;

		xmlNode->getAttr("Speed", fVar);
		mv_speed = fVar;

		xmlNode->getAttr("SpeedVar", fVar);
		mv_speedVar = fVar;

		xmlNode->getAttr("Lifetime", fVar);
		mv_lifetime = fVar;

		xmlNode->getAttr("LifetimeVar", fVar);
		mv_lifetimeVar = fVar;

		xmlNode->getAttr("Height", fVar);
		mv_height = fVar;

		xmlNode->getAttr("HeightVar", fVar);
		mv_heightVar = fVar;

		xmlNode->getAttr("PosVar", fVar);
		mv_posVar = fVar;
	}
	else
	{
		xmlNode->setAttr("UScale", mv_surfaceUScale);
		xmlNode->setAttr("VScale", mv_surfaceVScale);
		xmlNode->setAttr("VScale", mv_surfaceVScale);
		xmlNode->setAttr("Speed", mv_speed);
		xmlNode->setAttr("SpeedVar", mv_speedVar);
		xmlNode->setAttr("Lifetime", mv_lifetime);
		xmlNode->setAttr("LifetimeVar", mv_lifetimeVar);
		xmlNode->setAttr("Height", mv_height);
		xmlNode->setAttr("HeightVar", mv_heightVar);
		xmlNode->setAttr("PosVar", mv_posVar);
	}

	__super::Serialize(ar);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CWaterWaveObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveObject::SetPoint(int index, const Vec3& pos)
{
	float fWaterLevel(GetIEditorImpl()->Get3DEngine()->GetWaterLevel());   // snap to water level
	Vec3 pOrig(pos + GetWorldPos());
	pOrig.z = fWaterLevel;

	Vec3 p = pOrig - GetWorldPos();

	if (0 <= index && index < m_points.size())
	{
		m_points[index].pos = p;
		BezierCorrection(index);
		SetRoadSectors();
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3 CWaterWaveObject::MapViewToCP(CViewport* view, CPoint point)
{
	if (!view)
	{
		return Vec3(0);
	}

	Vec3 pPos(0, 0, GetIEditorImpl()->Get3DEngine()->GetWaterLevel()); // snap to water level

	Plane pIsecPlane;
	pIsecPlane.SetPlane(Vec3(0, 0, 1), pPos);

	Vec3 raySrc(0, 0, 0), rayDir(1, 0, 0);
	view->ViewToWorldRay(point, raySrc, rayDir);

	Vec3 v;

	Ray ray(raySrc, rayDir);
	if (!Intersect::Ray_Plane(ray, pIsecPlane, v))
	{
		Plane inversePlane = pIsecPlane;
		inversePlane.n = -inversePlane.n;
		inversePlane.d = -inversePlane.d;
		if (!Intersect::Ray_Plane(ray, inversePlane, v))
		{
			v.Set(0, 0, 0);
		}
	}

	// Snap value to grid.
	v = view->SnapToGrid(v);

	return v;
}

//////////////////////////////////////////////////////////////////////////
int CWaterWaveObject::MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLDblClick)
	{
		Vec3 pos = MapViewToCP((CViewport*)view, point);

		if (m_points.size() < 2)
		{
			SetPos(pos);
		}

		pos.z += CreationZOffset();

		if (m_points.size() == 0)
		{
			InsertPoint(-1, Vec3(0, 0, 0));
		}
		else
		{
			SetPoint(m_points.size() - 1, pos - GetWorldPos());
		}

		if (event == eMouseLDblClick)
		{
			if (m_points.size() > GetMinPoints())
			{
				// Remove last unneeded point.
				m_points.pop_back();
				SetRoadSectors();
				OnUpdateUI();
				return MOUSECREATE_OK;
			}
			else
				return MOUSECREATE_ABORT;
		}

		if (event == eMouseLDown)
		{
			Vec3 vlen = Vec3(pos.x, pos.y, 0) - Vec3(GetWorldPos().x, GetWorldPos().y, 0);
			if (m_points.size() > 2 && vlen.GetLength() < kSplinePointSelectionRadius)
			{
				OnUpdateUI();
				return MOUSECREATE_OK;
			}
			if (GetPointCount() >= GetMaxPoints())
			{
				OnUpdateUI();
				return MOUSECREATE_OK;
			}

			InsertPoint(-1, pos - GetWorldPos());
		}
		return MOUSECREATE_CONTINUE;
	}
	return __super::MouseCreateCallback(view, event, point, flags);
}

/*!
 * Class Description of WaterWaveObject.
 */
class CWaterWaveObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_ROAD; };
	const char*    ClassName()         { return "Water wave"; };
	const char*    Category()          { return "Misc"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CWaterWaveObject); };
};

// Disabled by AdamJ 10/04/2014 - Causing crashes on adding. This needs complete removal! See CL 43640
// REGISTER_CLASS_DESC(CWaterWaveObjectClassDesc);

