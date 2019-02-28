// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/Scaleform/IFlashPlayer.h>

#ifdef INCLUDE_SCALEFORM_SDK

// Define this to place instances of CFlashPlayer and CFlashVariableObject into GFx' own pool.
// Currently disabled to enable easier tracking of level heap conflicts.
// Note: even with this defined m_filePath and m_lock (and their shared counter objects) still don't go into the GFx mem pool.
//#define USE_GFX_POOL
	#include <CryCore/Platform/CryWindows.h>
	#pragma warning(push)
	#pragma warning(disable : 6326)// Potential comparison of a constant with another constant
	#pragma warning(disable : 6011)// Dereferencing NULL pointer
	#include <GFxPlayer.h>          // includes <windows.h>
	#pragma warning(pop)

	#include <CryRenderer/IScaleform.h>
	#include "SharedResources.h"
	#include "ScaleformRecording.h"
	#include <CryString/CryString.h>

#include <memory>

DECLARE_SHARED_POINTERS(CryCriticalSection);
DECLARE_SHARED_POINTERS(string);

struct ICVar;
	#if defined(ENABLE_FLASH_INFO)
struct SFlashProfilerData;
	#endif

namespace FlashHelpers
{

template<typename T>
class LinkedResourceList
{
public:
	template<typename S>
	struct Node
	{
		Node()
		{
			m_pHandle = 0;
			m_pPrev = this;
			m_pNext = this;
		};

		Node* m_pPrev;
		Node* m_pNext;
		S*    m_pHandle;
	};

	typedef Node<T> NodeType;

public:
	void Link(NodeType& node)
	{
		CryAutoCriticalSection lock(m_lock);

		assert(node.m_pNext == &node && node.m_pPrev == &node);
		node.m_pPrev = m_rootNode.m_pPrev;
		node.m_pNext = &m_rootNode;
		m_rootNode.m_pPrev->m_pNext = &node;
		m_rootNode.m_pPrev = &node;
	}

	void Unlink(NodeType& node)
	{
		CryAutoCriticalSection lock(m_lock);

		assert(node.m_pNext != 0 && node.m_pPrev != 0);
		node.m_pPrev->m_pNext = node.m_pNext;
		node.m_pNext->m_pPrev = node.m_pPrev;
		node.m_pNext = node.m_pPrev = 0;
	}

	NodeType& GetRoot()
	{
		return m_rootNode;
	}

	CryCriticalSection& GetLock()
	{
		return m_lock;
	}

private:
	NodeType           m_rootNode;
	CryCriticalSection m_lock;
};

} // namespace FlashHelpers

class CFlashPlayer : public IFlashPlayer, public IFlashPlayer_RenderProxy, public std::enable_shared_from_this<CFlashPlayer>
{
	friend struct FunctionHandlerAdaptor;

public:

	virtual bool           Load(const char* pFilePath, unsigned int options = DEFAULT, unsigned int cat = eCat_Default);

	virtual void           SetBackgroundColor(const ColorB& color);
	virtual void           SetBackgroundAlpha(float alpha);
	virtual float          GetBackgroundAlpha() const;
	virtual void           SetViewport(int x0, int y0, int width, int height, float aspectRatio = 1.0f);
	virtual void           GetViewport(int& x0, int& y0, int& width, int& height, float& aspectRatio) const;
	virtual void           SetViewScaleMode(EScaleModeType scaleMode);
	virtual EScaleModeType GetViewScaleMode() const;
	virtual void           SetViewAlignment(EAlignType viewAlignment);
	virtual EAlignType     GetViewAlignment() const;
	virtual void           SetScissorRect(int x0, int y0, int width, int height);
	virtual void           GetScissorRect(int& x0, int& y0, int& width, int& height) const;
	virtual void           Advance(float deltaTime);
	virtual void           Render();
	virtual void           SetClearFlags(uint32 clearFlags, ColorF clearColor = Clr_Transparent);
	virtual void           SetCompositingDepth(float depth);
	virtual void           StereoEnforceFixedProjectionDepth(bool enforce);
	virtual void           StereoSetCustomMaxParallax(float maxParallax = -1.0f);
	virtual void           AvoidStencilClear(bool avoid);
	virtual void           EnableMaskedRendering(bool enable);
	virtual void           ExtendCanvasToViewport(bool extend);

