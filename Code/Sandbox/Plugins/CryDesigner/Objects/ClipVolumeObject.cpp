// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ClipVolumeObject.h"
#include "Objects/ShapeObject.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "Tools/ClipVolumeTool.h"
#include "Util/Converter.h"
#include "Util/Display.h"
#include "ToolFactory.h"
#include "Viewport.h"
#include <Cry3DEngine/CGF/IChunkFile.h>
#include "Serialization/Decorators/EditorActionButton.h"
#include "Preferences/ViewportPreferences.h"

namespace Designer
{
REGISTER_CLASS_DESC(ClipVolumeClassDesc)

IMPLEMENT_DYNCREATE(ClipVolumeObject, CEntityObject)

int ClipVolumeObject::s_nGlobalFileClipVolumeID = 0;

ClipVolumeObject::ClipVolumeObject()
{
	m_entityClass = "ClipVolume";
	m_nUniqFileIdForExport = (s_nGlobalFileClipVolumeID++);

	UseMaterialLayersMask(false);
	CBaseObject::SetMaterial("%EDITOR%/Materials/areasolid");

	mv_filled.Set(true);

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_filled, "Filled", functor(*this, &ClipVolumeObject::OnPropertyChanged));
	m_pVarObject->AddVariable(mv_ignoreOutdoorAO, "IgnoreOutdoorAO", functor(*this, &ClipVolumeObject::OnPropertyChanged));
	m_pVarObject->AddVariable(mv_ratioViewDist, "ViewDistRatio", functor(*this, &ClipVolumeObject::OnPropertyChanged));
}

ClipVolumeObject::~ClipVolumeObject()
{
}

void ClipVolumeObject::PostLoad(CObjectArchive& ar)
{
	__super::PostLoad(ar);
	UpdateGameResource();
}

IStatObj* ClipVolumeObject::GetIStatObj()
{
	_smart_ptr<IStatObj> pStatObj;
	if (ModelCompiler* pBaseBrush = GetCompiler())
		pBaseBrush->GetIStatObj(&pStatObj);

	return pStatObj.get();
}

ModelCompiler* ClipVolumeObject::GetCompiler() const
{
	if (!m_pCompiler)
		m_pCompiler = new ModelCompiler(0);
	return m_pCompiler;
}

void ClipVolumeObject::Serialize(CObjectArchive& ar)
{
	__super::Serialize(ar);

	if (ar.bLoading)
	{
		if (!GetModel())
			SetModel(new Model);
		GetModel()->SetFlag(eModelFlag_LeaveFrameAfterPolygonRemoved);

		if (XmlNodeRef brushNode = ar.node->findChild("Brush"))
			GetModel()->Serialize(brushNode, ar.bLoading, ar.bUndo);

		if (ar.bUndo)
		{
			if (GetCompiler())
				GetCompiler()->DeleteAllRenderNodes();
		}

		ApplyPostProcess(GetMainContext());
	}
	else
	{
		if (GetCompiler())
		{
			XmlNodeRef brushNode = ar.node->newChild("Brush");
			GetModel()->Serialize(brushNode, ar.bLoading, ar.bUndo);
		}
	}
}

bool ClipVolumeObject::CreateGameObject()
{
	__super::CreateGameObject();

	if (m_pEntity)
	{
		const auto pClipVolumeProxy = m_pEntity->GetOrCreateComponent<IClipVolumeComponent>();

		string gameFileName = GenerateGameFilename();
		pClipVolumeProxy->SetGeometryFilename(gameFileName.c_str());

		UpdateGameResource();
	}

	return m_pEntity != NULL;
}

void ClipVolumeObject::GetLocalBounds(AABB& bbox)
{
	bbox.Reset();

	if (Model* pModel = GetModel())
		bbox = pModel->GetBoundBox();
}

void ClipVolumeObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	DrawDefault(dc);

	if (!GetDesigner())
	{
		dc.PushMatrix(GetWorldTM());
		Display::DisplayModel(dc, GetModel(), NULL, eShelf_Any, 2, ColorB(0, 0, 255));
		dc.PopMatrix();
	}
}

