// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Objects/SelectionGroup.h"
#include "Objects/BaseObject.h"
#include "Objects/BrushObject.h"
#include "Objects/CameraObject.h"
#include "Geometry/EdGeometry.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/IAttachment.h>
#include <CrySystem/IProjectManager.h>
#include "Objects/EntityObject.h"
#include "Material/Material.h"
#include "Terrain/TerrainManager.h"
#include "Vegetation/VegetationMap.h"
#include "Vegetation/VegetationObject.h"
#include "ViewManager.h"
#include "ExportManager.h"
#include "OBJExporter.h"
#include "OCMExporter.h"
#include "FBXExporterDialog.h"
#include "RenderViewport.h"
#include "FileDialogs/SystemFileDialog.h"
#include "QT/Widgets/QWaitProgress.h"
#include "Controls/QuestionDialog.h"

// CExportManager
CExportManager::CExportManager() :
	m_isPrecaching(false),
	m_pBaseObj(0),
	m_fScale(100.0f), // this scale is used by CryEngine RC
	m_bAnimationExport(false)
{
	RegisterExporter(new COBJExporter());
	RegisterExporter(new COCMExporter());
}

CExportManager::~CExportManager()
{
	m_data.Clear();
	for (TExporters::iterator ppExporter = m_exporters.begin(); ppExporter != m_exporters.end(); ++ppExporter)
		(*ppExporter)->Release();
}

bool CExportManager::RegisterExporter(IExporter* pExporter)
{
	if (!pExporter)
		return false;

	m_exporters.push_back(pExporter);
	return true;
}

IExporter* CExportManager::GetExporterForExtension(const char* szExtension) const
{
	for (TExporters::const_iterator ppExporter = m_exporters.begin(); ppExporter != m_exporters.end(); ++ppExporter)
	{
		IExporter* pExporter = (*ppExporter);
		if (pExporter)
		{
			if (!stricmp(pExporter->GetExtension(), szExtension))
			{
				return pExporter;
			}
		}
	}
	return nullptr;
}

