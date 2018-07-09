// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorPreparation.h"
#include "AutoGeneratorDataBase.h"
#include "AutoGenerator.h"

namespace LODGenerator 
{
	CAutoGeneratorPreparation::CAutoGeneratorPreparation(CAutoGenerator* pAutoGenerator):m_AutoGenerator(pAutoGenerator)
	{
		//m_bAwaitingResults = false;
	}

	CAutoGeneratorPreparation::~CAutoGeneratorPreparation()
	{

	}

// 	bool CAutoGeneratorPreparation::Tick(float* fProgress)
// 	{
// 		CLODGeneratorLib::SLODSequenceGenerationOutputList* pResults = &(m_AutoGenerator->GetDataBase().GetBakeMeshResult());
// 		if (m_bAwaitingResults)
// 		{
// 			if (CLODGeneratorLib::GetAsyncLODSequenceResults(pResults, fProgress))
// 			{
// 				m_bAwaitingResults=false;
// 			}
// 		}
// 		return !m_bAwaitingResults;
// 	}

	bool CAutoGeneratorPreparation::Generate()
	{
		CLODGeneratorLib::SLODSequenceGenerationOutputList* pResults = &(m_AutoGenerator->GetDataBase().GetBakeMeshResult());

		SAutoGeneratorParams& autoGeneratorParams = m_AutoGenerator->GetDataBase().GetParams();
		float fViewResolution = autoGeneratorParams.fViewResolution;
		int nViewsAround = autoGeneratorParams.nViewsAround;
		int nViewElevations = autoGeneratorParams.nViewElevations;
		float fSilhouetteWeight = autoGeneratorParams.fSilhouetteWeight;
		float fVertexWelding = autoGeneratorParams.fVertexWelding;
		bool bCheckTopology = autoGeneratorParams.bCheckTopology;
		bool bObjectHasBase = autoGeneratorParams.bObjectHasBase;

		CLODGeneratorLib::SLODSequenceGenerationInputParams inputParams;
		std::vector<CNodeCGF*>& nodeList = m_AutoGenerator->GetDataBase().GetNodeList();
		inputParams.nodeList.insert(inputParams.nodeList.begin(),nodeList.begin(),nodeList.end());
		inputParams.metersPerPixel=1.0f/fViewResolution;
		inputParams.numViewElevations=nViewElevations;
		inputParams.numViewsAround=nViewsAround;
		inputParams.bObjectHasBase=bObjectHasBase;
		inputParams.silhouetteWeight=fSilhouetteWeight;
		inputParams.vertexWeldingDistance=fVertexWelding;
		inputParams.bCheckTopology=bCheckTopology;

		CLODGeneratorLib::FreeLODSequence(pResults);
//		m_bAwaitingResults = false;
		if (CLODGeneratorLib::GenerateLODSequence(&inputParams, pResults, true))
		{
			//m_bAwaitingResults = true;
			return true;
		}
		return false;
	}
}