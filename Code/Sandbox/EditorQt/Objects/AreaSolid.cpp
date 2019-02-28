// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AreaSolid.h"
#include "PickEntitiesPanel.h"
#include "..\Viewport.h"
#include <CryEntitySystem/IEntitySystem.h>
#include "SurfaceInfoPicker.h"
#include "Brush/BrushDesigner.h"
#include "Brush/BrushDesignerPolygonDecomposer.h"
#include "Brush/BrushCommonInterface.h"
#include "Brush/AreaSolidEditTool.h"

class CAreaSolidPanel : public CDialog
{
	DECLARE_DYNAMIC(CAreaSolidPanel)

public:
	CAreaSolidPanel(CAreaSolid* pSolid, CWnd* pParent = NULL) : CDialog(CAreaSolidPanel::IDD, pParent)
	{
		m_pAreaSolid = pSolid;
		Create(IDD, pParent);
	}
	virtual ~CAreaSolidPanel()
	{
	}

	enum { IDD = IDD_PANEL_AREASOLID };

protected:
	void DoDataExchange(CDataExchange* pDX)
	{
		CDialog::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_EDIT_AREASOLID, m_editAreaSolidButton);
	}

	BOOL OnInitDialog() override
	{
		__super::OnInitDialog();

		if (m_pAreaSolid)
		{
			m_editAreaSolidButton.SetToolClass(RUNTIME_CLASS(CAreaSolidEditTool), "object", m_pAreaSolid);
			m_editAreaSolidButton.EnableWindow(TRUE);
		}

		return TRUE;
	}

	DECLARE_MESSAGE_MAP()

	CToolButton m_editAreaSolidButton;
	CAreaSolid* m_pAreaSolid;
};

IMPLEMENT_DYNAMIC(CAreaSolidPanel, CDialog)
BEGIN_MESSAGE_MAP(CAreaSolidPanel, CDialog)
END_MESSAGE_MAP()

int CAreaSolid::s_nGlobalFileAreaSolidID = 0;

IMPLEMENT_DYNCREATE(CAreaSolid, CEntityObject)

int CAreaSolid::m_nRollupId = 0;
CPickEntitiesPanel* CAreaSolid::m_pPanel = NULL;

int CAreaSolid::m_nActivateEditAreaSolidRollUpID = 0;
CAreaSolidPanel* CAreaSolid::m_pActivateEditAreaSolidPanel = NULL;

class CAreaSolidSizer : public ICrySizer
{
public:
	CAreaSolidSizer()
	{
		Reset();
	}
	~CAreaSolidSizer(){}

	void   Release() {}
	size_t GetTotalSize()
	{
		return m_size;
	}
	size_t GetObjectCount()
	{
		return m_count;
	}
	bool AddObject(const void* pIdentifier, size_t nSizeBytes, int nCount)
	{
		m_size += nSizeBytes;
		m_count += nCount;
		return true;
	}
	IResourceCollector* GetResourceCollector()
	{
		return NULL;
	}
	void SetResourceCollector(IResourceCollector* pColl)  {}
	void Push(const char* szComponentName)                {}
	void PushSubcomponent(const char* szSubcomponentName) {}
	void Pop()                                            {}
	void Reset()
	{
		m_count = 0;
		m_size = 0;
	}
	void End()
	{
	}

private:
	size_t m_count, m_size;
};

CAreaSolid::CAreaSolid() :
	m_pSizer(new CAreaSolidSizer())
{
	m_pBrush = new CBaseBrush(0);
	m_areaId = -1;
	m_edgeWidth = 0;
	mv_groupId = 0;
	mv_priority = 0;
	m_entityClass = "AreaSolid";
	m_bAreaModified = false;
	m_nUniqFileIdForExport = (s_nGlobalFileAreaSolidID++);

	GetDesigner()->SetModeFlag(CBrushDesigner::eDesignerMode_DrillAfterOffset);

	CBaseObject::SetMaterial("Editor/Materials/areasolid");

	SetColor(RGB(0, 0, 255));
}

