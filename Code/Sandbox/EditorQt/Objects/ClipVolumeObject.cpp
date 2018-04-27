// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ClipVolumeObject.h"
#include "ShapeObject.h"

#include "Controls\ToolButton.h"

#include "Brush/BrushDesigner.h"
#include "Brush/BrushDesignerPolygonDecomposer.h"
#include "Brush/BrushCommonInterface.h"
#include "Brush/ClipVolumeEditTool.h"
#include "Brush/BrushDesignerConverter.h"
#include "../Viewport.h"

class CClipVolumeObjectPanel : public CDialog
{
	DECLARE_DYNAMIC(CClipVolumeObjectPanel)

public:
	CClipVolumeObjectPanel(CClipVolumeObject* pClipVolume, CWnd* pParent = NULL) : CDialog(CClipVolumeObjectPanel::IDD, pParent)
	{
		m_pClipVolume = pClipVolume;
		Create(IDD, pParent);
	}
	virtual ~CClipVolumeObjectPanel()
	{
	}

	afx_msg void OnBnClickedLoadCgf()
	{
		CString filename = "";
		if (CFileUtil::SelectSingleFile(EFILE_TYPE_GEOMETRY, filename))
		{
			if (_smart_ptr<IStatObj> pStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(filename.GetString(), NULL, NULL, false))
			{
				_smart_ptr<CBrushDesigner> pNewDesigner = new CBrushDesigner();
				if (CBrushDesignerConverter::ConvertMeshToBrushDesigner(pStatObj->GetIndexedMesh(true), pNewDesigner))
				{
					GetIEditor()->BeginUndo();
					m_pClipVolume->GetDesigner()->RecordUndo("ClipVolume: Load CGF", m_pClipVolume);
					m_pClipVolume->SetDesigner(pNewDesigner);
					m_pClipVolume->UpdateBrush();
					m_pClipVolume->UpdateGameResource();
					GetIEditor()->AcceptUndo("ClipVolume: Load CGF");
				}
			}
		}
	}

	enum { IDD = IDD_PANEL_CLIPVOLUME };

protected:
	void DoDataExchange(CDataExchange* pDX)
	{
		CDialog::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_EDIT_CLIPVOLUME, m_editClipVolumeButton);
		DDX_Control(pDX, IDC_LOAD_CLIPVOLUME_FROM_CGF, m_loadCgfButton);
	}

	BOOL OnInitDialog() override
	{
		__super::OnInitDialog();

		if (m_pClipVolume)
		{
			m_editClipVolumeButton.SetToolClass(RUNTIME_CLASS(CClipVolumeEditTool), "object", m_pClipVolume);
			m_editClipVolumeButton.EnableWindow(TRUE);
			m_loadCgfButton.EnableWindow(TRUE);
		}

		return TRUE;
	}

	DECLARE_MESSAGE_MAP()

	CToolButton m_editClipVolumeButton;
	CCustomButton      m_loadCgfButton;

	CClipVolumeObject* m_pClipVolume;
};

IMPLEMENT_DYNAMIC(CClipVolumeObjectPanel, CDialog)

BEGIN_MESSAGE_MAP(CClipVolumeObjectPanel, CDialog)
ON_BN_CLICKED(IDC_LOAD_CLIPVOLUME_FROM_CGF, OnBnClickedLoadCgf)
END_MESSAGE_MAP()

IMPLEMENT_DYNCREATE(CClipVolumeObject, CEntityObject)

CClipVolumeObjectPanel * CClipVolumeObject::m_pEditClipVolumePanel = NULL;
int CClipVolumeObject::s_nEditClipVolumeRollUpID = 0;
int CClipVolumeObject::s_nGlobalFileClipVolumeID = 0;

CClipVolumeObject::CClipVolumeObject()
{
	m_entityClass = "ClipVolume";
	m_nUniqFileIdForExport = (s_nGlobalFileClipVolumeID++);

	EnableReload(false);
	UseMaterialLayersMask(false);

	AddVariable(mv_filled, "Filled");
}

CClipVolumeObject::~CClipVolumeObject()
{
}

IStatObj* CClipVolumeObject::GetIStatObj()
{
	_smart_ptr<IStatObj> pStatObj;
	if (CBaseBrush* pBaseBrush = GetBrush())
		pBaseBrush->GetIStatObj(&pStatObj);

	return pStatObj.get();
}

