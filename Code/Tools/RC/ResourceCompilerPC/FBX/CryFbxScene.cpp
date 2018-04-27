// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#pragma warning(disable: 4266) // 'function' : no override available for virtual member function from base 'type'; function is hidden
#pragma warning(disable: 4264) // 'virtual_function' : no override available for virtual member function from base 'class'; function is hidden

#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_Math.h>
#include <CryRenderer/VertexFormats.h>
#include <Cry3DEngine/IIndexedMesh.h>

#include "FileUtil.h"
#include "IRCLog.h"
#include "Export/MeshUtils.h"
#include "StringHelpers.h"
#include "Util.h"

#include "CryFbxScene.h"
#include "FbxConverter.h"

#ifndef FBXSDK_NEW_API
#define FBXSDK_NEW_API
#endif
#include "fbxsdk.h"

namespace 
{

Vec3 GetVec3FromFbxVector4(const FbxVector4& v)
{
	return Vec3((float)v[0], (float)v[1], (float)v[2]);
}

Quat GetQuatFromFbxVector4(const FbxVector4& q)
{
	return Quat((float)q[3], (float)q[0], (float)q[1], (float)q[2]);
}

Quat GetQuatFromFbxQuaternion(const FbxQuaternion& q)
{
	return Quat((float)q[3], (float)q[0], (float)q[1], (float)q[2]);
}

static Matrix34 GetMatrix34FromFbxAMatrix(const FbxAMatrix& m)
{
	// Note that FBX stores 4x4 matrices in non-mathematical form
	// (so translation is stored in fourth row instead of fourth column).
	const FbxVector4 x = m.GetRow(0);
	const FbxVector4 y = m.GetRow(1);
	const FbxVector4 z = m.GetRow(2);
	const FbxVector4 p = m.GetRow(3);

	Matrix34 result;

	result.m00 = (float)x[0];
	result.m10 = (float)x[1];
	result.m20 = (float)x[2];
	
	result.m01 = (float)y[0];
	result.m11 = (float)y[1];
	result.m21 = (float)y[2];

	result.m02 = (float)z[0];
	result.m12 = (float)z[1];
	result.m22 = (float)z[2];

	result.m03 = (float)p[0];
	result.m13 = (float)p[1];
	result.m23 = (float)p[2];

	return result;
}

// Get the geometry offset to a node. It is never inherited by the children.
FbxAMatrix GetGeometryOffset(const FbxNode* pFbxNode)
{
	const FbxVector4 t = pFbxNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 s = pFbxNode->GetGeometricScaling(FbxNode::eSourcePivot);
	const FbxVector4 r = pFbxNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxAMatrix offset(t, r, s);
	return offset;
}

static void GenerateTextureCoordinates(float* const res_s, float* const res_t, const float x, const float y, const float z)
{
	const float ax = ::fabs(x);
	const float ay = ::fabs(y);
	const float az = ::fabs(z);

	float s = 0.0f;
	float t = 0.0f;

	if (ax > 1e-3f || ay > 1e-3f || az > 1e-3f)
	{
		if (ax > ay)
		{
			if (ax > az)
			{
				// X rules
				s = y / ax;
				t = z / ax;
			}
			else
			{
				// Z rules
				s = x / az;
				t = y / az;
			}
		}
		else
		{
			// ax <= ay
			if (ay > az)
			{
				// Y rules
				s = x / ay;
				t = z / ay;
			}
			else
			{
				// Z rules
				s = x / az;
				t = y / az;
			}
		}
	}

	// Now the texture coordinates are in the range [-1,1].
	// We want normalized [0,1] texture coordinates.
	s = (s + 1) * 0.5f;
	t = (t + 1) * 0.5f;

	if (res_s)
	{
		*res_s = s;
	}
	if (res_t)
	{
		*res_t = t;
	}
}

class CryFbxMesh
{
public:
	FbxMesh* m_pFbxMesh;
	FbxNode* m_pFbxNode;  // needed to be stored here to access per-node materials
	bool m_bMeshReady;
	MeshUtils::Mesh* m_pMesh;
	bool m_bLinksReady;
	Scene::SLinksInfo* m_pLinks;
	const bool m_bGenerateTextureCoordinates;

public:
	CryFbxMesh(bool bGenerateTextureCoordinates)
		: m_pFbxMesh(nullptr)
		, m_pFbxNode(nullptr)
		, m_bMeshReady(false)
		, m_pMesh(nullptr)
		, m_bLinksReady(false)
		, m_pLinks(nullptr)
		, m_bGenerateTextureCoordinates(bGenerateTextureCoordinates)
	{
	}

	~CryFbxMesh()
	{
		delete m_pMesh;
		delete m_pLinks;
	}

