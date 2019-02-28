// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FbxScene.h"
#include "PhysicsProxies.h"
#include "ProxyData.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IIndexedMesh.h>

CProxyData::CProxyData()
	: m_pFbxScene(nullptr)
{}

CProxyData::~CProxyData() {}

void CProxyData::SetScene(const FbxTool::CScene* pFbxScene)
{
	CRY_ASSERT(pFbxScene);

	m_physProxies.clear();

	m_physProxiesNodeMap.clear();
	m_physProxiesNodeMap.resize(pFbxScene->GetNodeCount());

	m_pFbxScene = pFbxScene;
}

SPhysProxies* CProxyData::NewPhysProxies()
{
	m_physProxies.emplace_back(new SPhysProxies());
	signalPhysProxiesCreated(m_physProxies.back().get());
	return m_physProxies.back().get();
}

void CProxyData::AddNodePhysProxies(const FbxTool::SNode* pFbxNode, SPhysProxies* pPhysProxies)
{
	m_physProxiesNodeMap[pFbxNode->id].push_back(pPhysProxies);
}

const SPhysProxies* CProxyData::GetPhysProxiesByIndex(const FbxTool::SNode* pFbxNode, int id) const
{
	return m_physProxiesNodeMap[pFbxNode->id][id];
}

int CProxyData::GetPhysProxiesCount(const FbxTool::SNode* pFbxNode) const
{
	return m_physProxiesNodeMap[pFbxNode->id].size();
}

void CProxyData::RemovePhysProxies(SPhysProxies* pPhysProxies)
{
	if (pPhysProxies)
	{
		signalPhysProxiesAboutToBeRemoved(pPhysProxies);

		for (auto& physGeom : pPhysProxies->proxyGeometries)
		{
			gEnv->pPhysicalWorld->GetGeomManager()->UnregisterGeometry(physGeom);
		}

		auto earseIt = std::remove_if(m_physProxies.begin(), m_physProxies.end(), [pPhysProxies](const std::unique_ptr<SPhysProxies>& other)
		{
			return other.get() == pPhysProxies;
		});
		m_physProxies.erase(earseIt, m_physProxies.end());

		// Remove proxies from node mapping.
		for (auto& nodeProxies : m_physProxiesNodeMap)
		{
			auto earseIt = std::remove(nodeProxies.begin(), nodeProxies.end(), pPhysProxies);
			nodeProxies.erase(earseIt, nodeProxies.end());
		}
	}
}

void CProxyData::RemovePhysGeometry(phys_geometry* pPhysGeom)
{
	if (pPhysGeom)
	{
		signalPhysGeometryAboutToBeRemoved(pPhysGeom);
		gEnv->pPhysicalWorld->GetGeomManager()->UnregisterGeometry(pPhysGeom);

		for (auto& physProxies : m_physProxies)
		{
			auto eraseIt = std::remove_if(physProxies->proxyGeometries.begin(), physProxies->proxyGeometries.end(), [pPhysGeom](phys_geometry* other)
			{
				return other == pPhysGeom;
			});
			if (eraseIt != physProxies->proxyGeometries.end())
			{
				physProxies->proxyGeometries.erase(eraseIt, physProxies->proxyGeometries.end());
			}
		}
	}
}

uint64 CProxyData::HasPhysProxies(const FbxTool::SNode* pFbxNode) const
{
	uint64 isles = 0;
	const SPhysProxies* pProx = 0;
	for (int i = 0; i < GetPhysProxiesCount(pFbxNode); ++i)
	{
		pProx = GetPhysProxiesByIndex(pFbxNode, i);
		isles |= pProx->params.islandMap;
	}
	int n = pProx ? pProx->pSrc->isleTriCount.size() : 1;
	return isles | -(int64)((1ull << n) - 1 == isles);
}

