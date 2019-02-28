// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "glad_gl.h"

#ifndef __glad_glxext_h_

	#ifdef __glxext_h_
		#error GLX header already included, remove this include, glad already provides it
	#endif

	#define __glad_glxext_h_
	#define __glxext_h_

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

GLAPI int gladLoadGLX(Display* dpy, int screen);

GLAPI int gladLoadGLXLoader(GLADloadproc, Display * dpy, int screen);

	#ifndef GLEXT_64_TYPES_DEFINED
/* This code block is duplicated in glext.h, so must be protected */
		#define GLEXT_64_TYPES_DEFINED
/* Define int32_t, int64_t, and uint64_t types for UST/MSC */
/* (as used in the GLX_OML_sync_control extension). */
		#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
			#include <inttypes.h>
		#elif defined(__sun__) || defined(__digital__)
			#include <inttypes.h>
			#if defined(__STDC__)
				#if defined(__arch64__) || defined(_LP64)
typedef long int               int64_t;
typedef unsigned long int      uint64_t;
				#else
typedef long long int          int64_t;
typedef unsigned long long int uint64_t;
				#endif /* __arch64__ */
			#endif   /* __STDC__ */
		#elif defined(__VMS) || defined(__sgi)
			#include <inttypes.h>
		#elif defined(__SCO__) || defined(__USLC__)
			#include <stdint.h>
		#elif defined(__UNIXOS2__) || defined(__SOL64__)
typedef long int               int32_t;
typedef long long int          int64_t;
typedef unsigned long long int uint64_t;
		#elif defined(_WIN32) && defined(__GNUC__)
			#include <stdint.h>
		#elif defined(_WIN32)
typedef __int32          int32_t;
typedef __int64          int64_t;
typedef unsigned __int64 uint64_t;
		#else
/* Fallback if nothing above works */
			#include <inttypes.h>
		#endif
	#endif