	const MeshUtils::Mesh* GetMesh(
		const std::map<const FbxSurfaceMaterial*, int>& mapFbxMatToGlobalIndex, 
		const std::map<const FbxNode*, size_t>& mapFbxNodesToBoneIndex)
	{
		if (m_bMeshReady)
		{
			return m_pMesh;
		}

		assert(!m_pMesh);
		assert(m_pFbxNode);

		m_bMeshReady = true;

		if (m_pFbxMesh)
		{
			m_pMesh = CreateMesh(m_pFbxMesh, m_pFbxNode, mapFbxMatToGlobalIndex, m_bGenerateTextureCoordinates);

			if (m_pMesh && !mapFbxNodesToBoneIndex.empty())
			{
				m_pLinks = CreateLinksInfo(m_pFbxMesh, m_pFbxNode, mapFbxNodesToBoneIndex, *m_pMesh);
			}
		}

		return m_pMesh;
	}

private:
	static MeshUtils::Mesh* CreateMesh(
		const FbxMesh* pFbxMesh,
		const FbxNode* pFbxNode,
		const std::map<const FbxSurfaceMaterial*, int>& mapFbxMatToGlobalIndex,
		const bool bGenerateTextureCoordinates)
	{
		if (!pFbxMesh || !pFbxNode)
		{
			assert(0);
			return nullptr;
		}

		// Prepare materials

		// Node-local material index -> global index
		std::vector<int> mapMaterial;

		// Global material index to use in case of an invalid node-local material or invalid material index	in polygon
		const int badMaterial = mapFbxMatToGlobalIndex.find(nullptr)->second;

		{
			const int materialCount = pFbxNode->GetMaterialCount();
			mapMaterial.resize(materialCount, -1);

			for (int j = 0; j < materialCount; ++j)
			{
				const FbxSurfaceMaterial* const pFbxMaterial = pFbxNode->GetMaterial(j);

				std::map<const FbxSurfaceMaterial*, int>::const_iterator it = mapFbxMatToGlobalIndex.find(pFbxMaterial);

				if (it == mapFbxMatToGlobalIndex.end())
				{
					mapMaterial[j] = badMaterial;
				}
				else
				{
					mapMaterial[j] = it->second;
				}
			}
		}

		// Fill geometry

		std::unique_ptr<MeshUtils::Mesh> pMesh(new MeshUtils::Mesh());
		{
			MeshUtils::Mesh& mesh = *pMesh.get();

			const FbxVector4* const pControlPoints = pFbxMesh->GetControlPoints();
			const int* const pControlPointIndices = pFbxMesh->GetPolygonVertices();

			const FbxGeometryElementUV* const pElementUV = pFbxMesh->GetElementUV();
			const FbxGeometryElementVertexColor* const pElementVertexColor = pFbxMesh->GetElementVertexColor();

			FbxLayerElementArrayTemplate<int>* pMaterialIndices = nullptr;
			pFbxMesh->GetMaterialIndices(&pMaterialIndices); // per polygon

			const int numPolygons = pFbxMesh->GetPolygonCount();
			for (int polygonIndex = 0; polygonIndex < numPolygons; ++polygonIndex)
			{
				const int numPolygonVertices = pFbxMesh->GetPolygonSize(polygonIndex);
				if (numPolygonVertices <= 2)
				{
					continue;
				}

				int materialIndex = pMaterialIndices ? (*pMaterialIndices)[polygonIndex] : -1;
				if (materialIndex < 0 || materialIndex >= (int)mapMaterial.size())
				{
					materialIndex = badMaterial;
				}
				else
				{
					materialIndex = mapMaterial[materialIndex];
				}

				const int startIdx = pFbxMesh->GetPolygonVertexIndex(polygonIndex);

				// Triangulate the polygon as a fan and remember resulting vertices and faces

				int firstIndex = -1;
				int prevIndex = -1;

				MeshUtils::Face face;
				int verticesInFace = 0;

				for (int i = 0; i < numPolygonVertices; ++i)
				{
					const int newIndex = (int)mesh.m_positions.size();

					const int polygonVertexIndex = startIdx + i;
					const int controlPointIndex = pControlPointIndices[polygonVertexIndex];

					FbxVector4 pos = pControlPoints[controlPointIndex];

					FbxVector4 nor;
					pFbxMesh->GetPolygonVertexNormal(polygonIndex, i, nor);

					FbxVector2 texCoord(0, 0);
					if (pElementUV && !bGenerateTextureCoordinates)
					{
						GetGeometryElement(texCoord, pElementUV, polygonIndex, polygonVertexIndex, controlPointIndex);
						texCoord[1] = 1.0f - texCoord[1];
					}
					else
					{
						float u, v;
						GenerateTextureCoordinates(&u, &v, float(pos[0]), float(pos[1]), float(pos[2]));
						texCoord[0] = u;
						texCoord[1] = v;
					}

					FbxColor color(1, 1, 1, 1);
					if (pElementVertexColor)
					{
						GetGeometryElement(color, pElementVertexColor, polygonIndex, polygonVertexIndex, controlPointIndex);
					}

					// Position
					mesh.m_positions.push_back(Vec3(float(pos[0]), float(pos[1]), float(pos[2])));

					// Normal
					nor[3] = 0;
					nor.Normalize();
					mesh.m_normals.push_back(Vec3(float(nor[0]), float(nor[1]), float(nor[2])));

					// Texture coordinates
					mesh.m_texCoords.push_back(Vec2(float(texCoord[0]), float(texCoord[1])));

					// Color & alpha
					if (pElementVertexColor)
					{
						MeshUtils::Color c;
						c.r = (uint8)Util::getClamped(256 * color.mRed, 0.0, 255.0);
						c.g = (uint8)Util::getClamped(256 * color.mGreen, 0.0, 255.0);
						c.b = (uint8)Util::getClamped(256 * color.mBlue, 0.0, 255.0);
						mesh.m_colors.push_back(c);

						const uint8 a = (uint8)Util::getClamped(256 * color.mAlpha, 0.0, 255.0);
						mesh.m_alphas.push_back(a);
					}

					// Topology ID
					mesh.m_topologyIds.push_back(controlPointIndex);

					// Add face
					{
						if (i == 0)
						{
							firstIndex = newIndex;
						}

						int v[3];
						int n = 0;

						v[n++] = newIndex;

						if (i >= 3)
						{
							v[n++] = firstIndex;
							v[n++] = prevIndex;
						}

						for (int j = 0; j < n; ++j)
						{
							face.vertexIndex[verticesInFace++] = v[j];
							if (verticesInFace == 3)
							{
								verticesInFace = 0;
								mesh.m_faces.push_back(face);
								mesh.m_faceMatIds.push_back(materialIndex);
							}
						}

						prevIndex = newIndex;
					}
				}

				if (verticesInFace != 0)
				{
					assert(0);
					RCLogError("Internal error in mesh filler");
					return nullptr;
				}
			}

			if (mesh.GetVertexCount() <= 0 || mesh.GetFaceCount() <= 0)
			{
				RCLogError("Missing geometry data in mesh node");
				return nullptr;
			}
		}

		FillMorphingInfo(*pMesh, *pFbxMesh, *pFbxNode);

		return pMesh.release();
	}