	virtual void           Restart();
	virtual bool           IsPaused() const;
	virtual void           Pause(bool pause);
	virtual void           GotoFrame(unsigned int frameNumber);
	virtual bool           GotoLabeledFrame(const char* pLabel, int offset = 0);
	virtual unsigned int   GetCurrentFrame() const;
	virtual bool           HasLooped() const;

	virtual void           SetFSCommandHandler(IFSCommandHandler* pHandler, void* pUserData = 0);
	virtual void           SetExternalInterfaceHandler(IExternalInterfaceHandler* pHandler, void* pUserData = 0);
	virtual void           SendCursorEvent(const SFlashCursorEvent& cursorEvent);
	virtual void           SendKeyEvent(const SFlashKeyEvent& keyEvent);
	virtual void           SendCharEvent(const SFlashCharEvent& charEvent);
	virtual void           SetImeFocus();

	virtual bool           HitTest(float x, float y) const;

	virtual void           SetVisible(bool visible);
	virtual bool           GetVisible() const;

	virtual bool           SetOverrideTexture(const char* pResourceName, ITexture* pTexture, bool resize = true);

	virtual bool           SetVariable(const char* pPathToVar, const SFlashVarValue& value);
	virtual bool           SetVariable(const char* pPathToVar, const IFlashVariableObject* pVarObj);
	virtual bool           GetVariable(const char* pPathToVar, SFlashVarValue& value) const;
	virtual bool           GetVariable(const char* pPathToVar, IFlashVariableObject*& pVarObj) const;
	virtual bool           IsAvailable(const char* pPathToVar) const;
	virtual bool           SetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, const void* pData, unsigned int count);
	virtual unsigned int   GetVariableArraySize(const char* pPathToVar) const;
	virtual bool           GetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, void* pData, unsigned int count) const;
	virtual bool           Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);

	virtual bool           CreateString(const char* pString, IFlashVariableObject*& pVarObj);
	virtual bool           CreateStringW(const wchar_t* pString, IFlashVariableObject*& pVarObj);
	virtual bool           CreateObject(const char* pClassName, const SFlashVarValue* pArgs, unsigned int numArgs, IFlashVariableObject*& pVarObj);
	virtual bool           CreateArray(IFlashVariableObject*& pVarObj);
	virtual bool           CreateFunction(IFlashVariableObject*& pFuncVarObj, IActionScriptFunction* pFunc, void* pUserData = 0);

	virtual unsigned int   GetFrameCount() const;
	virtual float          GetFrameRate() const;
	virtual int            GetWidth() const;
	virtual int            GetHeight() const;
	virtual size_t         GetMetadata(char* pBuff, unsigned int buffSize) const;
	virtual bool           HasMetadata(const char* pTag) const;
	virtual const char*    GetFilePath() const;

	virtual void           ResetDirtyFlags();

	virtual void           ScreenToClient(int& x, int& y) const;
	virtual void           ClientToScreen(int& x, int& y) const;

	#if defined(ENABLE_DYNTEXSRC_PROFILING)
	virtual void LinkDynTextureSource(const struct IDynTextureSource* pDynTexSrc);
	#endif

	IScaleformPlayback* GetPlayback();

	// IFlashPlayer_RenderProxy interface
	virtual void RenderCallback(EFrameType ft);
	virtual void RenderPlaybackLocklessCallback(int cbIdx, EFrameType ft, bool finalPlayback);
	virtual void DummyRenderCallback(EFrameType ft);

public:
	CFlashPlayer();
	virtual ~CFlashPlayer();

	void DelegateFSCommandCallback(const char* pCommand, const char* pArgs);
	void DelegateExternalInterfaceCallback(const char* pMethodName, const GFxValue* pArgs, UInt numArgs);