void CExportManager::AddMesh(SExportObject* pObj, const IIndexedMesh* pIndMesh, Matrix34A* pTm)
{
	if (m_isPrecaching || !pObj)
		return;

	pObj->m_MeshHash = reinterpret_cast<size_t>(pIndMesh);
	IIndexedMesh::SMeshDescription meshDesc;
	pIndMesh->GetMeshDescription(meshDesc);

	// if we have subset of meshes we need to duplicate vertices,
	// keep transformation of submesh,
	// and store new offset for indices
	int newOffsetIndex = pObj->GetVertexCount();

	if (meshDesc.m_nVertCount)
	{
		pObj->m_vertices.reserve(meshDesc.m_nVertCount + newOffsetIndex);
		pObj->m_normals.reserve(meshDesc.m_nVertCount + newOffsetIndex);
		if (meshDesc.m_pColor)
			pObj->m_colors.reserve(meshDesc.m_nVertCount + newOffsetIndex);
	}

	for (int v = 0; v < meshDesc.m_nVertCount; ++v)
	{
		Vec3 n = meshDesc.m_pNorms[v].GetN();
		Vec3 tmp = (meshDesc.m_pVerts ? meshDesc.m_pVerts[v] : meshDesc.m_pVertsF16[v].ToVec3());
		if (pTm)
			tmp = pTm->TransformPoint(tmp);

		pObj->m_vertices.push_back(Export::Vec3ToVector3D(tmp * m_fScale));
		pObj->m_normals.push_back(Export::Vec3ToVector3D(n));
		if (meshDesc.m_pColor)
		{
			Export::Color color;
			ColorB colorB = meshDesc.m_pColor[v].GetRGBA();
			// r and b are swapped in render mesh code.
			color.r = float(colorB.b) / 255.0f;
			color.g = float(colorB.g) / 255.0f;
			color.b = float(colorB.r) / 255.0f;
			color.a = float(colorB.a) / 255.0f;
			pObj->m_colors.push_back(color);
		}
	}

	if (meshDesc.m_nCoorCount)
		pObj->m_texCoords.reserve(meshDesc.m_nCoorCount + newOffsetIndex);

	for (int v = 0; v < meshDesc.m_nCoorCount; ++v)
	{
		Export::UV tc;
		meshDesc.m_pTexCoord[v].ExportTo(tc.u, tc.v);
		tc.v = 1.0f - tc.v;
		pObj->m_texCoords.push_back(tc);
	}

	CMaterial* pMtl = 0;

	if (m_pBaseObj)
		pMtl = (CMaterial*)m_pBaseObj->GetRenderMaterial();

	if (pMtl)
		pObj->SetMaterialName(pMtl->GetFullName());

	if (pIndMesh->GetSubSetCount() && !(pIndMesh->GetSubSetCount() == 1 && pIndMesh->GetSubSet(0).nNumIndices == 0))
	{
		for (int i = 0; i < pIndMesh->GetSubSetCount(); ++i)
		{
			SExportMesh* pMesh = new SExportMesh();

			const SMeshSubset& sms = pIndMesh->GetSubSet(i);
			const vtx_idx* pIndices = &meshDesc.m_pIndices[sms.nFirstIndexId];
			int nTris = sms.nNumIndices / 3;
			pMesh->m_faces.reserve(nTris);
			for (int f = 0; f < nTris; ++f)
			{
				Export::Face face;
				face.idx[0] = *(pIndices++) + newOffsetIndex;
				face.idx[1] = *(pIndices++) + newOffsetIndex;
				face.idx[2] = *(pIndices++) + newOffsetIndex;
				pMesh->m_faces.push_back(face);
			}

			if (pMtl)
			{
				if (pMtl->IsMultiSubMaterial())
				{
					CMaterial* pSubMtl = 0;
					if (sms.nMatID < pMtl->GetSubMaterialCount())
						pSubMtl = pMtl->GetSubMaterial(sms.nMatID);
					pMesh->SetMaterial(pSubMtl, m_pBaseObj);
				}
				else
				{
					pMesh->SetMaterial(pMtl, m_pBaseObj);
				}
			}

			pObj->m_meshes.push_back(pMesh);
		}
	}
	else
	{
		SExportMesh* pMesh = new SExportMesh();
		if (meshDesc.m_nFaceCount == 0 && meshDesc.m_nIndexCount != 0 && meshDesc.m_pIndices != 0)
		{
			const vtx_idx* pIndices = &meshDesc.m_pIndices[0];
			int nTris = meshDesc.m_nIndexCount / 3;
			pMesh->m_faces.reserve(nTris);
			for (int f = 0; f < nTris; ++f)
			{
				Export::Face face;
				face.idx[0] = *(pIndices++) + newOffsetIndex;
				face.idx[1] = *(pIndices++) + newOffsetIndex;
				face.idx[2] = *(pIndices++) + newOffsetIndex;
				pMesh->m_faces.push_back(face);
			}
		}
		else
		{
			pMesh->m_faces.reserve(meshDesc.m_nFaceCount);
			for (int f = 0; f < meshDesc.m_nFaceCount; ++f)
			{
				Export::Face face;
				face.idx[0] = meshDesc.m_pFaces[f].v[0];
				face.idx[1] = meshDesc.m_pFaces[f].v[1];
				face.idx[2] = meshDesc.m_pFaces[f].v[2];
				pMesh->m_faces.push_back(face);
			}
		}

		if (m_pBaseObj && pMtl)
			pMesh->SetMaterial(pMtl, m_pBaseObj);
		pObj->m_meshes.push_back(pMesh);
	}
}

