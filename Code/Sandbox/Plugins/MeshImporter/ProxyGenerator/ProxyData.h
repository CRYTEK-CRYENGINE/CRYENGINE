// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

#include <memory>
#include <vector>

struct SPhysProxies;

namespace FbxTool
{

class CScene;
struct SNode;

} // namespace FbxTool

struct IStatObj;

// Holds proxy data and maps it to nodes of FBX scene.
class CProxyData
{
public:
	CProxyData();
	~CProxyData();

	void SetScene(const FbxTool::CScene* pFbxScene);

	SPhysProxies* NewPhysProxies();

	void AddNodePhysProxies(const FbxTool::SNode* pFbxNode, SPhysProxies* pPhysProxies);

	const SPhysProxies* GetPhysProxiesByIndex(const FbxTool::SNode* pFbxNode, int id) const;
	int GetPhysProxiesCount(const FbxTool::SNode* pFbxNode) const;

	void RemovePhysProxies(SPhysProxies* pPhysProxies);
	void RemovePhysGeometry(phys_geometry* pPhysGeom);

	uint64 HasPhysProxies(const FbxTool::SNode* pFbxNode) const;

	CCrySignal<void(SPhysProxies*)> signalPhysProxiesCreated;
	CCrySignal<void(SPhysProxies*, phys_geometry*)> signalPhysGeometryCreated;
	CCrySignal<void(phys_geometry* pOld, phys_geometry* pNew)> signalPhysGeometryAboutToBeReused;
	CCrySignal<void(SPhysProxies*)> signalPhysProxiesAboutToBeRemoved;
	CCrySignal<void(phys_geometry*)> signalPhysGeometryAboutToBeRemoved;
private:
	const FbxTool::CScene* m_pFbxScene;

	std::vector<std::unique_ptr<SPhysProxies>> m_physProxies;
	std::vector<std::vector<SPhysProxies*>> m_physProxiesNodeMap;  // Maps physics proxies to nodes. Indexed by node id.
};

