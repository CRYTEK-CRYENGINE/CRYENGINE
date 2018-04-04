// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   SegmentNode.cpp
//  Version:     v1.00
//  Created:     18/04/2012 by Allen Chen
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SegmentNode.h"
#include "SegmentedWorld.h"
#include <CrySystem/ITimer.h>
#include <CryAISystem/INavigationSystem.h>

#define MAX_MS_TO_STREAM 8.0f

CSegmentNode::CSegmentNode(CSegmentedWorld* pParent, int wx, int wy, int lx, int ly)
	: m_pSW(pParent),
	m_wx(wx),
	m_wy(wy),
	m_lx(lx),
	m_ly(ly),
	m_nSID(-1),
	m_inited(false),
	m_status(ESEG_IDLE),
	m_streamingStatus(ESSS_Streaming_TerrainData)
{
}

CSegmentNode::~CSegmentNode()
{
	Delete();

	m_pStreamTerrain = 0;
	m_pStreamVisArea = 0;
	m_pFileDataBlock = 0;
}

bool CSegmentNode::Init()
{
	assert(m_pSW);

	if (m_inited)
		return true;

	string segLevelPath = string().Format("seg\\seg%d_%d\\", m_wx, m_wy);
	static const char* gameFolder = gEnv->pConsole->GetCVar("sys_game_folder")->GetString();
	m_segGamePath = string().Format("%s\\levels\\%s\\%s", gameFolder, m_pSW->s_levelName.c_str(), segLevelPath.c_str());

	m_segPackName = string().Format("%s\\seg.pak", m_segGamePath.c_str());
	if (!gEnv->pCryPak->OpenPack(m_segPackName))
		return false;

	float size = (float)m_pSW->GetSegmentSizeInMeters();
	Vec3 vSize(size, size, size);
#if USE_RELATIVE_COORD
	m_nSID = gEnv->p3DEngine->GetITerrain()->CreateSegment(vSize, m_pSW->GridToWorldPosition(m_lx, m_ly), segLevelPath);
#else
	m_nSID = gEnv->p3DEngine->GetITerrain()->CreateSegment(vSize, m_pSW->GridToWorldPosition(m_wx, m_wy), segLevelPath);
#endif
	gEnv->p3DEngine->GetIVisAreaManager()->CreateSegment(m_nSID);
	gEnv->p3DEngine->LoadTerrainSurfacesFromXML(m_pSW->s_levelSurfaceTypes, false, m_nSID);

	m_inited = true;

	return m_inited;
}

void CSegmentNode::Delete()
{
	//for(int i = 0; i < m_globalEntities.size(); ++i)
	//{
	//	IEntity *pEntity = m_globalEntities[i];
	//	assert(gEnv->pEntitySystem->GetEntity(pEntity->GetId()));
	//	pEntity->Invisible(true);
	//}
	//m_globalEntities.clear();

	for (int j = 0; j < m_localEntities.size(); ++j)
	{
		IEntity* pEntity = m_localEntities[j];
		assert(gEnv->pEntitySystem->GetEntity(pEntity->GetId()));
		if (pEntity)
		{
			pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
			pEntity->ResetKeepAliveCounter();
			gEnv->pEntitySystem->RemoveEntity(pEntity->GetId(), true);
		}
	}
	m_localEntities.clear();

	gEnv->p3DEngine->GetIVisAreaManager()->DeleteSegment(m_nSID, false);
	gEnv->p3DEngine->GetITerrain()->DeleteSegment(m_nSID, false);

	gEnv->pCryPak->ClosePack(m_segPackName);
}

bool CSegmentNode::LoadTerrain(void* pData, int nDataSize, float maxMs)
{
	if (StreamingFinished())
		return true;

	const Vec3& vSegmentOrigin = gEnv->p3DEngine->GetITerrain()->GetSegmentOrigin(m_nSID);

	if (pData == 0)
	{
		string compiledDataPath = m_segGamePath + COMPILED_HEIGHT_MAP_FILE_NAME;
		CCryFile fData;
		if (!fData.Open(compiledDataPath, "rb"))
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CSegmentedWorld::LoadTerrainSegment: failed to open file: %s", compiledDataPath.c_str());
			return false;
		}

		int size = (int)fData.GetLength();
		if (!size)
		{
			fData.Close();
			return false;
		}

		byte* compiledData = new byte[size];
		fData.ReadRaw(compiledData, size);

		// Always stream completely here
		while (gEnv->p3DEngine->GetITerrain()->StreamCompiledData(compiledData, size, m_nSID, vSegmentOrigin))
			;

		delete[] compiledData;
		fData.Close();

		return true;
	}
	else
	{
		bool stillStreaming = false;

		CTimeValue startTime, endTime;
		startTime = gEnv->pTimer->GetAsyncTime();
		do
		{
			stillStreaming = gEnv->p3DEngine->GetITerrain()->StreamCompiledData((byte*)pData, nDataSize, m_nSID, vSegmentOrigin);
			endTime = gEnv->pTimer->GetAsyncTime();
		}
		while (stillStreaming && (endTime - startTime).GetMilliSeconds() < maxMs);

		return !stillStreaming;
	}
}

