#include "StdAfx.h"
#include "AutoGeneratorPreparation.h"
#include "AutoGeneratorDataBase.h"

namespace LODGenerator 
{
	CAutoGeneratorPreparation::CAutoGeneratorPreparation()
	{
		m_bAwaitingResults = false;
	}

	CAutoGeneratorPreparation::~CAutoGeneratorPreparation()
	{

	}

	bool CAutoGeneratorPreparation::Tick(float* fProgress)
	{
		CLODGeneratorLib::SLODSequenceGenerationOutput* pResults = &(CAutoGeneratorDataBase::Instance()->GetBakeMeshResult());
		if (m_bAwaitingResults)
		{
			if (CLODGeneratorLib::GetAsyncLODSequenceResults(pResults, fProgress))
			{
				m_bAwaitingResults=false;
			}
		}
		return !m_bAwaitingResults;
	}

	bool CAutoGeneratorPreparation::Generate()
	{
		CLODGeneratorLib::SLODSequenceGenerationOutput* pResults = &(CAutoGeneratorDataBase::Instance()->GetBakeMeshResult());

		CAutoGeneratorParams& autoGeneratorParams = CAutoGeneratorDataBase::Instance()->GetParams();
		int nSourceLod = autoGeneratorParams.GetGeometryOption<int>("nSourceLod");
		float fViewResolution = autoGeneratorParams.GetGeometryOption<float>("fViewResolution");
		int nViewsAround = autoGeneratorParams.GetGeometryOption<float>("nViewsAround");
		int nViewElevations = autoGeneratorParams.GetGeometryOption<float>("nViewElevations");
		float fSilhouetteWeight = autoGeneratorParams.GetGeometryOption<float>("fSilhouetteWeight");
		float fVertexWelding = autoGeneratorParams.GetGeometryOption<float>("fVertexWelding");
		bool bCheckTopology = autoGeneratorParams.GetGeometryOption<float>("bCheckTopology");
		bool bObjectHasBase = autoGeneratorParams.GetGeometryOption<bool>("bObjectHasBase");

		IStatObj* pObj = CAutoGeneratorDataBase::Instance()->GetLoadedModel(0);
		CLODGeneratorLib::SLODSequenceGenerationInputParams inputParams;
		inputParams.pInputMesh= nSourceLod == 0 ? pObj : pObj->GetLodObject(nSourceLod);
		inputParams.metersPerPixel=1.0f/fViewResolution;
		inputParams.numViewElevations=nViewElevations;
		inputParams.numViewsAround=nViewsAround;
		inputParams.bObjectHasBase=bObjectHasBase;
		inputParams.silhouetteWeight=fSilhouetteWeight;
		inputParams.vertexWeldingDistance=fVertexWelding;
		inputParams.bCheckTopology=bCheckTopology;

		CLODGeneratorLib::FreeLODSequence(pResults);
		m_bAwaitingResults = false;
		if (CLODGeneratorLib::GenerateLODSequence(&inputParams, pResults, true))
		{
			m_bAwaitingResults = true;
			return true;
		}
		return false;
	}
}