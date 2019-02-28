// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <Cry3DEngine/CGF/CryHeaders.h>  // EPhysicsGeomType
#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_Math.h>
#include <CryRenderer/VertexFormats.h>
#include <Cry3DEngine/CGF/CGFContent.h>

#include "IRCLog.h"
#include "IAssetManager.h"
#include "FileUtil.h"
#include "Export/MeshUtils.h"
#include "StringHelpers.h"
#include "Util.h"

#include "../CryEngine/Cry3DEngine/CGF/CGFSaver.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkFile.h"
#include "../CryEngine/Cry3DEngine/MeshCompiler/TransformHelpers.h"
#include "../StaticObjectCompiler.h"
#include "../StatCGFPhysicalize.h"
#include "CGA/SkeletonHelpers.h"
#include "CGF/CAFSaver.h"
#include "CGF/SkinSaver.h"
#include "CGF/CgfUtil.h"
#include "MathHelpers.h"  // MathHelpers::AutoFloatingPointExceptions
#include "SkeletonInfo.h"
#include "CGA/AnimationInfoLoader.h"
#include "CGA/AnimationLoader.h"

#include "XMLWriter.h"

#include "CryFbxScene.h"
#include "FbxConverter.h"
#include "ImportRequest.h"
#include "Scene.h"
#include "ImportRequest.h"
#include "LodGenerator/AutoGenerator.h"
#include "Decompose.h"

namespace FbxConverter_Private
{

// CGF is in cm 
static const float s_metersToCentimeters = 100.0f;

class CExportMaterial
{	
public:
	const string m_materialName;
	CMaterialCGF* m_pCgfMaterial;
	bool m_bOwnMaterial;

	std::vector<int> m_matIds;
	std::vector<int> m_renderMatIds;
	std::vector<int> m_physicsMatIds;

public:
	CExportMaterial(const string& materialName)
		: m_materialName(materialName)
		, m_pCgfMaterial(0)
		, m_bOwnMaterial(false)		
	{
	}

	~CExportMaterial()
	{
		if (m_pCgfMaterial && m_bOwnMaterial)
		{
			delete m_pCgfMaterial;
		}
		// FIXME: It's not clear who is expected to delete m_pCgfMaterial->subMaterials[i]
	}

	// For every IScene's material exportMatId_xxx[i] stores either < 0 (for unused/deletable materials) or final/exported mat id (for used materials)
	bool PrepareMaterials(
		Scene::IScene* pScene,
		const CImportRequest& ir,
		IResourceCompiler* pRC)
	{
		m_matIds.clear();
		m_renderMatIds.clear();
		m_physicsMatIds.clear();

		// Init materials as "unused/deletable" (-1)
		m_matIds.resize(pScene->GetMaterialCount(), -1);

		// Set final (exported) mat id for every used material
		// Traverse the materials in reverse order, so that the dummy material does not
		// override physicalization setting of its sub-material (dummy material has maximum index).
		for (int i = pScene->GetMaterialCount() - 1; i >= 0; --i)
		{
			const Scene::SMaterial* const pSceneMat = pScene->GetMaterial(i);
			if (!pSceneMat)
			{
				RCLogError("Internal error in material handling code");
				return false;
			}

			const CImportRequest::SMaterialInfo* const pMatInfo = ir.FindMaterialInfoByName(pSceneMat->name);
			if (pMatInfo)
			{
				m_matIds[i] = addMaterial(pMatInfo->finalSubmatIndex, pMatInfo->sourceName, pMatInfo->physicalization);
			}
			else if (ir.materials.empty())
			{
				m_matIds[i] = addMaterial(i, pSceneMat->name, PHYS_GEOM_TYPE_DEFAULT);
			}
		}

		if (!m_pCgfMaterial)
		{
			RCLogError("Nothing to export: all materials are marked as 'delete'");
			return false;
		}

		if ((int)m_pCgfMaterial->subMaterials.size() > MAX_SUB_MATERIALS)
		{
			RCLogError("Internal error: too many materials: %i (max is %i)", (int)m_pCgfMaterial->subMaterials.size(), (int)MAX_SUB_MATERIALS);
			return false;
		}

		m_renderMatIds = m_matIds;
		m_physicsMatIds = m_matIds;

		for (size_t i = 0; i < m_matIds.size(); ++i)
		{
			if (m_matIds[i] < 0)
			{
				continue;
			}
			switch (m_pCgfMaterial->subMaterials[m_matIds[i]]->nPhysicalizeType)
			{
			case PHYS_GEOM_TYPE_NONE:
				m_physicsMatIds[i] = -1;
				break;
			case PHYS_GEOM_TYPE_DEFAULT:
				break;
			case PHYS_GEOM_TYPE_NO_COLLIDE:
			case PHYS_GEOM_TYPE_OBSTRUCT:
			case PHYS_GEOM_TYPE_DEFAULT_PROXY:
				m_renderMatIds[i] = -1;
				break;
			default:
				assert(0);
				m_renderMatIds[i] = -1;
				m_physicsMatIds[i] = -1;
				break;
			}
		}

		// Debug printing of material mapping
		if (pRC->GetVerbosityLevel() > 1)
		{
			RCLog("Mapping %i scene material(s) to %d final submaterial(s):", (int)m_matIds.size(), (int)m_pCgfMaterial->subMaterials.size());
			for (size_t i = 0; i < m_matIds.size(); ++i)
			{
				RCLog("\t[%i] render:%i phys:%i '%s'", (int)i, m_renderMatIds[i], m_physicsMatIds[i], pScene->GetMaterial(i)->name.c_str());
			}
		}

		return true;
	}

protected:
	int addMaterial(int submatIndex, const char* name, EPhysicsGeomType physicalization)
	{
		if (submatIndex < 0 || submatIndex >= MAX_SUB_MATERIALS)
		{
			return -1;
		}

		if (!m_pCgfMaterial)
		{
			m_pCgfMaterial = new CMaterialCGF();
			m_bOwnMaterial = true;
			cry_strcpy(m_pCgfMaterial->name, m_materialName.c_str());
			m_pCgfMaterial->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
		}

		while (submatIndex >= m_pCgfMaterial->subMaterials.size())
		{
			CMaterialCGF* const p = new CMaterialCGF();
			cry_strcpy(p->name, "-unused-");
			p->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			m_pCgfMaterial->subMaterials.push_back(p);
		}

		cry_strcpy(m_pCgfMaterial->subMaterials[submatIndex]->name, name);
		m_pCgfMaterial->subMaterials[submatIndex]->nPhysicalizeType = physicalization;

		return submatIndex;
	}
};

bool AppendMesh(
	MeshUtils::Mesh& dstMesh, 
	const MeshUtils::Mesh& srcMesh, 
	const int submatCount, 
	const std::vector<int>& remapMatIds, 
	const std::vector<int>& mapSceneNodesToBoneId,
	const Matrix34& transform)
{
	MeshUtils::Mesh mesh;
	mesh = srcMesh;

	if (submatCount <= 0 || submatCount > MAX_SUB_MATERIALS)
	{
		RCLogError("Internal error: incorrect submaterial count detected in %s (%i)", __FUNCTION__, submatCount);
		return false;
	}

	// Remapping per-face material ids as requested, and deleting faces that use unused/deleted materials (remapped mat id < 0).
	for (int i = 0; i < mesh.GetFaceCount(); ++i)
	{
		const int matId = mesh.m_faceMatIds[i];
		if (matId < 0 || matId >= (int)remapMatIds.size())
		{
			RCLogError("Internal error: incorrect material data detected in %s", __FUNCTION__);
			return false;
		}
		const int newMatId = remapMatIds[matId];
		if (newMatId >= submatCount)
		{
			RCLogError("Internal error: incorrect remapping data detected in %s (newMatId: %i, max possible id is %i)", __FUNCTION__, newMatId, submatCount);
			return false;
		}

		if (newMatId < 0)
		{
			// Delete face
			const size_t last = mesh.m_faceMatIds.size() - 1;
			mesh.m_faceMatIds[i] = mesh.m_faceMatIds[last];
			mesh.m_faces[i] = mesh.m_faces[last];
			mesh.m_faceMatIds.resize(last);
			mesh.m_faces.resize(last);
			--i;
		}
		else
		{
			mesh.m_faceMatIds[i] = newMatId;
		}
	}

	// Remap bone links ids as requested.
	if (!mapSceneNodesToBoneId.empty())
	{
		for (auto& vertex : mesh.m_links)
		{
			for (auto& link : vertex.links)
			{
				link.boneId = mapSceneNodesToBoneId[link.boneId];
			}
		}
	}

	// Cleaning
	{
		for (size_t i = 0, n = mesh.m_links.size(); i < n; ++i)
		{
			const int maxBoneLinkCount = 8;  // TODO: pass it in parameters
			const char* const err = mesh.m_links[i].Normalize(MeshUtils::VertexLinks::eSort_ByWeight, 0.0f, maxBoneLinkCount);
			if (err)
			{
				RCLogWarning("%s: Failed to process vertex bone linking: %s", __FUNCTION__, err);
				return false;
			}
		}		

		mesh.RemoveDegradedFaces();

		// Prevent sharing materials by vertices (this call might create new vertices)
		mesh.SetVertexMaterialIdsFromFaceMaterialIds();

		mesh.ComputeVertexRemapping();

		// Delete duplicate vertices (note that mesh.m_vertexNewToOld[] and mesh.m_vertexOldToNew[] are not modified)
		mesh.RemoveVerticesByUsingComputedRemapping();

		mesh.m_vertexMatIds.clear();
		mesh.m_vertexOldToNew.clear();
		mesh.m_vertexNewToOld.clear();
	}

	if (mesh.GetFaceCount() <= 0)
	{
		return true;
	}

	// Transforming positions and normals
	{
		const Matrix33 transformNormals = Matrix33(transform).GetInverted().GetTransposed();
		for (int i = 0, n = mesh.GetVertexCount(); i < n; ++i)
		{
			mesh.m_positions[i] = transform.TransformPoint(mesh.m_positions[i]);
			mesh.m_normals[i] = transformNormals.TransformVector(mesh.m_normals[i]).normalize();
		}

		for (auto& morphTarget : mesh.m_morphTargets)
		{
			for (int i = 0, n = mesh.GetVertexCount(); i < n; ++i)
			{
				morphTarget.positions[i] = transform.TransformPoint(morphTarget.positions[i]);
			}
		}
	}

	// Validating
	{
		const char* const err = mesh.Validate();
		if (err)
		{
			RCLogWarning("%s: Failed: %s", __FUNCTION__, err);
			return false;
		}
	}

	// Appending
	if (!dstMesh.Append(mesh))
	{
		RCLogError("%s: Cannot merge meshes", __FUNCTION__);
		return false;
	}

	return true;
}

bool AppendMesh(
	MeshUtils::Mesh& dstMesh,
	const MeshUtils::Mesh& srcMesh,
	const int submatCount,
	const std::vector<int>& remapMatIds,
	const Matrix34& transform)
{
	std::vector<int> doNotRemapBones;

	return AppendMesh(
		dstMesh,
		srcMesh,
		submatCount,
		remapMatIds,
		doNotRemapBones,
		transform);
}


// Based on CLoaderCGF::LoadGeomChunk() and ColladaLoader.cpp's CreateCMeshFromMeshUtilsMesh()
CMesh* CreateCMesh(const MeshUtils::Mesh& a_mesh, const CMaterialCGF& mat)
{
	MeshUtils::Mesh mesh;
	mesh = a_mesh;

	const int submatCount = (int)mat.subMaterials.size();
	if (submatCount <= 0 || submatCount > MAX_SUB_MATERIALS)
	{
		RCLogError("Internal error: incorrect submaterial count detected in %s (%i)", __FUNCTION__, submatCount);
		return nullptr;
	}

	// Validation
	{
		const char* const err = mesh.Validate();
		if (err)
		{
			RCLogWarning("%s: Failed: %s", __FUNCTION__, err);
			return nullptr;
		}
	}

	// Creating and filling CMesh

	const int numFaces = mesh.GetFaceCount();
	const int numVertices = mesh.GetVertexCount();
	const bool hasTexCoords = !mesh.m_texCoords.empty();
	const bool hasColor = !mesh.m_colors.empty();
	const bool hasBoneMaps = !mesh.m_links.empty();

	CMesh* const pMesh = new CMesh();

	pMesh->SetFaceCount(numFaces);
	pMesh->SetVertexCount(numVertices);
	pMesh->ReallocStream(CMesh::TOPOLOGY_IDS, numVertices);
	pMesh->ReallocStream(CMesh::TEXCOORDS, numVertices);

	if (hasColor)
	{
		pMesh->ReallocStream(CMesh::COLORS_0, numVertices);
	}

	bool hasExtraWeights = false;
	if (hasBoneMaps)
	{
		for (int i = 0; i < numVertices; ++i)
		{
			if (mesh.m_links[i].links.size() > 4)
			{
				hasExtraWeights = true;
				break;
			}
		}

		pMesh->ReallocStream(CMesh::BONEMAPPING, numVertices);
		if (hasExtraWeights)
		{
			pMesh->ReallocStream(CMesh::EXTRABONEMAPPING, numVertices);
		}
	}

	pMesh->m_bbox.Reset();

	for (int i = 0; i < numVertices; ++i)
	{
		pMesh->m_pTopologyIds[i] = mesh.m_topologyIds[i];

		pMesh->m_pPositions[i] = mesh.m_positions[i];
		pMesh->m_bbox.Add(mesh.m_positions[i]);

		pMesh->m_pNorms[i] = SMeshNormal(mesh.m_normals[i]);

		if (hasTexCoords)
		{
			pMesh->m_pTexCoord[i] = SMeshTexCoord(mesh.m_texCoords[i].x, mesh.m_texCoords[i].y);
		}
		else
		{
			pMesh->m_pTexCoord[i] = SMeshTexCoord(0.0f, 0.0f);
		}

		if (hasColor)
		{
			pMesh->m_pColor0[i] = SMeshColor(
				mesh.m_colors[i].r,
				mesh.m_colors[i].g,
				mesh.m_colors[i].b,
				mesh.m_alphas[i]);
		}

		if (hasBoneMaps)
		{
			const MeshUtils::VertexLinks& inLinks = mesh.m_links[i];

			for (size_t b = 0; b < 4; ++b)
			{
				if (b < inLinks.links.size())
				{
					pMesh->m_pBoneMapping[i].weights[b] = (int)Util::getClamped(255.0f * inLinks.links[b].weight, 0.0f, 255.0f);
					pMesh->m_pBoneMapping[i].boneIds[b] = inLinks.links[b].boneId;
				}
			}

			if (hasExtraWeights)
			{
				for (size_t b = 4; b < 8; ++b)
				{
					if (b < inLinks.links.size())
					{
						pMesh->m_pExtraBoneMapping[i].weights[b - 4] = (int)Util::getClamped(255.0f * inLinks.links[b].weight, 0.0f, 255.0f);
						pMesh->m_pExtraBoneMapping[i].boneIds[b - 4] = inLinks.links[b].boneId;
					}
				}
			}
		}		
	}

	// Setting faces and subsets.
	// Note that in a subset we set .nMatID and .nPhysicalizeType only, other
	// members are not set - it's enough for MeshCompiler (called later)
	// to work properly. See SetupMeshSubsets() functions calls.
	// 
	// Note that we create one subset for each submaterial of 'mat' even for
	// submaterials that are not used in the mesh. It's needed because 
	// MakeMergedCGF() (that is possibly called later) calls CMesh.Append() and
	// these two functions are not smart enough to handle it properly otherwise.
	// In any case we'll have no waste at the end - our mesh compiler & optimizer
	// remove unused/empty subsets.

	pMesh->m_subsets.clear();
	pMesh->m_subsets.resize(submatCount);

	for (int i = 0; i < submatCount; ++i)
	{
		pMesh->m_subsets[i].nMatID = i;
		pMesh->m_subsets[i].nPhysicalizeType = mat.subMaterials[i]->nPhysicalizeType;
	}

	for (int i = 0; i < numFaces; ++i)
	{
		const int subsetIdx = mesh.m_faceMatIds[i];
		assert(subsetIdx >= 0 && subsetIdx < submatCount);
		pMesh->m_pFaces[i].nSubset = subsetIdx;

		for (int j = 0; j < 3; ++j)
		{
			const int vIdx = mesh.m_faces[i].vertexIndex[j];
			assert(vIdx >= 0 && vIdx < numVertices);
			pMesh->m_pFaces[i].v[j] = vIdx;
		}
	}	

	return pMesh;
}

const string GetNodePathAsString(
	const std::vector<string>& path)
{
	string s = "[ ";
	for (size_t i = 0, n = path.size(); i < n; ++i)
	{
		if (i > 0)
		{
			s += ", ";
		}
		s += "\"";
		s += path[i];
		s += "\"";
	}
	s += " ]";
	return s;
}

void LogNodes(const char* szHeader, const CContentCGF& cgf, IResourceCompiler* pRC)
{
	if (pRC->GetVerbosityLevel() <= 1)
	{
		return;
	}

	RCLog("%s", szHeader);

	for (int i = 0; i < cgf.GetNodeCount(); ++i)
	{
		const CNodeCGF* const p = cgf.GetNode(i);

		string s = StringHelpers::Format("CgfNode[%d]: '%s'", i, p->name);

		if (p->pParent)
		{
			s += StringHelpers::Format(" -> '%s'", p->pParent->name);
		}

		switch (p->type)
		{
		case CNodeCGF::NODE_MESH:
			s += StringHelpers::Format("  MESH(#v:%d)", p->pMesh->GetVertexCount());
			break;
		case CNodeCGF::NODE_HELPER:
			switch (p->helperType)
			{
			case HP_POINT:
				s += "  HELPER(POINT)";
				break;
			case HP_DUMMY:
				s += "  HELPER(DUMMY)";
				break;
			case HP_XREF:
				s += "  HELPER(XREF)";
				break;
			case HP_CAMERA:
				s += "  HELPER(CAMERA)";
				break;
			case HP_GEOMETRY:
				s += "  HELPER(GEOMETRY)";
				break;
			default:
				s += "  HELPER(<UNKNOWN>)";
				break;
			}
			break;
		case CNodeCGF::NODE_LIGHT:
			s += "  LIGHT";
			break;
		default:
			s += "  <UNKNOWN>";
			break;
		}

		if (p->pMesh)
		{
			s += StringHelpers::Format("  mesh(#v:%d)", p->pMesh->GetVertexCount());
		}

		RCLog("%s", s.c_str());
	}
}

class CSceneSaver
{
public:
	CSceneSaver(const ConvertContext* pContext, Scene::IScene* pScene, const Matrix34& sceneWorldToCryWorldTM)
		: m_pContext(pContext)
		, m_pRc(pContext->pRC)
		, m_pScene(pScene)
		, m_sceneWorldToCryWorldTM(sceneWorldToCryWorldTM)
	{
		assert(pContext);
		assert(m_pRc);
		assert(m_pScene);
	}