typedef XID                      GLXFBConfigID;
typedef struct __GLXFBConfigRec* GLXFBConfig;
typedef XID                      GLXContextID;
typedef struct __GLXcontextRec*  GLXContext;
typedef XID                      GLXPixmap;
typedef XID                      GLXDrawable;
typedef XID                      GLXWindow;
typedef XID                      GLXPbuffer;
typedef void (APIENTRY *         __GLXextFuncPtr)(void);
typedef XID                      GLXVideoCaptureDeviceNV;
typedef unsigned int             GLXVideoDeviceNV;
typedef XID                      GLXVideoSourceSGIX;
typedef XID                      GLXFBConfigIDSGIX;
typedef struct __GLXFBConfigRec* GLXFBConfigSGIX;
typedef XID                      GLXPbufferSGIX;
typedef struct
{
	int           event_type;  /* GLX_DAMAGED or GLX_SAVED */
	int           draw_type;   /* GLX_WINDOW or GLX_PBUFFER */
	unsigned long serial;      /* # of last request processed by server */
	Bool          send_event;  /* true if this came for SendEvent request */
	Display*      display;     /* display the event was read from */
	GLXDrawable   drawable;    /* XID of Drawable */
	unsigned int  buffer_mask; /* mask indicating which buffers are affected */
	unsigned int  aux_buffer;  /* which aux buffer was affected */
	int           x, y;
	int           width, height;
	int           count; /* if nonzero, at least this many more */
} GLXPbufferClobberEvent;
typedef struct
{
	int           type;
	unsigned long serial;     /* # of last request processed by server */
	Bool          send_event; /* true if this came from a SendEvent request */
	Display*      display;    /* Display the event was read from */
	GLXDrawable   drawable;   /* drawable on which event was requested in event mask */
	int           event_type;
	int64_t       ust;
	int64_t       msc;
	int64_t       sbc;
} GLXBufferSwapComplete;
typedef union __GLXEvent
{
	GLXPbufferClobberEvent glxpbufferclobber;
	GLXBufferSwapComplete  glxbufferswapcomplete;
	long                   pad[24];
} GLXEvent;
typedef struct
{
	int           type;
	unsigned long serial;
	Bool          send_event;
	Display*      display;
	int           extension;
	int           evtype;
	GLXDrawable   window;
	Bool          stereo_tree;
} GLXStereoNotifyEventEXT;
typedef struct
{
	int           type;
	unsigned long serial;     /* # of last request processed by server */
	Bool          send_event; /* true if this came for SendEvent request */
	Display*      display;    /* display the event was read from */
	GLXDrawable   drawable;   /* i.d. of Drawable */
	int           event_type; /* GLX_DAMAGED_SGIX or GLX_SAVED_SGIX */
	int           draw_type;  /* GLX_WINDOW_SGIX or GLX_PBUFFER_SGIX */
	unsigned int  mask;       /* mask indicating which buffers are affected*/
	int           x, y;
	int           width, height;
	int           count; /* if nonzero, at least this many more */
} GLXBufferClobberEventSGIX;
typedef struct
{
	char pipeName[80];      /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
	int  networkId;
} GLXHyperpipeNetworkSGIX;
typedef struct
{
	char         pipeName[80]; /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
	int          channel;
	unsigned int participationType;
	int          timeSlice;
} GLXHyperpipeConfigSGIX;
typedef struct
{
	char pipeName[80];   /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
	int  srcXOrigin, srcYOrigin, srcWidth, srcHeight;
	int  destXOrigin, destYOrigin, destWidth, destHeight;
} GLXPipeRect;
typedef struct
{
	char pipeName[80];   /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
	int  XOrigin, YOrigin, maxHeight, maxWidth;
} GLXPipeRectLimits;
	#define GLX_EXTENSION_NAME          "GLX"
	#define GLX_PbufferClobber          0
	#define GLX_BufferSwapComplete      1
	#define __GLX_NUMBER_EVENTS         17
	#define GLX_BAD_SCREEN              1
	#define GLX_BAD_ATTRIBUTE           2
	#define GLX_NO_EXTENSION            3
	#define GLX_BAD_VISUAL              4
	#define GLX_BAD_CONTEXT             5
	#define GLX_BAD_VALUE               6
	#define GLX_BAD_ENUM                7
	#define GLX_USE_GL                  1
	#define GLX_BUFFER_SIZE             2
	#define GLX_LEVEL                   3
	#define GLX_RGBA                    4
	#define GLX_DOUBLEBUFFER            5
	#define GLX_STEREO                  6
	#define GLX_AUX_BUFFERS             7
	#define GLX_RED_SIZE                8
	#define GLX_GREEN_SIZE              9
	#define GLX_BLUE_SIZE               10
	#define GLX_ALPHA_SIZE              11
	#define GLX_DEPTH_SIZE              12
	#define GLX_STENCIL_SIZE            13
	#define GLX_ACCUM_RED_SIZE          14
	#define GLX_ACCUM_GREEN_SIZE        15
	#define GLX_ACCUM_BLUE_SIZE         16
	#define GLX_ACCUM_ALPHA_SIZE        17
	#define GLX_VENDOR                  0x1
	#define GLX_VERSION                 0x2
	#define GLX_EXTENSIONS              0x3
	#define GLX_WINDOW_BIT              0x00000001
	#define GLX_PIXMAP_BIT              0x00000002
	#define GLX_PBUFFER_BIT             0x00000004
	#define GLX_RGBA_BIT                0x00000001
	#define GLX_COLOR_INDEX_BIT         0x00000002
	#define GLX_PBUFFER_CLOBBER_MASK    0x08000000
	#define GLX_FRONT_LEFT_BUFFER_BIT   0x00000001
	#define GLX_FRONT_RIGHT_BUFFER_BIT  0x00000002
	#define GLX_BACK_LEFT_BUFFER_BIT    0x00000004
	#define GLX_BACK_RIGHT_BUFFER_BIT   0x00000008
	#define GLX_AUX_BUFFERS_BIT         0x00000010
	#define GLX_DEPTH_BUFFER_BIT        0x00000020
	#define GLX_STENCIL_BUFFER_BIT      0x00000040
	#define GLX_ACCUM_BUFFER_BIT        0x00000080
	#define GLX_CONFIG_CAVEAT           0x20
	#define GLX_X_VISUAL_TYPE           0x22
	#define GLX_TRANSPARENT_TYPE        0x23
	#define GLX_TRANSPARENT_INDEX_VALUE 0x24
	#define GLX_TRANSPARENT_RED_VALUE   0x25
	#define GLX_TRANSPARENT_GREEN_VALUE 0x26
	#define GLX_TRANSPARENT_BLUE_VALUE  0x27
	#define GLX_TRANSPARENT_ALPHA_VALUE 0x28
	#define GLX_DONT_CARE               0xFFFFFFFF
	#define GLX_NONE                    0x8000
	#define GLX_SLOW_CONFIG             0x8001
	#define GLX_TRUE_COLOR              0x8002
	#define GLX_DIRECT_COLOR            0x8003
	#define GLX_PSEUDO_COLOR            0x8004
	#define GLX_STATIC_COLOR            0x8005
	#define GLX_GRAY_SCALE              0x8006
	#define GLX_STATIC_GRAY             0x8007
	#define GLX_TRANSPARENT_RGB         0x8008
	#define GLX_TRANSPARENT_INDEX       0x8009
	#define GLX_VISUAL_ID               0x800B
	#define GLX_SCREEN                  0x800C
	#define GLX_NON_CONFORMANT_CONFIG   0x800D
	#define GLX_DRAWABLE_TYPE           0x8010
	#define GLX_RENDER_TYPE             0x8011
	#define GLX_X_RENDERABLE            0x8012
	#define GLX_FBCONFIG_ID             0x8013
	#define GLX_RGBA_TYPE               0x8014
	#define GLX_COLOR_INDEX_TYPE        0x8015
	#define GLX_MAX_PBUFFER_WIDTH       0x8016
	#define GLX_MAX_PBUFFER_HEIGHT      0x8017
	#define GLX_MAX_PBUFFER_PIXELS      0x8018
	#define GLX_PRESERVED_CONTENTS      0x801B
	#define GLX_LARGEST_PBUFFER         0x801C
	#define GLX_WIDTH                   0x801D
	#define GLX_HEIGHT                  0x801E
	#define GLX_EVENT_MASK              0x801F
	#define GLX_DAMAGED                 0x8020
	#define GLX_SAVED                   0x8021
	#define GLX_WINDOW                  0x8022
	#define GLX_PBUFFER                 0x8023
	#define GLX_PBUFFER_HEIGHT          0x8040
	#define GLX_PBUFFER_WIDTH           0x8041
	#define GLX_SAMPLE_BUFFERS          100000
	#define GLX_SAMPLES                 100001
	#ifndef GLX_VERSION_1_0
		#define GLX_VERSION_1_0           1
