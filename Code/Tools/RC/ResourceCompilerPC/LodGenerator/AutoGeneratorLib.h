// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2013.
*************************************************************************/

#pragma once

#include <CryCore/smartptr.h>

class CMesh;
struct CNodeCGF;
namespace LODGenerator 
{
	class LODChainGenerate;
	class CLODGeneratorLib
	{
	public:
		struct SLODSequenceGenerationInputParams
		{
			std::vector<CNodeCGF*> nodeList;
			float metersPerPixel;
			float silhouetteWeight;
			float vertexWeldingDistance;
			int numViewElevations;
			int numViewsAround;
			bool bObjectHasBase;
			bool bCheckTopology;
		};

		struct SLODSequenceGenerationOutput
		{
			struct SMove
			{
				int from;
				int to;
				float error;
			};
			Vec3* positions;
			vtx_idx* indices;
			SMove* moveList;
			int numPositions;
			int numIndices;
			int numMoves;
			CNodeCGF* node;
		};

		struct SLODSequenceGenerationOutputList
		{
			LODChainGenerate *m_pThread;
			std::vector<SLODSequenceGenerationOutput*> m_pSubObjectOutput;
		};

		struct SLODGenerationInputParams
		{
			SLODSequenceGenerationOutput *pSequence;
			float nPercentage;
			bool bUnwrap;
		};

		struct SLODGenerationOutput
		{
			std::vector<CMesh*> pMeshList;
		};

		static bool GenerateLODSequence(const SLODSequenceGenerationInputParams *pInputParams, SLODSequenceGenerationOutputList *pReturnValues, bool bProcessAsync);
//		static bool GetAsyncLODSequenceResults(SLODSequenceGenerationOutputList *pReturnValues, float *pProgress);
		static bool FreeLODSequence(SLODSequenceGenerationOutputList *pReturnValues);
		static bool BuildLodMesh(SLODSequenceGenerationOutput *pSequence, const SLODGenerationInputParams *pInputParams,std::vector<CMesh*>& pMeshList);
		static bool GenerateUVs(Vec3* pPositions, vtx_idx *pIndices, int nNumFaces,std::vector<CMesh*>& pMeshList);
		static bool GenerateLOD(const SLODGenerationInputParams *pInputParams, SLODGenerationOutput *pReturnValues);
		static void CancelLODGeneration(SLODSequenceGenerationOutputList *pReturnValues);
	};


}
