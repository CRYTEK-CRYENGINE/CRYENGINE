// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLDevice.cpp
//  Version:     v1.00
//  Created:     03/05/2013 by Valerio Guagliumi.
//  Description: Implementation of the type CDevice and the functions to
//               initialize OpenGL contexts and detect hardware
//               capabilities.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "GLDevice.hpp"
#include "GLResource.hpp"

#if defined(DXGL_ANDROID_GL)
	#include <EGL/egl.h>
#endif
#if CRY_PLATFORM_APPLE
	#include "AppleGPUInfoUtils.h"
#endif

#if defined(USE_SDL2_VIDEO)
	#define HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(result) if (result < 0) { DXGL_ERROR("Failed to set GL attribute: %s", SDL_GetError()); return false; }
#endif //defined(USE_SDL2_VIDEO)

#if defined(DEBUG) && !CRY_PLATFORM_MAC
	#define DXGL_DEBUG_CONTEXT 1
#else
	#define DXGL_DEBUG_CONTEXT 0
#endif

#if defined(RELEASE)
	#define DXGL_DEBUG_OUTPUT_VERBOSITY     0
#elif DXGL_SUPPORT_DEBUG_OUTPUT
	#define DXGL_DEBUG_OUTPUT_VERBOSITY     1
	#if defined(DEBUG)
		#define DXGL_DEBUG_OUTPUT_SYNCHRONOUS 1
	#endif //defined(DEBUG)
#endif

#if DXGL_USE_ES_EMULATOR
extern PFNGLDEBUGMESSAGECONTROLKHRPROC glDebugMessageControl = NULL;
extern PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallback = NULL;
#endif //DXGL_USE_ES_EMULATOR

namespace NCryOpenGL
{

extern const DXGI_FORMAT DXGI_FORMAT_INVALID = DXGI_FORMAT_FORCE_UINT;

#if defined(DXGL_SINGLEWINDOW)
SMainWindow SMainWindow::ms_kInstance = { NULL, 0, 0, "", true };
#endif

#if DXGL_DEBUG_OUTPUT_VERBOSITY
	#if CRY_PLATFORM_WINDOWS
		#define DXGL_DEBUG_CALLBACK_CONVENTION APIENTRY
	#else
		#define DXGL_DEBUG_CALLBACK_CONVENTION
	#endif
void DXGL_DEBUG_CALLBACK_CONVENTION DebugCallback(GLenum eSource, GLenum eType, GLuint uId, GLenum eSeverity, GLsizei uLength, const GLchar* szMessage, void* pUserParam);
#endif //DXGL_DEBUG_OUTPUT_VERBOSITY

#if DXGL_USE_ES_EMULATOR

_smart_ptr<SDisplayConnection> CreateDisplayConnection(
  const SPixelFormatSpec& kPixelFormatSpec,
  const TNativeDisplay& kDefaultNativeDisplay)
{
	_smart_ptr<SDisplayConnection> spDisplayConnection(new SDisplayConnection());

	spDisplayConnection->m_kDisplay = eglGetDisplay(kDefaultNativeDisplay);
	if (spDisplayConnection->m_kDisplay == EGL_NO_DISPLAY)
	{
		DXGL_ERROR("eglGetDisplay failed");
		return NULL;
	}

	if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
	{
		DXGL_ERROR("eglBindAPI failed");
		return NULL;
	}

	if (eglInitialize(spDisplayConnection->m_kDisplay, NULL, NULL) != EGL_TRUE)
	{
		DXGL_ERROR("eglInitialize failed");
		return false;
	}

	EGLint aiAttributes[] =
	{
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
		EGL_RED_SIZE,        kPixelFormatSpec.m_pLayout->m_uRedBits,
		EGL_GREEN_SIZE,      kPixelFormatSpec.m_pLayout->m_uGreenBits,
		EGL_BLUE_SIZE,       kPixelFormatSpec.m_pLayout->m_uBlueBits,
		EGL_ALPHA_SIZE,      kPixelFormatSpec.m_pLayout->m_uAlphaBits,
		EGL_BUFFER_SIZE,     kPixelFormatSpec.m_pLayout->GetColorBits(),
		EGL_DEPTH_SIZE,      kPixelFormatSpec.m_pLayout->m_uDepthBits,
		EGL_STENCIL_SIZE,    kPixelFormatSpec.m_pLayout->m_uStencilBits,
		EGL_SAMPLE_BUFFERS,  kPixelFormatSpec.m_uNumSamples > 1 ? 1 : 0,
		EGL_SAMPLES,         kPixelFormatSpec.m_uNumSamples > 1 ? kPixelFormatSpec.m_uNumSamples : 0,
		EGL_NONE
	};

	EGLint iFoundConfigs;
	if (eglChooseConfig(spDisplayConnection->m_kDisplay, aiAttributes, &spDisplayConnection->m_kConfig, 1, &iFoundConfigs) != EGL_TRUE || iFoundConfigs < 1)
	{
		DXGL_ERROR("eglChooseConfig failed");
		return NULL;
	}

	#if CRY_PLATFORM_WINDOWS && !defined(USE_SDL2_VIDEO)
	EGLNativeWindowType kWindow(WindowFromDC(kDefaultNativeDisplay));
	#else
		#error "Not implemented on this platform"
	#endif
	spDisplayConnection->m_kSurface = eglCreateWindowSurface(spDisplayConnection->m_kDisplay, spDisplayConnection->m_kConfig, kWindow, NULL);
	if (spDisplayConnection->m_kSurface == EGL_NO_SURFACE)
	{
		DXGL_ERROR("eglCreateWindowSurface failed");
		return NULL;
	}

	return spDisplayConnection;
}

#elif defined(USE_SDL2_VIDEO)

bool InitializeSDLAttributes()
{
	int iResult = 0;

	// Setup context versions
	#if CRY_PLATFORM_MAC
	// Mac OS X only supports 4.1, but forcing values to 4.1 causes a significant
	// drop in perforamance. Requesting a 3.2 context gives us a 4.1 context
	// on capable hardware anyhow.
	SVersion kVersion = { 3, 2 };
	#elif defined(DXGL_ANDROID_GL)
	SVersion kVersion = { 4, 4 };
	#elif CRY_RENDERER_OPENGLES
	// Request OpenGL ES 3.0 context
		#if CRY_PLATFORM_ANDROID
	SVersion kVersion = { 3, 1 };
		#else
	SVersion kVersion = { 3, 1 };
		#endif
	#else
	SVersion kVersion = { 4, 3 };
	#endif

	iResult = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kVersion.m_uMajorVersion);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	iResult = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kVersion.m_uMinorVersion);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	#if defined(DXGL_ANDROID_GL)
	if (!eglBindAPI(EGL_OPENGL_API))
		iResult = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else
	#endif
	#if CRY_RENDERER_OPENGLES && !defined(DXGL_ANDROID_GL)
	iResult = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	#else
	{
		iResult = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	}
	#endif
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	iResult = SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	iResult = SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	return true;
}

bool SetSDLAttributes(const SFeatureSpec& kFeatureSpec, const SPixelFormatSpec& kPixelFormatSpec)
{
	int iResult = 0;

	InitializeSDLAttributes();

	// setup up number of bits per field
	iResult = SDL_GL_SetAttribute(SDL_GL_RED_SIZE, kPixelFormatSpec.m_pLayout->m_uRedBits);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	iResult = SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, kPixelFormatSpec.m_pLayout->m_uGreenBits);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	iResult = SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, kPixelFormatSpec.m_pLayout->m_uBlueBits);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	iResult = SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, kPixelFormatSpec.m_pLayout->m_uAlphaBits);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	iResult = SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, kPixelFormatSpec.m_pLayout->GetColorBits());
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	iResult = SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	#if CRY_PLATFORM_MAC
	iResult = SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, kPixelFormatSpec.m_bSRGB ? 1 : 0);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	#endif

	#if DXGL_DEBUG_CONTEXT
	iResult = SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	#endif //DXGL_DEBUG_CONTEXT

	// setup multisampling
	if (kPixelFormatSpec.m_uNumSamples > 1)
	{
		iResult = SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

		iResult = SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, kPixelFormatSpec.m_uNumSamples);
		HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	}
	else
	{
		iResult = SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

		iResult = SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);
	}

	HANDLE_SDL_ATTRIBUTE_ERROR_BOOL(iResult);

	return true;
}

bool CreateSDLWindowContext(TWindowContext& kWindowContext, const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen)
{
	uint32 uWindowFlags = SDL_WINDOW_OPENGL;
	if (bFullScreen)
	{
	#if CRY_PLATFORM_LINUX
		// SDL2 does not handle correctly fullscreen windows with non-native display mode on Linux.
		// It is easier to render offscreen at internal resolution and blit-scale it to native resolution at the end.
		// There is a bug with fullscreen on Linux: https://bugzilla.libsdl.org/show_bug.cgi?id=2373, which makes
		// the fullscreen window leave a black spot in the top.
		uWindowFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE;
	#else
		uWindowFlags |= SDL_WINDOW_FULLSCREEN;
	#endif
	}

	kWindowContext = SDL_CreateWindow(szTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, uWidth, uHeight, uWindowFlags);
	#if !defined(_DEBUG)
	SDL_SetWindowGrab(kWindowContext, SDL_TRUE);
	#endif
	if (!kWindowContext)
	{
		DXGL_ERROR("Failed to create SDL Window %s", SDL_GetError());
		return false;
	}

	// These calls fail, check SDL source (must be >0)
	//SDL_SetWindowMinimumSize(kWindowContext, 0, 0);
	//SDL_SetWindowMaximumSize(kWindowContext, 0, 0);

	if (bFullScreen)
	{
		SDL_DisplayMode kDisplayMode;
		SDL_zero(kDisplayMode);

		if (SDL_SetWindowDisplayMode(kWindowContext, &kDisplayMode) != 0)
		{
			DXGL_ERROR("Failed to set display mode: %s", SDL_GetError());
			return false;
		}

		SDL_ShowWindow(kWindowContext);
	}

	return true;
}

#elif CRY_PLATFORM_WINDOWS