GLAPI int GLAD_GLX_VERSION_1_0;
typedef XVisualInfo* (APIENTRYP PFNGLXCHOOSEVISUALPROC)(Display*, int, int*);
GLAPI PFNGLXCHOOSEVISUALPROC glad_glXChooseVisual;
typedef GLXContext (APIENTRYP   PFNGLXCREATECONTEXTPROC)(Display*, XVisualInfo*, GLXContext, Bool);
GLAPI PFNGLXCREATECONTEXTPROC glad_glXCreateContext;
typedef void (APIENTRYP         PFNGLXDESTROYCONTEXTPROC)(Display*, GLXContext);
GLAPI PFNGLXDESTROYCONTEXTPROC glad_glXDestroyContext;
typedef Bool (APIENTRYP         PFNGLXMAKECURRENTPROC)(Display*, GLXDrawable, GLXContext);
GLAPI PFNGLXMAKECURRENTPROC glad_glXMakeCurrent;
typedef void (APIENTRYP         PFNGLXCOPYCONTEXTPROC)(Display*, GLXContext, GLXContext, unsigned long);
GLAPI PFNGLXCOPYCONTEXTPROC glad_glXCopyContext;
typedef void (APIENTRYP         PFNGLXSWAPBUFFERSPROC)(Display*, GLXDrawable);
GLAPI PFNGLXSWAPBUFFERSPROC glad_glXSwapBuffers;
typedef GLXPixmap (APIENTRYP    PFNGLXCREATEGLXPIXMAPPROC)(Display*, XVisualInfo*, Pixmap);
GLAPI PFNGLXCREATEGLXPIXMAPPROC glad_glXCreateGLXPixmap;
typedef void (APIENTRYP         PFNGLXDESTROYGLXPIXMAPPROC)(Display*, GLXPixmap);
GLAPI PFNGLXDESTROYGLXPIXMAPPROC glad_glXDestroyGLXPixmap;
typedef Bool (APIENTRYP         PFNGLXQUERYEXTENSIONPROC)(Display*, int*, int*);
GLAPI PFNGLXQUERYEXTENSIONPROC glad_glXQueryExtension;
typedef Bool (APIENTRYP         PFNGLXQUERYVERSIONPROC)(Display*, int*, int*);
GLAPI PFNGLXQUERYVERSIONPROC glad_glXQueryVersion;
typedef Bool (APIENTRYP         PFNGLXISDIRECTPROC)(Display*, GLXContext);
GLAPI PFNGLXISDIRECTPROC glad_glXIsDirect;
typedef int (APIENTRYP          PFNGLXGETCONFIGPROC)(Display*, XVisualInfo*, int, int*);
GLAPI PFNGLXGETCONFIGPROC glad_glXGetConfig;
typedef GLXContext (APIENTRYP   PFNGLXGETCURRENTCONTEXTPROC)();
GLAPI PFNGLXGETCURRENTCONTEXTPROC glad_glXGetCurrentContext;
typedef GLXDrawable (APIENTRYP  PFNGLXGETCURRENTDRAWABLEPROC)();
GLAPI PFNGLXGETCURRENTDRAWABLEPROC glad_glXGetCurrentDrawable;
typedef void (APIENTRYP         PFNGLXWAITGLPROC)();
GLAPI PFNGLXWAITGLPROC glad_glXWaitGL;
typedef void (APIENTRYP         PFNGLXWAITXPROC)();
GLAPI PFNGLXWAITXPROC glad_glXWaitX;
typedef void (APIENTRYP         PFNGLXUSEXFONTPROC)(Font, int, int, int);
GLAPI PFNGLXUSEXFONTPROC glad_glXUseXFont;
	#endif
	#ifndef GLX_VERSION_1_1
		#define GLX_VERSION_1_1 1
