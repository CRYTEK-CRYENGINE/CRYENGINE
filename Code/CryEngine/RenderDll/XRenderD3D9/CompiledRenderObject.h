// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/PoolAllocator.h>
#include "xxhash.h"

// Special purpose array, base on the DynArray implementation, but embedding first elements into the body of the class itself
// Used for a special optimization case where array contain only few elements, in this case no dynamic allocation is needed.
template<class T, class I = int, class STORE = NArray::SmallDynStorage<>>
struct SSpecialRenderItemArray
{
	enum { eCachedItems = 3 };
public:
	SSpecialRenderItemArray() : m_size(0), m_capacity(0), m_array(0) {}
	~SSpecialRenderItemArray()
	{
		if (m_array)
			CryModuleFree(m_array);
	}
	void push_back(const T& value)
	{
		int index = m_size++;
		if (index >= eCachedItems)
		{
			if (m_size - eCachedItems > m_capacity)
			{
				m_capacity = m_capacity + 16;
				m_array = (T*)CryModuleRealloc(m_array, m_capacity * sizeof(T));
			}
			m_array[index - eCachedItems] = value;
		}
		else
			m_cached[index] = value;
	}
	T& operator[](size_t index)
	{
		if (index >= eCachedItems)
			return m_array[index - eCachedItems];
		else
			return m_cached[index];
	}
	const T& operator[](size_t index) const
	{
		if (index >= eCachedItems)
			return m_array[index - eCachedItems];
		else
			return m_cached[index];
	}
	size_t size() const { return m_size; }

private:
	size_t m_size;
	size_t m_capacity;
	T      m_cached[eCachedItems];
	T*     m_array;
};

//////////////////////////////////////////////////////////////////////////
// Class used to allocate/deallocate permanent render objects.
class CPermanentRenderObject : public IPermanentRenderObject
{
public:
	CPermanentRenderObject()
		: m_accessLock(0)
	{
		m_bPermanent = true; // must be in constructor
	};
	~CPermanentRenderObject();

	void AddRenderItem(CRenderElement* pElem, const SShaderItem& shaderItem, const AABB& aabb, float distance, uint32 nBatchFlags, ERenderListID nList, ERenderPassType renderPassType)
	{
		WriteLock lock(m_accessLock); // Block on write access to the render object

		// Must add this render item to the local array.
		// This is not thread safe!!!, code at 3d engine makes sure this can never be called simultaneously on the same RenderObject on the same pass type.
		//  General and shadows can be called simultaneously, they are writing to the separate arrays.
		SPermanentRendItem pri = {0};

		pri.m_sortValue = SRendItem::PackShaderItem(shaderItem);
		pri.m_nBatchFlags = nBatchFlags;
		pri.m_nRenderList = nList;
		pri.m_pCompiledObject = nullptr;
		pri.m_pRenderElement = pElem;
		//pri.nStencRef = m_nClipVolumeStencilRef + 1;
		pri.m_ElmFlags = pElem->m_Flags;
		pri.m_ObjFlags = m_ObjFlags;
 		pri.m_aabb = aabb;
 		pri.m_fDist = distance;

		m_permanentRenderItems[renderPassType].push_back(pri);
		PrepareForUse(pElem, renderPassType);
	}

	//
	void PrepareForUse(CRenderElement* pRenderElement, ERenderPassType passType)
	{
		if (pRenderElement && pRenderElement->mfGetType() == eDATA_Mesh)
		{
			CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(static_cast<CREMesh*>(pRenderElement)->m_pRenderMesh);
			if (pRenderMesh != m_pUsedRenderMesh[passType])
			{
				// Cache this render mesh, to prevent it's deletion while we still reference it
				m_pUsedRenderMesh[passType] = pRenderMesh;
			}
		}
		if (m_pCurrMaterial != m_pUsedMaterial)
		{
			// Cache used material
			m_pUsedMaterial = m_pCurrMaterial;
		}
	}

	static CPermanentRenderObject* AllocateFromPool();
	static void                    FreeToPool(CPermanentRenderObject* pObj);

	static void                    SetStaticPools(CRenderObjectsPools* pools) { s_pPools = pools; }

public:
	struct SPermanentRendItem
	{
		uint32                 m_sortValue;       //!< Encoded sort value, keeping info about material.
		uint32                 m_nBatchFlags;     //!< see EBatchFlags, batch flags describe on what passes this object will be used (transparent,z-prepass,etc...)
		CCompiledRenderObject* m_pCompiledObject; //!< Compiled object with precompiled PSO
		CRenderElement*        m_pRenderElement;  //!< Mesh subset, or a special rendering object

		ERenderObjectFlags     m_ObjFlags;        //!< flags calculated at add-time
		ERenderElementFlags    m_ElmFlags;
		float                  m_fDist;           //!< distance to render-view camera
		AABB                   m_aabb;            //!< AABB in local space
		ERenderListID          m_nRenderList;     //!< Defines in which render list this compiled object belongs to, see ERenderListID
	};
	//! Persistent object record all items added to the object for the general and for shadow pass.
	//! These items are then efficiently expanded adding render items to CRenderView lists.
	SSpecialRenderItemArray<SPermanentRendItem> m_permanentRenderItems[eRenderPass_NumTypes];