void CClipVolumeObject::Serialize(CObjectArchive& ar)
{
	__super::Serialize(ar);

	if (ar.bLoading)
	{
		if (!GetDesigner())
			SetDesigner(new CBrushDesigner);
		GetDesigner()->SetModeFlag(CBrushDesigner::eDesignerMode_DrillAfterOffset);

		if (XmlNodeRef brushNode = ar.node->findChild("Brush"))
			GetDesigner()->Serialize(brushNode, ar.bLoading, ar.bUndo);

		if (ar.bUndo)
		{
			if (GetBrush())
				GetBrush()->DeleteRenderAllNodes();
		}

		UpdateBrush();
		UpdateGameResource();
	}
	else
	{
		if (GetBrush())
		{
			XmlNodeRef brushNode = ar.node->newChild("Brush");
			GetDesigner()->Serialize(brushNode, ar.bLoading, ar.bUndo);
		}
	}
}

bool CClipVolumeObject::CreateGameObject()
{
	__super::CreateGameObject();

	if (m_pEntity)
	{
		CString gameFileName = GenerateGameFilename();
		SEntitySpawnParams spawnParams;
		spawnParams.pUserData = (void*)gameFileName.GetString();

		IEntityProxyPtr pClipVolumeProxy = m_pEntity->CreateProxy(ENTITY_PROXY_CLIPVOLUME);
		pClipVolumeProxy->Init(m_pEntity, spawnParams);

		UpdateGameResource();
	}

	return m_pEntity != NULL;
}

void CClipVolumeObject::GetLocalBounds(AABB& bbox)
{
	bbox.Reset();

	if (CBrushDesigner* pDesigner = GetDesigner())
		bbox = pDesigner->GetBoundBox();
}

void CClipVolumeObject::Display(DisplayContext& dc)
{
	if (IStatObj* pStatObj = GetIStatObj())
	{
		if (IsFrozen())
			dc.SetFreezeColor();

		COLORREF lineColor = GetColor();
		COLORREF solidColor = GetColor();

		if (IsSelected())
			lineColor = dc.GetSelectedColor();
		else if (IsHighlighted() && gSettings.viewports.bHighlightMouseOverGeometry)
			lineColor = RGB(255, 255, 0);

		SGeometryDebugDrawInfo dd;
		dd.tm = GetWorldTM();
		dd.bExtrude = false;
		dd.color = ColorB((unsigned int)solidColor);
		dd.lineColor = ColorB((unsigned int)lineColor);

		const float fAlpha = (IsSelected() || mv_filled) ? 0.3f : 0;
		dd.color.a = uint8(fAlpha * 255);
		dd.lineColor.a = 255;

		if (IsSelected())
		{
			dd.color.a = uint8(0.15f * 255);
			dd.lineColor.a = uint8(0.15f * 255);
			;

			dc.DepthTestOff();
			pStatObj->DebugDraw(dd);

			dd.color.a = uint8(0.25f * 255);
			dd.lineColor.a = uint8(1.0f * 255);
			;

			dc.DepthTestOn();
			pStatObj->DebugDraw(dd);
		}
		else
			pStatObj->DebugDraw(dd);
	}

	DrawDefault(dc);
}

void CClipVolumeObject::UpdateGameResource()
{
	if (m_pEntity)
	{
		if (IClipVolumeProxyPtr pVolume = crycomponent_cast<IClipVolumeProxyPtr>(m_pEntity->CreateProxy(ENTITY_PROXY_CLIPVOLUME)))
		{
			if (IStatObj* pStatObj = GetIStatObj())
			{
				pVolume->UpdateRenderMesh(pStatObj->GetRenderMesh());
			}
		}
	}

	UpdateCollisionData();
	InvalidateTM(0);   // BBox will most likely have changed
}