void ClipVolumeObject::UpdateGameResource()
{
	LOADING_TIME_PROFILE_SECTION
	DynArray<Vec3> meshFaces;

	if (m_pEntity)
	{
		if (const auto pVolume = m_pEntity->GetOrCreateComponent<IClipVolumeComponent>())
		{
			if (IStatObj* pStatObj = GetIStatObj())
			{
				if (IRenderMesh* pRenderMesh = pStatObj->GetRenderMesh())
				{
					pRenderMesh->LockForThreadAccess();
					if (pRenderMesh->GetIndicesCount() && pRenderMesh->GetVerticesCount())
					{
						meshFaces.reserve(pRenderMesh->GetIndicesCount());

						int posStride;
						const uint8* pPositions = (uint8*)pRenderMesh->GetPosPtr(posStride, FSL_READ);
						const vtx_idx* pIndices = pRenderMesh->GetIndexPtr(FSL_READ);

						TRenderChunkArray& Chunks = pRenderMesh->GetChunks();
						for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
						{
							CRenderChunk* pChunk = &Chunks[nChunkId];
							if (!(pChunk->m_nMatFlags & MTL_FLAG_NODRAW))
							{
								for (uint i = pChunk->nFirstIndexId; i < pChunk->nFirstIndexId + pChunk->nNumIndices; ++i)
									meshFaces.push_back(*alias_cast<Vec3*>(pPositions + pIndices[i] * posStride));
							}
						}
					}

					pRenderMesh->UnlockStream(VSF_GENERAL);
					pRenderMesh->UnLockForThreadAccess();

					pVolume->UpdateRenderMesh(pStatObj->GetRenderMesh(), meshFaces);
				}
			}
		}
	}

	UpdateCollisionData(meshFaces);
}

void ClipVolumeObject::UpdateCollisionData(const DynArray<Vec3>& meshFaces)
{
	struct EdgeCompare
	{
		enum ComparisonResult
		{
			eResultLess = 0,
			eResultEqual,
			eResultGreater
		};

		ComparisonResult pointCompare(const Vec3& a, const Vec3& b) const
		{
			for (int i = 0; i < 3; ++i)
			{
				if (a[i] < b[i]) return eResultLess;
				else if (a[i] > b[i]) return eResultGreater;
			}

			return eResultEqual;
		}

		bool operator()(const Lineseg& a, const Lineseg& b) const
		{
			ComparisonResult cmpResult = pointCompare(a.start, b.start);
			if (cmpResult == eResultEqual)
				cmpResult = pointCompare(a.end, b.end);

			return cmpResult == eResultLess;
		}
	};

	std::set<Lineseg, EdgeCompare> processedEdges;

	for (int i = 0; i < meshFaces.size(); i += 3)
	{
		for (int e = 0; e < 3; ++e)
		{
			Lineseg edge = Lineseg(
			  meshFaces[i + e],
			  meshFaces[i + (e + 1) % 3]
			  );

			if (EdgeCompare().pointCompare(edge.start, edge.end) == EdgeCompare::eResultGreater)
				std::swap(edge.start, edge.end);

			if (processedEdges.find(edge) == processedEdges.end())
				processedEdges.insert(edge);
		}
	}

	m_MeshEdges.clear();
	m_MeshEdges.insert(m_MeshEdges.begin(), processedEdges.begin(), processedEdges.end());
}

bool ClipVolumeObject::HitTest(HitContext& hc)
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

bool ClipVolumeObject::Init(CBaseObject* prev, const string& file)
{
	bool res = __super::Init(prev, file);
	SetColor(RGB(0, 0, 255));

	if (m_pEntity)
		m_pEntity->CreateProxy(ENTITY_PROXY_CLIPVOLUME);

	if (prev)
	{
		ClipVolumeObject* pVolume = static_cast<ClipVolumeObject*>(prev);
		ModelCompiler* pCompiler = pVolume->GetCompiler();
		Model* pModel = pVolume->GetModel();

		if (pCompiler && pModel)
		{
			SetCompiler(new ModelCompiler(*pCompiler));
			SetModel(new Model(*pModel));
			ApplyPostProcess(GetMainContext());
		}
	}

	if (ModelCompiler* pCompiler = GetCompiler())
		pCompiler->RemoveFlags(eCompiler_Physicalize | eCompiler_CastShadow);

	return res;
}