	virtual bool Save(const string& outputCgfFilename, const CImportRequest& ir) = 0;

	virtual ~CSceneSaver() {}

protected:
	// Compile and write resulting CGF
	bool SaveToCgf(
		const CImportRequest& ir,
		CContentCGF& cgf,
		bool bAsIsScene)
	{
		// _EM_INVALID is used to avoid Floating Point Exception inside CryPhysics
		MathHelpers::AutoFloatingPointExceptions autoFpe(~(_EM_INEXACT | _EM_UNDERFLOW | _EM_INVALID));

		// set bWantF32Vertices
		{
			const char* const optionName = "vertexPositionFormat";
			const string s = m_pContext->config->GetAsString(optionName, "f32", "f32");

			if (StringHelpers::EqualsIgnoreCase(s, "f32"))
			{
				cgf.GetExportInfo()->bWantF32Vertices = true;
			}
			else if (StringHelpers::EqualsIgnoreCase(s, "f16"))
			{
				cgf.GetExportInfo()->bWantF32Vertices = false;
			}
			else if (StringHelpers::EqualsIgnoreCase(s, "exporter"))
			{
				// Do nothing;
			}
			else
			{
				RCLogError("Unknown value of '%s': '%s'. Valid values are: 'f32', 'f16', 'exporter'.", optionName, s.c_str());
				return false;
			}
		}
		bool bStorePositionsAsF16 = !cgf.GetExportInfo()->bWantF32Vertices;

		bool bStoreIndicesAsU16 = false;
		{
			const char* const optionName = "vertexIndexFormat";
			const string s = m_pContext->config->GetAsString(optionName, "u32", "u32");

			if (StringHelpers::EqualsIgnoreCase(s, "u32"))
			{
				bStoreIndicesAsU16 = false;
			}
			else if (StringHelpers::EqualsIgnoreCase(s, "u16"))
			{
				bStoreIndicesAsU16 = true;
			}
			else
			{
				RCLogError("Unknown value of '%s': '%s'. Valid values are: 'u32', 'u16'.", optionName, s.c_str());
				return false;
			}
		}

		LogNodes("Input cgf:", cgf, m_pRc);

		CPhysicsInterface physicsInterface;
		CStaticObjectCompiler compiler(&physicsInterface, false);
		CContentCGF* const pCompiledCGF = compiler.MakeCompiledCGF(&cgf, !bAsIsScene, true);
		if (!pCompiledCGF)
		{
			return false;
		}

		CreateLod(*pCompiledCGF, ir);

		LogNodes("Compiled cgf:", *pCompiledCGF, m_pRc);

		CChunkFile chunkFile;
		CSaverCGF cgfSaver(chunkFile);

		const bool bNeedEndianSwap = false;
		const bool bUseQtangents = m_pContext->config->GetAsBool("qtangents", false, true);

		cgfSaver.SaveContent(pCompiledCGF, bNeedEndianSwap, bStorePositionsAsF16, bUseQtangents, bStoreIndicesAsU16);

		chunkFile.AddChunk(ChunkType_ImportSettings, 0, eEndianness_Native, ir.jsonData.data(), (int)ir.jsonData.size());
	
		return CgfUtil::WriteTempRename(chunkFile, cgf.GetFilename());
	}

	bool CreateLod(CContentCGF& cgf, const CImportRequest& ir)
	{
		// Create Lod
		RCLog("Auto create Lod Path: %s", cgf.GetFilename());
		LODGenerator::CAutoGenerator autoGenerator;

		RCLog("Init AutoGenerator");
		if (!autoGenerator.Init(cgf, ir))
			return false;

		RCLog("AutoGenerator prepare");
		if (!autoGenerator.Prepare())
			return false;

		RCLog("AutoGenerator Generate All Lod");
		if (!autoGenerator.GenerateAllLod())
			return false;

		return true;
	}

	int FindMatchingNodeInScene(const std::vector<string>& path) const
	{
		int foundNode = -1;

		std::vector<string> candidatePath;
		for (int i = 0, n = m_pScene->GetNodeCount(); i < n; ++i)
		{
			Scene::GetPath(candidatePath, m_pScene, i);

			if (path.size() != candidatePath.size())
			{
				continue;
			}

			bool bMatched = true;
			for (int j = (int)path.size() - 1; j >= 0; --j)
			{
				if (!StringHelpers::Equals(path[j], candidatePath[j]))
				{
					bMatched = false;
					break;
				}
			}
			if (bMatched)
			{
				return i;
			}
		}
		return -1;
	}

protected:
	const ConvertContext* m_pContext;
	IResourceCompiler* m_pRc;
	Scene::IScene* m_pScene;
	Matrix34 m_sceneWorldToCryWorldTM;  // orthonormal
};

void FlipTextureCoordinatesHorizontally(CMesh* pMesh)
{
	CRY_ASSERT(pMesh);
	SMeshTexCoord* const pTexCoords = pMesh->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);
	if (!pTexCoords)
	{
		return;
	}