	static Scene::SLinksInfo* CreateLinksInfo( 
		const FbxMesh* pFbxMesh, 
		const FbxNode* pFbxNode, 
		const std::map<const FbxNode*, size_t>& mapFbxNodesToBoneIndex, 
		MeshUtils::Mesh& mesh)
	{
		const int skinCount = pFbxMesh->GetDeformerCount(FbxDeformer::eSkin);
		if (!skinCount)
		{
			return nullptr;
		}

		if (skinCount > 1)
		{
			RCLogError("The mesh '%s' of node '%s' has multiple skin deformers.", pFbxMesh->GetName(), pFbxNode->GetName());
			return nullptr;
		}

		const int controlPointsCount = pFbxMesh->GetControlPointsCount();
		const int skinIndex = 0;
		const FbxSkin *pSkin = (FbxSkin*)pFbxMesh->GetDeformer(skinIndex, FbxDeformer::eSkin);
		const int clusterCount = pSkin->GetClusterCount();

		std::vector<MeshUtils::VertexLinks> links;
		links.resize(controlPointsCount);

		std::unique_ptr<Scene::SLinksInfo> pLinksInfo(new Scene::SLinksInfo());
		pLinksInfo->linkedNodes.reserve(clusterCount);

		for (int clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex)
		{
			const FbxCluster* pCluster = pSkin->GetCluster(clusterIndex);
			const FbxNode* pClusterNode = pCluster->GetLink();
			if (!pClusterNode)
			{
				continue;
			}

			const auto boneIt = mapFbxNodesToBoneIndex.find(pClusterNode);
			if (boneIt == mapFbxNodesToBoneIndex.end())
			{
				assert(0);
				continue;
			}

			const int boneId = boneIt->second;

			// Get vertex links
			{
				const int indexCount = pCluster->GetControlPointIndicesCount();
				const int* pIndices = pCluster->GetControlPointIndices();
				const double* pWeights = pCluster->GetControlPointWeights();

				for (int i = 0; i < indexCount; ++i)
				{
					const int controlPointIndex = pIndices[i];
					const float weight = (float)pWeights[i];
					links[controlPointIndex].links.emplace_back(boneId, weight);
				}
			}

			// Get bind pose
			{
				FbxAMatrix transform;
				pCluster->GetTransformMatrix(transform);

				FbxAMatrix linkTransform;
				pCluster->GetTransformLinkMatrix(linkTransform);

				pLinksInfo->linkedNodes.emplace_back(
					GetMatrix34FromFbxAMatrix(transform),
					GetMatrix34FromFbxAMatrix(linkTransform),
					boneId);
			}
		}

		// Remap topology vertex links to mesh vertex links
		if (!links.empty())
		{
			const int vertexCount = mesh.m_positions.size();
			mesh.m_links.reserve(vertexCount);
			for (size_t i = 0; i < vertexCount; ++i)
			{
				mesh.m_links.push_back(links[mesh.m_topologyIds[i]]);
			}
		}

		return pLinksInfo.release();
	}

