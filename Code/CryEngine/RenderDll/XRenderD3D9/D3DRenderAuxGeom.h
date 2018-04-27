// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/Renderer.h"     // CRenderer
#include "../Common/CommonRender.h" // gRenDev
#include "../Common/RenderAuxGeom.h"
#include "../Common/Shaders/Shader.h" // CShader

#include "Common/Include_HLSL_CPP_Shared.h"

#if defined(ENABLE_RENDER_AUX_GEOM)

class CD3D9Renderer;
class ICrySizer;
class CTexture;

//////////////////////////////////////////////////////////////////////////
hlsl_cbuffer_register(AuxGeomConstantBuffer, register (b0), 0)
{
	hlsl_matrix44(matViewProj);
	hlsl_float2(invScreenDim);
};

//////////////////////////////////////////////////////////////////////////
hlsl_cbuffer_register(AuxGeomObjectConstantBuffer, register (b0), 0)
{
	hlsl_matrix44(matViewProj);
	hlsl_float4(auxGeomObjColor);
	hlsl_float2(auxGeomObjShading);
	hlsl_float3(globalLightLocal);
};

template <int BufferSize>
struct TTransientConstantBufferHeap
{
	typedef std::forward_list<CConstantBufferPtr> TransientCBList;

	TransientCBList m_freeList;
	TransientCBList m_useList;
	CryCriticalSectionNonRecursive m_lock;

public:
	~TTransientConstantBufferHeap()
	{
		CRY_ASSERT(m_useList.empty());
	}

	//! Get or Allocates new free usable constant buffer
	CConstantBuffer* GetUsableConstantBuffer()
	{
		CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

		if (m_freeList.begin() == m_freeList.end())
			m_useList.emplace_front(gRenDev->m_DevBufMan.CreateConstantBuffer(BufferSize));
		else
			m_useList.splice_after(m_useList.before_begin(), m_freeList, m_freeList.before_begin());
		return *m_useList.begin();
	}

	void FreeUsedConstantBuffers()
	{
		CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

		m_freeList.splice_after(m_freeList.before_begin(), m_useList);
	}
};

class CRenderAuxGeomD3D;

class CAuxGeomCBCollector
{
public:
	using AUXJobs = std::vector<CAuxGeomCB*>;

private:
	class SThread;
	using AUXThreadMap = std::map<threadID, SThread*>;
	using AUXThreads = std::vector<SThread*>;
	

	class SThread
	{
		friend class CAuxGeomCBCollector;
		using AUXJobMap = std::vector<CAuxGeomCB*>;
		CAuxGeomCB*       m_cbCurrent;
		AUXJobMap         m_auxJobMap;
		CCamera			  m_camera;

		SDisplayContextKey displayContextKey;

		mutable CryRWLock m_rwlLocal;
	public:
		SThread();
		~SThread();
		CAuxGeomCB* Get(int jobID, threadID tid);
		void Add(CAuxGeomCB* newAuxGeomCB);
		//void AppendJob(AUXJobs& auxJobs);
		void FreeMemory();
		void SetDefaultCamera(const CCamera& camera);
		void SetDisplayContextKey(const SDisplayContextKey& displayContextKey);
		void GetMemoryUsage(ICrySizer* pSizer) const;
	};
	AUXThreadMap      m_auxThreadMap;
	mutable CryRWLock m_rwGlobal;
	CCamera           m_camera;

	SDisplayContextKey m_displayContextKey;
public:
	~CAuxGeomCBCollector();
	CAuxGeomCB* Get(int jobID);
	void Add(CAuxGeomCB* newAuxGeomCB);
	void FreeMemory();
	AUXJobs SubmitAuxGeomsAndPrepareForRendering();
	void SetDefaultCamera(const CCamera& camera);
	void SetDisplayContextKey(const SDisplayContextKey& displayContextKey);
	CCamera                 GetCamera() const;
	const SDisplayContextKey& GetDisplayContextKey() const;
	void GetMemoryUsage(ICrySizer* pSizer) const;
};

class CRenderAuxGeomD3D final : public IRenderAuxGeomImpl
{
public:
	virtual void RT_Flush(const SAuxGeomCBRawDataPackagedConst& data) override;
	void RT_Render(const CAuxGeomCBCollector::AUXJobs& auxGeoms);
	void RT_Reset(CAuxGeomCBCollector::AUXJobs& auxGeoms);

private:
	void RenderAuxGeom(const CAuxGeomCB* pAuxGeom);

public:
	static CRenderAuxGeomD3D* Create(CD3D9Renderer& renderer)
	{
		return(new CRenderAuxGeomD3D(renderer));
	}

public:
	~CRenderAuxGeomD3D();

