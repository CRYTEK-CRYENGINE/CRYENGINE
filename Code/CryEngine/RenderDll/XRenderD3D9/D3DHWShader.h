// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*=============================================================================
   D3DCGPShader.h : Direct3D9 CG pixel shaders interface declaration.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#ifndef __D3DHWSHADER_H__
#define __D3DHWSHADER_H__

#include "Common/Shaders/ShaderComponents.h"                 // SCGBind, SCGParam, etc.
#include "DeviceManager/D3D11/DeviceSubmissionQueue_D3D11.h" // CSubmissionQueue_DX11

#if CRY_PLATFORM_ORBIS && defined(USE_SCUE)
//#define USE_PER_FRAME_CONSTANT_BUFFER_UPDATES // TODO: Restore this
#endif

#define MERGE_SHADER_PARAMETERS 1

// Streams redefinitions (TEXCOORD#)
#define VSTR_COLOR1       4   //Base Stream
#define VSTR_NORMAL1      5   //Base Stream
#define VSTR_PSIZE        6   //Base particles stream
//#define VSTR_MORPHTARGETDELTA 7 // MorphTarget stream (VSF_HWSKIN_MORPHTARGET_INFO)
#define VSTR_TANGENT      8   // Tangents stream (VSF_TANGENTS)
#define VSTR_BITANGENT    9   // Tangents stream (VSF_TANGENTS) -> unused, remove
#define VSTR_BLENDWEIGHTS 10  // HWSkin stream (VSF_HWSKIN_INFO)
#define VSTR_BLENDINDICES 11  // HWSkin stream (VSF_HWSKIN_INFO)
#define VSTR_BONESPACE    12  // HWSkin stream (VSF_HWSKIN_INFO)

#define MAX_CONSTANTS_PS  512
#define MAX_CONSTANTS_VS  512
#define MAX_CONSTANTS_GS  512
#define MAX_CONSTANTS_DS  512
#define MAX_CONSTANTS_HS  512
#define MAX_CONSTANTS_CS  512

#define INST_PARAM_SIZE   sizeof(Vec4)

union CRY_ALIGN (16) UFloat4
{
	float       f[4];
	uint        ui[4];
#ifdef CRY_TYPE_SIMD4
	f32v4       v;

	ILINE void Load(const float* src)
	{
		v = *(f32v4*)src;
	}
#else
	ILINE void Load(const float* src)
	{
		f[0] = src[0];
		f[1] = src[1];
		f[2] = src[2];
		f[3] = src[3];
	}
#endif
};

class CConstantBuffer;

//==============================================================================

int D3DXGetSHParamHandle(void* pSH, SCGBind* pParam);

struct SParamsGroup
{
	std::vector<SCGParam> Params[2];
	std::vector<SCGParam> Params_Inst;
};

enum ED3DShError
{
	ED3DShError_NotCompiled,
	ED3DShError_CompilingError,
	ED3DShError_Fake,
	ED3DShError_Ok,
	ED3DShError_Compiling,
};

//====================================================================================

struct SCGParamsGroup
{
	uint16    nParams;
	uint16    nSamplers;
	uint16    nTextures;
	bool      bGeneral; // Indicates that we should use fast parameters handling
	SCGParam* pParams;
	int       nPool;
	int       nRefCounter;
	SCGParamsGroup()
	{
		nParams = 0;
		bGeneral = false;
		nPool = 0;
		pParams = NULL;
		nRefCounter = 1;
	}
	unsigned Size()                                  { return sizeof(*this); }
	void     GetMemoryUsage(ICrySizer* pSizer) const {}
};

#define PARAMS_POOL_SIZE 256

struct SCGParamPool
{
protected:
	PodArray<alloc_info_struct> m_alloc_info;
	Array<SCGParam>             m_Params;

public:

	SCGParamPool(int nEntries = 0);
	~SCGParamPool();
	SCGParamsGroup Alloc(int nEntries);
	bool           Free(SCGParamsGroup& Group);

	size_t         Size()
	{
		return sizeof(*this) + sizeOfV(m_alloc_info) + m_Params.size_mem();
	}
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_alloc_info);
		pSizer->Add(m_Params.begin(), m_Params.size());
	}
};

template<>
inline bool raw_movable(const SCGParamPool&)
{
	return true;
}

class CGParamManager
{
	friend class CHWShader_D3D;
	//friend struct CHWShader_D3D::SHWSInstance;

	static std::vector<uint32, stl::STLGlobalAllocator<uint32>> s_FreeGroups;

public:
	static void          Init();
	static void          Shutdown();

	static SCGParamPool* NewPool(int nEntries);
	static int           GetParametersGroup(SParamsGroup& Gr, int nId);
	static bool          FreeParametersGroup(int nID);

	static std::vector<SCGParamsGroup> s_Groups;
	static DynArray<SCGParamPool>      s_Pools;
};

//=========================================================================================

struct SD3DShader
{
	int   m_nRef;
	void* m_pHandle;
	bool  m_bBound;
	bool  m_bDisabled;

	SD3DShader()
	{
		m_nRef = 1;
		m_pHandle = NULL;
		m_bBound = false;
		m_bDisabled = false;
	}
	int AddRef()
	{
		return m_nRef++;
	}
	int  Release(EHWShaderClass eSHClass, int nSize);

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

struct SD3DShaderHandle
{
	SD3DShader* m_pShader;
	byte*       m_pData;
	int         m_nData;
	byte        m_bStatus;
	SD3DShaderHandle()
	{
		m_pShader = NULL;
		m_bStatus = 0;
		m_nData = 0;
		m_pData = NULL;
	}
	void SetShader(SD3DShader* pShader)
	{
		m_bStatus = 0;
		m_pShader = pShader;
	}
	void SetFake()
	{
		m_bStatus = 2;
	}
	void SetNonCompilable()
	{
		m_bStatus = 1;
	}
	int AddRef()
	{
		if (!m_pShader)
			return 0;
		return m_pShader->AddRef();
	}
	int Release(EHWShaderClass eSHClass, int nSize)
	{
		if (!m_pShader)
			return 0;
		return m_pShader->Release(eSHClass, nSize);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_pShader);
	}
};

struct SShaderAsyncInfo
{
	SShaderAsyncInfo* m_Next;           //!<
	SShaderAsyncInfo* m_Prev;           //!<
	inline void       Unlink()
	{
		if (!m_Next || !m_Prev)
			return;
		m_Next->m_Prev = m_Prev;
		m_Prev->m_Next = m_Next;
		m_Next = m_Prev = NULL;
	}
	inline void Link(SShaderAsyncInfo* Before)
	{
		if (m_Next || m_Prev)
			return;
		m_Next = Before->m_Next;
		Before->m_Next->m_Prev = this;
		Before->m_Next = this;
		m_Prev = Before;
	}
	static void FlushPendingShaders();

#if CRY_PLATFORM_DURANGO //|| CRY_RENDERER_OPENGL
	#define LPD3DXBUFFER    D3DBlob *
	#define ID3DXBuffer     D3DBlob
#endif

	int                  m_nHashInstance;
	uint64               m_RTMask;
	uint32               m_LightMask;
	uint32               m_MDMask;
	uint32               m_MDVMask;
	EHWShaderClass       m_eClass;
	UPipelineState       m_pipelineState;
	class CHWShader_D3D* m_pShader;
	CShader*             m_pFXShader;