public:
	#if defined(USE_GFX_POOL)
	GFC_MEMORY_REDEFINE_NEW(CFlashPlayer, GStat_Default_Mem)
	void* operator new(size_t, void* p) throw() { return p; }
	void* operator new[](size_t, void* p) throw() { return p; }
	void  operator delete(void*, void*) throw()   {}
	void  operator delete[](void*, void*) throw() {}
	#endif

public:
	static void                      RenderFlashInfo();
	static void                      SetFlashLoadMovieHandler(IFlashLoadMovieHandler* pHandler);
	static IFlashLoadMovieHandler*   GetFlashLoadMovieHandler();
	static void                      InitCVars();
	static int                       GetWarningLevel();
	static bool                      CheckFileModTimeEnabled();
	static size_t                    GetStaticPoolSize();
	static size_t                    GetAddressSpaceSize();
	static void                      GetFlashProfileResults(float& accumTime);
	static void                      DumpAndFixLeaks();
	static bool                      AllowMeshCacheReset();

	static IFlashPlayerBootStrapper* CreateBootstrapper();

	enum ELogOptions
	{
		LO_LOADING      = 0x01, // log flash loading
		LO_ACTIONSCRIPT = 0x02, // log flash action script execution
		LO_PEAKS        = 0x04, // log high-level flash function calls which cause peaks
	};
	static unsigned int  GetLogOptions();

	static std::shared_ptr<CFlashPlayer> CreateBootstrapped(GFxMovieDef* pMovieDef, unsigned int options, unsigned int cat);

private:
	bool   IsEdgeAaAllowed() const;
	void   UpdateRenderFlags();
	void   UpdateASVerbosity();
	size_t GetCommandBufferSize() const;

	bool   Bootstrap(GFxMovieDef* pMovieDef, unsigned int options, unsigned int cat);
	bool   ConstructInternal(const char* pFilePath, GFxMovieDef* pMovieDef, unsigned int options, unsigned int cat);

private:
	static bool IsFlashEnabled();

private:
	typedef FlashHelpers::LinkedResourceList<CFlashPlayer>           PlayerList;
	typedef FlashHelpers::LinkedResourceList<CFlashPlayer>::NodeType PlayerListNodeType;

	static PlayerList  ms_playerList;

	static PlayerList& GetList()
	{
		return ms_playerList;
	}
	static PlayerListNodeType& GetListRoot()
	{
		return ms_playerList.GetRoot();
	}

private:
	#if defined(ENABLE_FLASH_INFO)
	static ICVar * CV_sys_flash_info_peak_exclude;
	#endif
	static int ms_sys_flash;
	static int ms_sys_flash_edgeaa;
	#if defined(ENABLE_FLASH_INFO)
	//static int ms_sys_flash_info;
	static float                   ms_sys_flash_info_peak_tolerance;
	static float                   ms_sys_flash_info_histo_scale;
	#endif
	static int                     ms_sys_flash_log_options;
	static float                   ms_sys_flash_curve_tess_error;
	static int                     ms_sys_flash_warning_level;
	static int                     ms_sys_flash_static_pool_size;
	static int                     ms_sys_flash_address_space_kb;
	static int                     ms_sys_flash_allow_mesh_cache_reset;
	static int                     ms_sys_flash_reset_mesh_cache;
	static int                     ms_sys_flash_check_filemodtime;

	static int                     ms_sys_flash_mipmaps;

	static IFlashLoadMovieHandler* ms_pLoadMovieHandler;

