// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#ifdef CRY_PLATFORM_DURANGO
using d3dShaderHandleType = IGraphicsUnknown;
#else
using d3dShaderHandleType = IUnknown;
#endif

class SD3DShader
{
	friend struct SD3DShaderHandle;

	d3dShaderHandleType* m_pHandle = nullptr;
	EHWShaderClass m_eSHClass;
	std::size_t    m_nSize;

	std::size_t    m_refCount = 0;

	SD3DShader(d3dShaderHandleType *handle, EHWShaderClass eSHClass, std::size_t size) : m_pHandle(handle), m_eSHClass(eSHClass), m_nSize(size) {}

public:
	~SD3DShader() noexcept;
	SD3DShader(const SD3DShader&) = delete;
	SD3DShader &operator=(const SD3DShader&) = delete;
	
	void* GetHandle() const { return m_pHandle; }
	void GetMemoryUsage(ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }

	void AddRef() { ++m_refCount; }
	std::size_t Release()
	{
		const auto referencesLeft = --m_refCount;
		if (referencesLeft == 0)
			delete this;

		return referencesLeft;
	}

	bool m_bDisabled = false;
};

struct SD3DShaderHandle
{
	_smart_ptr<SD3DShader> m_pShader;

	byte*       m_pData = nullptr;
	int         m_nData = 0;
	byte        m_bStatus = 0;

	SD3DShaderHandle() = default;
	SD3DShaderHandle(_smart_ptr<SD3DShader> &&handle) noexcept : m_pShader(std::move(handle)) {}
	SD3DShaderHandle(d3dShaderHandleType *handle, EHWShaderClass eSHClass, std::size_t size) noexcept : m_pShader(new SD3DShader(handle, eSHClass, size)) {}
	SD3DShaderHandle(const SD3DShaderHandle&) = default;
	SD3DShaderHandle(SD3DShaderHandle&&) noexcept = default;
	SD3DShaderHandle& operator=(const SD3DShaderHandle&) = default;
	SD3DShaderHandle& operator=(SD3DShaderHandle&&) noexcept = default;

	void SetFake()
	{
		m_bStatus = 2;
	}
	void SetNonCompilable()
	{
		m_bStatus = 1;
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

const char* GetShaderlistName(uint32 nPlatform);
const char* CurrentPlatformShaderListFile();

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

		std::vector<const SFXTexture*> m_pFXTextures;
		
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
		int                        m_nMaxVecs[CB_NUM];
		short                      m_nInstMatrixID;
		short                      m_nInstIndex;
		short                      m_nInstructions;
		short                      m_nTempRegs;
		uint16                     m_VStreamMask_Stream;
		uint16                     m_VStreamMask_Decl;
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

			m_Shader.m_nDataSize = 0;
			m_Shader.m_pShaderData = nullptr;
		}

		void Release();
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
	int m_nCurInstFrame;

	// Bin FX support
	FXShaderToken  m_TokenTable;
	TArray<uint32> m_TokenData;

	virtual int Size() override
	{
		int nSize = sizeof(*this);
		nSize += sizeOfVP(m_Insts);
		//nSize += sizeOfMapP(m_LookupMap);
		nSize += sizeofVector(m_TokenData);
		nSize += sizeOfV(m_TokenTable);

		return nSize;
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
		//pSizer->AddObject(m_pCurInst); // crahes.., looks like somewhere this ptr is not set back to NULL
		pSizer->AddObject(m_Insts);
		//pSizer->AddObject( m_LookupMap );
		pSizer->AddObject(m_TokenData);
		pSizer->AddObject(m_TokenTable);

		pSizer->AddObject(m_CachedTokens.data(), m_CachedTokens.size());
		pSizer->AddObject(m_pCache);
	}
	CHWShader_D3D()
	{
#if CRY_PLATFORM_DESKTOP
		m_pTree = NULL;
		m_pParser = NULL;
#endif
		mfConstruct();
	}

	static void mfInit();
	void        mfConstruct();

	void mfFree()
	{
#if CRY_PLATFORM_DESKTOP
		SAFE_DELETE(m_pTree);
		SAFE_DELETE(m_pParser);
#endif

		m_Flags = 0;
		mfReset();
	}

	//============================================================================
	// Binary cache support
	SDeviceShaderEntry mfGetCacheItem(CShader* pFX, const char *name, SDiskShaderCache *cache, uint32& nFlags);
	bool         mfAddCacheItem(SDiskShaderCache* pCache, SShaderCacheHeaderItem* pItem, const byte* pData, int nLen, bool bFlush, CCryNameTSCRC Name);