bool SetWindowPixelFormat(const TWindowContext& kWindowContext, const SPixelFormatSpec* pPixelFormatSpec)
{
	int32 iPixelFormat(0);
	PIXELFORMATDESCRIPTOR kPixDesc;
	ZeroMemory(&kPixDesc, sizeof(PIXELFORMATDESCRIPTOR));
	// Check for wgl pixel format extension availability
	if (DXGL_WGL_EXTENSION_SUPPORTED(ARB_pixel_format) && pPixelFormatSpec)
	{
		int32 aiAttributes[128];
		int32* pAttrCursor = aiAttributes;

		*pAttrCursor++ = WGL_DRAW_TO_WINDOW_ARB;
		*pAttrCursor++ = GL_TRUE;
		*pAttrCursor++ = WGL_SUPPORT_OPENGL_ARB;
		*pAttrCursor++ = GL_TRUE;
		*pAttrCursor++ = WGL_DOUBLE_BUFFER_ARB;
		*pAttrCursor++ = GL_TRUE;
		*pAttrCursor++ = WGL_PIXEL_TYPE_ARB;
		*pAttrCursor++ = WGL_TYPE_RGBA_ARB;
		*pAttrCursor++ = WGL_RED_BITS_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uRedBits;
		*pAttrCursor++ = WGL_GREEN_BITS_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uGreenBits;
		*pAttrCursor++ = WGL_BLUE_BITS_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uBlueBits;
		*pAttrCursor++ = WGL_ALPHA_BITS_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uAlphaBits;
		*pAttrCursor++ = WGL_RED_SHIFT_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uRedShift;
		*pAttrCursor++ = WGL_GREEN_SHIFT_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uGreenShift;
		*pAttrCursor++ = WGL_BLUE_SHIFT_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uBlueShift;
		*pAttrCursor++ = WGL_ALPHA_SHIFT_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uAlphaShift;
		*pAttrCursor++ = WGL_COLOR_BITS_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->GetColorBits();
		*pAttrCursor++ = WGL_DEPTH_BITS_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uDepthBits;
		*pAttrCursor++ = WGL_STENCIL_BITS_ARB;
		*pAttrCursor++ = pPixelFormatSpec->m_pLayout->m_uStencilBits;

		// Sample counts 0 and 1 do not require multisampling attribute
		if (pPixelFormatSpec->m_uNumSamples > 1)
		{
			*pAttrCursor++ = WGL_SAMPLES_ARB;
			*pAttrCursor++ = pPixelFormatSpec->m_uNumSamples;
		}

		// Request SRGB pixel format only when needed, skip this attribute otherwise (fix for pedantic drivers)
		if (pPixelFormatSpec->m_bSRGB)
		{
			*pAttrCursor++ = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
			*pAttrCursor++ = GL_TRUE;
		}

		// Mark end of the attribute list
		*pAttrCursor = 0;

		uint32 uNumFormats;
		if (!DXGL_UNWRAPPED_FUNCTION(wglChoosePixelFormatARB)(kWindowContext, aiAttributes, NULL, 1, &iPixelFormat, &uNumFormats))
		{
			DXGL_ERROR("wglChoosePixelFormatARB failed");
			return false;
		}
	}
	else
	{
		kPixDesc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		kPixDesc.nVersion = 1;
		kPixDesc.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
		kPixDesc.iPixelType = PFD_TYPE_RGBA;
		kPixDesc.iLayerType = PFD_MAIN_PLANE;
		if (pPixelFormatSpec != NULL)
		{
			kPixDesc.cRedBits = pPixelFormatSpec->m_pLayout->m_uRedBits;
			kPixDesc.cGreenBits = pPixelFormatSpec->m_pLayout->m_uGreenBits;
			kPixDesc.cBlueBits = pPixelFormatSpec->m_pLayout->m_uBlueBits;
			kPixDesc.cAlphaBits = pPixelFormatSpec->m_pLayout->m_uAlphaBits;
			kPixDesc.cRedShift = pPixelFormatSpec->m_pLayout->m_uRedShift;
			kPixDesc.cGreenShift = pPixelFormatSpec->m_pLayout->m_uGreenShift;
			kPixDesc.cBlueShift = pPixelFormatSpec->m_pLayout->m_uBlueShift;
			kPixDesc.cAlphaShift = pPixelFormatSpec->m_pLayout->m_uAlphaShift;
			kPixDesc.cColorBits = pPixelFormatSpec->m_pLayout->GetColorBits();
			kPixDesc.cDepthBits = pPixelFormatSpec->m_pLayout->m_uDepthBits;
			kPixDesc.cStencilBits = pPixelFormatSpec->m_pLayout->m_uStencilBits;
			if (pPixelFormatSpec->m_uNumSamples > 1 || pPixelFormatSpec->m_bSRGB)
				DXGL_WARNING("wglChoosePixelFormatARB not available - multisampling and sRGB settings will be ignored");
		}
		else
			kPixDesc.cColorBits = 32;

		iPixelFormat = ChoosePixelFormat(kWindowContext, &kPixDesc);
		if (iPixelFormat == 0)
		{
			DXGL_ERROR("ChoosePixelFormat failed");
			return false;
		}

		if (pPixelFormatSpec == NULL &&
		    DescribePixelFormat(kWindowContext, iPixelFormat, sizeof(kPixDesc), &kPixDesc) == 0)
		{
			DXGL_ERROR("DescribePixelFormat failed");
			return false;
		}
	}

	if (!SetPixelFormat(kWindowContext, iPixelFormat, &kPixDesc))
	{
		DXGL_ERROR("SetPixelFormat failed");
		return false;
	}
	return true;
}

#endif

#if CRY_PLATFORM_WINDOWS