private:
	struct GRendererCommandBufferProxy
	{
	public:
		static const size_t NumCommandBuffers = 2;

	private:
		struct GRendererCommandDoubleBuffer
		{
		public:
			GRendererCommandDoubleBuffer(GMemoryHeap* pHeap)
			{
				assert(pHeap);
				for (size_t i = 0; i < NumCommandBuffers; ++i)
					new(&((GRendererCommandBuffer*) m_storage)[i])GRendererCommandBuffer(pHeap);
			}

			~GRendererCommandDoubleBuffer()
			{
				for (size_t i = 0; i < NumCommandBuffers; ++i)
					((GRendererCommandBuffer*) m_storage)[i].~GRendererCommandBuffer();
			}

			GRendererCommandBuffer& operator[](size_t i)
			{
				assert(i < NumCommandBuffers);
				return ((GRendererCommandBuffer*) m_storage)[i];
			}

			size_t GetBufferSize() const
			{
				size_t size = 0;
				for (size_t i = 0; i < NumCommandBuffers; ++i)
					size += ((GRendererCommandBuffer*) m_storage)[i].Capacity();
				return size;
			}

		private:
			char m_storage[NumCommandBuffers * sizeof(GRendererCommandBuffer)];
		};

	public:
		GRendererCommandBufferProxy()
			: m_pCmdDB(nullptr)
		{
			ZeroArray(m_storage);
		}

		~GRendererCommandBufferProxy()
		{
			assert(!m_pCmdDB);
		}

		void Release()
		{
			if (m_pCmdDB)
			{
				m_pCmdDB->~GRendererCommandDoubleBuffer();
				m_pCmdDB = 0;
			}
		}

		bool IsInitialized() const
		{
			return m_pCmdDB != 0;
		}

		void Init(GMemoryHeap* pHeap)
		{
			assert(!m_pCmdDB);
			if (!m_pCmdDB)
				m_pCmdDB = new(m_storage) GRendererCommandDoubleBuffer(pHeap);
		}

		size_t GetBufferSize() const
		{
			return m_pCmdDB ? m_pCmdDB->GetBufferSize() : 0;
		}

		GRendererCommandBuffer& operator[](size_t i)
		{
	#if !defined(_RELEASE)
			if (!m_pCmdDB) __debugbreak();
	#endif
			return (*m_pCmdDB)[i];
		}

		const GRendererCommandBuffer& operator[](size_t i) const
		{
	#if !defined(_RELEASE)
			if (!m_pCmdDB) __debugbreak();
	#endif
			return (*m_pCmdDB)[i];
		}

	private:
		GRendererCommandDoubleBuffer* m_pCmdDB;
		char                          m_storage[sizeof(GRendererCommandDoubleBuffer)];
	};

private:
	uint32 m_clearFlags;
	ColorF m_clearColor;
	float m_compDepth;
	float m_stereoCustomMaxParallax;
	bool  m_allowEgdeAA          : 1;
	bool  m_stereoFixedProjDepth : 1;
	bool  m_avoidStencilClear    : 1;
	bool  m_maskedRendering      : 1;
	unsigned char m_memArenaID   : 5;
	bool m_extendCanvasToVP      : 1;
	GFxRenderConfig  m_renderConfig;
	GFxActionControl m_asVerbosity;
	const unsigned int m_frameCount;
	const float m_frameRate;
	const int m_width;
	const int m_height;
	IFSCommandHandler* m_pFSCmdHandler;
	void* m_pFSCmdHandlerUserData;
	IExternalInterfaceHandler* m_pEIHandler;
	void* m_pEIHandlerUserData;
	GPtr<GFxMovieDef>  m_pMovieDef;
	GPtr<GFxMovieView> m_pMovieView;
	GPtr<GFxLoader2> m_pLoader;
	GPtr<IScaleformRecording> m_pRenderer;
	const stringPtr m_filePath;
	PlayerListNodeType m_node;
#if defined(ENABLE_FLASH_INFO)
	mutable SFlashProfilerData* m_pProfilerData;
#endif
	const CryCriticalSectionPtr m_lock;
	mutable GFxValue m_retValRefHolder;
	GRendererCommandBufferProxy m_cmdBuf;
#if defined(ENABLE_DYNTEXSRC_PROFILING)
	const struct IDynTextureSource* m_pDynTexSrc;
#endif
};

#endif // #ifdef INCLUDE_SCALEFORM_SDK
