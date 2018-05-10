// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLContext.hpp
//  Version:     v1.00
//  Created:     03/05/2013 by Valerio Guagliumi.
//  Description: Declaration of the type CDevice and the functions to
//               initialize OpenGL contexts and detect hardware
//               capabilities.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLDEVICE__
#define __GLDEVICE__

#include "GLCommon.hpp"
#include "GLContext.hpp"

namespace NCryOpenGL
{

// Optional device context features
enum EFeature
{
	eF_ComputeShader,
	eF_NUM
};

struct SResourceUnitCapabilities
{
	GLint m_aiMaxTotal;
	GLint m_aiMaxPerStage[eST_NUM];
};

// Hardware capabilities of a device context
struct SCapabilities
{
	GLint                     m_iMaxSamples;
	GLint                     m_iMaxVertexAttribs;

	SResourceUnitCapabilities m_akResourceUnits[eRUT_NUM];

	GLint                     m_iUniformBufferOffsetAlignment;
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	GLint                     m_iShaderStorageBufferOffsetAlignment;
#endif

	// vertex attrib binding
	GLint m_iMaxVertexAttribBindings;
	GLint m_iMaxVertexAttribRelativeOffset;

	// The supported usage for each GI format (union of D3D11_FORMAT_SUPPORT flags)
	uint32 m_auFormatSupport[eGIF_NUM];

#if DXGL_SUPPORT_COPY_IMAGE
	// Some drivers implementation of glCopyImageSubData does not work on cube map faces as specified by the standard
	bool m_bCopyImageWorksOnCubeMapFaces;
#endif
};

struct SVersion
{
	int32 m_uMajorVersion;
	int32 m_uMinorVersion;
};

struct SPixelFormatSpec
{
	const SUncompressedLayout* m_pLayout;
	uint32                     m_uNumSamples;
	bool                       m_bSRGB;
};

struct SFrameBufferSpec : SPixelFormatSpec
{
	uint32 m_uWidth;
	uint32 m_uHeight;
};

typedef SBitMask<eF_NUM, SUnsafeBitMaskWord> TFeatures;

struct SFeatureSpec
{
	TFeatures m_kFeatures;
	SVersion  m_kVersion;
};

struct SDisplayMode
{
	uint32    m_uWidth;
	uint32    m_uHeight;
	uint32    m_uFrequency;
#if defined(USE_SDL2_VIDEO)
	EGIFormat m_ePixelFormat;
#elif CRY_PLATFORM_WINDOWS
	uint32    m_uBitsPerPixel;
#endif
};

struct SResourceUnitPartitionBound
{
	uint32 m_uFirstUnit; // Lowest unit index used
	uint32 m_uNumUnits;  // Number of contiguous unit indices used
};

typedef SResourceUnitPartitionBound TPipelineResourceUnitPartitionBound[eST_NUM];

#if defined(USE_SDL2_VIDEO) && defined(DXGL_SINGLEWINDOW)

struct SMainWindow
{
	SDL_Window*        m_pSDLWindow;
	uint32             m_uWidth;
	uint32             m_uHeight;
	string             m_strTitle;
	bool               m_bFullScreen;