	static void FillMorphingInfo(
		MeshUtils::Mesh& mesh,
		const FbxMesh& fbxMesh,
		const FbxNode& fbxNode)
	{
		const int blendShapeCount = fbxMesh.GetDeformerCount(FbxDeformer::eBlendShape);
		if (!blendShapeCount)
		{
			return;
		}

		if (blendShapeCount > 1)
		{
			RCLogError("The mesh '%s' of node '%s' has multiple Blend Shape deformers.", fbxMesh.GetName(), fbxMesh.GetName());
			return;
		}

		assert(mesh.m_topologyIds.size() == mesh.m_positions.size());

		const int vertexCount = mesh.m_positions.size();
		const int topologyVertexCount = fbxMesh.GetControlPointsCount();

		auto& morphTargets = mesh.m_morphTargets;

		for (int blendShapeIndex = 0; blendShapeIndex < blendShapeCount; ++blendShapeIndex)
		{
			FbxBlendShape* pBlendShape = (FbxBlendShape*)fbxMesh.GetDeformer(blendShapeIndex, FbxDeformer::eBlendShape);
			const int channelCount = pBlendShape->GetBlendShapeChannelCount();

			for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex)
			{
				FbxBlendShapeChannel* pChannel = pBlendShape->GetBlendShapeChannel(channelIndex);
				const int targetCount = pChannel->GetTargetShapeCount();

				if (targetCount > 1)
				{
					RCLogWarning("Blend Shape Channel '%s' of node '%s' has has multiple target shapes (progressive morph).", pChannel->GetName(), fbxMesh.GetName());
				}

				morphTargets.reserve(morphTargets.size() + targetCount);

				for (int targetIndex = 0; targetIndex < targetCount; ++targetIndex)
				{
					FbxShape* pTarget = pChannel->GetTargetShape(targetIndex);
					const int controlPointsCount = pTarget->GetControlPointsCount();
					if (controlPointsCount != topologyVertexCount)
					{
						RCLogWarning("Morph target '%s' of node '%s' has different topology to base mesh and will not be imported.", pTarget->GetName(), fbxMesh.GetName());
						continue;
					}

					morphTargets.resize(morphTargets.size() + 1);
					morphTargets.back().name = pTarget->GetName();
					morphTargets.back().positions.reserve(mesh.m_positions.size());

					const FbxVector4* pControlPoints = pTarget->GetControlPoints();

					for (int i = 0; i < vertexCount; ++i)
					{
						const int j = mesh.m_topologyIds[i];
						const Vec3 v(GetVec3FromFbxVector4(pControlPoints[j]));
						morphTargets.back().positions.push_back(v);
					}
				}
			}
		}
	}

	template <typename ValueType, typename ElementArrayType>
	static void GetGeometryElement(ValueType& value, const ElementArrayType* pElementArray, int polygonIndex, int polygonVertexIndex, int controlPointIndex)
	{
		if (!pElementArray)
		{
			return;
		}

		int index;
		switch (pElementArray->GetMappingMode())
		{
		case FbxGeometryElement::eByControlPoint:
			// One mapping coordinate for each surface control point/vertex.
			index = controlPointIndex;
			break;
		case FbxGeometryElement::eByPolygonVertex:
			// One mapping coordinate for each vertex, for every polygon of which it is a part.
			// This means that a vertex will have as many mapping coordinates as polygons of which it is a part.
			index = polygonVertexIndex;
			break;
		case FbxGeometryElement::eByPolygon:
			// One mapping coordinate for the whole polygon.
			index = polygonIndex;
			break;
		default:
			return;
		}

		if (pElementArray->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
		{
			// Convert index from "index of value's index" to "index of value".
			const FbxLayerElementArrayTemplate<int>& indices = pElementArray->GetIndexArray();
			index = indices.GetAt(index);
		}

		const FbxLayerElementArrayTemplate<ValueType>& elements = pElementArray->GetDirectArray();
		value = elements.GetAt(index);
	}
};

class CryFbxNode
{
public:
	Scene::SNode m_node;
	FbxNode* m_pFbxNode;

public:
	CryFbxNode()
		: m_pFbxNode(nullptr)
	{
	}

	~CryFbxNode()
	{
	}
};

class CryFbxMaterial
{
public:
	Scene::SMaterial m_material;

public:
	CryFbxMaterial()
	{
	}

	~CryFbxMaterial()
	{
	}
};

} // namespace

//////////////////////////////////////////////////////////////////////////

namespace
{

class CryFbxManager
{
public:
	FbxManager* m_pFbxManager;
	FbxScene* m_pFbxScene;

public:
	CryFbxManager()
		: m_pFbxManager(nullptr)
		, m_pFbxScene(nullptr)
	{
	}

	~CryFbxManager()
	{
		Reset();
	}

	void Reset()
	{
		if (m_pFbxManager)
		{
			m_pFbxManager->Destroy();
			m_pFbxManager = nullptr;
		}
		m_pFbxScene = nullptr;
	}

