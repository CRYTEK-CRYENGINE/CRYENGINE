// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef WINAPI
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN 1
	#endif
	#include <windows.h>
#endif

#include "glad_gl.h"

#ifndef __glad_wglext_h_

	#ifdef __wglext_h_
		#error WGL header already included, remove this include, glad already provides it
	#endif

	#define __glad_wglext_h_
	#define __wglext_h_

	#ifndef APIENTRY
		#define APIENTRY
	#endif
	#ifndef APIENTRYP
		#define APIENTRYP APIENTRY *
	#endif

	#ifdef __cplusplus
extern "C" {
	#endif

	#ifndef __gladloadproc__
		#define __gladloadproc__
typedef void* (* GLADloadproc)(const char* name);
	#endif

	#ifndef GLAPI
		#if defined(GLAD_GLAPI_EXPORT)
			#if defined(WIN32) || defined(__CYGWIN__)
				#if defined(GLAD_GLAPI_EXPORT_BUILD)
					#if defined(__GNUC__)
						#define GLAPI __attribute__ ((dllexport)) extern
					#else
						#define GLAPI __declspec(dllexport) extern
					#endif
				#else
					#if defined(__GNUC__)
						#define GLAPI __attribute__ ((dllimport)) extern
					#else
						#define GLAPI __declspec(dllimport) extern
					#endif
				#endif
			#elif defined(__GNUC__) && defined(GLAD_GLAPI_EXPORT_BUILD)
				#define GLAPI __attribute__ ((visibility("default"))) extern
			#else
				#define GLAPI extern
			#endif
		#else
			#define GLAPI extern
		#endif
	#endif

GLAPI int gladLoadWGL(HDC hdc);

GLAPI int gladLoadWGLLoader(GLADloadproc, HDC hdc);

struct _GPU_DEVICE
{
	DWORD cb;
	CHAR  DeviceName[32];
	CHAR  DeviceString[128];
	DWORD Flags;
	RECT  rcVirtualScreen;
};
DECLARE_HANDLE(HPBUFFERARB);
DECLARE_HANDLE(HPBUFFEREXT);
DECLARE_HANDLE(HVIDEOOUTPUTDEVICENV);
DECLARE_HANDLE(HPVIDEODEV);
DECLARE_HANDLE(HPGPUNV);
DECLARE_HANDLE(HGPUNV);
DECLARE_HANDLE(HVIDEOINPUTDEVICENV);
typedef struct _GPU_DEVICE  GPU_DEVICE;
typedef struct _GPU_DEVICE* PGPU_DEVICE;
	#define WGL_NUMBER_PIXEL_FORMATS_ARB              0x2000
	#define WGL_DRAW_TO_WINDOW_ARB                    0x2001
	#define WGL_DRAW_TO_BITMAP_ARB                    0x2002
	#define WGL_ACCELERATION_ARB                      0x2003
	#define WGL_NEED_PALETTE_ARB                      0x2004
	#define WGL_NEED_SYSTEM_PALETTE_ARB               0x2005
	#define WGL_SWAP_LAYER_BUFFERS_ARB                0x2006
	#define WGL_SWAP_METHOD_ARB                       0x2007
	#define WGL_NUMBER_OVERLAYS_ARB                   0x2008
	#define WGL_NUMBER_UNDERLAYS_ARB                  0x2009
	#define WGL_TRANSPARENT_ARB                       0x200A
	#define WGL_TRANSPARENT_RED_VALUE_ARB             0x2037
	#define WGL_TRANSPARENT_GREEN_VALUE_ARB           0x2038
	#define WGL_TRANSPARENT_BLUE_VALUE_ARB            0x2039
	#define WGL_TRANSPARENT_ALPHA_VALUE_ARB           0x203A
	#define WGL_TRANSPARENT_INDEX_VALUE_ARB           0x203B
	#define WGL_SHARE_DEPTH_ARB                       0x200C
	#define WGL_SHARE_STENCIL_ARB                     0x200D
	#define WGL_SHARE_ACCUM_ARB                       0x200E
	#define WGL_SUPPORT_GDI_ARB                       0x200F
	#define WGL_SUPPORT_OPENGL_ARB                    0x2010
	#define WGL_DOUBLE_BUFFER_ARB                     0x2011
	#define WGL_STEREO_ARB                            0x2012
	#define WGL_PIXEL_TYPE_ARB                        0x2013
	#define WGL_COLOR_BITS_ARB                        0x2014
	#define WGL_RED_BITS_ARB                          0x2015
	#define WGL_RED_SHIFT_ARB                         0x2016
	#define WGL_GREEN_BITS_ARB                        0x2017
	#define WGL_GREEN_SHIFT_ARB                       0x2018
	#define WGL_BLUE_BITS_ARB                         0x2019
	#define WGL_BLUE_SHIFT_ARB                        0x201A
	#define WGL_ALPHA_BITS_ARB                        0x201B
	#define WGL_ALPHA_SHIFT_ARB                       0x201C
	#define WGL_ACCUM_BITS_ARB                        0x201D
	#define WGL_ACCUM_RED_BITS_ARB                    0x201E
	#define WGL_ACCUM_GREEN_BITS_ARB                  0x201F
	#define WGL_ACCUM_BLUE_BITS_ARB                   0x2020
	#define WGL_ACCUM_ALPHA_BITS_ARB                  0x2021
	#define WGL_DEPTH_BITS_ARB                        0x2022
	#define WGL_STENCIL_BITS_ARB                      0x2023
	#define WGL_AUX_BUFFERS_ARB                       0x2024
	#define WGL_NO_ACCELERATION_ARB                   0x2025
	#define WGL_GENERIC_ACCELERATION_ARB              0x2026
	#define WGL_FULL_ACCELERATION_ARB                 0x2027
	#define WGL_SWAP_EXCHANGE_ARB                     0x2028
	#define WGL_SWAP_COPY_ARB                         0x2029
	#define WGL_SWAP_UNDEFINED_ARB                    0x202A
	#define WGL_TYPE_RGBA_ARB                         0x202B
	#define WGL_TYPE_COLORINDEX_ARB                   0x202C
	#define WGL_SAMPLE_BUFFERS_ARB                    0x2041
	#define WGL_SAMPLES_ARB                           0x2042
	#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB          0x20A9
	#define WGL_CONTEXT_DEBUG_BIT_ARB                 0x00000001
	#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB    0x00000002
	#define WGL_CONTEXT_MAJOR_VERSION_ARB             0x2091
	#define WGL_CONTEXT_MINOR_VERSION_ARB             0x2092
	#define WGL_CONTEXT_LAYER_PLANE_ARB               0x2093
	#define WGL_CONTEXT_FLAGS_ARB                     0x2094
	#define ERROR_INVALID_VERSION_ARB                 0x2095
	#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126
	#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
	#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
	#define ERROR_INVALID_PROFILE_ARB                 0x2096
	#ifndef WGL_ARB_pixel_format
		#define WGL_ARB_pixel_format                    1
GLAPI int GLAD_WGL_ARB_pixel_format;
typedef BOOL (APIENTRYP PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC, int, int, UINT, const int*, int*);
GLAPI PFNWGLGETPIXELFORMATATTRIBIVARBPROC glad_wglGetPixelFormatAttribivARB;
typedef BOOL (APIENTRYP PFNWGLGETPIXELFORMATATTRIBFVARBPROC)(HDC, int, int, UINT, const int*, FLOAT*);
GLAPI PFNWGLGETPIXELFORMATATTRIBFVARBPROC glad_wglGetPixelFormatAttribfvARB;
typedef BOOL (APIENTRYP PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
GLAPI PFNWGLCHOOSEPIXELFORMATARBPROC glad_wglChoosePixelFormatARB;
	#endif
	#ifndef WGL_ARB_multisample
		#define WGL_ARB_multisample 1
GLAPI int GLAD_WGL_ARB_multisample;
	#endif
	#ifndef WGL_ARB_framebuffer_sRGB
		#define WGL_ARB_framebuffer_sRGB 1
GLAPI int GLAD_WGL_ARB_framebuffer_sRGB;
	#endif
	#ifndef WGL_ARB_create_context
		#define WGL_ARB_create_context 1
GLAPI int GLAD_WGL_ARB_create_context;
typedef HGLRC (APIENTRYP PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
GLAPI PFNWGLCREATECONTEXTATTRIBSARBPROC glad_wglCreateContextAttribsARB;
	#endif
	#ifndef WGL_ARB_create_context_profile
		#define WGL_ARB_create_context_profile 1
GLAPI int GLAD_WGL_ARB_create_context_profile;
	#endif
	#ifndef WGL_ARB_extensions_string
		#define WGL_ARB_extensions_string 1
GLAPI int GLAD_WGL_ARB_extensions_string;
typedef const char* (APIENTRYP PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC);
GLAPI PFNWGLGETEXTENSIONSSTRINGARBPROC glad_wglGetExtensionsStringARB;
	#endif
	#ifndef WGL_EXT_extensions_string
		#define WGL_EXT_extensions_string 1
GLAPI int GLAD_WGL_EXT_extensions_string;
typedef const char* (APIENTRYP PFNWGLGETEXTENSIONSSTRINGEXTPROC)();
GLAPI PFNWGLGETEXTENSIONSSTRINGEXTPROC glad_wglGetExtensionsStringEXT;
	#endif
	#if defined(__cplusplus) && defined(GLAD_INSTRUMENT_CALL)
struct glad_tag_wglGetPixelFormatAttribfvARB {};
inline BOOL glad_wrapped_wglGetPixelFormatAttribfvARB(HDC _hdc, int _iPixelFormat, int _iLayerPlane, UINT _nAttributes, const int* _piAttributes, FLOAT* _pfValues) { return GLAD_INSTRUMENT_CALL(wglGetPixelFormatAttribfvARB, _hdc, _iPixelFormat, _iLayerPlane, _nAttributes, _piAttributes, _pfValues); }
		#define wglGetPixelFormatAttribfvARB glad_wrapped_wglGetPixelFormatAttribfvARB
struct glad_tag_wglGetExtensionsStringEXT {};
inline const char* glad_wrapped_wglGetExtensionsStringEXT() { return GLAD_INSTRUMENT_CALL(wglGetExtensionsStringEXT); }
		#define wglGetExtensionsStringEXT glad_wrapped_wglGetExtensionsStringEXT
struct glad_tag_wglGetPixelFormatAttribivARB {};
inline BOOL glad_wrapped_wglGetPixelFormatAttribivARB(HDC _hdc, int _iPixelFormat, int _iLayerPlane, UINT _nAttributes, const int* _piAttributes, int* _piValues) { return GLAD_INSTRUMENT_CALL(wglGetPixelFormatAttribivARB, _hdc, _iPixelFormat, _iLayerPlane, _nAttributes, _piAttributes, _piValues); }
		#define wglGetPixelFormatAttribivARB glad_wrapped_wglGetPixelFormatAttribivARB
struct glad_tag_wglChoosePixelFormatARB {};
inline BOOL glad_wrapped_wglChoosePixelFormatARB(HDC _hdc, const int* _piAttribIList, const FLOAT* _pfAttribFList, UINT _nMaxFormats, int* _piFormats, UINT* _nNumFormats) { return GLAD_INSTRUMENT_CALL(wglChoosePixelFormatARB, _hdc, _piAttribIList, _pfAttribFList, _nMaxFormats, _piFormats, _nNumFormats); }
		#define wglChoosePixelFormatARB glad_wrapped_wglChoosePixelFormatARB
struct glad_tag_wglCreateContextAttribsARB {};
inline HGLRC glad_wrapped_wglCreateContextAttribsARB(HDC _hDC, HGLRC _hShareContext, const int* _attribList) { return GLAD_INSTRUMENT_CALL(wglCreateContextAttribsARB, _hDC, _hShareContext, _attribList); }
		#define wglCreateContextAttribsARB glad_wrapped_wglCreateContextAttribsARB
struct glad_tag_wglGetExtensionsStringARB {};
inline const char* glad_wrapped_wglGetExtensionsStringARB(HDC _hdc) { return GLAD_INSTRUMENT_CALL(wglGetExtensionsStringARB, _hdc); }
		#define wglGetExtensionsStringARB    glad_wrapped_wglGetExtensionsStringARB
	#else
		#define wglGetPixelFormatAttribfvARB glad_wglGetPixelFormatAttribfvARB
		#define wglGetExtensionsStringEXT    glad_wglGetExtensionsStringEXT
		#define wglGetPixelFormatAttribivARB glad_wglGetPixelFormatAttribivARB
		#define wglChoosePixelFormatARB      glad_wglChoosePixelFormatARB
		#define wglCreateContextAttribsARB   glad_wglCreateContextAttribsARB
		#define wglGetExtensionsStringARB    glad_wglGetExtensionsStringARB
	#endif

	#ifdef __cplusplus
}
	#endif

#endif