	D3DBlob*             m_pDevShader;
	D3DBlob*             m_pErrors;
	void*                m_pConstants;
	string               m_Name;
	string               m_Text;
	string               m_Errors;
	string               m_Profile;
	string               m_RequestLine;
	string               m_shaderList;
	//CShaderThread *m_pThread;
	std::vector<SCGBind> m_InstBindVars;
	byte                 m_bPending;
	bool                 m_bPendedFlush;
	bool                 m_bPendedSamplers;
	bool                 m_bPendedEnv;
	bool                 m_bDeleteAfterRequest;
	float                m_fMinDistance;
	int                  m_nFrame;
	int                  m_nThread;
	int                  m_nCombination;

	SShaderAsyncInfo()
		: m_Next(nullptr)
		, m_Prev(nullptr)
		, m_nHashInstance(-1)
		, m_RTMask(0)
		, m_LightMask(0)
		, m_MDMask(0)
		, m_MDVMask(0)
		, m_eClass(eHWSC_Num)
		, m_pShader(nullptr)
		, m_pFXShader(nullptr)
		, m_pDevShader(nullptr)
		, m_pErrors(nullptr)
		, m_pConstants(nullptr)
		, m_bPending(true) // this flag is now used as an atomic indication that if the async shader has been compiled
		, m_bPendedFlush(false)
		, m_bPendedSamplers(false)
		, m_bPendedEnv(false)
		, m_bDeleteAfterRequest(false)
		, m_fMinDistance(0.0f)
		, m_nFrame(0)
		, m_nThread(0)
		, m_nCombination(-1)
	{
	}

	~SShaderAsyncInfo();
	static volatile int      s_nPendingAsyncShaders;
	static int               s_nPendingAsyncShadersFXC;
	static SShaderAsyncInfo& PendingList();
	static SShaderAsyncInfo& PendingListT();
	static CryEvent          s_RequestEv;
};

#ifdef SHADER_ASYNC_COMPILATION

	#include <CryThreading/IThreadManager.h>
	#define SHADER_THREAD_NAME "ShaderCompile"

class CAsyncShaderTask
{
	friend class CD3D9Renderer; // so it can instantiate us

public:
	CAsyncShaderTask();

	static void InsertPendingShader(SShaderAsyncInfo* pAsync);
	int         GetThread()
	{
		return m_nThread;
	}
	int GetThreadFXC()
	{
		return m_nThreadFXC;
	}
	void SetThread(int nThread)
	{
		m_nThread = nThread;
		m_nThreadFXC = -1;
	}
	void SetThreadFXC(int nThread)
	{
		m_nThreadFXC = nThread;
	}

private:
	void                     FlushPendingShaders();

	static SShaderAsyncInfo& BuildList();
	SShaderAsyncInfo         m_flush_list;
	int                      m_nThread;
	int                      m_nThreadFXC;

	class CShaderThread : public IThread
	{
	public:
		CShaderThread(CAsyncShaderTask* task)
			: m_task(task)
			, m_quit(false)
		{
			CAsyncShaderTask::BuildList().m_Next = &CAsyncShaderTask::BuildList();
			CAsyncShaderTask::BuildList().m_Prev = &CAsyncShaderTask::BuildList();

			task->m_flush_list.m_Next = &task->m_flush_list;
			task->m_flush_list.m_Prev = &task->m_flush_list;

			if (!gEnv->pThreadManager->SpawnThread(this, SHADER_THREAD_NAME))
			{
				CryFatalError("Error spawning \"%s\" thread.", SHADER_THREAD_NAME);
			}
		}

		~CShaderThread()
		{
			m_quit = true;
			gEnv->pThreadManager->JoinThread(this, eJM_Join);
		}

	private:
		// Start accepting work on thread
		virtual void ThreadEntry();

		CAsyncShaderTask* m_task;
		volatile bool     m_quit;
	};

	CShaderThread m_thread;

	bool CompileAsyncShader(SShaderAsyncInfo* pAsync);
	void SubmitAsyncRequestLine(SShaderAsyncInfo* pAsync);
	bool PostCompile(SShaderAsyncInfo* pAsync);
};
#endif

class CHWShader_D3D : public CHWShader
{
	// TODO: remove all of these friends (via public access)
	friend class CCompiledRenderObject;
	friend class CD3D9Renderer;
	friend class CAsyncShaderTask;
	friend class CGParamManager;
	friend struct SShaderAsyncInfo;
	friend class CHWShader;
	friend class CShaderMan;
	friend struct InstContainerByHash;
	friend class CREGeomCache;
	friend struct SRenderStatePassD3D;
	friend struct SDeviceObjectHelpers;
	friend class CDeviceGraphicsCommandList;
	friend class CDeviceGraphicsPSO;
	friend class CDeviceGraphicsPSO_DX11;
	friend class CDeviceGraphicsPSO_DX12;
	friend class CGnmGraphicsPipelineState;
	friend class CDeviceObjectFactory;
	friend struct SInputLayout;
	friend class CDeviceGraphicsPSO_Vulkan;

	SShaderDevCache* m_pDevCache;

#if CRY_PLATFORM_DESKTOP
	SPreprocessTree* m_pTree;
	CParserBin*      m_pParser;
#endif

	struct SHWSInstance
	{
		friend struct SShaderAsyncInfo;

		SShaderBlob                m_Shader;

		SShaderCombIdent           m_Ident;

		SD3DShaderHandle           m_Handle;
		EHWShaderClass             m_eClass;

		int                        m_nParams[2]; // 0: Instance independent; 1: Instance depended
		std::vector<STexSamplerRT> m_pSamplers;
		std::vector<SCGSampler>    m_Samplers;
		std::vector<SCGTexture>    m_Textures;
		std::vector<SCGBind>       m_pBindVars;
		int                        m_nParams_Inst;
		float                      m_fLastAccess;
		int                        m_nUsed;
		int                        m_nUsedFrame;
		int                        m_nFrameSubmit;
		int                        m_nMaxVecs[3];
		short                      m_nInstMatrixID;
		short                      m_nInstIndex;
		short                      m_nInstructions;
		short                      m_nTempRegs;
		uint16                     m_VStreamMask_Stream;
		uint16                     m_VStreamMask_Decl;
		short                      m_nCache;
		short                      m_nParent;
		byte                       m_bDeleted         : 1;
		byte                       m_bHasPMParams     : 1;
		byte                       m_bFallback        : 1;
		byte                       m_bAsyncActivating : 1;
		byte                       m_bHasSendRequest  : 1;
		InputLayoutHandle          m_nVertexFormat;
		byte                       m_nNumInstAttributes;

		int                        m_DeviceObjectID;
		SShaderAsyncInfo*  m_pAsync;

#if CRY_RENDERER_VULKAN
		std::vector<SVertexInputStream>  m_VSInputStreams;
#endif

