// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   visareamancompile.cpp
//  Version:     v1.00
//  Created:     15/04/2005 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: check vis
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ObjMan.h"
#include "VisAreas.h"

bool CVisAreaManager::GetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, std::vector<struct IStatInstGroup*>** ppStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo)
{
#if !ENGINE_ENABLE_COMPILATION
	CryFatalError("serialization code removed, please enable 3DENGINE_ENABLE_COMPILATION in Cry3DEngine/StdAfx.h");
	return false;
#else
	float fStartTime = GetCurAsyncTimeSec();

	bool bHMap(!pExportInfo || pExportInfo->nHeigtmap);
	bool bObjs(!pExportInfo || pExportInfo->nObjTypeMask);

	//  PrintMessage("Exporting indoor data (%s, %.2f MB) ...",
	//  (bHMap && bObjs) ? "Objects and heightmap" : (bHMap ? "Heightmap" : (bObjs ? "Objects" : "Nothing")), ((float)nDataSize)/1024.f/1024.f);

	// write header
	SVisAreaManChunkHeader* pVisAreaManagerChunkHeader = (SVisAreaManChunkHeader*)pData;
	pVisAreaManagerChunkHeader->nVersion = VISAREAMANAGER_CHUNK_VERSION;
	pVisAreaManagerChunkHeader->nDummy = 0;
	pVisAreaManagerChunkHeader->nFlags = (eEndian == eBigEndian) ? SERIALIZATION_FLAG_BIG_ENDIAN : 0;
	pVisAreaManagerChunkHeader->nFlags2 = 0;
	pVisAreaManagerChunkHeader->nChunkSize = nDataSize;

	SwapEndian(*pVisAreaManagerChunkHeader, eEndian);

	UPDATE_PTR_AND_SIZE(pData, nDataSize, sizeof(SVisAreaManChunkHeader));

	pVisAreaManagerChunkHeader->nVisAreasNum = m_lstVisAreas.Count();
	pVisAreaManagerChunkHeader->nPortalsNum = m_lstPortals.Count();
	pVisAreaManagerChunkHeader->nOcclAreasNum = m_lstOcclAreas.Count();

	for (int i = 0; i < m_lstVisAreas.Count(); i++)
		m_lstVisAreas[i]->GetData(pData, nDataSize, *ppStatObjTable, *ppMatTable, *ppStatInstGroupTable, eEndian, pExportInfo);

	for (int i = 0; i < m_lstPortals.Count(); i++)
		m_lstPortals[i]->GetData(pData, nDataSize, *ppStatObjTable, *ppMatTable, *ppStatInstGroupTable, eEndian, pExportInfo);

	for (int i = 0; i < m_lstOcclAreas.Count(); i++)
		m_lstOcclAreas[i]->GetData(pData, nDataSize, *ppStatObjTable, *ppMatTable, *ppStatInstGroupTable, eEndian, pExportInfo);

	SAFE_DELETE(*ppStatObjTable);
	SAFE_DELETE(*ppMatTable);
	SAFE_DELETE(*ppStatInstGroupTable);

	if (!pExportInfo)
		PrintMessagePlus(" done in %.2f sec", GetCurAsyncTimeSec() - fStartTime);

	assert(nDataSize == 0);
	return nDataSize == 0;
#endif
}

int CVisAreaManager::GetCompiledDataSize(SHotUpdateInfo* pExportInfo)
{
#if !ENGINE_ENABLE_COMPILATION
	CryFatalError("serialization code removed, please enable 3DENGINE_ENABLE_COMPILATION in Cry3DEngine/StdAfx.h");
	return 0;
#else

	int nDataSize = 0;
	byte* pData = NULL;

	// get header size
	nDataSize += sizeof(SVisAreaManChunkHeader);

	for (int i = 0; i < m_lstVisAreas.Count(); i++)
		m_lstVisAreas[i]->GetData(pData, nDataSize, NULL, NULL, NULL, eLittleEndian, pExportInfo);

	for (int i = 0; i < m_lstPortals.Count(); i++)
		m_lstPortals[i]->GetData(pData, nDataSize, NULL, NULL, NULL, eLittleEndian, pExportInfo);

	for (int i = 0; i < m_lstOcclAreas.Count(); i++)
		m_lstOcclAreas[i]->GetData(pData, nDataSize, NULL, NULL, NULL, eLittleEndian, pExportInfo);

	return nDataSize;
#endif
}

