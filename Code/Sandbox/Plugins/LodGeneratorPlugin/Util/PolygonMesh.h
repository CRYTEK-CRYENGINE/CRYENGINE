#pragma once

#include "TopologyGraph.h"
namespace LODGenerator
{
	class CPolygonMesh
	{
	public:

		CPolygonMesh();
		~CPolygonMesh();

		void SetPolygon( CTopologyGraph* pPolygon, bool bForce, const Matrix34& worldTM = Matrix34::CreateIdentity(), int dwRndFlags = 0, int nViewDistRatio = 100, int nMinSpec = 0, uint8 materialLayerMask = 0 );
		void SetWorldTM( const Matrix34& worldTM );
		void SetMaterial( IMaterial* pMaterial );
		void ReleaseResources();
		IRenderNode* GetRenderNode() const { return m_pRenderNode; }

	private:

		void ApplyMaterial();
		void UpdateStatObjAndRenderNode( CTopologyGraph* pPolygon, const Matrix34& worldTM, int dwRndFlags, int nViewDistRatio, int nMinSpec, uint8 materialLayerMask );
		void ReleaseRenderNode();
		void CreateRenderNode();

		CTopologyGraph* m_pPolygon;
		IStatObj* m_pStatObj;
		IRenderNode* m_pRenderNode;
		_smart_ptr<IMaterial> m_pMaterial;

	};
}