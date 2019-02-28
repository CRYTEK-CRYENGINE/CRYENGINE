// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGenerator.h"
#include "AutoGeneratorDataBase.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include <Cry3DEngine/CGF/CGFContent.h>
#include "StringHelpers.h"
#include "../FBX/ImportRequest.h"

namespace LODGenerator 
{
	CAutoGenerator::CAutoGenerator()
	{
		m_PrepareDone = false;
		m_AutoGeneratorDataBase.ClearAllLodMeshResults();

		m_AutoGeneratorPreparation = new CAutoGeneratorPreparation(this);
		m_AutoGeneratorMeshCreator = new CAutoGeneratorMeshCreator(this);

		m_ContentCGF = NULL;
	}

	CAutoGenerator::~CAutoGenerator()
	{
		if (m_AutoGeneratorPreparation != NULL)
		{
			delete m_AutoGeneratorPreparation;
		}
		if (m_AutoGeneratorMeshCreator != NULL)
		{
			delete m_AutoGeneratorMeshCreator;
		}
	}

	bool CAutoGenerator::Init(CContentCGF& cgf,const CImportRequest& ir)
	{
		m_ContentCGF = &cgf;
		m_AutoLodSettings = &ir.autoLodSettings;

		// Create Lod
		int iNodeCount = cgf.GetNodeCount();
		std::vector<CNodeCGF*> nodeList;
		for (int i=0;i<iNodeCount;i++)
		{
			CNodeCGF* pNode =  cgf.GetNode(i);
			if (pNode->type != CNodeCGF::ENodeType::NODE_MESH)
			{
				continue;
			}

			if (!m_AutoLodSettings->IncludeNode(pNode->name))
				continue;

			CAutoLodSettings::sNodeParam nodeParam = m_AutoLodSettings->GetNodeParam(pNode->name);
			if (nodeParam.m_bAutoGenerate == false)
				continue;

			nodeList.push_back(pNode);
		}

		// wait for add logic
		if(!m_AutoGeneratorDataBase.SetNodeList(nodeList))
			return false;

		for each (CNodeCGF* node in nodeList)
		{
			if (!m_AutoLodSettings->IncludeNode(node->name))
				continue;

			CAutoLodSettings::sNodeParam nodeParam = m_AutoLodSettings->GetNodeParam(node->name);
			int iLodCount = nodeParam.m_iLodCount;
			float fOriginPercent = nodeParam.m_fPercent;
			for (int i=0;i<iLodCount;i++)
			{
				RCLog("Add Lod %d Percent:%f",i,fOriginPercent);
				int index = i + 1;
				float percent = pow(fOriginPercent,index);

				for (int i=0;i<m_NodeLodInfoList[node].size();i++)
				{
					if (m_NodeLodInfoList[node][i].percentage == percent)
					{
						RCLog("Node %s has the same lod(percent: %f)",node->name,percent);
						return false;
					}
				}

				CAutoGenerator::SLodInfo lodInfo;
				lodInfo.percentage = percent;
				lodInfo.lod = m_NodeLodInfoList[node].size() + 1;
				m_NodeLodInfoList[node].push_back(lodInfo);
			}
		}

		m_PrepareDone = false;

		return true;	
	}

	bool CAutoGenerator::Prepare()
	{
		m_PrepareDone = false;
		if(m_AutoGeneratorPreparation->Generate())
		{
			m_PrepareDone = true;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool CAutoGenerator::IsPrepareDone()
	{
		return m_PrepareDone;
	}

	bool CAutoGenerator::AutoCreateNodes()
	{
		std::vector<CNodeCGF*>& nodeList = m_AutoGeneratorDataBase.GetNodeList();
		for (int i=0;i<nodeList.size();i++)
		{
			int iLodCount = m_AutoGeneratorDataBase.GetLodCount(i);
			if (iLodCount <= 0)
				continue;

			for (int j=0;j<iLodCount;j++)
			{
				int iMeshCount = m_AutoGeneratorDataBase.GetLodMeshCount(i,j+1);
				for (int l=0;l<iMeshCount;l++)
				{
					string s = StringHelpers::Format("%s%i_%s_%d_", CGF_NODE_NAME_LOD_PREFIX,j+1 ,nodeList[i]->name,l);
					CMesh* pMesh = m_AutoGeneratorDataBase.GetLodMesh(i,j+1,l);
					if (pMesh == NULL)
						continue;

					CNodeCGF* const pCGFNode = new CNodeCGF();
					pCGFNode->pos_cont_id = 0;
					pCGFNode->rot_cont_id = 0;
					pCGFNode->scl_cont_id = 0;

					pCGFNode->localTM = Matrix34(IDENTITY);
					pCGFNode->worldTM = Matrix34(IDENTITY);
					pCGFNode->pParent = nodeList[i];

					pCGFNode->bIdentityMatrix = pCGFNode->worldTM.IsIdentity();

					pCGFNode->bPhysicsProxy = false;
					pCGFNode->nPhysicalizeFlags = 0;

					//pCGFNode->type = CNodeCGF::NODE_MESH;
					pCGFNode->type = CNodeCGF::NODE_HELPER;
					pCGFNode->helperType = HP_GEOMETRY;
					pCGFNode->pMesh = pMesh;

					pCGFNode->pMaterial = nodeList[i]->pMaterial;

					memset(pCGFNode->name,0,sizeof(pCGFNode->name));
					StringHelpers::SafeCopy(pCGFNode->name,s.length()+1,s.c_str());			

					m_ContentCGF->AddNode(pCGFNode);
				}
			}
		}

		return true;
	}

	bool CAutoGenerator::GenerateLodMesh(int index,int level)
	{
		RCLog("Generate Lod Mesh NodeIndex:%d LodLevel:%d",index,level);

		if (!m_PrepareDone)
			return false;

		if (level <= 0)
			return false;

		if (index < 0)
			return false;

		int iLodIndex = level - 1;
		CNodeCGF* node = m_AutoGeneratorDataBase.GetNode(index);
		CAutoGenerator::SLodInfo& lodInfo = m_NodeLodInfoList[node][iLodIndex];
		m_AutoGeneratorMeshCreator->SetPercentage(lodInfo.percentage);
		return m_AutoGeneratorMeshCreator->Generate(index,level);
	}

	bool CAutoGenerator::GenerateAllLodMesh()
	{
		if (!m_PrepareDone)
			return false;

		int iNodeCount = m_AutoGeneratorDataBase.GetNodeCount();

		for (int i=0;i<iNodeCount;i++)
		{
			CNodeCGF* node = m_AutoGeneratorDataBase.GetNode(i);
			if (node != NULL)
			{
				for (int j=0;j<m_NodeLodInfoList[node].size();j++)
				{
					if (!GenerateLodMesh(i,j + 1))
					{
						return false;
					}
				}
			}

		}


		return true;
	}

	bool CAutoGenerator::GetLodMesh(int index,int level, std::vector<CMesh*>** ppLodMesh)
	{
		return m_AutoGeneratorDataBase.GetLodMesh(index,level,ppLodMesh);
	}

	bool CAutoGenerator::GenerateLod(int index,int level)
	{
		if (!GenerateLodMesh(index,level))
			return false;

		return true;
	}

	bool CAutoGenerator::GenerateAllLod()
	{
		RCLog("Generate All Lod Mesh");
		if (!GenerateAllLodMesh())
			return false;

		RCLog("Generate All Lod Node");
		if(!AutoCreateNodes())
			return false;

		return true;
	}
}