	const int vertexCount = pMesh->GetVertexCount();
	for (int i = 0; i < vertexCount; ++i)
	{
		float u, v;
		pTexCoords[i].ExportTo(u, v);
		pTexCoords[i] = SMeshTexCoord(u, 1.0f - v);
	}
}

class CAsIsResultingScene : public CSceneSaver
{
	enum class ESceneType
	{
		Static,
		Skeleton,
		Skin,
		Animation
	};

	struct NodeInfo
	{
		NodeInfo() 
			: pName(nullptr)
			, pProperties(nullptr)
			, parentNode(-1)
			, sourceSceneNode(-1)
			, lod(0)
			, worldOriginal(IDENTITY)
			, world(IDENTITY)
			, local(IDENTITY)
			, geometryOffset(IDENTITY)
			, bPhysicsProxy(false)
			, bIgnoreMesh(false)
		{
		}

		const string* pName;
		const string* pProperties;
		int parentNode;  // index of parent NodeInfo node (or -1)
		int sourceSceneNode;  // index for Scene::IScene::GetNode() call
		int lod;			  // index of lod or 0
		Matrix34 worldOriginal;  // non-orthonormal
		Matrix34 world; // orthonormal
		Matrix34 local;
		Matrix34 geometryOffset; // the geometry offset to a node. It is never inherited by the children.
		bool bPhysicsProxy; // true if it is a physics proxy. 
		bool bIgnoreMesh; // do not export the node mesh.
	};

	std::vector<NodeInfo> m_nodes;
	float m_scale; // includes user scale and conversion to meters.

public:

	CAsIsResultingScene(const ConvertContext* pContext, Scene::IScene* pScene, const Matrix34& sceneWorldToCryWorldTM, const float scale)
		: CSceneSaver(pContext, pScene, sceneWorldToCryWorldTM)
		, m_scale(scale)
	{
	}

	ESceneType GetSceneType(const CImportRequest& ir)
	{
		if (ir.outputFilenameExt == "skin")
		{
			return ESceneType::Skin;
		}
		else if (ir.outputFilenameExt == "chr" || ir.outputFilenameExt == "skel")
		{
			return ESceneType::Skeleton;
		}
		else if (ir.outputFilenameExt == "i_caf" || ir.outputFilenameExt == "caf")
		{
			return ESceneType::Animation;
		}

		return ESceneType::Static;
	}

	bool Save(const string& outputCgfFilename, const CImportRequest& ir)
	{
		const ESceneType sceneType = GetSceneType(ir);
		const bool bCreateSkinningInfo = sceneType != ESceneType::Static;
		const bool bSaveSkin = (sceneType == ESceneType::Skin) || (sceneType == ESceneType::Skeleton);
		const bool bSaveAnimation = sceneType == ESceneType::Animation;

		if (!CollectNodes(sceneType, ir))
		{
			return false;
		}

		ComputeNodeTransforms(ir.bSceneOrigin);

		CContentCGF cgf(outputCgfFilename.c_str());

		{
			CExportInfoCGF* const pExportInfo = cgf.GetExportInfo();

			pExportInfo->bCompiledCGF = false;
			pExportInfo->bMergeAllNodes = ir.bMergeAllNodes;
			pExportInfo->bUseCustomNormals = !ir.bIgnoreCustomNormals;
			pExportInfo->bWantF32Vertices = true;
			pExportInfo->bHavePhysicsProxy = true;
			cry_strcpy(pExportInfo->rc_version_string, "fbx scene import");
		}

		if (ir.materials.empty() && (m_pRc->GetVerbosityLevel() > 0) && (sceneType != ESceneType::Skeleton) && (sceneType != ESceneType::Animation))
		{
			RCLogWarning("Missing material infos in JSON data.");
		}

		CExportMaterial exportMaterial(PathUtil::RemoveExtension(ir.materialFilename));
		if (!exportMaterial.PrepareMaterials(m_pScene, ir, m_pRc))
		{
			return false;
		}

		assert(exportMaterial.m_pCgfMaterial);
		cgf.SetCommonMaterial(exportMaterial.m_pCgfMaterial);
		exportMaterial.m_bOwnMaterial = false;  // CContentCGF deletes materials automatically

		std::vector<int> mapSceneNodesToBoneId;
		if (bCreateSkinningInfo && !SetCgfSkinningInfo(cgf, m_pScene, ir.jointPhysicsData, exportMaterial, mapSceneNodesToBoneId, sceneType))
		{
			return false;
		}

		if (!CreateCgfNodes(cgf, m_pScene, exportMaterial, mapSceneNodesToBoneId, sceneType))
		{
			return false;
		}

		if (bSaveSkin)
		{
			return SaveToSkin(ir, sceneType, cgf);
		}
		else if (bSaveAnimation)
		{
			return SaveToCaf(ir, cgf, mapSceneNodesToBoneId);
		}
		else
		{
			return SaveToCgf(ir, cgf, true);
		}
	}

private:

	static void AddMorphTarget(CSkinningInfo* const pSkinningInfo, const MeshUtils::Mesh& m)
	{
		const size_t meshVertexCount = m.GetVertexCount();

		for (size_t morphTargetIndex = 0; morphTargetIndex < m.m_morphTargets.size(); ++morphTargetIndex)
		{
			std::unique_ptr<MorphTargets> pMorphTarget(new MorphTargets);

			pMorphTarget->MeshID = -1; // TODO: Get the actual value!
			pMorphTarget->m_strName = m.m_morphTargets[morphTargetIndex].name;

			for (int vertexIndex = 0; vertexIndex < meshVertexCount; ++vertexIndex)
			{
				if (m.m_morphTargets[morphTargetIndex].positions[vertexIndex] == m.m_positions[vertexIndex])
				{
					continue;
				}
				SMeshMorphTargetVertex v;
				v.nVertexId = vertexIndex;
				v.ptVertex = m.m_morphTargets[morphTargetIndex].positions[vertexIndex] * s_metersToCentimeters;
				pMorphTarget->m_arrIntMorph.push_back(v);
			}
			pSkinningInfo->m_arrMorphTargets.push_back(pMorphTarget.release());
		}
	}

	bool CreateCgfNodes(
		CContentCGF& cgf,
		const Scene::IScene* pScene,
		CExportMaterial& exportMaterial,
		const std::vector<int>& mapSceneNodesToBoneId,
		ESceneType sceneType) const
	{
		size_t rootNodesCount = 0;

		const bool bIgnoreMeshes = (sceneType == ESceneType::Skeleton);

		// We enforce identity transform for root node If is the only root node in the .cgf (then the root index == 0).
		const bool bSetRootTransformToIdentity = (sceneType == ESceneType::Static) && std::none_of(m_nodes.begin() + 1, m_nodes.end(), [](const NodeInfo& node)
		{
			return node.parentNode < 0;
		});

		for (int i = 0; i < (int)m_nodes.size(); ++i)
		{
			const NodeInfo& ni = m_nodes[i];

			CNodeCGF* const pCGFNode = new CNodeCGF(); // will be auto deleted by CContentCGF cgf

			cry_strcpy(pCGFNode->name, ni.pName->c_str());

			pCGFNode->pos_cont_id = 0;
			pCGFNode->rot_cont_id = 0;
			pCGFNode->scl_cont_id = 0;

			pCGFNode->pParent = (ni.parentNode < 0) ? nullptr : cgf.GetNode(ni.parentNode);

			if (!bSetRootTransformToIdentity)
			{
				pCGFNode->localTM = ni.local;
				pCGFNode->worldTM = ni.world;
			}
			else if (pCGFNode->pParent) // We need to update world transformations.
			{
				pCGFNode->localTM = ni.local;
				pCGFNode->worldTM = pCGFNode->pParent->worldTM * ni.local;
			}
			else
			{
				pCGFNode->localTM = Matrix34(IDENTITY);
				pCGFNode->worldTM = Matrix34(IDENTITY);
			}

			pCGFNode->bIdentityMatrix = pCGFNode->worldTM.IsIdentity();

			pCGFNode->bPhysicsProxy = false;
			pCGFNode->nPhysicalizeFlags = 0;

			CMesh* pCMesh = nullptr;
			if (!bIgnoreMeshes && !ni.bIgnoreMesh)
			{
				MeshUtils::Mesh m;

				if (!BuildMesh(m, i, pScene, exportMaterial.m_pCgfMaterial, exportMaterial.m_matIds, mapSceneNodesToBoneId))
				{
					RCLogError("Failed to compose mesh for node '%s'", pCGFNode->name);
					return false;
				}

				if (m.GetFaceCount() > 0)
				{
					pCMesh = CreateCMesh(m, *exportMaterial.m_pCgfMaterial);
					if (!pCMesh)
					{
						RCLogError("Failed to create mesh for node '%s'", pCGFNode->name);
						return false;
					}

					if (!m.m_links.empty())
					{
						if (!m.m_morphTargets.empty())
						{
							AddMorphTarget(cgf.GetSkinningInfo(), m);
						}
					}
				}

				pCGFNode->pMaterial = exportMaterial.m_pCgfMaterial;
			}

			pCGFNode->type = pCMesh ? CNodeCGF::NODE_MESH : CNodeCGF::NODE_HELPER;
			pCGFNode->pMesh = pCMesh;
			pCGFNode->bPhysicsProxy = pCMesh && ni.bPhysicsProxy;

			if (pCMesh && (pCGFNode->bPhysicsProxy || (ni.lod > 0)))
			{
				pCGFNode->type = CNodeCGF::NODE_HELPER;
				pCGFNode->helperType = HelperTypes::HP_GEOMETRY;
			}

			pCGFNode->properties = ni.pProperties ? *ni.pProperties : "";

			cgf.AddNode(pCGFNode);
		}

		return true;
	}

	// The function uses the source scene node indexing, so all the parameter vectors must have size equal to m_pScene->GetNodeCount(); 
	// Returns the index of the sceleton root node of the source scene.
	int CollectBones(std::vector<bool>& isBone, std::vector<bool>& isBindPoseBone, std::vector<Matrix34>& bindPoseTransform) const
	{
		assert((int)isBone.size() == m_pScene->GetNodeCount());
		assert((int)isBindPoseBone.size() == m_pScene->GetNodeCount());
		assert((int)bindPoseTransform.size() == m_pScene->GetNodeCount());

		int rootNodeIdx = (int)m_pScene->GetNodeCount();

		for (const auto& node : m_nodes)
		{
			isBone[node.sourceSceneNode] = true;

			const Scene::SNode* const pNode = m_pScene->GetNode(node.sourceSceneNode);
			for (size_t attributeIndex = 0; attributeIndex < pNode->attributes.size(); ++attributeIndex)
			{
				if (pNode->attributes[attributeIndex].eType != Scene::SNode::eAttributeType_Mesh)
				{
					continue;
				}

				const int meshIndex = pNode->attributes[attributeIndex].index;
				const Scene::SLinksInfo* pMeshLinks = m_pScene->GetMeshLinks(meshIndex);
				if (!pMeshLinks || pMeshLinks->linkedNodes.empty())
				{
					// The mesh does not have skin deformer.
					continue;
				}

				// All nodes are bones except the skinned nodes.
				isBone[node.sourceSceneNode] = false;

				const Matrix34 transfromToTheNewMeshOrigin = node.world.GetInverted() * node.worldOriginal;

				// Loop through all the bones for this mesh. 
				for (const Scene::SLinkInfo& bindPose : pMeshLinks->linkedNodes)
				{
					Matrix34 m = bindPose.initialTransform.GetInverted() * bindPose.initialLinkTransform;
					m = transfromToTheNewMeshOrigin * m;
					m.SetTranslation(m.GetTranslation() * m_scale);

					// In a case if 'transfromToTheNewMeshOrigin' (actually node.worldOriginal) has a scale.
					// The mesh is already pre-multiplied (scaled) by transfromToTheNewMeshOrigin in the BuildMesh() call.
					m.OrthonormalizeFast();

					bindPoseTransform[bindPose.nodeIndex] = m;
					isBindPoseBone[bindPose.nodeIndex] = true;
				}
			}

			if (isBone[node.sourceSceneNode])
			{
				// Assumes that parent index is always less than the child index.
				rootNodeIdx = std::min(rootNodeIdx, node.sourceSceneNode);
			}
		}
		return rootNodeIdx != (int)m_pScene->GetNodeCount() ? rootNodeIdx : -1;
	}