	// Make a reference to a render mesh, to prevent it being deleted while permanent render object still exist.
	_smart_ptr<CRenderMesh> m_pUsedRenderMesh[eRenderPass_NumTypes];
	_smart_ptr<IMaterial>   m_pUsedMaterial;

	mutable volatile int    m_accessLock;

private:
	static CRenderObjectsPools* s_pPools;
};

//////////////////////////////////////////////////////////////////////////
// Collection of pools used by CompiledObjects
class CRenderObjectsPools
{
public:
	CRenderObjectsPools();
	~CRenderObjectsPools();
	CConstantBufferPtr AllocatePerDrawConstantBuffer();
	void               FreePerDrawConstantBuffer(CConstantBufferPtr&& buffer);

public:

	struct PoolSyncCriticalSection
	{
		void Lock()   { m_cs.Lock(); }
		void Unlock() { m_cs.Unlock(); }
		CryCriticalSectionNonRecursive m_cs;
	};

	// Global pool for compiled objects, overridden class new/delete operators use it.
	stl::TPoolAllocator<CCompiledRenderObject, PoolSyncCriticalSection> m_compiledObjectsPool;

	// Pool used to allocate permanent render objects
	stl::TPoolAllocator<CPermanentRenderObject, PoolSyncCriticalSection> m_permanentRenderObjectsPool;

private:
	// Used to pool per instance constant buffers.
	std::vector<CConstantBufferPtr> m_freeConstantBuffers;
};

enum EObjectCompilationOptions : uint8
{
	eObjCompilationOption_PipelineState             = BIT(0),
	eObjCompilationOption_PerDrawConstantBuffer     = BIT(1),
	eObjCompilationOption_PerDrawExtraResources     = BIT(2),
	eObjCompilationOption_InputStreams              = BIT(3), // e.g. geometry streams (vertex, index buffers)

	eObjCompilationOption_None                      = 0,
	eObjCompilationOption_PerDrawDataOnly           = eObjCompilationOption_PerDrawConstantBuffer     |
	                                                  eObjCompilationOption_PerDrawExtraResources,
	eObjCompilationOption_All                       = eObjCompilationOption_PipelineState             |
	                                                  eObjCompilationOption_PerDrawConstantBuffer     |
	                                                  eObjCompilationOption_PerDrawExtraResources |
	                                                  eObjCompilationOption_InputStreams
};
DEFINE_ENUM_FLAG_OPERATORS(EObjectCompilationOptions);

// Used by Graphics Pass pipeline
class CCompiledRenderObject
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Instance data is mapped directly to shader per instance constant buffer
	struct SPerInstanceShaderData
	{
		Matrix34 matrix;

		// 4 values making Vec4 (CustomData)
		float vegetationBendingRadius;
		float vegetationBendingScale;
		float dissolve;
		float tesselationPatchId;
	};

	struct SRootConstants
	{
	};

	struct SDrawParams
	{
		int32 m_nVerticesCount;
		int32 m_nStartIndex;
		int32 m_nNumIndices;

		SDrawParams()
			: m_nVerticesCount(0)
			, m_nStartIndex(0)
			, m_nNumIndices(0)
		{}
	};

	enum EDrawParams
	{
		eDrawParam_Shadow,
		eDrawParam_General,
		eDrawParam_Count,
	};