	static byte* mfBindsToCache(SHWSInstance* pInst, std::vector<SCGBind>* Binds, int nParams, byte* pP);
	const  byte* mfBindsFromCache(std::vector<SCGBind>& Binds, int nParams, const byte* pP);

	bool         mfActivateCacheItem(CShader* pSH, const SDeviceShaderEntry* cacheEntry, uint32 nFlags);
	bool         mfCreateCacheItem(SHWSInstance* pInst, CShader *ef, std::vector<SCGBind>& InstBinds, byte* pData, int nLen, bool bShaderThread);

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
	bool        mfUploadHW(SHWSInstance* pInst, const byte* pBuf, uint32 nSize, CShader* pSH, uint32 nFlags);
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

	
	static void   mfSetParameters(SCGParam* pParams, const int nParams, EHWShaderClass eSH, int nMaxRegs, Vec4* pOutBuffer, uint32 outBufferSize, const D3DViewPort* pVP);   // handles all the parameter except PI and SI ones

	//============================================================================

	void mfLostDevice(SHWSInstance* pInst, byte* pBuf, int nSize)
	{
		pInst->m_Handle.SetFake();
		pInst->m_Handle.m_pData = new byte[nSize];
		memcpy(pInst->m_Handle.m_pData, pBuf, nSize);
		pInst->m_Handle.m_nData = nSize;
	}

	bool PrecacheShader(CShader* pSH, const SShaderCombIdent &cache,uint32 nFlags) override;

	int CheckActivation(CShader* pSH, SHWSInstance*& pInst, uint32 nFlags);

	SHWSInstance* mfGetInstance(SShaderCombIdent& Ident, uint32 nFlags);
	SHWSInstance* mfGetInstance(int nHashInstance, SShaderCombIdent& Ident);
	SHWSInstance* mfGetHashInst(InstContainer *pInstCont, uint32 identHash, SShaderCombIdent& Ident, InstContainerIt& it);
	void          mfPrepareShaderDebugInfo(SHWSInstance* pInst, const char* szAsm, std::vector<SCGBind>& InstBindVars, void* pBuffer);
	void          mfGetSrcFileName(char* srcName, int nSize);
	void          mfGetDstFileName(SHWSInstance* pInst, char* dstname, int nSize, byte bType);
	static void   mfGenName(SHWSInstance* pInst, char* dstname, int nSize, byte bType);
	void          CorrectScriptEnums(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, const FXShaderToken& Table);
	bool          ConvertBinScriptToASCII(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, const FXShaderToken& Table, TArray<char>& Scr);
	bool          AddResourceLayoutToScriptHeader(SHWSInstance* pInst, const char* szProfile, const char* pFunCCryName, TArray<char>& Scr);
	void          RemoveUnaffectedParameters_D3D10(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars);
	void          AddResourceLayoutToBinScript(CParserBin& Parser, SHWSInstance* pInst, FXShaderToken* Table);
	bool          mfStoreCacheTokenMap(const FXShaderToken& Table, const TArray<uint32>& pSHData);
	void          mfSetDefaultRT(uint64& nAndMask, uint64& nOrMask);
	bool          AutoGenMultiresGS(TArray<char>& sNewScr, CShader *pSH);

public:
	bool        mfGetCacheTokenMap(FXShaderToken& Table, TArray<uint32>& pSHData);
	bool        mfGenerateScript(CShader* pSH, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, uint32 nFlags, TArray<char>& sNewScr);
	bool        mfActivate(CShader* pSH, uint32 nFlags);

	void        SetTokenFlags(uint32 nToken);
	uint64      CheckToken(uint32 nToken);
	uint64      CheckIfExpr_r(const uint32* pTokens, uint32& nCur, uint32 nSize);
	void        mfConstructFX_Mask_RT(const TArray<uint32>& pSHData);
	void        mfConstructFX(const FXShaderToken& Table, const TArray<uint32>& pSHData);

	static bool mfAddFXSampler(SHWSInstance* pInst, SShaderFXParams& FXParams, SFXSampler* pr, const char* ParamName, SCGBind* pBind, CShader* ef, EHWShaderClass eSHClass);
	static bool mfAddFXTexture(SHWSInstance* pInst, SShaderFXParams& FXParams, SFXTexture* pr, const char* ParamName, SCGBind* pBind, CShader* ef, EHWShaderClass eSHClass);

