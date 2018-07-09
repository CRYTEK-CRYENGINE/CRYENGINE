// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WriteProxies.h"
#include "ProxyData.h"
#include "PhysicsProxies.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryPhysics/physinterface.h>

void LogPrintf(const char* szFormat, ...);

namespace Private_WriteProxies
{

static IStatObj* SaveProxies(IStatObj* pStatObj, const SProxyTree* pProxyTree, const SProxyTree::SNode* pProxyTreeNode, int& nProxies, int slotParent = 0)
{
	for (size_t i = 0; i < pProxyTreeNode->m_physGeometryCount; ++i)
	{
		phys_geometry* const pProxyGeom = pProxyTree->m_physGeometries[pProxyTreeNode->m_firstPhysGeometryIndex + i];

		if (!(pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND))
		{
			IStatObj* pMeshObj = pStatObj;
			pStatObj = gEnv->p3DEngine->CreateStatObj();
			pStatObj->FreeIndexedMesh();
			pStatObj->AddSubObject(pMeshObj);
		}
		IStatObj* pProxObj = gEnv->p3DEngine->CreateStatObj();
		static CMesh meshDummy;
		if (!meshDummy.GetVertexCount())
		{
			meshDummy.SetIndexCount(3);
			meshDummy.SetVertexCount(3);
			for (int i = 0; i < 3; i++)
			{
				meshDummy.m_bbox.Add(meshDummy.m_pPositions[i] = Vec3(1, 0, 0).GetRotated(Vec3(1, 1, 1) * (1 / sqrt3), i * gf_PI * 0.5f));
				meshDummy.m_pIndices[i] = i;
				meshDummy.m_pNorms[i] = SMeshNormal(Vec3(1, 1, 1) * (1 / sqrt3));
			}
			meshDummy.m_subsets.resize(1);
			SMeshSubset& ss = meshDummy.m_subsets[0];
			memset(&ss, 0, sizeof(ss));
			ss.nNumIndices = ss.nNumVerts = 3;
			ss.nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			ss.fRadius = 1;
		}
		pProxObj->GetIndexedMesh(true)->SetMesh(meshDummy);
		pProxObj->Invalidate(false);
		pProxObj->SetPhysGeom(pProxyGeom, pProxyGeom->nMats);
		pProxyGeom->nRefCount++;
		const char* name = slotParent >= 0 ? "$physics_proxy" : "";
		pProxObj->SetGeoName(name);
		IStatObj::SSubObject& subobj = pStatObj->AddSubObject(pProxObj);
		subobj.name = name;
		subobj.nParent = slotParent;
		nProxies++;
	}

	slotParent = pStatObj->GetSubObjectCount() - 1;
	while (slotParent >= 0 && pStatObj->GetSubObject(slotParent)->name != pProxyTreeNode->m_name)
	{
		slotParent--;
	}

	for (size_t i = 0; i < pProxyTreeNode->m_childCount; ++i)
	{
		const SProxyTree::SNode* const pChildNode = &pProxyTree->m_nodes[pProxyTreeNode->m_firstChildIndex + i];
		pStatObj = SaveProxies(pStatObj, pProxyTree, pChildNode, nProxies, slotParent);
	}

	return pStatObj;
}

} // namespace Private_WriteProxies


SProxyTree::SProxyTree(const CProxyData* proxyData, const FbxTool::CScene* pFbxScene)
{
	const FbxTool::SNode* const pRoot = pFbxScene->GetRootNode();
	if (!pFbxScene->IsNodeIncluded(pRoot))
	{
		return;
	}

	m_nodes.reserve(pFbxScene->GetNodeCount());
	std::queue<const FbxTool::SNode*> Q;
	Q.push(pRoot);
	while (!Q.empty())
	{
		const FbxTool::SNode* pFbxNode = Q.front();
		Q.pop();
		m_nodes.emplace_back();
		SProxyTree::SNode& proxyTreeNode = m_nodes.back();
		proxyTreeNode.m_name = pFbxNode->szName;

		proxyTreeNode.m_firstChildIndex = m_nodes.size();
		proxyTreeNode.m_childCount = 0;
		for (size_t i = 0; i < pFbxNode->numChildren; ++i)
		{
			if (pFbxScene->IsNodeIncluded(pFbxNode))
			{
				Q.push(pFbxNode->ppChildren[i]);
				proxyTreeNode.m_childCount++;
			}
		}

		proxyTreeNode.m_firstPhysGeometryIndex = m_physGeometries.size();
		proxyTreeNode.m_physGeometryCount = 0;
		for (int i = 0, N = proxyData->GetPhysProxiesCount(pFbxNode); i < N; ++i)
		{
			const SPhysProxies* const pPhysProxies = proxyData->GetPhysProxiesByIndex(pFbxNode, i);
			m_physGeometries.insert(m_physGeometries.end(), pPhysProxies->proxyGeometries.begin(), pPhysProxies->proxyGeometries.end());
			proxyTreeNode.m_physGeometryCount += pPhysProxies->proxyGeometries.size();
		}
	}

	for (phys_geometry* pPhysGeom : m_physGeometries)
	{
		pPhysGeom->nRefCount++;
	}
}

SProxyTree::~SProxyTree()
{
	for (phys_geometry* pPhysGeom : m_physGeometries)
	{
		pPhysGeom->nRefCount--;
	}
}

void WriteAutoGenProxies(const string& fname, const SProxyTree* pProxyTree)
{	
	using namespace Private_WriteProxies;

	if (!pProxyTree->m_nodes.size())
	{
		return;
	}

	_smart_ptr<IStatObj> pStatObj = gEnv->p3DEngine->FindStatObjectByFilename(fname.c_str());
	if (pStatObj)
		pStatObj->Refresh(FRO_GEOMETRY);
	else
		pStatObj = gEnv->p3DEngine->LoadStatObj(fname.c_str(), 0, 0, false);
	if (!pStatObj)
	{
		LogPrintf("Can't find the generated file %s\n", fname.c_str());
		return;
	}
	for (int i = 0; i < pStatObj->GetSubObjectCount(); i++)
		if (pStatObj->GetSubObject(i)->pStatObj)
			pStatObj->GetSubObject(i)->pStatObj->GetIndexedMesh(true);
	int nProxies = 0;
	if ((pStatObj = SaveProxies(pStatObj, pProxyTree, &pProxyTree->m_nodes[0], nProxies)) && nProxies)
	{
		LogPrintf("Adding %d auto-generated physics proxies\n", nProxies);
		pStatObj->SaveToCGF(fname.c_str());
	}
}

