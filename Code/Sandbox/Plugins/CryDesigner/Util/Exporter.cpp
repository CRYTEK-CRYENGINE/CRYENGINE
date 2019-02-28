// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Exporter.h"

#include "Objects\BrushObject.h"
#include "Core/Helper.h"
#include "Objects/AreaSolidObject.h"
#include "Objects/ClipVolumeObject.h"
#include "Objects\Group.h"
#include "Material\Material.h"
#include <CryCore/Containers/CryArray.h>
#include "Util\Pakfile.h"
#include "Util/CryMemFile.h"
#include <Cry3DEngine/CGF/IChunkFile.h>
#include "Core/ModelCompiler.h"
#include "Core/Model.h"
#include "Core/PolygonDecomposer.h"
#include "Core/LoopPolygons.h"

#include <Cry3DEngine/I3DEngine.h>

#define BRUSH_SUB_FOLDER "Brush"
#define BRUSH_FILE       "brush.lst"
#define BRUSH_LIST_FILE  "brushlist.txt"

namespace Designer
{
struct brush_sort_predicate
{
	bool operator()(const std::pair<string, CBaseObject*>& left, const std::pair<string, CBaseObject*>& right)
	{
		return left.first < right.first;
	}
};

void Exporter::ExportBrushes(const string& path, CPakFile& pakFile)
{
	CLogFile::WriteLine("Exporting Brushes...");

	pakFile.RemoveDir(PathUtil::Make(path, BRUSH_SUB_FOLDER));

	string filename = PathUtil::Make(path, BRUSH_FILE);
	string brushListFilename = PathUtil::Make(path, BRUSH_LIST_FILE);
	CCryMemFile file;

	DynArray<CBaseObject*> objects;
	GetIEditor()->GetObjectManager()->GetObjects(objects);

	typedef std::vector<std::pair<string, CBaseObject*>> SortedObjects;
	SortedObjects sortedObjects;
	for (int i = 0; i < objects.size(); i++)
	{
		if (objects[i] == NULL)
			continue;
		if (!IsDesignerRelated(objects[i]))
			continue;

		string gameFileName;
		if (GenerateGameFilename(objects[i], gameFileName) == false)
			continue;

		if (!CheckIfHasValidModel(objects[i]))
			continue;

		sortedObjects.push_back(std::make_pair(gameFileName, objects[i]));
	}

	std::sort(sortedObjects.begin(), sortedObjects.end(), brush_sort_predicate());

	for (auto it = sortedObjects.begin(); it != sortedObjects.end(); ++it)
	{
		CBaseObject* pObject = it->second;

		if (pObject->GetType() != OBJTYPE_SOLID && pObject->GetType() != OBJTYPE_VOLUMESOLID)
			continue;

		if (pObject->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
		{
			AreaSolidObject* obj = (AreaSolidObject*)pObject;
			if (!ExportAreaSolid(path, obj, pakFile))
				assert(0);
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
		{
			ClipVolumeObject* pClipVolume = static_cast<ClipVolumeObject*>(pObject);
			if (!ExportClipVolume(path, pClipVolume, pakFile))
				assert(0);
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(DesignerObject)))
		{
			_smart_ptr<IStatObj> pStatObj = NULL;
			if (GetIStatObj(pObject, &pStatObj) == false)
				continue;
			string gameFileName;
			if (GenerateGameFilename(pObject, gameFileName) == false)
				continue;
			int nRenderFlag(0);
			if (GetRenderFlag(pObject, nRenderFlag) == false)
				continue;
			ExportStatObj(path, pStatObj, pObject, nRenderFlag, gameFileName, pakFile);
		}
	}

	pakFile.RemoveFile(filename);

	{
		CCryMemFile brushListFile;
		string tempStr;
		for (int i = 0; i < m_geoms.size(); i++)
		{
			tempStr = m_geoms[i].filename;
			tempStr += "\r\n";
			brushListFile.Write(tempStr.c_str(), tempStr.size());
		}
		pakFile.UpdateFile(brushListFilename, brushListFile);
	}

	CLogFile::WriteString("Done.");
}

void Exporter::ExportStatObj(const string& path, IStatObj* pStatObj, CBaseObject* pObj, int renderFlag, const string& sGeomFileName, CPakFile& pakFile)
{
	if (pStatObj == NULL || pObj == NULL)
		return;

	string sRealGeomFileName = sGeomFileName;
	sRealGeomFileName.Replace("%level%", PathUtil::ToUnixPath(PathUtil::RemoveSlash(path.GetString())).c_str());

	string sInternalGeomFileName = sGeomFileName;

	IChunkFile* pChunkFile = NULL;
	if (pStatObj->SaveToCGF(sRealGeomFileName, &pChunkFile))
	{
		void* pMemFile = NULL;
		int nFileSize = 0;
		pChunkFile->WriteToMemoryBuffer(&pMemFile, &nFileSize);
		pakFile.UpdateFile(sRealGeomFileName, pMemFile, nFileSize, true, ICryArchive::LEVEL_FASTER);
		pChunkFile->Release();
	}

	ExportedBrushGeom geom;
	ZeroStruct(geom);
	geom.size = sizeof(geom);
	cry_strcpy(geom.filename, sInternalGeomFileName);
	geom.flags = 0;
	geom.m_minBBox = pStatObj->GetBoxMin();
	geom.m_maxBBox = pStatObj->GetBoxMax();
	m_geoms.push_back(geom);
	int geomIndex = m_geoms.size() - 1;

	int mtlIndex = -1;
	CMaterial* pMaterial = (CMaterial*)pObj->GetMaterial();
	if (pMaterial)
	{
		mtlIndex = stl::find_in_map(m_mtlMap, pMaterial, -1);
		if (mtlIndex < 0)
		{
			ExportedBrushMaterial mtl;
			mtl.size = sizeof(mtl);
			memset(mtl.material, 0, sizeof(mtl.material));
			cry_strcpy(mtl.material, pMaterial->GetFullName());
			m_materials.push_back(mtl);
			mtlIndex = m_materials.size() - 1;
			m_mtlMap[pMaterial] = mtlIndex;
		}
	}

	ExportedBrushGeom* pBrushGeom = &m_geoms[geomIndex];
	if (pBrushGeom && pStatObj)
	{
		if (pStatObj->GetPhysGeom() == NULL && pStatObj->GetPhysGeom(PHYS_GEOM_TYPE_NO_COLLIDE) == NULL)
			pBrushGeom->flags |= ExportedBrushGeom::NO_PHYSICS;
	}
}

bool AppendDataToMemoryblock(CMemoryBlock& memoryBlock, int& memoryOffset, void* data, int datasize)
{
	if (memoryBlock.GetBuffer() == NULL)
		return false;
	int nextOffset = memoryOffset + datasize;
	if (nextOffset > memoryBlock.GetSize())
		return false;
	memcpy(((char*)memoryBlock.GetBuffer()) + memoryOffset, data, datasize);
	memoryOffset = nextOffset;
	return true;
}

bool ExportPolygonForAreaSolid(CMemoryBlock& memoryBlock, int& offset, const std::vector<Vertex>& vertices, const std::vector<SMeshFace>& meshFaces)
{
	int numberOfFaces(meshFaces.size());
	for (int k = 0; k < numberOfFaces; ++k)
	{
		int numberOfVertices(3);

		if (!AppendDataToMemoryblock(memoryBlock, offset, &numberOfVertices, sizeof(numberOfVertices)))
			return false;

		for (int a = 0; a < numberOfVertices; ++a)
		{
			Vec3 vPos = ToVec3(vertices[meshFaces[k].v[a]].pos);
			if (!AppendDataToMemoryblock(memoryBlock, offset, &vPos, sizeof(Vec3)))
				return false;
		}
	}
	return true;
}

bool Exporter::ExportAreaSolid(const string& path, AreaSolidObject* pAreaSolid, CPakFile& pakFile) const
{
	if (pAreaSolid == NULL)
		return false;
	ModelCompiler* pCompiler(pAreaSolid->GetCompiler());
	if (pCompiler == NULL)
		return false;
	Model* pModel(pAreaSolid->GetModel());
	if (pModel == NULL)
		return false;

	std::vector<PolygonPtr> optimizedPolygons;
	pModel->GetPolygonList(optimizedPolygons);

	int iPolygonSize = optimizedPolygons.size();
	if (iPolygonSize <= 0)
		return false;

	AreaSolidStatistic statisticForAreaSolid;
	ComputeAreaSolidMemoryStatistic(pAreaSolid, statisticForAreaSolid, optimizedPolygons);

	CMemoryBlock memoryBlock;
	memoryBlock.Allocate(statisticForAreaSolid.totalSize);

	int actualNumOfClosedPolygons = 0;
	int actualNumOfOpenPolygons = 0;
	static_assert(sizeof(actualNumOfClosedPolygons) == sizeof(statisticForAreaSolid.numOfClosedPolygons), "Data size mismatch");
	static_assert(sizeof(actualNumOfOpenPolygons) == sizeof(statisticForAreaSolid.numOfOpenPolygons), "Data size mismatch");

	int offset = 0;
	
	const int offsetNumOfClosedPolygons = offset;
	if (!AppendDataToMemoryblock(memoryBlock, offset, &statisticForAreaSolid.numOfClosedPolygons, sizeof(statisticForAreaSolid.numOfClosedPolygons)))
		return false;

	const int offsetNumOfOpenPolygons = offset;
	if (!AppendDataToMemoryblock(memoryBlock, offset, &statisticForAreaSolid.numOfOpenPolygons, sizeof(statisticForAreaSolid.numOfOpenPolygons)))
		return false;

	for (int i = 0; i < iPolygonSize; ++i)
	{
		FlexibleMesh mesh;
		PolygonDecomposer decomposer;
		decomposer.TriangulatePolygon(optimizedPolygons[i], mesh);
		if (!ExportPolygonForAreaSolid(memoryBlock, offset, mesh.vertexList, mesh.faceList))
			return false;

		actualNumOfClosedPolygons += int(mesh.faceList.size());
	}

	for (int i = 0; i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon(optimizedPolygons[i]);
		std::vector<PolygonPtr> innerPolygons = pPolygon->GetLoops()->GetHoleClones();
		for (int k = 0, iSize(innerPolygons.size()); k < iSize; ++k)
		{
			innerPolygons[k]->ReverseEdges();
			if (pModel->QueryEquivalentPolygon(innerPolygons[k]))
				continue;
			FlexibleMesh mesh;
			PolygonDecomposer decomposer;
			decomposer.TriangulatePolygon(innerPolygons[k], mesh);
			if (!ExportPolygonForAreaSolid(memoryBlock, offset, mesh.vertexList, mesh.faceList))
				return false;

			actualNumOfOpenPolygons += int(mesh.faceList.size());
		}
	}

	if (actualNumOfClosedPolygons != statisticForAreaSolid.numOfClosedPolygons)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "ExportAreaSolid (%s): the actual number of closed polygons is different from estimation", pAreaSolid->GetName().c_str());
		int tmpOffset = offsetNumOfClosedPolygons;
		if (!AppendDataToMemoryblock(memoryBlock, tmpOffset, &actualNumOfClosedPolygons, sizeof(actualNumOfClosedPolygons)))
			return false;
	}