		SHWSInstance()
			: m_Ident()
			, m_Handle()
			, m_eClass(eHWSC_Num)
			, m_nParams_Inst(-1)
			, m_fLastAccess(0.0f)
			, m_nUsed(0)
			, m_nUsedFrame(0)
			, m_nFrameSubmit(0)
			, m_nInstMatrixID(1)
			, m_nInstIndex(-1)
			, m_nInstructions(0)
			, m_nTempRegs(0)
			, m_VStreamMask_Stream(0)
			, m_VStreamMask_Decl(0)
			, m_nCache(-1)
			, m_nParent(-1)
			, m_bDeleted(false)
			, m_bHasPMParams(false)
			, m_bFallback(false)
			, m_bAsyncActivating(false)
			, m_bHasSendRequest(false)
			, m_nVertexFormat(EDefaultInputLayouts::P3F_C4B_T2F)
			, m_nNumInstAttributes(0)
			, m_DeviceObjectID(-1)
			, m_pAsync(nullptr)
		{
			m_nParams[0] = -1;
			m_nParams[1] = -1;
			m_nMaxVecs[0] = 0;
			m_nMaxVecs[1] = 0;
			m_nMaxVecs[2] = 0;

			m_Shader.m_nDataSize = 0;
			m_Shader.m_pShaderData = nullptr;
		}

		void Release(SShaderDevCache* pCache = NULL, bool bReleaseData = true);
		void GetInstancingAttribInfo(uint8 Attributes[32], int32 & nUsedAttr, int& nInstAttrMask);

		int  Size()
		{
			int nSize = sizeof(*this);
			nSize += sizeOfV(m_pSamplers);
			nSize += sizeOfV(m_pBindVars);

			return nSize;
		}

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(m_Handle);
			pSizer->AddObject(m_pSamplers);
			pSizer->AddObject(m_pBindVars);
			pSizer->AddObject(m_Shader.m_pShaderData, m_Shader.m_nDataSize);
		}

		bool IsAsyncCompiling()
		{
			if (m_pAsync || m_bAsyncActivating)
				return true;

			return false;
		}
	};

	typedef std::vector<SHWSInstance*> InstContainer;
	typedef InstContainer::iterator    InstContainerIt;

public:

	SHWSInstance* m_pCurInst;
	InstContainer m_Insts;

	static int    m_FrameObj;

	// FX support
	//std::vector<STexSamplerFX> m_Samplers;
	//std::vector<SFXParam> m_Params;
	int m_nCurInstFrame;

	// Bin FX support
	FXShaderToken  m_TokenTable;
	TArray<uint32> m_TokenData;

	virtual int Size()
	{
		int nSize = sizeof(*this);
		nSize += sizeOfVP(m_Insts);
		//nSize += sizeOfMapP(m_LookupMap);
		nSize += sizeofVector(m_TokenData);
		nSize += sizeOfV(m_TokenTable);

		return nSize;
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		//pSizer->AddObject(m_pCurInst); // crahes.., looks like somewhere this ptr is not set back to NULL
		pSizer->AddObject(m_pGlobalCache);
		pSizer->AddObject(m_pDevCache);
		pSizer->AddObject(m_Insts);
		//pSizer->AddObject( m_LookupMap );
		pSizer->AddObject(m_TokenData);
		pSizer->AddObject(m_TokenTable);
	}
	CHWShader_D3D()
	{
#if CRY_PLATFORM_DESKTOP
		m_pTree = NULL;
		m_pParser = NULL;
#endif
		mfConstruct();
	}
	static void InitialiseContainers();
	static void mfInit();
	void        mfConstruct()
	{
		if (s_bInitShaders)
		{
			memset(s_CurPSParams, 0, sizeof(Vec4) * MAX_CONSTANTS_PS);
			memset(s_CurVSParams, 0, sizeof(Vec4) * MAX_CONSTANTS_VS);
			InitialiseContainers();
			s_bInitShaders = false;
		}

		m_pCurInst = NULL;
		m_pDevCache = NULL;
		m_nCurInstFrame = 0;

		m_dwShaderType = 0;
	}

	void mfFree(uint32 CRC32)
	{
#if CRY_PLATFORM_DESKTOP
		SAFE_DELETE(m_pTree);
		SAFE_DELETE(m_pParser);
#endif

		m_Flags = 0;
		mfReset(CRC32);
	}

	//============================================================================
	// Binary cache support
	SShaderCacheHeaderItem* mfGetCacheItem(uint32& nFlags, int32& nSize);
	static bool             mfAddCacheItem(SShaderCache* pCache, SShaderCacheHeaderItem* pItem, const byte* pData, int nLen, bool bFlush, CCryNameTSCRC Name);

	bool                    mfCloseCacheFile()
	{
		SAFE_RELEASE(m_pDevCache);
		SAFE_RELEASE(m_pGlobalCache);

		return true;
	}

	static byte* mfBindsToCache(SHWSInstance* pInst, std::vector<SCGBind>* Binds, int nParams, byte* pP);
	byte*        mfBindsFromCache(std::vector<SCGBind>*& Binds, int nParams, byte* pP);

	bool         mfActivateCacheItem(CShader* pSH, SShaderCacheHeaderItem* pItem, uint32 nSize, uint32 nFlags);
	static bool  mfCreateCacheItem(SHWSInstance* pInst, std::vector<SCGBind>& InstBinds, byte* pData, int nLen, CHWShader_D3D* pSH, bool bShaderThread);

	//============================================================================

	int mfGetParams(int Type)
	{
		assert(m_pCurInst);
		return m_pCurInst->m_nParams[Type];
	}

	bool mfSetHWStartProfile(uint32 nFlags);
	//bool mfNextProfile(uint32 nFlags);

	void        mfSaveCGFile(const char* scr, const char* path);
	void        mfOutputCompilerError(string& strErr, const char* szSrc);
	static bool mfCreateShaderEnv(int nThread, SHWSInstance* pInst, D3DBlob* pShader, void*& pConstantTable, D3DBlob*& pErrorMsgs, std::vector<SCGBind>& InstBindVars, CHWShader_D3D* pSH, bool bShaderThread, CShader* pFXShader, int nCombination, const char* src = NULL);
	void        mfPrintCompileInfo(SHWSInstance* pInst);
	bool        mfCompileHLSL_Int(CShader* pSH, char* prog_text, D3DBlob** ppShader, void** ppConstantTable, D3DBlob** ppErrorMsgs, string& strErr, std::vector<SCGBind>& InstBindVars);

	int         mfAsyncCompileReady(SHWSInstance* pInst);
	bool        mfRequestAsync(CShader* pSH, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, const char* prog_text, const char* szProfile, const char* szEntry);

	void        mfSubmitRequestLine(SHWSInstance* pInst, string* pRequestLine = NULL);
	D3DBlob*    mfCompileHLSL(CShader* pSH, char* prog_text, void** ppConstantTable, D3DBlob** ppErrorMsgs, uint32 nFlags, std::vector<SCGBind>& InstBindVars);
	bool        mfUploadHW(SHWSInstance* pInst, byte* pBuf, uint32 nSize, CShader* pSH, uint32 nFlags);
	bool        mfUploadHW(D3DBlob* pShader, SHWSInstance* pInst, CShader* pSH, uint32 nFlags);

	ED3DShError mfIsValid_Int(SHWSInstance*& pInst, bool bFinalise);

	//ILINE most common outcome (avoid LHS on link register 360)
	ILINE ED3DShError mfIsValid(SHWSInstance*& pInst, bool bFinalise)
	{
		if (pInst->m_Handle.m_pShader)
			return ED3DShError_Ok;
		if (pInst->m_bAsyncActivating)
			return ED3DShError_NotCompiled;

		return mfIsValid_Int(pInst, bFinalise);
	}

	ED3DShError mfFallBack(SHWSInstance*& pInst, int nStatus);
	void        mfCommitCombinations(int nFrame, int nFrameDiff);
	void        mfCommitCombination(SHWSInstance* pInst, int nFrame, int nFrameDiff);
	void        mfLogShaderCacheMiss(SHWSInstance* pInst);
	void        mfLogShaderRequest(SHWSInstance* pInst);

	void        mfBindInstance(SHWSInstance* pInst, EHWShaderClass eHWSC)
	{
		m_pCurInst = pInst;
		if (eHWSC == eHWSC_Vertex)
		{
			s_pCurInstVS = pInst;
			if (s_pCurVS != pInst->m_Handle.m_pShader)
			{
				s_pCurVS = pInst->m_Handle.m_pShader;
#ifndef _RELEASE
				gRenDev->m_RP.m_PS[gRenDev->m_RP.m_nProcessThreadID].m_NumVShadChanges++;
#endif
				mfBind();
			}
		}
		else if (eHWSC == eHWSC_Pixel)
		{
			s_pCurInstPS = pInst;
			if (s_pCurPS != pInst->m_Handle.m_pShader)
			{
				s_pCurPS = pInst->m_Handle.m_pShader;
#ifndef _RELEASE
				gRenDev->m_RP.m_PS[gRenDev->m_RP.m_nProcessThreadID].m_NumPShadChanges++;
#endif
				mfBind();
			}
		}
	}

	void mfBind()
	{
		HRESULT hr = S_OK;
		if (mfIsValid(m_pCurInst, true) == ED3DShError_Ok)
		{
			{
				if (gRenDev->m_nFrameSwapID != m_pCurInst->m_nUsedFrame)
				{
					m_pCurInst->m_nUsedFrame = gRenDev->m_nFrameSwapID;
					m_pCurInst->m_nUsed++;
				}
				if (m_eSHClass == eHWSC_Pixel)
					gcpRendD3D->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_PS, (D3DResource*)m_pCurInst->m_Handle.m_pShader->m_pHandle);
				else if (m_eSHClass == eHWSC_Vertex)
					gcpRendD3D->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_VS, (D3DResource*)m_pCurInst->m_Handle.m_pShader->m_pHandle);
				else if (m_eSHClass == eHWSC_Geometry)
					gcpRendD3D->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_GS, (D3DResource*)m_pCurInst->m_Handle.m_pShader->m_pHandle);
				else if (m_eSHClass == eHWSC_Hull)
					gcpRendD3D->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_HS, (D3DResource*)m_pCurInst->m_Handle.m_pShader->m_pHandle);
				else if (m_eSHClass == eHWSC_Domain)
					gcpRendD3D->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_GS, (D3DResource*)m_pCurInst->m_Handle.m_pShader->m_pHandle);
				else if (m_eSHClass == eHWSC_Compute)
					gcpRendD3D->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_CS, (D3DResource*)m_pCurInst->m_Handle.m_pShader->m_pHandle);
			}
		}
		assert(SUCCEEDED(hr));
	}

	static void        mfCommitParamsMaterial();
	static void        mfCommitParamsGlobal();
	static void        mfCommitParams();

	static inline void mfBindGS(SD3DShader* pShader, void* pHandle)
	{
		if (s_pCurGS != pShader)
		{
			s_pCurGS = pShader;
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
			gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_NumGShadChanges++;
#endif
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			gcpRendD3D->GetDeviceContext().GSSetShader((ID3D11GeometryShader*)pHandle, NULL, 0);
#endif
		}
		if (!s_pCurGS)
		{
			s_pCurInstGS = NULL;
			s_nActivationFailMask &= ~(1 << eHWSC_Geometry);
		}
	}
	static inline void mfBindDS(SD3DShader* pShader, void* pHandle)
	{
		if (s_pCurDS != pShader)
		{
			s_pCurDS = pShader;
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
			gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_NumDShadChanges++;
#endif
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			gcpRendD3D->GetDeviceContext().DSSetShader((ID3D11DomainShader*)pHandle, NULL, 0);
#endif
		}
		if (!s_pCurDS)
		{
			s_pCurInstDS = NULL;
			s_nActivationFailMask &= ~(1 << eHWSC_Domain);
		}
	}
	static inline void mfBindHS(SD3DShader* pShader, void* pHandle)
	{
		if (s_pCurHS != pShader)
		{
			s_pCurHS = pShader;
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
			gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_NumHShadChanges++;
#endif
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			gcpRendD3D->GetDeviceContext().HSSetShader((ID3D11HullShader*)pHandle, NULL, 0);
#endif
		}
		if (!s_pCurHS)
		{
			s_pCurInstHS = NULL;
			s_nActivationFailMask &= ~(1 << eHWSC_Hull);
		}
	}
	static inline void mfBindCS(SD3DShader* pShader, void* pHandle)
	{
		if (s_pCurCS != pShader)
		{
			s_pCurCS = pShader;
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
			gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_NumCShadChanges++;
#endif
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			gcpRendD3D->GetDeviceContext().CSSetShader((ID3D11ComputeShader*)pHandle, NULL, 0);
#endif
		}
		if (!s_pCurCS)
		{
			s_pCurInstCS = NULL;
			s_nActivationFailMask &= ~(1 << eHWSC_Compute);
		}
	}

