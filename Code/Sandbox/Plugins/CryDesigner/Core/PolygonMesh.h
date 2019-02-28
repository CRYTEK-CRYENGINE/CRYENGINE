// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Polygon.h"

namespace Designer
{
class PolygonMesh : public _i_reference_target_t
{
public:

	PolygonMesh();
	~PolygonMesh();

	void         SetPolygon(PolygonPtr pPolygon, bool bForce, const Matrix34& worldTM = Matrix34::CreateIdentity(), int dwRndFlags = 0, int nViewDistRatio = 100, int nMinSpec = 0, uint8 materialLayerMask = 0);
	void         SetPolygons(const std::vector<PolygonPtr>& polygonList, bool bForce, const Matrix34& worldTM = Matrix34::CreateIdentity(), int dwRndFlags = 0, int nViewDistRatio = 100, int nMinSpec = 0, uint8 materialLayerMask = 0);
	void         SetWorldTM(const Matrix34& worldTM);
	void         SetMaterial(IMaterial* pMaterial);
	void         ReleaseResources();
	IRenderNode* GetRenderNode() const { return m_pRenderNode; }

private:

	void ApplyMaterial();
	void UpdateStatObjAndRenderNode(const FlexibleMesh& mesh, const Matrix34& worldTM, int dwRndFlags, int nViewDistRatio, int nMinSpec, uint8 materialLayerMask);
	void ReleaseRenderNode();
	void CreateRenderNode();

	std::vector<PolygonPtr> m_pPolygons;
	IStatObj*               m_pStatObj;
	IRenderNode*            m_pRenderNode;
	_smart_ptr<IMaterial>   m_pMaterial;

};
}