namespace NWin32Helper
{

enum
{
	eWS_Windowed   = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	eWS_FullScreen = WS_POPUP
};

bool GetDisplayRect(RECT* pRect, SOutput* pOutput)
{
	DEVMODEA kCurrentMode;
	if (EnumDisplaySettingsA(pOutput->m_strDeviceName.c_str(), ENUM_CURRENT_SETTINGS, &kCurrentMode) != TRUE)
	{
		DXGL_ERROR("Could not retrieve the current display settings for display %s", pOutput->m_strDeviceName.c_str());
		return false;
	}

	DXGL_TODO("Check if scaling according to the GetDeviceCaps is required");
	pRect->left = kCurrentMode.dmPosition.x;
	pRect->top = kCurrentMode.dmPosition.y;
	pRect->right = kCurrentMode.dmPosition.x + kCurrentMode.dmPelsWidth;
	pRect->bottom = kCurrentMode.dmPosition.y + kCurrentMode.dmPelsHeight;
	return true;
}

bool SetFullScreenContext(SOutput* pOutput, TNativeDisplay kNativeDisplay, DEVMODEA& kDevMode)
{
	if (ChangeDisplaySettingsExA(pOutput->m_strDeviceName.c_str(), &kDevMode, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL)
	{
		DXGL_ERROR("Could not change display settings");
		return false;
	}

	RECT kFullScreenRect;
	if (!GetDisplayRect(&kFullScreenRect, pOutput))
		return false;

	HWND kWindowHandle(WindowFromDC(kNativeDisplay));
	DWORD uStyle(GetWindowLong(kWindowHandle, GWL_STYLE));
	uStyle &= ~eWS_Windowed;
	uStyle |= eWS_FullScreen;
	if (SetWindowLong(kWindowHandle, GWL_STYLE, uStyle) == 0)
	{
		DXGL_WARNING("Could not set the window style");
	}
	if (SetWindowPos(kWindowHandle, NULL, kFullScreenRect.left, kFullScreenRect.top, kFullScreenRect.right - kFullScreenRect.left, kFullScreenRect.bottom - kFullScreenRect.top, SWP_NOCOPYBITS) != TRUE)
	{
		DXGL_WARNING("Could not set window posititon");
	}
	return true;
}

bool UnsetFullScreenContext(SOutput* pOutput)
{
	if (ChangeDisplaySettingsExA(pOutput->m_strDeviceName.c_str(), NULL, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL)
	{
		DXGL_ERROR("Could not restore original display settings");
		return false;
	}
	return true;
}

bool ResizeWindowContext(TNativeDisplay kNativeDisplay, uint32 uWidth, uint32 uHeight)
{
	HWND kWindowHandle(WindowFromDC(kNativeDisplay));

	RECT kWindowRect;
	if (!GetWindowRect(kWindowHandle, &kWindowRect))
	{
		DXGL_WARNING("Could not retrieve window rectangle - moving to origin");
		kWindowRect.left = kWindowRect.right = kWindowRect.top = kWindowRect.bottom = 0;
	}
	kWindowRect.right = kWindowRect.left + GetSystemMetrics(SM_CXDLGFRAME) * 2 + uWidth;
	kWindowRect.bottom = kWindowRect.top + GetSystemMetrics(SM_CXDLGFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION) + uHeight;

	DWORD uStyle(GetWindowLong(kWindowHandle, GWL_STYLE));
	uStyle &= ~eWS_FullScreen;
	uStyle |= eWS_Windowed;

	if (SetWindowLong(kWindowHandle, GWL_STYLE, uStyle) == 0)
	{
		DXGL_WARNING("Could not set the window style");
	}
	if (SetWindowPos(kWindowHandle, NULL, kWindowRect.left, kWindowRect.top, kWindowRect.right - kWindowRect.left, kWindowRect.bottom - kWindowRect.top, SWP_NOCOPYBITS) != TRUE)
	{
		DXGL_WARNING("Could not set window posititon");
	}

	return true;
}

};

#endif

#if !defined(USE_SDL2_VIDEO)
SOutput* GetWindowOutput(SAdapter* pAdapter, const TNativeDisplay& kNativeDisplay)
{
	#if CRY_PLATFORM_WINDOWS
	RECT kWindowRect;
	HWND kWindowHandle(WindowFromDC(kNativeDisplay));
	if (kWindowHandle == NULL || GetWindowRect(kWindowHandle, &kWindowRect) != TRUE)
	{
		DXGL_ERROR("Could not retrieve the device window rectangle");
		return false;
	}

	int32 iWindowCenterX((kWindowRect.left + kWindowRect.right) / 2);
	int32 iWindowCenterY((kWindowRect.top + kWindowRect.bottom) / 2);

	uint32 uOutput;
	SOutput* pMinDistOutput(NULL);
	uint32 uMinDistSqr;
	for (uOutput = 0; uOutput < pAdapter->m_kOutputs.size(); ++uOutput)
	{
		SOutput* pOutput(pAdapter->m_kOutputs.at(uOutput));
		RECT kDisplayRect;
		if (!NWin32Helper::GetDisplayRect(&kDisplayRect, pOutput))
			return NULL;

		if (kWindowRect.left >= kDisplayRect.left &&
		    kWindowRect.right <= kDisplayRect.right &&
		    kWindowRect.top >= kDisplayRect.top &&
		    kWindowRect.bottom <= kDisplayRect.bottom)
			return pOutput; // Window completely inside the display rectangle

		int32 iDistX(iWindowCenterX - (kDisplayRect.left + kDisplayRect.right) / 2);
		int32 iDistY(iWindowCenterY - (kDisplayRect.top + kDisplayRect.bottom) / 2);
		uint32 uCenterDistSqr(iDistX * iDistX + iDistY * iDistY);
		if (uOutput == 0 || uCenterDistSqr < uMinDistSqr)
		{
			uMinDistSqr = uCenterDistSqr;
			pMinDistOutput = pOutput;
		}
	}

	return pMinDistOutput;
	#else
		#error "Not implemented on this platform"
	#endif
}
#endif

#if defined(USE_SDL2_VIDEO)

EGIFormat SDLPixelFormatToGIFormat(uint32 uPixelFormat)
{
	switch (uPixelFormat)
	{
	case SDL_PIXELFORMAT_ABGR1555:
	case SDL_PIXELFORMAT_RGBA5551:
	case SDL_PIXELFORMAT_ARGB1555:
	case SDL_PIXELFORMAT_BGRA5551:
		return eGIF_B5G5R5A1_UNORM;
	case SDL_PIXELFORMAT_RGB565:
	case SDL_PIXELFORMAT_BGR565:
		return eGIF_B5G6R5_UNORM;
	case SDL_PIXELFORMAT_RGB24:
	case SDL_PIXELFORMAT_BGR24:
	case SDL_PIXELFORMAT_BGR888:
	case SDL_PIXELFORMAT_BGRX8888:
	case SDL_PIXELFORMAT_RGB888:
		return eGIF_B8G8R8X8_UNORM;
	case SDL_PIXELFORMAT_ARGB8888:
	case SDL_PIXELFORMAT_RGBA8888:
		return eGIF_R8G8B8A8_UNORM;
	case SDL_PIXELFORMAT_ABGR8888:
	case SDL_PIXELFORMAT_BGRA8888:
		return eGIF_B8G8R8A8_UNORM;
	default:
		DXGL_ERROR("Unsupported SDL pixel format as GI format");
		return eGIF_NUM;
	}
}

uint32 GIFormatToSDLPixelFormat(EGIFormat eGIFormat)
{
	switch (eGIFormat)
	{
	case eGIF_B5G5R5A1_UNORM:
		return SDL_PIXELFORMAT_BGRA5551;
	case eGIF_B5G6R5_UNORM:
		return SDL_PIXELFORMAT_BGR565;
	case eGIF_B8G8R8X8_UNORM:
		return SDL_PIXELFORMAT_BGRX8888;
	case eGIF_R8G8B8A8_UNORM:
		return SDL_PIXELFORMAT_RGBA8888;
	case eGIF_B8G8R8A8_UNORM:
		return SDL_PIXELFORMAT_BGRA8888;
	default:
		DXGL_ERROR("Unsupported GI format as SDL pixel format");
		return SDL_PIXELFORMAT_UNKNOWN;
	}
}

bool SDLDisplayModeToDisplayMode(SDisplayMode* pDisplayMode, const SDL_DisplayMode& kSDLDisplayMode)
{
	pDisplayMode->m_uHeight = kSDLDisplayMode.h;
	pDisplayMode->m_uWidth = kSDLDisplayMode.w;
	pDisplayMode->m_uFrequency = kSDLDisplayMode.refresh_rate;
	pDisplayMode->m_ePixelFormat = SDLPixelFormatToGIFormat(kSDLDisplayMode.format);
	return pDisplayMode->m_ePixelFormat != eGIF_NUM;
}

bool DisplayModeToSDLDisplayMode(SDL_DisplayMode* pSDLDisplayMode, const SDisplayMode& kDisplayMode)
{
	SDL_zero(*pSDLDisplayMode);
	pSDLDisplayMode->h = kDisplayMode.m_uHeight;
	pSDLDisplayMode->w = kDisplayMode.m_uWidth;
	pSDLDisplayMode->refresh_rate = kDisplayMode.m_uFrequency;
	pSDLDisplayMode->format = GIFormatToSDLPixelFormat(kDisplayMode.m_ePixelFormat);
	return pSDLDisplayMode->format != SDL_PIXELFORMAT_UNKNOWN;
}

#elif CRY_PLATFORM_WINDOWS

void DevModeToDisplayMode(SDisplayMode* pDisplayMode, const DEVMODEA& kDevMode)
{
	pDisplayMode->m_uBitsPerPixel = kDevMode.dmBitsPerPel;
	pDisplayMode->m_uWidth = kDevMode.dmPelsWidth;
	pDisplayMode->m_uHeight = kDevMode.dmPelsHeight;
	pDisplayMode->m_uFrequency = kDevMode.dmDisplayFrequency;
}

#endif

#if defined(USE_SDL2_VIDEO) || CRY_PLATFORM_WINDOWS

struct SDummyWindow
{
	TNativeDisplay          m_kNativeDisplay;
	#if !defined(USE_SDL2_VIDEO)
	HWND                    m_kWndHandle;
	ATOM                    m_kWndClassAtom;

	static LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	#endif //!defined(USE_SDL2_VIDEO)

	SDummyWindow()
		: m_kNativeDisplay(NULL)
	#if !defined(USE_SDL2_VIDEO)
		, m_kWndHandle(NULL)
		, m_kWndClassAtom(0)
	#endif //!defined(USE_SDL2_VIDEO)
	{
	}

	bool Initialize(const SPixelFormatSpec* pPixelFormatSpec)
	{
	#if defined(USE_SDL2_VIDEO)
		if (!InitializeSDLAttributes())
			return false;

		SDL_Window* pWindow = SDL_CreateWindow("Dummy DXGL window", 100, 100, 0, 0, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
		if (pWindow == NULL)
		{
			DXGL_ERROR("Creation of the dummy DXGL window failed (%s)", SDL_GetError());
			return false;
		}

		m_kNativeDisplay = new TWindowContext(pWindow);
	#elif CRY_PLATFORM_WINDOWS
		WNDCLASSW kWndClass;
		kWndClass.style = CS_HREDRAW | CS_VREDRAW;
		kWndClass.lpfnWndProc = (WNDPROC)iSystem->GetRootWindowMessageHandler();
		kWndClass.cbClsExtra = 0;
		kWndClass.cbWndExtra = 0;
		kWndClass.hInstance = NULL;
		kWndClass.hIcon = LoadIconA(NULL, (LPCSTR)IDI_WINLOGO);
		kWndClass.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
		kWndClass.hbrBackground = NULL;
		kWndClass.lpszMenuName = NULL;
		kWndClass.lpszClassName = L"Dummy DXGL window class";

		if ((m_kWndClassAtom = RegisterClassW(&kWndClass)) == NULL ||
		    (m_kWndHandle = CreateWindowW((LPCWSTR)MAKEINTATOM(m_kWndClassAtom), L"Dummy DXGL window", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL)) == NULL ||
		    (m_kNativeDisplay = GetDC(m_kWndHandle)) == NULL)
		{
			DXGL_ERROR("Creation of the dummy DXGL window failed (%d)", GetLastError());
			return false;
		}

		#if !DXGL_USE_ES_EMULATOR
		if (!SetWindowPixelFormat(m_kNativeDisplay, pPixelFormatSpec))
			return false;
		#endif //!DXGL_USE_ES_EMULATOR
	#endif

		return true;
	}

	void Shutdown()
	{
	#if defined(USE_SDL2_VIDEO)
		if (m_kNativeDisplay)
		{
			if (*m_kNativeDisplay)
				SDL_DestroyWindow(*m_kNativeDisplay);
			delete m_kNativeDisplay;
		}
	#elif CRY_PLATFORM_WINDOWS
		if (m_kWndHandle != NULL)
			DestroyWindow(m_kWndHandle);
		if (m_kWndClassAtom != 0)
			UnregisterClassW((LPCWSTR)MAKEINTATOM(m_kWndClassAtom), NULL);
	#endif
	}
};

#endif //defined(USE_SDL2_VIDEO) || CRY_PLATFORM_WINDOWS

////////////////////////////////////////////////////////////////////////////
// CDevice implementation
////////////////////////////////////////////////////////////////////////////

#if DXGL_FULL_EMULATION
uint32 CDevice::ms_uNumContextsPerDevice = 16;
#else
uint32 CDevice::ms_uNumContextsPerDevice = 1;
#endif

CDevice* CDevice::ms_pCurrentDevice = NULL;

CDevice::CDevice(SAdapter* pAdapter, const SFeatureSpec& kFeatureSpec, const SPixelFormatSpec& kPixelFormatSpec)
	: m_spAdapter(pAdapter)
	, m_kFeatureSpec(kFeatureSpec)
	, m_kPixelFormatSpec(kPixelFormatSpec)
#if OGL_SINGLE_CONTEXT
	, m_bContextFenceIssued(false)
#else
	, m_kContextFenceIssued(false)
#endif
#if DXGL_FULL_EMULATION
	, m_pDummyWindow(NULL)
#endif //DXGL_FULL_EMULATION
{
	if (ms_pCurrentDevice == NULL)
		ms_pCurrentDevice = this;
	m_pCurrentContextTLS = CreateTLS();
}

CDevice::~CDevice()
{
	Shutdown();
	DestroyTLS(m_pCurrentContextTLS);
#if DXGL_FULL_EMULATION
	if (m_pDummyWindow != NULL)
		delete m_pDummyWindow;
#endif //DXGL_FULL_EMULATION
	if (ms_pCurrentDevice == this)
		ms_pCurrentDevice = NULL;
}

#if !DXGL_FULL_EMULATION
void CDevice::Configure(uint32 uNumSharedContexts)
{
	ms_uNumContextsPerDevice = min((uint32)MAX_NUM_CONTEXT_PER_DEVICE, 1 + uNumSharedContexts);
}
#endif //!DXGL_FULL_EMULATION

#if defined(USE_SDL2_VIDEO)

bool CDevice::CreateSDLWindow(const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen, HWND* pHandle)
{
	#if defined(DXGL_SINGLEWINDOW)
	SMainWindow::ms_kInstance.m_strTitle = szTitle;
	SMainWindow::ms_kInstance.m_uWidth = uWidth;
	SMainWindow::ms_kInstance.m_uHeight = uHeight;
	SMainWindow::ms_kInstance.m_bFullScreen = bFullScreen;
	SMainWindow::ms_kInstance.m_pSDLWindow = NULL;
	*pHandle = reinterpret_cast<HWND>(&SMainWindow::ms_kInstance.m_pSDLWindow);
	return true;
	#else
	TWindowContext* pWindowContext = new TWindowContext();
	if (!CreateSDLWindowContext(*pWindowContext, szTitle, uWidth, uHeight, bFullScreen))
		return false;
	*pHandle = reinterpret_cast<HWND>(pWindowContext);
	return true;
	#endif
}

void CDevice::DestroySDLWindow(HWND kHandle)
{
	TWindowContext* pWindowContext = reinterpret_cast<TWindowContext*>(kHandle);
	if (*pWindowContext)
		SDL_DestroyWindow(*pWindowContext);
	delete pWindowContext;
}

#endif //defined(USE_SDL2_VIDEO)

bool CDevice::Initialize(const TNativeDisplay& kDefaultNativeDisplay)
{
	if (kDefaultNativeDisplay == NULL)
	{
#if DXGL_FULL_EMULATION
		SDummyWindow* pDummyWindow(new SDummyWindow());
		if (!pDummyWindow->Initialize(&m_kPixelFormatSpec))
		{
			delete pDummyWindow;
			return false;
		}
		m_pDummyWindow = pDummyWindow;
		m_kDefaultNativeDisplay = pDummyWindow->m_kNativeDisplay;
#else
		return false;
#endif
	}
	else
		m_kDefaultNativeDisplay = kDefaultNativeDisplay;

	std::vector<TRenderingContext> kRenderingContexts;
	if (!CreateRenderingContexts(m_kDefaultWindowContext, kRenderingContexts, m_kFeatureSpec, m_kPixelFormatSpec, m_kDefaultNativeDisplay))
		return false;

	m_kContexts.reserve(kRenderingContexts.size());
	uint32 uContext;
	for (uContext = 0; uContext < kRenderingContexts.size(); ++uContext)
	{
		const TRenderingContext& kRenderingContext(kRenderingContexts.at(uContext));

		CContext* pContext(new CContext(this, kRenderingContext, m_kDefaultWindowContext, uContext));
		MakeCurrent(m_kDefaultWindowContext, kRenderingContext);

		if (uContext == 0)
			InitializeResourceUnitPartitions();

		if (!pContext->Initialize())
		{
			delete pContext;
			return false;
		}

		m_kFreeContexts.Push(*pContext);
		m_kContexts.push_back(pContext);
	}

	MakeCurrent(m_kDefaultWindowContext, NULL);

	return true;
}

void CDevice::Shutdown()
{
	MakeCurrent(m_kDefaultWindowContext, NULL);

	TContexts::const_iterator kContextIter = m_kContexts.begin();
	const TContexts::const_iterator kContextEnd = m_kContexts.end();
	while (kContextIter != kContextEnd)
	{
		TRenderingContext kRenderingContext((*kContextIter)->GetRenderingContext());
		delete *kContextIter;
		// Delete context after all resources have been released. Avoids memory
		// leaks an crashes with non-nvidia drivers
#if DXGL_USE_ES_EMULATOR
		eglDestroyContext(m_kDefaultWindowContext->m_kDisplay, kRenderingContext);
#elif defined(USE_SDL2_VIDEO)
		SDL_GL_DeleteContext(kRenderingContext);
#elif CRY_PLATFORM_WINDOWS
		wglDeleteContext(kRenderingContext);
#else
	#error "Not supported on this platform"
#endif
		++kContextIter;
	}
	m_kContexts.clear();

	if (m_kDefaultWindowContext != NULL)
	{
		ReleaseWindowContext(m_kDefaultWindowContext);
	}

#if defined(USE_SDL2_VIDEO)
	SDL_DestroyWindow(m_kDefaultWindowContext);
	SDL_Quit();
#endif //defined(USE_SDL2_VIDEO)
}

bool CDevice::Present(const TWindowContext& kTargetWindowContext)
{
#if DXGL_USE_ES_EMULATOR
	return eglSwapBuffers(kTargetWindowContext->m_kDisplay, kTargetWindowContext->m_kSurface) == EGL_TRUE;
#elif defined(USE_SDL2_VIDEO)
	SDL_GL_SwapWindow(kTargetWindowContext);
	return true;
#elif CRY_PLATFORM_WINDOWS
	return SwapBuffers(kTargetWindowContext) == TRUE;
#else
	DXGL_NOT_IMPLEMENTED;
	return false;
#endif
}

#if !OGL_SINGLE_CONTEXT

CContext* CDevice::ReserveContext()
{
	CContext* pCurrentContext(static_cast<CContext*>(GetTLSValue(m_pCurrentContextTLS)));
	CContext* pReservedContext(NULL);
	if (pCurrentContext != NULL)
		pReservedContext = pCurrentContext->GetReservedContext();

	if (pReservedContext == NULL)
	{
		pReservedContext = AllocateContext();
		if (pReservedContext == NULL)
			return NULL;
	}

	if (pCurrentContext == NULL)
	{
		pCurrentContext = pReservedContext;
		SetCurrentContext(pCurrentContext);
	}

	pReservedContext->IncReservationCount();
	pCurrentContext->SetReservedContext(pReservedContext);

	return pCurrentContext;
}

void CDevice::ReleaseContext()
{
	CContext* pCurrentContext(GetCurrentContext());
	assert(pCurrentContext != NULL);
	CContext* pReservedContext(pCurrentContext->GetReservedContext());
	assert(pReservedContext != NULL);

	if (pReservedContext->DecReservationCount() == 0)
	{
		if (pCurrentContext == pReservedContext)
			SetCurrentContext(NULL);
		pCurrentContext->SetReservedContext(NULL);
		FreeContext(pReservedContext);
	}
}

#endif

CContext* CDevice::AllocateContext()
{
	CContext* pContext(static_cast<CContext*>(m_kFreeContexts.Pop()));
	if (pContext == NULL)
	{
		DXGL_ERROR("CDevice::AllocateContext failed - no free context available. Please increase the number of contexts at initialization time (currently %d).", (int)m_kContexts.size());
	}
	return pContext;
}

void CDevice::FreeContext(CContext* pContext)
{
	m_kFreeContexts.Push(*pContext);
}

void CDevice::BindContext(CContext* pContext)
{
#if OGL_SINGLE_CONTEXT
	SetCurrentContext(pContext);
#else
	CContext* pCurrentContext(GetCurrentContext());
	if (pCurrentContext != NULL)
		pContext->SetReservedContext(pCurrentContext->GetReservedContext());

	SetCurrentContext(pContext);
#endif
}

void CDevice::UnbindContext(CContext* pContext)
{
#if OGL_SINGLE_CONTEXT
	SetCurrentContext(NULL);
#else
	CContext* pCurrentContext(static_cast<CContext*>(GetTLSValue(m_pCurrentContextTLS)));
	assert(pCurrentContext != NULL);
	SetCurrentContext(pCurrentContext->GetReservedContext());
#endif
}

void CDevice::SetCurrentContext(CContext* pContext)
{
	CContext* pPreviousContext(static_cast<CContext*>(GetTLSValue(m_pCurrentContextTLS)));

	bool bSuccess;
	if (pContext != NULL)
		bSuccess = MakeCurrent(pContext->GetWindowContext(), pContext->GetRenderingContext());
	else if (pPreviousContext != NULL)
		bSuccess = MakeCurrent(pPreviousContext->GetWindowContext(), NULL);

	SetTLSValue(m_pCurrentContextTLS, pContext);

	if (!bSuccess)
	{
		DXGL_ERROR("SetCurrentContext failed");
	}
}

NCryOpenGL::CContext* CDevice::GetCurrentContext()
{
	return static_cast<CContext*>(GetTLSValue(m_pCurrentContextTLS));
}

uint32 CDevice::GetMaxContextCount()
{
	return ms_uNumContextsPerDevice;
}

void CDevice::IssueFrameFences()
{
#if OGL_SINGLE_CONTEXT
	Exchange(&m_bContextFenceIssued, 1);
#else
	for (uint32 uContext = 0; uContext < m_kContexts.size(); ++uContext)
		m_kContextFenceIssued.Set(uContext, true);
#endif
}

bool CDevice::CreateRenderingContexts(
  TWindowContext& kWindowContext,
  std::vector<TRenderingContext>& kRenderingContexts,
  const SFeatureSpec& kFeatureSpec,
  const SPixelFormatSpec& kPixelFormatSpec,
  const TNativeDisplay& kNativeDisplay)
{
	if (!CreateWindowContext(kWindowContext, kFeatureSpec, kPixelFormatSpec, kNativeDisplay))
		return false;

#if DXGL_USE_ES_EMULATOR
	EGLint aiContextAttributes[] = 
	{
		EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
	};
#elif CRY_PLATFORM_WINDOWS && !defined(USE_SDL2_VIDEO)
	int32 aiAttributes[9] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, kFeatureSpec.m_kVersion.m_uMajorVersion,
		WGL_CONTEXT_MINOR_VERSION_ARB, kFeatureSpec.m_kVersion.m_uMinorVersion,
		WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0, 0,
		0
	};

	if (CRenderer::CV_r_EnableDebugLayer || DXGL_DEBUG_CONTEXT)
	{
		aiAttributes[6] = WGL_CONTEXT_FLAGS_ARB;
		aiAttributes[7] = WGL_CONTEXT_DEBUG_BIT_ARB;
	}
#endif

	kRenderingContexts.reserve(ms_uNumContextsPerDevice);
	for (uint32 uContext = 0; uContext < ms_uNumContextsPerDevice; ++uContext)
	{
#if DXGL_USE_ES_EMULATOR
		EGLContext kSharedRenderingContext(EGL_NO_CONTEXT);
		if (uContext > 0)
			MakeCurrent(kWindowContext, kSharedRenderingContext);
		TRenderingContext kRenderingContext(eglCreateContext(kWindowContext->m_kDisplay, kWindowContext->m_kConfig, kSharedRenderingContext, aiContextAttributes));
#elif defined(USE_SDL2_VIDEO)
		if (uContext > 0)
		{
			// This is required because SDL_GL_CreateContext implicitly shares the new context
			// with the current context.
			if (SDL_GL_MakeCurrent(kWindowContext, kRenderingContexts.at(0)) != 0)
			{
				DXGL_ERROR("Failed to set current context: %s", SDL_GetError());
				return false;
			}
		}
		TRenderingContext kRenderingContext(SDL_GL_CreateContext(kWindowContext));
	#if !CRY_PLATFORM_IOS
		// Cannot override vsync in iOS
		if (SDL_GL_SetSwapInterval(0) != 0)
			DXGL_ERROR("Failed to set swap interval: %s", SDL_GetError());
	#endif
#elif CRY_PLATFORM_WINDOWS
		TRenderingContext kSharedRenderingContext(NULL);
		if (uContext > 0)
			kSharedRenderingContext = kRenderingContexts.at(0);
		TRenderingContext kRenderingContext(DXGL_UNWRAPPED_FUNCTION(wglCreateContextAttribsARB)(kWindowContext, kSharedRenderingContext, aiAttributes));
#endif

		if (!kRenderingContext)
		{
			DXGL_ERROR("Failed to create context %d", uContext);
			return false;
		}

		kRenderingContexts.push_back(kRenderingContext);
	}

#if DXGL_DEBUG_OUTPUT_VERBOSITY
	if (SupportDebugOutput())
	{
		GLenum aeSeverityLevels[] =
		{
			GL_DEBUG_SEVERITY_HIGH,
			GL_DEBUG_SEVERITY_MEDIUM,
			GL_DEBUG_SEVERITY_LOW,
			GL_DEBUG_SEVERITY_NOTIFICATION
		};

		for (uint32 uContext = 0; uContext < kRenderingContexts.size(); ++uContext)
		{
			MakeCurrent(kWindowContext, kRenderingContexts.at(uContext));
			glEnable(GL_DEBUG_OUTPUT);
	#if defined(DXGL_DEBUG_OUTPUT_SYNCHRONOUS)
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	#endif //defined(DXGL_DEBUG_OUTPUT_SYNCHRONOUS)
			glDebugMessageCallback((GLDEBUGPROC)&DebugCallback, NULL);

			for (uint32 uSeverityLevel = 0; uSeverityLevel < DXGL_ARRAY_SIZE(aeSeverityLevels); ++uSeverityLevel)
				glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, aeSeverityLevels[uSeverityLevel], 0, NULL, (uSeverityLevel < DXGL_DEBUG_OUTPUT_VERBOSITY) ? GL_TRUE : GL_FALSE);
		}
		MakeCurrent(kWindowContext, NULL);
	}
#endif // DXGL_DEBUG_OUTPUT_VERBOSITY

	return true;
}

bool CDevice::SetFullScreenState(const SFrameBufferSpec& kFrameBufferSpec, bool bFullScreen, SOutput* pOutput)
{
#if defined(USE_SDL2_VIDEO)
	if (pOutput != NULL)
	{
		DXGL_NOT_IMPLEMENTED;
		return false;
	}
	SDL_SetWindowFullscreen(m_kDefaultWindowContext, bFullScreen ? SDL_WINDOW_FULLSCREEN : 0);
	return true;
#elif CRY_PLATFORM_WINDOWS
	if (bFullScreen)
	{
		if (pOutput == NULL)
			pOutput = GetWindowOutput(m_spAdapter, m_kDefaultNativeDisplay);
		if (pOutput == NULL)
		{
			DXGL_ERROR("Could not retrieve the display output corresponding to the window context");
			return false;
		}

		if (pOutput != m_spFullScreenOutput)
		{
			DEVMODEA kReqDevMode;
			memset(&kReqDevMode, 0, sizeof(kReqDevMode));
			kReqDevMode.dmSize = sizeof(kReqDevMode);
			kReqDevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
			kReqDevMode.dmPelsWidth = kFrameBufferSpec.m_uWidth;
			kReqDevMode.dmPelsHeight = kFrameBufferSpec.m_uHeight;
			kReqDevMode.dmBitsPerPel = kFrameBufferSpec.m_pLayout->GetPixelBits();

			if (!NWin32Helper::SetFullScreenContext(pOutput, m_kDefaultNativeDisplay, kReqDevMode))
				return false;

			m_spFullScreenOutput = pOutput;
		}
	}
	else
	{
		if (m_spFullScreenOutput != NULL)
		{
			if (!NWin32Helper::UnsetFullScreenContext(m_spFullScreenOutput))
				return false;

			m_spFullScreenOutput = NULL;
		}
	}
	return true;
#else
	DXGL_NOT_IMPLEMENTED;
	return false;
#endif
}

bool CDevice::ResizeTarget(const SDisplayMode& kTargetMode)
{
#if defined(USE_SDL2_VIDEO)
	const SGIFormatInfo* pPixelFormatInfo(GetGIFormatInfo(kTargetMode.m_ePixelFormat));
	if (pPixelFormatInfo == NULL || pPixelFormatInfo->m_pUncompressed == m_kPixelFormatSpec.m_pLayout)
#elif CRY_PLATFORM_WINDOWS
	if (kTargetMode.m_uBitsPerPixel != m_kPixelFormatSpec.m_pLayout->GetPixelBits())
#else
	#error "Not implemented on this platform"
	if (false)
#endif
	{
		DXGL_WARNING("ResizeTarget does not support changing the window pixel format");
	}

#if defined(USE_SDL2_VIDEO)
	if (m_spFullScreenOutput != NULL)
	{
		SDL_DisplayMode kSDLTargetMode;
		if (!DisplayModeToSDLDisplayMode(&kSDLTargetMode, kTargetMode))
			return false;

		// SDL2 does not update the display mode of a window that is already fullscreen
		// we have to make it windowed, change and then fullscreen again
		SDL_SetWindowFullscreen(m_kDefaultWindowContext, 0);
		SDL_SetWindowDisplayMode(m_kDefaultWindowContext, &kSDLTargetMode);
		SDL_SetWindowFullscreen(m_kDefaultWindowContext, SDL_WINDOW_FULLSCREEN);
	}
	else
	{
		SDL_SetWindowSize(m_kDefaultWindowContext, kTargetMode.m_uWidth, kTargetMode.m_uHeight);
	}
	return true;
#elif CRY_PLATFORM_WINDOWS
	if (m_spFullScreenOutput != NULL)
	{
		DEVMODEA kDevMode;
		memset(&kDevMode, 0, sizeof(kDevMode));
		kDevMode.dmSize = sizeof(kDevMode);
		kDevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
		kDevMode.dmPelsWidth = kTargetMode.m_uWidth;
		kDevMode.dmPelsHeight = kTargetMode.m_uHeight;
		kDevMode.dmBitsPerPel = kTargetMode.m_uBitsPerPixel;
		if (kTargetMode.m_uFrequency != 0)
		{
			kDevMode.dmFields |= DM_DISPLAYFREQUENCY;
			kDevMode.dmDisplayFrequency = kTargetMode.m_uFrequency;
		}

		return NWin32Helper::SetFullScreenContext(m_spFullScreenOutput, m_kDefaultNativeDisplay, kDevMode);
	}
	else
	{
		return NWin32Helper::ResizeWindowContext(m_kDefaultNativeDisplay, kTargetMode.m_uWidth, kTargetMode.m_uHeight);
	}
#else
	#error "Not implemented on this platform"
	return false;
#endif
}

bool CDevice::MakeCurrent(const TWindowContext& kWindowContext, TRenderingContext kRenderingContext)
{
#if DXGL_USE_ES_EMULATOR
	EGLSurface kSurface(kRenderingContext == EGL_NO_CONTEXT ? EGL_NO_SURFACE : kWindowContext->m_kSurface);
	return eglMakeCurrent(kWindowContext->m_kDisplay, kSurface, kSurface, kRenderingContext) == EGL_TRUE;
#elif defined(USE_SDL2_VIDEO)
	return SDL_GL_MakeCurrent(kRenderingContext == NULL ? NULL : kWindowContext, kRenderingContext) == 0;
#elif CRY_PLATFORM_WINDOWS
	return wglMakeCurrent(kRenderingContext == NULL ? NULL : kWindowContext, kRenderingContext) == TRUE;
#else
	DXGL_NOT_IMPLEMENTED;
	return false;
#endif
}

DXGL_TODO("Move default partitions somewhere else/find a better way since it's not engine-related - possibly pass through DXGLInitialize");

#define _PARTITION_PRED(_First, _Count) { _First, _Count },
#define PARTITION_PRED(_Argument)       _PARTITION_PRED _Argument
#define PARTITION(_Vertex, _Fragment, _Geometry, _TessControl, _TessEvaluation, _Compute) \
  { DXGL_SHADER_TYPE_MAP(PARTITION_PRED, _Vertex, _Fragment, _Geometry, _TessControl, _TessEvaluation, _Compute) },

const TPipelineResourceUnitPartitionBound g_akTextureUnitBounds[] =
{
	//         VERTEX      FRAGMENT    GEOMETRY    TESSCTL     TESSEVAL    COMPUTE
	PARTITION((0, 10), (0, 16), (0, 6), (0, 0), (0, 0), (0, 0))                         // Graphics
	PARTITION((0, 0),  (0, 0),  (0, 0), (0, 0), (0, 0), (0, 32))                        // Compute
};

const TPipelineResourceUnitPartitionBound g_akUniformBufferUnitBounds[] =
{
	//         VERTEX      FRAGMENT    GEOMETRY    TESSCTL     TESSEVAL    COMPUTE
	PARTITION((0, 8), (0, 8), (0, 8), (0, 8), (0, 8), (0, 0))                           // Graphics
	PARTITION((0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 16))                          // Compute
};

#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

const TPipelineResourceUnitPartitionBound g_akStorageBufferUnitBounds[] =
{
	//         VERTEX      FRAGMENT    GEOMETRY    TESSCTL     TESSEVAL    COMPUTE
	PARTITION((14, 2), (16, 8), (0, 0), (0, 0), (0, 0), (0, 0))                         // Graphics
	PARTITION((0,  0), (0,  0), (0, 0), (0, 0), (0, 0), (16, 8))                        // Compute
};

#endif

#if DXGL_SUPPORT_SHADER_IMAGES

const TPipelineResourceUnitPartitionBound g_akImageUnitBounds[] =
{
	//         VERTEX      FRAGMENT    GEOMETRY    TESSCTL     TESSEVAL    COMPUTE
	PARTITION((0, 0), (0, 8), (0, 0), (0, 0), (0, 0), (0, 0))                           // Graphics
	PARTITION((0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 8))                           // Compute
};

#endif

#undef PARTITION
#undef PARTITION_PRED
#undef _PARTITION_PRED

void CDevice::InitializeResourceUnitPartitions()
{
	const SCapabilities& kCapabilities(m_spAdapter->m_kCapabilities);

	PartitionResourceIndices(eRUT_Texture      , g_akTextureUnitBounds      , DXGL_ARRAY_SIZE(g_akTextureUnitBounds));
	PartitionResourceIndices(eRUT_UniformBuffer, g_akUniformBufferUnitBounds, DXGL_ARRAY_SIZE(g_akUniformBufferUnitBounds));
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	PartitionResourceIndices(eRUT_StorageBuffer, g_akStorageBufferUnitBounds, DXGL_ARRAY_SIZE(g_akStorageBufferUnitBounds));
#endif
#if DXGL_SUPPORT_SHADER_IMAGES
	PartitionResourceIndices(eRUT_Image        , g_akImageUnitBounds        , DXGL_ARRAY_SIZE(g_akImageUnitBounds));
#endif
}

const char* GetResourceUnitTypeName(EResourceUnitType eUnitType)
{
	switch (eUnitType)
	{
	case eRUT_Texture:
		return "Texture unit";
	case eRUT_UniformBuffer:
		return "Uniform buffer unit";
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	case eRUT_StorageBuffer:
		return "Storage buffer unit";
#endif
#if DXGL_SUPPORT_SHADER_IMAGES
	case eRUT_Image:
		return "Image unit";
#endif
	default:
		assert(false);
		return "?";
	}
}

bool TryDistributeResourceIndices(
  SIndexPartition& kPartition,
  const SResourceUnitCapabilities& kCapabilities,
  const TPipelineResourceUnitPartitionBound& akStageBounds)
{
	uint32 uTotBelowLimit = 0;
	uint32 uTotAvailable = kCapabilities.m_aiMaxTotal;

	uint32 uTotUsed = 0;
	for (uint32 uStage = 0; uStage < eST_NUM; ++uStage)
	{
		kPartition.m_akStages[uStage].m_uCount = akStageBounds[uStage].m_uNumUnits;
		uTotUsed += akStageBounds[uStage].m_uNumUnits;

		if (kCapabilities.m_aiMaxPerStage[uStage] < kPartition.m_akStages[uStage].m_uCount)
			return false;

		uTotBelowLimit += kCapabilities.m_aiMaxPerStage[uStage] - kPartition.m_akStages[uStage].m_uCount;
	}

	if (uTotUsed > uTotAvailable)
		return false;

	while (uTotAvailable > uTotUsed && uTotBelowLimit > 0)
	{
		uint32 uTotRemaining = uTotAvailable - uTotUsed;
		uint32 uTotAssigned = 0;
		for (uint32 uStage = 0; uStage < eST_NUM; ++uStage)
		{
			uint32 uBelowLimit = kCapabilities.m_aiMaxPerStage[uStage] - kPartition.m_akStages[uStage].m_uCount;
			if (uBelowLimit > 0)
			{
				uint32 uAssigned = min(uTotRemaining - uTotAssigned, max(1u, uTotRemaining * uBelowLimit / uTotBelowLimit));
				kPartition.m_akStages[uStage].m_uCount += uAssigned;
				uTotAssigned += uAssigned;
			}
		}
		assert(uTotAssigned > 0 && uTotAssigned <= uTotRemaining);
		uTotUsed += uTotAssigned;
	}

	for (uint32 uStage = 0, uFirstSlot = 0; uStage < eST_NUM; ++uStage)
	{
		kPartition.m_akStages[uStage].m_uFirstIn = akStageBounds[uStage].m_uFirstUnit;
		kPartition.m_akStages[uStage].m_uFirstOut = uFirstSlot;
		uFirstSlot += kPartition.m_akStages[uStage].m_uCount;
	}

	return true;
}

void CDevice::PartitionResourceIndices(
  EResourceUnitType eUnitType,
  const TPipelineResourceUnitPartitionBound* akPartitionBounds,
  uint32 uNumPartitions)
{
	CDevice::TPartitions& kPartitions = m_kResourceUnitPartitions[eUnitType];
	const SResourceUnitCapabilities& kCapabilities = m_spAdapter->m_kCapabilities.m_akResourceUnits[eUnitType];

	kPartitions.reserve(uNumPartitions);

	for (uint32 uPartition = 0; uPartition < uNumPartitions; ++uPartition)
	{
		const TPipelineResourceUnitPartitionBound& akStageBounds(akPartitionBounds[uPartition]);

		SIndexPartition kPartition;
		if (TryDistributeResourceIndices(kPartition, kCapabilities, akStageBounds))
			kPartitions.push_back(kPartition);
		else
			DXGL_WARNING("%s partition %d is not supported by this configuration - it will not be used", GetResourceUnitTypeName(eUnitType), uPartition);
	}
}

#if defined(USE_SDL2_VIDEO) || CRY_PLATFORM_WINDOWS

struct SDummyContext
{
	SDummyWindow      m_kDummyWindow;
	TRenderingContext m_kRenderingContext;
	#if DXGL_USE_ES_EMULATOR
	EGLDisplay        m_kDisplay;
	EGLSurface        m_kWindowSurface;
	EGLConfig         m_kConfig;
	#endif //DXGL_USE_ES_EMULATOR