#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
	static inline void mfSetCB(int eClass, int nSlot, D3DBuffer* pBuffer)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	#ifndef _RELEASE
		if (nSlot >= CB_NUM)
			__debugbreak();
	#endif

		s_pCurDevCB[eClass][nSlot] = reinterpret_cast<uint64>(pBuffer);

		switch (eClass)
		{
		case eHWSC_Vertex:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, pBuffer, nSlot);
			break;
		case eHWSC_Pixel:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, pBuffer, nSlot);
			break;
		case eHWSC_Geometry:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_GS, pBuffer, nSlot);
			break;
		case eHWSC_Domain:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_DS, pBuffer, nSlot);
			break;
		case eHWSC_Hull:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_HS, pBuffer, nSlot);
			break;
		case eHWSC_Compute:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_CS, pBuffer, nSlot);
			break;
		}
	}
#endif

	static inline void mfSetCB(int eClass, int nSlot, CConstantBuffer* pBuffer)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

		uint64 code = (pBuffer) ? pBuffer->GetCode() : 0;
#if !CRY_PLATFORM_ORBIS // having this check breaks rendering on orbis, should be correctly fixed as this is a large optimization
		if (s_pCurDevCB[eClass][nSlot] != code)
#endif
		{
			s_pCurDevCB[eClass][nSlot] = code;

			switch (eClass)
			{
			case eHWSC_Vertex:
				gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, pBuffer, nSlot);
				break;
			case eHWSC_Pixel:
				gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, pBuffer, nSlot);
				break;
			case eHWSC_Geometry:
				gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_GS, pBuffer, nSlot);
				break;
			case eHWSC_Domain:
				gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_DS, pBuffer, nSlot);
				break;
			case eHWSC_Hull:
				gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_HS, pBuffer, nSlot);
				break;
			case eHWSC_Compute:
				gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_CS, pBuffer, nSlot);
				break;
			}
		}
	}

	static inline void mfClearCB()
	{
		for (int i = 0; i < (int)eHWSC_Num; ++i)
			for (int j = 0; j < (int)CB_NUM; ++j)
				mfSetCB(i, j, (CConstantBuffer*)NULL);
		gcpRendD3D->m_DevMan.CommitDeviceStates();
	}

	static inline void mfUnbindCB(CConstantBuffer* pCB)
	{
		uint64 code = (pCB) ? pCB->GetCode() : 0;
		bool changed = false;
		if (pCB)
		{
			for (int i = 0; i < (int)eHWSC_Num; ++i)
			{
				for (int j = 0; j < (int)CB_NUM; ++j)
				{
					if (s_pCurDevCB[i][j] == code)
					{
						mfSetCB(i, j, (CConstantBuffer*)NULL);
						changed = true;
					}
				}
			}
		}
		if (changed)
			gcpRendD3D->m_DevMan.CommitDeviceStates();
	}

	static inline void mfFreeCB(int nCBufSlot, EHWShaderClass eSHData)
	{
		if (!s_pDataCB[eSHData][nCBufSlot])
			return;

		assert(s_pCurReqCB[eSHData][nCBufSlot]);

		if ((TRUNCATE_PTR)s_pDataCB[eSHData][nCBufSlot] != 1)
		{
#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
			CDeviceObjectFactory::Unmap(s_pCurReqCB[eSHData][nCBufSlot], 0, 0, nMaxVecs * sizeof(Vec4), D3D11_MAP_WRITE_PER_FRAME);
#else
			s_pCurReqCB[eSHData][nCBufSlot]->EndWrite();
#endif
		}

		s_pDataCB[eSHData][nCBufSlot] = NULL;
	}

	static inline void mfCommitCB(int nCBufSlot, EHWShaderClass eSHData, EHWShaderClass eSH)
	{
		if (!s_pDataCB[eSHData][nCBufSlot])
			return;

		assert(s_pCurReqCB[eSHData][nCBufSlot]);

		if ((INT_PTR)s_pDataCB[eSHData][nCBufSlot] != 1)
		{
#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
			CDeviceObjectFactory::Unmap(s_pCurReqCB[eSHData][nCBufSlot], 0, 0, nMaxVecs * sizeof(Vec4), D3D11_MAP_WRITE_PER_FRAME);
#else
			s_pCurReqCB[eSHData][nCBufSlot]->EndWrite();
#endif
		}

		s_pDataCB[eSHData][nCBufSlot] = NULL;
		mfSetCB(eSH, nCBufSlot, s_pCurReqCB[eSHData][nCBufSlot]);
	}
	static inline void mfSetCBConst(int nReg, int nCBufSlot, EHWShaderClass eSH, const float* fData, const int nVecs, int nMaxVecs)
	{
		EHWShaderClass eSHOrig = eSH;
		assert(nCBufSlot >= 0 || nCBufSlot < CB_NUM);
		assert(s_pCB[eSH][nCBufSlot]);
		//assert(nReg + nVecs <= nMaxVecs);
		if (nReg + nVecs > nMaxVecs)
		{
			iLog->Log("ERROR: Attempt to modify CB: %d outside of the range (%d+%d > %d) (Shader: %s)", nCBufSlot, nReg, nVecs, nMaxVecs, gRenDev->m_RP.m_pShader ? gRenDev->m_RP.m_pShader->GetName() : "Unknown");
			return;
		}
		if (s_pDataCB[eSH][nCBufSlot] && s_nCurMaxVecs[eSH][nCBufSlot] != nMaxVecs)
		{
			mfCommitCB(nCBufSlot, eSH, eSHOrig);
		}
		if (!s_pDataCB[eSH][nCBufSlot])
		{
			s_nCurMaxVecs[eSH][nCBufSlot] = nMaxVecs;

			{
				if (!s_pCB[eSH][nCBufSlot][nMaxVecs] && nMaxVecs)
				{
#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
					GetDeviceObjectFactory().CreateBuffer(nMaxVecs, sizeof(Vec4), CDeviceObjectFactory::HEAP_DYNAMIC | CDeviceObjectFactory::USAGE_CPU_WRITE, CDeviceObjectFactory::BIND_CONSTANT_BUFFER, &s_pCB[eSH][nCBufSlot][nMaxVecs]);
#else
					s_pCB[eSH][nCBufSlot][nMaxVecs] = gcpRendD3D->m_DevBufMan.CreateConstantBufferRaw(nMaxVecs * sizeof(Vec4));
#endif
					if (s_pCB[eSH][nCBufSlot][nMaxVecs] == NULL)
					{
						iLog->Log("ERROR: CBuffer %d Create() failed for shader %s result %d", nCBufSlot, gRenDev->m_RP.m_pShader ? gRenDev->m_RP.m_pShader->GetName() : "Unknown", -1);
#if !defined(_RELEASE)
						__debugbreak();
#endif
						return;
					}
				}

				{
					s_pCurReqCB[eSH][nCBufSlot] = s_pCB[eSH][nCBufSlot][nMaxVecs];
					STALL_PROFILER("set const_buffer");
#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
					s_pDataCB[eSH][nCBufSlot] = (Vec4*)CDeviceObjectFactory::Map(s_pCurReqCB[eSH][nCBufSlot], 0, 0, 0, D3D11_MAP_WRITE_PER_FRAME);
#else
					s_pDataCB[eSH][nCBufSlot] = (Vec4*)s_pCurReqCB[eSH][nCBufSlot]->BeginWrite();
#endif
				}
			}

#ifdef DO_RENDERLOG
			if (CD3D9Renderer::CV_d3d11_CBUpdateStats)
			{
				static unsigned int s_lastFrame(0);
				static unsigned int s_numCalls(0);
				static unsigned int s_minUpdateBytes(0);
				static unsigned int s_maxUpdateBytes(0);
				static unsigned int s_totalUpdateBytes(0);

				unsigned int updateBytes = (unsigned int) (nMaxVecs * sizeof(Vec4));
				unsigned int curFrame = gcpRendD3D->GetFrameID(false);
				if (s_lastFrame != curFrame)
				{
					if (s_lastFrame != 0)
					{
						unsigned int avgUpdateBytes = s_totalUpdateBytes / s_numCalls;
						gEnv->pLog->Log("-------------------------------------------------------");
						gEnv->pLog->Log("CB update statistics for frame %d:", s_lastFrame);
						gEnv->pLog->Log("#UpdateSubresource() = %d calls", s_numCalls);
						gEnv->pLog->Log("SmallestTransfer = %d kb (%d bytes)", (s_minUpdateBytes + 1023) >> 10, s_minUpdateBytes);
						gEnv->pLog->Log("BiggestTransfer = %d kb (%d bytes)", (s_maxUpdateBytes + 1023) >> 10, s_maxUpdateBytes);
						gEnv->pLog->Log("AvgTransfer = %d kb (%d bytes)", (avgUpdateBytes + 1023) >> 10, avgUpdateBytes);
						gEnv->pLog->Log("TotalTransfer = %d kb (%d bytes)", (s_totalUpdateBytes + 1023) >> 10, s_totalUpdateBytes);
					}

					s_lastFrame = curFrame;
					s_numCalls = 1;
					s_minUpdateBytes = updateBytes;
					s_maxUpdateBytes = updateBytes;
					s_totalUpdateBytes = updateBytes;
				}
				else
				{
					++s_numCalls;
					s_minUpdateBytes = min(updateBytes, s_minUpdateBytes);
					s_maxUpdateBytes = max(updateBytes, s_maxUpdateBytes);
					s_totalUpdateBytes += updateBytes;
				}
			}
#endif
		}
		else
		{
			assert(s_nCurMaxVecs[eSH][nCBufSlot] == nMaxVecs);
		}
		const Vec4* vData = (const Vec4*)fData;
#ifdef DO_RENDERLOG
		if (CRenderer::CV_r_log >= 3)
		{
			for (int i = 0; i < nVecs; i++)
			{
				gRenDev->Logv("%d %d - %d: (%.3f, %.3f, %.3f, %.3f)", eSH, nCBufSlot, i + nReg, vData[i][0], vData[i][1], vData[i][2], vData[i][3]);
			}
			gRenDev->Logv("\n");
		}
#endif

#if CRY_PLATFORM_SSE2
		if ((((uintptr_t)vData) & 0xf) == 0u)
		{
			__m128* const __restrict vDst = (__m128*)&s_pDataCB[eSH][nCBufSlot][nReg];
			const __m128* const __restrict vSrc = (const __m128*)vData;
			for (int i = 0; i < nVecs; i++) _mm_stream_ps((float*)&vDst[i], vSrc[i]);
#if !CRY_PLATFORM_ORBIS // SFENCE implied on command-buffer submit
			_mm_sfence();
#endif
		}
		else
#endif
		{
			memcpy(&s_pDataCB[eSH][nCBufSlot][nReg], vData, (size_t)nVecs << 4);
		}
	}
	static inline void mfSetGSConst(int nReg, int nCBufSlot, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetCBConst(nReg, nCBufSlot, eHWSC_Geometry, vData, nParams, nMaxVecs);
	}

	static inline void mfSetPSConst(int nReg, int nCBufSlot, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetCBConst(nReg, nCBufSlot, eHWSC_Pixel, vData, nParams, nMaxVecs);
	}

	static inline void mfSetPSConstA(int nReg, int nCBufSlot, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetCBConst(nReg, nCBufSlot, eHWSC_Pixel, vData, nParams, nMaxVecs);
	}

	static inline void mfSetVSConst(int nReg, int nCBufSlot, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetCBConst(nReg, nCBufSlot, eHWSC_Vertex, vData, nParams, nMaxVecs);
	}

	static inline void mfSetVSConstA(int nReg, int nCBufSlot, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetCBConst(nReg, nCBufSlot, eHWSC_Vertex, vData, nParams, nMaxVecs);
	}
	static inline void mfSetPSConst(int nReg, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetPSConst(nReg, CB_PER_BATCH, vData, nParams, nMaxVecs);
	}
	static inline void mfSetVSConst(int nReg, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetVSConst(nReg, CB_PER_BATCH, vData, nParams, nMaxVecs);
	}

	static inline void mfSetGSConst(int nReg, const float* vData, int nParams, int nMaxVecs = 32)
	{
		mfSetGSConst(nReg, CB_PER_BATCH, vData, nParams, nMaxVecs);
	}

	static inline void mfParameterReg(int nReg, int nCBufSlot, EHWShaderClass eSH, const float* v, int nComps, int nMaxVecs)
	{
		mfSetCBConst(nReg, nCBufSlot, eSH, v, nComps, nMaxVecs);
	}

	static void ILINE mfParameterf(const SCGBind* ParamBind, const float* v, int nComps, EHWShaderClass eSH, int nMaxVecs)
	{
		if (!ParamBind || ParamBind->m_dwBind < 0)
			return;
		int nReg = ParamBind->m_dwBind;
		mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, nComps, nMaxVecs);
	}
	static void ILINE mfParameterfA(const SCGBind* ParamBind, const float* v, int nComps, EHWShaderClass eSH, int nMaxVecs)
	{
		if (!ParamBind || ParamBind->m_dwBind < 0)
			return;
		int nReg = ParamBind->m_dwBind;
		mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, nComps, nMaxVecs);
	}

	static void mfParameterf(const SCGBind* ParamBind, const float* v, EHWShaderClass eSH, int nMaxVecs)
	{
		if (!ParamBind || ParamBind->m_dwBind < 0)
			return;
		int nReg = ParamBind->m_dwBind;
		mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
	}
	static void mfParameterfA(const SCGBind* ParamBind, const float* v, EHWShaderClass eSH, int nMaxVecs)
	{
		if (!ParamBind || ParamBind->m_dwBind < 0)
			return;
		int nReg = ParamBind->m_dwBind;
		mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
	}
	static void mfParameteri(const SCGBind* ParamBind, const float* v, EHWShaderClass eSH, int nMaxVecs)
	{
		if (!ParamBind)
			return;
		int nReg = ParamBind->m_dwBind;
		mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
	}
	static void mfParameterb(const SCGBind* ParamBind, const float* v, EHWShaderClass eSH, int nMaxVecs)
	{
		if (!ParamBind)
			return;

		assert(ParamBind->m_dwBind >= 0 && ParamBind->m_nParameters >= 1);
		if ((int)ParamBind->m_dwBind < 0)
			return;

		int nReg = ParamBind->m_dwBind;
		mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
	}

	SCGBind* mfGetParameterBind(const CCryNameR& Name)
	{
		if (!m_pCurInst)
			return NULL;

		std::vector<SCGBind>& pBinds = m_pCurInst->m_pBindVars;
		//if (!pBinds)
		//  return NULL;
		int i;
		int nSize = pBinds.size();
		for (i = 0; i < nSize; i++)
		{
			if (Name == pBinds[i].m_Name)
				return &pBinds[i];
		}
		return NULL;
	}
	void mfParameterf(const CCryNameR& Name, const float* v)
	{
		SCGBind* pBind = mfGetParameterBind(Name);
		if (pBind)
		{
			mfParameterf(pBind, v, m_eSHClass, m_pCurInst->m_nMaxVecs[CB_PER_BATCH]);
		}
	}

	static float* mfSetParametersPI(SCGParam* pParams, const int nParams, float* pDst, EHWShaderClass eSH, int nMaxRegs);                                     // handles only PI and SI parameters
	static void   mfSetParameters(SCGParam* pParams, const int nParams, EHWShaderClass eSH, int nMaxRegs, Vec4* pOutBuffer = 0, uint32 outBufferSize = 0);   // handles all the parameter except PI and SI ones
	static void   mfSetGeneralParametersPI(SCGParam* pParams, const int nParams, EHWShaderClass eSH, int nMaxRegs);                                           // handles only PI and SI parameters
	static void   mfSetGeneralParameters(SCGParam* pParams, const int nParams, EHWShaderClass eSH, int nMaxRegs);                                             // handles all the parameter except PI and SI ones

	//============================================================================

	void mfLostDevice(SHWSInstance* pInst, byte* pBuf, int nSize)
	{
		pInst->m_Handle.SetFake();
		pInst->m_Handle.m_pData = new byte[nSize];
		memcpy(pInst->m_Handle.m_pData, pBuf, nSize);
		pInst->m_Handle.m_nData = nSize;
	}

	ILINE int mfCheckActivation(CShader* pSH, SHWSInstance*& pInst, uint32 nFlags)
	{
		ED3DShError eError = mfIsValid(pInst, true);
		if (eError == ED3DShError_NotCompiled)
		{
			if (!mfActivate(pSH, nFlags))
			{
				pInst = m_pCurInst;
				if (!pInst->IsAsyncCompiling())
					pInst->m_Handle.SetNonCompilable();
				else
				{
					eError = mfIsValid(pInst, true);
					if (eError == ED3DShError_CompilingError || pInst->m_bAsyncActivating)
						return 0;
					if (m_eSHClass == eHWSC_Vertex)
						return 1;
					else
						return -1;
				}
				return 0;
			}
			if (gRenDev->m_RP.m_pCurTechnique)
				mfUpdatePreprocessFlags(gRenDev->m_RP.m_pCurTechnique);
			pInst = m_pCurInst;
		}
		else if (eError == ED3DShError_Fake)
		{
			if (pInst->m_Handle.m_pData)
			{
				if (gRenDev && !gRenDev->CheckDeviceLost())
				{
					mfUploadHW(pInst, pInst->m_Handle.m_pData, pInst->m_Handle.m_nData, pSH, nFlags);
					SAFE_DELETE_ARRAY(pInst->m_Handle.m_pData);
					pInst->m_Handle.m_nData = 0;
				}
				else
					eError = ED3DShError_CompilingError;
			}
		}
		if (eError == ED3DShError_CompilingError)
			return 0;
		return 1;
	}

	void mfSetForOverdraw(SHWSInstance* pInst, uint32 nFlags, uint64& RTMask);

	void mfSetParametersPI(CRenderObject* pObj, CShader* pFXShader)
	{
		if (!m_pCurInst)
			return;
		SHWSInstance* pInst = m_pCurInst;
		if (pInst->m_nParams[1] >= 0)
		{
			SCGParamsGroup& Group = CGParamManager::s_Groups[pInst->m_nParams[1]];
			if (Group.bGeneral)
				mfSetGeneralParametersPI(Group.pParams, Group.nParams, m_eSHClass, pInst->m_nMaxVecs[1]);
			else
				mfSetParametersPI(Group.pParams, Group.nParams, NULL, m_eSHClass, pInst->m_nMaxVecs[1]);
		}
	}
	inline void mfSetParametersPB()
	{
		if (!m_pCurInst)
			return;
		SHWSInstance* pInst = m_pCurInst;
		if (pInst->m_nParams[0] >= 0)
		{
			SCGParamsGroup& Group = CGParamManager::s_Groups[pInst->m_nParams[0]];
			if (Group.bGeneral)
				mfSetGeneralParameters(Group.pParams, Group.nParams, m_eSHClass, pInst->m_nMaxVecs[0]);
			else
				mfSetParameters(Group.pParams, Group.nParams, m_eSHClass, pInst->m_nMaxVecs[0]);
		}
	}
	inline void mfSetSamplers(const std::vector<STexSamplerRT>& Samplers, EHWShaderClass eHWClass)
	{
		mfSetSamplers_Old(Samplers, eHWClass);
		if (!m_pCurInst)
			return;
		SHWSInstance* pInst = m_pCurInst;
		mfSetSamplers(pInst->m_Samplers, eHWClass);
		mfSetTextures(pInst->m_Textures, eHWClass);
	}