	// Validate if selected bones form the minimum skeleton subtree
	bool ValidateBoneHierarchy(std::vector<bool> isBone) const
	{
		const int sceneNodeCount = m_pScene->GetNodeCount();
		std::vector<bool> minimumSubtree(sceneNodeCount, false);
		const int minimumSubtreeRoot = FindMinimumSubtree(minimumSubtree, *m_pScene, isBone);
		if (minimumSubtreeRoot == 0)
		{
			RCLogWarning("The skeleton has multiple roots.");
		}

		// // Check all nodes but dummy root (==0)
		for (int i = 1; i < sceneNodeCount; ++i)
		{
			if (minimumSubtree[i] && !isBone[i])
			{
				RCLogWarning("The skeleton tree refers to a FBX node not selected to import: %s", m_pScene->GetNode(i)->name.c_str());
			}
		}
		return true;
	}

	// Stores indices of a joint and proxy node for node paths of a SJointPhysicsData object.
	struct SProxyBinding
	{
		const int jointNode; // index in [0, m_pScene->GetNodeCount()).
		const int proxyNode; // index in [0, m_pScene->GetNodeCount()).
		const CImportRequest::SJointPhysicsData jpd;

		SProxyBinding(int jointNode, int proxyNode, const CImportRequest::SJointPhysicsData& jpd)
			: jointNode(jointNode)
			, proxyNode(proxyNode)
			, jpd(jpd)
		{
		}
	};

	struct SJointLimitBinding
	{
		const int jointNode;

		// Joint limits.
		float values[6];
		char mask;

		SJointLimitBinding(int jointNode)
			: jointNode(jointNode)
			, mask(0)
		{}
	};

	// Let p be some element of the vector returned by GetProxyBindings. Then p.first, p.second are indices of m_pScene->m_nodes,
	// such that p.second is the physics proxy geometry of p.first.
	void GetJointBindings(
		const std::vector<CImportRequest::SJointPhysicsData>& jointPhysicsData,
		std::vector<SProxyBinding>& pb,
		std::vector<SJointLimitBinding>& jlb,
		std::vector<int>& mapNodeToProxyBinding) const
	{
		assert(mapNodeToProxyBinding.size() == m_pScene->GetNodeCount());

		pb.clear();
		pb.reserve(jointPhysicsData.size());

		jlb.clear();
		jlb.reserve(jointPhysicsData.size());

		for (int i = 0, N = (int)jointPhysicsData.size(); i < N; ++i)
		{
			const auto& jpd = jointPhysicsData[i];

			const int jointNode = FindMatchingNodeInScene(jpd.jointNodePath);
			if (jointNode == -1)
			{
				continue;
			}

			const int proxyNode = FindMatchingNodeInScene(jpd.proxyNodePath);
			if (proxyNode > 0)
			{
				mapNodeToProxyBinding[jointNode] = pb.size();
				pb.emplace_back(jointNode, proxyNode, jpd);
			}

			if (jpd.jointLimits.mask != 0)
			{
				SJointLimitBinding jl(jointNode);
				memcpy(jl.values, jpd.jointLimits.values, sizeof(jl.values));
				jl.mask = jpd.jointLimits.mask;
				jlb.push_back(jl);
			}
		}
	}

	static void BuildPhysicalProxyFromMesh(PhysicalProxy& pp, const MeshUtils::Mesh& mesh)
	{
		pp.ChunkID = -1;

		pp.m_arrPoints.reserve(mesh.m_positions.size());
		for (const auto& p : mesh.m_positions)
		{
			pp.m_arrPoints.push_back(p);
		}

		pp.m_arrIndices.reserve(3 * mesh.m_faces.size());
		for (const auto& f : mesh.m_faces)
		{
			pp.m_arrIndices.push_back(f.vertexIndex[0]);
			pp.m_arrIndices.push_back(f.vertexIndex[1]);
			pp.m_arrIndices.push_back(f.vertexIndex[2]);
		}

		pp.m_arrMaterials.reserve(mesh.m_faceMatIds.size());
		for (const auto& id : mesh.m_faceMatIds)
		{
			// pp.m_arrMaterials.push_back(id);
			pp.m_arrMaterials.push_back(0);
		}
	}

	bool BuildPhysicalProxy(
		PhysicalProxy& boneMesh, // out 
		const NodeInfo& boneNodeInfo,
		const Matrix34& boneWorldTransform,
		const SProxyBinding& proxyBinding,
		CExportMaterial& exportMaterial,
		const std::vector<int>& mapSceneNodesToBoneId) const
	{
		const int jointIndex = proxyBinding.jointNode;
		assert(mapSceneNodesToBoneId[jointIndex] != -1);

		const Scene::SNode* const pProxyNode = m_pScene->GetNode(proxyBinding.proxyNode);
		assert(pProxyNode);

		NodeInfo proxyNodeInfo;
		proxyNodeInfo.sourceSceneNode = proxyBinding.proxyNode;
		proxyNodeInfo.worldOriginal = m_sceneWorldToCryWorldTM * pProxyNode->worldTransform;
		proxyNodeInfo.worldOriginal.SetTranslation(proxyNodeInfo.worldOriginal.GetTranslation() * m_scale);
		proxyNodeInfo.geometryOffset = pProxyNode->geometryOffset;
		proxyNodeInfo.geometryOffset.SetTranslation(proxyNodeInfo.geometryOffset.GetTranslation() * m_scale);
		proxyNodeInfo.world = boneWorldTransform;
		if (proxyBinding.jpd.bSnapToJoint)
		{
			proxyNodeInfo.worldOriginal = boneNodeInfo.worldOriginal;
		}

		MeshUtils::Mesh m;
		if (!BuildMesh(m, proxyNodeInfo, m_pScene, exportMaterial.m_pCgfMaterial, exportMaterial.m_matIds, mapSceneNodesToBoneId))
		{
			RCLogWarning("Failed to compose proxy mesh for joint '%s' with proxy '%s'",
				m_pScene->GetNode(jointIndex)->name,
				m_pScene->GetNode(proxyBinding.proxyNode)->name);
			return false;
		}

		BuildPhysicalProxyFromMesh(boneMesh, m);
		return true;
	}

	static void WriteBoneDefaults(BONE_ENTITY& e)
	{
		extern void WritePhysDefaults(BONE_ENTITY& e);

		WritePhysDefaults(e);

		const float invalidTag = 100.0f;  // Set this as m00 value of a matrix to tag it as invalid.
		e.phys.framemtx[0][0] = invalidTag;
	}

	void WriteJointLimits(DynArray<BONE_ENTITY>& boneEntities, std::vector<SJointLimitBinding>& limitBindings, const std::vector<int>& mapNodeToBoneId) const
	{
		for (const auto& jlb : limitBindings)
		{
			const int boneId = mapNodeToBoneId[jlb.jointNode];
			if (boneId == -1)
			{
				continue;
			}

			BONE_ENTITY& boneEntity = boneEntities[boneId];

			boneEntity.phys.flags = all_angles_locked;

			// Min limits.
			for (int axis = 0; axis < 3; ++axis)
			{
				const int axisLimit = axis;
				if (jlb.mask & (1 << axisLimit))
				{
					boneEntity.phys.min[axis] = DEG2RAD(jlb.values[axisLimit]);
					boneEntity.phys.flags &= ~(angle0_locked << axis);
				}
			}

			// Max limits.
			for (int axis = 0; axis < 3; ++axis)
			{
				const int axisLimit = 3 + axis;
				if (jlb.mask & (1 << axisLimit))
				{
					boneEntity.phys.max[axis] = DEG2RAD(jlb.values[axisLimit]);
					boneEntity.phys.flags &= ~(angle0_locked << axis);
				}
			}
		}
	}

	bool SetCgfSkinningInfo(
		CContentCGF& cgf,
		const Scene::IScene* pScene,
		const std::vector<CImportRequest::SJointPhysicsData>& jointPhysicsData,
		CExportMaterial& exportMaterial,
		std::vector<int>& mapNodeToBoneId,
		ESceneType sceneType) const
	{
		assert(pScene);

		const int sceneNodeCount = m_pScene->GetNodeCount();

		std::vector<bool> isBone(sceneNodeCount, false);
		std::vector<bool> isBindPoseBone(sceneNodeCount, false);
		std::vector<Matrix34> bindPoseTransform(sceneNodeCount, Matrix34(IDENTITY));

		const int root = CollectBones(isBone, isBindPoseBone, bindPoseTransform);

		if (!ValidateBoneHierarchy(isBone))
		{
			return false;
		}

		const int parent = root > 0 ? m_pScene->GetNode(root)->parent : root;

		std::vector<int> boneNodes;
		SelectChildNodesOrderedByNameDepthFirst(boneNodes, parent, [&isBone](int node)
		{
			return isBone[node];
		});

		const size_t boneCount = boneNodes.size();
		mapNodeToBoneId.resize(sceneNodeCount);
		std::fill(mapNodeToBoneId.begin(), mapNodeToBoneId.end(), -1);
		for (size_t i = 0; i < boneCount; i++)
		{
			mapNodeToBoneId[boneNodes[i]] = i;
		}

		// Fill in the Skinning Info.

		CSkinningInfo* const pSkinningInfo = cgf.GetSkinningInfo();
		pSkinningInfo->m_arrBoneEntities.resize(boneCount);
		pSkinningInfo->m_arrBonesDesc.resize(boneCount);
		pSkinningInfo->m_arrPhyBoneMeshes.clear();

		std::vector<SProxyBinding> proxyBindings;
		std::vector<SJointLimitBinding> limitBindings;
		std::vector<int> mapNodeToProxyBinding;
		mapNodeToProxyBinding.resize(sceneNodeCount, -1);
		GetJointBindings(jointPhysicsData, proxyBindings, limitBindings, mapNodeToProxyBinding);

		for (const auto& node : m_nodes)
		{
			const int boneID = mapNodeToBoneId[node.sourceSceneNode];

			if (boneID == -1)
			{
				continue;
			}

			BONE_ENTITY& boneEntity = pSkinningInfo->m_arrBoneEntities[boneID];
			memset(&boneEntity, 0, sizeof(boneEntity));
			boneEntity.BoneID = boneID;
			boneEntity.ParentID = -1;
			boneEntity.nChildren = 0;
			boneEntity.ControllerID = SkeletonHelpers::ComputeControllerId(node.pName->c_str());
			boneEntity.prop[0] = 0;
			WriteBoneDefaults(boneEntity);
			boneEntity.phys.nPhysGeom = -1;

			CryBoneDescData& boneDesc = pSkinningInfo->m_arrBonesDesc[boneID];
			memset(&boneDesc, 0, sizeof(boneDesc));
			cry_strcpy(boneDesc.m_arrBoneName, node.pName->c_str());
			boneDesc.m_nControllerID = SkeletonHelpers::ComputeControllerId(node.pName->c_str());

			if (node.parentNode == -1)
			{
				// CryEngine expects that root nodes have identity orientation. 
				// So, here we assign the corrected node transfromation.
				boneDesc.m_DefaultB2W = node.world;
			}
			else if (isBindPoseBone[node.sourceSceneNode])
			{
				boneDesc.m_DefaultB2W = bindPoseTransform[node.sourceSceneNode];
			}
			else if (sceneType == ESceneType::Skeleton || sceneType == ESceneType::Animation)
			{
				// We do not have proper skin to get bind pose for this bone.
				// So, here we assign the corrected node transfromation.
				boneDesc.m_DefaultB2W = node.world;
			}
			else
			{
				RCLogWarning("Incomplete BindPose. Bone '%s' is not part of the BindPose definition.", node.pName->c_str());

				// Inherit transformation of parent bone. 
				const int parentBoneId = mapNodeToBoneId[m_nodes[node.parentNode].sourceSceneNode];
				boneDesc.m_DefaultB2W = pSkinningInfo->m_arrBonesDesc[parentBoneId].m_DefaultB2W;
			}

			boneDesc.m_DefaultW2B = boneDesc.m_DefaultB2W.GetInverted();

			// if the bone has physical proxy.
			int proxyBindingId = mapNodeToProxyBinding[node.sourceSceneNode];
			if (proxyBindingId != -1)
			{
				PhysicalProxy proxy;
				if (BuildPhysicalProxy(proxy, node, boneDesc.m_DefaultB2W, proxyBindings[proxyBindingId], exportMaterial, mapNodeToBoneId))
				{
					boneEntity.phys.nPhysGeom = pSkinningInfo->m_arrPhyBoneMeshes.size();
					pSkinningInfo->m_arrPhyBoneMeshes.push_back(proxy);
				}
			}
		}

		WriteJointLimits(pSkinningInfo->m_arrBoneEntities, limitBindings, mapNodeToBoneId);

		// Init bone hierarchy
		for (size_t i = 0; i < boneCount; i++)
		{
			auto& boneEntities = pSkinningInfo->m_arrBoneEntities;
			auto& boneDescriptors = pSkinningInfo->m_arrBonesDesc;

			assert(boneEntities[i].BoneID == i);

			const Scene::SNode* node = pScene->GetNode(boneNodes[i]);
			if (node->parent >= 0)
			{
				int parentBoneId = mapNodeToBoneId[node->parent];
				if (parentBoneId >= 0 || parentBoneId < boneCount)
				{
					boneEntities[i].ParentID = parentBoneId;
					++boneEntities[parentBoneId].nChildren;
					boneDescriptors[i].m_nOffsetParent = parentBoneId - int(i);
				}
			}
		}
		return true;
	}