bool CSegmentNode::LoadVisArea(void* pData, int nDataSize, float maxMs)
{
	if (StreamingFinished())
		return true;

	// should be moved to a single step
	std::vector<IStatObj*>* pStatObjTable = NULL;
	std::vector<IMaterial*>* pMatTable = NULL;
	std::vector<IStatInstGroup*>* pStatInstGroupTable = NULL;
	gEnv->p3DEngine->GetITerrain()->GetTables(pStatObjTable, pMatTable, pStatInstGroupTable, m_nSID);

	const Vec3& vSegmentOrigin = gEnv->p3DEngine->GetITerrain()->GetSegmentOrigin(m_nSID);

	Vec2 vIndexOffset(0, 0);
	string offsetFile = m_segGamePath + "offsetinfo.xml";
	XmlNodeRef offsetInfo = gEnv->pSystem->LoadXmlFromFile(offsetFile);
	if (offsetInfo)
		offsetInfo->getAttr("vOff", vIndexOffset);

	if (pData == 0)
	{
		string compiledDataPath = m_segGamePath + COMPILED_VISAREA_MAP_FILE_NAME;
		CCryFile fData;
		if (!fData.Open(compiledDataPath, "rb"))
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CSegmentedWorld::LoadVisAreaSegment: failed to open file: %s", compiledDataPath.c_str());
			return false;
		}

		int size = (int)fData.GetLength();
		if (!size)
		{
			fData.Close();
			return false;
		}

		byte* compiledData = new byte[size];
		fData.ReadRaw(compiledData, size);

		// Always stream completely here
		gEnv->p3DEngine->GetIVisAreaManager()->StreamCompiledData(compiledData, size, m_nSID, pStatObjTable, pMatTable, pStatInstGroupTable, vSegmentOrigin, vIndexOffset);

		delete[] compiledData;
		fData.Close();

		return true;
	}
	else
	{
		bool stillStreaming = false;

		CTimeValue startTime, endTime;
		startTime = gEnv->pTimer->GetAsyncTime();
		do
		{
			stillStreaming = gEnv->p3DEngine->GetIVisAreaManager()->StreamCompiledData((byte*)pData, nDataSize, m_nSID, pStatObjTable, pMatTable, pStatInstGroupTable, vSegmentOrigin, vIndexOffset);
			endTime = gEnv->pTimer->GetAsyncTime();
		}
		while (stillStreaming && (endTime - startTime).GetMilliSeconds() < maxMs);

		return !stillStreaming;
	}
}

bool CSegmentNode::LoadEntities()
{
	if (StreamingFinished())
		return true;

	string entitiesPath = m_segGamePath + "entities.xml";
	XmlNodeRef entitiesNode = gEnv->pSystem->LoadXmlFromFile(entitiesPath);

	if (!entitiesNode)
		return false;

	if (entitiesNode->getChildCount())
	{
		const Vec3& vSegmentOrigin = gEnv->p3DEngine->GetITerrain()->GetSegmentOrigin(m_nSID);
		gEnv->pEntitySystem->LoadEntities(entitiesNode, true, vSegmentOrigin, &m_globalEntities, &m_localEntities);
	}

	for (int i = 0; i < m_globalEntities.size(); i++)
	{
		IEntity* pEntity = m_globalEntities[i];
		assert(gEnv->pEntitySystem->GetEntity(pEntity->GetId()));
		pEntity->Invisible(false);

		// force update position for physics proxy
		if (pEntity->GetPhysics())
			pEntity->SetPos(pEntity->GetPos(), 0, true, true);
	}

	return true;
}

bool CSegmentNode::LoadNavigation()
{
	if (StreamingFinished())
		return true;

	if (gEnv->bServer || !gEnv->bMultiplayer)
	{
		string navigationDataFile = m_segGamePath + "mnmnav.bai";
		INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();
		if (pNavigationSystem)
			pNavigationSystem->ReadFromFile(navigationDataFile, false);
	}

	return true;
}

//
// Load areas(AIPaths, ...) for this segment.
bool CSegmentNode::LoadAreas()
{
	if (StreamingFinished())
		return true;

	if (gEnv->bServer || !gEnv->bMultiplayer)
	{
		string areasDataFilePath = m_segGamePath + "areas.bai";
		const Vec3& vSegmentOrigin = gEnv->p3DEngine->GetITerrain()->GetSegmentOrigin(m_nSID);
#ifdef SEG_WORLD
		gEnv->pAISystem->ReadAreasFromFile(areasDataFilePath, vSegmentOrigin);
#else
		gEnv->pAISystem->ReadAreasFromFile(areasDataFilePath);
#endif
	}

	return true;
}