bool CExportManager::AddStatObj(SExportObject* pObj, IStatObj* pStatObj, Matrix34A* pTm)
{
	IIndexedMesh* pIndMesh = 0;

	if (pStatObj->GetSubObjectCount())
	{
		for (int i = 0; i < pStatObj->GetSubObjectCount(); i++)
		{
			IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(i);
			if (pSubObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj)
			{
				pIndMesh = 0;
				if (m_isOccluder)
				{
					if (pSubObj->pStatObj->GetLodObject(2))
						pIndMesh = pSubObj->pStatObj->GetLodObject(2)->GetIndexedMesh(true);
					if (!pIndMesh && pSubObj->pStatObj->GetLodObject(1))
						pIndMesh = pSubObj->pStatObj->GetLodObject(1)->GetIndexedMesh(true);
				}
				if (!pIndMesh)
					pIndMesh = pSubObj->pStatObj->GetIndexedMesh(true);
				if (pIndMesh)
				{
					AddMesh(pObj, pIndMesh, pTm);
				}
			}
		}
	}

	if (!pIndMesh)
	{
		if (m_isOccluder)
		{
			if (pStatObj->GetLodObject(2))
				pIndMesh = pStatObj->GetLodObject(2)->GetIndexedMesh(true);
			if (!pIndMesh && pStatObj->GetLodObject(1))
				pIndMesh = pStatObj->GetLodObject(1)->GetIndexedMesh(true);
		}
		if (!pIndMesh)
			pIndMesh = pStatObj->GetIndexedMesh(true);
		if (pIndMesh)
			AddMesh(pObj, pIndMesh, pTm);
	}

	return true;
}