	if (actualNumOfOpenPolygons != statisticForAreaSolid.numOfOpenPolygons)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "ExportAreaSolid (%s): the actual number of open polygons is different from estimation", pAreaSolid->GetName().c_str());
		int tmpOffset = offsetNumOfOpenPolygons;
		if (!AppendDataToMemoryblock(memoryBlock, tmpOffset, &actualNumOfOpenPolygons, sizeof(actualNumOfOpenPolygons)))
			return false;
	}

	const int actualTotalSize = offset;
	CRY_ASSERT(statisticForAreaSolid.totalSize == actualTotalSize);
	if (statisticForAreaSolid.totalSize != offset)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "ExportAreaSolid (%s): area solid file size is different from estimation", pAreaSolid->GetName().c_str());
	}

	string sRealGeomFileName;
	pAreaSolid->GenerateGameFilename(sRealGeomFileName);
	sRealGeomFileName.Replace("%level%", PathUtil::ToUnixPath(PathUtil::RemoveSlash(path.GetString())).c_str());

	pakFile.UpdateFile(sRealGeomFileName, memoryBlock.GetBuffer(), actualTotalSize, true, ICryArchive::LEVEL_FASTER);
	return true;
}

bool Exporter::ExportClipVolume(const string& path, ClipVolumeObject* pClipVolume, CPakFile& pakFile)
{
	if (!pClipVolume)
		return false;

	if (ModelCompiler* pCompiler = pClipVolume->GetCompiler())
	{
		_smart_ptr<IStatObj> pStatObj;
		if (pCompiler->GetIStatObj(&pStatObj))
		{
			bool bStatObjUpdated = UpdateStatObjWithoutBackFaces(pClipVolume);

			string sRealGeomFileName = pClipVolume->GenerateGameFilename();
			sRealGeomFileName.Replace("%level%", PathUtil::ToUnixPath(PathUtil::RemoveSlash(path.GetString())).c_str());

			IChunkFile* pChunkFile = NULL;
			if (pStatObj->SaveToCGF(sRealGeomFileName, &pChunkFile))
			{
				pClipVolume->ExportBspTree(pChunkFile);

				void* pMemFile = NULL;
				int nFileSize = 0;
				pChunkFile->WriteToMemoryBuffer(&pMemFile, &nFileSize);
				pakFile.UpdateFile(sRealGeomFileName, pMemFile, nFileSize, true, ICryArchive::LEVEL_FASTER);
				pChunkFile->Release();
			}

			if (bStatObjUpdated)
			{
				UpdateStatObj(pClipVolume);
			}
			return true;
		}
	}

	return false;
}