bool CVisAreaManager::Load(FILE*& f, int& nDataSize, struct SVisAreaManChunkHeader* pVisAreaManagerChunkHeader, std::vector<struct IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable)
{
	bool bRes;

	// in case of small data amount (console game) load entire file into memory in single operation
	if (nDataSize < 4 * 1024 * 1024)
	{
		_smart_ptr<IMemoryBlock> pMemBlock = gEnv->pCryPak->PoolAllocMemoryBlock(nDataSize + 8, "LoadIndoors");
		byte* pPtr = (byte*)pMemBlock->GetData();
		while (UINT_PTR(pPtr) & 3)
			pPtr++;

		if (GetPak()->FReadRaw(pPtr, 1, nDataSize - sizeof(SVisAreaManChunkHeader), f) != nDataSize - sizeof(SVisAreaManChunkHeader))
			return false;

		bRes = Load_T(pPtr, nDataSize, pVisAreaManagerChunkHeader, pStatObjTable, pMatTable, false, NULL);
	}
	else
	{
		bRes = Load_T(f, nDataSize, pVisAreaManagerChunkHeader, pStatObjTable, pMatTable, false, NULL);
	}

	return bRes;

}

bool CVisAreaManager::SetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo)
{
	SVisAreaManChunkHeader* pChunkHeader = (SVisAreaManChunkHeader*)pData;
	pData += sizeof(SVisAreaManChunkHeader);

	SwapEndian(*pChunkHeader, eLittleEndian);

	bool bRes = Load_T(pData, nDataSize, pChunkHeader, *ppStatObjTable, *ppMatTable, bHotUpdate, pExportInfo);

	SAFE_DELETE(*ppStatObjTable);
	SAFE_DELETE(*ppMatTable);

	return bRes;
}

namespace
{
	inline void HelperUnregisterEngineObjectsInArea(PodArray<CVisArea*>& arrayVisArea, const SHotUpdateInfo* pExportInfo, PodArray<IRenderNode*>& arrUnregisteredObjects, bool bOnlyEngineObjects)
	{
		for (int i = 0; i < arrayVisArea.Count(); i++)
		{
			if (arrayVisArea[i]->IsObjectsTreeValid())
			{
				arrayVisArea[i]->GetObjectsTree()->UnregisterEngineObjectsInArea(pExportInfo, arrUnregisteredObjects, bOnlyEngineObjects);
			}
		}
	}
}

void CVisAreaManager::UnregisterEngineObjectsInArea(const SHotUpdateInfo* pExportInfo, PodArray<IRenderNode*>& arrUnregisteredObjects, bool bOnlyEngineObjects)
{
	HelperUnregisterEngineObjectsInArea(m_lstVisAreas,  pExportInfo, arrUnregisteredObjects, bOnlyEngineObjects);
	HelperUnregisterEngineObjectsInArea(m_lstPortals,   pExportInfo, arrUnregisteredObjects, bOnlyEngineObjects);
	HelperUnregisterEngineObjectsInArea(m_lstOcclAreas, pExportInfo, arrUnregisteredObjects, bOnlyEngineObjects);
}

void CVisAreaManager::OnVisAreaDeleted(IVisArea* pArea)
{
	for (int i = 0, num = m_lstCallbacks.size(); i < num; i++)
		m_lstCallbacks[i]->OnVisAreaDeleted(pArea);

	m_lstActiveOcclVolumes.Delete((CVisArea*)pArea);
	m_lstIndoorActiveOcclVolumes.Delete((CVisArea*)pArea);
	m_lstActiveEntransePortals.Delete((CVisArea*)pArea);
}

