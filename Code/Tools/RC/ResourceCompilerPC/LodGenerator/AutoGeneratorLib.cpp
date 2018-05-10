// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2014.
*************************************************************************/

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include "AutoGeneratorLib.h"
#include <CryThreading/IThreadManager.h>
//#include "AtlasGenerator.h"
//#include "TopologyGraph.h"
//#include "TopologyTransform.h"
#include "AutoGeneratorUVUnwrap.h"
#include <CryThreading/CryThreadImpl.h>
#include <Cry3DEngine/CGF/CGFContent.h>

// Provides definition for PushProfilingMarker() in debug builds.
#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.h>

#include "StealingThreadPool.h"

// For documentation search for "LOD Generator" on Confluence

//#define VISUAL_CHANGE_DEBUG			// Timings, debug printfs and double checking of error metric
//#define LODGEN_VERIFY						// Full verify of all data structures. Slow but possibly useful for debugging
#define MAX_MOVELIMIT	4096				// Maximum moves one polygon can be involved in. (Summation of all moves each of it's vertices can do)
#define INCLUDE_LOD_GENERATOR

#define LODGEN_ASSERT(condition) { if (!(condition)) __debugbreak(); }


#include "VisualChangeCalculatorView.h"
#include "LODChainGenerateThread.h"
#include "VisualChangeCalculatorViewThread.h"
#include "VisualChangeCalculatorViewJob.h"
#include "VisualChangeCalculator.h"
#include "LODChainGenerate.h"

//////////////////////////////////////////////////////////////////////////
threadID CryGetCurrentThreadId()
{
	return GetCurrentThreadId();
}

// just copy from platform_impl.h need to fix ??
//////////////////////////////////////////////////////////////////////////
void CrySleep(unsigned int dwMilliseconds)
{
	Sleep(dwMilliseconds);
}
//////////////////////////////////////////////////////////////////////////
void CryLowLatencySleep(unsigned int dwMilliseconds)
{
#if CRY_PLATFORM_DURANGO
	if (dwMilliseconds > 32) // just do an OS sleep for long periods, because we just assume that if we sleep for this long, we don't need a accurate latency (max diff is likly 15ms)
		CrySleep(dwMilliseconds);
	else // do a more accurate "sleep" by yielding this CPU to other threads
	{
		LARGE_INTEGER frequency;
		LARGE_INTEGER currentTime;
		LARGE_INTEGER endTime;

		QueryPerformanceCounter(&endTime);

		// Ticks in microseconds (1/1000 ms)
		QueryPerformanceFrequency(&frequency);
		endTime.QuadPart += (dwMilliseconds * frequency.QuadPart) / (1000ULL);

		do
		{
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();
			SwitchToThread();

			QueryPerformanceCounter(&currentTime);
		} while (currentTime.QuadPart < endTime.QuadPart);
	}
#else
	CrySleep(dwMilliseconds);
#endif
}


namespace LODGenerator 
{
	///////////////////////////////////////////////////////////
	// External Interface
	///////////////////////////////////////////////////////////

	bool CLODGeneratorLib::GenerateLODSequence(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues, bool bProcessAsync)
	{
#ifdef INCLUDE_LOD_GENERATOR
		pReturnValues->m_pSubObjectOutput.clear();
		if (pInputParams->nodeList.size() != 0)
		{
			pReturnValues->m_pThread=new LODChainGenerate();
			return pReturnValues->m_pThread->ProcessLodder(pInputParams, pReturnValues);
		}
#endif
		return false;
	}

// 	bool CLODGeneratorLib::GetAsyncLODSequenceResults(CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues, float *pProgress)
// 	{
// #ifdef INCLUDE_LOD_GENERATOR
// 		if (pReturnValues->m_pThread)
// 		{
// 			if (!pReturnValues->m_pThread->CheckStatus(pProgress))
// 				return false;
// 			pReturnValues->m_pThread->WaitForCompletion();
// 			SAFE_DELETE(pReturnValues->m_pThread);
// 		}
// #endif
// 		return true;
// 	}

	bool CLODGeneratorLib::FreeLODSequence(CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues)
	{
#ifdef INCLUDE_LOD_GENERATOR
//		while (!GetAsyncLODSequenceResults(pReturnValues, NULL));
		for (int i=0;i<pReturnValues->m_pSubObjectOutput.size();i++)
		{
			CLODGeneratorLib::SLODSequenceGenerationOutput* pReturnValue = pReturnValues->m_pSubObjectOutput[i];
			delete[] pReturnValue->indices;
			delete[] pReturnValue->positions;
			delete[] pReturnValue->moveList;
			pReturnValue->indices=NULL;
			pReturnValue->positions=NULL;
			pReturnValue->moveList=NULL;
			pReturnValue->numIndices=0;
			pReturnValue->numMoves=0;
			pReturnValue->numPositions=0;
			pReturnValue->node = NULL;		
		}
		pReturnValues->m_pSubObjectOutput.clear();
#endif
		return true;
	}

