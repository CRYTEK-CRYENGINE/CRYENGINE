/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2013.
*************************************************************************/

#pragma once

#include <smartptr.h>

struct IStatObj;
namespace LODGenerator 
{
	class LODChainGenerateThread;
	class CLODGenerator
	{
	public:
		struct SLODSequenceGenerationInputParams
		{
			_smart_ptr<IStatObj> pInputMesh;
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
			LODChainGenerateThread *pThread;

			_smart_ptr<IStatObj> pStatObj;
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
			IStatObj *pStatObj;
		};

		static bool GenerateLODSequence(const SLODSequenceGenerationInputParams *pInputParams, SLODSequenceGenerationOutput *pReturnValues, bool bProcessAsync);
		static bool GetAsyncLODSequenceResults(SLODSequenceGenerationOutput *pReturnValues, float *pProgress);
		static bool FreeLODSequence(SLODSequenceGenerationOutput *pReturnValues);
		static IStatObj * BuildStatObj(SLODSequenceGenerationOutput *pSequence, const CLODGenerator::SLODGenerationInputParams *pInputParams);
		static IStatObj* GenerateUVs(Vec3* pPositions, vtx_idx *pIndices, int nNumFaces);
		static bool GenerateLOD(const SLODGenerationInputParams *pInputParams, SLODGenerationOutput *pReturnValues);
		static void CancelLODGeneration(SLODSequenceGenerationOutput *pReturnValues);
	};


}