	// TODO: This code is duplicated in MeshImporter.
	static void AdjustIOSettings(FbxIOSettings* pIOSettings)
	{
		assert(pIOSettings);

		// General
		pIOSettings->SetBoolProp(IMP_PRESETS, true);
		pIOSettings->SetBoolProp(IMP_STATISTICS, true);
		pIOSettings->SetBoolProp(IMP_GEOMETRY, true);
		pIOSettings->SetBoolProp(IMP_ANIMATION, true);
		pIOSettings->SetBoolProp(IMP_LIGHT, false);
		pIOSettings->SetBoolProp(IMP_ENVIRONMENT, false);
		pIOSettings->SetBoolProp(IMP_CAMERA, false);
		pIOSettings->SetBoolProp(IMP_VIEW_CUBE, false);
		pIOSettings->SetBoolProp(IMP_ZOOMEXTENTS, false);
		pIOSettings->SetBoolProp(IMP_GLOBAL_AMBIENT_COLOR, false);
		pIOSettings->SetBoolProp(IMP_META_DATA, false);
		pIOSettings->SetBoolProp(IMP_REMOVEBADPOLYSFROMMESH, false);   // ?
		pIOSettings->SetStringProp(IMP_EXTRACT_FOLDER, ".");

		// FBX
		pIOSettings->SetBoolProp(IMP_FBX_TEMPLATE, true);
		pIOSettings->SetBoolProp(IMP_FBX_PIVOT, true);
		pIOSettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
		pIOSettings->SetBoolProp(IMP_FBX_CHARACTER, true);
		pIOSettings->SetBoolProp(IMP_FBX_CONSTRAINT, false);
		pIOSettings->SetBoolProp(IMP_FBX_MERGE_LAYER_AND_TIMEWARP, false);
		pIOSettings->SetBoolProp(IMP_FBX_GOBO, false);
		pIOSettings->SetBoolProp(IMP_FBX_SHAPE, true);
		pIOSettings->SetBoolProp(IMP_FBX_LINK, false);
		pIOSettings->SetBoolProp(IMP_FBX_MATERIAL, true);
		pIOSettings->SetBoolProp(IMP_FBX_TEXTURE, false);
		pIOSettings->SetBoolProp(IMP_FBX_MODEL, true);
		pIOSettings->SetBoolProp(IMP_FBX_ANIMATION, true);
		pIOSettings->SetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, false);   // textures etc only needed for materials

		// 3DS
		pIOSettings->SetBoolProp(IMP_3DS_REFERENCENODE, false);
		pIOSettings->SetBoolProp(IMP_3DS_TEXTURE, true);
		pIOSettings->SetBoolProp(IMP_3DS_MATERIAL, true);
		pIOSettings->SetBoolProp(IMP_3DS_ANIMATION, true);
		pIOSettings->SetBoolProp(IMP_3DS_MESH, true);
		pIOSettings->SetBoolProp(IMP_3DS_LIGHT, false);
		pIOSettings->SetBoolProp(IMP_3DS_CAMERA, false);
		pIOSettings->SetBoolProp(IMP_3DS_AMBIENT_LIGHT, false);
		pIOSettings->SetBoolProp(IMP_3DS_RESCALING, true);
		pIOSettings->SetBoolProp(IMP_3DS_FILTER, false);
		pIOSettings->SetBoolProp(IMP_3DS_SMOOTHGROUP, false);

		// OBJ
		pIOSettings->SetBoolProp(IMP_OBJ_REFERENCE_NODE, false);

		// DXF
		pIOSettings->SetBoolProp(IMP_DXF_WELD_VERTICES, false);
		pIOSettings->SetBoolProp(IMP_DXF_OBJECT_DERIVATION, false);
		pIOSettings->SetBoolProp(IMP_DXF_REFERENCE_NODE, false);
	}

	bool LoadScene(const char* fbxFilename)
	{
		Reset();

		m_pFbxManager = FbxManager::Create();

		if (!m_pFbxManager)
		{
			RCLogError("Failed to create FBX manager");
			return false;
		}

		FbxIOSettings* const pSettings = FbxIOSettings::Create(m_pFbxManager, IOSROOT);

		if (!pSettings)
		{
			RCLogError("Failed to create FBX I/O object");
			Reset();
			return false;
		}

		AdjustIOSettings(pSettings);

		m_pFbxManager->SetIOSettings(pSettings);

		FbxImporter* const pImporter = FbxImporter::Create(m_pFbxManager, "");

		if (!pImporter)
		{
			RCLogError("Failed to create FBX importer");
			Reset();
			return false;
		}

		if (!pImporter->Initialize(fbxFilename, -1, m_pFbxManager->GetIOSettings()))
		{
			RCLogError("Call to FbxImporter::Initialize('%s') failed: '%s'", fbxFilename, pImporter->GetStatus().GetErrorString());
			Reset();
			return false;
		}

		// Create a new FBX scene so it can be populated by the imported file.
		m_pFbxScene = FbxScene::Create(m_pFbxManager, "myScene");

		if (!m_pFbxScene)
		{
			RCLogError("Failed to create FBX scene");
			Reset();
			return false;
		}

		if (!pImporter->Import(m_pFbxScene))
		{
			RCLogError("Failed to import FBX data from '%s': '%s'", fbxFilename, pImporter->GetStatus().GetErrorString());
			Reset();
			return false;
		}

		pImporter->Destroy();

		return true;
	}
};

} // namespace

//////////////////////////////////////////////////////////////////////////

struct CryFbxSceneImpl
{
	CryFbxManager m_fbxMan;

	char m_forwardUpAxes[5];  // "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>"

	double m_unitSizeInCentimeters;

	std::vector<CryFbxNode> m_nodes;
	std::vector<CryFbxMesh> m_meshes;

	std::vector<CryFbxMaterial> m_materials;
	std::map<const FbxSurfaceMaterial*, int> m_mapFbxMatToGlobalIndex;

	std::vector<size_t> m_boneNodes; 
	std::map<const FbxNode*, size_t> m_mapFbxNodesToBoneIndex;

	std::vector<Scene::SAnimation> m_animations;

	CryFbxSceneImpl()
	{
		Reset();
	}

	~CryFbxSceneImpl()
	{
		Reset();
	}

