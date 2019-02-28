// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PhysicsProxies.h"

#include <CrySandbox/CrySignal.h>

// struct SPhysProxies; TODO: forward declare.
class CProxyData;

namespace FbxTool
{
struct SNode;
}

struct phys_geometry;
struct IGeometry;

struct SRenderContext;
struct SMouseEvent;
class CCamera;

class CProxyGenerator
{
public:
	explicit CProxyGenerator(CProxyData* pProxyData);

	CProxyData* GetProxyData();

	SPhysProxies* AddPhysProxies(const FbxTool::SNode* pFbxNode, const Matrix34& mtxGlobal);
	phys_geometry* AddProxyGeom(SPhysProxies* pPhysProxies, IGeometry* pProxyGeom, bool replaceIfPresent = false);

	void CloseHoles(SPhysProxies* pProx);
	void ReopenHoles(SPhysProxies* pProx);
	void SelectAll(SPhysProxies* pProx);
	void SelectNone(SPhysProxies* pProx);
	void GenerateProxies(SPhysProxies* pProx, std::vector<phys_geometry*>& newPhysGeoms);
	void ResetProxies(SPhysProxies* pProx);
	void ResetAndRegenerate(SPhysProxies* pProx, phys_geometry* pPhysGeom, std::vector<phys_geometry*>& newPhysGeoms);

	void Reset();

	enum class EVisibility
	{
		Hidden,
		Solid,
		Wireframe
	};

	struct SShowParams
	{
		EVisibility sourceVisibility;
		EVisibility primitivesVisibility;
		EVisibility meshesVisibility;
		bool bShowVoxels;
	};

	void Render(SPhysProxies* pProx, phys_geometry* pProxyGeom, const SRenderContext& rc, const SShowParams& showParams);
	void OnMouse(SPhysProxies* pProx, const SMouseEvent& ev, const CCamera& cam);

	CCrySignal<void(const SPhysProxies*)> signalProxyIslandsChanged;
private:
	CProxyData* m_pProxyData;

	int m_hitShift;
	int m_proxyIsland;
	uint64 m_hitmask;
};
