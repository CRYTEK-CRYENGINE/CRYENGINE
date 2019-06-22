// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExportDataTypes.h"
#include "IEditorMaterial.h"

namespace Private_ExportDataTypes
{

static void SetTexture(Export::TPath& outName, const IRenderShaderResources* pRes, int nSlot)
{
	SEfResTexture* pTex = pRes->GetTexture(nSlot);
	if (pTex)
	{
		ICryPak* const pPak = GetISystem()->GetIPak();
		CryPathString path;
		pPak->AdjustFileName(pTex->m_Name.c_str(), path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
		cry_strcat(outName, path.c_str());
	}
}

}

//////////////////////////////////////////////////////////
// SExportMesh
SExportMesh::SExportMesh()
{
	::ZeroMemory(&material, sizeof(material));
	material.opacity = 1.0f;
}

void SExportMesh::SetMaterial(IEditorMaterial* pMtl, CBaseObject* pBaseObj)
{
	using namespace Private_ExportDataTypes;

	if (!pMtl)
	{
		cry_strcpy(material.name, pBaseObj->GetName());
		return;
	}

	cry_strcpy(material.name, pMtl->GetFullName());

	IMaterial* matInfo = pMtl->GetMatInfo();
	IRenderShaderResources* pRes = matInfo->GetShaderItem().m_pShaderResources;
	if (!pRes)
		return;

	const ColorF& difColor = pRes->GetColorValue(EFTT_DIFFUSE);
	material.diffuse.r = difColor.r;
	material.diffuse.g = difColor.g;
	material.diffuse.b = difColor.b;
	material.diffuse.a = difColor.a;

	const ColorF& specColor = pRes->GetColorValue(EFTT_SPECULAR);
	material.specular.r = specColor.r;
	material.specular.g = specColor.g;
	material.specular.b = specColor.b;
	material.specular.a = specColor.a;

	material.opacity = pRes->GetStrengthValue(EFTT_OPACITY);
	material.smoothness = pRes->GetStrengthValue(EFTT_SMOOTHNESS);

	SetTexture(material.mapDiffuse, pRes, EFTT_DIFFUSE);
	SetTexture(material.mapSpecular, pRes, EFTT_SPECULAR);
	SetTexture(material.mapOpacity, pRes, EFTT_OPACITY);
	SetTexture(material.mapNormals, pRes, EFTT_NORMALS);
	SetTexture(material.mapDecal, pRes, EFTT_DECAL_OVERLAY);
	SetTexture(material.mapDisplacement, pRes, EFTT_HEIGHT);
}

//////////////////////////////////////////////////////////
// CExportObject
SExportObject::SExportObject(const char* pName) :
	m_MeshHash(0)
{
	pos.x = pos.y = pos.z = 0;
	rot.v.x = rot.v.y = rot.v.z = 0;
	rot.w = 1.0f;
	scale.x = scale.y = scale.z = 1.0f;

	nParent = -1;

	cry_strcpy(name, pName);

	materialName[0] = '\0';

	entityType = Export::eEntity;

	cameraTargetNodeName[0] = '\0';

	m_pLastObject = 0;
}

void SExportObject::SetMaterialName(const char* pName)
{
	cry_strcpy(materialName, pName);
}

void SExportObject::Weld()
{
	auto vectorLess = [](const Export::Vector3D& a, const Export::Vector3D& b) 
	{ 
		if (fabsf(a.x - b.x) < VEC_EPSILON)
		{
			if (fabsf(a.y - b.y) < VEC_EPSILON)
			{
				if (fabsf(a.z - b.z) < VEC_EPSILON)
				{
					return false;
				}
				return a.z < b.z;
			}
			return a.y < b.y;
		}
		return a.x < b.x;
	};

	auto uvLess = [](const Export::UV& a, const Export::UV& b)
	{
		constexpr float texCoordEpsilon = 0.001f;
		if (fabsf(a.u - b.u) < texCoordEpsilon)
		{
			if (fabsf(a.v - b.v) < texCoordEpsilon)
			{
				return false;
			}
			return a.v < b.v;
		}
		return a.u < b.u;
	};

	std::map<Export::Vector3D, uint32, decltype(vectorLess)> vertices(vectorLess);
	std::map<Export::Vector3D, uint32, decltype(vectorLess)> normals(vectorLess);
	std::map<Export::UV, uint32, decltype(uvLess)> texCoords(uvLess);

	uint32 vertexCount = 0;
	uint32 normalCount = 0;
	uint32 texCoordCount = 0;
	for (size_t meshIndex = 0, meshCount = m_meshes.size(); meshIndex < meshCount; ++meshIndex)
	{
		SExportMesh& mesh = *m_meshes[meshIndex];
		for (size_t faceIndex = 0, faceCount = mesh.m_faces.size(); faceIndex < faceCount; ++faceIndex)
		{
			Export::Face& face = mesh.m_faces[faceIndex];
			for (int i = 0; i < 3; ++i)
			{
				const Export::Vector3D v = m_vertices[face.vertex[i]];
				const auto vertexIt = vertices.find(v);
				if (vertexIt != vertices.end())
				{
					face.vertex[i] = vertexIt->second;
				}
				else
				{
					face.vertex[i] = vertexCount;
					vertices[v] = vertexCount;
					++vertexCount;
				}

				if (!m_normals.empty())
				{
					const Export::Vector3D n = m_normals[face.normal[i]];
					const auto normalIt = normals.find(n);
					if (normalIt != normals.end())
					{
						face.normal[i] = normalIt->second;
					}
					else
					{
						face.normal[i] = normalCount;
						normals[n] = normalCount;
						++normalCount;
					}
				}

				if (!m_texCoords.empty())
				{
					const Export::UV uv = m_texCoords[face.texCoord[i]];
					const auto texCoordIt = texCoords.find(uv);
					if (texCoordIt != texCoords.end())
					{
						face.texCoord[i] = texCoordIt->second;
					}
					else
					{
						face.texCoord[i] = texCoordCount;
						texCoords[uv] = texCoordCount;
						++texCoordCount;
					}
				}
			}
		}
	}

	m_vertices.resize(vertices.size());
	for (const auto vertex : vertices)
	{
		m_vertices[vertex.second] = vertex.first;
	}

	m_normals.resize(normals.size());
	for (const auto normal : normals)
	{
		m_normals[normal.second] = normal.first;
	}

	m_texCoords.resize(texCoords.size());
	for (const auto texCoord : texCoords)
	{
		m_texCoords[texCoord.second] = texCoord.first;
	}
}

// SExportData
SExportObject* SExportData::AddObject(const char* objectName)
{
	for (size_t objectID = 0; objectID < m_objects.size(); ++objectID)
	{
		if (strcmp(m_objects[objectID]->name, objectName) == 0)
		{
			return m_objects[objectID];
		}
	}

	SExportObject* pObj = new SExportObject(objectName);
	m_objects.push_back(pObj);
	return m_objects[m_objects.size() - 1];
}

SExportObject* SExportData::AddObject(SExportObject* pObject)
{
	if (pObject)
	{
		m_objects.push_back(pObject);
		return pObject;
	}

	return nullptr;
}

void SExportData::Clear()
{
	m_objects.clear();
}