bool CExportManager::AddCharacter(SExportObject* pObj, ICharacterInstance* pCharacter, Matrix34A* pTm)
{
	if (!pCharacter || m_isOccluder)
		return false;

	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
	ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
	if (pSkeletonPose)
	{
		uint32 nJointCount = rIDefaultSkeleton.GetJointCount();
		for (uint32 i = 0; i < nJointCount; ++i)
		{
			IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(i);
			if (pStatObj)
			{
				const QuatT& quat = pSkeletonPose->GetAbsJointByID(i);
				Matrix34A tm(quat);
				AddStatObj(pObj, pStatObj, &tm);
			}
		}
	}

	IAttachmentManager* AttachmentManager = pCharacter->GetIAttachmentManager();
	if (AttachmentManager)
	{
		uint32 nAttachmentCount = AttachmentManager->GetAttachmentCount();
		for (uint32 i = 0; i < nAttachmentCount; ++i)
		{
			IAttachment* pAttachment = AttachmentManager->GetInterfaceByIndex(i);
			if (pAttachment)
			{
				IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
				if (pAttachmentObject)
				{
					if (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_StatObj)
					{
						IStatObj* pStatObj = pAttachmentObject->GetIStatObj();
						if (pStatObj)
						{
							QuatT quat = pAttachment->GetAttModelRelative();
							Matrix34A tm(quat);
							AddStatObj(pObj, pStatObj, &tm);
						}
					}
					else if (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
					{
					}
					else if (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Skeleton)
					{
						ICharacterInstance* pCharacter = pAttachmentObject->GetICharacterInstance();
						if (pCharacter)
							AddCharacter(pObj, pCharacter);
					}
				}
			}
		}
	}

	// TODO: Add functionality after a code will be fixed on Engine side
	/*
	   IRenderMesh* pRenderMesh = pCharacter->GetRenderMesh();
	   if (pRenderMesh)
	   {
	   // should return indexed mesh if render mesh exists
	   IIndexedMesh* pIndMesh = pRenderMesh->GetIndexedMesh();
	   }
	   /**/
	return true;
}

bool CExportManager::AddMeshes(SExportObject* pObj)
{
	CEdGeometry* pEdGeom = m_pBaseObj->GetGeometry();
	IIndexedMesh* pIndMesh = 0;

	if (m_pBaseObj->GetType() == OBJTYPE_SOLID)
	{
		IStatObj* pStatObj = m_pBaseObj->GetIStatObj();
		if (pStatObj)
			pIndMesh = pStatObj->GetIndexedMesh();
	}

	if (pIndMesh)
	{
		AddMesh(pObj, pIndMesh);
		return true;
	}

	if (pEdGeom)
	{
		size_t idx = 0;
		size_t nextIdx = 0;
		do
		{
			pIndMesh = 0;
			if (m_isOccluder)
			{
				if (pEdGeom->GetIStatObj() && pEdGeom->GetIStatObj()->GetLodObject(2))
					pIndMesh = pEdGeom->GetIStatObj()->GetLodObject(2)->GetIndexedMesh(true);
				if (!pIndMesh && pEdGeom->GetIStatObj() && pEdGeom->GetIStatObj()->GetLodObject(1))
					pIndMesh = pEdGeom->GetIStatObj()->GetLodObject(1)->GetIndexedMesh(true);
			}

			if (!pIndMesh)
			{
				pIndMesh = pEdGeom->GetIndexedMesh(idx);
				nextIdx++;
			}

			if (!pIndMesh)
				break;

			Matrix34 tm;
			pEdGeom->GetTM(&tm, idx);
			Matrix34A objTM = tm;
			AddMesh(pObj, pIndMesh, &objTM);
			idx = nextIdx;
		}
		while (pIndMesh && idx);

		if (idx > 0)
			return true;
	}

	if (m_pBaseObj->GetType() == OBJTYPE_ENTITY)
	{
		CEntityObject* pEntityObject = (CEntityObject*)m_pBaseObj;
		IRenderNode* pEngineNode = pEntityObject->GetEngineNode();
		IEntity* pEntity = pEntityObject->GetIEntity();

		if (pEngineNode)
		{
			if (m_isPrecaching)
			{
				GetIEditorImpl()->Get3DEngine()->PrecacheRenderNode(pEngineNode, 0);
			}
			else
			{
				Matrix34A tm;
				IStatObj* pStatObj = pEngineNode->GetEntityStatObj(0, 0, &tm);
				if (pStatObj)
				{
					Matrix34A objTM = m_pBaseObj->GetWorldTM();
					objTM.Invert();
					tm = objTM * tm;
					AddStatObj(pObj, pStatObj, &tm);
				}

				for (int i = 0; i < pEntity->GetSlotCount(); ++i)
				{
					if (pEntity)
					{
						ICharacterInstance* pCharacter = pEntity->GetCharacter(i);
						if (pCharacter)
							AddCharacter(pObj, pCharacter);
					}
				}
			}
		}
	}

	return true;
}

bool CExportManager::AddObject(CBaseObject* pBaseObj)
{
	if (pBaseObj->GetType() == OBJTYPE_GROUP)
	{
		for (int i = 0; i < pBaseObj->GetChildCount(); i++)
			AddObject(pBaseObj->GetChild(i));
	}

	if (m_isOccluder)
	{
		const ObjectType OT = pBaseObj->GetType();
		IRenderNode* pRenderNode = pBaseObj->GetEngineNode();
		if ((OT == OBJTYPE_BRUSH || OT == OBJTYPE_SOLID) && pRenderNode)
		{
			if ((pRenderNode->GetRndFlags() & ERF_GOOD_OCCLUDER) == 0)
				return false;
		}
		else
		{
			return false;
		}
	}

	m_pBaseObj = pBaseObj;

	if (m_isPrecaching)
	{
		AddMeshes(0);
		return true;
	}

	SExportObject* pObj = new SExportObject(m_pBaseObj->GetName());

	AddPosRotScale(pObj, pBaseObj);
	m_data.m_objects.push_back(pObj);

	m_objectMap[pBaseObj] = int(m_data.m_objects.size() - 1);

	AddMeshes(pObj);
	m_pBaseObj = 0;

	return true;
}

void CExportManager::AddPosRotScale(SExportObject* pObj, const CBaseObject* pBaseObj)
{
	Vec3 pos = pBaseObj->GetPos();
	pObj->pos.x = pos.x * m_fScale;
	pObj->pos.y = pos.y * m_fScale;
	pObj->pos.z = pos.z * m_fScale;

	Quat rot = pBaseObj->GetRotation();
	pObj->rot.v.x = rot.v.x;
	pObj->rot.v.y = rot.v.y;
	pObj->rot.v.z = rot.v.z;
	pObj->rot.w = rot.w;

	Vec3 scale = pBaseObj->GetScale();
	pObj->scale.x = scale.x;
	pObj->scale.y = scale.y;
	pObj->scale.z = scale.z;

	if (pBaseObj->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
	{
		AddEntityData(pObj, eAnimParamType_PositionX, pBaseObj->GetPos().x, 0.0f);
		AddEntityData(pObj, eAnimParamType_PositionY, pBaseObj->GetPos().y, 0.0f);
		AddEntityData(pObj, eAnimParamType_PositionZ, pBaseObj->GetPos().z, 0.0f);
		AddEntityData(pObj, eAnimParamType_RotationX, pBaseObj->GetRotation().v.x, 0.0f);
		AddEntityData(pObj, eAnimParamType_RotationY, pBaseObj->GetRotation().v.y, 0.0f);
		AddEntityData(pObj, eAnimParamType_RotationZ, pBaseObj->GetRotation().v.z, 0.0f);
	}
}

void CExportManager::AddEntityData(SExportObject* pObj, EAnimParamType dataType, const float fValue, const float fTime)
{
	Export::EntityAnimData entityData;
	entityData.dataType = dataType;
	entityData.leftTangent = Export::kTangentDelta;
	entityData.rightTangent = Export::kTangentDelta;
	entityData.rightTangentWeight = 0.0f;
	entityData.leftTangentWeight = 0.0f;
	entityData.keyValue = fValue;
	entityData.keyTime = fTime;
	pObj->m_entityAnimData.push_back(entityData);
}

void CExportManager::SolveHierarchy()
{
	for (TObjectMap::iterator it = m_objectMap.begin(); it != m_objectMap.end(); ++it)
	{
		CBaseObject* pObj = it->first;
		int index = it->second;
		if (pObj && pObj->GetParent())
		{
			CBaseObject* pParent = pObj->GetParent();
			TObjectMap::iterator itFind = m_objectMap.find(pParent);
			if (itFind != m_objectMap.end())
			{
				int indexOfParent = itFind->second;
				if (indexOfParent >= 0 && index >= 0)
					m_data.m_objects[index]->nParent = indexOfParent;
			}
		}
	}

	m_objectMap.clear();
}

bool CExportManager::IsDuplicateObjectBeingAdded(const string& newObjectName)
{
	for (int objectID = 0; objectID < m_data.m_objects.size(); ++objectID)
	{
		if (newObjectName.CompareNoCase(m_data.m_objects[objectID]->name) == 0)
		{
			return true;
		}
	}

	return false;
}

bool CExportManager::AddSelectedObjects()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();

	int numObjects = pSelection->GetCount();
	if (numObjects > m_data.m_objects.size())
		m_data.m_objects.reserve(numObjects + 1); // +1 for terrain

	// First run pipeline to precache geometry
	m_isPrecaching = true;
	for (int i = 0; i < numObjects; i++)
	{
		AddObject(pSelection->GetObject(i));
	}

	GetIEditorImpl()->Get3DEngine()->ProposeContentPrecache();

	// Repeat pipeline to collect geometry
	m_isPrecaching = false;
	for (int i = 0; i < numObjects; i++)
	{
		AddObject(pSelection->GetObject(i));
	}

	return true;
}

bool CExportManager::IsNotChildOfGroup(CBaseObject* pObj)
{
	if (pObj->GetParent())
	{
		CBaseObject* pParentObj = pObj->GetParent();
		while (pParentObj)
		{
			if (pParentObj->GetType() == OBJTYPE_GROUP)
				return false;
			pParentObj = pParentObj->GetParent();
		}
	}

	return true; // not a child
}

bool CExportManager::AddSelectedRegionObjects()
{
	AABB box;
	GetIEditorImpl()->GetSelectedRegion(box);
	if (box.IsEmpty())
		return false;

	std::vector<CBaseObject*> objects;
	GetIEditorImpl()->GetObjectManager()->FindObjectsInAABB(box, objects);

	int numObjects = objects.size();
	if (numObjects > m_data.m_objects.size())
		m_data.m_objects.reserve(numObjects + 1); // +1 for terrain

	// First run pipeline to precache geometry
	m_isPrecaching = true;
	for (size_t i = 0; i < numObjects; ++i)
	{
		if (IsNotChildOfGroup(objects[i])) // skip childs of groups
			AddObject(objects[i]);
	}

	GetIEditorImpl()->Get3DEngine()->ProposeContentPrecache();

	// Repeat pipeline to collect geometry
	m_isPrecaching = false;
	for (size_t i = 0; i < numObjects; ++i)
	{
		if (IsNotChildOfGroup(objects[i])) // skip childs of groups
			AddObject(objects[i]);
	}

	return true;
}

bool CExportManager::AddTerrain()
{
	AABB box;
	GetIEditorImpl()->GetSelectedRegion(box);
	if (box.IsEmpty())
		return false;

	CHeightmap& heightmap = *GetIEditorImpl()->GetHeightmap();
	t_hmap* pHeightmapData = heightmap.GetData();
	ASSERT(pHeightmapData);
	int nHeightmapWidth = heightmap.GetWidth();
	int nHeightmapHeight = heightmap.GetHeight();
	float unitSize = float(heightmap.GetUnitSize());
	float unitSizeScaled = unitSize * m_fScale;

	CRect rc;
	rc.left = box.min.y / unitSize; // x and y swapped in Heightmap
	rc.top = box.min.x / unitSize;
	rc.right = box.max.y / unitSize;
	rc.bottom = box.max.x / unitSize;

	if ((int)rc.bottom - rc.top <= 0 && (int)rc.right - rc.left <= 0)
		return false;

	// Adjust the rectangle to be valid
	if (rc.right < 0)
		rc.right = 0;
	if (rc.left < 0)
		rc.left = 0;
	if (rc.top < 0)
		rc.top = 0;
	if (rc.bottom < 0)
		rc.bottom = 0;

	if (rc.right >= nHeightmapWidth)
		rc.right = nHeightmapWidth - 1;
	if (rc.bottom >= nHeightmapHeight)
		rc.bottom = nHeightmapHeight - 1;

	int nExportWidth = rc.right - rc.left;
	int nExportHeight = rc.bottom - rc.top;

	if (!nExportWidth || !nExportHeight)
		return false;

	SExportObject* pObj = new SExportObject("Sandbox_Terrain");
	m_data.m_objects.push_back(pObj);

	SExportMesh* pMesh = new SExportMesh();

	pObj->m_vertices.reserve((nExportWidth + 1) * (nExportHeight + 1));
	pObj->m_texCoords.reserve((nExportWidth + 1) * (nExportHeight + 1));

	for (int y = rc.top; y <= rc.bottom; ++y)
	{
		for (int x = rc.left; x <= rc.right; ++x)
		{
			Export::Vector3D vec;
			vec.y = float(x) * unitSizeScaled; // x and y swapped in Heightmap
			vec.x = float(y) * unitSizeScaled;
			vec.z = pHeightmapData[x + y * nHeightmapWidth] * m_fScale;
			pObj->m_vertices.push_back(vec);

			Export::UV tc;
			tc.u = float(x) / nExportWidth;
			tc.v = 1.0f - float(y) / nExportHeight;
			pObj->m_texCoords.push_back(tc);
		}
	}

	CWaitProgress progress("Export Terrain");

	pMesh->m_faces.reserve(nExportWidth * nExportWidth * 2); // quad has 2 triangles
	for (int y = 0; y < nExportHeight; ++y)
	{
		for (int x = 0; x < nExportWidth; ++x)
		{
			int nExtendedWidth = nExportWidth + 1;
			Export::Face face;
			face.idx[0] = x + (y) * nExtendedWidth;
			face.idx[1] = x + (y + 1) * nExtendedWidth;
			face.idx[2] = x + 1 + (y) * nExtendedWidth;
			pMesh->m_faces.push_back(face);

			face.idx[0] = x + 1 + (y) * nExtendedWidth;
			face.idx[1] = x + (y + 1) * nExtendedWidth;
			face.idx[2] = x + 1 + (y + 1) * nExtendedWidth;
			pMesh->m_faces.push_back(face);
		}

		if (nExportHeight >= 10 && y % (nExportHeight / 10) == 0 && !progress.Step(100 * y / nExportHeight))
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Export Terrain was skipped by user."));
			return false;
		}
	}

	// Adjust material
	Export::Material& material = pMesh->material;
	material.opacity = 1.0f;
	material.diffuse.r = 1.0f;
	material.diffuse.g = 1.0f;
	material.diffuse.b = 1.0f;
	cry_strcpy(material.name, "Sandbox_Terrain");

	pObj->m_meshes.push_back(pMesh);

	/*
	   // Not sure do we need this info, just keep code from old version
	   // Export Info
	   {
	   char szInfoPath[_MAX_PATH];
	   cry_strcpy(szInfoPath, pszFileName);
	   PathRemoveExtension(szInfoPath);
	   cry_strcat(szInfoPath,".inf");
	   if (CFileUtil::OverwriteFile( szInfoPath ))
	   {
	    FILE *infoFile = fopen( szInfoPath,"wt" );
	    if (infoFile)
	    {
	      fprintf( infoFile,"x=%d,y=%d,width=%d,height=%d",rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top );
	      fclose( infoFile );
	    }
	   }
	   }
	 */

	return true;
}