#if !defined(_RELEASE)
	static void   LogSamplerTextureMismatch(const CTexture* pTex, const STexSamplerRT* pSampler, EHWShaderClass shaderClass, const char* pMaterialName);
#endif
	static bool   mfSetTextures(const std::vector<SCGTexture>& Textures, EHWShaderClass eHWClass);
	static bool   mfSetSamplers(const std::vector<SCGSampler>& Samplers, EHWShaderClass eHWClass);
	static bool   mfSetSamplers_Old(const std::vector<STexSamplerRT>& Samplers, EHWShaderClass eHWClass);
	SHWSInstance* mfGetInstance(CShader* pSH, SShaderCombIdent& Ident, uint32 nFlags);
	SHWSInstance* mfGetInstance(CShader* pSH, int nHashInstance, SShaderCombIdent& Ident);
	SHWSInstance* mfGetHashInst(InstContainer *pInstCont, uint32 identHash, SShaderCombIdent& Ident, InstContainerIt& it);
	static void   mfPrepareShaderDebugInfo(SHWSInstance* pInst, CHWShader_D3D* pSH, const char* szAsm, std::vector<SCGBind>& InstBindVars, void* pBuffer);
	void          mfGetSrcFileName(char* srcName, int nSize);
	static void   mfGetDstFileName(SHWSInstance* pInst, CHWShader_D3D* pSH, char* dstname, int nSize, byte bType);
	static void   mfGenName(SHWSInstance* pInst, char* dstname, int nSize, byte bType);
	void          CorrectScriptEnums(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, FXShaderToken* Table);
	bool          ConvertBinScriptToASCII(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, FXShaderToken* Table, TArray<char>& Scr);
	bool          AddResourceLayoutToScript(SHWSInstance* pInst, const char* szProfile, const char* pFunCCryName, TArray<char>& Scr);
	void          RemoveUnaffectedParameters_D3D10(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars);
	bool          mfStoreCacheTokenMap(FXShaderToken*& Table, TArray<uint32>*& pSHData, const char* szName);
	void          mfGetTokenMap(CResFile* pRes, SDirEntry* pDE, FXShaderToken*& Table, TArray<uint32>*& pSHData);
	void          mfSetDefaultRT(uint64& nAndMask, uint64& nOrMask);
	bool          AutoGenMultiresGS(TArray<char>& sNewScr, CShader *pSH);