GLAPI int GLAD_GLX_VERSION_1_1;
typedef const char* (APIENTRYP PFNGLXQUERYEXTENSIONSSTRINGPROC)(Display*, int);
GLAPI PFNGLXQUERYEXTENSIONSSTRINGPROC glad_glXQueryExtensionsString;
typedef const char* (APIENTRYP PFNGLXQUERYSERVERSTRINGPROC)(Display*, int, int);
GLAPI PFNGLXQUERYSERVERSTRINGPROC glad_glXQueryServerString;
typedef const char* (APIENTRYP PFNGLXGETCLIENTSTRINGPROC)(Display*, int);
GLAPI PFNGLXGETCLIENTSTRINGPROC glad_glXGetClientString;
	#endif
	#ifndef GLX_VERSION_1_2
		#define GLX_VERSION_1_2 1
GLAPI int GLAD_GLX_VERSION_1_2;
typedef Display* (APIENTRYP PFNGLXGETCURRENTDISPLAYPROC)();
GLAPI PFNGLXGETCURRENTDISPLAYPROC glad_glXGetCurrentDisplay;
	#endif
	#ifndef GLX_VERSION_1_3
		#define GLX_VERSION_1_3 1
GLAPI int GLAD_GLX_VERSION_1_3;
typedef GLXFBConfig* (APIENTRYP PFNGLXGETFBCONFIGSPROC)(Display*, int, int*);
GLAPI PFNGLXGETFBCONFIGSPROC glad_glXGetFBConfigs;
typedef GLXFBConfig* (APIENTRYP PFNGLXCHOOSEFBCONFIGPROC)(Display*, int, const int*, int*);
GLAPI PFNGLXCHOOSEFBCONFIGPROC glad_glXChooseFBConfig;
typedef int (APIENTRYP          PFNGLXGETFBCONFIGATTRIBPROC)(Display*, GLXFBConfig, int, int*);
GLAPI PFNGLXGETFBCONFIGATTRIBPROC glad_glXGetFBConfigAttrib;
typedef XVisualInfo* (APIENTRYP PFNGLXGETVISUALFROMFBCONFIGPROC)(Display*, GLXFBConfig);
GLAPI PFNGLXGETVISUALFROMFBCONFIGPROC glad_glXGetVisualFromFBConfig;
typedef GLXWindow (APIENTRYP    PFNGLXCREATEWINDOWPROC)(Display*, GLXFBConfig, Window, const int*);
GLAPI PFNGLXCREATEWINDOWPROC glad_glXCreateWindow;
typedef void (APIENTRYP         PFNGLXDESTROYWINDOWPROC)(Display*, GLXWindow);
GLAPI PFNGLXDESTROYWINDOWPROC glad_glXDestroyWindow;
typedef GLXPixmap (APIENTRYP    PFNGLXCREATEPIXMAPPROC)(Display*, GLXFBConfig, Pixmap, const int*);
GLAPI PFNGLXCREATEPIXMAPPROC glad_glXCreatePixmap;
typedef void (APIENTRYP         PFNGLXDESTROYPIXMAPPROC)(Display*, GLXPixmap);
GLAPI PFNGLXDESTROYPIXMAPPROC glad_glXDestroyPixmap;
typedef GLXPbuffer (APIENTRYP   PFNGLXCREATEPBUFFERPROC)(Display*, GLXFBConfig, const int*);
GLAPI PFNGLXCREATEPBUFFERPROC glad_glXCreatePbuffer;
typedef void (APIENTRYP         PFNGLXDESTROYPBUFFERPROC)(Display*, GLXPbuffer);
GLAPI PFNGLXDESTROYPBUFFERPROC glad_glXDestroyPbuffer;
typedef void (APIENTRYP         PFNGLXQUERYDRAWABLEPROC)(Display*, GLXDrawable, int, unsigned int*);
GLAPI PFNGLXQUERYDRAWABLEPROC glad_glXQueryDrawable;
typedef GLXContext (APIENTRYP   PFNGLXCREATENEWCONTEXTPROC)(Display*, GLXFBConfig, int, GLXContext, Bool);
GLAPI PFNGLXCREATENEWCONTEXTPROC glad_glXCreateNewContext;
typedef Bool (APIENTRYP         PFNGLXMAKECONTEXTCURRENTPROC)(Display*, GLXDrawable, GLXDrawable, GLXContext);
GLAPI PFNGLXMAKECONTEXTCURRENTPROC glad_glXMakeContextCurrent;
typedef GLXDrawable (APIENTRYP  PFNGLXGETCURRENTREADDRAWABLEPROC)();
GLAPI PFNGLXGETCURRENTREADDRAWABLEPROC glad_glXGetCurrentReadDrawable;
typedef int (APIENTRYP          PFNGLXQUERYCONTEXTPROC)(Display*, GLXContext, int, int*);
GLAPI PFNGLXQUERYCONTEXTPROC glad_glXQueryContext;
typedef void (APIENTRYP         PFNGLXSELECTEVENTPROC)(Display*, GLXDrawable, unsigned long);
GLAPI PFNGLXSELECTEVENTPROC glad_glXSelectEvent;
typedef void (APIENTRYP         PFNGLXGETSELECTEDEVENTPROC)(Display*, GLXDrawable, unsigned long*);
GLAPI PFNGLXGETSELECTEDEVENTPROC glad_glXGetSelectedEvent;
	#endif
	#ifndef GLX_VERSION_1_4
		#define GLX_VERSION_1_4 1