	template<typename UnaryPredicate>
	void SelectChildNodesOrderedByNameDepthFirst(std::vector<int>& result, int root, UnaryPredicate f) const
	{
		const auto* pNode = m_pScene->GetNode(root);

		std::vector<int> children;
		children.reserve(pNode->children.size());
		std::copy_if(pNode->children.begin(), pNode->children.end(), std::back_inserter(children), f);
		std::sort(children.begin(), children.end(), [this](int a, int b)
		{
			return m_pScene->GetNode(a)->name.compareNoCase(m_pScene->GetNode(b)->name) < 0;
		});

		for (const int child : children)
		{
			result.push_back(child);
			SelectChildNodesOrderedByNameDepthFirst(result, child, f);
		}
	}

	bool SaveToSkin(
		const CImportRequest& ir,
		const ESceneType sceneType,
		CContentCGF& cgf)
	{
		if (!SaveToSkinUncompiled(ir, sceneType, cgf))
		{
			return false;
		}

		// Compile and write resulting CHR/SKIN file

		std::vector<string> args = { "/refresh", string().Format("/overwriteextension=%s", ir.outputFilenameExt.c_str()) };
		return m_pRc->CompileSingleFileNested(cgf.GetFilename(), args);
	}

	bool SaveToSkinUncompiled(
		const CImportRequest& ir,
		const ESceneType sceneType,
		CContentCGF& cgf)
	{
		CSkinningInfo* pSkinningInfo = cgf.GetSkinningInfo();

		if (pSkinningInfo->m_arrBonesDesc.empty())
		{
			return false;
		}

		for (int i = 0; i < cgf.GetNodeCount(); ++i)
		{
			CMesh* const pMesh = cgf.GetNode(i)->pMesh;
			if (pMesh)
			{
				// CLoaderCGF flips texture coordiantes of uncompiled meshes. Flipping the coordinates here negates this.
				FlipTextureCoordinatesHorizontally(pMesh);
			}
		}

		CChunkFile chunkFile;
		CSaverCGF cgfSaver(chunkFile);
		cgfSaver.SetContent(&cgf);

		const bool bSwapEndian = false;

		// Only store 
		cgfSaver.SaveExportFlags(bSwapEndian);
		cgfSaver.SaveMaterials(bSwapEndian);
		cgfSaver.SaveUncompiledNodes();				// the nodes of character will be compile later
		cgfSaver.SaveUncompiledMorphTargets();

		if (sceneType == ESceneType::Skeleton)
		{
			// When writing a .chr file, we do not write any nodes, so we have to write a material explicitly.
			for (int i = 0, N = cgf.GetNodeCount(); i < N; ++i)
			{
				if (cgf.GetNode(i)->type == CNodeCGF::NODE_MESH)
				{
					RCLogWarning("Unexpected mesh node when writing skeleton.\n");
					break;
				}
			}

			if (cgf.GetCommonMaterial())
			{
				cgfSaver.SaveMaterial(cgf.GetCommonMaterial(), bSwapEndian);
			}
		}

		CSkinSaver::SaveUncompiledSkinningInfo(cgfSaver, *pSkinningInfo, bSwapEndian, s_metersToCentimeters);

		chunkFile.AddChunk(ChunkType_ImportSettings, 0, eEndianness_Native, ir.jsonData.data(), (int)ir.jsonData.size());

		return CgfUtil::WriteTempRename(chunkFile, cgf.GetFilename());
	}

	static void GetDecomposedTransform(
		const Matrix34& transform,
		Vec3& t,
		Quat& r,
		Vec3& s)
	{
		decomp::AffineParts parts;
		struct MatrixContainer { decomp::HMatrix m; };
		decomp::decomp_affine(((MatrixContainer*)&transform)->m, &parts);

		t.Set(parts.t.x, parts.t.y, parts.t.z);
		r = *(Quat*)&parts.q;
		s.Set(parts.k.x, parts.k.y, parts.k.z);
	}

	static bool DoesContainDuplicatedControllerIds(const CSkinningInfo& skinningInfo)
	{
		std::vector<uint32> sortedJointIds;
		{
			const auto& joints = skinningInfo.m_arrBoneEntities;

			sortedJointIds.reserve(joints.size());
			std::transform(joints.begin(), joints.end(), std::back_inserter(sortedJointIds), [](const BONE_ENTITY& x) { return x.ControllerID; });
			std::sort(sortedJointIds.begin(), sortedJointIds.end());
		}

		auto current = sortedJointIds.begin();
		std::vector<uint32>::iterator duplicate;
		while ((duplicate = std::adjacent_find(current, sortedJointIds.end())) != sortedJointIds.end())
		{
			// go to next id
			current = std::upper_bound(duplicate, sortedJointIds.end(), *duplicate);

			for (const auto& bone : skinningInfo.m_arrBonesDesc)
			{
				if (bone.m_nControllerID == *duplicate)
				{
					RCLogWarning("Duplicate controller id: %u, %s", bone.m_nControllerID, bone.m_arrBoneName);
				}
			}
		}
		return current != sortedJointIds.begin();
	}

	template< typename T>
	static T Clamp(T value, T lower, T upper)
	{
		return (value <= lower) ? lower : ((value >= upper) ? upper : value);
	}

	int GetAnimationStackIndex(const string& name)
	{
		for (int i = 0; i < m_pScene->GetAnimationStackCount(); ++i)
		{
			const Scene::SAnimation* pAnimation = m_pScene->GetAnimationStack(i);
			if (name.compareNoCase(pAnimation->name) == 0)
			{
				return i;
			}
		}

		return -1;
	}

	void LogAnimationStacks()
	{
		RCLog("Available animations:");
		for (int i = 0; i < m_pScene->GetAnimationStackCount(); ++i)
		{
			const Scene::SAnimation* pAnimation = m_pScene->GetAnimationStack(i);
			RCLog("%s", pAnimation->name.c_str());
		}
	}

	bool SetAnimationStack(int& startFrame, int& endFrame, const CImportRequest& ir)
	{
		int animationStackIndex = 0;

		if (!ir.animation.name.empty())
		{
			animationStackIndex = GetAnimationStackIndex(ir.animation.name);
			if (animationStackIndex < 0)
			{
				RCLogError("Can't find requested animation stack '%s'", ir.animation.name.c_str());
				LogAnimationStacks();
				return false;
			}
		}

		// Static pose if fbx scene does not have animation.
		if (!m_pScene->GetAnimationStackCount())
		{
			startFrame = 0;
			endFrame = 0;
			return true;
		}

		if (!m_pScene->SetCurrentAnimationStack(animationStackIndex))
		{
			return false;
		}

		const Scene::SAnimation* pAnimation = m_pScene->GetAnimationStack(animationStackIndex);
		startFrame = ir.animation.startFrame >= 0 ? Clamp(ir.animation.startFrame, pAnimation->startFrame, pAnimation->endFrame) : pAnimation->startFrame;
		endFrame = ir.animation.endFrame >= 0 ? Clamp(ir.animation.endFrame, startFrame, pAnimation->endFrame) : pAnimation->endFrame;

		if (startFrame > endFrame)
		{
			// This should never happen.
			assert(0);
			return false;
		}

		return true;
	}

	bool SaveToCaf(
		const CImportRequest& ir,
		CContentCGF& cgf,
		const std::vector<int>& mapSceneNodesToBoneId)
	{
		if (!SaveToICaf(ir, cgf, mapSceneNodesToBoneId))
		{
			return false;
		}

		if (ir.outputFilenameExt.compareNoCase("i_caf") == 0)
		{
			return true;
		}

		CSkeletonInfo skeleton;
		{
			skeleton = CSkeletonInfo();
			skeleton.m_SkinningInfo = *cgf.GetSkinningInfo();
		}

		SPlatformAnimationSetup platform;
		{
			platform = SPlatformAnimationSetup();
		}

		SAnimationDesc animDesc;
		{
			animDesc = SAnimationDesc();
			animDesc.m_bSkipSaveToDatabase = true;
			animDesc.m_bAdditiveAnimation = false;
			animDesc.m_skeletonName = "";
			animDesc.m_bNewFormat = true;
			animDesc.newFmt.m_autodeletePosEps = 0.0f;
			animDesc.newFmt.m_autodeleteRotEps = 0.0f;
			animDesc.newFmt.m_autodeleteSclEps = 0.0f;
			animDesc.newFmt.m_compressPosMul = 1.0f;
			animDesc.newFmt.m_compressRotMul = 1.0f;
			animDesc.newFmt.m_compressSclMul = 1.0f;
		}

		CAnimationCompressor compressor(skeleton);
		if (!compressor.LoadCAF(cgf.GetFilename(), platform, animDesc, eCAFLoadUncompressedOnly))
		{
			RCLogError("Failed to generate an intermediate CAF file: '%s'.", cgf.GetFilename());
			return false;
		}

		const char* const szMotionNodeName = !ir.animation.motionNodePath.empty() ? ir.animation.motionNodePath.back() : "";
		compressor.EnableRootMotionExtraction(szMotionNodeName);
		compressor.EnableTrackAligning(false); // TODO: parametrize from RC input arguments
		compressor.m_GlobalAnimationHeader.SetFilePathCAF(cgf.GetFilename());

		if (!compressor.ProcessAnimation(false))
		{
			RCLogError("Failed to perform inplace i_caf->caf conversion for '%s'.", cgf.GetFilename());
			return false;
		}

		compressor.m_GlobalAnimationHeader.m_nCompressedControllerCount = compressor.m_GlobalAnimationHeader.CountCompressedControllers();

		if (!FileUtil::EnsureDirectoryExists(PathUtil::GetPathWithoutFilename(cgf.GetFilename())))
		{
			RCLogError("Failed to create directory for '%s'.", cgf.GetFilename());
			return false;
		}

		CChunkFile chunkFile;
		compressor.SaveMotionParameters(&chunkFile, false);

		CSaverCGF cgfSaver(chunkFile);
		const auto savedControllerCount = compressor.SaveControllers(cgfSaver, false);
		if (savedControllerCount < compressor.m_GlobalAnimationHeader.m_nCompressedControllerCount)
		{
			RCLogWarning("Some animation controllers have not been saved successfully (expected: %u, saved: %u)", compressor.m_GlobalAnimationHeader.m_nCompressedControllerCount, savedControllerCount);
		}

		chunkFile.AddChunk(ChunkType_ImportSettings, 0, eEndianness_Native, ir.jsonData.data(), (int)ir.jsonData.size());

		return CgfUtil::WriteTempRename(chunkFile, cgf.GetFilename());
	}