	SDummyContext()
		: m_kRenderingContext(NULL)
	#if DXGL_USE_ES_EMULATOR
		, m_kDisplay(EGL_NO_DISPLAY)
		, m_kWindowSurface(EGL_NO_SURFACE)
	#endif //DXGL_USE_ES_EMULATOR
	{
	}

	bool Initialize()
	{
		if (!m_kDummyWindow.Initialize(NULL))
			return false;
	#if DXGL_USE_ES_EMULATOR
		EGLint iNumConfigs;
		EGLint aiSurfaceAttributes[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE };
		#if CRY_PLATFORM_WINDOWS
		EGLNativeWindowType kNativeWindow = m_kDummyWindow.m_kWndHandle;
		#else
			#error "Not implemented on this platform"
		#endif
		if ((m_kDisplay = eglGetDisplay(m_kDummyWindow.m_kNativeDisplay)) == EGL_NO_DISPLAY ||
		    eglInitialize(m_kDisplay, NULL, NULL) != EGL_TRUE ||
		    eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE ||
		    eglChooseConfig(m_kDisplay, aiSurfaceAttributes, &m_kConfig, 1, &iNumConfigs) != EGL_TRUE || iNumConfigs < 1 ||
		    (m_kWindowSurface = eglCreateWindowSurface(m_kDisplay, m_kConfig, kNativeWindow, NULL)) == EGL_NO_SURFACE)
		{
			DXGL_ERROR("Creation of the dummy DXGL window failed");
			return false;
		}
		EGLint aiContextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
		m_kRenderingContext = eglCreateContext(m_kDisplay, m_kConfig, EGL_NO_CONTEXT, aiContextAttributes);
		if (m_kRenderingContext == NULL ||
		    eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE ||
		    eglMakeCurrent(m_kDisplay, m_kWindowSurface, m_kWindowSurface, m_kRenderingContext) != EGL_TRUE)
		{
			DXGL_ERROR("Dummy DXGL context creation failed");
			return false;
		}
	#elif defined(USE_SDL2_VIDEO)
		m_kRenderingContext = SDL_GL_CreateContext(*m_kDummyWindow.m_kNativeDisplay);
		if (m_kRenderingContext == NULL)
		{
			DXGL_ERROR("Dummy DXGL context creation failed");
			return false;
		}

		if (SDL_GL_MakeCurrent(*m_kDummyWindow.m_kNativeDisplay, m_kRenderingContext) != 0)
		{
			DXGL_ERROR("Failed to set DXGL dummy context as current");
			return false;
		}
	#else
		m_kRenderingContext = wglCreateContext(m_kDummyWindow.m_kNativeDisplay);
		if (m_kRenderingContext == NULL ||
		    wglMakeCurrent(m_kDummyWindow.m_kNativeDisplay, m_kRenderingContext) != TRUE)
		{
			DXGL_ERROR("Dummy DXGL context creation failed");
			return false;
		}
	#endif
		return true;
	}