	void Reset()
	{
		m_nodes.clear();
		m_meshes.clear();
		m_materials.clear();
		m_mapFbxMatToGlobalIndex.clear();
		m_fbxMan.Reset();
		m_forwardUpAxes[0] = 0;
		m_unitSizeInCentimeters = 1.0;
	}

	void PrepareMaterials()
	{
		m_materials.clear();
		m_mapFbxMatToGlobalIndex.clear();

		// Collect materials from all modes, except the root node (it's an auxiliary node)
		for (size_t i = 1; i < m_nodes.size(); ++i)
		{
			CryFbxNode& node = m_nodes[i];

			if (!node.m_pFbxNode)
			{
				continue;
			}

			const int materialCount = node.m_pFbxNode->GetMaterialCount();

			for (int j = 0; j < materialCount; ++j)
			{
				const FbxSurfaceMaterial* const pFbxMaterial = node.m_pFbxNode->GetMaterial(j);

				if (!pFbxMaterial || pFbxMaterial->GetName()[0] == 0)
				{
					continue;
				}

				std::map<const FbxSurfaceMaterial*, int>::const_iterator it = m_mapFbxMatToGlobalIndex.find(pFbxMaterial);

				if (it == m_mapFbxMatToGlobalIndex.end())
				{
					m_mapFbxMatToGlobalIndex[pFbxMaterial] = (int)m_materials.size();
					CryFbxMaterial m;
					m.m_material.name = pFbxMaterial->GetName();
					m_materials.push_back(m);
				}
			}
		}

		// Create a special material - all materials without name and all invalid material references will be mapped to it
		{
			m_mapFbxMatToGlobalIndex[nullptr] = (int)m_materials.size();
			CryFbxMaterial m;
			m.m_material.name = "";
			m_materials.push_back(m);
		}
	}

	bool PrepareBones()
	{
		bool bHasSkin = std::any_of(m_meshes.begin(), m_meshes.end(), [](const CryFbxMesh& mesh)
		{
			return mesh.m_pFbxMesh->GetDeformerCount(FbxDeformer::eSkin);
		});

		if (!bHasSkin)
		{
			RCLogError("FBX Scene does not have any skinned mesh.");
			return false;
		}

		for (int i = 0; i < m_nodes.size(); ++i)
		{
			m_mapFbxNodesToBoneIndex[m_nodes[i].m_pFbxNode] = i;
		}
		return true;
	}

	void PrepareAnimations()
	{
		FbxScene* pFbxScene = m_fbxMan.m_pFbxScene;

		FbxTimeSpan timeLineTimeSpan;
		pFbxScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(timeLineTimeSpan);

		FbxArray<FbxString*> animStackNameArray;
		pFbxScene->FillAnimStackNameArray(animStackNameArray);

		m_animations.resize(animStackNameArray.Size());

		for (int i = 0; i < animStackNameArray.Size(); ++i)
		{
			FbxTime start = timeLineTimeSpan.GetStart();
			FbxTime stop = timeLineTimeSpan.GetStop();

			const FbxTakeInfo* const pTakeInfo = pFbxScene->GetTakeInfo(*(animStackNameArray[i]));
			if (pTakeInfo)
			{
				start = pTakeInfo->mLocalTimeSpan.GetStart();
				stop = pTakeInfo->mLocalTimeSpan.GetStop();
			}

			m_animations[i].name = animStackNameArray[i]->Buffer();
			m_animations[i].startFrame = (int)start.GetFrameCount();
			m_animations[i].endFrame = (int)stop.GetFrameCount();
		}
	}
};

//////////////////////////////////////////////////////////////////////////

CryFbxScene::CryFbxScene()
	: m_pImpl(new CryFbxSceneImpl())
{
	assert(m_pImpl);
}

CryFbxScene::~CryFbxScene()
{	
	Reset();
	delete m_pImpl;
}

void CryFbxScene::Reset()
{
	m_pImpl->Reset();
}

bool CryFbxScene::LoadScene(const char * fbxFilename, bool bVerboseLog, bool bCollectSkinningInfo, bool bGenerateTextureCoordinates)
{
	if (!LoadScene_StaticMesh(fbxFilename, bVerboseLog, bGenerateTextureCoordinates) || (bCollectSkinningInfo && !m_pImpl->PrepareBones()))
	{
		return false;
	}
	m_pImpl->PrepareAnimations();
	return true;
}

//////////////////////////////////////////////////////////////////////////


static FbxAMatrix GetFbxNodeWorldTransform(FbxNode* pFbxNode, const FbxTime time = FbxTime(FBXSDK_TC_INFINITY))
{
	if (!pFbxNode)
	{
		FbxAMatrix m;
		m.SetIdentity();
		return m;
	}

	return pFbxNode->EvaluateGlobalTransform(time);
}

const char* CryFbxScene::GetForwardUpAxes() const
{
	return &m_pImpl->m_forwardUpAxes[0];
}


double CryFbxScene::GetUnitSizeInCentimeters() const
{
	return m_pImpl->m_unitSizeInCentimeters;
}


int CryFbxScene::GetNodeCount() const
{
	return (int)m_pImpl->m_nodes.size();
}

const Scene::SNode* CryFbxScene::GetNode(int idx) const
{
	if (idx < 0 || idx >= GetNodeCount())
	{
		return nullptr;
	}
	return &m_pImpl->m_nodes[idx].m_node;
}