	bool SaveToICaf(
		const CImportRequest& ir,
		CContentCGF& cgf,
		const std::vector<int>& mapSceneNodesToBoneId)
	{
		const CSkinningInfo* pSkinningInfo = cgf.GetSkinningInfo();

		if (pSkinningInfo->m_arrBoneEntities.empty())
			return false;

		if (DoesContainDuplicatedControllerIds(*pSkinningInfo))
		{
			RCLogError("Failed to compile animation '%s'. FBX source contains duplicated controller names or colliding name hashes.", cgf.GetFilename());
			return false;
		}

		int startFrame = -1;
		int endFrame = -1;
		if (!SetAnimationStack(startFrame, endFrame, ir))
		{
			return false;
		}
		assert(startFrame >= 0 && endFrame >= 0 && startFrame <= endFrame);

		const bool bSwapEndian = false;

		CChunkFile chunkFile;
		CSaverCAF cafSaver(cgf.GetFilename(), chunkFile);

		const int frameCount = endFrame - startFrame + 1;
		CSaverAnim::SaveTiming(&chunkFile, startFrame, endFrame);

		std::vector<CryKeyPQS> frames(frameCount);
		std::vector<bool> nontrivialRoot(m_nodes.size(), false);

		for (size_t i = 0; i < m_nodes.size(); ++i)
		{
			const auto& node = m_nodes[i];

			const int boneId = mapSceneNodesToBoneId[node.sourceSceneNode];
			
			// Exports bones animation only.
			if (boneId == -1)
			{
				continue;
			}

			// Animation does not work well if root bone has initial translation or scale or it is not orthonormal. 
			// In this case we:
			// 1. Set the bone transformation to identity.
			// 2. Save child nodes of such a node in the world coordinate system.
			if (node.parentNode == -1)
			{
				Vec3 t;
				Quat r;
				Vec3 s;
				GetDecomposedTransform(node.worldOriginal, t, r, s);
				nontrivialRoot[i] = !IsEquivalent(s, Vec3(1)) || !IsEquivalent(t, Vec3(0)) || !node.worldOriginal.IsOrthonormal();
			}
			const bool saveNodeAnimation = !nontrivialRoot[i];
			const bool transformToLocalSpace = node.parentNode != -1 && !nontrivialRoot[node.parentNode];

			for (int frame = 0; frame < frameCount; ++frame)
			{
				Matrix34 nodeTransform(IDENTITY);
				Matrix34 parentTransform(IDENTITY);

				if (saveNodeAnimation)
				{
					const Scene::STrs trs = m_pScene->EvaluateNodeGlobalTransform(node.sourceSceneNode, startFrame + frame);
					nodeTransform = m_sceneWorldToCryWorldTM * Matrix34::Create(trs.scale, trs.rotation, trs.translation);

					if (transformToLocalSpace)
					{
						const Scene::STrs trs = m_pScene->EvaluateNodeGlobalTransform(m_nodes[node.parentNode].sourceSceneNode, startFrame + frame);
						parentTransform = m_sceneWorldToCryWorldTM * Matrix34::Create(trs.scale, trs.rotation, trs.translation);
					}
				}

				nodeTransform = parentTransform.GetInverted() * nodeTransform;

				Vec3 t;
				Quat r;
				Vec3 s;
				GetDecomposedTransform(nodeTransform, t, r, s);

				frames[frame].position = t * s_metersToCentimeters * m_scale;
				frames[frame].rotation = r;
				frames[frame].scale = s;
			}
			cafSaver.SaveController(&chunkFile, frames, pSkinningInfo->m_arrBoneEntities[boneId].ControllerID);
		}

		CSaverCGF cgfSaver(chunkFile);
		CSkinSaver::SaveBoneNames(cgfSaver, pSkinningInfo->m_arrBonesDesc, bSwapEndian);

		chunkFile.AddChunk(ChunkType_ImportSettings, 0, eEndianness_Native, ir.jsonData.data(), (int)ir.jsonData.size());

		return CgfUtil::WriteTempRename(chunkFile, cgf.GetFilename());
	}

	// If fbx scene has many roots, returns the one that has more nodes.
	int FindRootOfMaximumNodeSubtree(std::vector<bool> selectedNodes) const
	{
		int root = -1;
		size_t maxCount = 0;

		// Ignore dummy root (i == 0).
		for (int i = 1; i < m_pScene->GetNodeCount(); ++i)
		{
			if (m_pScene->GetNode(i)->parent != 0)
			{
				continue;
			}

			size_t count = 0;
			TraverseDepthFirstPreOrder(m_pScene, i, [&selectedNodes, &count](int node)
			{
				if (selectedNodes[node])
				{
					++count;
				}
			});

			if (maxCount < count)
			{
				maxCount = count;
				root = i;
			}
		}
		return root;
	}

	std::vector<bool> FindMaximumNodeSubtree(std::vector<bool> selectedNodes) const
	{
		const int sceneNodeCount = m_pScene->GetNodeCount();
		const int root = FindRootOfMaximumNodeSubtree(selectedNodes);

		std::vector<bool> bones(sceneNodeCount, false);
		TraverseDepthFirstPreOrder(m_pScene, root, [&selectedNodes, &bones](int node)
		{
			bones[node] = selectedNodes[node];
		});
		return bones;
	}

	static bool IsSkinLinkedToUnselectedBone(const MeshUtils::Mesh& mesh, const std::vector<bool>& nodesToImport)
	{
		for (const auto& vertex : mesh.m_links)
		{
			for (const auto& link : vertex.links)
			{
				if (!nodesToImport[link.boneId])
				{
					return true;
				}
			}
		}
		return false;
	}

	static int GetLodIndexFromName(const string& nodeName)
	{
		static const size_t lodNamePrefixLength = strlen(CGF_NODE_NAME_LOD_PREFIX);
		if (strnicmp(nodeName.c_str(), CGF_NODE_NAME_LOD_PREFIX, lodNamePrefixLength) != 0)
		{
			return 0;
		}

		if (nodeName.length() <= lodNamePrefixLength)
		{
			RCLogWarning("LOD node name '%s' doesn't contain LOD index", nodeName.c_str());
			return 0;
		}

		return atoi(nodeName.c_str() + lodNamePrefixLength);
	}