GLAPI int GLAD_GLX_VERSION_1_4;
typedef __GLXextFuncPtr (APIENTRYP PFNGLXGETPROCADDRESSPROC)(const GLubyte*);
GLAPI PFNGLXGETPROCADDRESSPROC glad_glXGetProcAddress;
	#endif
	#if defined(__cplusplus) && defined(GLAD_INSTRUMENT_CALL)
struct glad_tag_glXGetSelectedEvent {};
inline void glad_wrapped_glXGetSelectedEvent(Display* _dpy, GLXDrawable _draw, unsigned long* _event_mask) { GLAD_INSTRUMENT_CALL(glXGetSelectedEvent, _dpy, _draw, _event_mask); }
		#define glXGetSelectedEvent glad_wrapped_glXGetSelectedEvent
struct glad_tag_glXQueryExtension {};
inline Bool glad_wrapped_glXQueryExtension(Display* _dpy, int* _errorb, int* _event) { return GLAD_INSTRUMENT_CALL(glXQueryExtension, _dpy, _errorb, _event); }
		#define glXQueryExtension glad_wrapped_glXQueryExtension
struct glad_tag_glXMakeCurrent {};
inline Bool glad_wrapped_glXMakeCurrent(Display* _dpy, GLXDrawable _drawable, GLXContext _ctx) { return GLAD_INSTRUMENT_CALL(glXMakeCurrent, _dpy, _drawable, _ctx); }
		#define glXMakeCurrent glad_wrapped_glXMakeCurrent
struct glad_tag_glXSelectEvent {};
inline void glad_wrapped_glXSelectEvent(Display* _dpy, GLXDrawable _draw, unsigned long _event_mask) { GLAD_INSTRUMENT_CALL(glXSelectEvent, _dpy, _draw, _event_mask); }
		#define glXSelectEvent glad_wrapped_glXSelectEvent
struct glad_tag_glXCreateContext {};
inline GLXContext glad_wrapped_glXCreateContext(Display* _dpy, XVisualInfo* _vis, GLXContext _shareList, Bool _direct) { return GLAD_INSTRUMENT_CALL(glXCreateContext, _dpy, _vis, _shareList, _direct); }
		#define glXCreateContext glad_wrapped_glXCreateContext
struct glad_tag_glXCreateGLXPixmap {};
inline GLXPixmap glad_wrapped_glXCreateGLXPixmap(Display* _dpy, XVisualInfo* _visual, Pixmap _pixmap) { return GLAD_INSTRUMENT_CALL(glXCreateGLXPixmap, _dpy, _visual, _pixmap); }
		#define glXCreateGLXPixmap glad_wrapped_glXCreateGLXPixmap
struct glad_tag_glXQueryVersion {};
inline Bool glad_wrapped_glXQueryVersion(Display* _dpy, int* _maj, int* _min) { return GLAD_INSTRUMENT_CALL(glXQueryVersion, _dpy, _maj, _min); }
		#define glXQueryVersion glad_wrapped_glXQueryVersion