Scene::STrs CryFbxScene::EvaluateNodeLocalTransform(int nodeIdx, int frameIndex) const
{
	Scene::STrs trs;

	if (nodeIdx < 0 || nodeIdx >= GetNodeCount())
	{
		return trs;
	}

	FbxTime fbxTime;
	fbxTime.SetFrame(frameIndex);
	FbxAMatrix fbxTransform = m_pImpl->m_nodes[nodeIdx].m_pFbxNode->EvaluateLocalTransform(fbxTime);

	trs.translation = GetVec3FromFbxVector4(fbxTransform.GetT());
	trs.rotation = GetQuatFromFbxQuaternion(fbxTransform.GetQ());
	trs.scale = GetVec3FromFbxVector4(fbxTransform.GetS());

	return trs;
}

Scene::STrs CryFbxScene::EvaluateNodeGlobalTransform(int nodeIdx, int frameIndex) const
{
	Scene::STrs trs;

	if (nodeIdx < 0 || nodeIdx >= GetNodeCount())
	{
		return trs;
	}

	FbxTime fbxTime;
	fbxTime.SetFrame(frameIndex);
	FbxAMatrix fbxTransform = m_pImpl->m_nodes[nodeIdx].m_pFbxNode->EvaluateGlobalTransform(fbxTime);

	trs.translation = GetVec3FromFbxVector4(fbxTransform.GetT());
	trs.rotation = GetQuatFromFbxQuaternion(fbxTransform.GetQ());
	trs.scale = GetVec3FromFbxVector4(fbxTransform.GetS());

	return trs;
}

int CryFbxScene::GetMeshCount() const
{
	return (int)m_pImpl->m_meshes.size();
}

const MeshUtils::Mesh* CryFbxScene::GetMesh(int idx) const
{
	if (idx < 0 || idx >= GetMeshCount())
	{
		return nullptr;
	}
	return m_pImpl->m_meshes[idx].GetMesh(m_pImpl->m_mapFbxMatToGlobalIndex, m_pImpl->m_mapFbxNodesToBoneIndex);
}

const Scene::SLinksInfo* CryFbxScene::GetMeshLinks(int idx) const
{
	if (!GetMesh(idx))
	{
		return nullptr;
	}

	return m_pImpl->m_meshes[idx].m_pLinks;
}

int CryFbxScene::GetMaterialCount() const
{
	return (int)m_pImpl->m_materials.size();
}

const Scene::SMaterial* CryFbxScene::GetMaterial(int idx) const
{
	if (idx < 0 || idx >= GetMaterialCount())
	{
		return nullptr;
	}
	return &m_pImpl->m_materials[idx].m_material;
}

int CryFbxScene::GetAnimationStackCount() const
{
	return (int)m_pImpl->m_animations.size();
}
const Scene::SAnimation* CryFbxScene::GetAnimationStack(int idx) const
{
	if (idx < 0 || idx >= GetAnimationStackCount())
	{
		return nullptr;
	}

	return &m_pImpl->m_animations[idx];
}

bool CryFbxScene::SetCurrentAnimationStack(int idx)
{
	if (idx < 0 || idx >= GetAnimationStackCount())
	{
		return false;
	}

	FbxScene* pFbxScene = m_pImpl->m_fbxMan.m_pFbxScene;
	const char* szName = GetAnimationStack(idx)->name.c_str();
	FbxAnimStack* pAnimationStack = pFbxScene->FindMember<FbxAnimStack>(szName);
	if (!pAnimationStack)
	{
		RCLogError("Can't set animation stack '%s'", szName);
		return false;
	}

	pFbxScene->SetCurrentAnimationStack(pAnimationStack);
	return true;
}