bool CAreaSolid::Init(IEditor* ie, CBaseObject* prev, const CString& file)
{
	bool res = CEntityObject::Init(ie, prev, file);

	if (m_pEntity)
		m_pEntity->CreateProxy(ENTITY_PROXY_AREA);

	if (prev)
	{
		CAreaSolid* pAreaSolid = static_cast<CAreaSolid*>(prev);
		CBaseBrush* pBrush = pAreaSolid->GetBrush();
		CBrushDesigner* pDesigner = pAreaSolid->GetDesigner();
		if (pBrush && pDesigner)
		{
			SetBrush(new CBaseBrush(*pBrush));
			SetDesigner(new CBrushDesigner(*pDesigner));
			UpdateBrush();
		}
	}

	return res;
}

void CAreaSolid::Done()
{
	CEditTool* pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CAreaSolidEditTool)))
	{
		CBaseObject* pBaseObject = ((CAreaSolidEditTool*)pTool)->GetBaseObject();
		if (this == pBaseObject)
			pTool->EndEditParams();
	}

	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::InitVariables()
{
	AddVariable(m_areaId, "AreaId", functor(*this, &CAreaSolid::OnAreaChange));
	AddVariable(m_edgeWidth, "FadeInZone", functor(*this, &CAreaSolid::OnSizeChange));
	AddVariable(mv_groupId, "GroupId", functor(*this, &CAreaSolid::OnAreaChange));
	AddVariable(mv_priority, "Priority", functor(*this, &CAreaSolid::OnAreaChange));
	AddVariable(mv_filled, "Filled", functor(*this, &CAreaSolid::OnAreaChange));
}