struct glad_tag_glXGetCurrentReadDrawable {};
inline GLXDrawable glad_wrapped_glXGetCurrentReadDrawable() { return GLAD_INSTRUMENT_CALL(glXGetCurrentReadDrawable); }
		#define glXGetCurrentReadDrawable glad_wrapped_glXGetCurrentReadDrawable
struct glad_tag_glXDestroyPixmap {};
inline void glad_wrapped_glXDestroyPixmap(Display* _dpy, GLXPixmap _pixmap) { GLAD_INSTRUMENT_CALL(glXDestroyPixmap, _dpy, _pixmap); }
		#define glXDestroyPixmap glad_wrapped_glXDestroyPixmap
struct glad_tag_glXGetCurrentContext {};
inline GLXContext glad_wrapped_glXGetCurrentContext() { return GLAD_INSTRUMENT_CALL(glXGetCurrentContext); }
		#define glXGetCurrentContext glad_wrapped_glXGetCurrentContext
struct glad_tag_glXGetProcAddress {};
inline __GLXextFuncPtr glad_wrapped_glXGetProcAddress(const GLubyte* _procName) { return GLAD_INSTRUMENT_CALL(glXGetProcAddress, _procName); }
		#define glXGetProcAddress glad_wrapped_glXGetProcAddress
struct glad_tag_glXWaitGL {};
inline void glad_wrapped_glXWaitGL() { GLAD_INSTRUMENT_CALL(glXWaitGL); }
		#define glXWaitGL glad_wrapped_glXWaitGL
struct glad_tag_glXIsDirect {};
inline Bool glad_wrapped_glXIsDirect(Display* _dpy, GLXContext _ctx) { return GLAD_INSTRUMENT_CALL(glXIsDirect, _dpy, _ctx); }
		#define glXIsDirect glad_wrapped_glXIsDirect
struct glad_tag_glXDestroyWindow {};
inline void glad_wrapped_glXDestroyWindow(Display* _dpy, GLXWindow _win) { GLAD_INSTRUMENT_CALL(glXDestroyWindow, _dpy, _win); }
		#define glXDestroyWindow glad_wrapped_glXDestroyWindow
struct glad_tag_glXCreateWindow {};
inline GLXWindow glad_wrapped_glXCreateWindow(Display* _dpy, GLXFBConfig _config, Window _win, const int* _attrib_list) { return GLAD_INSTRUMENT_CALL(glXCreateWindow, _dpy, _config, _win, _attrib_list); }
		#define glXCreateWindow glad_wrapped_glXCreateWindow
struct glad_tag_glXCopyContext {};
inline void glad_wrapped_glXCopyContext(Display* _dpy, GLXContext _src, GLXContext _dst, unsigned long _mask) { GLAD_INSTRUMENT_CALL(glXCopyContext, _dpy, _src, _dst, _mask); }
		#define glXCopyContext glad_wrapped_glXCopyContext
struct glad_tag_glXCreatePbuffer {};
inline GLXPbuffer glad_wrapped_glXCreatePbuffer(Display* _dpy, GLXFBConfig _config, const int* _attrib_list) { return GLAD_INSTRUMENT_CALL(glXCreatePbuffer, _dpy, _config, _attrib_list); }
		#define glXCreatePbuffer glad_wrapped_glXCreatePbuffer
struct glad_tag_glXSwapBuffers {};
inline void glad_wrapped_glXSwapBuffers(Display* _dpy, GLXDrawable _drawable) { GLAD_INSTRUMENT_CALL(glXSwapBuffers, _dpy, _drawable); }
		#define glXSwapBuffers glad_wrapped_glXSwapBuffers
struct glad_tag_glXGetCurrentDisplay {};
inline Display* glad_wrapped_glXGetCurrentDisplay() { return GLAD_INSTRUMENT_CALL(glXGetCurrentDisplay); }
		#define glXGetCurrentDisplay glad_wrapped_glXGetCurrentDisplay
struct glad_tag_glXGetCurrentDrawable {};
inline GLXDrawable glad_wrapped_glXGetCurrentDrawable() { return GLAD_INSTRUMENT_CALL(glXGetCurrentDrawable); }
		#define glXGetCurrentDrawable glad_wrapped_glXGetCurrentDrawable
struct glad_tag_glXQueryContext {};
inline int glad_wrapped_glXQueryContext(Display* _dpy, GLXContext _ctx, int _attribute, int* _value) { return GLAD_INSTRUMENT_CALL(glXQueryContext, _dpy, _ctx, _attribute, _value); }
		#define glXQueryContext glad_wrapped_glXQueryContext