	void Shutdown()
	{
	#if DXGL_USE_ES_EMULATOR
		eglMakeCurrent(m_kDisplay, NULL, NULL, NULL);
		if (m_kRenderingContext != NULL)
			eglDestroyContext(m_kDisplay, m_kRenderingContext);
		if (m_kDisplay != EGL_NO_DISPLAY)
		{
			if (m_kWindowSurface != EGL_NO_SURFACE)
				eglDestroySurface(m_kDisplay, m_kWindowSurface);
			eglTerminate(m_kDisplay);
		}
	#elif defined(USE_SDL2_VIDEO)
		if (m_kRenderingContext != NULL)
			SDL_GL_DeleteContext(m_kRenderingContext);
	#else
		wglMakeCurrent(NULL, NULL);
		if (m_kRenderingContext != 0)
			wglDeleteContext(m_kRenderingContext);
	#endif
		m_kDummyWindow.Shutdown();
	}
};
#endif //defined(USE_SDL2_VIDEO) || CRY_PLATFORM_WINDOWS

bool FeatureLevelToFeatureSpec(SFeatureSpec& kFeatureSpec, D3D_FEATURE_LEVEL eFeatureLevel)
{
#if DXGLES && !defined(DXGL_ES_SUBSET)
	if (eFeatureLevel == D3D_FEATURE_LEVEL_11_0)
	{
		kFeatureSpec.m_kVersion.m_uMajorVersion = 3;
		kFeatureSpec.m_kVersion.m_uMinorVersion = 1;
	}
	else
	{
		DXGL_ERROR("Feature level not implemented on OpenGL ES");
		return false;
	}
#else
	switch (eFeatureLevel)
	{
	case D3D_FEATURE_LEVEL_9_1:
	case D3D_FEATURE_LEVEL_9_2:
	case D3D_FEATURE_LEVEL_9_3:
		kFeatureSpec.m_kVersion.m_uMajorVersion = 2;
		kFeatureSpec.m_kVersion.m_uMinorVersion = 0;
		break;
	case D3D_FEATURE_LEVEL_10_0:
	case D3D_FEATURE_LEVEL_10_1:
		kFeatureSpec.m_kVersion.m_uMajorVersion = 3;
		kFeatureSpec.m_kVersion.m_uMinorVersion = 2;
		break;
	case D3D_FEATURE_LEVEL_11_0:
		kFeatureSpec.m_kVersion.m_uMajorVersion = 4;
		kFeatureSpec.m_kVersion.m_uMinorVersion = 3;
		break;
	default:
		DXGL_ERROR("Unknown feature level");
		return false;
	}
#endif

	kFeatureSpec.m_kFeatures.Set(eF_ComputeShader, eFeatureLevel >= D3D_FEATURE_LEVEL_10_0);
	return true;
}

void GetStandardPixelFormatSpec(SPixelFormatSpec& kPixelFormatSpec)
{
	kPixelFormatSpec.m_pLayout = GetGIFormatInfo(eGIF_R8G8B8A8_UNORM_SRGB)->m_pUncompressed;
	kPixelFormatSpec.m_uNumSamples = 1;
	kPixelFormatSpec.m_bSRGB = true;
}

bool SwapChainDescToFrameBufferSpec(SFrameBufferSpec& kFrameBufferSpec, const DXGI_SWAP_CHAIN_DESC& kSwapChainDesc)
{
	EGIFormat eGIFormat(GetGIFormat(kSwapChainDesc.BufferDesc.Format));
	if (eGIFormat == eGIF_NUM)
		return false;

	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(eGIFormat));