void CClipVolumeObject::UpdateCollisionData()
{
	struct SEdgeCompare
	{
		enum ComparisonResult
		{
			eResultLess = 0,
			eResultEqual,
			eResultGreater
		};

		ComparisonResult pointCompare(const Vec3& a, const Vec3& b)
		{
			for (int i = 0; i < 3; ++i)
			{
				if (a[i] < b[i]) return eResultLess;
				else if (a[i] > b[i]) return eResultGreater;
			}

			return eResultEqual;
		}

		bool operator()(const Lineseg& a, const Lineseg& b)
		{
			ComparisonResult cmpResult = pointCompare(a.start, b.start);
			if (cmpResult == eResultEqual)
				cmpResult = pointCompare(a.end, b.end);

			return cmpResult == eResultLess;
		}
	};

	std::set<Lineseg, SEdgeCompare> processedEdges;
	m_MeshEdges.clear();

	if (IStatObj* pStatObj = GetIStatObj())
	{
		if (IRenderMesh* pRenderMesh = pStatObj->GetRenderMesh())
		{
			pRenderMesh->LockForThreadAccess();
			m_MeshEdges.reserve(pRenderMesh->GetIndicesCount());

			if (pRenderMesh->GetIndicesCount() && pRenderMesh->GetVerticesCount())
			{
				int posStride;
				const uint8* pPositions = (uint8*)pRenderMesh->GetPosPtr(posStride, FSL_READ);
				const vtx_idx* pIndices = pRenderMesh->GetIndexPtr(FSL_READ);

				TRenderChunkArray& Chunks = pRenderMesh->GetChunks();
				for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
				{
					CRenderChunk* pChunk = &Chunks[nChunkId];
					if (!(pChunk->m_nMatFlags & MTL_FLAG_NODRAW))
					{
						for (uint i = pChunk->nFirstIndexId; i < pChunk->nFirstIndexId + pChunk->nNumIndices; i += 3)
						{
							for (int e = 0; e < 3; ++e)
							{
								uint32 i0 = pIndices[i + e];
								uint32 i1 = pIndices[i + (e + 1) % 3];

								Lineseg edge = Lineseg(
								  *alias_cast<Vec3*>(pPositions + i0 * posStride),
								  *alias_cast<Vec3*>(pPositions + i1 * posStride)
								  );

								if (SEdgeCompare().pointCompare(edge.start, edge.end) == SEdgeCompare::eResultGreater)
									std::swap(edge.start, edge.end);

								if (processedEdges.find(edge) == processedEdges.end())
									processedEdges.insert(edge);
							}
						}
					}
				}

				m_MeshEdges.insert(m_MeshEdges.begin(), processedEdges.begin(), processedEdges.end());
			}

			pRenderMesh->UnlockStream(VSF_GENERAL);
			pRenderMesh->UnLockForThreadAccess();
		}
	}
}

bool CClipVolumeObject::HitTest(HitContext& hc)
{
	const float DistanceThreshold = 5.0f; // distance threshold for edge collision detection (in pixels)

	Matrix34 inverseWorldTM = GetWorldTM().GetInverted();
	Vec3 raySrc = inverseWorldTM.TransformPoint(hc.raySrc);
	Vec3 rayDir = inverseWorldTM.TransformVector(hc.rayDir).GetNormalized();

	AABB bboxLS;
	GetLocalBounds(bboxLS);

	bool bHit = false;
	Vec3 vHitPos;

	if (Intersect::Ray_AABB(raySrc, rayDir, bboxLS, vHitPos))
	{
		if (hc.b2DViewport)
		{
			vHitPos = GetWorldTM().TransformPoint(vHitPos);
			bHit = true;
		}
		else if (hc.camera)
		{
			Matrix44A mView, mProj;
			mathMatrixPerspectiveFov(&mProj, hc.camera->GetFov(), hc.camera->GetProjRatio(), hc.camera->GetNearPlane(), hc.camera->GetFarPlane());
			mathMatrixLookAt(&mView, hc.camera->GetPosition(), hc.camera->GetPosition() + hc.camera->GetViewdir(), hc.camera->GetUp());

			Matrix44A mWorldViewProj = GetWorldTM().GetTransposed() * mView * mProj;

			int viewportWidth, viewportHeight;
			hc.view->GetDimensions(&viewportWidth, &viewportHeight);
			Vec3 testPosClipSpace = Vec3(2.0f * hc.point2d.x / (float)viewportWidth - 1.0f, -2.0f * hc.point2d.y / (float)viewportHeight + 1.0f, 0);

			const float minDist = sqr(DistanceThreshold / viewportWidth * 2.0f) + sqr(DistanceThreshold / viewportHeight * 2.0f);
			const float aspect = viewportHeight / (float)viewportWidth;

			for (int e = 0; e < m_MeshEdges.size(); ++e)
			{
				Vec4 e0 = Vec4(m_MeshEdges[e].start, 1.0f) * mWorldViewProj;
				Vec4 e1 = Vec4(m_MeshEdges[e].end, 1.0f) * mWorldViewProj;

				if (e0.w < hc.camera->GetNearPlane() && e1.w < hc.camera->GetNearPlane())
					continue;
				else if (e0.w < hc.camera->GetNearPlane() || e1.w < hc.camera->GetNearPlane())
				{
					if (e0.w > e1.w)
						std::swap(e0, e1);

					// Push vertex slightly in front of near plane
					float t = (hc.camera->GetNearPlane() + 1e-5 - e0.w) / (e1.w - e0.w);
					e0 += (e1 - e0) * t;
				}

				Lineseg edgeClipSpace = Lineseg(Vec3(e0 / e0.w), Vec3(e1 / e1.w));

				float t;
				Distance::Point_Lineseg2D<float>(testPosClipSpace, edgeClipSpace, t);
				Vec3 closestPoint = edgeClipSpace.GetPoint(t);

				const float closestDistance = sqr(testPosClipSpace.x - closestPoint.x) + sqr(testPosClipSpace.y - closestPoint.y) * aspect;
				if (closestDistance < minDist)
				{
					// unproject to world space. NOTE: CCamera::Unproject uses clip space coordinate system (no y-flip)
					Vec3 v0 = Vec3((edgeClipSpace.start.x * 0.5 + 0.5) * viewportWidth, (edgeClipSpace.start.y * 0.5 + 0.5) * viewportHeight, edgeClipSpace.start.z);
					Vec3 v1 = Vec3((edgeClipSpace.end.x * 0.5 + 0.5) * viewportWidth, (edgeClipSpace.end.y * 0.5 + 0.5) * viewportHeight, edgeClipSpace.end.z);

					hc.camera->Unproject(Vec3::CreateLerp(v0, v1, t), vHitPos);
					bHit = true;

					break;
				}
			}
		}
	}

	if (bHit)
	{
		hc.dist = hc.raySrc.GetDistance(vHitPos);
		hc.object = this;
		return true;
	}

	return false;
}