struct glad_tag_glXChooseVisual {};
inline XVisualInfo* glad_wrapped_glXChooseVisual(Display* _dpy, int _screen, int* _attribList) { return GLAD_INSTRUMENT_CALL(glXChooseVisual, _dpy, _screen, _attribList); }
		#define glXChooseVisual glad_wrapped_glXChooseVisual
struct glad_tag_glXQueryServerString {};
inline const char* glad_wrapped_glXQueryServerString(Display* _dpy, int _screen, int _name) { return GLAD_INSTRUMENT_CALL(glXQueryServerString, _dpy, _screen, _name); }
		#define glXQueryServerString glad_wrapped_glXQueryServerString
struct glad_tag_glXDestroyContext {};
inline void glad_wrapped_glXDestroyContext(Display* _dpy, GLXContext _ctx) { GLAD_INSTRUMENT_CALL(glXDestroyContext, _dpy, _ctx); }
		#define glXDestroyContext glad_wrapped_glXDestroyContext
struct glad_tag_glXDestroyGLXPixmap {};
inline void glad_wrapped_glXDestroyGLXPixmap(Display* _dpy, GLXPixmap _pixmap) { GLAD_INSTRUMENT_CALL(glXDestroyGLXPixmap, _dpy, _pixmap); }
		#define glXDestroyGLXPixmap glad_wrapped_glXDestroyGLXPixmap
struct glad_tag_glXGetFBConfigAttrib {};
inline int glad_wrapped_glXGetFBConfigAttrib(Display* _dpy, GLXFBConfig _config, int _attribute, int* _value) { return GLAD_INSTRUMENT_CALL(glXGetFBConfigAttrib, _dpy, _config, _attribute, _value); }
		#define glXGetFBConfigAttrib glad_wrapped_glXGetFBConfigAttrib
struct glad_tag_glXUseXFont {};
inline void glad_wrapped_glXUseXFont(Font _font, int _first, int _count, int _list) { GLAD_INSTRUMENT_CALL(glXUseXFont, _font, _first, _count, _list); }
		#define glXUseXFont glad_wrapped_glXUseXFont
struct glad_tag_glXDestroyPbuffer {};
inline void glad_wrapped_glXDestroyPbuffer(Display* _dpy, GLXPbuffer _pbuf) { GLAD_INSTRUMENT_CALL(glXDestroyPbuffer, _dpy, _pbuf); }
		#define glXDestroyPbuffer glad_wrapped_glXDestroyPbuffer
struct glad_tag_glXChooseFBConfig {};
inline GLXFBConfig* glad_wrapped_glXChooseFBConfig(Display* _dpy, int _screen, const int* _attrib_list, int* _nelements) { return GLAD_INSTRUMENT_CALL(glXChooseFBConfig, _dpy, _screen, _attrib_list, _nelements); }
		#define glXChooseFBConfig glad_wrapped_glXChooseFBConfig
struct glad_tag_glXCreateNewContext {};
inline GLXContext glad_wrapped_glXCreateNewContext(Display* _dpy, GLXFBConfig _config, int _render_type, GLXContext _share_list, Bool _direct) { return GLAD_INSTRUMENT_CALL(glXCreateNewContext, _dpy, _config, _render_type, _share_list, _direct); }
		#define glXCreateNewContext glad_wrapped_glXCreateNewContext
struct glad_tag_glXMakeContextCurrent {};
inline Bool glad_wrapped_glXMakeContextCurrent(Display* _dpy, GLXDrawable _draw, GLXDrawable _read, GLXContext _ctx) { return GLAD_INSTRUMENT_CALL(glXMakeContextCurrent, _dpy, _draw, _read, _ctx); }
		#define glXMakeContextCurrent glad_wrapped_glXMakeContextCurrent
struct glad_tag_glXGetConfig {};
inline int glad_wrapped_glXGetConfig(Display* _dpy, XVisualInfo* _visual, int _attrib, int* _value) { return GLAD_INSTRUMENT_CALL(glXGetConfig, _dpy, _visual, _attrib, _value); }
		#define glXGetConfig glad_wrapped_glXGetConfig
struct glad_tag_glXGetFBConfigs {};
inline GLXFBConfig* glad_wrapped_glXGetFBConfigs(Display* _dpy, int _screen, int* _nelements) { return GLAD_INSTRUMENT_CALL(glXGetFBConfigs, _dpy, _screen, _nelements); }
		#define glXGetFBConfigs glad_wrapped_glXGetFBConfigs
