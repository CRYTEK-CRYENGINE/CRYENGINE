// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _DEFORMABLE_NODE_
#define _DEFORMABLE_NODE_

struct SDeformableData;
struct SMMRMProjectile;

class CDeformableNode
{
	SDeformableData**         m_pData;
	size_t                    m_nData;
	uint32                    m_nFrameId;
	Vec3*                     m_wind;
	primitives::sphere*       m_Colliders;
	int                       m_nColliders;
	SMMRMProjectile*          m_Projectiles;
	int                       m_nProjectiles;
	AABB                      m_bbox;
	IGeneralMemoryHeap*       m_pHeap;
	std::vector<CRenderChunk> m_renderChunks;
	_smart_ptr<IRenderMesh>   m_renderMesh;
	size_t                    m_numVertices, m_numIndices;
	CStatObj*                 m_pStatObj;
	JobManager::SJobState     m_cullState;
	JobManager::SJobState     m_updateState;
	bool                      m_all_prepared : 1;

protected:

	void UpdateInternalDeform(SDeformableData* pData
	                          , CRenderObject* pRenderObject, const AABB& bbox
	                          , const SRenderingPassInfo& passInfo
	                          , _smart_ptr<IRenderMesh>&
	                          , strided_pointer<SVF_P3S_C4B_T2S>
	                          , strided_pointer<SPipTangents>
	                          , vtx_idx* idxBuf
	                          , size_t& iv
	                          , size_t& ii);

	void ClearInstanceData();

	void ClearSimulationData();

	void CreateInstanceData(SDeformableData* pData, CStatObj* pStatObj);

	void BakeInternal(SDeformableData* pData, const Matrix34& worldTM);

public:

	CDeformableNode();

	~CDeformableNode();

	void SetStatObj(CStatObj* pStatObj);

	void CreateDeformableSubObject(bool create, const Matrix34& worldTM, IGeneralMemoryHeap* pHeap);

	void RenderInternalDeform(CRenderObject* pRenderObject
	                          , int nLod, const AABB& bbox, const SRenderingPassInfo& passInfo
	                          );

	void BakeDeform(const Matrix34& worldTM);

	bool HasDeformableData() const { return m_nData != 0; }
};

#endif
