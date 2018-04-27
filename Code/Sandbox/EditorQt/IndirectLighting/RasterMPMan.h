// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __raster_mpman_h__
#define __raster_mpman_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Raster.h"
#include <CryThreading/IThreadManager.h>

//#define DO_RASTER_MP		//enables multiprocessing

//controls multi processing of triangle rasterization
class CRasterMPMan
#if defined(DO_RASTER_MP)
	: public IThread
#endif
{
public:
	CRasterMPMan();

	//project once, each subsequent call will invalidate results from before
	void PushProjectTrianglesOS
	(
	  CBBoxRaster* pRasterBBox,
	  const Vec3& crWorldMin,
	  const Vec3& crWorldMax,
	  const Matrix34& crOSToWorldRotMatrix,
	  const std::vector<SMatChunk>& crChunks,
	  bool useConservativeFill,
	  const ITriangleValidator* cpTriangleValidator
	);

	//starts next processing set
	void StartProjectTrianglesOS();

	static const uint32 scMaxMatChunks = 32;

	inline void SynchronizeProjectTrianglesOS()//wait till a subsequent call of ProjectTrianglesOS has been finished
	{
#if !defined(DO_RASTER_MP)
		scActiveSet = (scActiveSet + 1) & 0x1;//switch set
		scMPDataCount[scActiveSet] = 0;//reset it
#else
		if (m_CoreCount <= 1)
		{
			scActiveSet = (scActiveSet + 1) & 0x1;//switch set
			scMPDataCount[scActiveSet] = 0;//reset it
			return;
		}
		if (m_ThreadActive)
			WaitForSingleObject(scEventID, INFINITE);
		scActiveSet = (scActiveSet + 1) & 0x1;//switch set
		m_ThreadActive = false;
#endif
	}

	inline void InitMP()
	{
#if defined(DO_RASTER_MP)
		//init multiprocessing
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		m_CoreCount = std::max(systemInfo.dwNumberOfProcessors, (DWORD)1);
		if (m_CoreCount > 1)
		{
			scEventID = CreateEvent(NULL, false, false, NULL);
			scWorkerEventID = CreateEvent(NULL, false, false, NULL);

			if (!gEnv->pThreadManager->SpawnThread(this, "SHLighting_TriRaster"))
			{
				CryFatalError("Error spawning \"SHLighting_TriRaster\" thread.");
			}
		}
#endif
	}

	inline void CleanupMP()
	{
#if defined(DO_RASTER_MP)
		if (m_CoreCount > 1)
		{
			scMPDataCount[scActiveSet] = 0xFFFFFFFF;//signal end
			scActiveSet = (scActiveSet + 1) & 0x1;//switch set
			m_ThreadActive = true;
			SetEvent(scWorkerEventID);
			WaitForSingleObject(scEventID, INFINITE);
			CloseHandle(scEventID);
			CloseHandle(scWorkerEventID);
		}
#endif
	}

private:
	//multiprocessing related things
	//it absolutely goes hand in hand with main app, data lifetime controlled there
	//structure holding all required data
	struct SMPData
	{
		CBBoxRaster*                  pBBoxRaster;
		Vec3                          cWorldMin;
		Vec3                          cWorldMax;
		const Matrix34*               cpOSToWorldRotMatrix;
		const std::vector<SMatChunk>* cpChunks;
		bool                          useConservativeFill;
		const ITriangleValidator*     cpTriangleValidator;
		SMPData() : pBBoxRaster(NULL), cWorldMin(Vec3(0.f, 0.f, 0.f)), cWorldMax(Vec3(0.f, 0.f, 0.f)), cpOSToWorldRotMatrix(NULL),
			cpChunks(NULL), useConservativeFill(true), cpTriangleValidator(NULL)
		{}
	};
#if defined(DO_RASTER_MP)
	//core routine for MP
	virtual void ThreadEntry();
#endif

	static SMPData scMPData[scMaxMatChunks][2]; //double buffered multiprocessing data
	static uint32  scMPDataCount[2];
	static uint32  scActiveSet;         //current active index to push into
	bool           m_ThreadActive;      //true if a thread job is to synchronized, very lazy, only changed and accessed by main thread
	HANDLE         m_ThreadID;          //thread handle
	static HANDLE  scEventID;           //event handle (to signal completion to main thread)
	static HANDLE  scWorkerEventID;     //event handle (to signal processing demand to worker thread)
	unsigned int   m_CoreCount;         //number of system cores, 1 will not toggle mp
};

inline void CRasterMPMan::PushProjectTrianglesOS
(
  CBBoxRaster* pRasterBBox,
  const Vec3& crWorldMin,
  const Vec3& crWorldMax,
  const Matrix34& crOSToWorldRotMatrix,
  const std::vector<SMatChunk>& crChunks,
  bool useConservativeFill,
  const ITriangleValidator* cpTriangleValidator
)
{
	assert(scMPDataCount[scActiveSet] < scMaxMatChunks);
	SMPData& rCurData = scMPData[scMPDataCount[scActiveSet]++][scActiveSet];
	rCurData.pBBoxRaster = pRasterBBox;
	rCurData.cWorldMin = crWorldMin;
	rCurData.cWorldMax = crWorldMax;
	rCurData.cpOSToWorldRotMatrix = &crOSToWorldRotMatrix;
	rCurData.cpChunks = &crChunks;
	rCurData.useConservativeFill = useConservativeFill;
	rCurData.cpTriangleValidator = cpTriangleValidator;
}

inline CRasterMPMan::CRasterMPMan() : m_ThreadActive(false), m_CoreCount(1)
{
	CRasterMPMan::scActiveSet = 0;
	CRasterMPMan::scMPDataCount[0] = 0;
	CRasterMPMan::scMPDataCount[1] = 0;
}

#endif // __raster_mpman_h__

