// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   SegmentNode.h
//  Version:     v1.00
//  Created:     18/04/2012 by Allen Chen
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Segment_Node_h__
#define __Segment_Node_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CrySystem/IStreamEngine.h>
#define USE_RELATIVE_COORD 1

struct SMemBlock : public CMultiThreadRefCount
{
	SMemBlock() { pData = 0; nDataSize = 0; }
	explicit SMemBlock(int size) { pData = new uint8[size]; nDataSize = size; }
	~SMemBlock() { SAFE_DELETE_ARRAY(pData); }

	uint8* pData;
	int    nDataSize;
};

enum ESegmentStatus
{
	ESEG_IDLE,
	ESEG_INIT,
	ESEG_STREAM,
};

enum ESegmentStreamingStatus
{
	ESSS_Streaming_TerrainData,
	ESSS_Setting_TerrainData,
	ESSS_Streaming_VisAreaData,
	ESSS_Setting_VisAreaData,
	ESSS_Streaming_EntityData,
	ESSS_Setting_EntityData,
	ESSS_Loaded,
	ESSS_Warning,
};

class CSegmentedWorld;

class CSegmentNode : public IStreamCallback
{
	friend class CSegmentedWorld;
public:
	CSegmentNode(CSegmentedWorld* pParent, int wx, int wy, int lx, int ly);
	~CSegmentNode();

	bool Init();
	void Stream();
	void Delete();
	void Update();

	bool LoadTerrain(void* pData, int nDataSize, float maxMs);
	bool LoadVisArea(void* pData, int nDataSize, float maxMs);
	bool LoadEntities();
	bool LoadNavigation();
	bool LoadAreas();
	bool ForceLoad(unsigned int flags);

	bool OffsetApplied(int lx, int ly) { return (m_lx == lx && m_ly == ly); }
	void SetLocalCoords(int lx, int ly);
	void SetStatus(ESegmentStatus val) { m_status = val; }
	bool StreamingSucceeded()          { return m_streamingStatus == ESSS_Loaded; }
	bool StreamingFinished()           { return m_streamingStatus >= ESSS_Loaded; }

	// IStreamCallback
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
	{
		if (nError == 0)
		{
			_smart_ptr<SMemBlock> pBlock = new SMemBlock(pStream->GetBytesRead());
			memcpy(pBlock->pData, pStream->GetBuffer(), pStream->GetBytesRead());

			AUTO_LOCK(m_streamDatasLock);
			m_streamDatas[pStream] = pBlock;
		}
	}
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError) {}
	// ~IStreamCallback

	std::vector<IEntity*> m_globalEntities;
	std::vector<IEntity*> m_localEntities;

protected:
	CSegmentedWorld*                              m_pSW;
	int                                           m_nSID;
	int                                           m_wx, m_wy;
	int                                           m_lx, m_ly;
	bool                                          m_inited;

	string                                        m_segGamePath;
	string                                        m_segPackName;
	std::vector<string>                           m_log;

	ESegmentStatus                                m_status;
	ESegmentStreamingStatus                       m_streamingStatus;

	std::map<IReadStream*, _smart_ptr<SMemBlock>> m_streamDatas;
	CryCriticalSection                            m_streamDatasLock;

	IReadStreamPtr                                m_pStreamTerrain;
	IReadStreamPtr                                m_pStreamVisArea;
	_smart_ptr<SMemBlock>                         m_pFileDataBlock;
};

#endif // __Segment_Node_h__