public:
	bool        mfGetCacheTokenMap(FXShaderToken*& Table, TArray<uint32>*& pSHData, uint64 nMaskGen);
	bool        mfGenerateScript(CShader* pSH, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, uint32 nFlags, FXShaderToken* Table, TArray<uint32>* pSHData, TArray<char>& sNewScr);
	bool        mfActivate(CShader* pSH, uint32 nFlags, FXShaderToken* Table = NULL, TArray<uint32>* pSHData = NULL);

	void        SetTokenFlags(uint32 nToken);
	uint64      CheckToken(uint32 nToken);
	uint64      CheckIfExpr_r(uint32* pTokens, uint32& nCur, uint32 nSize);
	void        mfConstructFX_Mask_RT(FXShaderToken* Table, TArray<uint32>* pSHData);
	void        mfConstructFX(FXShaderToken* Table, TArray<uint32>* pSHData);

	static bool mfAddFXSampler(SHWSInstance* pInst, SShaderFXParams& FXParams, SFXSampler* pr, const char* ParamName, SCGBind* pBind, CShader* ef, EHWShaderClass eSHClass);
	static bool mfAddFXTexture(SHWSInstance* pInst, SShaderFXParams& FXParams, SFXTexture* pr, const char* ParamName, SCGBind* pBind, CShader* ef, EHWShaderClass eSHClass);

	static void mfAddFXParameter(SHWSInstance* pInst, SParamsGroup& OutParams, SShaderFXParams& FXParams, SFXParam* pr, const char* ParamName, SCGBind* pBind, CShader* ef, bool bInstParam, EHWShaderClass eSHClass);
	static bool mfAddFXParameter(SHWSInstance* pInst, SParamsGroup& OutParams, SShaderFXParams& FXParams, const char* param, SCGBind* bn, bool bInstParam, EHWShaderClass eSHClass, CShader* pFXShader);
	static void mfGatherFXParameters(SHWSInstance* pInst, std::vector<SCGBind>* BindVars, std::vector<SCGBind>* InstBindVars, CHWShader_D3D* pSH, int nFlags, CShader* pFXShader);

	static void mfCreateBinds(SHWSInstance* pInst, void* pConstantTable, byte* pShader, int nSize);
	bool        mfUpdateSamplers(CShader* pSH);
	static void mfPostVertexFormat(SHWSInstance * pInst, CHWShader_D3D * pHWSH, bool bCol, byte bNormal, bool bTC0, bool bTC1[2], bool bPSize, bool bTangent[2], bool bBitangent[2], bool bHWSkin, bool bSH[2], bool bMorphTarget, bool bMorph);
	void        mfUpdateFXVertexFormat(SHWSInstance* pInst, CShader* pSH);

	/*EHWSProfile mfGetCurrentProfile()
	   {
	   return m_pCurInst->m_eProfileType;
	   }*/
	void ModifyLTMask(uint32& nMask);