public:
	/////////////////////////////////////////////////////////////////////////////
	// Packed parameters
	uint32 m_StencilRef                 : 8; //!< Stencil ref value
	uint32 m_nNumVertexStreams          : 8; //!< Number of vertex streams specified
	uint32 m_nLastVertexStreamSlot      : 8; //!< Highest vertex stream slot
	uint32 m_bOwnPerInstanceCB          : 1; //!< True if object owns its own per instance constant buffer, and is not sharing it with other compiled object
	uint32 m_bIncomplete                : 1; //!< True if compilation failed
	uint32 m_bHasTessellation           : 1; //!< True if tessellation is enabled
	uint32 m_bSharedWithShadow          : 1; //!< True if objects is shared between shadow and general pass
	uint32 m_bDynamicInstancingPossible : 1; //!< True if this render object can be dynamically instanced with other compiled render object
	uint32 m_bCustomRenderElement       : 1; //!< When dealing with not known render element, will cause Draw be redirected to the RenderElement itself
	/////////////////////////////////////////////////////////////////////////////

	// AABB in world space
	AABB m_aabb;

	//////////////////////////////////////////////////////////////////////////
	// Members used in submitting in the draw submission, sorted by access order
	// This area should be smaller than 128 bytes (2x 64byte cache-lines)

	// From Material
	CDeviceResourceSetPtr m_materialResourceSet;

	// Per instance constant buffer contain transformation matrix and other potentially not often changed data.
	CConstantBufferPtr    m_perDrawCB;

	// Per instance extra data
	CDeviceResourceSetPtr m_perDrawExtraResources;

	// Streams data.
	const CDeviceInputStream* m_vertexStreamSet;
	const CDeviceInputStream* m_indexStreamSet;

	uint32                    m_perDrawInstances;
	CConstantBufferPtr        m_pInstancingConstBuffer; //!< Constant Buffer with all instances

	// DrawCall parameters, store separate values for merged shadow-gen draw calls
	SDrawParams m_drawParams[eDrawParam_Count];

	// Array of the PSOs for every ERenderableTechnique.
	// Sub array of PSO is defined inside the pass, and indexed by an integer passed inside the SGraphicsPipelinePassContext
	// This data-blob alone is 200 bytes (the head of the array lies within the first 128 bytes)
	DevicePipelineStatesArray m_pso[eStage_SCENE_NUM];

	// / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / / /
	// These 2 members must be initialized prior to calling Compile
	CRenderElement*        m_pRenderElement;
	CRenderObject*         m_pRO;
	SShaderItem            m_shaderItem;

	//////////////////////////////////////////////////////////////////////////
	EObjectCompilationOptions m_compilationDirtyFlags[CRenderObject::eRenderPass_NumTypes];

	// Optimized access to constants that can be directly bound to the root signature.
	SRootConstants         m_rootConstants;

	int                    m_TessellationPatchIDOffset; // for tessellation only
	CGpuBuffer*            m_pTessellationAdjacencyBuffer;
	CGpuBuffer*            m_pExtraSkinWeights; // eight weight skinning

	// Used for instancing.
	SPerInstanceShaderData m_instanceData;

	//////////////////////////////////////////////////////////////////////////
public:
	CCompiledRenderObject()
		: m_StencilRef(0)
		, m_nNumVertexStreams(0)
		, m_nLastVertexStreamSlot(0)
		, m_bOwnPerInstanceCB(false)
		, m_bIncomplete(true)
		, m_bHasTessellation(false)
		, m_bSharedWithShadow(false)
		, m_bCustomRenderElement(false)
		, m_vertexStreamSet(nullptr)
		, m_indexStreamSet(nullptr)
		, m_perDrawInstances(1)
		, m_pRenderElement(nullptr)
		, m_pRO(nullptr)
		, m_compilationDirtyFlags{ eObjCompilationOption_All, eObjCompilationOption_All }
		, m_TessellationPatchIDOffset(-1)
		, m_pTessellationAdjacencyBuffer(nullptr)
		, m_pExtraSkinWeights(nullptr)
	{
		for (int i = 0; i < eStage_SCENE_NUM; ++i)
		{
			std::fill(m_pso[i].begin(), m_pso[i].end(), nullptr);
		}
	}
	~CCompiledRenderObject();

	// Compile(): Returns true if the compilation is fully finished, false if compilation should be retriggered later

	bool Compile(EObjectCompilationOptions compilationOptions, const CRenderObject::SInstanceInfo& instanceInfo, uint64 objFlags, ERenderElementFlags elmFlags, const AABB& localAABB, CRenderView* pRenderView);
	void PrepareForUse(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bInstanceOnly) const;

	void DrawToCommandList(const SGraphicsPipelinePassContext& RESTRICT_REFERENCE passContext, CDeviceCommandList* commandList, CConstantBuffer* pDynamicInstancingBuffer = nullptr, uint32 dynamicInstancingCount = 1) const;

	void Init(const SShaderItem& shaderItem, CRenderElement* pRE);

	// Check if on the fly instancing can be used between this and next object
	// If instancing is possible writes current object instance data to the outputInstanceData parameter.
	bool CheckDynamicInstancing(const SGraphicsPipelinePassContext& RESTRICT_REFERENCE passContext, const CCompiledRenderObject* RESTRICT_POINTER pNextObject) const;

	// Returns cached data used to fill dynamic instancing buffer
	const SPerInstanceShaderData& GetInstancingData() const { return m_instanceData; }

	static CCompiledRenderObject* AllocateFromPool();
	static void                   FreeToPool(CCompiledRenderObject* ptr);
	static void                   SetStaticPools(CRenderObjectsPools* pools) { s_pPools = pools; }

private:
	void CompilePerDrawCB(CRenderObject* pRenderObject, const CRenderObject::SInstanceInfo& instanceInfo, uint64 objFlags);
	void CompilePerInstanceCB(CRenderObject* pRenderObject, bool bForce);
	void CompilePerDrawExtraResources(CRenderObject* pRenderObject, CRenderView* pRenderView);

	void UpdatePerDrawCB(void* pData, size_t size);

#ifdef DO_RENDERSTATS
	void TrackStats(const SGraphicsPipelinePassContext& RESTRICT_REFERENCE passContext, CRenderObject* pRenderObject) const;
#endif

private:
	static CRenderObjectsPools*           s_pPools;
	static CryCriticalSectionNonRecursive m_drawCallInfoLock;
};