	void        FreeMemory();

	int         GetDeviceDataSize();
	void        ReleaseDeviceObjects();
	HRESULT     RestoreDeviceObjects();
	void        GetMemoryUsage(ICrySizer* pSizer) const;
	void        ReleaseResources();
	void*       operator new(size_t s)
	{
		uint8* p = (uint8*) malloc(s + 16 + 8);
		memset(p, 0, s + 16 + 8);
		uint8* pRet = (uint8*) ((size_t) (p + 16 + 8) & ~0xF);
		((uint8**) pRet)[-1] = p;
		return pRet;
	}

	void operator delete(void* p)
	{
		free(((uint8**)p)[-1]);
	}

private:
	struct SDrawObjMesh
	{
		SDrawObjMesh()
			: m_numVertices(0)
			, m_numFaces(0)
			, m_pVB(~0u)
			, m_pIB(~0u)
		{
		}

		~SDrawObjMesh()
		{
			Release();
		}

		void Release();

		int  GetDeviceDataSize() const;

		uint32          m_numVertices;
		uint32          m_numFaces;
		buffer_handle_t m_pVB;
		buffer_handle_t m_pIB;
		SCompiledRenderPrimitive m_primitive;
	};

	enum EAuxObjNumLOD
	{
		e_auxObjNumLOD = 5
	};

	struct SMatrices
	{
		SMatrices()
			: m_pCurTransMat(nullptr)
		{
			m_matView.SetIdentity();
			m_matViewInv.SetIdentity();
			m_matProj.SetIdentity();
			m_matTrans3D.SetIdentity();

			m_matTrans2D = Matrix44A(2, 0, 0, 0,
			                         0, -2, 0, 0,
			                         0, 0, 0, 0,
			                         -1, 1, 0, 1);
		}

		void UpdateMatrices(const CCamera& camera);

		Matrix44A        m_matView;
		Matrix44A        m_matViewInv;
		Matrix44A        m_matProj;
		Matrix44A        m_matTrans3D;
		Matrix44A        m_matTrans2D;
		const Matrix44A* m_pCurTransMat;
	};

private:
	CRenderAuxGeomD3D(CD3D9Renderer& renderer);

	bool              PreparePass(const CCamera& camera, const SDisplayContextKey& displayContextKey, CPrimitiveRenderPass& pass, SRenderViewport* getViewport);

	CDeviceGraphicsPSOPtr GetGraphicsPSO(const SAuxGeomRenderFlags& flags, const CCryNameTSCRC& techique, ERenderPrimitiveType topology, InputLayoutHandle format);

	void              DrawAuxPrimitives(const CAuxGeomCB::SAuxGeomCBRawData& rawData,CAuxGeomCB::AuxSortedPushBuffer::const_iterator itBegin, CAuxGeomCB::AuxSortedPushBuffer::const_iterator itEnd, const Matrix44& mViewProj, const SRenderViewport& vp, int texID);

	bool              PrepareRendering(const CAuxGeomCB::SAuxGeomCBRawData* pAuxGeomData, SRenderViewport* viewportOut);
	void              Prepare(const SAuxGeomRenderFlags& renderFlags, Matrix44A& mat, const SDisplayContextKey& displayContextKey);
	void              FinishRendering();
	void              ClearCaches();

	template<typename TMeshFunc>
	HRESULT                                  CreateMesh(SDrawObjMesh& mesh, TMeshFunc meshFunc);

	const Matrix44A&                         GetCurrentView() const;
	const Matrix44A&                         GetCurrentViewInv() const;
	const Matrix44A&                         GetCurrentProj() const;
	const Matrix44A&                         GetCurrentTrans3D() const;
	const Matrix44A&                         GetCurrentTrans2D() const;
	const Matrix44A&                         GetCurrentTransUnit() const;

	bool                                     IsOrthoMode() const;
	bool                                     HasWorldMatrix() const;

	const CAuxGeomCB::AuxVertexBuffer&       GetAuxVertexBuffer() const;
	const CAuxGeomCB::AuxIndexBuffer&        GetAuxIndexBuffer() const;
	const CAuxGeomCB::AuxDrawObjParamBuffer& GetAuxDrawObjParamBuffer() const;
	const Matrix44A& GetAuxOrthoMatrix(int idx) const;
	const Matrix34&  GetAuxWorldMatrix(int idx) const;

private:

	class CBufferManager
	{
		buffer_handle_t vbAux = ~0u;
		buffer_handle_t ibAux = ~0u;

		static buffer_handle_t fill(buffer_handle_t buf, BUFFER_BIND_TYPE type, const void* data, size_t size);
		static buffer_handle_t update(BUFFER_BIND_TYPE type, const void* data, size_t size);

	public:
		~CBufferManager();

		void            FillVB(const void* src, size_t size) { vbAux = fill(vbAux, BBT_VERTEX_BUFFER, src, size); }
		void            FillIB(const void* src, size_t size) { ibAux = fill(ibAux, BBT_INDEX_BUFFER, src, size); }

		buffer_handle_t GetVB()                              { return vbAux; }
		buffer_handle_t GetIB()                              { return ibAux; }
	};

	class CAuxPrimitiveHeap
	{
		typedef std::forward_list<SCompiledRenderPrimitive> CompiledRPList;
		CompiledRPList m_freeList;
		CompiledRPList m_useList;

	public:
		SCompiledRenderPrimitive* GetUsablePrimitive();
		void FreeUsedPrimitives();
		void Clear();
	};

	class CAuxPSOCache
	{
		std::unordered_map<CDeviceGraphicsPSODesc,CDeviceGraphicsPSOPtr> m_cache;

	public:
		CDeviceGraphicsPSOPtr GetOrCreatePSO(const CDeviceGraphicsPSODesc &psoDesc);
		void                  Clear();
	};

	class CAuxDeviceResourceSetCacheForTexture : NoCopy
	{
		CDeviceResourceSetPtr m_pDefaultWhite;
		std::unordered_map<int,std::pair<CDeviceResourceSetPtr,_smart_ptr<CTexture>>> m_cache;

	public:
		~CAuxDeviceResourceSetCacheForTexture();

		CDeviceResourceSetPtr GetOrCreateResourceSet(int textureId);
		void                  Clear();

	private:
		static bool OnTextureInvalidated(void* pListener, SResourceBindPoint bindPoint, UResourceReference resource, uint32 invalidationFlags);
	};

	struct SAuxCurrentState
	{
		const CDeviceInputStream*  m_pVertexInputSet = nullptr;
		const CDeviceInputStream*  m_pIndexInputSet = nullptr;
		uint8                      m_stencilRef = 0;
		CDeviceResourceLayoutPtr   m_pResourceLayout_WithTexture;
	};

	CD3D9Renderer&                            m_renderer;

	CBufferManager                            m_bufman;
	CPrimitiveRenderPass                      m_geomPass;
	CPrimitiveRenderPass                      m_textPass;

	SAuxCurrentState                          m_currentState;

	SMatrices                                 m_matrices;

	CAuxGeomCB::EPrimType                     m_curPrimType;

	uint8                                     m_curPointSize;

	int                                       m_curTransMatrixIdx;
	int                                       m_curWorldMatrixIdx;

	CShader*                                  m_pAuxGeomShader;
	EAuxGeomPublicRenderflags_DrawInFrontMode m_curDrawInFrontMode;

	CAuxGeomCB::AuxSortedPushBuffer           m_auxSortedPushBuffer;
	const CAuxGeomCB::SAuxGeomCBRawData*      m_pCurCBRawData;              //!< Helper raw data pointer to currently used data in RT_Flush. 
	                                                                        //!< It is used to reduce data pass

	int                                       CV_r_auxGeom;

	SDrawObjMesh                              m_sphereObj[e_auxObjNumLOD];
	SDrawObjMesh                              m_coneObj[e_auxObjNumLOD];
	SDrawObjMesh                              m_cylinderObj[e_auxObjNumLOD];

	CAuxPSOCache                              m_auxPsoCache;
	CAuxDeviceResourceSetCacheForTexture      m_auxTextureCache;
	CAuxPrimitiveHeap                         m_auxRenderPrimitiveHeap;

	// Buffer Heaps
	TTransientConstantBufferHeap<sizeof(sizeof(HLSL_AuxGeomObjectConstantBuffer))> m_auxConstantBufferHeap;
};

#endif // #if defined(ENABLE_RENDER_AUX_GEOM)