bool CExportManager::AddVegetation()
{
	if (m_isOccluder)
		return false;
	AABB box;
	GetIEditorImpl()->GetSelectedRegion(box);
	if (box.IsEmpty())
		return false;

	CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	if (!pVegetationMap)
		return false;

	std::vector<CVegetationInstance*> vegetationInstances;
	pVegetationMap->GetObjectInstances(box.min.x, box.min.y, box.max.x, box.max.y, vegetationInstances);

	// Precaching render nodes
	for (std::vector<CVegetationInstance*>::const_iterator ppVegetationInstance = vegetationInstances.begin();
	     ppVegetationInstance != vegetationInstances.end(); ++ppVegetationInstance)
	{
		const CVegetationInstance* pVegetationInstance = *ppVegetationInstance;
		IRenderNode* pRenderNode = pVegetationInstance->pRenderNode;
		if (!pRenderNode)
			continue;
		GetIEditorImpl()->Get3DEngine()->PrecacheRenderNode(pRenderNode, 0);
	}

	GetIEditorImpl()->Get3DEngine()->ProposeContentPrecache();

	int iName = 0;
	for (std::vector<CVegetationInstance*>::const_iterator ppVegetationInstance = vegetationInstances.begin();
	     ppVegetationInstance != vegetationInstances.end(); ++ppVegetationInstance)
	{
		const CVegetationInstance* pVegetationInstance = *ppVegetationInstance;
		IRenderNode* pRenderNode = pVegetationInstance->pRenderNode;
		if (!pRenderNode)
			continue;

		CVegetationObject* pVegetationObject = pVegetationInstance->object;
		IStatObj* pStatObj = pVegetationObject->GetObject();

		string name;
		name.Format("VegetationInstance_%d", iName++);

		SExportObject* pObj = new SExportObject(name);

		Vec3 pos = pVegetationInstance->pos;
		pObj->pos.x = pos.x * m_fScale;
		pObj->pos.y = pos.y * m_fScale;
		pObj->pos.z = pos.z * m_fScale;

		Quat rot = Quat::CreateIdentity();
		Matrix34A mat;
		if (pRenderNode->GetEntityStatObj(0, 0, &mat) && mat.IsOrthonormal())
		{
			rot = Quat(mat);
		}
		else
		{
			float z = pVegetationInstance->angle * g_PI2 / 256.f;
			rot.SetRotationZ(z);
		}
		pObj->rot.v.x = rot.v.x;
		pObj->rot.v.y = rot.v.y;
		pObj->rot.v.z = rot.v.z;
		pObj->rot.w = rot.w;

		Vec3 scale(pVegetationInstance->scale);
		pObj->scale.x = scale.x;
		pObj->scale.y = scale.y;
		pObj->scale.z = scale.z;

		m_data.m_objects.push_back(pObj);

		if (pStatObj)
		{
			AddStatObj(pObj, pStatObj);
		}

		// pVegetationObject->GetMaterial()  // keep for future
	}

	return true;
}