template<class T>
bool CVisAreaManager::Load_T(T*& f, int& nDataSize, SVisAreaManChunkHeader* pVisAreaManagerChunkHeader, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo)
{
	if (pVisAreaManagerChunkHeader->nVersion != VISAREAMANAGER_CHUNK_VERSION)
	{ Error("CVisAreaManager::SetCompiledData: version of file is %d, expected version is %d", pVisAreaManagerChunkHeader->nVersion, (int)VISAREAMANAGER_CHUNK_VERSION); return 0; }

	if (pVisAreaManagerChunkHeader->nChunkSize != nDataSize)
	{ Error("CVisAreaManager::SetCompiledData: data size mismatch (%d != %d)", pVisAreaManagerChunkHeader->nChunkSize, nDataSize); return 0; }

	bool bHMap(!pExportInfo || pExportInfo->nHeigtmap);
	bool bObjs(!pExportInfo || pExportInfo->nObjTypeMask);
	AABB* pBox = (pExportInfo && !pExportInfo->areaBox.IsReset()) ? &pExportInfo->areaBox : NULL;

	EEndian eEndian = (pVisAreaManagerChunkHeader->nFlags & SERIALIZATION_FLAG_BIG_ENDIAN) ? eBigEndian : eLittleEndian;

	PodArray<IRenderNode*> arrUnregisteredObjects;
	UnregisterEngineObjectsInArea(pExportInfo, arrUnregisteredObjects, true);

	PodArray<IRenderNode*> arrUnregisteredEntities;
	UnregisterEngineObjectsInArea(NULL, arrUnregisteredEntities, false);

	DeleteAllVisAreas();

	SAFE_DELETE(m_pAABBTree);
	m_pCurArea = m_pCurPortal = 0;

	{
		// construct areas
		m_lstVisAreas.PreAllocate(pVisAreaManagerChunkHeader->nVisAreasNum, pVisAreaManagerChunkHeader->nVisAreasNum);
		m_lstPortals.PreAllocate(pVisAreaManagerChunkHeader->nPortalsNum, pVisAreaManagerChunkHeader->nPortalsNum);
		m_lstOcclAreas.PreAllocate(pVisAreaManagerChunkHeader->nOcclAreasNum, pVisAreaManagerChunkHeader->nOcclAreasNum);

		nDataSize -= sizeof(SVisAreaManChunkHeader);

		//    if(bHotUpdate)
		//    PrintMessage("Importing indoor data (%s, %.2f MB) ...",
		//  (bHMap && bObjs) ? "Objects and heightmap" : (bHMap ? "Heightmap" : (bObjs ? "Objects" : "Nothing")), ((float)nDataSize)/1024.f/1024.f);
		m_visAreas.PreAllocate(pVisAreaManagerChunkHeader->nVisAreasNum);
		m_visAreaColdData.PreAllocate(pVisAreaManagerChunkHeader->nVisAreasNum);

		m_portals.PreAllocate(pVisAreaManagerChunkHeader->nPortalsNum);
		m_portalColdData.PreAllocate(pVisAreaManagerChunkHeader->nPortalsNum);

		m_occlAreas.PreAllocate(pVisAreaManagerChunkHeader->nOcclAreasNum);
		m_occlAreaColdData.PreAllocate(pVisAreaManagerChunkHeader->nOcclAreasNum);

		for (int i = 0; i < m_lstVisAreas.Count(); i++)
			m_lstVisAreas[i] = CreateTypeVisArea();
		for (int i = 0; i < m_lstPortals.Count(); i++)
			m_lstPortals[i] = CreateTypePortal();
		for (int i = 0; i < m_lstOcclAreas.Count(); i++)
			m_lstOcclAreas[i] = CreateTypeOcclArea();
	}

	{
		// load areas content
		for (int i = 0; i < m_lstVisAreas.Count(); i++)
			m_lstVisAreas[i]->Load(f, nDataSize, pStatObjTable, pMatTable, eEndian, pExportInfo);

		for (int i = 0; i < m_lstPortals.Count(); i++)
			m_lstPortals[i]->Load(f, nDataSize, pStatObjTable, pMatTable, eEndian, pExportInfo);

		for (int i = 0; i < m_lstOcclAreas.Count(); i++)
			m_lstOcclAreas[i]->Load(f, nDataSize, pStatObjTable, pMatTable, eEndian, pExportInfo);
	}

	for (int i = 0; i < arrUnregisteredObjects.Count(); i++)
		arrUnregisteredObjects[i]->ReleaseNode();
	arrUnregisteredObjects.Reset();

	for (int i = 0; i < arrUnregisteredEntities.Count(); i++)
		Get3DEngine()->RegisterEntity(arrUnregisteredEntities[i]);
	arrUnregisteredEntities.Reset();

	SAFE_DELETE(m_pAABBTree);
	m_pCurArea = m_pCurPortal = 0;
	UpdateConnections();

	return nDataSize == 0;
}

//////////////////////////////////////////////////////////////////////
// Segmented World
inline bool IsContainBox2D(const AABB& base, const AABB& test)
{
	if (base.min.x < test.max.x && base.max.x > test.min.x &&
	    base.min.y < test.max.y && base.max.y > test.min.y)
		return true;

	return false;
}

template<class T>
void CVisAreaManager::ResetVisAreaList(PodArray<CVisArea*>& lstVisAreas, PodArray<CVisArea*, ReservedVisAreaBytes>& visAreas, PodArray<T>& visAreaColdData)
{
	for (int i = 0; i < visAreas.Count(); i++)
	{
		CVisArea* pVisArea = visAreas[i];
		if (pVisArea->IsObjectsTreeValid())
			pVisArea->GetObjectsTree()->SetVisArea(pVisArea);
		pVisArea->SetColdDataPtr(&visAreaColdData[i]);
		lstVisAreas[i] = pVisArea;
	}
}