	kFrameBufferSpec.m_uWidth = kSwapChainDesc.BufferDesc.Width;
	kFrameBufferSpec.m_uHeight = kSwapChainDesc.BufferDesc.Height;
	kFrameBufferSpec.m_uNumSamples = kSwapChainDesc.SampleDesc.Count;
	kFrameBufferSpec.m_bSRGB = pFormatInfo->m_pTexture->m_bSRGB;
	kFrameBufferSpec.m_pLayout = pFormatInfo->m_pUncompressed;

	return true;
}

bool GetNativeDisplay(TNativeDisplay& kNativeDisplay, HWND kWindowHandle)
{
#if defined(USE_SDL2_VIDEO)
	kNativeDisplay = reinterpret_cast<const NCryOpenGL::TNativeDisplay>(kWindowHandle);
	return true;
#elif CRY_PLATFORM_WINDOWS
	kNativeDisplay = GetDC(kWindowHandle);
	if (kNativeDisplay == NULL)
	{
		DXGL_ERROR("Could not retrieve the DC of the swap chain output window");
		return false;
	}
	return true;
#else
	#error "Not supported on this platform"
#endif
}

bool CreateWindowContext(
  TWindowContext& kWindowContext,
  const SFeatureSpec& kFeatureSpec,
  const SPixelFormatSpec& kPixelFormatSpec,
  const TNativeDisplay& kNativeDisplay)
{
#if DXGL_USE_ES_EMULATOR
	kWindowContext = CreateDisplayConnection(kPixelFormatSpec, kNativeDisplay);
	if (kWindowContext == NULL)
		return false;
#elif defined(USE_SDL2_VIDEO)
	if (!SetSDLAttributes(kFeatureSpec, kPixelFormatSpec))
		return false;
	#if defined(DXGL_SINGLEWINDOW)
	SMainWindow& kMainWindow = SMainWindow::ms_kInstance;
	if (!CreateSDLWindowContext(*kNativeDisplay, kMainWindow.m_strTitle.c_str(), kMainWindow.m_uWidth, kMainWindow.m_uHeight, kMainWindow.m_bFullScreen))
		return false;
	#endif
	kWindowContext = *kNativeDisplay;
#elif CRY_PLATFORM_WINDOWS
	kWindowContext = kNativeDisplay;
	if (!SetWindowPixelFormat(kWindowContext, &kPixelFormatSpec))
		return false;
#endif

	return true;
}

void ReleaseWindowContext(const TWindowContext& kWindowContext)
{
#if DXGL_USE_ES_EMULATOR
	eglDestroySurface(kWindowContext->m_kDisplay, kWindowContext->m_kSurface);
	eglTerminate(kWindowContext->m_kDisplay);
#endif //DXGL_USE_ES_EMULATOR
}

#if DXGL_SUPPORT_QUERY_INTERNAL_FORMAT_SUPPORT

bool QueryInternalFormatSupport(GLenum eTarget, GLenum eInternalFormat, GLenum eQueryName, uint32 uFlag, uint32& uMask)
{
	if (DXGL_GL_EXTENSION_SUPPORTED(ARB_internalformat_query2))
	{
		GLint iSupported;
		glGetInternalformativ(eTarget, eInternalFormat, eQueryName, 1, &iSupported);
		bool bSupported;
		switch (iSupported)
		{
		case GL_NONE:
			bSupported = false;
			break;
		case GL_CAVEAT_SUPPORT:
			DXGL_WARNING("Internal format supported but not optimal");
			bSupported = true;
			break;
		case GL_FULL_SUPPORT:
		case GL_TRUE:
			bSupported = true;
			break;
		default:
			DXGL_ERROR("Invalid parameter returned by internal format query");
			return false;
		}
		uMask = bSupported ? uMask | uFlag : uMask & ~uFlag;
		return true;
	}
	return false;
}

void QueryInternalFormatTexSupport(GLenum eTarget, GLenum eInternalFormat, uint32 uTexFlag, uint32& uMask)
{
	if (QueryInternalFormatSupport(eTarget, eInternalFormat, GL_INTERNALFORMAT_SUPPORTED, uTexFlag, uMask) && (uMask & uTexFlag) != 0)
	{
	#if !defined(_RELEASE)
		GLint iPreferred;
		glGetInternalformativ(eTarget, eInternalFormat, GL_INTERNALFORMAT_PREFERRED, 1, &iPreferred);
		if (iPreferred != eInternalFormat)
			DXGL_WARNING("Internal format supported but not preferred");
	#endif //!defined(_RELEASE)
	}
}

#endif // DXGL_SUPPORT_QUERY_INTERNAL_FORMAT_SUPPORT

uint32 DetectGIFormatSupport(EGIFormat eGIFormat)
{
	uint32 uSupport(0);

	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(eGIFormat));
	if (pFormatInfo == NULL)
		return uSupport;

	uSupport = pFormatInfo->m_uDefaultSupport;

	const STextureFormat* pTextureFormat(pFormatInfo->m_pTexture);
	if (pTextureFormat != NULL)
	{
#if DXGL_SUPPORT_QUERY_INTERNAL_FORMAT_SUPPORT
		QueryInternalFormatTexSupport(GL_TEXTURE_1D, pTextureFormat->m_iInternalFormat, D3D11_FORMAT_SUPPORT_TEXTURE1D, uSupport);
		QueryInternalFormatTexSupport(GL_TEXTURE_2D, pTextureFormat->m_iInternalFormat, D3D11_FORMAT_SUPPORT_TEXTURE2D, uSupport);
		QueryInternalFormatTexSupport(GL_TEXTURE_3D, pTextureFormat->m_iInternalFormat, D3D11_FORMAT_SUPPORT_TEXTURE3D, uSupport);
		QueryInternalFormatTexSupport(GL_TEXTURE_CUBE_MAP, pTextureFormat->m_iInternalFormat, D3D11_FORMAT_SUPPORT_TEXTURECUBE, uSupport);
		QueryInternalFormatSupport(GL_TEXTURE_2D, pTextureFormat->m_iInternalFormat, GL_MIPMAP, D3D11_FORMAT_SUPPORT_MIP, uSupport);
#else
		DXGL_TODO("Use an alternative way to detect texture format support such as proxy textures");
		uSupport |=
		  D3D11_FORMAT_SUPPORT_TEXTURE1D
		  | D3D11_FORMAT_SUPPORT_TEXTURE2D
		  | D3D11_FORMAT_SUPPORT_TEXTURE3D
		  | D3D11_FORMAT_SUPPORT_TEXTURECUBE
		  | D3D11_FORMAT_SUPPORT_MIP;
#endif
	}
	else
	{
		uSupport &= ~(
		  D3D11_FORMAT_SUPPORT_TEXTURE1D
		  | D3D11_FORMAT_SUPPORT_TEXTURE2D
		  | D3D11_FORMAT_SUPPORT_TEXTURE3D
		  | D3D11_FORMAT_SUPPORT_TEXTURECUBE
		  | D3D11_FORMAT_SUPPORT_MIP);
	}

	const SUncompressedLayout* pUncompressedLayout(pFormatInfo->m_pUncompressed);
	if (pUncompressedLayout != NULL && pTextureFormat != NULL)
	{
#if DXGL_SUPPORT_QUERY_INTERNAL_FORMAT_SUPPORT
		if (QueryInternalFormatSupport(GL_TEXTURE_2D, pTextureFormat->m_iInternalFormat, GL_FRAMEBUFFER_RENDERABLE, D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, uSupport))
		{
			uint32 uColorRenderable(0);
			QueryInternalFormatSupport(GL_TEXTURE_2D, pTextureFormat->m_iInternalFormat, GL_COLOR_RENDERABLE, D3D11_FORMAT_SUPPORT_RENDER_TARGET, uColorRenderable);
			uint32 uDepthRenderable(0);
			QueryInternalFormatSupport(GL_TEXTURE_2D, pTextureFormat->m_iInternalFormat, GL_DEPTH_RENDERABLE, D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, uDepthRenderable);
			uint32 uStencilRenderable(0);
			QueryInternalFormatSupport(GL_TEXTURE_2D, pTextureFormat->m_iInternalFormat, GL_STENCIL_RENDERABLE, D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, uStencilRenderable);
			uSupport |= uColorRenderable | uDepthRenderable | uStencilRenderable;

			QueryInternalFormatSupport(GL_TEXTURE_2D, pTextureFormat->m_iInternalFormat, GL_FRAMEBUFFER_BLEND, D3D11_FORMAT_SUPPORT_BLENDABLE, uSupport);
		}
#else
		DXGL_TODO("Use an alternative way to detect format renderability such as per-platform tables in GLFormat.cpp");
		uSupport |=
		  D3D11_FORMAT_SUPPORT_RENDER_TARGET
		  | D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
		  | D3D11_FORMAT_SUPPORT_BLENDABLE
		  | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL;
#endif
	}
	else
	{
		uSupport &= ~(
		  D3D11_FORMAT_SUPPORT_RENDER_TARGET
		  | D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
		  | D3D11_FORMAT_SUPPORT_BLENDABLE
		  | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL);
	}
	return uSupport;
}

#if DXGL_SUPPORT_COPY_IMAGE && DXGL_SUPPORT_GETTEXIMAGE