public:
	virtual ~CHWShader_D3D();
	bool mfSetVS(int nFlags = 0);
	bool mfSetPS(int nFlags = 0);
	bool mfSetGS(int nFlags = 0);
	bool mfSetHS(int nFlags = 0);
	bool mfSetCS(int nFlags = 0);
	bool mfSetDS(int nFlags = 0);
	bool mfSet(int nFlags = 0)
	{
		if (m_eSHClass == eHWSC_Vertex)
			return mfSetVS(nFlags);
		else if (m_eSHClass == eHWSC_Pixel)
			return mfSetPS(nFlags);
		else if (m_eSHClass == eHWSC_Geometry)
			return mfSetGS(nFlags);
		else if (m_eSHClass == eHWSC_Compute)
			return mfSetCS(nFlags);
		else if (m_eSHClass == eHWSC_Hull)
			return mfSetHS(nFlags);
		else if (m_eSHClass == eHWSC_Domain)
			return mfSetDS(nFlags);
		return false;
	}
	virtual bool        mfAddEmptyCombination(CShader* pSH, uint64 nRT, uint64 nGL, uint32 nLT);
	virtual bool        mfStoreEmptyCombination(SEmptyCombination& Comb);
	virtual bool        mfSetV(int nFlags = 0) { return mfSet(nFlags); };
	virtual void        mfReset(uint32 CRC32);
	virtual const char* mfGetEntryName()       { return m_EntryFunc.c_str(); }
	virtual bool        mfFlushCacheFile();
	virtual bool        Export(SShaderSerializeContext& SC);
	virtual bool        mfPrecache(SShaderCombination& cmb, bool bForce, bool bFallback, CShader* pSH, CShaderResources* pRes);

	// Vertex shader specific functions
	virtual InputLayoutHandle mfVertexFormat(bool& bUseTangents, bool& bUseLM, bool& bUseHWSkin);
	static InputLayoutHandle  mfVertexFormat(SHWSInstance* pInst, CHWShader_D3D* pSH, D3DBlob* pBuffer, void* pConstantTable);
	virtual void          mfUpdatePreprocessFlags(SShaderTechnique* pTech);

	virtual const char*   mfGetActivatedCombinations(bool bForLevel);

	static void           mfSetGlobalParams();
	static void           mfSetCameraParams();
	static void           mfSetCM();
	static void           mfSetPV(const D3DViewPort* pCustomViewport = nullptr);
	static bool           mfAddGlobalTexture(SCGTexture& Texture);
	static bool           mfAddGlobalSampler(STexSamplerRT& Sampler);

	static uint16         GetDeclaredVertexStreamMask(void* pHwInstance)
	{
		CRY_ASSERT(pHwInstance && reinterpret_cast<SHWSInstance*>(pHwInstance)->m_eClass == eHWSC_Vertex);
		return reinterpret_cast<SHWSInstance*>(pHwInstance)->m_VStreamMask_Decl;
	}

	static void ShutDown();

	static Vec4 GetVolumetricFogParams(const CRenderCamera& rcam);
	static Vec4 GetVolumetricFogRampParams();
	static Vec4 GetVolumetricFogSunDir(const Vec3& sunDir);
	static void GetFogColorGradientConstants(Vec4& fogColGradColBase, Vec4& fogColGradColDelta);
	static Vec4 GetFogColorGradientRadial(const CRenderCamera& rcam);

	// Import/Export
	bool ExportSamplers(SCHWShader& SHW, SShaderSerializeContext& SC);
	bool ExportParams(SCHWShader& SHW, SShaderSerializeContext& SC);

	static CRY_ALIGN(16) Vec4 s_CurPSParams[];
	static CRY_ALIGN(16) Vec4 s_CurVSParams[];

