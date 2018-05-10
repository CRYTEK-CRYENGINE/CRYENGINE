// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Geometry/EdMesh.h"
#include "SplineDistributor.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "Material/Material.h"
#include "Util/MFCUtil.h"
#include "Objects/InspectorWidgetCreator.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CSplineDistributor, CSplineObject)

//////////////////////////////////////////////////////////////////////////
CSplineDistributor::CSplineDistributor() :
	m_pGeometry(nullptr),
	m_bGameObjectCreated(false)
{
	mv_step = 4.0f;
	mv_follow = true;
	mv_zAngle = 0.0f;

	mv_outdoor = false;
	mv_castShadowMaps = true;
	mv_rainOccluder = true;
	mv_registerByBBox = false;
	mv_hideable = 0;
	mv_ratioLOD = 100;
	mv_ratioViewDist = 100;
	mv_noDecals = false;
	mv_recvWind = false;
	mv_integrQuality = 0;
	mv_Occluder = false;

	SetColor(CMFCUtils::Vec2Rgb(Vec3(0.4f, 0.5f, 0.4f)));

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_geometryFile, "Geometry", functor(*this, &CSplineDistributor::OnGeometryChange), IVariable::DT_OBJECT);
	m_pVarObject->AddVariable(mv_step, "StepSize", "Step Size", functor(*this, &CSplineDistributor::OnParamChange));
	mv_step.SetLimits(0.25f, 100.f);
	m_pVarObject->AddVariable(mv_follow, "Follow", "Follow Spline", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_zAngle, "ZAngle", "Z Angle", functor(*this, &CSplineDistributor::OnParamChange));
	mv_zAngle.SetLimits(-360.f, 360.f);

	m_pVarObject->AddVariable(mv_outdoor, "OutdoorOnly", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_castShadowMaps, "CastShadowMaps", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_rainOccluder, "RainOccluder", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_registerByBBox, "SupportSecondVisarea", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_hideable, "Hideable", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_ratioLOD, "LodRatio", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_ratioViewDist, "ViewDistRatio", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_excludeFromTriangulation, "NotTriangulate", functor(*this, &CSplineDistributor::OnParamChange));
	// keep for future implementation
	//AddVariable( mv_aiRadius, "AIRadius", functor(*this,&CSplineDistributor::OnAIRadiusVarChange) );
	m_pVarObject->AddVariable(mv_noDecals, "NoStaticDecals", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_recvWind, "RecvWind", functor(*this, &CSplineDistributor::OnParamChange));
	m_pVarObject->AddVariable(mv_Occluder, "Occluder", functor(*this, &CSplineDistributor::OnParamChange));

	mv_ratioLOD.SetLimits(0, 255);
	mv_ratioViewDist.SetLimits(0, 255);
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CSplineObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CSplineDistributor>("Spline Distributor", [](CSplineDistributor* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_geometryFile, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_step, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_follow, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_zAngle, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_outdoor, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_castShadowMaps, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_rainOccluder, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_registerByBBox, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_hideable, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_ratioLOD, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_ratioViewDist, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_excludeFromTriangulation, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_noDecals, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_recvWind, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_Occluder, ar);
	});
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	FreeGameData();
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CSplineDistributor::CreateGameObject()
{
	m_bGameObjectCreated = true;
	OnUpdate();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::SetObjectCount(int num)
{
	int oldNum = m_renderNodes.size();
	if (oldNum < num)
	{
		m_renderNodes.resize(num);
		for (int i = oldNum; i < num; ++i)
		{
			m_renderNodes[i] = GetIEditorImpl()->Get3DEngine()->CreateRenderNode(eERType_Brush);
		}
	}
	else
	{
		for (int i = num; i < oldNum; ++i)
		{
			GetIEditorImpl()->Get3DEngine()->DeleteRenderNode(m_renderNodes[i]);
		}
		m_renderNodes.resize(num);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::FreeGameData()
{
	SetObjectCount(0);

	// Release Mesh.
	if (m_pGeometry)
		m_pGeometry->RemoveUser();
	m_pGeometry = 0;
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::OnUpdate()
{
	if (!m_bGameObjectCreated)
		return;
	SetObjectCount(int(GetSplineLength() / mv_step) + 1);

	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::UpdateVisibility(bool visible)
{
	if (visible == CheckFlags(OBJFLAG_INVISIBLE))
	{
		__super::UpdateVisibility(visible);
		UpdateEngineNode();
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSplineDistributor::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::OnParamChange(IVariable* pVariable)
{
	OnUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::OnGeometryChange(IVariable* pVariable)
{
	LoadGeometry(mv_geometryFile);
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::LoadGeometry(const string& filename)
{
	if (m_pGeometry)
	{
		if (m_pGeometry->IsSameObject(filename))
			return;

		m_pGeometry->RemoveUser();
	}

	m_pGeometry = CEdMesh::LoadMesh(filename);
	if (m_pGeometry)
		m_pGeometry->AddUser();

	if (m_pGeometry)
		UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::InvalidateTM(int nWhyFlags)
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (!(nWhyFlags & eObjectUpdateFlags_RestoreUndo)) // Can skip updating game object when restoring undo.
	{
		UpdateEngineNode(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::UpdateEngineNode(bool bOnlyTransform)
{
	if (m_renderNodes.empty() || GetPointCount() == 0)
		return;

	float length = GetSplineLength();
	int numObjects = m_renderNodes.size();

	for (int i = 0; i < numObjects; ++i)
	{
		IRenderNode* pRenderNode = m_renderNodes[i];

		// Set brush render flags.
		int renderFlags = 0;
		if (mv_outdoor)
			renderFlags |= ERF_OUTDOORONLY;
		if (mv_castShadowMaps)
			renderFlags |= ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS;
		if (mv_rainOccluder)
			renderFlags |= ERF_RAIN_OCCLUDER;
		if (mv_registerByBBox)
			renderFlags |= ERF_REGISTER_BY_BBOX;
		if (IsHidden() || IsHiddenBySpec())
			renderFlags |= ERF_HIDDEN;
		if (mv_hideable == 1)
			renderFlags |= ERF_HIDABLE;
		if (mv_hideable == 2)
			renderFlags |= ERF_HIDABLE_SECONDARY;
		if (mv_excludeFromTriangulation)
			renderFlags |= ERF_EXCLUDE_FROM_TRIANGULATION;
		if (mv_noDecals)
			renderFlags |= ERF_NO_DECALNODE_DECALS;
		if (mv_recvWind)
			renderFlags |= ERF_RECVWIND;
		if (mv_Occluder)
			renderFlags |= ERF_GOOD_OCCLUDER;

		if (pRenderNode->GetRndFlags() & ERF_COLLISION_PROXY)
			renderFlags |= ERF_COLLISION_PROXY;

		if (pRenderNode->GetRndFlags() & ERF_RAYCAST_PROXY)
			renderFlags |= ERF_RAYCAST_PROXY;

		if (IsSelected())
			renderFlags |= ERF_SELECTED;

		pRenderNode->SetRndFlags(renderFlags);

		pRenderNode->SetMinSpec(GetMinSpec());
		pRenderNode->SetViewDistRatio(mv_ratioViewDist);
		pRenderNode->SetLodRatio(mv_ratioLOD);

		pRenderNode->SetMaterialLayers(GetMaterialLayersMask());

		if (m_pGeometry)
		{
			Matrix34A tm;

			tm.SetIdentity();

			if (mv_zAngle != 0.0f)
			{
				Matrix34A tmRot;
				tmRot.SetRotationZ(mv_zAngle / 180.f * g_PI);
				tm = tmRot * tm;
			}

			int index;
			float pos = GetPosByDistance(i * mv_step, index);
			Vec3 p = GetBezierPos(index, pos);

			if (mv_follow)
			{
				Matrix34A tmPos;
				Vec3 t = GetBezierTangent(index, pos);
				Vec3 b = GetLocalBezierNormal(index, pos);
				Vec3 n = t.Cross(b);
				tmPos.SetFromVectors(t, b, n, p);
				tm = tmPos * tm;
			}
			else
			{
				Matrix34A tmPos;
				tmPos.SetTranslationMat(p);
				tm = tmPos * tm;
			}

			tm = GetWorldTM() * tm;

			pRenderNode->SetEntityStatObj(m_pGeometry->GetIStatObj(), &tm);
		}
		else
		{
			pRenderNode->SetEntityStatObj(0, 0);
		}

		//IStatObj *pStatObj = pRenderNode->GetEntityStatObj();
		//if (pStatObj)
		//{
		//	float r = mv_aiRadius;
		//	pStatObj->SetAIVegetationRadius(r);
		//}

		// Fast exit if only transformation needs to be changed.
		if (bOnlyTransform)
			continue;

		if (GetMaterial())
		{
			((CMaterial*)GetMaterial())->AssignToEntity(pRenderNode);
		}
		else
		{
			// Reset all material settings for this node.
			pRenderNode->SetMaterial(0);
		}

		pRenderNode->SetLayerId(0);

	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineDistributor::SetLayerId(uint16 nLayerId)
{
	for (size_t i = 0; i < m_renderNodes.size(); ++i)
	{
		if (m_renderNodes[i])
			m_renderNodes[i]->SetLayerId(nLayerId);
	}
}

//////////////////////////////////////////////////////////////////////////
// Class Description of SplineDistributor.

//////////////////////////////////////////////////////////////////////////
class CSplineDistributorClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; }
	const char*    ClassName()         { return "SplineDistributor"; }
	const char*    Category()          { return "Misc"; }
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CSplineDistributor); }
};

REGISTER_CLASS_DESC(CSplineDistributorClassDesc);