	static bool IsProxyFromName(const string& nodeName)
	{
		const char* reservedNames[] = { CGF_NODE_NAME_PHYSICS_PROXY0, CGF_NODE_NAME_PHYSICS_PROXY1, CGF_NODE_NAME_PHYSICS_PROXY2 };
		for (const char* szProxyName : reservedNames)
		{
			static const size_t proxyNameLength = strlen(szProxyName);
			if (strnicmp(nodeName.c_str(), szProxyName, proxyNameLength) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool CollectNodes(ESceneType sceneType, const CImportRequest& ir)
	{
		m_nodes.clear();

		const bool bSkin = (sceneType == ESceneType::Skeleton) || (sceneType == ESceneType::Skin);
		const bool bSelectAllNodes = !bSkin && ir.nodes.empty();

		const int sceneNodeCount = m_pScene->GetNodeCount();

		// Bitfield of nodes selected to import.
		std::vector<bool> nodesToImport(sceneNodeCount, bSelectAllNodes);

		std::vector<const CImportRequest::SNodeInfoType*> mapNodeIndexToRequestInfo(sceneNodeCount, nullptr);
		SelectRequestedNodesRecursively(nodesToImport, mapNodeIndexToRequestInfo, ir.nodes);

		int skinnedMeshNodeIndex = -1;

		if (sceneType == ESceneType::Skin)
		{
			CSkinHelper skinHelper(m_pScene);
			const std::vector<bool>* const pRequestedSubset = !ir.nodes.empty() ? &nodesToImport : nullptr;

			auto skinnedMeshes = skinHelper.FindSkinnedMeshes(pRequestedSubset);

			if (skinnedMeshes.empty())
			{
				RCLogError("The %s does not have any skinned meshes.", pRequestedSubset ? "import request" : "file");
				return false;
			}
			else if (skinnedMeshes.size() > 1)
			{
				auto maxElement = std::max_element(skinnedMeshes.begin(), skinnedMeshes.end(), [this](const CSkinHelper::SMeshIndex& a, const CSkinHelper::SMeshIndex& b)
				{
					return m_pScene->GetMesh(a.mesh)->GetFaceCount() < m_pScene->GetMesh(b.mesh)->GetFaceCount();
				});
				std::swap(*maxElement, *skinnedMeshes.begin());

				RCLogWarning("%i skinned meshes found, '%s' is selected to import.", 
					skinnedMeshes.size(),
					m_pScene->GetNode(skinnedMeshes.front().node)->name.c_str());

				if (m_pRc->GetVerbosityLevel() > 1)
				{
					for (size_t i = 1; i < skinnedMeshes.size(); ++i)
					{
						RCLogWarning("'%s' is ignored.", m_pScene->GetNode(skinnedMeshes[i].node)->name.c_str());
					}
				}
				skinnedMeshes.resize(1);
			}
			skinnedMeshNodeIndex = skinnedMeshes.front().node;

			// Add meshes
			for (const auto& mesh : skinnedMeshes)
			{
				nodesToImport[mesh.node] = true;
			}

			// Add bones
			std::vector<bool> boneNodes = skinHelper.SelectBones(skinnedMeshes);
			int root = FindMinimumSubtree(nodesToImport, *m_pScene, boneNodes);
			MarkPath(nodesToImport, *m_pScene, root);
		}
		else if (sceneType == ESceneType::Skeleton)
		{
			// Collect skinned meshes to get bind position.
			CSkinHelper skinHelper(m_pScene);
			auto skinnedMeshes = skinHelper.FindSkinnedMeshes();

			// Autodetect bone nodes if we do not have any user selected nodes.
			if (ir.nodes.empty())
			{
				std::vector<bool> boneNodes(sceneNodeCount, true);

				// Collect bones from all skinned meshes.
				if (!skinnedMeshes.empty())
				{
					boneNodes = skinHelper.SelectBones(skinnedMeshes);
				}

				const int root = FindMinimumSubtree(nodesToImport, *m_pScene, boneNodes);

				// if we have many skeletons, select the one that has more bones.
				if (root == 0)
				{
					nodesToImport = std::move(FindMaximumNodeSubtree(nodesToImport));
				}
				else
				{
					MarkPath(nodesToImport, *m_pScene, root);
				}
			}

			// Add meshes to get bind pose. 
			for (const auto& mesh : skinnedMeshes)
			{
				nodesToImport[mesh.node] = true;
			}
		}

		std::vector<int> remap(sceneNodeCount, -1);

		// Collect all selected nodes, except the dummy root.
		nodesToImport[0] = false;

		const size_t nodesToImportCount = std::count(nodesToImport.begin(), nodesToImport.end(), true);
		m_nodes.resize(nodesToImportCount);

		for (size_t i = 1, j = 0; i < m_pScene->GetNodeCount(); ++i)
		{
			if (!nodesToImport[i])
			{
				continue;
			}

			const Scene::SNode* const pNode = m_pScene->GetNode(i);
			assert(pNode);

			NodeInfo& ni = m_nodes[j];
			ni.parentNode = remap[pNode->parent];
			ni.sourceSceneNode = i;

			if (mapNodeIndexToRequestInfo[i])
			{
				ni.pName = &mapNodeIndexToRequestInfo[i]->name;
				ni.pProperties = &mapNodeIndexToRequestInfo[i]->properties;
			}
			else
			{
				ni.pName = &pNode->name;
				ni.pProperties = nullptr;
			}

			ni.bPhysicsProxy = IsProxyFromName(*ni.pName);
			ni.lod = GetLodIndexFromName(*ni.pName);
			if ((ni.lod > 0) && (ni.parentNode < 0))
			{
				RCLogError("Node %s must have parent node.", ni.pName->c_str());
				return false;
			}

			ni.worldOriginal = m_sceneWorldToCryWorldTM * pNode->worldTransform;
			ni.worldOriginal.SetTranslation(ni.worldOriginal.GetTranslation() * m_scale);

			ni.geometryOffset = pNode->geometryOffset;
			ni.geometryOffset.SetTranslation(ni.geometryOffset.GetTranslation() * m_scale);

			ni.world = Matrix34(IDENTITY);
			ni.local = Matrix34(IDENTITY);

			// .skin format allows only one mesh in the file.
			ni.bIgnoreMesh = (skinnedMeshNodeIndex != -1) && (skinnedMeshNodeIndex != i);

			remap[i] = (int)j;
			++j;
		}

		if (m_nodes.empty())
		{
			RCLogError("%s: Nothing to export", __FUNCTION__);
			return false;
		}

		// Some extra validation.
		for (const auto& node : m_nodes)
		{
			// Please see Code\CryEngine\Cry3DEngine\StatObjLoad.cpp - CStatObj::LoadCGF_Int.
			if (node.pName->compareNoCase("main") == 0 && Scene::IsMesh(*m_pScene, node.sourceSceneNode))
			{
				RCLogWarning("A mesh with name 'main' has been found. The name 'Main' is reserved for Destroyable Objects. "
					"All fbx scene meshes will be hidden except the one called 'Main'.");
			}
		}

		return true;
	}

	bool SelectRequestedNodesRecursively( 
		std::vector<bool>& nodesToImport, 
		std::vector<const CImportRequest::SNodeInfoType*>& mapNodeIndexToRequestInfo, 
		const std::vector<CImportRequest::SNodeInfoType>& requestedNodes)
	{
		for (size_t i = 0; i < requestedNodes.size(); ++i)
		{
				int sceneNodeIndex = FindMatchingNodeInScene(requestedNodes[i].path);
				if (sceneNodeIndex < 0)
			{
					RCLogError("Node %s is not found in the source scene", GetNodePathAsString(requestedNodes[i].path).c_str());
					return false;
				}
				nodesToImport[sceneNodeIndex] = true;
				mapNodeIndexToRequestInfo[sceneNodeIndex] = &requestedNodes[i];

				if (!SelectRequestedNodesRecursively(nodesToImport, mapNodeIndexToRequestInfo, requestedNodes[i].children))
				{
					return false;
				}
		}
		return true;
	}

	void ComputeNodeTransforms(bool bSceneOrigin)
	{
		// CRYENGINE has an optimization that ignores transform (assumes it's identity)
		// of a mesh node if the mesh node is the only mesh node in .cgf. 
		// We enforce identity transform for such mesh nodes in the code below (see
		// condition "meshCount == 1 && i == nodeWithMesh").

		size_t meshCount = 0;
		size_t nodeWithMesh = m_nodes.size();
		for (size_t i = 0; i < m_nodes.size(); ++i)
		{
			const NodeInfo& ni = m_nodes[i];

			const Scene::SNode* const pNode = m_pScene->GetNode(ni.sourceSceneNode);
			assert(pNode);

			for (size_t j = 0; j < pNode->attributes.size(); ++j)
			{
				if (pNode->attributes[j].eType == Scene::SNode::eAttributeType_Mesh)
				{
					++meshCount;
					nodeWithMesh = i;
					break;
				}
			}
		}

		for (size_t i = 0; i < m_nodes.size(); ++i)
		{
			NodeInfo& ni = m_nodes[i];

			// CryEngine expects that the following nodes have identity orientation:
			// - all root cgf nodes.
			// - a mesh node if the mesh node is the only mesh node in cgf.
			if (ni.parentNode < 0 || (meshCount == 1 && i == nodeWithMesh))
			{
				ni.world.SetTranslationMat(ni.worldOriginal.GetTranslation());
			}
			else if ((ni.lod > 0) || ni.bPhysicsProxy)
			{
				// LOD[1..n] and proxies are in place of parents LOD0. It is needed to transform LOD and proxy meshes.
				ni.world = m_nodes[ni.parentNode].world;
			}
			else
			{
				ni.world = TransformHelpers::ComputeOrthonormalMatrix(ni.worldOriginal);
			}

			if (bSceneOrigin)
			{
				ni.world.SetTranslation(Vec3(0, 0, 0));
			}

			// Compute local transform
			if (ni.parentNode < 0)
			{
				ni.local = ni.world;
			}
			else
			{
				assert(ni.parentNode < (int)i);
				ni.local = m_nodes[ni.parentNode].world.GetInverted() * ni.world;
			}
		}
	}

	bool BuildMesh(
		MeshUtils::Mesh& m,
		const NodeInfo& ni,
		const Scene::IScene* pScene,
		const CMaterialCGF* pCgfMaterial,
		const std::vector<int>& exportMatIds,
		const std::vector<int>& mapSceneNodesToBoneId) const
	{
		m.Clear();

		const Scene::SNode* const pNode = pScene->GetNode(ni.sourceSceneNode);
		assert(pNode);

		// Create matrix to transform vertices and normals from their current local(modeling) CS to new local(modeling) CS
		// Scene node->result node(resulting matrix is not orthonormal, it's just to transform mesh geometry)
		const Matrix34 sceneNodeToOrthonormalNodeTM = ni.world.GetInverted() * ni.worldOriginal;

		// Apply geometryOffset and unit scale.
		const Matrix34 meshTransform = sceneNodeToOrthonormalNodeTM * ni.geometryOffset * Matrix34::CreateScale(Vec3(m_scale));

		for (size_t i = 0; i < pNode->attributes.size(); ++i)
		{
			if (pNode->attributes[i].eType != Scene::SNode::eAttributeType_Mesh)
			{
				continue;
			}
			const int meshIndex = pNode->attributes[i].index;
			const MeshUtils::Mesh* const pMesh = pScene->GetMesh(meshIndex);
			if (!pMesh)
			{
				continue;
			}

			if (!AppendMesh(m, *pMesh, (int)pCgfMaterial->subMaterials.size(), exportMatIds, mapSceneNodesToBoneId, meshTransform))
			{
				return false;
			}
		}

		return true;
	}

	bool BuildMesh(
		MeshUtils::Mesh& m,
		int nodeIndex,
		const Scene::IScene* pScene,
		const CMaterialCGF* pCgfMaterial,
		const std::vector<int>& exportMatIds,
		const std::vector<int>& mapSceneNodesToBoneId) const
	{
		return BuildMesh(m, m_nodes[nodeIndex], pScene, pCgfMaterial, exportMatIds, mapSceneNodesToBoneId);
	}

	class CSkinHelper
	{
	public:
		CSkinHelper(const Scene::IScene* pScene) : m_pScene(pScene) 
		{
			assert(m_pScene);
		}

		struct SMeshIndex
		{
			SMeshIndex() : node(-1), mesh(-1) {}
			SMeshIndex(int node, int mesh) : node(node), mesh(mesh) {}

			int node;
			int mesh;
		};

		std::vector<SMeshIndex> FindSkinnedMeshes(const std::vector<bool>* const pSubset = nullptr) const
		{
			assert(!pSubset || pSubset->size() == m_pScene->GetNodeCount());

			std::vector<SMeshIndex> meshes;
			for (size_t nodeIndex = 0; nodeIndex < m_pScene->GetNodeCount(); ++nodeIndex)
			{
				if (pSubset && !(*pSubset)[nodeIndex])
				{
					continue;
				}

				const Scene::SNode* const pNode = m_pScene->GetNode(nodeIndex);
				for (size_t attributeIndex = 0; attributeIndex < pNode->attributes.size(); ++attributeIndex)
				{
					if (pNode->attributes[attributeIndex].eType != Scene::SNode::eAttributeType_Mesh)
					{
						continue;
					}

					const int meshIndex = pNode->attributes[attributeIndex].index;
					const MeshUtils::Mesh* const pMesh = m_pScene->GetMesh(meshIndex);

					if (!pMesh || pMesh->m_links.empty())
					{
						continue;
					}

					meshes.emplace_back(nodeIndex, meshIndex);
				}
			}
			return meshes;
		}

		std::vector<bool> SelectBones(const std::vector<SMeshIndex>& meshes) const
		{
			std::vector<bool> boneNodes(m_pScene->GetNodeCount(), false);

			for (const SMeshIndex& index : meshes)
			{
				const MeshUtils::Mesh* const pMesh = m_pScene->GetMesh(index.mesh);

				for (const auto& vertex : pMesh->m_links)
				{
					for (const auto& link : vertex.links)
					{
						assert(link.boneId < m_pScene->GetNodeCount());
						boneNodes[link.boneId] = true;
					}
				}

				const Scene::SLinksInfo* pBindPose = m_pScene->GetMeshLinks(index.mesh);
				if (!pBindPose)
				{
					for (const Scene::SLinkInfo& bone : pBindPose->linkedNodes)
					{
						assert(bone.nodeIndex < m_pScene->GetNodeCount());
						boneNodes[bone.nodeIndex] = true;
					}
				}
			}
			return boneNodes;
		}
	private:
		const Scene::IScene* m_pScene;
	};
};

// Note: 'filename' is used for error messages only
bool ValidateSourceSceneHierarchy(const char* filename, Scene::IScene* pScene)
{
	if (!filename)
	{
		filename = "";
	}

	if (!pScene)
	{
		RCLogError("%s: Internal error. Missing scene pointer for '%s'.", __FUNCTION__, filename);
		return false;
	}

	if (pScene->GetNodeCount() <= 0)
	{
		RCLogError("Failed to load '%s'.", filename);
		return false;
	}

	if (pScene->GetNodeCount() == 1)
	{
		RCLogError("Scene in '%s' is empty (no nodes).", filename);
		return false;
	}

	// Validating links to parents

	if (pScene->GetNode(0)->parent >= 0)
	{
		RCLogError("Scene's root node in '%s' has a parent.", filename);
		return false;
	}

	for (int i = 1; i < pScene->GetNodeCount(); ++i)
	{
		if (pScene->GetNode(i)->parent < 0)
		{
			RCLogError("Scene in '%s' has multiple roots.", filename);
			return false;
		}

		if (pScene->GetNode(i)->parent == i)
		{
			RCLogError("Node %d in scene in '%s' has bad link to parent.", i, filename);
			return false;
		}

		if (pScene->GetNode(i)->parent > i)
		{
			RCLogError("Node %d in scene in '%s' has a forward-referenced parent.", i, filename);
			return false;
		}
	}

	// Validating children

	std::set<int> tmpIntSet;

	for (int i = 0; i < pScene->GetNodeCount(); ++i)
	{
		const std::vector<int>& children = pScene->GetNode(i)->children;

		tmpIntSet.clear();

		for (int j = 0; j < (int)children.size(); ++j)
		{
			if (children[j] < 0 || children[j] >= pScene->GetNodeCount())
			{
				RCLogError("Node %d in scene in '%s' has a invalid link (%d) to a child.", i, filename);
				return false;
			}

			if (children[j] == i)
			{
				RCLogError("Node %d in scene in '%s' has bad link to a child.", i, filename);
				return false;
			}

			if (children[j] < i)
			{
				RCLogError("Node %d in scene in '%s' has a back-referenced child.", i, filename);
				return false;
			}

			const Scene::SNode* const pChild = pScene->GetNode(children[j]);

			if (pChild->parent != i)
			{
				RCLogError("Child of node %d in scene in '%s' has wrong parent link (%d).", i, filename, pChild->parent);
				return false;
			}

			const size_t n = tmpIntSet.size();
			tmpIntSet.insert(children[j]);
			if (n == tmpIntSet.size())
			{
				RCLogError("Child of node %d in scene in '%s' has duplicate child link (%d).", i, filename, children[j]);
				return false;
			}
		}
	}

	return true;
}

string MakeEscapedXmlString(const string& str)
{
	string s = str;

	s.replace("&", "&amp;");
	s.replace("\"", "&quot;");
	s.replace("\'", "&apos;");
	s.replace("<", "&lt;");
	s.replace(">", "&gt;");

	return s;
}

void CreateManifestFile(const string& filename, Scene::IScene* pScene)
{
	XMLFileSink sink(filename);
	XMLWriter writer(&sink);

	XMLWriter::Element elemRoot(writer, "scene");

	elemRoot.Attribute("unitSizeInCm", StringHelpers::Format("%g", pScene->GetUnitSizeInCentimeters()).c_str());

	{
		XMLWriter::Element elemNodes(writer, "nodes");

		for (int i = 0; i < pScene->GetNodeCount(); ++i)
		{
			XMLWriter::Element elemNode(writer, "node");

			const Scene::SNode* const pSceneNode = pScene->GetNode(i);

			elemNode.Attribute("index", i);  // just for reference/debug

			elemNode.Attribute("name", MakeEscapedXmlString(pSceneNode->name));

			elemNode.Attribute("parentNodeIndex", pSceneNode->parent);

			for (int j = 0; j < (int)pSceneNode->attributes.size(); ++j)
			{
				XMLWriter::Element el(writer, "attribute");

				const Scene::SNode::Attribute& attrib = pSceneNode->attributes[j];

				if (attrib.eType == Scene::SNode::eAttributeType_Mesh)
				{
					el.Attribute("type", "mesh");
					el.Attribute("meshIndex", attrib.index);
				}
				else
				{
					el.Attribute("type", "unknown");
				}
			}
		}
	}

	{
		XMLWriter::Element elemNodes(writer, "meshes");

		for (int i = 0; i < pScene->GetMeshCount(); ++i)
		{
			XMLWriter::Element elemMesh(writer, "mesh");

			elemMesh.Attribute("index", i);  // just for reference/debug

			const MeshUtils::Mesh* const pMesh = pScene->GetMesh(i);
			if (pMesh)
			{
				elemMesh.Attribute("triFaceCount", pMesh->GetFaceCount());
			}
		}
	}
}

bool ValidateSceneAndCreateManifestFile(Scene::IScene* pScene, const string& sSceneFile, const string& sManifestFile)
{
	if (!ValidateSourceSceneHierarchy(sSceneFile.c_str(), pScene))
	{
		return false;
	}

	RCLog("Creating manifest file '%s'", sManifestFile.c_str());

	bool bSuccess = true;
	try
	{
		CreateManifestFile(sManifestFile, pScene);
	}
	catch (...)
	{
		bSuccess = false;
	}

	if (!bSuccess || !FileUtil::FileExists(sManifestFile.c_str()))
	{
		RCLogError("Failed to create manifest file '%s' for '%s'", sManifestFile.c_str(), sSceneFile.c_str());
		::remove(sManifestFile.c_str());
		return false;
	}

	return true;
}


// Compute coordinate system transform to convert everything to CryEngine's CS.
bool ComputeSceneAxisTransform(
	Matrix34& axesTM,
	const Scene::IScene* const pScene,
	const CImportRequest& ir)
{
	const string cryEngineForwardUp = "-Y+Z";

	string sceneForwardUp = ir.forwardUpAxes;
	{
		Matrix33 unused;
		if (TransformHelpers::GetForwardUpAxesMatrix(unused, sceneForwardUp.c_str()) != 0)
		{
			sceneForwardUp = pScene->GetForwardUpAxes();
			if (TransformHelpers::GetForwardUpAxesMatrix(unused, sceneForwardUp.c_str()) != 0)
			{
				sceneForwardUp = "-Y+Z";
			}
		}
	}

	RCLog("Converting scene's forward & up coordinate axes '%s' to CryEngine's '%s'", sceneForwardUp.c_str(), cryEngineForwardUp.c_str());

	const char* const err = TransformHelpers::ComputeForwardUpAxesTransform(axesTM, sceneForwardUp.c_str(), cryEngineForwardUp.c_str());
	if (err)
	{
		RCLogError("Failed to compute scene transform: %s", err);
		return false;
	}

	return true;
}

float ComputeSceneScale(
	const Scene::IScene* const pScene,
	const CImportRequest& ir)
{
	// Unit size.
	double sourceUnitSizeInCm;
	{
		struct Unit
		{
			double sizeInCm;
			const char* name0;
			const char* name1;
			const char* name2;
		};

		static const Unit units[] = 
		{
			{      0.00, "",   "file",       "fromfile" },
			{      0.10, "mm", "millimeter", "millimeters" },
			{      1.00, "cm", "centimeter", "centimeters" },
			{      2.54, "in", "inch",       "inches" },
			{     10.00, "dm", "decimeter",  "decimeters" },
			{     30.48, "ft", "foot",       "feet" },
			{     91.44, "yd", "yard",       "yards" },
			{    100.00, "m",  "meter",      "meters" },
			{ 100000.00, "km", "kilometer",  "kilometers" }
		};

		sourceUnitSizeInCm = -1;

		for (int i = 0; i < CRY_ARRAY_COUNT(units); ++i)
		{
			if (StringHelpers::EqualsIgnoreCase(ir.sourceUnitSizeText, units[i].name0) ||
				StringHelpers::EqualsIgnoreCase(ir.sourceUnitSizeText, units[i].name1) ||
				StringHelpers::EqualsIgnoreCase(ir.sourceUnitSizeText, units[i].name2))
			{
				sourceUnitSizeInCm = units[i].sizeInCm;
				break;
			}
		}

		if (sourceUnitSizeInCm < 0)
		{
			RCLog("Unknown source unit size specified: '%s'. Using unit settings from the asset file.", ir.sourceUnitSizeText.c_str());
		}

		if (sourceUnitSizeInCm <= 0)
		{
			sourceUnitSizeInCm = pScene->GetUnitSizeInCentimeters();
		}
	}
	RCLog("Source unit size: %g cm", sourceUnitSizeInCm);

	// User scale.
	const float userScale = (ir.scale <= 0) ? 1.0f : ir.scale;
	RCLog("User scale: %g", userScale);

	// The final (composite) scale.
	const float scale = float(double(userScale) * (sourceUnitSizeInCm * 0.01));
	RCLog("Final scale (includes user scale and conversion to meters): %f", scale);

	return scale;
}

bool ValidateAndExportScene(
	const string& outputCgfFilename,
	Scene::IScene* pScene,
	const string& sourceSceneFilename,
	const CImportRequest& ir,
	const ConvertContext& convertContext)
{
	using namespace FbxConverter_Private;

	if (!ValidateSourceSceneHierarchy(sourceSceneFilename.c_str(), pScene))
	{
		return false;
	}

	Matrix34 axesTM;
	if (!ComputeSceneAxisTransform(axesTM, pScene, ir))
	{
		return false;
	}
	const float scale = ComputeSceneScale(pScene, ir);

	CAsIsResultingScene scene(&convertContext, pScene, axesTM, scale);
	return scene.Save(outputCgfFilename, ir);
}

} // namespace

bool CFbxConverter::Process()
{
	using namespace FbxConverter_Private;

	const string inputFilepath = m_CC.GetSourcePath();

	// Manifest creation (if requested)
	{
		const string sManifestFile = m_CC.config->GetAsString("manifest", "", "");
		if (!sManifestFile.empty())
		{
			RCLog("Loading scene file '%s'", inputFilepath.c_str());

			CryFbxScene scene;

			scene.LoadScene(inputFilepath, m_CC.pRC->GetVerbosityLevel() > 0, false, false);

			return ValidateSceneAndCreateManifestFile(&scene, inputFilepath, sManifestFile);
		}
	}

	// Importing scene from a geometry file

	CImportRequest ir;

	if (StringHelpers::EndsWithIgnoreCase(inputFilepath.c_str(), ".fbx"))
	{
		RCLogError("FbxConverter expected .json or .cgf file, but has received .fbx file '%s'", inputFilepath.c_str());
		return false;
	}

	if (!ir.LoadFromFile(inputFilepath.c_str()))
	{
		return false;
	}

	if (ir.outputFilenameExt.compare("cgf") != 0
		&& ir.outputFilenameExt.compare("chr") != 0
		&& ir.outputFilenameExt.compare("skin") != 0
		&& ir.outputFilenameExt.compare("caf") != 0
		&& ir.outputFilenameExt.compare("i_caf") != 0)
	{
		RCLogError("Unsupported output file format '%s'.", ir.outputFilenameExt.c_str());
		return false;
	}

	string sourceSceneFilename;

	if (ir.sourceFilename.empty())
	{
		sourceSceneFilename = PathUtil::ReplaceExtension(inputFilepath, "fbx");
	}
	else
	{
		if (PathHelpers::IsRelative(ir.sourceFilename))
		{
			sourceSceneFilename = PathUtil::Make(PathUtil::GetPathWithoutFilename(inputFilepath), ir.sourceFilename);
		}
		else
		{
			sourceSceneFilename = ir.sourceFilename;
		}
	}

	const string overwriteSource = m_CC.config->GetAsString("overwritesourcefile", "", "");
	if (!overwriteSource.empty())
	{
		sourceSceneFilename = overwriteSource;
	}

	const string outputFilename = PathUtil::ReplaceExtension(inputFilepath, ir.outputFilenameExt).c_str();
	const bool bVerboseLog = m_CC.pRC->GetVerbosityLevel() > 0;
	const bool bCollectSkinningInfo = ir.outputFilenameExt != "cgf";
	const bool bGenerateTextureCoordinates = ir.bIgnoreTextureCoordinates;

	RCLog("Importing scene from file '%s' to '%s'", sourceSceneFilename.c_str(), outputFilename.c_str());

	CryFbxScene scene;

	if (!scene.LoadScene(sourceSceneFilename.c_str(), bVerboseLog, bCollectSkinningInfo, bGenerateTextureCoordinates))
	{
		return false;
	}

	if (!ValidateAndExportScene(outputFilename, &scene, sourceSceneFilename, ir, m_CC))
	{
		return false;
	}

	m_CC.pRC->AddInputOutputFilePair(inputFilepath, outputFilename);
	return m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, sourceSceneFilename, { outputFilename });
}

CFbxConverter::CFbxConverter()
{
}

CFbxConverter::~CFbxConverter()
{
}

void CFbxConverter::BeginProcessing(const IConfig* config)
{
}

const char* CFbxConverter::GetExt(int index) const
{
	switch (index)
	{
	case 0:
		return "fbx";
	default:
		return 0;
	}
}