	static void mfAddFXParameter(SHWSInstance* pInst, SParamsGroup& OutParams, SShaderFXParams& FXParams, SFXParam* pr, const char* ParamName, SCGBind* pBind, CShader* ef, bool bInstParam, EHWShaderClass eSHClass);
	static bool mfAddFXParameter(SHWSInstance* pInst, SParamsGroup& OutParams, SShaderFXParams& FXParams, const char* param, SCGBind* bn, bool bInstParam, EHWShaderClass eSHClass, CShader* pFXShader);
	static void mfGatherFXParameters(SHWSInstance* pInst, std::vector<SCGBind>& BindVars, CHWShader_D3D* pSH, int nFlags, CShader* pFXShader);

	static void mfCreateBinds(std::vector<SCGBind> &binds, const void* pConstantTable, std::size_t nSize);
	static void mfPostVertexFormat(SHWSInstance * pInst, CHWShader_D3D * pHWSH, bool bCol, byte bNormal, bool bTC0, bool bTC1[2], bool bPSize, bool bTangent[2], bool bBitangent[2], bool bHWSkin, bool bSH[2], bool bMorphTarget, bool bMorph);
	void        mfUpdateFXVertexFormat(SHWSInstance* pInst, CShader* pSH);

	void        ModifyLTMask(uint32& nMask);

public:
	virtual ~CHWShader_D3D();

	bool                mfAddEmptyCombination(uint64 nRT, uint64 nGL, uint32 nLT, const SCacheCombination& cmbSaved) override;
	bool                mfStoreEmptyCombination(SEmptyCombination& Comb) override;

	virtual void        mfReset() override;
	virtual const char* mfGetEntryName() override { return m_EntryFunc.c_str(); }
	virtual bool        mfFlushCacheFile() override;
	virtual bool        Export(CShader *pSH, SShaderSerializeContext& SC) override;

	enum class cacheValidationResult { ok, no_lookup, version_mismatch, checksum_mismatch };
	cacheValidationResult mfValidateCache(const SDiskShaderCache &cache);

	bool               mfWarmupCache(CShader* pFX);
	void               mfPrecacheAllCombinations(CShader* pFX, CResFileOpenScope &rfOpenGuard, SDiskShaderCache &cache);
	SDeviceShaderEntry mfShaderEntryFromCache(CShader* pFX, const CDirEntry& de, CResFileOpenScope &rfOpenGuard, SDiskShaderCache &cache);

	// Vertex shader specific functions
	virtual InputLayoutHandle mfVertexFormat(bool& bUseTangents, bool& bUseLM, bool& bUseHWSkin) override;
	static InputLayoutHandle  mfVertexFormat(SHWSInstance* pInst, CHWShader_D3D* pSH, D3DBlob* pBuffer, void* pConstantTable);
	virtual void          mfUpdatePreprocessFlags(SShaderTechnique* pTech) override;

	virtual const char*   mfGetActivatedCombinations(bool bForLevel) override;

	static uint16         GetDeclaredVertexStreamMask(void* pHwInstance)
	{
		CRY_ASSERT(pHwInstance && reinterpret_cast<SHWSInstance*>(pHwInstance)->m_eClass == eHWSC_Vertex);
		return reinterpret_cast<SHWSInstance*>(pHwInstance)->m_VStreamMask_Decl;
	}

	static void ShutDown();

	static Vec4 GetVolumetricFogParams(const CCamera& camera);
	static Vec4 GetVolumetricFogRampParams();
	static Vec4 GetVolumetricFogSunDir(const Vec3& sunDir);
	static void GetFogColorGradientConstants(Vec4& fogColGradColBase, Vec4& fogColGradColDelta);
	static Vec4 GetFogColorGradientRadial(const CCamera& camera);

	// Import/Export
	bool ExportSamplers(SCHWShader& SHW, SShaderSerializeContext& SC);
	bool ExportParams(SCHWShader& SHW, SShaderSerializeContext& SC);

	static int                          s_nActivationFailMask;

	static bool                         s_bInitShaders;

	static int                          s_nResetDeviceFrame;
	static int                          s_nInstFrame;

	static int                          s_nDevicePSDataSize;
	static int                          s_nDeviceVSDataSize;
};

#endif  // __D3DHWSHADER_H__
