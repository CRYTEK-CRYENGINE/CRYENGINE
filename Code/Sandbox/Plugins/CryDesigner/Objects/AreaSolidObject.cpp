// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AreaSolidObject.h"
#include "Viewport.h"
#include <CryEntitySystem/IEntitySystem.h>
#include "SurfaceInfoPicker.h"
#include "Core/Model.h"
#include "Core/PolygonDecomposer.h"
#include "Core/Helper.h"
#include "Tools/AreaSolidTool.h"
#include "ToolFactory.h"
#include "Core/LoopPolygons.h"
#include "Util/ExcludedEdgeManager.h"
#include "Util/Display.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "Objects/DisplayContext.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "Preferences/ViewportPreferences.h"

namespace Designer
{
REGISTER_CLASS_DESC(AreaSolidClassDesc);

int AreaSolidObject::s_nGlobalFileAreaSolidID = 0;

IMPLEMENT_DYNCREATE(AreaSolidObject, CEntityObject)

AreaSolidObject::AreaSolidObject()
	: m_innerFadeDistance(0.0f)
{
	m_pCompiler = new ModelCompiler(0);
	m_areaId = -1;
	m_edgeWidth = 0;
	mv_groupId = 0;
	mv_priority = 0;
	m_entityClass = "AreaSolid";
	m_nUniqFileIdForExport = (s_nGlobalFileAreaSolidID++);
	mv_filled.Set(true);

	GetModel()->SetFlag(eModelFlag_LeaveFrameAfterPolygonRemoved);

	CBaseObject::SetMaterial("%EDITOR%/Materials/areasolid");
	m_pSizer = GetIEditor()->GetSystem()->CreateSizer();
	SetColor(RGB(0, 0, 255));

	m_bIgnoreGameUpdate = true;
}

AreaSolidObject::~AreaSolidObject()
{
	m_pSizer->Release();
}

bool AreaSolidObject::Init(CBaseObject* prev, const string& file)
{
	m_bIgnoreGameUpdate = true;

	bool res = CEntityObject::Init(prev, file);

	if (m_pEntity)
		m_pEntity->CreateProxy(ENTITY_PROXY_AREA);

	if (prev)
	{
		AreaSolidObject* pAreaSolid = static_cast<AreaSolidObject*>(prev);
		ModelCompiler* pCompiler = pAreaSolid->GetCompiler();
		Model* pModel = pAreaSolid->GetModel();
		if (pCompiler && pModel)
		{
			SetCompiler(new ModelCompiler(*pCompiler));
			SetModel(new Model(*pModel));
			ApplyPostProcess(GetMainContext(), ePostProcess_Mesh);
		}
	}

	m_bIgnoreGameUpdate = false;

	return res;
}

void AreaSolidObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(m_innerFadeDistance, "InnerFadeDistance", functor(*this, &AreaSolidObject::OnAreaChange));
	m_pVarObject->AddVariable(m_areaId, "AreaId", functor(*this, &AreaSolidObject::OnAreaChange));
	m_pVarObject->AddVariable(m_edgeWidth, "FadeInZone", functor(*this, &AreaSolidObject::OnSizeChange));
	m_pVarObject->AddVariable(mv_groupId, "GroupId", functor(*this, &AreaSolidObject::OnAreaChange));
	m_pVarObject->AddVariable(mv_priority, "Priority", functor(*this, &AreaSolidObject::OnAreaChange));
	m_pVarObject->AddVariable(mv_filled, "Filled", functor(*this, &AreaSolidObject::OnAreaChange));
}

bool AreaSolidObject::CreateGameObject()
{
	bool bRes = __super::CreateGameObject();
	if (bRes)
		UpdateGameArea();
	return bRes;
}

bool AreaSolidObject::HitTest(HitContext& hc)
{
	if (!GetCompiler())
		return false;

	Vec3 pnt;
	Vec3 localRaySrc;
	Vec3 localRayDir;
	CSurfaceInfoPicker::RayWorldToLocal(GetWorldTM(), hc.raySrc, hc.rayDir, localRaySrc, localRayDir);
	bool bHit = false;

	if (Intersect::Ray_AABB(localRaySrc, localRayDir, GetModel()->GetBoundBox(), pnt))
	{
		Vec3 prevSrc = hc.raySrc;
		Vec3 prevDir = hc.rayDir;
		hc.raySrc = localRaySrc;
		hc.rayDir = localRayDir;

		if (GetCompiler())
			bHit = Designer::HitTest(BrushRay(ToBrushVec3(localRaySrc), ToBrushVec3(localRayDir)), GetMainContext(), hc);

		hc.raySrc = prevSrc;
		hc.rayDir = prevDir;

		Vec3 localHitPos = localRaySrc + localRayDir * hc.dist;
		Vec3 worldHitPos = GetWorldTM().TransformPoint(localHitPos);
		hc.dist = (worldHitPos - hc.raySrc).len();
	}

	if (bHit)
		hc.object = this;

	return bHit;
}