bool DetectIfCopyImageWorksOnCubeMapFaces()
{
	GLuint auInput[16 * 3 * 3];
	for (uint32 uPixel = 0; uPixel < DXGL_ARRAY_SIZE(auInput); ++uPixel)
		auInput[uPixel] = (GLuint)uPixel;

	GLuint auTextures[2];
	glGenTextures(2, auTextures);
	glTextureStorage2DEXT(auTextures[0], GL_TEXTURE_2D, 1, GL_RGBA8, 4 * 3, 4 * 3);
	glTextureStorage2DEXT(auTextures[1], GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, 4, 4);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glTextureSubImage2DEXT(auTextures[0], GL_TEXTURE_2D, 0, 0, 0, 4 * 3, 4 * 3, GL_RGBA, GL_UNSIGNED_BYTE, auInput);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	GLuint auFace[16];
	GLuint auOutput[16 * 6];
	bool bCopied(true);
	for (uint32 uFace = 0; uFace < 6; ++uFace)
	{
		uint32 uX((uFace % 3) * 4), uY((uFace / 3) * 4);
		glCopyImageSubData(auTextures[0], GL_TEXTURE_2D, 0, uX, uY, 0, auTextures[1], GL_TEXTURE_CUBE_MAP, 0, 0, 0, uFace, 4, 4, 1);
		glGetTextureImageEXT(auTextures[1], GL_TEXTURE_CUBE_MAP_POSITIVE_X + uFace, 0, GL_RGBA, GL_UNSIGNED_BYTE, auFace);
		for (uint32 uRow = 0; uRow < 4; ++uRow)
			NCryOpenGL::Memcpy(auOutput + (uRow + uY) * 4 * 3 + uX, auFace + uRow * 4, 4 * sizeof(GLuint));
	}

	glDeleteTextures(2, auTextures);

	return memcmp(auInput, auOutput, sizeof(auOutput)) == 0;
}

#endif //DXGL_SUPPORT_COPY_IMAGE && DXGL_SUPPORT_GETTEXIMAGE

#define ELEMENT(_Enum) _Enum,

static const GLenum g_aeMaxTextureUnits[] =
{
	DXGL_SHADER_TYPE_MAP(ELEMENT,
	                     GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
	                     GL_MAX_TEXTURE_IMAGE_UNITS,
	                     GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS,
	                     GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS,
	                     GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS,
	                     GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS)
	GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
};

static const GLenum g_aeMaxUniformBufferUnits[] =
{
	DXGL_SHADER_TYPE_MAP(ELEMENT,
	                     GL_MAX_VERTEX_UNIFORM_BLOCKS,
	                     GL_MAX_FRAGMENT_UNIFORM_BLOCKS,
	                     GL_MAX_GEOMETRY_UNIFORM_BLOCKS,
	                     GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS,
	                     GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS,
	                     GL_MAX_COMPUTE_UNIFORM_BLOCKS)
	GL_MAX_UNIFORM_BUFFER_BINDINGS
};

#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

static const GLenum g_aeMaxStorageBufferUnits[] =
{
	DXGL_SHADER_TYPE_MAP(ELEMENT,
	                     GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS,
	                     GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS,
	                     GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS,
	                     GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS,
	                     GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS,
	                     GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS)
	GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS
};

#endif

#if DXGL_SUPPORT_SHADER_IMAGES

static const GLenum g_aeMaxImageUnits[] =
{
	DXGL_SHADER_TYPE_MAP(ELEMENT,
	                     GL_MAX_VERTEX_IMAGE_UNIFORMS,
	                     GL_MAX_FRAGMENT_IMAGE_UNIFORMS,
	                     GL_MAX_GEOMETRY_IMAGE_UNIFORMS,
	                     GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS,
	                     GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS,
	                     GL_MAX_COMPUTE_IMAGE_UNIFORMS)
	GL_MAX_IMAGE_UNITS
};

#endif

#undef ELEMENT

template<const uint32 uPerStageImplementationLimit, const uint32 uTotalImplementationLimit>
void DetectResourceUnitCapabilities(SResourceUnitCapabilities* pCapabilities, const GLenum* aeMaxUnits)
{
	memset(pCapabilities->m_aiMaxPerStage, 0, sizeof(pCapabilities->m_aiMaxPerStage));
	for (uint32 uStage = 0; uStage < eST_NUM; ++uStage)
	{
		glGetIntegerv(aeMaxUnits[uStage], pCapabilities->m_aiMaxPerStage + uStage);
		if (pCapabilities->m_aiMaxPerStage[uStage] > uPerStageImplementationLimit)
			pCapabilities->m_aiMaxPerStage[uStage] = uPerStageImplementationLimit;
	}

	pCapabilities->m_aiMaxTotal = 0;
	glGetIntegerv(aeMaxUnits[eST_NUM], &pCapabilities->m_aiMaxTotal);
	if (pCapabilities->m_aiMaxTotal > uTotalImplementationLimit)
		pCapabilities->m_aiMaxTotal = uTotalImplementationLimit;
}

bool DetectFeaturesAndCapabilities(TFeatures& kFeatures, SCapabilities& kCapabilities)
{
	glGetIntegerv(GL_MAX_SAMPLES, &kCapabilities.m_iMaxSamples);
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &kCapabilities.m_iMaxVertexAttribs);
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &kCapabilities.m_iShaderStorageBufferOffsetAlignment);
#endif
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &kCapabilities.m_iUniformBufferOffsetAlignment);

	DetectResourceUnitCapabilities<MAX_STAGE_TEXTURE_UNITS       , MAX_TEXTURE_UNITS       >(&kCapabilities.m_akResourceUnits[eRUT_Texture      ], g_aeMaxTextureUnits);
	DetectResourceUnitCapabilities<MAX_STAGE_UNIFORM_BUFFER_UNITS, MAX_UNIFORM_BUFFER_UNITS>(&kCapabilities.m_akResourceUnits[eRUT_UniformBuffer], g_aeMaxUniformBufferUnits);
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	DetectResourceUnitCapabilities<MAX_STAGE_STORAGE_BUFFER_UNITS, MAX_STORAGE_BUFFER_UNITS>(&kCapabilities.m_akResourceUnits[eRUT_StorageBuffer], g_aeMaxStorageBufferUnits);
#endif
#if DXGL_SUPPORT_SHADER_IMAGES
	DetectResourceUnitCapabilities<MAX_STAGE_IMAGE_UNITS         , MAX_IMAGE_UNITS         >(&kCapabilities.m_akResourceUnits[eRUT_Image        ], g_aeMaxImageUnits);
#endif

#if DXGL_SUPPORT_VERTEX_ATTRIB_BINDING
	if (SupportVertexAttribBinding())
	{
		glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &kCapabilities.m_iMaxVertexAttribBindings);
		glGetIntegerv(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, &kCapabilities.m_iMaxVertexAttribRelativeOffset);

		if (kCapabilities.m_iMaxVertexAttribBindings > MAX_VERTEX_ATTRIB_BINDINGS)
			kCapabilities.m_iMaxVertexAttribBindings = MAX_VERTEX_ATTRIB_BINDINGS;
	}
#endif

	for (uint32 uGIFormat = 0; uGIFormat < eGIF_NUM; ++uGIFormat)
		kCapabilities.m_auFormatSupport[uGIFormat] = DetectGIFormatSupport((EGIFormat)uGIFormat);
#if DXGL_SUPPORT_COMPUTE
	kFeatures.Set(eF_ComputeShader, SupportCompute());
#else
	kFeatures.Set(eF_ComputeShader, false);
#endif

#if DXGL_SUPPORT_COPY_IMAGE && DXGL_SUPPORT_GETTEXIMAGE
	kCapabilities.m_bCopyImageWorksOnCubeMapFaces = DetectIfCopyImageWorksOnCubeMapFaces();
#endif

	return true;
}

size_t DetectVideoMemory()
{
#if DXGL_EXTENSION_LOADER
	#if !DXGLES
	if (DXGL_GL_EXTENSION_SUPPORTED(NVX_gpu_memory_info))
	{
		GLint iVMemKB = 0;
		glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &iVMemKB);
		return iVMemKB * 1024;
	}
	else if (DXGL_GL_EXTENSION_SUPPORTED(ATI_meminfo))
	{
		GLint aiTexFreeMemoryInfo[4] = { 0, 0, 0, 0 };
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, aiTexFreeMemoryInfo);
		return aiTexFreeMemoryInfo[0] * 1024;
	}
	#else
	DXGL_TODO("Not yet implemented for GLES");
	#endif
	return 0;
#elif CRY_PLATFORM_MAC
	return static_cast<size_t>(GetVRAMForDisplay(0));
#elif CRY_PLATFORM_IOS
	DXGL_TODO("Not yet implemented for iOS")
	return 0;
#elif DXGL_USE_ES_EMULATOR
	DXGL_TODO("Not yet implemented for GLES emulation");
	return 0;
#else
	#error "Not implemented on this platform"
#endif
}

EDriverVendor DetectDriverVendor(const char* szVendorName)
{
	struct
	{
		EDriverVendor eDriverVendor;
		char*         szName;
	} akKnownVendors[] =
	{
		{ eDV_NVIDIA,   "NVIDIA Corporation"                  },
		{ eDV_NOUVEAU,  "Nouveau"                             },
		{ eDV_NOUVEAU,  "nouveau"                             },
		{ eDV_ATI,      "ATI Technologies Inc."               },
		{ eDV_AMD,      "Advanced Micro Devices, Inc."        },
		{ eDV_INTEL,    "Intel"                               },
		{ eDV_INTEL,    "Intel Inc."                          },
		{ eDV_INTEL,    "Intel Corporation"                   },
		{ eDV_INTEL_OS, "Intel Open Source Technology Center" },
	};

	for (uint32 uVendor = 0; uVendor < DXGL_ARRAY_SIZE(akKnownVendors); ++uVendor)
	{
		if (strcmp(szVendorName, akKnownVendors[uVendor].szName) == 0)
		{
			return akKnownVendors[uVendor].eDriverVendor;
		}
	}
	return eDV_UNKNOWN;
}

#if DXGL_EXTENSION_LOADER

bool LoadGLEntryPoints(SDummyContext& kDummyContext)
{
	#if defined(DXGL_USE_GLEW)
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		DXGL_ERROR("GLEW initialization failed");
		return false;
	}
	#elif defined(DXGL_USE_GLAD)
	if (gladLoadGL() != 1)
	{
		DXGL_ERROR("Failed to retrieve GL entry points");
		return false;
	}
		#if CRY_PLATFORM_WINDOWS
	if (gladLoadWGL(kDummyContext.m_kDummyWindow.m_kNativeDisplay) != 1)
	{
		DXGL_ERROR("Failed to retrieve WGL entry points");
		return false;
	}
		#elif CRY_PLATFORM_ANDROID
	gladLoadEGL();
		#elif CRY_PLATFORM_LINUX
	if (gladLoadGLX(NULL, 0) != 1)
	{
		DXGL_ERROR("Failed to retrieve GLX entry points");
		return false;
	}
		#else
			#error "Not implemented on this platform"
		#endif
	#else
		#error "Not implemented on this configuration"
	#endif
	return true;
}

#endif //DXGL_EXTENSION_LOADER

#if DXGL_USE_ES_EMULATOR

template<typename FunctionPtr>
bool GetEntryPoint(FunctionPtr& pEntryPoint, const char* szName)
{
	if ((pEntryPoint = (FunctionPtr)eglGetProcAddress(szName)) != NULL)
		return true;

	#if CRY_PLATFORM_WINDOWS
	if ((pEntryPoint = (FunctionPtr)wglGetProcAddress(szName)) != NULL)
		return true;
	#endif // CRY_PLATFORM_WINDOWS

	DXGL_WARNING("Could not load entry point for \"%s\"", szName);
	return false;
}

bool LoadGLESExtensionEntryPoints()
{
	return
	  GetEntryPoint(glDebugMessageCallback, "glDebugMessageCallback") &&
	  GetEntryPoint(glDebugMessageControl, "glDebugMessageControl");
}

