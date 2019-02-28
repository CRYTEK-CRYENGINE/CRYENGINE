// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorDataBase.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include <Cry3DEngine/CGF/CGFContent.h>

namespace LODGenerator 
{
	CAutoGeneratorDataBase* CAutoGeneratorDataBase::m_pInstance = NULL;

	CAutoGeneratorDataBase::CAutoGeneratorDataBase()
	{
		memset(&m_results, 0, sizeof(m_results));
	}

	CAutoGeneratorDataBase::~CAutoGeneratorDataBase()
	{

	}

	SAutoGeneratorParams& CAutoGeneratorDataBase::GetParams()
	{
		return m_autoGeneratorParams;
	}

	CLODGeneratorLib::SLODSequenceGenerationOutputList& CAutoGeneratorDataBase::GetBakeMeshResult()
	{
		return m_results;
	}

	void CAutoGeneratorDataBase::ClearAllLodMeshResults()
	{
		m_resultsMeshMap.clear();
// 		std::map<int,std::vector<CMesh*>>::iterator iter;
// 		std::vector<CMesh*>::iterator iter2;
// 		for (iter = m_resultsMeshMap.begin();iter != m_resultsMeshMap.end();iter++)
// 		{
// 			for (iter2 = iter->second.begin();iter2 != iter->second.end();iter2++)
// 			{
// 				if(*iter2 != NULL)
// 					delete *iter2;
// 			}
// 		}
	}
	//??? logic error
	bool CAutoGeneratorDataBase::AddLodMesh(int index,int iLod,std::vector<CMesh*>& pLodMeshList)
	{
		if (pLodMeshList.size() == 0)
			return false;

		for (int i = 0;i<pLodMeshList.size();i++)
		{
			if (pLodMeshList[i] == NULL)
				return false;
		}
		std::map<int,std::map<int,std::vector<CMesh*>>>::iterator iter = m_resultsMeshMap.find(index);
		if (iter == m_resultsMeshMap.end())
		{
			m_resultsMeshMap.insert(std::map<int,std::map<int,std::vector<CMesh*>>>::value_type(index,std::map<int,std::vector<CMesh*>>()));
			iter = m_resultsMeshMap.find(index);
		}

		if (iter != m_resultsMeshMap.end())
		{
			std::map<int,std::vector<CMesh*>>::iterator iter2 = m_resultsMeshMap[index].find(iLod);
			if (iter2 == m_resultsMeshMap[index].end())
			{
				m_resultsMeshMap[index].insert(std::map<int,std::vector<CMesh*>>::value_type(iLod,std::vector<CMesh*>()));
				iter2 = m_resultsMeshMap[index].find(iLod);
			}

			if (iter2 != m_resultsMeshMap[index].end())
			{
				if(m_resultsMeshMap[index][iLod].size() != 0)
				{
					for (int i=0;i<m_resultsMeshMap[index][iLod].size();i++)
					{
						if(m_resultsMeshMap[index][iLod][i] != NULL)
							delete m_resultsMeshMap[index][iLod][i];
					}
				}
				m_resultsMeshMap[index][iLod].clear();

				for (int i = 0;i<pLodMeshList.size();i++)
				{
					m_resultsMeshMap[index][iLod].push_back(pLodMeshList[i]);
				}
			}
		}

		return true;
	}

	int CAutoGeneratorDataBase::GetLodCount(int index)
	{
		std::map<int,std::map<int,std::vector<CMesh*>>>::iterator iter = m_resultsMeshMap.find(index);
		if (iter != m_resultsMeshMap.end())
		{
			return m_resultsMeshMap[index].size();
		}
		return -1;
	}

	int CAutoGeneratorDataBase::GetLodMeshCount(int index,int iLod)
	{
		std::map<int,std::map<int,std::vector<CMesh*>>>::iterator iter = m_resultsMeshMap.find(index);
		if (iter != m_resultsMeshMap.end())
		{
			std::map<int,std::vector<CMesh*>>::iterator iter2 = m_resultsMeshMap[index].find(iLod);
			if (iter2 != m_resultsMeshMap[index].end())
			{
				return m_resultsMeshMap[index][iLod].size();
			}
			else
				return -1;
		}
		return -1;
	}

	CMesh* CAutoGeneratorDataBase::GetLodMesh(int index,int iLod, int meshIndex)
	{
		std::map<int,std::map<int,std::vector<CMesh*>>>::iterator iter = m_resultsMeshMap.find(index);
		if (iter != m_resultsMeshMap.end())
		{
			std::map<int,std::vector<CMesh*>>::iterator iter2 = m_resultsMeshMap[index].find(iLod);
			if (iter2 != m_resultsMeshMap[index].end())
			{
				if (meshIndex >= iter2->second.size())
				{
					return NULL;
				}
				return iter2->second[meshIndex];
			}
			else
			{
				return NULL;
			}
		}
		else
			return NULL;
	}

	bool CAutoGeneratorDataBase::GetLodMesh(int index,int iLod, std::vector<CMesh*>** pLodObj)
	{
		std::map<int,std::map<int,std::vector<CMesh*>>>::iterator iter = m_resultsMeshMap.find(index);
		if (iter != m_resultsMeshMap.end())
		{
			std::map<int,std::vector<CMesh*>>::iterator iter2 = m_resultsMeshMap[index].find(iLod);
			if (iter2 != m_resultsMeshMap[index].end())
			{
				*pLodObj = &iter2->second;
				return true;
			}
			else
			{
				*pLodObj = NULL;
				return false;
			}
		}
		else
			return false;
	}

	bool CAutoGeneratorDataBase::SetNodeList(std::vector<CNodeCGF*>& nodeList)
	{
		if (nodeList.size() == 0)
			return false;

		for (int i=0;i<nodeList.size();i++)
		{
			if (nodeList[i] == NULL)
				return false;
		}
		
		m_NodeList.clear();
		m_NodeList.insert(m_NodeList.begin(),nodeList.begin(),nodeList.end());

		return true;
	}

	std::vector<CNodeCGF*>& CAutoGeneratorDataBase::GetNodeList()
	{
		return m_NodeList;
	}

	int CAutoGeneratorDataBase::GetNodeCount()
	{
		return m_NodeList.size();
	}

	CNodeCGF* CAutoGeneratorDataBase::GetNode(int index)
	{
		if (index >= m_NodeList.size())
			return NULL;
		return m_NodeList[index];
	}
}