bool CExportManager::AddOcclusionObjects()
{

	std::vector<CBaseObject*> objects, objectsTmp;
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CBrushObject), objects);
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(OBJTYPE_SOLID, objectsTmp);
	objects.insert(objects.end(), objectsTmp.begin(), objectsTmp.end());

	int numObjects = objects.size();
	if (numObjects > m_data.GetObjectCount())
		m_data.m_objects.reserve(numObjects); // +1 for terrain

	//if (numObjects > m_data.m_objects.size())
	//	m_data.m_objects.reserve(numObjects); // +1 for terrain

	// First run pipeline to precache geometry
	m_isPrecaching = true;
	for (size_t i = 0; i < numObjects; ++i)
	{
		if (IsNotChildOfGroup(objects[i])) // skip childs of groups
			AddObject(objects[i]);
	}

	GetIEditorImpl()->Get3DEngine()->ProposeContentPrecache();

	// Repeat pipeline to collect geometry
	m_isPrecaching = false;
	for (size_t i = 0; i < numObjects; ++i)
	{
		if (IsNotChildOfGroup(objects[i])) // skip childs of groups
			AddObject(objects[i]);
	}

	return true;
}

bool CExportManager::ExportToFile(const char* filename, bool bClearDataAfterExport)
{
	bool bRet = false;
	string ext = PathUtil::GetExt(filename);

	if (m_data.GetObjectCount() == 0)
	{
		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("No valid data found for exporter."));
		return false;
	}

	for (int i = 0; i < m_exporters.size(); ++i)
	{
		IExporter* pExporter = m_exporters[i];
		if (!stricmp(ext, pExporter->GetExtension()))
		{
			bRet = pExporter->ExportToFile(filename, &m_data);
			break;
		}
	}

	if (bClearDataAfterExport)
		m_data.Clear();
	return bRet;
}