#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
	static D3DBuffer**                  s_pCB[eHWSC_Num][CB_NUM];
	static D3DBuffer*                   s_pCurReqCB[eHWSC_Num][CB_NUM];
#else
	static CConstantBuffer**            s_pCB[eHWSC_Num][CB_NUM];
	static CConstantBuffer*             s_pCurReqCB[eHWSC_Num][CB_NUM];
#endif
	static uint64                       s_pCurDevCB[eHWSC_Num][CB_NUM];
	static Vec4*                        s_pDataCB[eHWSC_Num][CB_NUM];
	static int                          s_nCurMaxVecs[eHWSC_Num][CB_NUM];
	static int                          s_nMax_PF_Vecs[eHWSC_Num];
	static int                          s_nMax_SG_Vecs[eHWSC_Num];
	static int                          s_nMax_CM_Vecs[eHWSC_Num];

	static CHWShader_D3D::SHWSInstance* s_pCurInstGS;
	static bool                         s_bFirstGS;
	static CHWShader_D3D::SHWSInstance* s_pCurInstDS;
	static bool                         s_bFirstDS;
	static CHWShader_D3D::SHWSInstance* s_pCurInstHS;
	static bool                         s_bFirstHS;
	static CHWShader_D3D::SHWSInstance* s_pCurInstCS;
	static bool                         s_bFirstCS;
	static CHWShader_D3D::SHWSInstance* s_pCurInstVS;
	static bool                         s_bFirstVS;
	static CHWShader_D3D::SHWSInstance* s_pCurInstPS;
	static bool                         s_bFirstPS;

	static int                          s_nActivationFailMask;

	static int                          s_PSParamsToCommit[];
	static int                          s_NumPSParamsToCommit;
	static int                          s_VSParamsToCommit[];
	static int                          s_NumVSParamsToCommit;

	static bool                         s_bInitShaders;

	static int                          s_nResetDeviceFrame;
	static int                          s_nInstFrame;

	static int                          s_nDevicePSDataSize;
	static int                          s_nDeviceVSDataSize;

	static std::vector<SCGParam>        s_CM_Params[eHWSC_Num]; // Per-frame parameters

	static std::vector<SCGTexture>      s_PF_Textures;   // Per-frame textures
	static std::vector<STexSamplerRT>   s_PF_Samplers;   // Per-frame samplers

	friend struct SShaderTechniqueStat;
};

struct SShaderTechniqueStat
{
	SShaderTechnique*            pTech;
	CShader*                     pShader;
	CHWShader_D3D*               pVS;
	CHWShader_D3D*               pPS;
	CHWShader_D3D::SHWSInstance* pVSInst;
	CHWShader_D3D::SHWSInstance* pPSInst;
};

extern std::vector<SShaderTechniqueStat> g_SelectedTechs;

#endif  // __D3DHWSHADER_H__