void ClipVolumeObject::EntityLinked(const string& name, CryGUID targetEntityId)
{
	if (targetEntityId != CryGUID::Null())
	{
		CBaseObject* pObject = FindObject(targetEntityId);
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* target = static_cast<CEntityObject*>(pObject);
			string type = target->GetTypeDescription();

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

void ClipVolumeObject::LoadFromCGF()
{
	string filename = "";
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_GEOMETRY, filename))
	{
		if (_smart_ptr<IStatObj> pStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(filename.GetString(), NULL, NULL, false))
		{
			_smart_ptr<Model> pNewModel = new Model();
			if (Converter::ConvertMeshToBrushDesigner(pStatObj->GetIndexedMesh(true), pNewModel))
			{
				GetIEditor()->GetIUndoManager()->Begin();
				GetModel()->RecordUndo("ClipVolume: Load CGF", this);
				SetModel(pNewModel);
				ApplyPostProcess(GetMainContext());
				GetIEditor()->GetIUndoManager()->Accept("ClipVolume: Load CGF");
			}
		}
	}
}

void ClipVolumeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<ClipVolumeObject>("Clip Volume", [](ClipVolumeObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_filled, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_ignoreOutdoorAO, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_ratioViewDist, ar);

		if (ar.openBlock("operators", "Operators"))
		{
			if (ar.openBlock("volume", "Volume"))
			{
				ar(Serialization::ActionButton([=]
				{
					GetIEditor()->ExecuteCommand("general.open_pane 'Modeling'");

					GetIEditor()->SetEditTool("EditTool.ClipVolumeTool", false);
				}),
					"edit", "^Edit");

				ar(Serialization::ActionButton(std::bind(&ClipVolumeObject::LoadFromCGF, pObject)), "load_cgf", "^Load CGF");
				ar.closeBlock();
			}
			ar.closeBlock();
		}
	});
}

void ClipVolumeObject::OnPropertyChanged(IVariable* var)
{
	if (var == &mv_ignoreOutdoorAO || var == &mv_ratioViewDist)
	{
		if (!m_pEntity)
			return;
		if (const auto pVolume = m_pEntity->GetOrCreateComponent<IClipVolumeComponent>())
			pVolume->SetProperties(mv_ignoreOutdoorAO, mv_ratioViewDist);
	}
	else if (var == &mv_filled)
	{
		UpdateHiddenIStatObjState();
	}
}

std::vector<EDesignerTool> ClipVolumeObject::GetIncompatibleSubtools()
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


string ClipVolumeObject::GenerateGameFilename()
{
	string sRealGeomFileName;
	char sId[128];
	itoa(m_nUniqFileIdForExport, sId, 10);
	sRealGeomFileName = string("%level%/Brush/clipvolume_") + sId;
	return sRealGeomFileName + ".cgf";
}

void ClipVolumeObject::ExportBspTree(IChunkFile* pChunkFile) const
{
	if (m_pEntity)
	{
		if (IClipVolumeComponent* pClipVolumeProxy = static_cast<IClipVolumeComponent*>(m_pEntity->GetProxy(ENTITY_PROXY_CLIPVOLUME)))
		{
			if (IBSPTree3D* pBspTree = pClipVolumeProxy->GetBspTree())
			{
				size_t nBufferSize = pBspTree->WriteToBuffer(NULL);

				void* pTreeData = new uint8[nBufferSize];
				pBspTree->WriteToBuffer(pTreeData);

				pChunkFile->AddChunk(ChunkType_BspTreeData, 0x1, eEndianness_Little, pTreeData, nBufferSize);

				delete[] pTreeData;
			}
		}
	}
}

void ClipVolumeObject::OnEvent(ObjectEvent event)
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

bool ClipVolumeObject::IsHiddenByOption()
{
	bool visible = false;
	mv_filled.Get(visible);
	return !(visible && GetIEditor()->IsHelpersDisplayed());
}

void ClipVolumeObject::SetHidden(bool bHidden, bool bAnimated)
{
	__super::SetHidden(bHidden, bAnimated);
	UpdateHiddenIStatObjState();
}

void ClipVolumeObject::InvalidateTM(int nWhyFlags)
{
	__super::InvalidateTM(nWhyFlags);
	UpdateEngineNode();
	UpdateGameResource();
}

}