bool CSegmentNode::ForceLoad(unsigned int flags)
{
	if (StreamingFinished())
		return true;

	if (!Init())
	{
		m_streamingStatus = ESSS_Warning;
		return false;
	}

	bool result = true;

	if (flags & ISegmentsManager::slfTerrain)
		result &= LoadTerrain(0, 0, FLT_MAX);
	if (flags & ISegmentsManager::slfVisArea)
		result &= LoadVisArea(0, 0, FLT_MAX);
	if (flags & ISegmentsManager::slfEntity)
		result &= LoadEntities();
	if (flags & ISegmentsManager::slfNavigation)
	{
		result &= LoadNavigation();
		result &= LoadAreas();
	}

	m_streamingStatus = result ? ESSS_Loaded : ESSS_Warning;

	return result;
}

void CSegmentNode::Stream()
{
	switch (m_streamingStatus)
	{
	case ESSS_Streaming_TerrainData:
		{
			if (m_pStreamTerrain == 0)
			{
				// start streaming terrain.dat
				string compiledDataPath = m_segGamePath + COMPILED_HEIGHT_MAP_FILE_NAME;
				m_pStreamTerrain = gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, compiledDataPath, this);
			}
			else if (m_pStreamTerrain->IsError())
			{
				// nothing to do here
				m_streamingStatus = ESSS_Warning;
			}
			else if (m_pStreamTerrain->IsFinished())
			{
				m_streamDatasLock.Lock();
				m_pFileDataBlock = m_streamDatas[m_pStreamTerrain];
				m_streamDatas.erase(m_pStreamTerrain);
				m_streamDatasLock.Unlock();

				m_streamingStatus = ESSS_Setting_TerrainData;
			}
			break;
		}

	case ESSS_Setting_TerrainData:
		{
			if (!StreamingFinished())
			{
				bool bTerrainLoaded = LoadTerrain(m_pFileDataBlock->pData, m_pFileDataBlock->nDataSize, MAX_MS_TO_STREAM);

				if (bTerrainLoaded)
					m_streamingStatus = ESSS_Streaming_VisAreaData;
			}
			else
			{
				m_streamingStatus = ESSS_Warning;
			}
			break;
		}

	case ESSS_Streaming_VisAreaData:
		{
			if (m_pStreamVisArea == 0)
			{
				// start streaming indoor.dat
				string compiledDataPath = m_segGamePath + COMPILED_VISAREA_MAP_FILE_NAME;
				m_pStreamVisArea = gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, compiledDataPath, this);
			}
			else if (m_pStreamVisArea->IsError())
			{
				// nothing to do here
				m_streamingStatus = ESSS_Warning;
			}
			else if (m_pStreamVisArea->IsFinished())
			{
				m_streamDatasLock.Lock();
				m_pFileDataBlock = m_streamDatas[m_pStreamVisArea];
				m_streamDatas.erase(m_pStreamVisArea);
				m_streamDatasLock.Unlock();

				m_streamingStatus = ESSS_Setting_VisAreaData;
			}
			break;
		}

	case ESSS_Setting_VisAreaData:
		{
			if (!StreamingFinished())
			{
				bool bVisAreaLoaded = LoadVisArea(m_pFileDataBlock->pData, m_pFileDataBlock->nDataSize, MAX_MS_TO_STREAM);

				if (bVisAreaLoaded)
				{
					// CS - update connections..
					gEnv->p3DEngine->GetIVisAreaManager()->UpdateConnections();
					m_streamingStatus = ESSS_Streaming_EntityData;
				}
			}
			else
			{
				m_streamingStatus = ESSS_Warning;
			}
			break;
		}

	case ESSS_Streaming_EntityData:
		{
			if (!StreamingFinished())
			{
				bool bEntitiesLoaded = LoadEntities();
				LoadNavigation();
				LoadAreas();

				if (bEntitiesLoaded)
					m_streamingStatus = ESSS_Setting_EntityData;
			}
			else
			{
				m_streamingStatus = ESSS_Warning;
			}
			break;
		}

	case ESSS_Setting_EntityData:
		{
			m_streamingStatus = ESSS_Loaded;

			break;
		}
	}
}

void CSegmentNode::Update()
{
	switch (m_status)
	{
	case ESEG_INIT:
		Init();
		m_status = ESEG_STREAM;
		break;

	case ESEG_STREAM:
		if (StreamingFinished())
			m_status = ESEG_IDLE;
		else
			Stream();
		break;

	case ESEG_IDLE:
	default:
		break;
	}
}

void CSegmentNode::SetLocalCoords(int lx, int ly)
{
#if USE_RELATIVE_COORD
	assert(m_pSW);

	if (OffsetApplied(lx, ly))
		return;

	gEnv->p3DEngine->GetITerrain()->SetSegmentOrigin(m_nSID, m_pSW->GridToWorldPosition(lx, ly), true);

	m_lx = lx;
	m_ly = ly;
#endif
}
