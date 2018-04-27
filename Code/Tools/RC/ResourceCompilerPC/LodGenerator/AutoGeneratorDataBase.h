// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "AutoGeneratorLib.h"

struct CMaterialCGF;
namespace LODGenerator 
{	
	struct SAutoGeneratorParams
	{
		SAutoGeneratorParams()
		{
			fViewResolution = 26.6932144;
			nViewsAround = 12;
			nViewElevations = 3;
			fSilhouetteWeight = 5.0;
			fVertexWelding = 0.001;
			bCheckTopology = true;
			bObjectHasBase = false;
		}

		float fViewResolution;
		int nViewsAround;
		int nViewElevations;
		float fSilhouetteWeight;
		float fVertexWelding;
		bool bCheckTopology;
		bool bObjectHasBase;
	};
	class CAutoGeneratorDataBase
	{
	public:
		CAutoGeneratorDataBase();
		~CAutoGeneratorDataBase();

		bool SetNodeList(std::vector<CNodeCGF*>& nodeList);
		std::vector<CNodeCGF*>& GetNodeList();
		
		SAutoGeneratorParams& GetParams();

		CLODGeneratorLib::SLODSequenceGenerationOutputList& GetBakeMeshResult();

		void ClearAllLodMeshResults();
		bool AddLodMesh(int index,int iLod,std::vector<CMesh*>& pLodObj);
		int GetLodCount(int index);
		int GetLodMeshCount(int index,int iLod);
		bool GetLodMesh(int index,int iLod, std::vector<CMesh*>** ppLodObj);
		CMesh* GetLodMesh(int index,int iLod, int meshIndex);

		int GetNodeCount();
		CNodeCGF* GetNode(int index);

	private:
		static CAutoGeneratorDataBase * m_pInstance;

	private:
		//-------------------------------------------------------------------------------------------
		// input node list
		std::vector<CNodeCGF*> m_NodeList;

		//-------------------------------------------------------------------------------------------
		// output mesh 
		CLODGeneratorLib::SLODSequenceGenerationOutputList m_results;
		// out lod obj
		std::map<int,std::map<int,std::vector<CMesh*>>> m_resultsMeshMap;

		//-------------------------------------------------------------------------------------------
		// parameters
		SAutoGeneratorParams m_autoGeneratorParams;


	};
}