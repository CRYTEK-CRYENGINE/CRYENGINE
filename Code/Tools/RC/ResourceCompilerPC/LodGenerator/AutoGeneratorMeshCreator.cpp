// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorMeshCreator.h"
#include "AutoGeneratorDataBase.h"
#include "AutoGeneratorHelper.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include "AutoGenerator.h"

namespace LODGenerator 
{
	CAutoGeneratorMeshCreator::CAutoGeneratorMeshCreator(CAutoGenerator* pAutoGenerator):m_AutoGenerator(pAutoGenerator)
	{
		m_fPercentage = 50;
	}

	CAutoGeneratorMeshCreator::~CAutoGeneratorMeshCreator()
	{

	}

	void CAutoGeneratorMeshCreator::SetPercentage(float fPercentage)
	{
		m_fPercentage = fPercentage;
	}

	bool CAutoGeneratorMeshCreator::Generate(const int index,const int nLodId)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = &m_AutoGenerator->GetDataBase();
		SAutoGeneratorParams& autoGeneratorParams = m_AutoGenerator->GetDataBase().GetParams();

		std::vector<CMesh*> pLodMeshList;
		bool bGenerated = false;

		CLODGeneratorLib::SLODSequenceGenerationOutput* pResult = pAutoGeneratorDataBase->GetBakeMeshResult().m_pSubObjectOutput[index];
		{
			CLODGeneratorLib::SLODGenerationInputParams lodParams;
			CLODGeneratorLib::SLODGenerationOutput lodReturn;
			lodParams.pSequence=pResult;
			lodParams.nPercentage=100*m_fPercentage;
			lodParams.bUnwrap=true;
			bGenerated = CLODGeneratorLib::GenerateLOD(&lodParams, &lodReturn);
			for (int i=0;i<lodReturn.pMeshList.size();i++)
			{
				pLodMeshList.push_back(lodReturn.pMeshList[i]);
			}
		}

		if (bGenerated)
		{
			int iMatId = -1;
			if ( PrepareMaterial(index))
			{
				iMatId = CreateLODMaterial(index,nLodId);
			}

			if (pLodMeshList.size() != 0)
			{
				if (iMatId != -1)
				{
					for (int i=0;i<pLodMeshList.size();i++)
					{
						CMesh* pMesh = pLodMeshList[i];
						for (int j=0;j<pMesh->m_subsets.size();j++)
						{
							pMesh->m_subsets[j].nMatID = iMatId;
						}
					}
				}

				pAutoGeneratorDataBase->AddLodMesh(index,nLodId,pLodMeshList);
			}
		}

		return true;
	}

	bool CAutoGeneratorMeshCreator::PrepareMaterial(const int index)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = &m_AutoGenerator->GetDataBase();
		CLODGeneratorLib::SLODSequenceGenerationOutput* pResult = pAutoGeneratorDataBase->GetBakeMeshResult().m_pSubObjectOutput[index];

		CMaterialCGF *pMaterial=NULL;
		pMaterial = pResult->node->pMaterial;

		if (!pMaterial)
			return false;

		return true;
	}

	int CAutoGeneratorMeshCreator::CreateLODMaterial(const int index,int iLod)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = &m_AutoGenerator->GetDataBase();
		CLODGeneratorLib::SLODSequenceGenerationOutput* pResult = pAutoGeneratorDataBase->GetBakeMeshResult().m_pSubObjectOutput[index];

		CMaterialCGF *pMaterial=NULL;
		pMaterial = pResult->node->pMaterial;

		if (!pMaterial)
			return -1;

		CMaterialCGF* const p = new CMaterialCGF();
		sprintf(p->name, "NoName_Lod%d", iLod);
		p->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
		pMaterial->subMaterials.push_back(p);
		int iSeat = pMaterial->subMaterials.size() - 1;

		return iSeat;
	}

}