#endif //DXGL_USE_ES_EMULATOR

bool DetectAdapters(std::vector<SAdapterPtr>& kAdapters)
{
	SDummyContext kDummyContext;
	bool bSuccess(kDummyContext.Initialize());
	if (bSuccess)
	{
		SAdapterPtr spAdapter(new SAdapter);
#if DXGL_EXTENSION_LOADER
		if (!LoadGLEntryPoints(kDummyContext))
			bSuccess = false;
		else
#elif DXGL_USE_ES_EMULATOR
		if (!LoadGLESExtensionEntryPoints())
			return false;
		else
#elif CRY_PLATFORM_ANDROID && CRY_RENDERER_OPENGLES
		if (!android_gles_3_0_init())
		{
			DXGL_ERROR("Current device does not have a valid OpenGL ES 3 driver");
			bSuccess = false;
		}
		else
#endif //DXGL_EXTENSION_LOADER
		{
			spAdapter->m_strRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
			spAdapter->m_strVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
			spAdapter->m_strVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
			spAdapter->m_uVRAMBytes = DetectVideoMemory();
			spAdapter->m_eDriverVendor = DetectDriverVendor(spAdapter->m_strVendor.c_str());

			if (!DetectFeaturesAndCapabilities(spAdapter->m_kFeatures, spAdapter->m_kCapabilities))
				return false;
			kAdapters.push_back(spAdapter);
		}
	}

	kDummyContext.Shutdown();
	return bSuccess;
}

bool DetectOutputs(const SAdapter& kAdapter, std::vector<SOutputPtr>& kOutputs)
{
#if defined(USE_SDL2_VIDEO)
	int uNumDisplays = SDL_GetNumVideoDisplays();
	if (uNumDisplays <= 0)
	{
		DXGL_ERROR("Failed to retrieve number of displays: %s", SDL_GetError());
		return false;
	}
	uint32 uDisplay;
	for (uDisplay = 0; uDisplay < (uint32)uNumDisplays; ++uDisplay)
	{
		int uNumDisplayModes = SDL_GetNumDisplayModes(uDisplay);
		if (uNumDisplayModes < 0)
		{
			DXGL_ERROR("Failed to retrieve number of display modes for display %d: %s", uDisplay, SDL_GetError());
			return false;
		}
		SOutputPtr spOutput(new SOutput);
		char acBuffer[32];
		cry_sprintf(acBuffer, "SDL Screen %d", uDisplay);
		spOutput->m_strDeviceName = acBuffer;
		spOutput->m_strDeviceID = spOutput->m_strDeviceName;
		uint32 uDisplayMode;
		for (uDisplayMode = 0; uDisplayMode < (uint32)uNumDisplayModes; ++uDisplayMode)
		{
			SDL_DisplayMode kSDLDisplayMode;
			if (0 != SDL_GetDisplayMode(uDisplay, uDisplayMode, &kSDLDisplayMode))
			{
				DXGL_ERROR("Failed to retrieve display mode info for output %d: %s", uDisplay, SDL_GetError());
				return false;
			}

			SDisplayMode kDisplayMode;
			if (SDLDisplayModeToDisplayMode(&kDisplayMode, kSDLDisplayMode))
				spOutput->m_kModes.push_back(kDisplayMode);
		}

		SDL_DisplayMode kSDLDesktopDisplayMode;
		if (SDL_GetDesktopDisplayMode(uDisplay, &kSDLDesktopDisplayMode) != 0 ||
		    !SDLDisplayModeToDisplayMode(&spOutput->m_kDesktopMode, kSDLDesktopDisplayMode))
		{
			DXGL_ERROR("Could not retrieve the desktop display mode for display %d", uDisplay);
			return false;
		}

		kOutputs.push_back(spOutput);
	}

	return true;
#elif CRY_PLATFORM_WINDOWS
	uint32 uDisplay(0);
	DISPLAY_DEVICEA kDisplayDevice;
	ZeroMemory(&kDisplayDevice, sizeof(kDisplayDevice));
	kDisplayDevice.cb = sizeof(kDisplayDevice);
	while (EnumDisplayDevicesA(NULL, uDisplay, &kDisplayDevice, 0))
	{
		SOutputPtr spOutput(new SOutput);
		spOutput->m_strDeviceID = kDisplayDevice.DeviceID;
		spOutput->m_strDeviceName = kDisplayDevice.DeviceName;

		DEVMODEA kDevMode;
		ZeroMemory(&kDevMode, sizeof(kDevMode));
		kDevMode.dmSize = sizeof(kDevMode);

		SDisplayMode kDisplayMode;
		uint32 uModeID(0);
		while (EnumDisplaySettingsA(kDisplayDevice.DeviceName, uModeID, &kDevMode))
		{
			DevModeToDisplayMode(&kDisplayMode, kDevMode);
			++uModeID;

			spOutput->m_kModes.push_back(kDisplayMode);
		}

		if (!spOutput->m_kModes.empty())
		{
			if (!EnumDisplaySettingsA(kDisplayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &kDevMode))
			{
				DXGL_ERROR("Could not retrieve the desktop display mode mode for display %d", uDisplay);
				return false;
			}
			DevModeToDisplayMode(&spOutput->m_kDesktopMode, kDevMode);

			kOutputs.push_back(spOutput);
		}
		++uDisplay;
	}
	return true;
#else
	DXGL_NOT_IMPLEMENTED;
	return false;
#endif
}

bool CheckFormatMultisampleSupport(SAdapter* pAdapter, EGIFormat eFormat, uint32 uNumSamples)
{
	return
	  uNumSamples <= pAdapter->m_kCapabilities.m_iMaxSamples;
}

void GetDXGIModeDesc(DXGI_MODE_DESC* pDXGIModeDesc, const SDisplayMode& kDisplayMode)
{
	pDXGIModeDesc->Width = kDisplayMode.m_uWidth;
	pDXGIModeDesc->Height = kDisplayMode.m_uHeight;
	pDXGIModeDesc->RefreshRate.Numerator = kDisplayMode.m_uFrequency;
	pDXGIModeDesc->RefreshRate.Denominator = 1;

#if defined(USE_SDL2_VIDEO)
	pDXGIModeDesc->Format = GetDXGIFormat(kDisplayMode.m_ePixelFormat);
#elif CRY_PLATFORM_WINDOWS
	DXGL_TODO("Check if there is a better way of mapping GL display modes to formats")
	switch (kDisplayMode.m_uBitsPerPixel)
	{
	case 32:
		pDXGIModeDesc->Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case 64:
		pDXGIModeDesc->Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		break;
	default:
		pDXGIModeDesc->Format = DXGI_FORMAT_UNKNOWN;
		break;
	}
#endif

	pDXGIModeDesc->ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	pDXGIModeDesc->Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
}

bool GetDisplayMode(SDisplayMode* pDisplayMode, const DXGI_MODE_DESC& kDXGIModeDesc)
{
	pDisplayMode->m_uWidth = kDXGIModeDesc.Width;
	pDisplayMode->m_uHeight = kDXGIModeDesc.Height;
	if (kDXGIModeDesc.RefreshRate.Denominator != 0)
		pDisplayMode->m_uFrequency = kDXGIModeDesc.RefreshRate.Numerator / kDXGIModeDesc.RefreshRate.Denominator;
	else
		pDisplayMode->m_uFrequency = 0;
#if defined(USE_SDL2_VIDEO)
	pDisplayMode->m_ePixelFormat = GetGIFormat(kDXGIModeDesc.Format);
#elif CRY_PLATFORM_WINDOWS
	switch (kDXGIModeDesc.Format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		pDisplayMode->m_uBitsPerPixel = 32;
		break;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		pDisplayMode->m_uBitsPerPixel = 64;
		break;
	default:
		{
			EGIFormat eGIFormat(GetGIFormat(kDXGIModeDesc.Format));
			const SGIFormatInfo* pFormatInfo;
			if (eGIFormat == eGIF_NUM ||
			    (pFormatInfo = GetGIFormatInfo(eGIFormat)) == NULL ||
			    pFormatInfo->m_pUncompressed == NULL)
			{
				DXGL_ERROR("Invalid DXGI format for display mode");
				return false;
			}
			pDisplayMode->m_uBitsPerPixel = pFormatInfo->m_pUncompressed->GetPixelBits();
		}
		break;
	}
#endif
	DXGL_TODO("Consider scanline order and scaling if possible");

	return true;
}

#if DXGL_DEBUG_OUTPUT_VERBOSITY

void DXGL_DEBUG_CALLBACK_CONVENTION DebugCallback(GLenum eSource, GLenum eType, GLuint uId, GLenum eSeverity, GLsizei uLength, const char* szMessage, void* pUserParam)
{
	ELogSeverity eLogSeverity;
	switch (eSeverity)
	{
	case GL_DEBUG_SEVERITY_HIGH:
		eLogSeverity = eLS_Error;
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		eLogSeverity = eLS_Warning;
		break;
	default:
		eLogSeverity = eLS_Warning;
		break;
	}
	NCryOpenGL::LogMessage(eLogSeverity, "[GL_DEBUG] %s [source=0x%X, type=0x%X, id=0x%X]", szMessage, (uint32)eSource, (uint32)eType, (uint32)uId);
}

#endif //DXGL_DEBUG_OUTPUT_VERBOSITY

#if DXGL_TRACE_CALLS

void CallTracePrintf(const char* szFormat, ...)
{
	CDevice* pDevice(CDevice::GetCurrentDevice());
	if (pDevice == NULL)
		return;

	CContext* pCurrentContext(pDevice->GetCurrentContext());
	if (pCurrentContext == NULL)
		return;

	char acBuffer[512];
	va_list kArgs;
	va_start(kArgs, szFormat);
	cry_vsprintf(acBuffer, szFormat, kArgs);
	va_end(kArgs);

	pCurrentContext->CallTraceWrite(acBuffer);
}

void CallTraceFlush()
{
	CDevice* pDevice(CDevice::GetCurrentDevice());
	if (pDevice == NULL)
		return;

	CContext* pCurrentContext(pDevice->GetCurrentContext());
	if (pCurrentContext == NULL)
		return;

	pCurrentContext->CallTraceFlush();
}

#endif

#if DXGL_CHECK_ERRORS

void CheckErrors()
{
	enum {MAX_ERROR_QUERIES = 4};
	uint32 uNumQueries(0);
	GLenum eErrorCode;
	while ((eErrorCode = DXGL_UNWRAPPED_FUNCTION(glGetError)()) != GL_NO_ERROR)
	{
		const char* szName;
		const char* szMessage;
		switch (eErrorCode)
		{
		case GL_CONTEXT_LOST:
			szName = "GL_CONTEXT_LOST";
			szMessage = "Context has been lost and reset by the driver";
			break;
		case GL_INVALID_ENUM:
			szName = "GL_INVALID_ENUM";
			szMessage = "Enum argument out of range";
			break;
		case GL_INVALID_VALUE:
			szName = "GL_INVALID_VALUE";
			szMessage = "Numeric argument out of range";
			break;
		case GL_INVALID_OPERATION:
			szName = "GL_INVALID_OPERATION";
			szMessage = "Operation illegal in current state";
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			szName = "GL_INVALID_FRAMEBUFFER_OPERATION";
			szMessage = "Framebuffer object is not complete";
			break;
		case GL_OUT_OF_MEMORY:
			szName = "GL_OUT_OF_MEMORY";
			szMessage = "Not enough memory left to execute command";
			break;
		case GL_STACK_OVERFLOW:
			szName = "GL_STACK_OVERFLOW";
			szMessage = "Command would cause a stack overflow";
			break;
		case GL_STACK_UNDERFLOW:
			szName = "GL_STACK_UNDERFLOW";
			szMessage = "Command would cause a stack underflow";
			break;
		default:
			szName = "?";
			szMessage = "Unknown GL error";
			break;
		}
		DXGL_ERROR("GL error: %s (0x%04X) - %s", szName, eErrorCode, szMessage);
		if (++uNumQueries > MAX_ERROR_QUERIES)
		{
			DXGL_ERROR("GL error limit reached - probably no context set");
			break;
		}
	}
}

#endif //DXGL_CHECK_ERRORS

}