bool CryFbxScene::LoadScene_StaticMesh(const char* fbxFilename, bool bVerboseLog, bool bGenerateTextureCoordinates)
{
	Reset();

	if (!FileUtil::FileExists(fbxFilename))
	{
		RCLogError("Can't find scene file '%s'", fbxFilename);
		return false;
	}

	if (!m_pImpl->m_fbxMan.LoadScene(fbxFilename))
	{
		Reset();
		return false;
	}

	// Strip the scene of all Fbx_Root nodes - to prevent Fbx_Root
	// nodes to appear in the node paths (full names) we produce.
	FbxRootNodeUtility::RemoveAllFbxRoots(m_pImpl->m_fbxMan.m_pFbxScene);

	// Coordinate axes Forward and Up
	{
		FbxAxisSystem axisSystem = m_pImpl->m_fbxMan.m_pFbxScene->GetGlobalSettings().GetAxisSystem();
		if (axisSystem.GetCoorSystem() != FbxAxisSystem::eRightHanded)
		{
			RCLogError("Scene coordinate system in %s is left-handed. We support only right-handed scenes.", fbxFilename);
			Reset();
			return false;
		}

		int sign = 0;

		const char* possibleForwardAxes;

		switch (axisSystem.GetUpVector(sign))
		{
		case FbxAxisSystem::eXAxis:
			m_pImpl->m_forwardUpAxes[3] = 'X';
			possibleForwardAxes = "YZ";
			break;
		case FbxAxisSystem::eYAxis:
			m_pImpl->m_forwardUpAxes[3] = 'Y';
			possibleForwardAxes = "XZ";
			break;
		case FbxAxisSystem::eZAxis:
			m_pImpl->m_forwardUpAxes[3] = 'Z';
			possibleForwardAxes = "XY";
			break;
		default:
			RCLogError("Scene coordinate system info in %s is damaged: unknown value of axis 'up'.", fbxFilename);
			Reset();
			return false;
		}
		m_pImpl->m_forwardUpAxes[2] = sign < 0 ? '-' : '+';

		switch (axisSystem.GetFrontVector(sign))
		{
		case FbxAxisSystem::eParityEven:
			m_pImpl->m_forwardUpAxes[1] = possibleForwardAxes[0];
			break;
		case FbxAxisSystem::eParityOdd:
			m_pImpl->m_forwardUpAxes[1] = possibleForwardAxes[1];
			break;
		default:
			RCLogError("Scene coordinate system info in %s is damaged: unknown value of axis 'forward'.", fbxFilename);
			Reset();
			return false;
		}
		m_pImpl->m_forwardUpAxes[0] = sign < 0 ? '-' : '+';
			
		m_pImpl->m_forwardUpAxes[4] = 0;
	}

	// Unit size
	{
		double unitSizeInCm = m_pImpl->m_fbxMan.m_pFbxScene->GetGlobalSettings().GetSystemUnit().GetScaleFactor();
		if (unitSizeInCm <= 0)
		{
			unitSizeInCm = 1.0;
		}

		struct Unit
		{
			const char* name;
			double sizeInCm;
		} units[] = 
		{
			{ "millimeter", 0.1 },
			{ "centimeter", 1.0 },
			{ "inch", 2.54 },
			{ "decimeter", 10.0 },
			{ "foot", 30.48 },
			{ "yard", 91.44 },
			{ "meter", 100.0 },
			{ "kilometer", 100000.0 }
		};

		const char* unitName = nullptr;
		for (int i = 0; i < CRY_ARRAY_COUNT(units); ++i)
		{
			if (unitSizeInCm * 0.9999 <= units[i].sizeInCm && units[i].sizeInCm <= unitSizeInCm * 1.0001)
			{
				unitName = units[i].name;
				break;
			}
		}

		if (!unitName)
		{
			unitName = "custom";
		}

		m_pImpl->m_unitSizeInCentimeters = unitSizeInCm;

		RCLog("FBX unit: %s, %g cm", unitName, unitSizeInCm);
	}

	FbxNode* const pRootFbxNode = m_pImpl->m_fbxMan.m_pFbxScene->GetRootNode();
	if (!pRootFbxNode)
	{
		RCLogError("The scene in FBX file is empty");
		Reset();
		return false;
	}

	// Collect nodes worth exporting by traversing node subtrees

	assert(m_pImpl->m_nodes.empty());

	CryFbxNode node;

	node.m_pFbxNode = pRootFbxNode;
	node.m_node.name = "<root>";
	node.m_node.parent = -1;
	node.m_node.geometryOffset = Matrix34(IDENTITY);
	node.m_node.worldTransform = Matrix34(IDENTITY);  // TODO: maybe it's better to set zeros only, to prevent callers from using it

	if (bVerboseLog)
	{
		RCLog("Root node: '%s' (storing as '<root>')", pRootFbxNode->GetName());
	}

	m_pImpl->m_nodes.push_back(node);

	for (int parent = 0; parent < (int)m_pImpl->m_nodes.size(); ++parent)
	{
		for (int i = 0; i < m_pImpl->m_nodes[parent].m_pFbxNode->GetChildCount(); ++i)
		{
			FbxNode* const pFbxNode = m_pImpl->m_nodes[parent].m_pFbxNode->GetChild(i);
			assert(pFbxNode);

			if (bVerboseLog)
			{
				RCLog("Node: '%s'", pFbxNode->GetName());
			}

			m_pImpl->m_nodes[parent].m_node.children.push_back((int)m_pImpl->m_nodes.size());

			node.m_pFbxNode = pFbxNode;
			node.m_node.name = pFbxNode->GetName();
			node.m_node.parent = parent;
			node.m_node.worldTransform = GetMatrix34FromFbxAMatrix(GetFbxNodeWorldTransform(pFbxNode));
			node.m_node.geometryOffset = GetMatrix34FromFbxAMatrix(GetGeometryOffset(pFbxNode));
			node.m_node.attributes.clear();

			for (int j = 0; j < pFbxNode->GetNodeAttributeCount(); ++j)
			{
				FbxNodeAttribute* const pAttribute = pFbxNode->GetNodeAttributeByIndex(j);
				if (pAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
				{
					const Scene::SNode::Attribute attr(Scene::SNode::eAttributeType_Mesh, (int)m_pImpl->m_meshes.size());
					node.m_node.attributes.push_back(attr);

					CryFbxMesh mesh(bGenerateTextureCoordinates);
					mesh.m_pFbxMesh = static_cast<FbxMesh*>(pAttribute);
					mesh.m_pFbxNode = pFbxNode;
					m_pImpl->m_meshes.push_back(mesh);
				}
			}			

			m_pImpl->m_nodes.push_back(node);
		}
	}

	m_pImpl->PrepareMaterials();

	return true;
}
// eof