	static SMainWindow ms_kInstance;
};

#endif

#if DXGL_USE_ES_EMULATOR

DXGL_DECLARE_REF_COUNTED(struct, SDisplayConnection)
{
	EGLDisplay m_kDisplay;
	EGLSurface m_kSurface;
	EGLConfig m_kConfig;
};

#endif //DXGL_USE_ES_EMULATOR

DXGL_DECLARE_REF_COUNTED(struct, SOutput)
{
	string m_strDeviceID;
	string m_strDeviceName;
	std::vector<SDisplayMode> m_kModes;
	SDisplayMode m_kDesktopMode;
};

enum EDriverVendor
{
	eDV_NVIDIA,
	eDV_NOUVEAU,
	eDV_AMD,
	eDV_ATI,
	eDV_INTEL,
	eDV_INTEL_OS,
	eDV_UNKNOWN
};

DXGL_DECLARE_REF_COUNTED(struct, SAdapter)
{
	string m_strRenderer;
	string m_strVendor;
	string m_strVersion;

	SCapabilities m_kCapabilities;
	std::vector<SOutputPtr> m_kOutputs;

	TFeatures m_kFeatures;
	size_t m_uVRAMBytes;
	EDriverVendor m_eDriverVendor;
};

#if DXGL_FULL_EMULATION
struct SDummyWindow;
#endif //DXGL_FULL_EMULATION

#if DXGL_USE_ES_EMULATOR
typedef EGLNativeDisplayType TNativeDisplay;
#elif defined(USE_SDL2_VIDEO)
typedef TWindowContext*      TNativeDisplay;
#else
typedef TWindowContext       TNativeDisplay;
#endif

DXGL_DECLARE_REF_COUNTED(class, CDevice)
{
public:

	CDevice(SAdapter * pAdapter, const SFeatureSpec &kFeatureSpec, const SPixelFormatSpec &kPixelFormatSpec);
	~CDevice();

#if !DXGL_FULL_EMULATION
	static void Configure(uint32 uNumSharedContexts);
#endif //!DXGL_FULL_EMULATION
#if defined(USE_SDL2_VIDEO)
	static bool CreateSDLWindow(const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen, HWND * pHandle);
	static void DestroySDLWindow(HWND kHandle);
#endif //defined(USE_SDL2_VIDEO)

	bool Initialize(const TNativeDisplay &kDefaultNativeDisplay);
	void Shutdown();
	bool Present(const TWindowContext &kTargetWindowContext);

#if !OGL_SINGLE_CONTEXT
	CContext* ReserveContext();
	void ReleaseContext();
#endif
	CContext* AllocateContext();
	void FreeContext(CContext * pContext);
	void BindContext(CContext * pContext);
	void UnbindContext(CContext * pContext);
	void SetCurrentContext(CContext * pContext);
	CContext* GetCurrentContext();
	uint32 GetMaxContextCount();

	void IssueFrameFences();
#if OGL_SINGLE_CONTEXT
	bool FlushFrameFence(uint32 uContext) { return Exchange(&m_bContextFenceIssued, 0) == 1; }
#else
	bool FlushFrameFence(uint32 uContext) { return m_kContextFenceIssued.Set(uContext, false); }
#endif

	SResourceNamePool&     GetTextureNamePool()                                          { return m_kTextureNamePool; }
	SResourceNamePool&     GetBufferNamePool()                                           { return m_kBufferNamePool; }
	SResourceNamePool&     GetFrameBufferNamePool()                                      { return m_kFrameBufferNamePool; }

	const SIndexPartition& GetResourceUnitPartition(EResourceUnitType eType, uint32 uID) { return m_kResourceUnitPartitions[eType][uID]; }
	uint32                 GetNumResourceUnitPartitions(EResourceUnitType eType)         { return (uint32)m_kResourceUnitPartitions[eType].size(); }

	bool SetFullScreenState(const SFrameBufferSpec &kFrameBufferSpec, bool bFullScreen, SOutput * pOutput);
	bool ResizeTarget(const SDisplayMode &kTargetMode);
	void SetBackBufferTexture(SDefaultFrameBufferTexture * pBackTexture);
	SAdapter*               GetAdapter()              { return m_spAdapter; }
	const TWindowContext&   GetDefaultWindowContext() { return m_kDefaultWindowContext; }
	const SFeatureSpec&     GetFeatureSpec()          { return m_kFeatureSpec; }
	const SPixelFormatSpec& GetPixelFormatSpec()      { return m_kPixelFormatSpec; }
	static CDevice*         GetCurrentDevice()        { return ms_pCurrentDevice; }

protected:

	typedef std::vector<CContext*>       TContexts;
	typedef std::vector<SIndexPartition> TPartitions;

	void InitializeResourceUnitPartitions();

	void PartitionResourceIndices(
	  EResourceUnitType eUnitType,
	  const TPipelineResourceUnitPartitionBound * akPartitionBounds,
	  uint32 uNumPartitions);

	static bool CreateRenderingContexts(
	  TWindowContext & kWindowContext,
	  std::vector<TRenderingContext> &kRenderingContexts,
	  const SFeatureSpec &kFeatureSpec,
	  const SPixelFormatSpec &kPixelFormat,
	  const TNativeDisplay &kNativeDisplay);

	static bool MakeCurrent(const TWindowContext &kWindowContext, TRenderingContext kRenderingContext);

	static uint32 ms_uNumContextsPerDevice;
	static CDevice* ms_pCurrentDevice;

	SOutputPtr m_spFullScreenOutput;
	SAdapterPtr m_spAdapter;
	SFeatureSpec m_kFeatureSpec;
	SPixelFormatSpec m_kPixelFormatSpec;
	TWindowContext m_kDefaultWindowContext;
	TNativeDisplay m_kDefaultNativeDisplay;
#if DXGL_FULL_EMULATION
	SDummyWindow* m_pDummyWindow;
#endif //DXGL_FULL_EMULATION
	TContexts m_kContexts;
	SList m_kFreeContexts;
	void* m_pCurrentContextTLS;

#if OGL_SINGLE_CONTEXT
	volatile LONG m_bContextFenceIssued;
#else
	SBitMask<MAX_NUM_CONTEXT_PER_DEVICE, SSpinlockBitMaskWord> m_kContextFenceIssued;
#endif

	SResourceNamePool m_kTextureNamePool;
	SResourceNamePool m_kBufferNamePool;
	SResourceNamePool m_kFrameBufferNamePool;

	TPartitions m_kResourceUnitPartitions[eRUT_NUM];
};

bool                 FeatureLevelToFeatureSpec(SFeatureSpec& kContextSpec, D3D_FEATURE_LEVEL eFeatureLevel);
void                 GetStandardPixelFormatSpec(SPixelFormatSpec& kPixelFormatSpec);
bool                 SwapChainDescToFrameBufferSpec(SFrameBufferSpec& kFrameBufferSpec, const DXGI_SWAP_CHAIN_DESC& kSwapChainDesc);
bool                 GetNativeDisplay(TNativeDisplay& kNativeDisplay, HWND kWindowHandle);
bool                 CreateWindowContext(TWindowContext& kWindowContext, const SFeatureSpec& kFeatureSpec, const SPixelFormatSpec& kPixelFormatSpec, const TNativeDisplay& kNativeDisplay);
void                 ReleaseWindowContext(const TWindowContext& kWindowContext);
#if defined(USE_SDL2_VIDEO)
const SGIFormatInfo* SDLFormatToGIFormatInfo(int format);
#endif //defined(USE_SDL2_VIDEO)

uint32 DetectGIFormatSupport(EGIFormat eGIFormat);
bool   DetectFeaturesAndCapabilities(TFeatures& kFeatures, SCapabilities& kCapabilities);
bool   DetectAdapters(std::vector<SAdapterPtr>& kAdapters);
bool   DetectOutputs(const SAdapter& kAdapter, std::vector<SOutputPtr>& kOutputs);
bool   CheckFormatMultisampleSupport(SAdapter* pAdapter, EGIFormat eFormat, uint32 uNumSamples);
void   GetDXGIModeDesc(DXGI_MODE_DESC* pDXGIModeDesc, const SDisplayMode& kDisplayMode);
bool   GetDisplayMode(SDisplayMode* pDisplayMode, const DXGI_MODE_DESC& kDXGIModeDesc);

}

#endif //__GLDEVICE__
