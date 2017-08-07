// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/PoolAllocator.h>

struct SRenderNodeTempData
{
	struct SUserData
	{
		int                             lastSeenFrame[MAX_RECURSION_LEVELS]; // must be first, see IRenderNode::SetDrawFrame()
		int                             lastSeenShadowFrame;                 // When was last rendered to shadow
		CRenderObject*                  arrPermanentRenderObjects[MAX_STATOBJ_LODS_NUM];
		float														arrLodLastTimeUsed[MAX_STATOBJ_LODS_NUM];
		Matrix34                        objMat;
		OcclusionTestClient             m_OcclState;
		struct IFoliage*                m_pFoliage;
		struct IClipVolume*             m_pClipVolume;

		Vec4                            vEnvironmentProbeMults;
		uint32                          nCubeMapId                  : 16;
		uint32                          nCubeMapIdCacheClearCounter : 16;
		uint32                          nWantedLod                  : 8;
		uint32                          bToDelete                   : 1;
		uint32                          bTerrainColorWasUsed        : 1;
		volatile int                    permanentObjectCreateLock;
		IRenderNode*                    pOwnerNode;
		uint32                          nStatObjLastModificationId;
	};

public:
	SUserData userData;

public:
	SRenderNodeTempData() { ZeroStruct(*this); assert((void*)this == (void*)&this->userData); }
	~SRenderNodeTempData() { Free(); };

	void Free();
	void FreeRenderObjects();
	void InvalidateRenderObjectsInstanceData();

	void OffsetPosition(const Vec3& delta)
	{
		userData.objMat.SetTranslation(userData.objMat.GetTranslation() + delta);
	}
	bool IsValid() const { return !userData.bToDelete; }

	void MarkForDelete()
	{
		userData.bToDelete = true;
		userData.pOwnerNode = 0;
	}
};

// Class responsible for managing potentially visible render nodes.
class CVisibleRenderNodesManager
{
public:
	static int const MAX_NODES_CHECK_PER_FRAME = 500;
	static int const MAX_DELETE_BUFFERS = 3;

	struct Statistics
	{
		int numFree;
		int numUsed;
	};

public:
	CVisibleRenderNodesManager();
	~CVisibleRenderNodesManager();

	SRenderNodeTempData* AllocateTempData(int lastSeenFrame);

	// Set last frame for this rendering pass.
	// Return if last frame changed and rendering should be done this frame for this pass.
	bool SetLastSeenFrame(SRenderNodeTempData* pTempData, const SRenderingPassInfo& passInfo);

	// Iteratively update array of visible nodes checking if they are expired
	void       UpdateVisibleNodes(int currentFrame, int maxNodesToCheck = MAX_NODES_CHECK_PER_FRAME);
	void       InvalidateAll();
	void       OnEntityDeleted(IEntity *pEntity);
	Statistics GetStatistics() const;

	void       ClearAll();

private:
	void OnRenderNodeVisibilityChange( IRenderNode *pRenderNode,bool bVisible );

private:
	std::vector<SRenderNodeTempData*> m_visibleNodes;
	int                               m_lastStartUpdateNode;

	int                               m_firstAddedNode;

	int                               m_currentNodesToDelete;
	std::vector<SRenderNodeTempData*> m_toDeleteNodes[MAX_DELETE_BUFFERS];

	int                               m_lastUpdateFrame;

	struct PoolSyncCriticalSection
	{
		void Lock()   { m_cs.Lock(); }
		void Unlock() { m_cs.Unlock(); }
		CryCriticalSectionNonRecursive m_cs;
	};
	stl::TPoolAllocator<SRenderNodeTempData, PoolSyncCriticalSection> m_pool;

	CryCriticalSectionNonRecursive m_accessLock;
};