int ComputeFaceSize(const std::vector<SMeshFace>& faceList)
{
	int totalSize(0);
	for (int k = 0, iFaceSize(faceList.size()); k < iFaceSize; ++k)
	{
		totalSize += sizeof(int);
		totalSize += 3 * sizeof(Vec3);
	}
	return totalSize;
}

void Exporter::ComputeAreaSolidMemoryStatistic(AreaSolidObject* pAreaSolid, AreaSolidStatistic& outStatistic, std::vector<PolygonPtr>& optimizedPolygons)
{
	if (pAreaSolid == NULL)
		return;
	ModelCompiler* pCompiler(pAreaSolid->GetCompiler());
	if (pCompiler == NULL)
		return;
	Model* pModel(pAreaSolid->GetModel());
	if (pModel == NULL)
		return;
	int iPolygonSize = optimizedPolygons.size();

	memset(&outStatistic, 0, sizeof(outStatistic));

	outStatistic.totalSize += sizeof(outStatistic.numOfClosedPolygons);
	outStatistic.totalSize += sizeof(outStatistic.numOfOpenPolygons);

	for (int i = 0; i < iPolygonSize; ++i)
	{
		FlexibleMesh mesh;
		PolygonDecomposer decomposer;
		decomposer.TriangulatePolygon(optimizedPolygons[i], mesh);

		outStatistic.numOfClosedPolygons += mesh.faceList.size();
		outStatistic.totalSize += ComputeFaceSize(mesh.faceList);
	}

	for (int i = 0; i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon(optimizedPolygons[i]);
		std::vector<PolygonPtr> innerPolygons = pPolygon->GetLoops()->GetHoleClones();
		for (int k = 0, iSize(innerPolygons.size()); k < iSize; ++k)
		{
			innerPolygons[k]->ReverseEdges();
			if (pModel->QueryEquivalentPolygon(innerPolygons[k]))
				continue;

			FlexibleMesh mesh;
			PolygonDecomposer decomposer;
			decomposer.TriangulatePolygon(innerPolygons[k], mesh);

			outStatistic.numOfOpenPolygons += mesh.faceList.size();
			outStatistic.totalSize += ComputeFaceSize(mesh.faceList);
		}
	}
}
}