void AreaSolidObject::GetBoundBox(AABB& box)
{
	if (GetCompiler())
		box.SetTransformedAABB(GetWorldTM(), GetModel()->GetBoundBox());
	else
		box = AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
}

void AreaSolidObject::GetLocalBounds(AABB& box)
{
	if (GetCompiler())
		box = GetModel()->GetBoundBox();
	else
		box = AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
}


void AreaSolidObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAreaObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<AreaSolidObject>("Area Solid", [](AreaSolidObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->m_innerFadeDistance, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_areaId, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_edgeWidth, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_groupId, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_priority, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_filled, ar);

		if (ar.openBlock("area", "<Area"))
		{
			ar(Serialization::ActionButton([=]
			{
				GetIEditor()->ExecuteCommand("general.open_pane 'Modeling'");

				GetIEditor()->SetEditTool("EditTool.AreaSolidTool", false);
			}), "edit", "^Edit");
			ar.closeBlock();
		}
	});
}

void AreaSolidObject::Serialize(CObjectArchive& ar)
{
	__super::Serialize(ar);

	XmlNodeRef xmlNode = ar.node;

	if (ar.bLoading)
	{
		XmlNodeRef brushNode = xmlNode->findChild("Brush");

		if (!GetModel())
			SetModel(new Model);

		GetModel()->SetFlag(eModelFlag_LeaveFrameAfterPolygonRemoved);

		if (brushNode)
			GetModel()->Serialize(brushNode, ar.bLoading, ar.bUndo);

		if (ar.bUndo)
		{
			if (GetCompiler())
				GetCompiler()->DeleteAllRenderNodes();
		}

		SetAreaId(m_areaId);

		ApplyPostProcess(GetMainContext());
	}
	else if (GetCompiler())
	{
		XmlNodeRef brushNode = xmlNode->newChild("Brush");
		GetModel()->Serialize(brushNode, ar.bLoading, ar.bUndo);
	}
}

XmlNodeRef AreaSolidObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = __super::Export(levelPath, xmlNode);

	XmlNodeRef componentsNode = objNode->findChild("Components");
	if (componentsNode)
	{
		XmlNodeRef componentNode = componentsNode->findChild("Component");
		if (componentNode)
		{
			XmlNodeRef areaNode = componentNode->findChild("Area");
			if (areaNode)
			{
				string gameFilename;
				GenerateGameFilename(gameFilename);
				areaNode->setAttr("AreaSolidFileName", gameFilename.GetBuffer());
			}
		}
	}

	return objNode;
}

void AreaSolidObject::SetAreaId(int nAreaId)
{
	m_areaId = nAreaId;
	UpdateGameArea();
}

class CAreaSolidBeginEnd
{
public:

	CAreaSolidBeginEnd(IEntityAreaComponent* const pArea, Matrix34 const& tm)
		: m_pArea(pArea)
	{
		if (m_pArea)
		{
			m_pArea->BeginSettingSolid(tm);
		}
	}

	~CAreaSolidBeginEnd()
	{
		if (m_pArea)
		{
			m_pArea->EndSettingSolid();
		}
	}

private:

	IEntityAreaComponent* const m_pArea;
};