struct glad_tag_glXCreatePixmap {};
inline GLXPixmap glad_wrapped_glXCreatePixmap(Display* _dpy, GLXFBConfig _config, Pixmap _pixmap, const int* _attrib_list) { return GLAD_INSTRUMENT_CALL(glXCreatePixmap, _dpy, _config, _pixmap, _attrib_list); }
		#define glXCreatePixmap glad_wrapped_glXCreatePixmap
struct glad_tag_glXWaitX {};
inline void glad_wrapped_glXWaitX() { GLAD_INSTRUMENT_CALL(glXWaitX); }
		#define glXWaitX glad_wrapped_glXWaitX
struct glad_tag_glXGetVisualFromFBConfig {};
inline XVisualInfo* glad_wrapped_glXGetVisualFromFBConfig(Display* _dpy, GLXFBConfig _config) { return GLAD_INSTRUMENT_CALL(glXGetVisualFromFBConfig, _dpy, _config); }
		#define glXGetVisualFromFBConfig glad_wrapped_glXGetVisualFromFBConfig
struct glad_tag_glXQueryDrawable {};
inline void glad_wrapped_glXQueryDrawable(Display* _dpy, GLXDrawable _draw, int _attribute, unsigned int* _value) { GLAD_INSTRUMENT_CALL(glXQueryDrawable, _dpy, _draw, _attribute, _value); }
		#define glXQueryDrawable glad_wrapped_glXQueryDrawable
struct glad_tag_glXQueryExtensionsString {};
inline const char* glad_wrapped_glXQueryExtensionsString(Display* _dpy, int _screen) { return GLAD_INSTRUMENT_CALL(glXQueryExtensionsString, _dpy, _screen); }
		#define glXQueryExtensionsString glad_wrapped_glXQueryExtensionsString
struct glad_tag_glXGetClientString {};
inline const char* glad_wrapped_glXGetClientString(Display* _dpy, int _name) { return GLAD_INSTRUMENT_CALL(glXGetClientString, _dpy, _name); }
		#define glXGetClientString        glad_wrapped_glXGetClientString
	#else
		#define glXGetSelectedEvent       glad_glXGetSelectedEvent
		#define glXQueryExtension         glad_glXQueryExtension
		#define glXMakeCurrent            glad_glXMakeCurrent
		#define glXSelectEvent            glad_glXSelectEvent
		#define glXCreateContext          glad_glXCreateContext
		#define glXCreateGLXPixmap        glad_glXCreateGLXPixmap
		#define glXQueryVersion           glad_glXQueryVersion
		#define glXGetCurrentReadDrawable glad_glXGetCurrentReadDrawable
		#define glXDestroyPixmap          glad_glXDestroyPixmap
		#define glXGetCurrentContext      glad_glXGetCurrentContext
		#define glXGetProcAddress         glad_glXGetProcAddress
		#define glXWaitGL                 glad_glXWaitGL
		#define glXIsDirect               glad_glXIsDirect
		#define glXDestroyWindow          glad_glXDestroyWindow
		#define glXCreateWindow           glad_glXCreateWindow
		#define glXCopyContext            glad_glXCopyContext
		#define glXCreatePbuffer          glad_glXCreatePbuffer
		#define glXSwapBuffers            glad_glXSwapBuffers
		#define glXGetCurrentDisplay      glad_glXGetCurrentDisplay
		#define glXGetCurrentDrawable     glad_glXGetCurrentDrawable
		#define glXQueryContext           glad_glXQueryContext
		#define glXChooseVisual           glad_glXChooseVisual
		#define glXQueryServerString      glad_glXQueryServerString
		#define glXDestroyContext         glad_glXDestroyContext
		#define glXDestroyGLXPixmap       glad_glXDestroyGLXPixmap
		#define glXGetFBConfigAttrib      glad_glXGetFBConfigAttrib
		#define glXUseXFont               glad_glXUseXFont
		#define glXDestroyPbuffer         glad_glXDestroyPbuffer
		#define glXChooseFBConfig         glad_glXChooseFBConfig
		#define glXCreateNewContext       glad_glXCreateNewContext
		#define glXMakeContextCurrent     glad_glXMakeContextCurrent
		#define glXGetConfig              glad_glXGetConfig
		#define glXGetFBConfigs           glad_glXGetFBConfigs
		#define glXCreatePixmap           glad_glXCreatePixmap
		#define glXWaitX                  glad_glXWaitX
		#define glXGetVisualFromFBConfig  glad_glXGetVisualFromFBConfig
		#define glXQueryDrawable          glad_glXQueryDrawable
		#define glXQueryExtensionsString  glad_glXQueryExtensionsString
		#define glXGetClientString        glad_glXGetClientString
	#endif

	#ifdef __cplusplus
}
	#endif

#endif