//////////////////////////////////////////////////////////////////////////
bool CAreaSolid::CreateGameObject()
{
	bool bRes = __super::CreateGameObject();
	if (bRes)
	{
		UpdateGameArea();
	}
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CAreaSolid::HitTest(HitContext& hc)
{
	if (!GetBrush())
		return false;

	Vec3 pnt;
	Vec3 localRaySrc;
	Vec3 localRayDir;
	CSurfaceInfoPicker::RayWorldToLocal(GetWorldTM(), hc.raySrc, hc.rayDir, localRaySrc, localRayDir);
	bool bHit = false;

	if (Intersect::Ray_AABB(localRaySrc, localRayDir, GetDesigner()->GetBoundBox(), pnt))
	{
		Vec3 prevSrc = hc.raySrc;
		Vec3 prevDir = hc.rayDir;
		hc.raySrc = localRaySrc;
		hc.rayDir = localRayDir;

		if (GetBrush())
			bHit = GetBrush()->HitTest(this, GetDesigner(), hc);

		hc.raySrc = prevSrc;
		hc.rayDir = prevDir;

		// The local hit distance should be transformed into world reference
		// in case of local coordinate being scaled.
		Vec3 localHitPos = localRaySrc + localRayDir * hc.dist;
		Vec3 worldHitPos = GetWorldTM().TransformPoint(localHitPos);
		hc.dist = (worldHitPos - hc.raySrc).len();
	}
	if (bHit)
		hc.object = this;
	return bHit;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::GetBoundBox(AABB& box)
{
	if (GetBrush())
		box.SetTransformedAABB(GetWorldTM(), GetDesigner()->GetBoundBox());
	else
		box = AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::GetLocalBounds(AABB& box)
{
	if (GetBrush())
		box = GetDesigner()->GetBoundBox();
	else
		box = AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::BeginEditParams(IEditor* ie, int flags)
{
	if (!m_pActivateEditAreaSolidPanel)
	{
		m_pActivateEditAreaSolidPanel = new CAreaSolidPanel(this);
		m_nActivateEditAreaSolidRollUpID = GetIEditor()->AddRollUpPage(ROLLUP_OBJECTS, _T("Activate Edit Tool"), m_pActivateEditAreaSolidPanel);
	}

	__super::BeginEditParams(ie, flags);

	if (!m_pPanel)
	{
		m_pPanel = new CPickEntitiesPanel;
		m_pPanel->Create(CPickEntitiesPanel::IDD, AfxGetMainWnd());
		m_nRollupId = ie->AddRollUpPage(ROLLUP_OBJECTS, "Attached Entities", m_pPanel);
	}
	if (m_pPanel)
		m_pPanel->SetOwner(this);
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::EndEditParams(IEditor* ie)
{
	if (m_nRollupId)
		ie->RemoveRollUpPage(ROLLUP_OBJECTS, m_nRollupId);
	m_nRollupId = 0;
	m_pPanel = NULL;

	__super::EndEditParams(ie);

	if (m_nActivateEditAreaSolidRollUpID)
		ie->RemoveRollUpPage(ROLLUP_OBJECTS, m_nActivateEditAreaSolidRollUpID);
	m_nActivateEditAreaSolidRollUpID = 0;
	m_pActivateEditAreaSolidPanel = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::OnAreaChange(IVariable* pVar)
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::OnSizeChange(IVariable* pVar)
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::Serialize(CObjectArchive& ar)
{
	__super::Serialize(ar);

	XmlNodeRef xmlNode = ar.node;

	if (ar.bLoading)
	{
		XmlNodeRef brushNode = xmlNode->findChild("Brush");

		if (!GetDesigner())
			SetDesigner(new CBrushDesigner);

		GetDesigner()->SetModeFlag(CBrushDesigner::eDesignerMode_DrillAfterOffset);

		if (brushNode)
			GetDesigner()->Serialize(brushNode, ar.bLoading, ar.bUndo);

		if (ar.bUndo)
		{
			if (GetBrush())
				GetBrush()->DeleteRenderAllNodes();
		}

		SetAreaId(m_areaId);

		UpdateBrush();
		UpdateGameArea();
	}
	else
	{
		if (GetBrush())
		{
			XmlNodeRef brushNode = xmlNode->newChild("Brush");
			GetDesigner()->Serialize(brushNode, ar.bLoading, ar.bUndo);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CAreaSolid::Export(const CString& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = __super::Export(levelPath, xmlNode);
	XmlNodeRef areaNode = objNode->findChild("Area");
	if (areaNode)
		areaNode->setAttr("AreaSolidFileName", GenerateGameFilename());
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::SetAreaId(int nAreaId)
{
	m_areaId = nAreaId;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::PostLoad(CObjectArchive& ar)
{
	// After loading Update game structure.
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSolid::UpdateGameArea()
{
	if (!m_pEntity)
		return;

	IEntityAreaProxyPtr pArea = crycomponent_cast<IEntityAreaProxyPtr>(m_pEntity->CreateProxy(ENTITY_PROXY_AREA));
	if (!pArea)
		return;

	pArea->SetProximity(m_edgeWidth);
	pArea->SetID(m_areaId);
	pArea->SetGroup(mv_groupId);
	pArea->SetPriority(mv_priority);

	pArea->ClearEntities();
	for (int i = 0; i < GetEntityCount(); i++)
	{
		CEntityObject* pEntity = GetEntity(i);
		if (pEntity && pEntity->GetIEntity())
			pArea->AddEntity(pEntity->GetIEntity()->GetId());
	}

	if (GetBrush() == NULL)
		return;

	CBrushDesigner* pDesigner(GetDesigner());
	const float MinimumAreaSolidRadius = 0.1f;
	if (GetDesigner()->GetBoundBox().GetRadius() <= MinimumAreaSolidRadius)
		return;

	DESIGNER_SHELF_RECONSTRUCTOR(GetDesigner());
	GetDesigner()->SetShelf(0);

	std::vector<CBrushRegion::RegionPtr> regionList;
	pDesigner->GetRegionList(regionList);

	pArea->BeginSettingSolid(GetWorldTM());
	for (int i = 0, iRegionSize(regionList.size()); i < iRegionSize; ++i)
	{
		BUtil::VertexList vertexList;
		BUtil::FaceList faceList;
		CBrushDesignerPolygonDecomposer decomposer;
		decomposer.TriangulateRegion(regionList[i], vertexList, faceList);

		AddConvexhullToEngineArea(pArea.get(), vertexList, faceList, true);
		std::vector<CBrushRegion::RegionPtr> innerRegions;
		regionList[i]->GetSeparatedRegions(innerRegions, CBrushRegion::eSR_InnerHull);
		for (int k = 0, iSize(innerRegions.size()); k < iSize; ++k)
		{
			if (pDesigner->QueryEquivalentRegion(innerRegions[k]))
				continue;
			innerRegions[k]->ReverseEdges();
			vertexList.clear();
			faceList.clear();
			CBrushDesignerPolygonDecomposer decomposer;
			decomposer.TriangulateRegion(innerRegions[k], vertexList, faceList);
			AddConvexhullToEngineArea(pArea.get(), vertexList, faceList, false);
		}
	}
	pArea->EndSettingSolid();

	m_pSizer->Reset();
	pArea->GetMemoryUsage(m_pSizer.get());
}

void CAreaSolid::AddConvexhullToEngineArea(IEntityAreaProxy* pArea, const BUtil::VertexList& vertexList, const BUtil::FaceList& faceList, bool bObstructrion)
{
	for (int a = 0, iFaceSize(faceList.size()); a < iFaceSize; ++a)
	{
		std::vector<Vec3> convex;
		for (int b = 0, iIndexSize(faceList[a]->m_IndexList.size()); b < iIndexSize; ++b)
			convex.push_back(ToVec3(vertexList[faceList[a]->m_IndexList[b]]->m_Pos));
		pArea->AddConvexHullToSolid(&convex[0], bObstructrion, convex.size());
	}
}

void CAreaSolid::InvalidateTM(int nWhyFlags)
{
	__super::InvalidateTM(nWhyFlags);
	UpdateEngineNode();
	UpdateGameArea();
}

void CAreaSolid::Display(DisplayContext& dc)
{
	DrawDefault(dc);

	if (GetBrush())
	{
		for (int i = 0; i < GetEntityCount(); i++)
		{
			CEntityObject* pEntity = GetEntity(i);
			if (!pEntity)
				continue;
			if (!pEntity->GetIEntity())
				continue;

			BrushVec3 vNearestPos;
			if (!QueryNearestPos(pEntity->GetWorldPos(), vNearestPos))
				continue;

			dc.DrawLine(pEntity->GetWorldPos(), ToVec3(vNearestPos), ColorF(1.0f, 1.0f, 1.0f, 0.7f), ColorF(1.0f, 1.0f, 1.0f, 0.7f));
		}
	}
}

void CAreaSolid::DisplayMemoryUsage(DisplayContext& dc)
{
	CString sizeBuffer;
	sizeBuffer.Format("Size : %.4f KB", (float)m_pSizer->GetTotalSize() * 0.001f);
	DrawTextOn2DBox(dc, GetPos(), sizeBuffer, 1.4f, ColorF(1, 1, 1, 1), ColorF(0, 0, 0, 0.25f));
}

CString CAreaSolid::GenerateGameFilename() const
{
	CString sRealGeomFileName;
#ifdef SEG_WORLD
	sRealGeomFileName = CString("%level%/Brush/areasolid") + GuidUtil::ToString(GetId());
#else
	char sId[128];
	itoa(m_nUniqFileIdForExport, sId, 10);
	sRealGeomFileName = CString("%level%/Brush/areasolid") + sId;
#endif
	return sRealGeomFileName;
}

void CAreaSolid::SetMaterial(IEditorMaterial* mtl)
{
	CBaseObject::SetMaterial(mtl);
	UpdateEngineNode();
}

void CAreaSolid::OnEvent(ObjectEvent event)
{
	__super::OnEvent(event);

	switch (event)
	{
	case EVENT_INGAME:
		{
			if (GetBrush())
				GetBrush()->SetRenderFlags(GetBrush()->GetRenderFlags() | ERF_HIDDEN);
			CEditTool* pEditTool = GetIEditor()->GetEditTool();
			if (pEditTool)
			{
				if (pEditTool->IsKindOf(RUNTIME_CLASS(CBrushDesignerEditTool)))
					((CBrushDesignerEditTool*)pEditTool)->LeaveCurrentTool();
			}
		}
		break;

	case EVENT_OUTOFGAME:
		if (GetBrush())
			GetBrush()->SetRenderFlags(GetBrush()->GetRenderFlags() & (~ERF_HIDDEN));
		break;
	}
}

CBaseBrush* CAreaSolid::GetBrush() const
{
	if (!m_pBrush)
		m_pBrush = new CBaseBrush(0);
	return m_pBrush;
}