void AreaSolidObject::UpdateGameArea()
{
	LOADING_TIME_PROFILE_SECTION
	if (!m_pEntity)
		return;

	const auto pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();
	if (!pArea)
		return;

	CAreaSolidBeginEnd const areaSolidBeginEnd(pArea, GetWorldTM());
	pArea->SetProximity(m_edgeWidth);
	pArea->SetID(m_areaId);
	pArea->SetGroup(mv_groupId);
	pArea->SetPriority(mv_priority);
	pArea->SetInnerFadeDistance(m_innerFadeDistance);

	pArea->RemoveEntities();
	for (size_t polygonIndex = 0; polygonIndex < GetEntityCount(); ++polygonIndex)
	{
		CEntityObject* pEntity = GetEntity(polygonIndex);
		if (pEntity && pEntity->GetIEntity())
			pArea->AddEntity(pEntity->GetIEntity()->GetId());
	}

	if (GetCompiler() == nullptr)
		return;

	Model* pModel(GetModel());
	const float MinimumAreaSolidRadius = 0.1f;
	if (GetModel()->GetBoundBox().GetRadius() <= MinimumAreaSolidRadius)
		return;

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Base);

	std::vector<PolygonPtr> polygonList;
	pModel->GetPolygonList(polygonList);

	for (auto const& polygon : polygonList)
	{
		FlexibleMesh mesh;
		PolygonDecomposer decomposer;
		decomposer.TriangulatePolygon(polygon, mesh);

		std::vector<std::vector<Vec3>> faces;
		ConvertMeshFacesToFaces(mesh.vertexList, mesh.faceList, faces);

		AddConvexhullToEngineArea(pArea, faces, true);
		std::vector<PolygonPtr> innerPolygons = polygon->GetLoops()->GetHoleClones();

		for (auto const& innerPolygon : innerPolygons)
		{
			innerPolygon->ReverseEdges();
			if (pModel->QueryEquivalentPolygon(innerPolygon))
				continue;

			bool bRealHole = false;
			for (int a = 0, iEdgeCount(innerPolygon->GetEdgeCount()); a < iEdgeCount; ++a)
			{
				PolygonList neighborPolygons;
				GetModel()->QueryNeighbourPolygonsByEdge(innerPolygon->GetEdge(a), neighborPolygons);
				if (neighborPolygons.size() == 1)
				{
					bRealHole = true;
					break;
				}
			}
			if (!bRealHole)
				continue;

			mesh.Clear();
			faces.clear();
			PolygonDecomposer decomposer;
			decomposer.TriangulatePolygon(innerPolygon, mesh);
			ConvertMeshFacesToFaces(mesh.vertexList, mesh.faceList, faces);

			AddConvexhullToEngineArea(pArea, faces, false);
		}
	}
	m_pSizer->Reset();
	pArea->GetMemoryUsage(m_pSizer);
}

void AreaSolidObject::AddConvexhullToEngineArea(IEntityAreaComponent* pArea, std::vector<std::vector<Vec3>>& faces, bool bObstructrion)
{
	for (auto const& face : faces)
	{
		pArea->AddConvexHullToSolid(&face[0], bObstructrion, face.size());
	}
}

void AreaSolidObject::InvalidateTM(int nWhyFlags)
{
	__super::InvalidateTM(nWhyFlags);
	UpdateEngineNode();
	UpdateGameArea();
}

void AreaSolidObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	DrawDefault(dc);

	if (GetCompiler())
	{
		for (int i = 0, iCount(GetEntityCount()); i < GetEntityCount(); i++)
		{
			CEntityObject* pEntity = GetEntity(i);
			if (!pEntity || !pEntity->GetIEntity())
				continue;

			BrushVec3 vNearestPos;
			if (!QueryNearestPos(pEntity->GetWorldPos(), vNearestPos))
				continue;

			dc.DrawLine(pEntity->GetWorldPos(), ToVec3(vNearestPos), ColorF(1.0f, 1.0f, 1.0f, 0.7f), ColorF(1.0f, 1.0f, 1.0f, 0.7f));
		}
	}

	DisplayMemoryUsage(dc);

	if (!GetDesigner())
	{
		ExcludedEdgeManager excludeEdges;

		if (IsHighlighted())
		{
			AABB bb;
			GetLocalBounds(bb);

			BrushVec3 bottom[] = {
				ToBrushVec3(Vec3(bb.min.x, bb.min.y, bb.min.z)),
				ToBrushVec3(Vec3(bb.max.x, bb.min.y, bb.min.z)),
				ToBrushVec3(Vec3(bb.max.x, bb.max.y, bb.min.z)),
				ToBrushVec3(Vec3(bb.min.x, bb.max.y, bb.min.z))
			};

			BrushVec3 top[] = {
				ToBrushVec3(Vec3(bb.min.x, bb.min.y, bb.max.z)),
				ToBrushVec3(Vec3(bb.max.x, bb.min.y, bb.max.z)),
				ToBrushVec3(Vec3(bb.max.x, bb.max.y, bb.max.z)),
				ToBrushVec3(Vec3(bb.min.x, bb.max.y, bb.max.z))
			};

			for (int i = 0; i < 4; ++i)
			{
				excludeEdges.Add(BrushEdge3D(bottom[i], bottom[(i + 1) % 4]));
				excludeEdges.Add(BrushEdge3D(top[i], top[(i + 1) % 4]));
				excludeEdges.Add(BrushEdge3D(bottom[i], top[i]));
			}
		}

		dc.PushMatrix(GetWorldTM());
		ColorB oldColor = dc.GetColor();
		Display::DisplayModel(dc, GetModel(), &excludeEdges, eShelf_Any, 2, ColorB(0, 0, 255));
		dc.SetColor(oldColor);
		dc.PopMatrix();
	}
}