	bool CLODGeneratorLib::BuildLodMesh(SLODSequenceGenerationOutput *pSequence, const CLODGeneratorLib::SLODGenerationInputParams *pInputParams,std::vector<CMesh*>& pMeshList)
	{
		if ( pSequence->node != NULL )
		{
			CMesh* pOriginMesh = pSequence->node->pMesh;
			AABB aabb = pOriginMesh->m_bbox;

			int numMoves=(int)floorf(pSequence->numMoves*(1.0f-pInputParams->nPercentage/100.0f)+0.5f);
			int newFaces=0;
			std::vector<vtx_idx> pNewIndices;
			std::vector<int> remap;
			remap.resize(pSequence->numPositions);
			for (int i=0; i<remap.size(); i++)
				remap[i]=i;
			for (int i=0; i<numMoves; i++)
				remap[pSequence->moveList[i].from]=pSequence->moveList[i].to;
			for (int i=0; i<remap.size(); i++)
			{
				int cur=i;
				while (remap[cur]!=cur)
					cur=remap[cur];
				remap[i]=cur;
			}
			for (vtx_idx *it=pSequence->indices, *end=&pSequence->indices[pSequence->numIndices]; it!=end; it+=3)
			{
				int id[3];
				id[0]=remap[*(it+0)];
				id[1]=remap[*(it+1)];
				id[2]=remap[*(it+2)];
				if (id[0]!=id[1] && id[0]!=id[2] && id[1]!=id[2])
				{
					pNewIndices.push_back((vtx_idx)id[0]);
					pNewIndices.push_back((vtx_idx)id[1]);
					pNewIndices.push_back((vtx_idx)id[2]);
					newFaces++;
				}
			}

			if (pInputParams->bUnwrap)
			{
				AutoUV uv;
				uv.Run(pSequence->positions, &pNewIndices[0], newFaces,eUV_Common);
				pMeshList.insert(pMeshList.begin(),uv.m_outMeshList.begin(),uv.m_outMeshList.end());

			}
			else
			{
				CMesh *pMesh=new CMesh();
				std::vector<int> posRemap;
				std::vector<int> outVerts;
				posRemap.resize(pSequence->numPositions, -1);
				pMesh->SetIndexCount(pNewIndices.size());
				vtx_idx *pIndices=pMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
				for (std::vector<vtx_idx>::iterator it=pNewIndices.begin(), end=pNewIndices.end(); it!=end; ++it)
				{
					if (posRemap[*it]==-1)
					{
						posRemap[*it]=outVerts.size();
						outVerts.push_back(*it);
					}
					*(pIndices++)=posRemap[*it];
				}
				pMesh->SetVertexCount(outVerts.size());
				Vec3* pVertices=pMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS);
				for (std::vector<int>::iterator it=outVerts.begin(), end=outVerts.end(); it!=end; ++it)
				{
					*(pVertices++)=pSequence->positions[*it];
				}
				SMeshSubset subset;
				subset.nNumIndices=pNewIndices.size();
				subset.nNumVerts=outVerts.size();
				subset.fRadius=100.0f;
				subset.fTexelDensity=1.0f;
				pMesh->m_subsets.push_back(subset);

				pMeshList.push_back(pMesh);
			}

			for (int i=0;i<pMeshList.size();i++)
			{
				pMeshList[i]->m_bbox = aabb;
			}
		}

		return true;
	}

	bool CLODGeneratorLib::GenerateLOD(const CLODGeneratorLib::SLODGenerationInputParams *pInputParams, CLODGeneratorLib::SLODGenerationOutput *pReturnValues)
	{
#ifdef INCLUDE_LOD_GENERATOR
		SLODSequenceGenerationOutput *pSequence=pInputParams->pSequence;
		if (pSequence && pSequence->node && BuildLodMesh(pSequence,pInputParams,pReturnValues->pMeshList) )
			return true;
#endif
		return false;
	}

	bool CLODGeneratorLib::GenerateUVs(Vec3* pPositions, vtx_idx *pIndices, int nNumFaces,std::vector<CMesh*>& pMeshList)
	{			
		AutoUV uv;
		uv.Run(pPositions, pIndices, nNumFaces,eUV_ABF);
		pMeshList.insert(pMeshList.begin(),uv.m_outMeshList.begin(),uv.m_outMeshList.end());
		return true;
	}

	void CLODGeneratorLib::CancelLODGeneration(CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues)
	{
		if (pReturnValues)
		{
			if (pReturnValues->m_pThread)
			{
//				pReturnValues->m_pThread->Cancel();
//				gEnv->pThreadManager->JoinThread(pReturnValues->m_pThread, eJM_Join);

				delete pReturnValues->m_pThread;
				pReturnValues->m_pThread = NULL;
			}

			FreeLODSequence(pReturnValues);
		}
	}

}