bool CClipVolumeObject::Init(IEditor* ie, CBaseObject* prev, const CString& file)
{
	bool res = __super::Init(ie, prev, file);
	SetColor(RGB(0, 0, 255));

	if (m_pEntity)
		m_pEntity->CreateProxy(ENTITY_PROXY_CLIPVOLUME);

	if (prev)
	{
		CClipVolumeObject* pVolume = static_cast<CClipVolumeObject*>(prev);
		CBaseBrush* pBrush = pVolume->GetBrush();
		CBrushDesigner* pDesigner = pVolume->GetDesigner();

		if (pBrush && pDesigner)
		{
			SetBrush(new CBaseBrush(*pBrush));
			SetDesigner(new CBrushDesigner(*pDesigner));
			UpdateBrush();
		}
	}

	if (CBaseBrush* pBrush = GetBrush())
		pBrush->RemoveFlags(CBaseBrush::eBaseBrushFlag_Physicalize | CBaseBrush::eBaseBrushFlag_CastShadow);

	return res;
}

void CClipVolumeObject::Done()
{
	CEditTool* pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CClipVolumeEditTool)))
	{
		CBaseObject* pBaseObject = ((CClipVolumeEditTool*)pTool)->GetBaseObject();
		if (this == pBaseObject)
			pTool->EndEditParams();
	}

	__super::Done();
}

void CClipVolumeObject::EntityLinked(const CString& name, GUID targetEntityId)
{
	if (targetEntityId != GUID_NULL)
	{
		CBaseObject* pObject = FindObject(targetEntityId);
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* target = static_cast<CEntityObject*>(pObject);
			CString type = target->GetTypeDescription();

			if ((stricmp(type, "Light") == 0) || (stricmp(type, "EnvironmentLight") == 0) || (stricmp(type, "EnvironmentLightTOD") == 0))
			{
				CEntityScript* pScript = target->GetScript();
				IEntity* pEntity = target->GetIEntity();

				if (pScript && pEntity)
					pScript->CallOnPropertyChange(pEntity);
			}
		}
	}
}

void CClipVolumeObject::BeginEditParams(IEditor* ie, int flags)
{
	if (!m_pEditClipVolumePanel)
	{
		m_pEditClipVolumePanel = new CClipVolumeObjectPanel(this);
		s_nEditClipVolumeRollUpID = GetIEditor()->AddRollUpPage(ROLLUP_OBJECTS, _T("Edit Clipvolume"), m_pEditClipVolumePanel);
	}

	// note: deliberately skipping CEntityObject::BeginEditParams so Script and Entity panels are not shown
	CBaseObject::BeginEditParams(ie, flags);
}

//////////////////////////////////////////////////////////////////////////
void CClipVolumeObject::EndEditParams(IEditor* ie)
{
	CBaseObject::EndEditParams(ie);

	if (s_nEditClipVolumeRollUpID)
		ie->RemoveRollUpPage(ROLLUP_OBJECTS, s_nEditClipVolumeRollUpID);
	s_nEditClipVolumeRollUpID = 0;
	m_pEditClipVolumePanel = NULL;

	UpdateGameResource();
}

CString CClipVolumeObject::GenerateGameFilename()
{
	CString sRealGeomFileName;
#ifdef SEG_WORLD
	sRealGeomFileName = CString("%level%/Brush/clipvolume_") + GuidUtil::ToString(GetId());
#else
	char sId[128];
	itoa(m_nUniqFileIdForExport, sId, 10);
	sRealGeomFileName = CString("%level%/Brush/clipvolume_") + sId;
#endif
	return sRealGeomFileName + ".cgf";
}