void AreaSolidObject::DisplayMemoryUsage(DisplayContext& dc)
{
	if (IsEmpty() || !GetDesigner())
		return;

	AABB aabb;
	GetBoundBox(aabb);

	string sizeBuffer;
	sizeBuffer.Format("Size : %.4f KB", (float)m_pSizer->GetTotalSize() * 0.001f);
	DrawTextOn2DBox(dc, aabb.GetCenter(), sizeBuffer, 1.4f, ColorF(1, 1, 1, 1), ColorF(0, 0, 0, 0.25f));
}

void AreaSolidObject::GenerateGameFilename(string& generatedFileName) const
{
	generatedFileName.Format("%%level%%/Brush/areasolid%d", m_nUniqFileIdForExport);
}

void AreaSolidObject::SetMaterial(IEditorMaterial* mtl)
{
	CBaseObject::SetMaterial(mtl);
	UpdateEngineNode();
}

void AreaSolidObject::OnEvent(ObjectEvent event)
{
	__super::OnEvent(event);

	switch (event)
	{
	case EVENT_INGAME:
		if (GetDesigner())
			GetDesigner()->LeaveCurrentTool();
	case EVENT_HIDE_HELPER:
	case EVENT_OUTOFGAME:
	case EVENT_SHOW_HELPER:
	{
		UpdateHiddenIStatObjState();
		break;
	}
	}
}

bool AreaSolidObject::IsHiddenByOption()
{
	bool visible = false;
	mv_filled.Get(visible);
	return !(visible && GetIEditor()->IsHelpersDisplayed());
}

void AreaSolidObject::SetHidden(bool bHidden, bool bAnimated)
{
	__super::SetHidden(bHidden, bAnimated);
	UpdateHiddenIStatObjState();
}

ModelCompiler* AreaSolidObject::GetCompiler() const
{
	if (!m_pCompiler)
		m_pCompiler = new ModelCompiler(0);
	return m_pCompiler;
}

//////////////////////////////////////////////////////////////////////////
void AreaSolidObject::Reload(bool bReloadScript /* = false */)
{
	LOADING_TIME_PROFILE_SECTION
	__super::Reload(bReloadScript);

	// During reloading the entity+proxies get completely removed.
	// Hence we need to completely recreate the proxy and update all data to it.
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void AreaSolidObject::OnAreaChange(IVariable* pVar)
{
	if (pVar == &mv_filled)
	{
		UpdateHiddenIStatObjState();
	}
	else
	{
		UpdateGameArea();
	}
}

//////////////////////////////////////////////////////////////////////////
void AreaSolidObject::OnEntityAdded(IEntity const* const pIEntity)
{
	if (!m_bIgnoreGameUpdate && m_pEntity != nullptr)
	{
		auto const pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea != nullptr)
		{
			pArea->AddEntity(pIEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void AreaSolidObject::OnEntityRemoved(IEntity const* const pIEntity)
{
	if (!m_bIgnoreGameUpdate && m_pEntity != nullptr)
	{
		auto const pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea != nullptr)
		{
#ifdef SW_ENTITY_ID_USE_GUID
			pArea->RemoveEntity(pIEntity->GetGuid());
#else
			pArea->RemoveEntity(pIEntity->GetId());
#endif
		}
	}
}


std::vector<EDesignerTool> AreaSolidObject::GetIncompatibleSubtools()
{
	std::vector<EDesignerTool> toolList;
	toolList.push_back(eDesigner_Separate);
	toolList.push_back(eDesigner_Copy);
	toolList.push_back(eDesigner_ArrayClone);
	toolList.push_back(eDesigner_CircleClone);
	toolList.push_back(eDesigner_UVMapping);
	toolList.push_back(eDesigner_Export);
	toolList.push_back(eDesigner_Boolean);
	toolList.push_back(eDesigner_Subdivision);
	toolList.push_back(eDesigner_SmoothingGroup);

	return toolList;
}
} // namespace Designer