bool CExportManager::Export(const char* defaultName, const char* defaultExt, const char* defaultPath, bool isSelectedObjects, bool isSelectedRegionObjects, bool isTerrain, bool isOccluder, bool bAnimationExport)
{
	m_bAnimationExport = bAnimationExport;
	m_isOccluder = isOccluder;
	const float OldScale = m_fScale;
	if (isOccluder)
	{
		m_fScale = 1.f;
	}

	m_data.Clear();
	m_objectMap.clear();

	ExtensionFilterVector extensionFilters;
	for (TExporters::iterator ppExporter = m_exporters.begin(); ppExporter != m_exporters.end(); ++ppExporter)
	{
		IExporter* pExporter = (*ppExporter);
		if (pExporter)
		{
			QString ext = pExporter->GetExtension();
			const bool bIsDefault = ext == defaultExt;
			CExtensionFilter filter(QStringLiteral("%1 (%2)").arg(QString(pExporter->GetShortDescription()), ext), ext);
			if (bIsDefault) // the default extension should be the first item in the list, so it's the default export options
			{
				extensionFilters.prepend(filter);
			}
			else
			{
				extensionFilters << filter;
			}
		}
	}

	bool returnRes = false;

	string absoluteDefaultPath = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute(), PathUtil::MakeGamePath(defaultPath));
	const QDir dir = QDir::cleanPath(absoluteDefaultPath.c_str());

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = dir.absolutePath();
	runParams.initialFile = QString("%1.%2").arg(defaultName).arg(defaultExt);
	runParams.extensionFilters = extensionFilters;

	QString filePath = CSystemFileDialog::RunExportFile(runParams, nullptr);

	if (m_bAnimationExport || !filePath.isEmpty())
	{
		CWaitCursor wait;
		if (isSelectedObjects)
			AddSelectedObjects();
		if (isSelectedRegionObjects)
			AddSelectedRegionObjects();

		if (!bAnimationExport)
		{
			SolveHierarchy();
		}

		if (isSelectedRegionObjects)
			AddVegetation();
		if (isTerrain)
			AddTerrain();

		if (isOccluder)
			AddOcclusionObjects();

		// Animation export is done through trackview exporter
		if (!m_bAnimationExport)
		{
			returnRes = ExportToFile(filePath.toUtf8().constData());
		}
	}

	m_fScale = OldScale;

	return returnRes;
}

bool CExportManager::ImportFromFile(const char* filename)
{
	bool bRet = false;
	string ext = PathUtil::GetExt(filename);

	m_data.Clear();

	for (size_t handlerID = 0; handlerID < m_exporters.size(); ++handlerID)
	{
		IExporter* pExporter = m_exporters[handlerID];
		if (!stricmp(ext, pExporter->GetExtension()))
		{
			bRet = pExporter->ImportFromFile(filename, &m_data);
			break;
		}
	}

	return bRet;
}

bool CExportManager::ExportSingleStatObj(IStatObj* pStatObj, const char* filename)
{
	SExportObject* pObj = new SExportObject(PathUtil::GetFileName(filename));
	AddStatObj(pObj, pStatObj);
	m_data.m_objects.push_back(pObj);
	ExportToFile(filename, true);
	return true;
}

