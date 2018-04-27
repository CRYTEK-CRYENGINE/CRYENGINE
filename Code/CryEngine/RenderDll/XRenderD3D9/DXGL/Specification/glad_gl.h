// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __glad_h_

#ifdef __gl_h_
	#error OpenGL header already included, remove this include, glad already provides it
#endif

#define __glad_h_
#define __gl_h_

#if defined(_WIN32) && !defined(APIENTRY) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN 1
	#endif
	#include <windows.h>
#endif

#ifndef APIENTRY
	#define APIENTRY
#endif
#ifndef APIENTRYP
	#define APIENTRYP APIENTRY *
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct gladGLversionStruct
{
	int major;
	int minor;
};

extern struct gladGLversionStruct GLVersion;

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

GLAPI int gladLoadGL(void);

GLAPI int gladLoadGLLoader(GLADloadproc);

#include <stddef.h>
#include "khrplatform.h"
#ifndef GLEXT_64_TYPES_DEFINED
/* This code block is duplicated in glxext.h, so must be protected */
	#define GLEXT_64_TYPES_DEFINED
/* Define int32_t, int64_t, and uint64_t types for UST/MSC */
/* (as used in the GL_EXT_timer_query extension). */
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
typedef unsigned int     GLenum;
typedef unsigned char    GLboolean;
typedef unsigned int     GLbitfield;
typedef void             GLvoid;
typedef signed char      GLbyte;
typedef short            GLshort;
typedef int              GLint;
typedef int              GLclampx;
typedef unsigned char    GLubyte;
typedef unsigned short   GLushort;
typedef unsigned int     GLuint;
typedef int              GLsizei;
typedef float            GLfloat;
typedef float            GLclampf;
typedef double           GLdouble;
typedef double           GLclampd;
typedef void*            GLeglImageOES;
typedef char             GLchar;
typedef char             GLcharARB;
#ifdef __APPLE__
typedef void*            GLhandleARB;
#else
typedef unsigned int     GLhandleARB;
#endif
typedef unsigned short   GLhalfARB;
typedef unsigned short   GLhalf;
typedef GLint            GLfixed;
typedef ptrdiff_t        GLintptr;
typedef ptrdiff_t        GLsizeiptr;
typedef int64_t          GLint64;
typedef uint64_t         GLuint64;
typedef ptrdiff_t        GLintptrARB;
typedef ptrdiff_t        GLsizeiptrARB;
typedef int64_t          GLint64EXT;
typedef uint64_t         GLuint64EXT;
typedef struct __GLsync* GLsync;
struct _cl_context;
struct _cl_event;
typedef void (APIENTRY * GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
typedef void (APIENTRY * GLDEBUGPROCARB)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
typedef void (APIENTRY * GLDEBUGPROCKHR)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
typedef void (APIENTRY * GLDEBUGPROCAMD)(GLuint id, GLenum category, GLenum severity, GLsizei length, const GLchar* message, void* userParam);
typedef unsigned short   GLhalfNV;
typedef GLintptr         GLvdpauSurfaceNV;
#define GL_DEPTH_BUFFER_BIT                                           0x00000100
#define GL_STENCIL_BUFFER_BIT                                         0x00000400
#define GL_COLOR_BUFFER_BIT                                           0x00004000
#define GL_FALSE                                                      0
#define GL_TRUE                                                       1
#define GL_POINTS                                                     0x0000
#define GL_LINES                                                      0x0001
#define GL_LINE_LOOP                                                  0x0002
#define GL_LINE_STRIP                                                 0x0003
#define GL_TRIANGLES                                                  0x0004
#define GL_TRIANGLE_STRIP                                             0x0005
#define GL_TRIANGLE_FAN                                               0x0006
#define GL_NEVER                                                      0x0200
#define GL_LESS                                                       0x0201
#define GL_EQUAL                                                      0x0202
#define GL_LEQUAL                                                     0x0203
#define GL_GREATER                                                    0x0204
#define GL_NOTEQUAL                                                   0x0205
#define GL_GEQUAL                                                     0x0206
#define GL_ALWAYS                                                     0x0207
#define GL_ZERO                                                       0
#define GL_ONE                                                        1
#define GL_SRC_COLOR                                                  0x0300
#define GL_ONE_MINUS_SRC_COLOR                                        0x0301
#define GL_SRC_ALPHA                                                  0x0302
#define GL_ONE_MINUS_SRC_ALPHA                                        0x0303
#define GL_DST_ALPHA                                                  0x0304
#define GL_ONE_MINUS_DST_ALPHA                                        0x0305
#define GL_DST_COLOR                                                  0x0306
#define GL_ONE_MINUS_DST_COLOR                                        0x0307
#define GL_SRC_ALPHA_SATURATE                                         0x0308
#define GL_NONE                                                       0
#define GL_FRONT_LEFT                                                 0x0400
#define GL_FRONT_RIGHT                                                0x0401
#define GL_BACK_LEFT                                                  0x0402
#define GL_BACK_RIGHT                                                 0x0403
#define GL_FRONT                                                      0x0404
#define GL_BACK                                                       0x0405
#define GL_LEFT                                                       0x0406
#define GL_RIGHT                                                      0x0407
#define GL_FRONT_AND_BACK                                             0x0408
#define GL_NO_ERROR                                                   0
#define GL_INVALID_ENUM                                               0x0500
#define GL_INVALID_VALUE                                              0x0501
#define GL_INVALID_OPERATION                                          0x0502
#define GL_OUT_OF_MEMORY                                              0x0505
#define GL_CW                                                         0x0900
#define GL_CCW                                                        0x0901
#define GL_POINT_SIZE                                                 0x0B11
#define GL_POINT_SIZE_RANGE                                           0x0B12
#define GL_POINT_SIZE_GRANULARITY                                     0x0B13
#define GL_LINE_SMOOTH                                                0x0B20
#define GL_LINE_WIDTH                                                 0x0B21
#define GL_LINE_WIDTH_RANGE                                           0x0B22
#define GL_LINE_WIDTH_GRANULARITY                                     0x0B23
#define GL_POLYGON_MODE                                               0x0B40
#define GL_POLYGON_SMOOTH                                             0x0B41
#define GL_CULL_FACE                                                  0x0B44
#define GL_CULL_FACE_MODE                                             0x0B45
#define GL_FRONT_FACE                                                 0x0B46
#define GL_DEPTH_RANGE                                                0x0B70
#define GL_DEPTH_TEST                                                 0x0B71
#define GL_DEPTH_WRITEMASK                                            0x0B72
#define GL_DEPTH_CLEAR_VALUE                                          0x0B73
#define GL_DEPTH_FUNC                                                 0x0B74
#define GL_STENCIL_TEST                                               0x0B90
#define GL_STENCIL_CLEAR_VALUE                                        0x0B91
#define GL_STENCIL_FUNC                                               0x0B92
#define GL_STENCIL_VALUE_MASK                                         0x0B93
#define GL_STENCIL_FAIL                                               0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL                                    0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS                                    0x0B96
#define GL_STENCIL_REF                                                0x0B97
#define GL_STENCIL_WRITEMASK                                          0x0B98
#define GL_VIEWPORT                                                   0x0BA2
#define GL_DITHER                                                     0x0BD0
#define GL_BLEND_DST                                                  0x0BE0
#define GL_BLEND_SRC                                                  0x0BE1
#define GL_BLEND                                                      0x0BE2
#define GL_LOGIC_OP_MODE                                              0x0BF0
#define GL_COLOR_LOGIC_OP                                             0x0BF2
#define GL_DRAW_BUFFER                                                0x0C01
#define GL_READ_BUFFER                                                0x0C02
#define GL_SCISSOR_BOX                                                0x0C10
#define GL_SCISSOR_TEST                                               0x0C11
#define GL_COLOR_CLEAR_VALUE                                          0x0C22
#define GL_COLOR_WRITEMASK                                            0x0C23
#define GL_DOUBLEBUFFER                                               0x0C32
#define GL_STEREO                                                     0x0C33
#define GL_LINE_SMOOTH_HINT                                           0x0C52
#define GL_POLYGON_SMOOTH_HINT                                        0x0C53
#define GL_UNPACK_SWAP_BYTES                                          0x0CF0
#define GL_UNPACK_LSB_FIRST                                           0x0CF1
#define GL_UNPACK_ROW_LENGTH                                          0x0CF2
#define GL_UNPACK_SKIP_ROWS                                           0x0CF3
#define GL_UNPACK_SKIP_PIXELS                                         0x0CF4
#define GL_UNPACK_ALIGNMENT                                           0x0CF5
#define GL_PACK_SWAP_BYTES                                            0x0D00
#define GL_PACK_LSB_FIRST                                             0x0D01
#define GL_PACK_ROW_LENGTH                                            0x0D02
#define GL_PACK_SKIP_ROWS                                             0x0D03
#define GL_PACK_SKIP_PIXELS                                           0x0D04
#define GL_PACK_ALIGNMENT                                             0x0D05
#define GL_MAX_TEXTURE_SIZE                                           0x0D33
#define GL_MAX_VIEWPORT_DIMS                                          0x0D3A
#define GL_SUBPIXEL_BITS                                              0x0D50
#define GL_TEXTURE_1D                                                 0x0DE0
#define GL_TEXTURE_2D                                                 0x0DE1
#define GL_POLYGON_OFFSET_UNITS                                       0x2A00
#define GL_POLYGON_OFFSET_POINT                                       0x2A01
#define GL_POLYGON_OFFSET_LINE                                        0x2A02
#define GL_POLYGON_OFFSET_FILL                                        0x8037
#define GL_POLYGON_OFFSET_FACTOR                                      0x8038
#define GL_TEXTURE_BINDING_1D                                         0x8068
#define GL_TEXTURE_BINDING_2D                                         0x8069
#define GL_TEXTURE_WIDTH                                              0x1000
#define GL_TEXTURE_HEIGHT                                             0x1001
#define GL_TEXTURE_INTERNAL_FORMAT                                    0x1003
#define GL_TEXTURE_BORDER_COLOR                                       0x1004
#define GL_TEXTURE_RED_SIZE                                           0x805C
#define GL_TEXTURE_GREEN_SIZE                                         0x805D
#define GL_TEXTURE_BLUE_SIZE                                          0x805E
#define GL_TEXTURE_ALPHA_SIZE                                         0x805F
#define GL_DONT_CARE                                                  0x1100
#define GL_FASTEST                                                    0x1101
#define GL_NICEST                                                     0x1102
#define GL_BYTE                                                       0x1400
#define GL_UNSIGNED_BYTE                                              0x1401
#define GL_SHORT                                                      0x1402
#define GL_UNSIGNED_SHORT                                             0x1403
#define GL_INT                                                        0x1404
#define GL_UNSIGNED_INT                                               0x1405
#define GL_FLOAT                                                      0x1406
#define GL_DOUBLE                                                     0x140A
#define GL_CLEAR                                                      0x1500
#define GL_AND                                                        0x1501
#define GL_AND_REVERSE                                                0x1502
#define GL_COPY                                                       0x1503
#define GL_AND_INVERTED                                               0x1504
#define GL_NOOP                                                       0x1505
#define GL_XOR                                                        0x1506
#define GL_OR                                                         0x1507
#define GL_NOR                                                        0x1508
#define GL_EQUIV                                                      0x1509
#define GL_INVERT                                                     0x150A
#define GL_OR_REVERSE                                                 0x150B
#define GL_COPY_INVERTED                                              0x150C
#define GL_OR_INVERTED                                                0x150D
#define GL_NAND                                                       0x150E
#define GL_SET                                                        0x150F
#define GL_TEXTURE                                                    0x1702
#define GL_COLOR                                                      0x1800
#define GL_DEPTH                                                      0x1801
#define GL_STENCIL                                                    0x1802
#define GL_STENCIL_INDEX                                              0x1901
#define GL_DEPTH_COMPONENT                                            0x1902
#define GL_RED                                                        0x1903
#define GL_GREEN                                                      0x1904
#define GL_BLUE                                                       0x1905
#define GL_ALPHA                                                      0x1906
#define GL_RGB                                                        0x1907
#define GL_RGBA                                                       0x1908
#define GL_POINT                                                      0x1B00
#define GL_LINE                                                       0x1B01
#define GL_FILL                                                       0x1B02
#define GL_KEEP                                                       0x1E00
#define GL_REPLACE                                                    0x1E01
#define GL_INCR                                                       0x1E02
#define GL_DECR                                                       0x1E03
#define GL_VENDOR                                                     0x1F00
#define GL_RENDERER                                                   0x1F01
#define GL_VERSION                                                    0x1F02
#define GL_EXTENSIONS                                                 0x1F03
#define GL_NEAREST                                                    0x2600
#define GL_LINEAR                                                     0x2601
#define GL_NEAREST_MIPMAP_NEAREST                                     0x2700
#define GL_LINEAR_MIPMAP_NEAREST                                      0x2701
#define GL_NEAREST_MIPMAP_LINEAR                                      0x2702
#define GL_LINEAR_MIPMAP_LINEAR                                       0x2703
#define GL_TEXTURE_MAG_FILTER                                         0x2800
#define GL_TEXTURE_MIN_FILTER                                         0x2801
#define GL_TEXTURE_WRAP_S                                             0x2802
#define GL_TEXTURE_WRAP_T                                             0x2803
#define GL_PROXY_TEXTURE_1D                                           0x8063
#define GL_PROXY_TEXTURE_2D                                           0x8064
#define GL_REPEAT                                                     0x2901
#define GL_R3_G3_B2                                                   0x2A10
#define GL_RGB4                                                       0x804F
#define GL_RGB5                                                       0x8050
#define GL_RGB8                                                       0x8051
#define GL_RGB10                                                      0x8052
#define GL_RGB12                                                      0x8053
#define GL_RGB16                                                      0x8054
#define GL_RGBA2                                                      0x8055
#define GL_RGBA4                                                      0x8056
#define GL_RGB5_A1                                                    0x8057
#define GL_RGBA8                                                      0x8058
#define GL_RGB10_A2                                                   0x8059
#define GL_RGBA12                                                     0x805A
#define GL_RGBA16                                                     0x805B
#define GL_UNSIGNED_BYTE_3_3_2                                        0x8032
#define GL_UNSIGNED_SHORT_4_4_4_4                                     0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1                                     0x8034
#define GL_UNSIGNED_INT_8_8_8_8                                       0x8035
#define GL_UNSIGNED_INT_10_10_10_2                                    0x8036
#define GL_TEXTURE_BINDING_3D                                         0x806A
#define GL_PACK_SKIP_IMAGES                                           0x806B
#define GL_PACK_IMAGE_HEIGHT                                          0x806C
#define GL_UNPACK_SKIP_IMAGES                                         0x806D
#define GL_UNPACK_IMAGE_HEIGHT                                        0x806E
#define GL_TEXTURE_3D                                                 0x806F
#define GL_PROXY_TEXTURE_3D                                           0x8070
#define GL_TEXTURE_DEPTH                                              0x8071
#define GL_TEXTURE_WRAP_R                                             0x8072
#define GL_MAX_3D_TEXTURE_SIZE                                        0x8073
#define GL_UNSIGNED_BYTE_2_3_3_REV                                    0x8362
#define GL_UNSIGNED_SHORT_5_6_5                                       0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV                                   0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4_REV                                 0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV                                 0x8366
#define GL_UNSIGNED_INT_8_8_8_8_REV                                   0x8367
#define GL_UNSIGNED_INT_2_10_10_10_REV                                0x8368
#define GL_BGR                                                        0x80E0
#define GL_BGRA                                                       0x80E1
#define GL_MAX_ELEMENTS_VERTICES                                      0x80E8
#define GL_MAX_ELEMENTS_INDICES                                       0x80E9
#define GL_CLAMP_TO_EDGE                                              0x812F
#define GL_TEXTURE_MIN_LOD                                            0x813A
#define GL_TEXTURE_MAX_LOD                                            0x813B
#define GL_TEXTURE_BASE_LEVEL                                         0x813C
#define GL_TEXTURE_MAX_LEVEL                                          0x813D
#define GL_SMOOTH_POINT_SIZE_RANGE                                    0x0B12
#define GL_SMOOTH_POINT_SIZE_GRANULARITY                              0x0B13
#define GL_SMOOTH_LINE_WIDTH_RANGE                                    0x0B22
#define GL_SMOOTH_LINE_WIDTH_GRANULARITY                              0x0B23
#define GL_ALIASED_LINE_WIDTH_RANGE                                   0x846E
#define GL_TEXTURE0                                                   0x84C0
#define GL_TEXTURE1                                                   0x84C1
#define GL_TEXTURE2                                                   0x84C2
#define GL_TEXTURE3                                                   0x84C3
#define GL_TEXTURE4                                                   0x84C4
#define GL_TEXTURE5                                                   0x84C5
#define GL_TEXTURE6                                                   0x84C6
#define GL_TEXTURE7                                                   0x84C7
#define GL_TEXTURE8                                                   0x84C8
#define GL_TEXTURE9                                                   0x84C9
#define GL_TEXTURE10                                                  0x84CA
#define GL_TEXTURE11                                                  0x84CB
#define GL_TEXTURE12                                                  0x84CC
#define GL_TEXTURE13                                                  0x84CD
#define GL_TEXTURE14                                                  0x84CE
#define GL_TEXTURE15                                                  0x84CF
#define GL_TEXTURE16                                                  0x84D0
#define GL_TEXTURE17                                                  0x84D1
#define GL_TEXTURE18                                                  0x84D2
#define GL_TEXTURE19                                                  0x84D3
#define GL_TEXTURE20                                                  0x84D4
#define GL_TEXTURE21                                                  0x84D5
#define GL_TEXTURE22                                                  0x84D6
#define GL_TEXTURE23                                                  0x84D7
#define GL_TEXTURE24                                                  0x84D8
#define GL_TEXTURE25                                                  0x84D9
#define GL_TEXTURE26                                                  0x84DA
#define GL_TEXTURE27                                                  0x84DB
#define GL_TEXTURE28                                                  0x84DC
#define GL_TEXTURE29                                                  0x84DD
#define GL_TEXTURE30                                                  0x84DE
#define GL_TEXTURE31                                                  0x84DF
#define GL_ACTIVE_TEXTURE                                             0x84E0
#define GL_MULTISAMPLE                                                0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE                                   0x809E
#define GL_SAMPLE_ALPHA_TO_ONE                                        0x809F
#define GL_SAMPLE_COVERAGE                                            0x80A0
#define GL_SAMPLE_BUFFERS                                             0x80A8
#define GL_SAMPLES                                                    0x80A9
#define GL_SAMPLE_COVERAGE_VALUE                                      0x80AA
#define GL_SAMPLE_COVERAGE_INVERT                                     0x80AB
#define GL_TEXTURE_CUBE_MAP                                           0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP                                   0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X                                0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X                                0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y                                0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y                                0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z                                0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z                                0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP                                     0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE                                  0x851C
#define GL_COMPRESSED_RGB                                             0x84ED
#define GL_COMPRESSED_RGBA                                            0x84EE
#define GL_TEXTURE_COMPRESSION_HINT                                   0x84EF
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE                              0x86A0
#define GL_TEXTURE_COMPRESSED                                         0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS                             0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS                                 0x86A3
#define GL_CLAMP_TO_BORDER                                            0x812D
#define GL_BLEND_DST_RGB                                              0x80C8
#define GL_BLEND_SRC_RGB                                              0x80C9
#define GL_BLEND_DST_ALPHA                                            0x80CA
#define GL_BLEND_SRC_ALPHA                                            0x80CB
#define GL_POINT_FADE_THRESHOLD_SIZE                                  0x8128
#define GL_DEPTH_COMPONENT16                                          0x81A5
#define GL_DEPTH_COMPONENT24                                          0x81A6
#define GL_DEPTH_COMPONENT32                                          0x81A7
#define GL_MIRRORED_REPEAT                                            0x8370
#define GL_MAX_TEXTURE_LOD_BIAS                                       0x84FD
#define GL_TEXTURE_LOD_BIAS                                           0x8501
#define GL_INCR_WRAP                                                  0x8507
#define GL_DECR_WRAP                                                  0x8508
#define GL_TEXTURE_DEPTH_SIZE                                         0x884A
#define GL_TEXTURE_COMPARE_MODE                                       0x884C
#define GL_TEXTURE_COMPARE_FUNC                                       0x884D
#define GL_FUNC_ADD                                                   0x8006
#define GL_FUNC_SUBTRACT                                              0x800A
#define GL_FUNC_REVERSE_SUBTRACT                                      0x800B
#define GL_MIN                                                        0x8007
#define GL_MAX                                                        0x8008
#define GL_CONSTANT_COLOR                                             0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR                                   0x8002
#define GL_CONSTANT_ALPHA                                             0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA                                   0x8004
#define GL_BUFFER_SIZE                                                0x8764
#define GL_BUFFER_USAGE                                               0x8765
#define GL_QUERY_COUNTER_BITS                                         0x8864
#define GL_CURRENT_QUERY                                              0x8865
#define GL_QUERY_RESULT                                               0x8866
#define GL_QUERY_RESULT_AVAILABLE                                     0x8867
#define GL_ARRAY_BUFFER                                               0x8892
#define GL_ELEMENT_ARRAY_BUFFER                                       0x8893
#define GL_ARRAY_BUFFER_BINDING                                       0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING                               0x8895
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING                         0x889F
#define GL_READ_ONLY                                                  0x88B8
#define GL_WRITE_ONLY                                                 0x88B9
#define GL_READ_WRITE                                                 0x88BA
#define GL_BUFFER_ACCESS                                              0x88BB
#define GL_BUFFER_MAPPED                                              0x88BC
#define GL_BUFFER_MAP_POINTER                                         0x88BD
#define GL_STREAM_DRAW                                                0x88E0
#define GL_STREAM_READ                                                0x88E1
#define GL_STREAM_COPY                                                0x88E2
#define GL_STATIC_DRAW                                                0x88E4
#define GL_STATIC_READ                                                0x88E5
#define GL_STATIC_COPY                                                0x88E6
#define GL_DYNAMIC_DRAW                                               0x88E8
#define GL_DYNAMIC_READ                                               0x88E9
#define GL_DYNAMIC_COPY                                               0x88EA
#define GL_SAMPLES_PASSED                                             0x8914
#define GL_SRC1_ALPHA                                                 0x8589
#define GL_BLEND_EQUATION_RGB                                         0x8009
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED                                0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE                                   0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE                                 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE                                   0x8625
#define GL_CURRENT_VERTEX_ATTRIB                                      0x8626
#define GL_VERTEX_PROGRAM_POINT_SIZE                                  0x8642
#define GL_VERTEX_ATTRIB_ARRAY_POINTER                                0x8645
#define GL_STENCIL_BACK_FUNC                                          0x8800
#define GL_STENCIL_BACK_FAIL                                          0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL                               0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS                               0x8803
#define GL_MAX_DRAW_BUFFERS                                           0x8824
#define GL_DRAW_BUFFER0                                               0x8825
#define GL_DRAW_BUFFER1                                               0x8826
#define GL_DRAW_BUFFER2                                               0x8827
#define GL_DRAW_BUFFER3                                               0x8828
#define GL_DRAW_BUFFER4                                               0x8829
#define GL_DRAW_BUFFER5                                               0x882A
#define GL_DRAW_BUFFER6                                               0x882B
#define GL_DRAW_BUFFER7                                               0x882C
#define GL_DRAW_BUFFER8                                               0x882D
#define GL_DRAW_BUFFER9                                               0x882E
#define GL_DRAW_BUFFER10                                              0x882F
#define GL_DRAW_BUFFER11                                              0x8830
#define GL_DRAW_BUFFER12                                              0x8831
#define GL_DRAW_BUFFER13                                              0x8832
#define GL_DRAW_BUFFER14                                              0x8833
#define GL_DRAW_BUFFER15                                              0x8834
#define GL_BLEND_EQUATION_ALPHA                                       0x883D
#define GL_MAX_VERTEX_ATTRIBS                                         0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED                             0x886A
#define GL_MAX_TEXTURE_IMAGE_UNITS                                    0x8872
#define GL_FRAGMENT_SHADER                                            0x8B30
#define GL_VERTEX_SHADER                                              0x8B31
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS                            0x8B49
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS                              0x8B4A
#define GL_MAX_VARYING_FLOATS                                         0x8B4B
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS                             0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS                           0x8B4D
#define GL_SHADER_TYPE                                                0x8B4F
#define GL_FLOAT_VEC2                                                 0x8B50
#define GL_FLOAT_VEC3                                                 0x8B51
#define GL_FLOAT_VEC4                                                 0x8B52
#define GL_INT_VEC2                                                   0x8B53
#define GL_INT_VEC3                                                   0x8B54
#define GL_INT_VEC4                                                   0x8B55
#define GL_BOOL                                                       0x8B56
#define GL_BOOL_VEC2                                                  0x8B57
#define GL_BOOL_VEC3                                                  0x8B58
#define GL_BOOL_VEC4                                                  0x8B59
#define GL_FLOAT_MAT2                                                 0x8B5A
#define GL_FLOAT_MAT3                                                 0x8B5B
#define GL_FLOAT_MAT4                                                 0x8B5C
#define GL_SAMPLER_1D                                                 0x8B5D
#define GL_SAMPLER_2D                                                 0x8B5E
#define GL_SAMPLER_3D                                                 0x8B5F
#define GL_SAMPLER_CUBE                                               0x8B60
#define GL_SAMPLER_1D_SHADOW                                          0x8B61
#define GL_SAMPLER_2D_SHADOW                                          0x8B62
#define GL_DELETE_STATUS                                              0x8B80
#define GL_COMPILE_STATUS                                             0x8B81
#define GL_LINK_STATUS                                                0x8B82
#define GL_VALIDATE_STATUS                                            0x8B83
#define GL_INFO_LOG_LENGTH                                            0x8B84
#define GL_ATTACHED_SHADERS                                           0x8B85
#define GL_ACTIVE_UNIFORMS                                            0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH                                  0x8B87
#define GL_SHADER_SOURCE_LENGTH                                       0x8B88
#define GL_ACTIVE_ATTRIBUTES                                          0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH                                0x8B8A
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT                            0x8B8B
#define GL_SHADING_LANGUAGE_VERSION                                   0x8B8C
#define GL_CURRENT_PROGRAM                                            0x8B8D
#define GL_POINT_SPRITE_COORD_ORIGIN                                  0x8CA0
#define GL_LOWER_LEFT                                                 0x8CA1
#define GL_UPPER_LEFT                                                 0x8CA2
#define GL_STENCIL_BACK_REF                                           0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK                                    0x8CA4
#define GL_STENCIL_BACK_WRITEMASK                                     0x8CA5
#define GL_PIXEL_PACK_BUFFER                                          0x88EB
#define GL_PIXEL_UNPACK_BUFFER                                        0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING                                  0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING                                0x88EF
#define GL_FLOAT_MAT2x3                                               0x8B65
#define GL_FLOAT_MAT2x4                                               0x8B66
#define GL_FLOAT_MAT3x2                                               0x8B67
#define GL_FLOAT_MAT3x4                                               0x8B68
#define GL_FLOAT_MAT4x2                                               0x8B69
#define GL_FLOAT_MAT4x3                                               0x8B6A
#define GL_SRGB                                                       0x8C40
#define GL_SRGB8                                                      0x8C41
#define GL_SRGB_ALPHA                                                 0x8C42
#define GL_SRGB8_ALPHA8                                               0x8C43
#define GL_COMPRESSED_SRGB                                            0x8C48
#define GL_COMPRESSED_SRGB_ALPHA                                      0x8C49
#define GL_COMPARE_REF_TO_TEXTURE                                     0x884E
#define GL_CLIP_DISTANCE0                                             0x3000
#define GL_CLIP_DISTANCE1                                             0x3001
#define GL_CLIP_DISTANCE2                                             0x3002
#define GL_CLIP_DISTANCE3                                             0x3003
#define GL_CLIP_DISTANCE4                                             0x3004
#define GL_CLIP_DISTANCE5                                             0x3005
#define GL_CLIP_DISTANCE6                                             0x3006
#define GL_CLIP_DISTANCE7                                             0x3007
#define GL_MAX_CLIP_DISTANCES                                         0x0D32
#define GL_MAJOR_VERSION                                              0x821B
#define GL_MINOR_VERSION                                              0x821C
#define GL_NUM_EXTENSIONS                                             0x821D
#define GL_CONTEXT_FLAGS                                              0x821E
#define GL_COMPRESSED_RED                                             0x8225
#define GL_COMPRESSED_RG                                              0x8226
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT                        0x00000001
#define GL_RGBA32F                                                    0x8814
#define GL_RGB32F                                                     0x8815
#define GL_RGBA16F                                                    0x881A
#define GL_RGB16F                                                     0x881B
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER                                0x88FD
#define GL_MAX_ARRAY_TEXTURE_LAYERS                                   0x88FF
#define GL_MIN_PROGRAM_TEXEL_OFFSET                                   0x8904
#define GL_MAX_PROGRAM_TEXEL_OFFSET                                   0x8905
#define GL_CLAMP_READ_COLOR                                           0x891C
#define GL_FIXED_ONLY                                                 0x891D
#define GL_MAX_VARYING_COMPONENTS                                     0x8B4B
#define GL_TEXTURE_1D_ARRAY                                           0x8C18
#define GL_PROXY_TEXTURE_1D_ARRAY                                     0x8C19
#define GL_TEXTURE_2D_ARRAY                                           0x8C1A
#define GL_PROXY_TEXTURE_2D_ARRAY                                     0x8C1B
#define GL_TEXTURE_BINDING_1D_ARRAY                                   0x8C1C
#define GL_TEXTURE_BINDING_2D_ARRAY                                   0x8C1D
#define GL_R11F_G11F_B10F                                             0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV                               0x8C3B
#define GL_RGB9_E5                                                    0x8C3D
#define GL_UNSIGNED_INT_5_9_9_9_REV                                   0x8C3E
#define GL_TEXTURE_SHARED_SIZE                                        0x8C3F
#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH                      0x8C76
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE                             0x8C7F
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS                 0x8C80
#define GL_TRANSFORM_FEEDBACK_VARYINGS                                0x8C83
#define GL_TRANSFORM_FEEDBACK_BUFFER_START                            0x8C84
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE                             0x8C85
#define GL_PRIMITIVES_GENERATED                                       0x8C87
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN                      0x8C88
#define GL_RASTERIZER_DISCARD                                         0x8C89
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS              0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS                    0x8C8B
#define GL_INTERLEAVED_ATTRIBS                                        0x8C8C
#define GL_SEPARATE_ATTRIBS                                           0x8C8D
#define GL_TRANSFORM_FEEDBACK_BUFFER                                  0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING                          0x8C8F
#define GL_RGBA32UI                                                   0x8D70
#define GL_RGB32UI                                                    0x8D71
#define GL_RGBA16UI                                                   0x8D76
#define GL_RGB16UI                                                    0x8D77
#define GL_RGBA8UI                                                    0x8D7C
#define GL_RGB8UI                                                     0x8D7D
#define GL_RGBA32I                                                    0x8D82
#define GL_RGB32I                                                     0x8D83
#define GL_RGBA16I                                                    0x8D88
#define GL_RGB16I                                                     0x8D89
#define GL_RGBA8I                                                     0x8D8E
#define GL_RGB8I                                                      0x8D8F
#define GL_RED_INTEGER                                                0x8D94
#define GL_GREEN_INTEGER                                              0x8D95
#define GL_BLUE_INTEGER                                               0x8D96
#define GL_RGB_INTEGER                                                0x8D98
#define GL_RGBA_INTEGER                                               0x8D99
#define GL_BGR_INTEGER                                                0x8D9A
#define GL_BGRA_INTEGER                                               0x8D9B
#define GL_SAMPLER_1D_ARRAY                                           0x8DC0
#define GL_SAMPLER_2D_ARRAY                                           0x8DC1
#define GL_SAMPLER_1D_ARRAY_SHADOW                                    0x8DC3
#define GL_SAMPLER_2D_ARRAY_SHADOW                                    0x8DC4
#define GL_SAMPLER_CUBE_SHADOW                                        0x8DC5
#define GL_UNSIGNED_INT_VEC2                                          0x8DC6
#define GL_UNSIGNED_INT_VEC3                                          0x8DC7
#define GL_UNSIGNED_INT_VEC4                                          0x8DC8
#define GL_INT_SAMPLER_1D                                             0x8DC9
#define GL_INT_SAMPLER_2D                                             0x8DCA
#define GL_INT_SAMPLER_3D                                             0x8DCB
#define GL_INT_SAMPLER_CUBE                                           0x8DCC
#define GL_INT_SAMPLER_1D_ARRAY                                       0x8DCE
#define GL_INT_SAMPLER_2D_ARRAY                                       0x8DCF
#define GL_UNSIGNED_INT_SAMPLER_1D                                    0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_2D                                    0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_3D                                    0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE                                  0x8DD4
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY                              0x8DD6
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY                              0x8DD7
#define GL_QUERY_WAIT                                                 0x8E13
#define GL_QUERY_NO_WAIT                                              0x8E14
#define GL_QUERY_BY_REGION_WAIT                                       0x8E15
#define GL_QUERY_BY_REGION_NO_WAIT                                    0x8E16
#define GL_BUFFER_ACCESS_FLAGS                                        0x911F
#define GL_BUFFER_MAP_LENGTH                                          0x9120
#define GL_BUFFER_MAP_OFFSET                                          0x9121
#define GL_DEPTH_COMPONENT32F                                         0x8CAC
#define GL_DEPTH32F_STENCIL8                                          0x8CAD
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV                             0x8DAD
#define GL_INVALID_FRAMEBUFFER_OPERATION                              0x0506
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING                      0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE                      0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE                            0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE                          0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE                           0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE                          0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE                          0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE                        0x8217
#define GL_FRAMEBUFFER_DEFAULT                                        0x8218
#define GL_FRAMEBUFFER_UNDEFINED                                      0x8219
#define GL_DEPTH_STENCIL_ATTACHMENT                                   0x821A
#define GL_MAX_RENDERBUFFER_SIZE                                      0x84E8
#define GL_DEPTH_STENCIL                                              0x84F9
#define GL_UNSIGNED_INT_24_8                                          0x84FA
#define GL_DEPTH24_STENCIL8                                           0x88F0
#define GL_TEXTURE_STENCIL_SIZE                                       0x88F1
#define GL_TEXTURE_RED_TYPE                                           0x8C10
#define GL_TEXTURE_GREEN_TYPE                                         0x8C11
#define GL_TEXTURE_BLUE_TYPE                                          0x8C12
#define GL_TEXTURE_ALPHA_TYPE                                         0x8C13
#define GL_TEXTURE_DEPTH_TYPE                                         0x8C16
#define GL_UNSIGNED_NORMALIZED                                        0x8C17
#define GL_FRAMEBUFFER_BINDING                                        0x8CA6
#define GL_DRAW_FRAMEBUFFER_BINDING                                   0x8CA6
#define GL_RENDERBUFFER_BINDING                                       0x8CA7
#define GL_READ_FRAMEBUFFER                                           0x8CA8
#define GL_DRAW_FRAMEBUFFER                                           0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING                                   0x8CAA
#define GL_RENDERBUFFER_SAMPLES                                       0x8CAB
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE                         0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME                         0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL                       0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE               0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER                       0x8CD4
#define GL_FRAMEBUFFER_COMPLETE                                       0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT                          0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT                  0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER                         0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER                         0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED                                    0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS                                      0x8CDF
#define GL_COLOR_ATTACHMENT0                                          0x8CE0
#define GL_COLOR_ATTACHMENT1                                          0x8CE1
#define GL_COLOR_ATTACHMENT2                                          0x8CE2
#define GL_COLOR_ATTACHMENT3                                          0x8CE3
#define GL_COLOR_ATTACHMENT4                                          0x8CE4
#define GL_COLOR_ATTACHMENT5                                          0x8CE5
#define GL_COLOR_ATTACHMENT6                                          0x8CE6
#define GL_COLOR_ATTACHMENT7                                          0x8CE7
#define GL_COLOR_ATTACHMENT8                                          0x8CE8
#define GL_COLOR_ATTACHMENT9                                          0x8CE9
#define GL_COLOR_ATTACHMENT10                                         0x8CEA
#define GL_COLOR_ATTACHMENT11                                         0x8CEB
#define GL_COLOR_ATTACHMENT12                                         0x8CEC
#define GL_COLOR_ATTACHMENT13                                         0x8CED
#define GL_COLOR_ATTACHMENT14                                         0x8CEE
#define GL_COLOR_ATTACHMENT15                                         0x8CEF
#define GL_COLOR_ATTACHMENT16                                         0x8CF0
#define GL_COLOR_ATTACHMENT17                                         0x8CF1
#define GL_COLOR_ATTACHMENT18                                         0x8CF2
#define GL_COLOR_ATTACHMENT19                                         0x8CF3
#define GL_COLOR_ATTACHMENT20                                         0x8CF4
#define GL_COLOR_ATTACHMENT21                                         0x8CF5
#define GL_COLOR_ATTACHMENT22                                         0x8CF6
#define GL_COLOR_ATTACHMENT23                                         0x8CF7
#define GL_COLOR_ATTACHMENT24                                         0x8CF8
#define GL_COLOR_ATTACHMENT25                                         0x8CF9
#define GL_COLOR_ATTACHMENT26                                         0x8CFA
#define GL_COLOR_ATTACHMENT27                                         0x8CFB
#define GL_COLOR_ATTACHMENT28                                         0x8CFC
#define GL_COLOR_ATTACHMENT29                                         0x8CFD
#define GL_COLOR_ATTACHMENT30                                         0x8CFE
#define GL_COLOR_ATTACHMENT31                                         0x8CFF
#define GL_DEPTH_ATTACHMENT                                           0x8D00
#define GL_STENCIL_ATTACHMENT                                         0x8D20
#define GL_FRAMEBUFFER                                                0x8D40
#define GL_RENDERBUFFER                                               0x8D41
#define GL_RENDERBUFFER_WIDTH                                         0x8D42
#define GL_RENDERBUFFER_HEIGHT                                        0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT                               0x8D44
#define GL_STENCIL_INDEX1                                             0x8D46
#define GL_STENCIL_INDEX4                                             0x8D47
#define GL_STENCIL_INDEX8                                             0x8D48
#define GL_STENCIL_INDEX16                                            0x8D49
#define GL_RENDERBUFFER_RED_SIZE                                      0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE                                    0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE                                     0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE                                    0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE                                    0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE                                  0x8D55
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE                         0x8D56
#define GL_MAX_SAMPLES                                                0x8D57
#define GL_INDEX                                                      0x8222
#define GL_FRAMEBUFFER_SRGB                                           0x8DB9
#define GL_HALF_FLOAT                                                 0x140B
#define GL_MAP_READ_BIT                                               0x0001
#define GL_MAP_WRITE_BIT                                              0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT                                   0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT                                  0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT                                     0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT                                     0x0020
#define GL_COMPRESSED_RED_RGTC1                                       0x8DBB
#define GL_COMPRESSED_SIGNED_RED_RGTC1                                0x8DBC
#define GL_COMPRESSED_RG_RGTC2                                        0x8DBD
#define GL_COMPRESSED_SIGNED_RG_RGTC2                                 0x8DBE
#define GL_RG                                                         0x8227
#define GL_RG_INTEGER                                                 0x8228
#define GL_R8                                                         0x8229
#define GL_R16                                                        0x822A
#define GL_RG8                                                        0x822B
#define GL_RG16                                                       0x822C
#define GL_R16F                                                       0x822D
#define GL_R32F                                                       0x822E
#define GL_RG16F                                                      0x822F
#define GL_RG32F                                                      0x8230
#define GL_R8I                                                        0x8231
#define GL_R8UI                                                       0x8232
#define GL_R16I                                                       0x8233
#define GL_R16UI                                                      0x8234
#define GL_R32I                                                       0x8235
#define GL_R32UI                                                      0x8236
#define GL_RG8I                                                       0x8237
#define GL_RG8UI                                                      0x8238
#define GL_RG16I                                                      0x8239
#define GL_RG16UI                                                     0x823A
#define GL_RG32I                                                      0x823B
#define GL_RG32UI                                                     0x823C
#define GL_VERTEX_ARRAY_BINDING                                       0x85B5
#define GL_SAMPLER_2D_RECT                                            0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW                                     0x8B64
#define GL_SAMPLER_BUFFER                                             0x8DC2
#define GL_INT_SAMPLER_2D_RECT                                        0x8DCD
#define GL_INT_SAMPLER_BUFFER                                         0x8DD0
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT                               0x8DD5
#define GL_UNSIGNED_INT_SAMPLER_BUFFER                                0x8DD8
#define GL_TEXTURE_BUFFER                                             0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE                                    0x8C2B
#define GL_TEXTURE_BINDING_BUFFER                                     0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING                          0x8C2D
#define GL_TEXTURE_RECTANGLE                                          0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE                                  0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE                                    0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE                                 0x84F8
#define GL_R8_SNORM                                                   0x8F94
#define GL_RG8_SNORM                                                  0x8F95
#define GL_RGB8_SNORM                                                 0x8F96
#define GL_RGBA8_SNORM                                                0x8F97
#define GL_R16_SNORM                                                  0x8F98
#define GL_RG16_SNORM                                                 0x8F99
#define GL_RGB16_SNORM                                                0x8F9A
#define GL_RGBA16_SNORM                                               0x8F9B
#define GL_SIGNED_NORMALIZED                                          0x8F9C
#define GL_PRIMITIVE_RESTART                                          0x8F9D
#define GL_PRIMITIVE_RESTART_INDEX                                    0x8F9E
#define GL_COPY_READ_BUFFER                                           0x8F36
#define GL_COPY_WRITE_BUFFER                                          0x8F37
#define GL_UNIFORM_BUFFER                                             0x8A11
#define GL_UNIFORM_BUFFER_BINDING                                     0x8A28
#define GL_UNIFORM_BUFFER_START                                       0x8A29
#define GL_UNIFORM_BUFFER_SIZE                                        0x8A2A
#define GL_MAX_VERTEX_UNIFORM_BLOCKS                                  0x8A2B
#define GL_MAX_GEOMETRY_UNIFORM_BLOCKS                                0x8A2C
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS                                0x8A2D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS                                0x8A2E
#define GL_MAX_UNIFORM_BUFFER_BINDINGS                                0x8A2F
#define GL_MAX_UNIFORM_BLOCK_SIZE                                     0x8A30
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS                     0x8A31
#define GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS                   0x8A32
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS                   0x8A33
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT                            0x8A34
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH                       0x8A35
#define GL_ACTIVE_UNIFORM_BLOCKS                                      0x8A36
#define GL_UNIFORM_TYPE                                               0x8A37
#define GL_UNIFORM_SIZE                                               0x8A38
#define GL_UNIFORM_NAME_LENGTH                                        0x8A39
#define GL_UNIFORM_BLOCK_INDEX                                        0x8A3A
#define GL_UNIFORM_OFFSET                                             0x8A3B
#define GL_UNIFORM_ARRAY_STRIDE                                       0x8A3C
#define GL_UNIFORM_MATRIX_STRIDE                                      0x8A3D
#define GL_UNIFORM_IS_ROW_MAJOR                                       0x8A3E
#define GL_UNIFORM_BLOCK_BINDING                                      0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE                                    0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH                                  0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS                              0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES                       0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER                  0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER                0x8A45
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER                0x8A46
#define GL_INVALID_INDEX                                              0xFFFFFFFF
#define GL_CONTEXT_CORE_PROFILE_BIT                                   0x00000001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT                          0x00000002
#define GL_LINES_ADJACENCY                                            0x000A
#define GL_LINE_STRIP_ADJACENCY                                       0x000B
#define GL_TRIANGLES_ADJACENCY                                        0x000C
#define GL_TRIANGLE_STRIP_ADJACENCY                                   0x000D
#define GL_PROGRAM_POINT_SIZE                                         0x8642
#define GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS                           0x8C29
#define GL_FRAMEBUFFER_ATTACHMENT_LAYERED                             0x8DA7
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS                       0x8DA8
#define GL_GEOMETRY_SHADER                                            0x8DD9
#define GL_GEOMETRY_VERTICES_OUT                                      0x8916
#define GL_GEOMETRY_INPUT_TYPE                                        0x8917
#define GL_GEOMETRY_OUTPUT_TYPE                                       0x8918
#define GL_MAX_GEOMETRY_UNIFORM_COMPONENTS                            0x8DDF
#define GL_MAX_GEOMETRY_OUTPUT_VERTICES                               0x8DE0
#define GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS                       0x8DE1
#define GL_MAX_VERTEX_OUTPUT_COMPONENTS                               0x9122
#define GL_MAX_GEOMETRY_INPUT_COMPONENTS                              0x9123
#define GL_MAX_GEOMETRY_OUTPUT_COMPONENTS                             0x9124
#define GL_MAX_FRAGMENT_INPUT_COMPONENTS                              0x9125
#define GL_CONTEXT_PROFILE_MASK                                       0x9126
#define GL_DEPTH_CLAMP                                                0x864F
#define GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION                   0x8E4C
#define GL_FIRST_VERTEX_CONVENTION                                    0x8E4D
#define GL_LAST_VERTEX_CONVENTION                                     0x8E4E
#define GL_PROVOKING_VERTEX                                           0x8E4F
#define GL_TEXTURE_CUBE_MAP_SEAMLESS                                  0x884F
#define GL_MAX_SERVER_WAIT_TIMEOUT                                    0x9111
#define GL_OBJECT_TYPE                                                0x9112
#define GL_SYNC_CONDITION                                             0x9113
#define GL_SYNC_STATUS                                                0x9114
#define GL_SYNC_FLAGS                                                 0x9115
#define GL_SYNC_FENCE                                                 0x9116
#define GL_SYNC_GPU_COMMANDS_COMPLETE                                 0x9117
#define GL_UNSIGNALED                                                 0x9118
#define GL_SIGNALED                                                   0x9119
#define GL_ALREADY_SIGNALED                                           0x911A
#define GL_TIMEOUT_EXPIRED                                            0x911B
#define GL_CONDITION_SATISFIED                                        0x911C
#define GL_WAIT_FAILED                                                0x911D
#define GL_TIMEOUT_IGNORED                                            0xFFFFFFFFFFFFFFFF
#define GL_SYNC_FLUSH_COMMANDS_BIT                                    0x00000001
#define GL_SAMPLE_POSITION                                            0x8E50
#define GL_SAMPLE_MASK                                                0x8E51
#define GL_SAMPLE_MASK_VALUE                                          0x8E52
#define GL_MAX_SAMPLE_MASK_WORDS                                      0x8E59
#define GL_TEXTURE_2D_MULTISAMPLE                                     0x9100
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE                               0x9101
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY                               0x9102
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY                         0x9103
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE                             0x9104
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY                       0x9105
#define GL_TEXTURE_SAMPLES                                            0x9106
#define GL_TEXTURE_FIXED_SAMPLE_LOCATIONS                             0x9107
#define GL_SAMPLER_2D_MULTISAMPLE                                     0x9108
#define GL_INT_SAMPLER_2D_MULTISAMPLE                                 0x9109
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE                        0x910A
#define GL_SAMPLER_2D_MULTISAMPLE_ARRAY                               0x910B
#define GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY                           0x910C
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY                  0x910D
#define GL_MAX_COLOR_TEXTURE_SAMPLES                                  0x910E
#define GL_MAX_DEPTH_TEXTURE_SAMPLES                                  0x910F
#define GL_MAX_INTEGER_SAMPLES                                        0x9110
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR                                0x88FE
#define GL_SRC1_COLOR                                                 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR                                       0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA                                       0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS                               0x88FC
#define GL_ANY_SAMPLES_PASSED                                         0x8C2F
#define GL_SAMPLER_BINDING                                            0x8919
#define GL_RGB10_A2UI                                                 0x906F
#define GL_TEXTURE_SWIZZLE_R                                          0x8E42
#define GL_TEXTURE_SWIZZLE_G                                          0x8E43
#define GL_TEXTURE_SWIZZLE_B                                          0x8E44
#define GL_TEXTURE_SWIZZLE_A                                          0x8E45
#define GL_TEXTURE_SWIZZLE_RGBA                                       0x8E46
#define GL_TIME_ELAPSED                                               0x88BF
#define GL_TIMESTAMP                                                  0x8E28
#define GL_INT_2_10_10_10_REV                                         0x8D9F
#define GL_SAMPLE_SHADING                                             0x8C36
#define GL_MIN_SAMPLE_SHADING_VALUE                                   0x8C37
#define GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET                          0x8E5E
#define GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET                          0x8E5F
#define GL_TEXTURE_CUBE_MAP_ARRAY                                     0x9009
#define GL_TEXTURE_BINDING_CUBE_MAP_ARRAY                             0x900A
#define GL_PROXY_TEXTURE_CUBE_MAP_ARRAY                               0x900B
#define GL_SAMPLER_CUBE_MAP_ARRAY                                     0x900C
#define GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW                              0x900D
#define GL_INT_SAMPLER_CUBE_MAP_ARRAY                                 0x900E
#define GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY                        0x900F
#define GL_DRAW_INDIRECT_BUFFER                                       0x8F3F
#define GL_DRAW_INDIRECT_BUFFER_BINDING                               0x8F43
#define GL_GEOMETRY_SHADER_INVOCATIONS                                0x887F
#define GL_MAX_GEOMETRY_SHADER_INVOCATIONS                            0x8E5A
#define GL_MIN_FRAGMENT_INTERPOLATION_OFFSET                          0x8E5B
#define GL_MAX_FRAGMENT_INTERPOLATION_OFFSET                          0x8E5C
#define GL_FRAGMENT_INTERPOLATION_OFFSET_BITS                         0x8E5D
#define GL_MAX_VERTEX_STREAMS                                         0x8E71
#define GL_DOUBLE_VEC2                                                0x8FFC
#define GL_DOUBLE_VEC3                                                0x8FFD
#define GL_DOUBLE_VEC4                                                0x8FFE
#define GL_DOUBLE_MAT2                                                0x8F46
#define GL_DOUBLE_MAT3                                                0x8F47
#define GL_DOUBLE_MAT4                                                0x8F48
#define GL_DOUBLE_MAT2x3                                              0x8F49
#define GL_DOUBLE_MAT2x4                                              0x8F4A
#define GL_DOUBLE_MAT3x2                                              0x8F4B
#define GL_DOUBLE_MAT3x4                                              0x8F4C
#define GL_DOUBLE_MAT4x2                                              0x8F4D
#define GL_DOUBLE_MAT4x3                                              0x8F4E
#define GL_ACTIVE_SUBROUTINES                                         0x8DE5
#define GL_ACTIVE_SUBROUTINE_UNIFORMS                                 0x8DE6
#define GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS                        0x8E47
#define GL_ACTIVE_SUBROUTINE_MAX_LENGTH                               0x8E48
#define GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH                       0x8E49
#define GL_MAX_SUBROUTINES                                            0x8DE7
#define GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS                           0x8DE8
#define GL_NUM_COMPATIBLE_SUBROUTINES                                 0x8E4A
#define GL_COMPATIBLE_SUBROUTINES                                     0x8E4B
#define GL_PATCHES                                                    0x000E
#define GL_PATCH_VERTICES                                             0x8E72
#define GL_PATCH_DEFAULT_INNER_LEVEL                                  0x8E73
#define GL_PATCH_DEFAULT_OUTER_LEVEL                                  0x8E74
#define GL_TESS_CONTROL_OUTPUT_VERTICES                               0x8E75
#define GL_TESS_GEN_MODE                                              0x8E76
#define GL_TESS_GEN_SPACING                                           0x8E77
#define GL_TESS_GEN_VERTEX_ORDER                                      0x8E78
#define GL_TESS_GEN_POINT_MODE                                        0x8E79
#define GL_ISOLINES                                                   0x8E7A
#define GL_FRACTIONAL_ODD                                             0x8E7B
#define GL_FRACTIONAL_EVEN                                            0x8E7C
#define GL_MAX_PATCH_VERTICES                                         0x8E7D
#define GL_MAX_TESS_GEN_LEVEL                                         0x8E7E
#define GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS                        0x8E7F
#define GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS                     0x8E80
#define GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS                       0x8E81
#define GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS                    0x8E82
#define GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS                         0x8E83
#define GL_MAX_TESS_PATCH_COMPONENTS                                  0x8E84
#define GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS                   0x8E85
#define GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS                      0x8E86
#define GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS                            0x8E89
#define GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS                         0x8E8A
#define GL_MAX_TESS_CONTROL_INPUT_COMPONENTS                          0x886C
#define GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS                       0x886D
#define GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS               0x8E1E
#define GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS            0x8E1F
#define GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER            0x84F0
#define GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER         0x84F1
#define GL_TESS_EVALUATION_SHADER                                     0x8E87
#define GL_TESS_CONTROL_SHADER                                        0x8E88
#define GL_TRANSFORM_FEEDBACK                                         0x8E22
#define GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED                           0x8E23
#define GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE                           0x8E24
#define GL_TRANSFORM_FEEDBACK_BINDING                                 0x8E25
#define GL_MAX_TRANSFORM_FEEDBACK_BUFFERS                             0x8E70
#define GL_FIXED                                                      0x140C
#define GL_IMPLEMENTATION_COLOR_READ_TYPE                             0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT                           0x8B9B
#define GL_LOW_FLOAT                                                  0x8DF0
#define GL_MEDIUM_FLOAT                                               0x8DF1
#define GL_HIGH_FLOAT                                                 0x8DF2
#define GL_LOW_INT                                                    0x8DF3
#define GL_MEDIUM_INT                                                 0x8DF4
#define GL_HIGH_INT                                                   0x8DF5
#define GL_SHADER_COMPILER                                            0x8DFA
#define GL_SHADER_BINARY_FORMATS                                      0x8DF8
#define GL_NUM_SHADER_BINARY_FORMATS                                  0x8DF9
#define GL_MAX_VERTEX_UNIFORM_VECTORS                                 0x8DFB
#define GL_MAX_VARYING_VECTORS                                        0x8DFC
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS                               0x8DFD
#define GL_RGB565                                                     0x8D62
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT                            0x8257
#define GL_PROGRAM_BINARY_LENGTH                                      0x8741
#define GL_NUM_PROGRAM_BINARY_FORMATS                                 0x87FE
#define GL_PROGRAM_BINARY_FORMATS                                     0x87FF
#define GL_VERTEX_SHADER_BIT                                          0x00000001
#define GL_FRAGMENT_SHADER_BIT                                        0x00000002
#define GL_GEOMETRY_SHADER_BIT                                        0x00000004
#define GL_TESS_CONTROL_SHADER_BIT                                    0x00000008
#define GL_TESS_EVALUATION_SHADER_BIT                                 0x00000010
#define GL_ALL_SHADER_BITS                                            0xFFFFFFFF
#define GL_PROGRAM_SEPARABLE                                          0x8258
#define GL_ACTIVE_PROGRAM                                             0x8259
#define GL_PROGRAM_PIPELINE_BINDING                                   0x825A
#define GL_MAX_VIEWPORTS                                              0x825B
#define GL_VIEWPORT_SUBPIXEL_BITS                                     0x825C
#define GL_VIEWPORT_BOUNDS_RANGE                                      0x825D
#define GL_LAYER_PROVOKING_VERTEX                                     0x825E
#define GL_VIEWPORT_INDEX_PROVOKING_VERTEX                            0x825F
#define GL_UNDEFINED_VERTEX                                           0x8260
#define GL_COPY_READ_BUFFER_BINDING                                   0x8F36
#define GL_COPY_WRITE_BUFFER_BINDING                                  0x8F37
#define GL_TRANSFORM_FEEDBACK_ACTIVE                                  0x8E24
#define GL_TRANSFORM_FEEDBACK_PAUSED                                  0x8E23
#define GL_UNPACK_COMPRESSED_BLOCK_WIDTH                              0x9127
#define GL_UNPACK_COMPRESSED_BLOCK_HEIGHT                             0x9128
#define GL_UNPACK_COMPRESSED_BLOCK_DEPTH                              0x9129
#define GL_UNPACK_COMPRESSED_BLOCK_SIZE                               0x912A
#define GL_PACK_COMPRESSED_BLOCK_WIDTH                                0x912B
#define GL_PACK_COMPRESSED_BLOCK_HEIGHT                               0x912C
#define GL_PACK_COMPRESSED_BLOCK_DEPTH                                0x912D
#define GL_PACK_COMPRESSED_BLOCK_SIZE                                 0x912E
#define GL_NUM_SAMPLE_COUNTS                                          0x9380
#define GL_MIN_MAP_BUFFER_ALIGNMENT                                   0x90BC
#define GL_ATOMIC_COUNTER_BUFFER                                      0x92C0
#define GL_ATOMIC_COUNTER_BUFFER_BINDING                              0x92C1
#define GL_ATOMIC_COUNTER_BUFFER_START                                0x92C2
#define GL_ATOMIC_COUNTER_BUFFER_SIZE                                 0x92C3
#define GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE                            0x92C4
#define GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS               0x92C5
#define GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES        0x92C6
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER          0x92C7
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER    0x92C8
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER 0x92C9
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER        0x92CA
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER        0x92CB
#define GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS                          0x92CC
#define GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS                    0x92CD
#define GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS                 0x92CE
#define GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS                        0x92CF
#define GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS                        0x92D0
#define GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS                        0x92D1
#define GL_MAX_VERTEX_ATOMIC_COUNTERS                                 0x92D2
#define GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS                           0x92D3
#define GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS                        0x92D4
#define GL_MAX_GEOMETRY_ATOMIC_COUNTERS                               0x92D5
#define GL_MAX_FRAGMENT_ATOMIC_COUNTERS                               0x92D6
#define GL_MAX_COMBINED_ATOMIC_COUNTERS                               0x92D7
#define GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE                             0x92D8
#define GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS                         0x92DC
#define GL_ACTIVE_ATOMIC_COUNTER_BUFFERS                              0x92D9
#define GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX                        0x92DA
#define GL_UNSIGNED_INT_ATOMIC_COUNTER                                0x92DB
#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT                            0x00000001
#define GL_ELEMENT_ARRAY_BARRIER_BIT                                  0x00000002
#define GL_UNIFORM_BARRIER_BIT                                        0x00000004
#define GL_TEXTURE_FETCH_BARRIER_BIT                                  0x00000008
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT                            0x00000020
#define GL_COMMAND_BARRIER_BIT                                        0x00000040
#define GL_PIXEL_BUFFER_BARRIER_BIT                                   0x00000080
#define GL_TEXTURE_UPDATE_BARRIER_BIT                                 0x00000100
#define GL_BUFFER_UPDATE_BARRIER_BIT                                  0x00000200
#define GL_FRAMEBUFFER_BARRIER_BIT                                    0x00000400
#define GL_TRANSFORM_FEEDBACK_BARRIER_BIT                             0x00000800
#define GL_ATOMIC_COUNTER_BARRIER_BIT                                 0x00001000
#define GL_ALL_BARRIER_BITS                                           0xFFFFFFFF
#define GL_MAX_IMAGE_UNITS                                            0x8F38
#define GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS              0x8F39
#define GL_IMAGE_BINDING_NAME                                         0x8F3A
#define GL_IMAGE_BINDING_LEVEL                                        0x8F3B
#define GL_IMAGE_BINDING_LAYERED                                      0x8F3C
#define GL_IMAGE_BINDING_LAYER                                        0x8F3D
#define GL_IMAGE_BINDING_ACCESS                                       0x8F3E
#define GL_IMAGE_1D                                                   0x904C
#define GL_IMAGE_2D                                                   0x904D
#define GL_IMAGE_3D                                                   0x904E
#define GL_IMAGE_2D_RECT                                              0x904F
#define GL_IMAGE_CUBE                                                 0x9050
#define GL_IMAGE_BUFFER                                               0x9051
#define GL_IMAGE_1D_ARRAY                                             0x9052
#define GL_IMAGE_2D_ARRAY                                             0x9053
#define GL_IMAGE_CUBE_MAP_ARRAY                                       0x9054
#define GL_IMAGE_2D_MULTISAMPLE                                       0x9055
#define GL_IMAGE_2D_MULTISAMPLE_ARRAY                                 0x9056
#define GL_INT_IMAGE_1D                                               0x9057
#define GL_INT_IMAGE_2D                                               0x9058
#define GL_INT_IMAGE_3D                                               0x9059
#define GL_INT_IMAGE_2D_RECT                                          0x905A
#define GL_INT_IMAGE_CUBE                                             0x905B
#define GL_INT_IMAGE_BUFFER                                           0x905C
#define GL_INT_IMAGE_1D_ARRAY                                         0x905D
#define GL_INT_IMAGE_2D_ARRAY                                         0x905E
#define GL_INT_IMAGE_CUBE_MAP_ARRAY                                   0x905F
#define GL_INT_IMAGE_2D_MULTISAMPLE                                   0x9060
#define GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY                             0x9061
#define GL_UNSIGNED_INT_IMAGE_1D                                      0x9062
#define GL_UNSIGNED_INT_IMAGE_2D                                      0x9063
#define GL_UNSIGNED_INT_IMAGE_3D                                      0x9064
#define GL_UNSIGNED_INT_IMAGE_2D_RECT                                 0x9065
#define GL_UNSIGNED_INT_IMAGE_CUBE                                    0x9066
#define GL_UNSIGNED_INT_IMAGE_BUFFER                                  0x9067
#define GL_UNSIGNED_INT_IMAGE_1D_ARRAY                                0x9068
#define GL_UNSIGNED_INT_IMAGE_2D_ARRAY                                0x9069
#define GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY                          0x906A
#define GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE                          0x906B
#define GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY                    0x906C
#define GL_MAX_IMAGE_SAMPLES                                          0x906D
#define GL_IMAGE_BINDING_FORMAT                                       0x906E
#define GL_IMAGE_FORMAT_COMPATIBILITY_TYPE                            0x90C7
#define GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE                         0x90C8
#define GL_IMAGE_FORMAT_COMPATIBILITY_BY_CLASS                        0x90C9
#define GL_MAX_VERTEX_IMAGE_UNIFORMS                                  0x90CA
#define GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS                            0x90CB
#define GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS                         0x90CC
#define GL_MAX_GEOMETRY_IMAGE_UNIFORMS                                0x90CD
#define GL_MAX_FRAGMENT_IMAGE_UNIFORMS                                0x90CE
#define GL_MAX_COMBINED_IMAGE_UNIFORMS                                0x90CF
#define GL_COMPRESSED_RGBA_BPTC_UNORM                                 0x8E8C
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM                           0x8E8D
#define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT                           0x8E8E
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT                         0x8E8F
#define GL_TEXTURE_IMMUTABLE_FORMAT                                   0x912F
#define GL_NUM_SHADING_LANGUAGE_VERSIONS                              0x82E9
#define GL_VERTEX_ATTRIB_ARRAY_LONG                                   0x874E
#define GL_COMPRESSED_RGB8_ETC2                                       0x9274
#define GL_COMPRESSED_SRGB8_ETC2                                      0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2                   0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2                  0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC                                  0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC                           0x9279
#define GL_COMPRESSED_R11_EAC                                         0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC                                  0x9271
#define GL_COMPRESSED_RG11_EAC                                        0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC                                 0x9273
#define GL_PRIMITIVE_RESTART_FIXED_INDEX                              0x8D69
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE                            0x8D6A
#define GL_MAX_ELEMENT_INDEX                                          0x8D6B
#define GL_COMPUTE_SHADER                                             0x91B9
#define GL_MAX_COMPUTE_UNIFORM_BLOCKS                                 0x91BB
#define GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS                            0x91BC
#define GL_MAX_COMPUTE_IMAGE_UNIFORMS                                 0x91BD
#define GL_MAX_COMPUTE_SHARED_MEMORY_SIZE                             0x8262
#define GL_MAX_COMPUTE_UNIFORM_COMPONENTS                             0x8263
#define GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS                         0x8264
#define GL_MAX_COMPUTE_ATOMIC_COUNTERS                                0x8265
#define GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS                    0x8266
#define GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS                         0x90EB
#define GL_MAX_COMPUTE_WORK_GROUP_COUNT                               0x91BE
#define GL_MAX_COMPUTE_WORK_GROUP_SIZE                                0x91BF
#define GL_COMPUTE_WORK_GROUP_SIZE                                    0x8267
#define GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER                 0x90EC
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER         0x90ED
#define GL_DISPATCH_INDIRECT_BUFFER                                   0x90EE
#define GL_DISPATCH_INDIRECT_BUFFER_BINDING                           0x90EF
#define GL_COMPUTE_SHADER_BIT                                         0x00000020
#define GL_DEBUG_OUTPUT_SYNCHRONOUS                                   0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH                           0x8243
#define GL_DEBUG_CALLBACK_FUNCTION                                    0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM                                  0x8245
#define GL_DEBUG_SOURCE_API                                           0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM                                 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER                               0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY                                   0x8249
#define GL_DEBUG_SOURCE_APPLICATION                                   0x824A
#define GL_DEBUG_SOURCE_OTHER                                         0x824B
#define GL_DEBUG_TYPE_ERROR                                           0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR                             0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR                              0x824E
#define GL_DEBUG_TYPE_PORTABILITY                                     0x824F
#define GL_DEBUG_TYPE_PERFORMANCE                                     0x8250
#define GL_DEBUG_TYPE_OTHER                                           0x8251
#define GL_MAX_DEBUG_MESSAGE_LENGTH                                   0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES                                  0x9144
#define GL_DEBUG_LOGGED_MESSAGES                                      0x9145
#define GL_DEBUG_SEVERITY_HIGH                                        0x9146
#define GL_DEBUG_SEVERITY_MEDIUM                                      0x9147
#define GL_DEBUG_SEVERITY_LOW                                         0x9148
#define GL_DEBUG_TYPE_MARKER                                          0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP                                      0x8269
#define GL_DEBUG_TYPE_POP_GROUP                                       0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION                                0x826B
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH                                0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH                                    0x826D
#define GL_BUFFER                                                     0x82E0
#define GL_SHADER                                                     0x82E1
#define GL_PROGRAM                                                    0x82E2
#define GL_QUERY                                                      0x82E3
#define GL_PROGRAM_PIPELINE                                           0x82E4
#define GL_SAMPLER                                                    0x82E6
#define GL_MAX_LABEL_LENGTH                                           0x82E8
#define GL_DEBUG_OUTPUT                                               0x92E0
#define GL_CONTEXT_FLAG_DEBUG_BIT                                     0x00000002
#define GL_MAX_UNIFORM_LOCATIONS                                      0x826E
#define GL_FRAMEBUFFER_DEFAULT_WIDTH                                  0x9310
#define GL_FRAMEBUFFER_DEFAULT_HEIGHT                                 0x9311
#define GL_FRAMEBUFFER_DEFAULT_LAYERS                                 0x9312
#define GL_FRAMEBUFFER_DEFAULT_SAMPLES                                0x9313
#define GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS                 0x9314
#define GL_MAX_FRAMEBUFFER_WIDTH                                      0x9315
#define GL_MAX_FRAMEBUFFER_HEIGHT                                     0x9316
#define GL_MAX_FRAMEBUFFER_LAYERS                                     0x9317
#define GL_MAX_FRAMEBUFFER_SAMPLES                                    0x9318
#define GL_INTERNALFORMAT_SUPPORTED                                   0x826F
#define GL_INTERNALFORMAT_PREFERRED                                   0x8270
#define GL_INTERNALFORMAT_RED_SIZE                                    0x8271
#define GL_INTERNALFORMAT_GREEN_SIZE                                  0x8272
#define GL_INTERNALFORMAT_BLUE_SIZE                                   0x8273
#define GL_INTERNALFORMAT_ALPHA_SIZE                                  0x8274
#define GL_INTERNALFORMAT_DEPTH_SIZE                                  0x8275
#define GL_INTERNALFORMAT_STENCIL_SIZE                                0x8276
#define GL_INTERNALFORMAT_SHARED_SIZE                                 0x8277
#define GL_INTERNALFORMAT_RED_TYPE                                    0x8278
#define GL_INTERNALFORMAT_GREEN_TYPE                                  0x8279
#define GL_INTERNALFORMAT_BLUE_TYPE                                   0x827A
#define GL_INTERNALFORMAT_ALPHA_TYPE                                  0x827B
#define GL_INTERNALFORMAT_DEPTH_TYPE                                  0x827C
#define GL_INTERNALFORMAT_STENCIL_TYPE                                0x827D
#define GL_MAX_WIDTH                                                  0x827E
#define GL_MAX_HEIGHT                                                 0x827F
#define GL_MAX_DEPTH                                                  0x8280
#define GL_MAX_LAYERS                                                 0x8281
#define GL_MAX_COMBINED_DIMENSIONS                                    0x8282
#define GL_COLOR_COMPONENTS                                           0x8283
#define GL_DEPTH_COMPONENTS                                           0x8284
#define GL_STENCIL_COMPONENTS                                         0x8285
#define GL_COLOR_RENDERABLE                                           0x8286
#define GL_DEPTH_RENDERABLE                                           0x8287
#define GL_STENCIL_RENDERABLE                                         0x8288
#define GL_FRAMEBUFFER_RENDERABLE                                     0x8289
#define GL_FRAMEBUFFER_RENDERABLE_LAYERED                             0x828A
#define GL_FRAMEBUFFER_BLEND                                          0x828B
#define GL_READ_PIXELS                                                0x828C
#define GL_READ_PIXELS_FORMAT                                         0x828D
#define GL_READ_PIXELS_TYPE                                           0x828E
#define GL_TEXTURE_IMAGE_FORMAT                                       0x828F
#define GL_TEXTURE_IMAGE_TYPE                                         0x8290
#define GL_GET_TEXTURE_IMAGE_FORMAT                                   0x8291
#define GL_GET_TEXTURE_IMAGE_TYPE                                     0x8292
#define GL_MIPMAP                                                     0x8293
#define GL_MANUAL_GENERATE_MIPMAP                                     0x8294
#define GL_AUTO_GENERATE_MIPMAP                                       0x8295
#define GL_COLOR_ENCODING                                             0x8296
#define GL_SRGB_READ                                                  0x8297
#define GL_SRGB_WRITE                                                 0x8298
#define GL_FILTER                                                     0x829A
#define GL_VERTEX_TEXTURE                                             0x829B
#define GL_TESS_CONTROL_TEXTURE                                       0x829C
#define GL_TESS_EVALUATION_TEXTURE                                    0x829D
#define GL_GEOMETRY_TEXTURE                                           0x829E
#define GL_FRAGMENT_TEXTURE                                           0x829F
#define GL_COMPUTE_TEXTURE                                            0x82A0
#define GL_TEXTURE_SHADOW                                             0x82A1
#define GL_TEXTURE_GATHER                                             0x82A2
#define GL_TEXTURE_GATHER_SHADOW                                      0x82A3
#define GL_SHADER_IMAGE_LOAD                                          0x82A4
#define GL_SHADER_IMAGE_STORE                                         0x82A5
#define GL_SHADER_IMAGE_ATOMIC                                        0x82A6
#define GL_IMAGE_TEXEL_SIZE                                           0x82A7
#define GL_IMAGE_COMPATIBILITY_CLASS                                  0x82A8
#define GL_IMAGE_PIXEL_FORMAT                                         0x82A9
#define GL_IMAGE_PIXEL_TYPE                                           0x82AA
#define GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_TEST                        0x82AC
#define GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_TEST                      0x82AD
#define GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_WRITE                       0x82AE
#define GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_WRITE                     0x82AF
#define GL_TEXTURE_COMPRESSED_BLOCK_WIDTH                             0x82B1
#define GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT                            0x82B2
#define GL_TEXTURE_COMPRESSED_BLOCK_SIZE                              0x82B3
#define GL_CLEAR_BUFFER                                               0x82B4
#define GL_TEXTURE_VIEW                                               0x82B5
#define GL_VIEW_COMPATIBILITY_CLASS                                   0x82B6
#define GL_FULL_SUPPORT                                               0x82B7
#define GL_CAVEAT_SUPPORT                                             0x82B8
#define GL_IMAGE_CLASS_4_X_32                                         0x82B9
#define GL_IMAGE_CLASS_2_X_32                                         0x82BA
#define GL_IMAGE_CLASS_1_X_32                                         0x82BB
#define GL_IMAGE_CLASS_4_X_16                                         0x82BC
#define GL_IMAGE_CLASS_2_X_16                                         0x82BD
#define GL_IMAGE_CLASS_1_X_16                                         0x82BE
#define GL_IMAGE_CLASS_4_X_8                                          0x82BF
#define GL_IMAGE_CLASS_2_X_8                                          0x82C0
#define GL_IMAGE_CLASS_1_X_8                                          0x82C1
#define GL_IMAGE_CLASS_11_11_10                                       0x82C2
#define GL_IMAGE_CLASS_10_10_10_2                                     0x82C3
#define GL_VIEW_CLASS_128_BITS                                        0x82C4
#define GL_VIEW_CLASS_96_BITS                                         0x82C5
#define GL_VIEW_CLASS_64_BITS                                         0x82C6
#define GL_VIEW_CLASS_48_BITS                                         0x82C7
#define GL_VIEW_CLASS_32_BITS                                         0x82C8
#define GL_VIEW_CLASS_24_BITS                                         0x82C9
#define GL_VIEW_CLASS_16_BITS                                         0x82CA
#define GL_VIEW_CLASS_8_BITS                                          0x82CB
#define GL_VIEW_CLASS_S3TC_DXT1_RGB                                   0x82CC
#define GL_VIEW_CLASS_S3TC_DXT1_RGBA                                  0x82CD
#define GL_VIEW_CLASS_S3TC_DXT3_RGBA                                  0x82CE
#define GL_VIEW_CLASS_S3TC_DXT5_RGBA                                  0x82CF
#define GL_VIEW_CLASS_RGTC1_RED                                       0x82D0
#define GL_VIEW_CLASS_RGTC2_RG                                        0x82D1
#define GL_VIEW_CLASS_BPTC_UNORM                                      0x82D2
#define GL_VIEW_CLASS_BPTC_FLOAT                                      0x82D3
#define GL_UNIFORM                                                    0x92E1
#define GL_UNIFORM_BLOCK                                              0x92E2
#define GL_PROGRAM_INPUT                                              0x92E3
#define GL_PROGRAM_OUTPUT                                             0x92E4
#define GL_BUFFER_VARIABLE                                            0x92E5
#define GL_SHADER_STORAGE_BLOCK                                       0x92E6
#define GL_VERTEX_SUBROUTINE                                          0x92E8
#define GL_TESS_CONTROL_SUBROUTINE                                    0x92E9
#define GL_TESS_EVALUATION_SUBROUTINE                                 0x92EA
#define GL_GEOMETRY_SUBROUTINE                                        0x92EB
#define GL_FRAGMENT_SUBROUTINE                                        0x92EC
#define GL_COMPUTE_SUBROUTINE                                         0x92ED
#define GL_VERTEX_SUBROUTINE_UNIFORM                                  0x92EE
#define GL_TESS_CONTROL_SUBROUTINE_UNIFORM                            0x92EF
#define GL_TESS_EVALUATION_SUBROUTINE_UNIFORM                         0x92F0
#define GL_GEOMETRY_SUBROUTINE_UNIFORM                                0x92F1
#define GL_FRAGMENT_SUBROUTINE_UNIFORM                                0x92F2
#define GL_COMPUTE_SUBROUTINE_UNIFORM                                 0x92F3
#define GL_TRANSFORM_FEEDBACK_VARYING                                 0x92F4
#define GL_ACTIVE_RESOURCES                                           0x92F5
#define GL_MAX_NAME_LENGTH                                            0x92F6
#define GL_MAX_NUM_ACTIVE_VARIABLES                                   0x92F7
#define GL_MAX_NUM_COMPATIBLE_SUBROUTINES                             0x92F8
#define GL_NAME_LENGTH                                                0x92F9
#define GL_TYPE                                                       0x92FA
#define GL_ARRAY_SIZE                                                 0x92FB
#define GL_OFFSET                                                     0x92FC
#define GL_BLOCK_INDEX                                                0x92FD
#define GL_ARRAY_STRIDE                                               0x92FE
#define GL_MATRIX_STRIDE                                              0x92FF
#define GL_IS_ROW_MAJOR                                               0x9300
#define GL_ATOMIC_COUNTER_BUFFER_INDEX                                0x9301
#define GL_BUFFER_BINDING                                             0x9302
#define GL_BUFFER_DATA_SIZE                                           0x9303
#define GL_NUM_ACTIVE_VARIABLES                                       0x9304
#define GL_ACTIVE_VARIABLES                                           0x9305
#define GL_REFERENCED_BY_VERTEX_SHADER                                0x9306
#define GL_REFERENCED_BY_TESS_CONTROL_SHADER                          0x9307
#define GL_REFERENCED_BY_TESS_EVALUATION_SHADER                       0x9308
#define GL_REFERENCED_BY_GEOMETRY_SHADER                              0x9309
#define GL_REFERENCED_BY_FRAGMENT_SHADER                              0x930A
#define GL_REFERENCED_BY_COMPUTE_SHADER                               0x930B
#define GL_TOP_LEVEL_ARRAY_SIZE                                       0x930C
#define GL_TOP_LEVEL_ARRAY_STRIDE                                     0x930D
#define GL_LOCATION                                                   0x930E
#define GL_LOCATION_INDEX                                             0x930F
#define GL_IS_PER_PATCH                                               0x92E7
#define GL_SHADER_STORAGE_BUFFER                                      0x90D2
#define GL_SHADER_STORAGE_BUFFER_BINDING                              0x90D3
#define GL_SHADER_STORAGE_BUFFER_START                                0x90D4
#define GL_SHADER_STORAGE_BUFFER_SIZE                                 0x90D5
#define GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS                           0x90D6
#define GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS                         0x90D7
#define GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS                     0x90D8
#define GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS                  0x90D9
#define GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS                         0x90DA
#define GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS                          0x90DB
#define GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS                         0x90DC
#define GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS                         0x90DD
#define GL_MAX_SHADER_STORAGE_BLOCK_SIZE                              0x90DE
#define GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT                     0x90DF
#define GL_SHADER_STORAGE_BARRIER_BIT                                 0x00002000
#define GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES                       0x8F39
#define GL_DEPTH_STENCIL_TEXTURE_MODE                                 0x90EA
#define GL_TEXTURE_BUFFER_OFFSET                                      0x919D
#define GL_TEXTURE_BUFFER_SIZE                                        0x919E
#define GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT                            0x919F
#define GL_TEXTURE_VIEW_MIN_LEVEL                                     0x82DB
#define GL_TEXTURE_VIEW_NUM_LEVELS                                    0x82DC
#define GL_TEXTURE_VIEW_MIN_LAYER                                     0x82DD
#define GL_TEXTURE_VIEW_NUM_LAYERS                                    0x82DE
#define GL_TEXTURE_IMMUTABLE_LEVELS                                   0x82DF
#define GL_VERTEX_ATTRIB_BINDING                                      0x82D4
#define GL_VERTEX_ATTRIB_RELATIVE_OFFSET                              0x82D5
#define GL_VERTEX_BINDING_DIVISOR                                     0x82D6
#define GL_VERTEX_BINDING_OFFSET                                      0x82D7
#define GL_VERTEX_BINDING_STRIDE                                      0x82D8
#define GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET                          0x82D9
#define GL_MAX_VERTEX_ATTRIB_BINDINGS                                 0x82DA
#define GL_VERTEX_BINDING_BUFFER                                      0x8F4F
#define GL_DISPLAY_LIST                                               0x82E7
#define GL_MAX_VERTEX_ATTRIB_STRIDE                                   0x82E5
#define GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED                    0x8221
#define GL_TEXTURE_BUFFER_BINDING                                     0x8C2A
#define GL_MAP_PERSISTENT_BIT                                         0x0040
#define GL_MAP_COHERENT_BIT                                           0x0080
#define GL_DYNAMIC_STORAGE_BIT                                        0x0100
#define GL_CLIENT_STORAGE_BIT                                         0x0200
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT                           0x00004000
#define GL_BUFFER_IMMUTABLE_STORAGE                                   0x821F
#define GL_BUFFER_STORAGE_FLAGS                                       0x8220
#define GL_CLEAR_TEXTURE                                              0x9365
#define GL_LOCATION_COMPONENT                                         0x934A
#define GL_TRANSFORM_FEEDBACK_BUFFER_INDEX                            0x934B
#define GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE                           0x934C
#define GL_QUERY_BUFFER                                               0x9192
#define GL_QUERY_BUFFER_BARRIER_BIT                                   0x00008000
#define GL_QUERY_BUFFER_BINDING                                       0x9193
#define GL_QUERY_RESULT_NO_WAIT                                       0x9194
#define GL_MIRROR_CLAMP_TO_EDGE                                       0x8743
#define GL_CONTEXT_LOST                                               0x0507
#define GL_NEGATIVE_ONE_TO_ONE                                        0x935E
#define GL_ZERO_TO_ONE                                                0x935F
#define GL_CLIP_ORIGIN                                                0x935C
#define GL_CLIP_DEPTH_MODE                                            0x935D
#define GL_QUERY_WAIT_INVERTED                                        0x8E17
#define GL_QUERY_NO_WAIT_INVERTED                                     0x8E18
#define GL_QUERY_BY_REGION_WAIT_INVERTED                              0x8E19
#define GL_QUERY_BY_REGION_NO_WAIT_INVERTED                           0x8E1A
#define GL_MAX_CULL_DISTANCES                                         0x82F9
#define GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES                       0x82FA
#define GL_TEXTURE_TARGET                                             0x1006
#define GL_QUERY_TARGET                                               0x82EA
#define GL_GUILTY_CONTEXT_RESET                                       0x8253
#define GL_INNOCENT_CONTEXT_RESET                                     0x8254
#define GL_UNKNOWN_CONTEXT_RESET                                      0x8255
#define GL_RESET_NOTIFICATION_STRATEGY                                0x8256
#define GL_LOSE_CONTEXT_ON_RESET                                      0x8252
#define GL_NO_RESET_NOTIFICATION                                      0x8261
#define GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT                             0x00000004
#define GL_CONTEXT_RELEASE_BEHAVIOR                                   0x82FB
#define GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH                             0x82FC
#ifndef GL_VERSION_1_0
	#define GL_VERSION_1_0                                              1
GLAPI int GLAD_GL_VERSION_1_0;
typedef void (APIENTRYP           PFNGLCULLFACEPROC)(GLenum);
GLAPI PFNGLCULLFACEPROC glad_glCullFace;
typedef void (APIENTRYP           PFNGLFRONTFACEPROC)(GLenum);
GLAPI PFNGLFRONTFACEPROC glad_glFrontFace;
typedef void (APIENTRYP           PFNGLHINTPROC)(GLenum, GLenum);
GLAPI PFNGLHINTPROC glad_glHint;
typedef void (APIENTRYP           PFNGLLINEWIDTHPROC)(GLfloat);
GLAPI PFNGLLINEWIDTHPROC glad_glLineWidth;
typedef void (APIENTRYP           PFNGLPOINTSIZEPROC)(GLfloat);
GLAPI PFNGLPOINTSIZEPROC glad_glPointSize;
typedef void (APIENTRYP           PFNGLPOLYGONMODEPROC)(GLenum, GLenum);
GLAPI PFNGLPOLYGONMODEPROC glad_glPolygonMode;
typedef void (APIENTRYP           PFNGLSCISSORPROC)(GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLSCISSORPROC glad_glScissor;
typedef void (APIENTRYP           PFNGLTEXPARAMETERFPROC)(GLenum, GLenum, GLfloat);
GLAPI PFNGLTEXPARAMETERFPROC glad_glTexParameterf;
typedef void (APIENTRYP           PFNGLTEXPARAMETERFVPROC)(GLenum, GLenum, const GLfloat*);
GLAPI PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv;
typedef void (APIENTRYP           PFNGLTEXPARAMETERIPROC)(GLenum, GLenum, GLint);
GLAPI PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
typedef void (APIENTRYP           PFNGLTEXPARAMETERIVPROC)(GLenum, GLenum, const GLint*);
GLAPI PFNGLTEXPARAMETERIVPROC glad_glTexParameteriv;
typedef void (APIENTRYP           PFNGLTEXIMAGE1DPROC)(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLTEXIMAGE1DPROC glad_glTexImage1D;
typedef void (APIENTRYP           PFNGLTEXIMAGE2DPROC)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
typedef void (APIENTRYP           PFNGLDRAWBUFFERPROC)(GLenum);
GLAPI PFNGLDRAWBUFFERPROC glad_glDrawBuffer;
typedef void (APIENTRYP           PFNGLCLEARPROC)(GLbitfield);
GLAPI PFNGLCLEARPROC glad_glClear;
typedef void (APIENTRYP           PFNGLCLEARCOLORPROC)(GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLCLEARCOLORPROC glad_glClearColor;
typedef void (APIENTRYP           PFNGLCLEARSTENCILPROC)(GLint);
GLAPI PFNGLCLEARSTENCILPROC glad_glClearStencil;
typedef void (APIENTRYP           PFNGLCLEARDEPTHPROC)(GLdouble);
GLAPI PFNGLCLEARDEPTHPROC glad_glClearDepth;
typedef void (APIENTRYP           PFNGLSTENCILMASKPROC)(GLuint);
GLAPI PFNGLSTENCILMASKPROC glad_glStencilMask;
typedef void (APIENTRYP           PFNGLCOLORMASKPROC)(GLboolean, GLboolean, GLboolean, GLboolean);
GLAPI PFNGLCOLORMASKPROC glad_glColorMask;
typedef void (APIENTRYP           PFNGLDEPTHMASKPROC)(GLboolean);
GLAPI PFNGLDEPTHMASKPROC glad_glDepthMask;
typedef void (APIENTRYP           PFNGLDISABLEPROC)(GLenum);
GLAPI PFNGLDISABLEPROC glad_glDisable;
typedef void (APIENTRYP           PFNGLENABLEPROC)(GLenum);
GLAPI PFNGLENABLEPROC glad_glEnable;
typedef void (APIENTRYP           PFNGLFINISHPROC)();
GLAPI PFNGLFINISHPROC glad_glFinish;
typedef void (APIENTRYP           PFNGLFLUSHPROC)();
GLAPI PFNGLFLUSHPROC glad_glFlush;
typedef void (APIENTRYP           PFNGLBLENDFUNCPROC)(GLenum, GLenum);
GLAPI PFNGLBLENDFUNCPROC glad_glBlendFunc;
typedef void (APIENTRYP           PFNGLLOGICOPPROC)(GLenum);
GLAPI PFNGLLOGICOPPROC glad_glLogicOp;
typedef void (APIENTRYP           PFNGLSTENCILFUNCPROC)(GLenum, GLint, GLuint);
GLAPI PFNGLSTENCILFUNCPROC glad_glStencilFunc;
typedef void (APIENTRYP           PFNGLSTENCILOPPROC)(GLenum, GLenum, GLenum);
GLAPI PFNGLSTENCILOPPROC glad_glStencilOp;
typedef void (APIENTRYP           PFNGLDEPTHFUNCPROC)(GLenum);
GLAPI PFNGLDEPTHFUNCPROC glad_glDepthFunc;
typedef void (APIENTRYP           PFNGLPIXELSTOREFPROC)(GLenum, GLfloat);
GLAPI PFNGLPIXELSTOREFPROC glad_glPixelStoref;
typedef void (APIENTRYP           PFNGLPIXELSTOREIPROC)(GLenum, GLint);
GLAPI PFNGLPIXELSTOREIPROC glad_glPixelStorei;
typedef void (APIENTRYP           PFNGLREADBUFFERPROC)(GLenum);
GLAPI PFNGLREADBUFFERPROC glad_glReadBuffer;
typedef void (APIENTRYP           PFNGLREADPIXELSPROC)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
GLAPI PFNGLREADPIXELSPROC glad_glReadPixels;
typedef void (APIENTRYP           PFNGLGETBOOLEANVPROC)(GLenum, GLboolean*);
GLAPI PFNGLGETBOOLEANVPROC glad_glGetBooleanv;
typedef void (APIENTRYP           PFNGLGETDOUBLEVPROC)(GLenum, GLdouble*);
GLAPI PFNGLGETDOUBLEVPROC glad_glGetDoublev;
typedef GLenum (APIENTRYP         PFNGLGETERRORPROC)();
GLAPI PFNGLGETERRORPROC glad_glGetError;
typedef void (APIENTRYP           PFNGLGETFLOATVPROC)(GLenum, GLfloat*);
GLAPI PFNGLGETFLOATVPROC glad_glGetFloatv;
typedef void (APIENTRYP           PFNGLGETINTEGERVPROC)(GLenum, GLint*);
GLAPI PFNGLGETINTEGERVPROC glad_glGetIntegerv;
typedef const GLubyte* (APIENTRYP PFNGLGETSTRINGPROC)(GLenum);
GLAPI PFNGLGETSTRINGPROC glad_glGetString;
typedef void (APIENTRYP           PFNGLGETTEXIMAGEPROC)(GLenum, GLint, GLenum, GLenum, void*);
GLAPI PFNGLGETTEXIMAGEPROC glad_glGetTexImage;
typedef void (APIENTRYP           PFNGLGETTEXPARAMETERFVPROC)(GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETTEXPARAMETERFVPROC glad_glGetTexParameterfv;
typedef void (APIENTRYP           PFNGLGETTEXPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETTEXPARAMETERIVPROC glad_glGetTexParameteriv;
typedef void (APIENTRYP           PFNGLGETTEXLEVELPARAMETERFVPROC)(GLenum, GLint, GLenum, GLfloat*);
GLAPI PFNGLGETTEXLEVELPARAMETERFVPROC glad_glGetTexLevelParameterfv;
typedef void (APIENTRYP           PFNGLGETTEXLEVELPARAMETERIVPROC)(GLenum, GLint, GLenum, GLint*);
GLAPI PFNGLGETTEXLEVELPARAMETERIVPROC glad_glGetTexLevelParameteriv;
typedef GLboolean (APIENTRYP      PFNGLISENABLEDPROC)(GLenum);
GLAPI PFNGLISENABLEDPROC glad_glIsEnabled;
typedef void (APIENTRYP           PFNGLDEPTHRANGEPROC)(GLdouble, GLdouble);
GLAPI PFNGLDEPTHRANGEPROC glad_glDepthRange;
typedef void (APIENTRYP           PFNGLVIEWPORTPROC)(GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLVIEWPORTPROC glad_glViewport;
#endif
#ifndef GL_VERSION_1_1
	#define GL_VERSION_1_1 1
GLAPI int GLAD_GL_VERSION_1_1;
typedef void (APIENTRYP      PFNGLDRAWARRAYSPROC)(GLenum, GLint, GLsizei);
GLAPI PFNGLDRAWARRAYSPROC glad_glDrawArrays;
typedef void (APIENTRYP      PFNGLDRAWELEMENTSPROC)(GLenum, GLsizei, GLenum, const void*);
GLAPI PFNGLDRAWELEMENTSPROC glad_glDrawElements;
typedef void (APIENTRYP      PFNGLPOLYGONOFFSETPROC)(GLfloat, GLfloat);
GLAPI PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset;
typedef void (APIENTRYP      PFNGLCOPYTEXIMAGE1DPROC)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint);
GLAPI PFNGLCOPYTEXIMAGE1DPROC glad_glCopyTexImage1D;
typedef void (APIENTRYP      PFNGLCOPYTEXIMAGE2DPROC)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
GLAPI PFNGLCOPYTEXIMAGE2DPROC glad_glCopyTexImage2D;
typedef void (APIENTRYP      PFNGLCOPYTEXSUBIMAGE1DPROC)(GLenum, GLint, GLint, GLint, GLint, GLsizei);
GLAPI PFNGLCOPYTEXSUBIMAGE1DPROC glad_glCopyTexSubImage1D;
typedef void (APIENTRYP      PFNGLCOPYTEXSUBIMAGE2DPROC)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYTEXSUBIMAGE2DPROC glad_glCopyTexSubImage2D;
typedef void (APIENTRYP      PFNGLTEXSUBIMAGE1DPROC)(GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXSUBIMAGE1DPROC glad_glTexSubImage1D;
typedef void (APIENTRYP      PFNGLTEXSUBIMAGE2DPROC)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D;
typedef void (APIENTRYP      PFNGLBINDTEXTUREPROC)(GLenum, GLuint);
GLAPI PFNGLBINDTEXTUREPROC glad_glBindTexture;
typedef void (APIENTRYP      PFNGLDELETETEXTURESPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
typedef void (APIENTRYP      PFNGLGENTEXTURESPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENTEXTURESPROC glad_glGenTextures;
typedef GLboolean (APIENTRYP PFNGLISTEXTUREPROC)(GLuint);
GLAPI PFNGLISTEXTUREPROC glad_glIsTexture;
#endif
#ifndef GL_VERSION_1_2
	#define GL_VERSION_1_2 1
GLAPI int GLAD_GL_VERSION_1_2;
typedef void (APIENTRYP PFNGLDRAWRANGEELEMENTSPROC)(GLenum, GLuint, GLuint, GLsizei, GLenum, const void*);
GLAPI PFNGLDRAWRANGEELEMENTSPROC glad_glDrawRangeElements;
typedef void (APIENTRYP PFNGLTEXIMAGE3DPROC)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLTEXIMAGE3DPROC glad_glTexImage3D;
typedef void (APIENTRYP PFNGLTEXSUBIMAGE3DPROC)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXSUBIMAGE3DPROC glad_glTexSubImage3D;
typedef void (APIENTRYP PFNGLCOPYTEXSUBIMAGE3DPROC)(GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYTEXSUBIMAGE3DPROC glad_glCopyTexSubImage3D;
#endif
#ifndef GL_VERSION_1_3
	#define GL_VERSION_1_3 1
GLAPI int GLAD_GL_VERSION_1_3;
typedef void (APIENTRYP PFNGLACTIVETEXTUREPROC)(GLenum);
GLAPI PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
typedef void (APIENTRYP PFNGLSAMPLECOVERAGEPROC)(GLfloat, GLboolean);
GLAPI PFNGLSAMPLECOVERAGEPROC glad_glSampleCoverage;
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXIMAGE3DPROC)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_glCompressedTexImage3D;
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXIMAGE2DPROC)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_glCompressedTexImage2D;
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXIMAGE1DPROC)(GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXIMAGE1DPROC glad_glCompressedTexImage1D;
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_glCompressedTexSubImage3D;
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_glCompressedTexSubImage2D;
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)(GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glad_glCompressedTexSubImage1D;
typedef void (APIENTRYP PFNGLGETCOMPRESSEDTEXIMAGEPROC)(GLenum, GLint, void*);
GLAPI PFNGLGETCOMPRESSEDTEXIMAGEPROC glad_glGetCompressedTexImage;
#endif
#ifndef GL_VERSION_1_4
	#define GL_VERSION_1_4 1
GLAPI int GLAD_GL_VERSION_1_4;
typedef void (APIENTRYP PFNGLBLENDFUNCSEPARATEPROC)(GLenum, GLenum, GLenum, GLenum);
GLAPI PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate;
typedef void (APIENTRYP PFNGLMULTIDRAWARRAYSPROC)(GLenum, const GLint*, const GLsizei*, GLsizei);
GLAPI PFNGLMULTIDRAWARRAYSPROC glad_glMultiDrawArrays;
typedef void (APIENTRYP PFNGLMULTIDRAWELEMENTSPROC)(GLenum, const GLsizei*, GLenum, const void**, GLsizei);
GLAPI PFNGLMULTIDRAWELEMENTSPROC glad_glMultiDrawElements;
typedef void (APIENTRYP PFNGLPOINTPARAMETERFPROC)(GLenum, GLfloat);
GLAPI PFNGLPOINTPARAMETERFPROC glad_glPointParameterf;
typedef void (APIENTRYP PFNGLPOINTPARAMETERFVPROC)(GLenum, const GLfloat*);
GLAPI PFNGLPOINTPARAMETERFVPROC glad_glPointParameterfv;
typedef void (APIENTRYP PFNGLPOINTPARAMETERIPROC)(GLenum, GLint);
GLAPI PFNGLPOINTPARAMETERIPROC glad_glPointParameteri;
typedef void (APIENTRYP PFNGLPOINTPARAMETERIVPROC)(GLenum, const GLint*);
GLAPI PFNGLPOINTPARAMETERIVPROC glad_glPointParameteriv;
typedef void (APIENTRYP PFNGLBLENDCOLORPROC)(GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLBLENDCOLORPROC glad_glBlendColor;
typedef void (APIENTRYP PFNGLBLENDEQUATIONPROC)(GLenum);
GLAPI PFNGLBLENDEQUATIONPROC glad_glBlendEquation;
#endif
#ifndef GL_VERSION_1_5
	#define GL_VERSION_1_5 1
GLAPI int GLAD_GL_VERSION_1_5;
typedef void (APIENTRYP      PFNGLGENQUERIESPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENQUERIESPROC glad_glGenQueries;
typedef void (APIENTRYP      PFNGLDELETEQUERIESPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETEQUERIESPROC glad_glDeleteQueries;
typedef GLboolean (APIENTRYP PFNGLISQUERYPROC)(GLuint);
GLAPI PFNGLISQUERYPROC glad_glIsQuery;
typedef void (APIENTRYP      PFNGLBEGINQUERYPROC)(GLenum, GLuint);
GLAPI PFNGLBEGINQUERYPROC glad_glBeginQuery;
typedef void (APIENTRYP      PFNGLENDQUERYPROC)(GLenum);
GLAPI PFNGLENDQUERYPROC glad_glEndQuery;
typedef void (APIENTRYP      PFNGLGETQUERYIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETQUERYIVPROC glad_glGetQueryiv;
typedef void (APIENTRYP      PFNGLGETQUERYOBJECTIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETQUERYOBJECTIVPROC glad_glGetQueryObjectiv;
typedef void (APIENTRYP      PFNGLGETQUERYOBJECTUIVPROC)(GLuint, GLenum, GLuint*);
GLAPI PFNGLGETQUERYOBJECTUIVPROC glad_glGetQueryObjectuiv;
typedef void (APIENTRYP      PFNGLBINDBUFFERPROC)(GLenum, GLuint);
GLAPI PFNGLBINDBUFFERPROC glad_glBindBuffer;
typedef void (APIENTRYP      PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
typedef void (APIENTRYP      PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENBUFFERSPROC glad_glGenBuffers;
typedef GLboolean (APIENTRYP PFNGLISBUFFERPROC)(GLuint);
GLAPI PFNGLISBUFFERPROC glad_glIsBuffer;
typedef void (APIENTRYP      PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
GLAPI PFNGLBUFFERDATAPROC glad_glBufferData;
typedef void (APIENTRYP      PFNGLBUFFERSUBDATAPROC)(GLenum, GLintptr, GLsizeiptr, const void*);
GLAPI PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
typedef void (APIENTRYP      PFNGLGETBUFFERSUBDATAPROC)(GLenum, GLintptr, GLsizeiptr, void*);
GLAPI PFNGLGETBUFFERSUBDATAPROC glad_glGetBufferSubData;
typedef void* (APIENTRYP     PFNGLMAPBUFFERPROC)(GLenum, GLenum);
GLAPI PFNGLMAPBUFFERPROC glad_glMapBuffer;
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERPROC)(GLenum);
GLAPI PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer;
typedef void (APIENTRYP      PFNGLGETBUFFERPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETBUFFERPARAMETERIVPROC glad_glGetBufferParameteriv;
typedef void (APIENTRYP      PFNGLGETBUFFERPOINTERVPROC)(GLenum, GLenum, void**);
GLAPI PFNGLGETBUFFERPOINTERVPROC glad_glGetBufferPointerv;
#endif
#ifndef GL_VERSION_2_0
	#define GL_VERSION_2_0 1
GLAPI int GLAD_GL_VERSION_2_0;
typedef void (APIENTRYP      PFNGLBLENDEQUATIONSEPARATEPROC)(GLenum, GLenum);
GLAPI PFNGLBLENDEQUATIONSEPARATEPROC glad_glBlendEquationSeparate;
typedef void (APIENTRYP      PFNGLDRAWBUFFERSPROC)(GLsizei, const GLenum*);
GLAPI PFNGLDRAWBUFFERSPROC glad_glDrawBuffers;
typedef void (APIENTRYP      PFNGLSTENCILOPSEPARATEPROC)(GLenum, GLenum, GLenum, GLenum);
GLAPI PFNGLSTENCILOPSEPARATEPROC glad_glStencilOpSeparate;
typedef void (APIENTRYP      PFNGLSTENCILFUNCSEPARATEPROC)(GLenum, GLenum, GLint, GLuint);
GLAPI PFNGLSTENCILFUNCSEPARATEPROC glad_glStencilFuncSeparate;
typedef void (APIENTRYP      PFNGLSTENCILMASKSEPARATEPROC)(GLenum, GLuint);
GLAPI PFNGLSTENCILMASKSEPARATEPROC glad_glStencilMaskSeparate;
typedef void (APIENTRYP      PFNGLATTACHSHADERPROC)(GLuint, GLuint);
GLAPI PFNGLATTACHSHADERPROC glad_glAttachShader;
typedef void (APIENTRYP      PFNGLBINDATTRIBLOCATIONPROC)(GLuint, GLuint, const GLchar*);
GLAPI PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation;
typedef void (APIENTRYP      PFNGLCOMPILESHADERPROC)(GLuint);
GLAPI PFNGLCOMPILESHADERPROC glad_glCompileShader;
typedef GLuint (APIENTRYP    PFNGLCREATEPROGRAMPROC)();
GLAPI PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
typedef GLuint (APIENTRYP    PFNGLCREATESHADERPROC)(GLenum);
GLAPI PFNGLCREATESHADERPROC glad_glCreateShader;
typedef void (APIENTRYP      PFNGLDELETEPROGRAMPROC)(GLuint);
GLAPI PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
typedef void (APIENTRYP      PFNGLDELETESHADERPROC)(GLuint);
GLAPI PFNGLDELETESHADERPROC glad_glDeleteShader;
typedef void (APIENTRYP      PFNGLDETACHSHADERPROC)(GLuint, GLuint);
GLAPI PFNGLDETACHSHADERPROC glad_glDetachShader;
typedef void (APIENTRYP      PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint);
GLAPI PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
typedef void (APIENTRYP      PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
GLAPI PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
typedef void (APIENTRYP      PFNGLGETACTIVEATTRIBPROC)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*);
GLAPI PFNGLGETACTIVEATTRIBPROC glad_glGetActiveAttrib;
typedef void (APIENTRYP      PFNGLGETACTIVEUNIFORMPROC)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*);
GLAPI PFNGLGETACTIVEUNIFORMPROC glad_glGetActiveUniform;
typedef void (APIENTRYP      PFNGLGETATTACHEDSHADERSPROC)(GLuint, GLsizei, GLsizei*, GLuint*);
GLAPI PFNGLGETATTACHEDSHADERSPROC glad_glGetAttachedShaders;
typedef GLint (APIENTRYP     PFNGLGETATTRIBLOCATIONPROC)(GLuint, const GLchar*);
GLAPI PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation;
typedef void (APIENTRYP      PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
typedef void (APIENTRYP      PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
typedef void (APIENTRYP      PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETSHADERIVPROC glad_glGetShaderiv;
typedef void (APIENTRYP      PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
typedef void (APIENTRYP      PFNGLGETSHADERSOURCEPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETSHADERSOURCEPROC glad_glGetShaderSource;
typedef GLint (APIENTRYP     PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
GLAPI PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
typedef void (APIENTRYP      PFNGLGETUNIFORMFVPROC)(GLuint, GLint, GLfloat*);
GLAPI PFNGLGETUNIFORMFVPROC glad_glGetUniformfv;
typedef void (APIENTRYP      PFNGLGETUNIFORMIVPROC)(GLuint, GLint, GLint*);
GLAPI PFNGLGETUNIFORMIVPROC glad_glGetUniformiv;
typedef void (APIENTRYP      PFNGLGETVERTEXATTRIBDVPROC)(GLuint, GLenum, GLdouble*);
GLAPI PFNGLGETVERTEXATTRIBDVPROC glad_glGetVertexAttribdv;
typedef void (APIENTRYP      PFNGLGETVERTEXATTRIBFVPROC)(GLuint, GLenum, GLfloat*);
GLAPI PFNGLGETVERTEXATTRIBFVPROC glad_glGetVertexAttribfv;
typedef void (APIENTRYP      PFNGLGETVERTEXATTRIBIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETVERTEXATTRIBIVPROC glad_glGetVertexAttribiv;
typedef void (APIENTRYP      PFNGLGETVERTEXATTRIBPOINTERVPROC)(GLuint, GLenum, void**);
GLAPI PFNGLGETVERTEXATTRIBPOINTERVPROC glad_glGetVertexAttribPointerv;
typedef GLboolean (APIENTRYP PFNGLISPROGRAMPROC)(GLuint);
GLAPI PFNGLISPROGRAMPROC glad_glIsProgram;
typedef GLboolean (APIENTRYP PFNGLISSHADERPROC)(GLuint);
GLAPI PFNGLISSHADERPROC glad_glIsShader;
typedef void (APIENTRYP      PFNGLLINKPROGRAMPROC)(GLuint);
GLAPI PFNGLLINKPROGRAMPROC glad_glLinkProgram;
typedef void (APIENTRYP      PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar**, const GLint*);
GLAPI PFNGLSHADERSOURCEPROC glad_glShaderSource;
typedef void (APIENTRYP      PFNGLUSEPROGRAMPROC)(GLuint);
GLAPI PFNGLUSEPROGRAMPROC glad_glUseProgram;
typedef void (APIENTRYP      PFNGLUNIFORM1FPROC)(GLint, GLfloat);
GLAPI PFNGLUNIFORM1FPROC glad_glUniform1f;
typedef void (APIENTRYP      PFNGLUNIFORM2FPROC)(GLint, GLfloat, GLfloat);
GLAPI PFNGLUNIFORM2FPROC glad_glUniform2f;
typedef void (APIENTRYP      PFNGLUNIFORM3FPROC)(GLint, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLUNIFORM3FPROC glad_glUniform3f;
typedef void (APIENTRYP      PFNGLUNIFORM4FPROC)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLUNIFORM4FPROC glad_glUniform4f;
typedef void (APIENTRYP      PFNGLUNIFORM1IPROC)(GLint, GLint);
GLAPI PFNGLUNIFORM1IPROC glad_glUniform1i;
typedef void (APIENTRYP      PFNGLUNIFORM2IPROC)(GLint, GLint, GLint);
GLAPI PFNGLUNIFORM2IPROC glad_glUniform2i;
typedef void (APIENTRYP      PFNGLUNIFORM3IPROC)(GLint, GLint, GLint, GLint);
GLAPI PFNGLUNIFORM3IPROC glad_glUniform3i;
typedef void (APIENTRYP      PFNGLUNIFORM4IPROC)(GLint, GLint, GLint, GLint, GLint);
GLAPI PFNGLUNIFORM4IPROC glad_glUniform4i;
typedef void (APIENTRYP      PFNGLUNIFORM1FVPROC)(GLint, GLsizei, const GLfloat*);
GLAPI PFNGLUNIFORM1FVPROC glad_glUniform1fv;
typedef void (APIENTRYP      PFNGLUNIFORM2FVPROC)(GLint, GLsizei, const GLfloat*);
GLAPI PFNGLUNIFORM2FVPROC glad_glUniform2fv;
typedef void (APIENTRYP      PFNGLUNIFORM3FVPROC)(GLint, GLsizei, const GLfloat*);
GLAPI PFNGLUNIFORM3FVPROC glad_glUniform3fv;
typedef void (APIENTRYP      PFNGLUNIFORM4FVPROC)(GLint, GLsizei, const GLfloat*);
GLAPI PFNGLUNIFORM4FVPROC glad_glUniform4fv;
typedef void (APIENTRYP      PFNGLUNIFORM1IVPROC)(GLint, GLsizei, const GLint*);
GLAPI PFNGLUNIFORM1IVPROC glad_glUniform1iv;
typedef void (APIENTRYP      PFNGLUNIFORM2IVPROC)(GLint, GLsizei, const GLint*);
GLAPI PFNGLUNIFORM2IVPROC glad_glUniform2iv;
typedef void (APIENTRYP      PFNGLUNIFORM3IVPROC)(GLint, GLsizei, const GLint*);
GLAPI PFNGLUNIFORM3IVPROC glad_glUniform3iv;
typedef void (APIENTRYP      PFNGLUNIFORM4IVPROC)(GLint, GLsizei, const GLint*);
GLAPI PFNGLUNIFORM4IVPROC glad_glUniform4iv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX2FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX2FVPROC glad_glUniformMatrix2fv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX3FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
typedef void (APIENTRYP      PFNGLVALIDATEPROGRAMPROC)(GLuint);
GLAPI PFNGLVALIDATEPROGRAMPROC glad_glValidateProgram;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB1DPROC)(GLuint, GLdouble);
GLAPI PFNGLVERTEXATTRIB1DPROC glad_glVertexAttrib1d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB1DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIB1DVPROC glad_glVertexAttrib1dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB1FPROC)(GLuint, GLfloat);
GLAPI PFNGLVERTEXATTRIB1FPROC glad_glVertexAttrib1f;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB1FVPROC)(GLuint, const GLfloat*);
GLAPI PFNGLVERTEXATTRIB1FVPROC glad_glVertexAttrib1fv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB1SPROC)(GLuint, GLshort);
GLAPI PFNGLVERTEXATTRIB1SPROC glad_glVertexAttrib1s;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB1SVPROC)(GLuint, const GLshort*);
GLAPI PFNGLVERTEXATTRIB1SVPROC glad_glVertexAttrib1sv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB2DPROC)(GLuint, GLdouble, GLdouble);
GLAPI PFNGLVERTEXATTRIB2DPROC glad_glVertexAttrib2d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB2DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIB2DVPROC glad_glVertexAttrib2dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB2FPROC)(GLuint, GLfloat, GLfloat);
GLAPI PFNGLVERTEXATTRIB2FPROC glad_glVertexAttrib2f;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB2FVPROC)(GLuint, const GLfloat*);
GLAPI PFNGLVERTEXATTRIB2FVPROC glad_glVertexAttrib2fv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB2SPROC)(GLuint, GLshort, GLshort);
GLAPI PFNGLVERTEXATTRIB2SPROC glad_glVertexAttrib2s;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB2SVPROC)(GLuint, const GLshort*);
GLAPI PFNGLVERTEXATTRIB2SVPROC glad_glVertexAttrib2sv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB3DPROC)(GLuint, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLVERTEXATTRIB3DPROC glad_glVertexAttrib3d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB3DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIB3DVPROC glad_glVertexAttrib3dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB3FPROC)(GLuint, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLVERTEXATTRIB3FPROC glad_glVertexAttrib3f;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB3FVPROC)(GLuint, const GLfloat*);
GLAPI PFNGLVERTEXATTRIB3FVPROC glad_glVertexAttrib3fv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB3SPROC)(GLuint, GLshort, GLshort, GLshort);
GLAPI PFNGLVERTEXATTRIB3SPROC glad_glVertexAttrib3s;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB3SVPROC)(GLuint, const GLshort*);
GLAPI PFNGLVERTEXATTRIB3SVPROC glad_glVertexAttrib3sv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4NBVPROC)(GLuint, const GLbyte*);
GLAPI PFNGLVERTEXATTRIB4NBVPROC glad_glVertexAttrib4Nbv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4NIVPROC)(GLuint, const GLint*);
GLAPI PFNGLVERTEXATTRIB4NIVPROC glad_glVertexAttrib4Niv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4NSVPROC)(GLuint, const GLshort*);
GLAPI PFNGLVERTEXATTRIB4NSVPROC glad_glVertexAttrib4Nsv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4NUBPROC)(GLuint, GLubyte, GLubyte, GLubyte, GLubyte);
GLAPI PFNGLVERTEXATTRIB4NUBPROC glad_glVertexAttrib4Nub;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4NUBVPROC)(GLuint, const GLubyte*);
GLAPI PFNGLVERTEXATTRIB4NUBVPROC glad_glVertexAttrib4Nubv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4NUIVPROC)(GLuint, const GLuint*);
GLAPI PFNGLVERTEXATTRIB4NUIVPROC glad_glVertexAttrib4Nuiv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4NUSVPROC)(GLuint, const GLushort*);
GLAPI PFNGLVERTEXATTRIB4NUSVPROC glad_glVertexAttrib4Nusv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4BVPROC)(GLuint, const GLbyte*);
GLAPI PFNGLVERTEXATTRIB4BVPROC glad_glVertexAttrib4bv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4DPROC)(GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLVERTEXATTRIB4DPROC glad_glVertexAttrib4d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIB4DVPROC glad_glVertexAttrib4dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4FPROC)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLVERTEXATTRIB4FPROC glad_glVertexAttrib4f;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4FVPROC)(GLuint, const GLfloat*);
GLAPI PFNGLVERTEXATTRIB4FVPROC glad_glVertexAttrib4fv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4IVPROC)(GLuint, const GLint*);
GLAPI PFNGLVERTEXATTRIB4IVPROC glad_glVertexAttrib4iv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4SPROC)(GLuint, GLshort, GLshort, GLshort, GLshort);
GLAPI PFNGLVERTEXATTRIB4SPROC glad_glVertexAttrib4s;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4SVPROC)(GLuint, const GLshort*);
GLAPI PFNGLVERTEXATTRIB4SVPROC glad_glVertexAttrib4sv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4UBVPROC)(GLuint, const GLubyte*);
GLAPI PFNGLVERTEXATTRIB4UBVPROC glad_glVertexAttrib4ubv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4UIVPROC)(GLuint, const GLuint*);
GLAPI PFNGLVERTEXATTRIB4UIVPROC glad_glVertexAttrib4uiv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIB4USVPROC)(GLuint, const GLushort*);
GLAPI PFNGLVERTEXATTRIB4USVPROC glad_glVertexAttrib4usv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
GLAPI PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
#endif
#ifndef GL_VERSION_2_1
	#define GL_VERSION_2_1 1
GLAPI int GLAD_GL_VERSION_2_1;
typedef void (APIENTRYP PFNGLUNIFORMMATRIX2X3FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX2X3FVPROC glad_glUniformMatrix2x3fv;
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3X2FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX3X2FVPROC glad_glUniformMatrix3x2fv;
typedef void (APIENTRYP PFNGLUNIFORMMATRIX2X4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX2X4FVPROC glad_glUniformMatrix2x4fv;
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4X2FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX4X2FVPROC glad_glUniformMatrix4x2fv;
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3X4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX3X4FVPROC glad_glUniformMatrix3x4fv;
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4X3FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLUNIFORMMATRIX4X3FVPROC glad_glUniformMatrix4x3fv;
#endif
#ifndef GL_VERSION_3_0
	#define GL_VERSION_3_0 1
GLAPI int GLAD_GL_VERSION_3_0;
typedef void (APIENTRYP           PFNGLCOLORMASKIPROC)(GLuint, GLboolean, GLboolean, GLboolean, GLboolean);
GLAPI PFNGLCOLORMASKIPROC glad_glColorMaski;
typedef void (APIENTRYP           PFNGLGETBOOLEANI_VPROC)(GLenum, GLuint, GLboolean*);
GLAPI PFNGLGETBOOLEANI_VPROC glad_glGetBooleani_v;
typedef void (APIENTRYP           PFNGLGETINTEGERI_VPROC)(GLenum, GLuint, GLint*);
GLAPI PFNGLGETINTEGERI_VPROC glad_glGetIntegeri_v;
typedef void (APIENTRYP           PFNGLENABLEIPROC)(GLenum, GLuint);
GLAPI PFNGLENABLEIPROC glad_glEnablei;
typedef void (APIENTRYP           PFNGLDISABLEIPROC)(GLenum, GLuint);
GLAPI PFNGLDISABLEIPROC glad_glDisablei;
typedef GLboolean (APIENTRYP      PFNGLISENABLEDIPROC)(GLenum, GLuint);
GLAPI PFNGLISENABLEDIPROC glad_glIsEnabledi;
typedef void (APIENTRYP           PFNGLBEGINTRANSFORMFEEDBACKPROC)(GLenum);
GLAPI PFNGLBEGINTRANSFORMFEEDBACKPROC glad_glBeginTransformFeedback;
typedef void (APIENTRYP           PFNGLENDTRANSFORMFEEDBACKPROC)();
GLAPI PFNGLENDTRANSFORMFEEDBACKPROC glad_glEndTransformFeedback;
typedef void (APIENTRYP           PFNGLBINDBUFFERRANGEPROC)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLBINDBUFFERRANGEPROC glad_glBindBufferRange;
typedef void (APIENTRYP           PFNGLBINDBUFFERBASEPROC)(GLenum, GLuint, GLuint);
GLAPI PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase;
typedef void (APIENTRYP           PFNGLTRANSFORMFEEDBACKVARYINGSPROC)(GLuint, GLsizei, const GLchar**, GLenum);
GLAPI PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_glTransformFeedbackVaryings;
typedef void (APIENTRYP           PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)(GLuint, GLuint, GLsizei, GLsizei*, GLsizei*, GLenum*, GLchar*);
GLAPI PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_glGetTransformFeedbackVarying;
typedef void (APIENTRYP           PFNGLCLAMPCOLORPROC)(GLenum, GLenum);
GLAPI PFNGLCLAMPCOLORPROC glad_glClampColor;
typedef void (APIENTRYP           PFNGLBEGINCONDITIONALRENDERPROC)(GLuint, GLenum);
GLAPI PFNGLBEGINCONDITIONALRENDERPROC glad_glBeginConditionalRender;
typedef void (APIENTRYP           PFNGLENDCONDITIONALRENDERPROC)();
GLAPI PFNGLENDCONDITIONALRENDERPROC glad_glEndConditionalRender;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBIPOINTERPROC)(GLuint, GLint, GLenum, GLsizei, const void*);
GLAPI PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer;
typedef void (APIENTRYP           PFNGLGETVERTEXATTRIBIIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETVERTEXATTRIBIIVPROC glad_glGetVertexAttribIiv;
typedef void (APIENTRYP           PFNGLGETVERTEXATTRIBIUIVPROC)(GLuint, GLenum, GLuint*);
GLAPI PFNGLGETVERTEXATTRIBIUIVPROC glad_glGetVertexAttribIuiv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI1IPROC)(GLuint, GLint);
GLAPI PFNGLVERTEXATTRIBI1IPROC glad_glVertexAttribI1i;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI2IPROC)(GLuint, GLint, GLint);
GLAPI PFNGLVERTEXATTRIBI2IPROC glad_glVertexAttribI2i;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI3IPROC)(GLuint, GLint, GLint, GLint);
GLAPI PFNGLVERTEXATTRIBI3IPROC glad_glVertexAttribI3i;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4IPROC)(GLuint, GLint, GLint, GLint, GLint);
GLAPI PFNGLVERTEXATTRIBI4IPROC glad_glVertexAttribI4i;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI1UIPROC)(GLuint, GLuint);
GLAPI PFNGLVERTEXATTRIBI1UIPROC glad_glVertexAttribI1ui;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI2UIPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXATTRIBI2UIPROC glad_glVertexAttribI2ui;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI3UIPROC)(GLuint, GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXATTRIBI3UIPROC glad_glVertexAttribI3ui;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4UIPROC)(GLuint, GLuint, GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXATTRIBI4UIPROC glad_glVertexAttribI4ui;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI1IVPROC)(GLuint, const GLint*);
GLAPI PFNGLVERTEXATTRIBI1IVPROC glad_glVertexAttribI1iv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI2IVPROC)(GLuint, const GLint*);
GLAPI PFNGLVERTEXATTRIBI2IVPROC glad_glVertexAttribI2iv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI3IVPROC)(GLuint, const GLint*);
GLAPI PFNGLVERTEXATTRIBI3IVPROC glad_glVertexAttribI3iv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4IVPROC)(GLuint, const GLint*);
GLAPI PFNGLVERTEXATTRIBI4IVPROC glad_glVertexAttribI4iv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI1UIVPROC)(GLuint, const GLuint*);
GLAPI PFNGLVERTEXATTRIBI1UIVPROC glad_glVertexAttribI1uiv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI2UIVPROC)(GLuint, const GLuint*);
GLAPI PFNGLVERTEXATTRIBI2UIVPROC glad_glVertexAttribI2uiv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI3UIVPROC)(GLuint, const GLuint*);
GLAPI PFNGLVERTEXATTRIBI3UIVPROC glad_glVertexAttribI3uiv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4UIVPROC)(GLuint, const GLuint*);
GLAPI PFNGLVERTEXATTRIBI4UIVPROC glad_glVertexAttribI4uiv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4BVPROC)(GLuint, const GLbyte*);
GLAPI PFNGLVERTEXATTRIBI4BVPROC glad_glVertexAttribI4bv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4SVPROC)(GLuint, const GLshort*);
GLAPI PFNGLVERTEXATTRIBI4SVPROC glad_glVertexAttribI4sv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4UBVPROC)(GLuint, const GLubyte*);
GLAPI PFNGLVERTEXATTRIBI4UBVPROC glad_glVertexAttribI4ubv;
typedef void (APIENTRYP           PFNGLVERTEXATTRIBI4USVPROC)(GLuint, const GLushort*);
GLAPI PFNGLVERTEXATTRIBI4USVPROC glad_glVertexAttribI4usv;
typedef void (APIENTRYP           PFNGLGETUNIFORMUIVPROC)(GLuint, GLint, GLuint*);
GLAPI PFNGLGETUNIFORMUIVPROC glad_glGetUniformuiv;
typedef void (APIENTRYP           PFNGLBINDFRAGDATALOCATIONPROC)(GLuint, GLuint, const GLchar*);
GLAPI PFNGLBINDFRAGDATALOCATIONPROC glad_glBindFragDataLocation;
typedef GLint (APIENTRYP          PFNGLGETFRAGDATALOCATIONPROC)(GLuint, const GLchar*);
GLAPI PFNGLGETFRAGDATALOCATIONPROC glad_glGetFragDataLocation;
typedef void (APIENTRYP           PFNGLUNIFORM1UIPROC)(GLint, GLuint);
GLAPI PFNGLUNIFORM1UIPROC glad_glUniform1ui;
typedef void (APIENTRYP           PFNGLUNIFORM2UIPROC)(GLint, GLuint, GLuint);
GLAPI PFNGLUNIFORM2UIPROC glad_glUniform2ui;
typedef void (APIENTRYP           PFNGLUNIFORM3UIPROC)(GLint, GLuint, GLuint, GLuint);
GLAPI PFNGLUNIFORM3UIPROC glad_glUniform3ui;
typedef void (APIENTRYP           PFNGLUNIFORM4UIPROC)(GLint, GLuint, GLuint, GLuint, GLuint);
GLAPI PFNGLUNIFORM4UIPROC glad_glUniform4ui;
typedef void (APIENTRYP           PFNGLUNIFORM1UIVPROC)(GLint, GLsizei, const GLuint*);
GLAPI PFNGLUNIFORM1UIVPROC glad_glUniform1uiv;
typedef void (APIENTRYP           PFNGLUNIFORM2UIVPROC)(GLint, GLsizei, const GLuint*);
GLAPI PFNGLUNIFORM2UIVPROC glad_glUniform2uiv;
typedef void (APIENTRYP           PFNGLUNIFORM3UIVPROC)(GLint, GLsizei, const GLuint*);
GLAPI PFNGLUNIFORM3UIVPROC glad_glUniform3uiv;
typedef void (APIENTRYP           PFNGLUNIFORM4UIVPROC)(GLint, GLsizei, const GLuint*);
GLAPI PFNGLUNIFORM4UIVPROC glad_glUniform4uiv;
typedef void (APIENTRYP           PFNGLTEXPARAMETERIIVPROC)(GLenum, GLenum, const GLint*);
GLAPI PFNGLTEXPARAMETERIIVPROC glad_glTexParameterIiv;
typedef void (APIENTRYP           PFNGLTEXPARAMETERIUIVPROC)(GLenum, GLenum, const GLuint*);
GLAPI PFNGLTEXPARAMETERIUIVPROC glad_glTexParameterIuiv;
typedef void (APIENTRYP           PFNGLGETTEXPARAMETERIIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETTEXPARAMETERIIVPROC glad_glGetTexParameterIiv;
typedef void (APIENTRYP           PFNGLGETTEXPARAMETERIUIVPROC)(GLenum, GLenum, GLuint*);
GLAPI PFNGLGETTEXPARAMETERIUIVPROC glad_glGetTexParameterIuiv;
typedef void (APIENTRYP           PFNGLCLEARBUFFERIVPROC)(GLenum, GLint, const GLint*);
GLAPI PFNGLCLEARBUFFERIVPROC glad_glClearBufferiv;
typedef void (APIENTRYP           PFNGLCLEARBUFFERUIVPROC)(GLenum, GLint, const GLuint*);
GLAPI PFNGLCLEARBUFFERUIVPROC glad_glClearBufferuiv;
typedef void (APIENTRYP           PFNGLCLEARBUFFERFVPROC)(GLenum, GLint, const GLfloat*);
GLAPI PFNGLCLEARBUFFERFVPROC glad_glClearBufferfv;
typedef void (APIENTRYP           PFNGLCLEARBUFFERFIPROC)(GLenum, GLint, GLfloat, GLint);
GLAPI PFNGLCLEARBUFFERFIPROC glad_glClearBufferfi;
typedef const GLubyte* (APIENTRYP PFNGLGETSTRINGIPROC)(GLenum, GLuint);
GLAPI PFNGLGETSTRINGIPROC glad_glGetStringi;
typedef GLboolean (APIENTRYP      PFNGLISRENDERBUFFERPROC)(GLuint);
GLAPI PFNGLISRENDERBUFFERPROC glad_glIsRenderbuffer;
typedef void (APIENTRYP           PFNGLBINDRENDERBUFFERPROC)(GLenum, GLuint);
GLAPI PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer;
typedef void (APIENTRYP           PFNGLDELETERENDERBUFFERSPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers;
typedef void (APIENTRYP           PFNGLGENRENDERBUFFERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers;
typedef void (APIENTRYP           PFNGLRENDERBUFFERSTORAGEPROC)(GLenum, GLenum, GLsizei, GLsizei);
GLAPI PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage;
typedef void (APIENTRYP           PFNGLGETRENDERBUFFERPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_glGetRenderbufferParameteriv;
typedef GLboolean (APIENTRYP      PFNGLISFRAMEBUFFERPROC)(GLuint);
GLAPI PFNGLISFRAMEBUFFERPROC glad_glIsFramebuffer;
typedef void (APIENTRYP           PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
GLAPI PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer;
typedef void (APIENTRYP           PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers;
typedef void (APIENTRYP           PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers;
typedef GLenum (APIENTRYP         PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);
GLAPI PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
typedef void (APIENTRYP           PFNGLFRAMEBUFFERTEXTURE1DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
GLAPI PFNGLFRAMEBUFFERTEXTURE1DPROC glad_glFramebufferTexture1D;
typedef void (APIENTRYP           PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
GLAPI PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D;
typedef void (APIENTRYP           PFNGLFRAMEBUFFERTEXTURE3DPROC)(GLenum, GLenum, GLenum, GLuint, GLint, GLint);
GLAPI PFNGLFRAMEBUFFERTEXTURE3DPROC glad_glFramebufferTexture3D;
typedef void (APIENTRYP           PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum, GLenum, GLenum, GLuint);
GLAPI PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;
typedef void (APIENTRYP           PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLenum, GLenum, GLenum, GLint*);
GLAPI PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetFramebufferAttachmentParameteriv;
typedef void (APIENTRYP           PFNGLGENERATEMIPMAPPROC)(GLenum);
GLAPI PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
typedef void (APIENTRYP           PFNGLBLITFRAMEBUFFERPROC)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
GLAPI PFNGLBLITFRAMEBUFFERPROC glad_glBlitFramebuffer;
typedef void (APIENTRYP           PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
GLAPI PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glRenderbufferStorageMultisample;
typedef void (APIENTRYP           PFNGLFRAMEBUFFERTEXTURELAYERPROC)(GLenum, GLenum, GLuint, GLint, GLint);
GLAPI PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_glFramebufferTextureLayer;
typedef void* (APIENTRYP          PFNGLMAPBUFFERRANGEPROC)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
GLAPI PFNGLMAPBUFFERRANGEPROC glad_glMapBufferRange;
typedef void (APIENTRYP           PFNGLFLUSHMAPPEDBUFFERRANGEPROC)(GLenum, GLintptr, GLsizeiptr);
GLAPI PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_glFlushMappedBufferRange;
typedef void (APIENTRYP           PFNGLBINDVERTEXARRAYPROC)(GLuint);
GLAPI PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
typedef void (APIENTRYP           PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
typedef void (APIENTRYP           PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
typedef GLboolean (APIENTRYP      PFNGLISVERTEXARRAYPROC)(GLuint);
GLAPI PFNGLISVERTEXARRAYPROC glad_glIsVertexArray;
#endif
#ifndef GL_VERSION_3_1
	#define GL_VERSION_3_1 1
GLAPI int GLAD_GL_VERSION_3_1;
typedef void (APIENTRYP   PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum, GLint, GLsizei, GLsizei);
GLAPI PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced;
typedef void (APIENTRYP   PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum, GLsizei, GLenum, const void*, GLsizei);
GLAPI PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced;
typedef void (APIENTRYP   PFNGLTEXBUFFERPROC)(GLenum, GLenum, GLuint);
GLAPI PFNGLTEXBUFFERPROC glad_glTexBuffer;
typedef void (APIENTRYP   PFNGLPRIMITIVERESTARTINDEXPROC)(GLuint);
GLAPI PFNGLPRIMITIVERESTARTINDEXPROC glad_glPrimitiveRestartIndex;
typedef void (APIENTRYP   PFNGLCOPYBUFFERSUBDATAPROC)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr);
GLAPI PFNGLCOPYBUFFERSUBDATAPROC glad_glCopyBufferSubData;
typedef void (APIENTRYP   PFNGLGETUNIFORMINDICESPROC)(GLuint, GLsizei, const GLchar**, GLuint*);
GLAPI PFNGLGETUNIFORMINDICESPROC glad_glGetUniformIndices;
typedef void (APIENTRYP   PFNGLGETACTIVEUNIFORMSIVPROC)(GLuint, GLsizei, const GLuint*, GLenum, GLint*);
GLAPI PFNGLGETACTIVEUNIFORMSIVPROC glad_glGetActiveUniformsiv;
typedef void (APIENTRYP   PFNGLGETACTIVEUNIFORMNAMEPROC)(GLuint, GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETACTIVEUNIFORMNAMEPROC glad_glGetActiveUniformName;
typedef GLuint (APIENTRYP PFNGLGETUNIFORMBLOCKINDEXPROC)(GLuint, const GLchar*);
GLAPI PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex;
typedef void (APIENTRYP   PFNGLGETACTIVEUNIFORMBLOCKIVPROC)(GLuint, GLuint, GLenum, GLint*);
GLAPI PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_glGetActiveUniformBlockiv;
typedef void (APIENTRYP   PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)(GLuint, GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_glGetActiveUniformBlockName;
typedef void (APIENTRYP   PFNGLUNIFORMBLOCKBINDINGPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding;
#endif
#ifndef GL_VERSION_3_2
	#define GL_VERSION_3_2 1
GLAPI int GLAD_GL_VERSION_3_2;
typedef void (APIENTRYP      PFNGLDRAWELEMENTSBASEVERTEXPROC)(GLenum, GLsizei, GLenum, const void*, GLint);
GLAPI PFNGLDRAWELEMENTSBASEVERTEXPROC glad_glDrawElementsBaseVertex;
typedef void (APIENTRYP      PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC)(GLenum, GLuint, GLuint, GLsizei, GLenum, const void*, GLint);
GLAPI PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_glDrawRangeElementsBaseVertex;
typedef void (APIENTRYP      PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)(GLenum, GLsizei, GLenum, const void*, GLsizei, GLint);
GLAPI PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_glDrawElementsInstancedBaseVertex;
typedef void (APIENTRYP      PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC)(GLenum, const GLsizei*, GLenum, const void**, GLsizei, const GLint*);
GLAPI PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glad_glMultiDrawElementsBaseVertex;
typedef void (APIENTRYP      PFNGLPROVOKINGVERTEXPROC)(GLenum);
GLAPI PFNGLPROVOKINGVERTEXPROC glad_glProvokingVertex;
typedef GLsync (APIENTRYP    PFNGLFENCESYNCPROC)(GLenum, GLbitfield);
GLAPI PFNGLFENCESYNCPROC glad_glFenceSync;
typedef GLboolean (APIENTRYP PFNGLISSYNCPROC)(GLsync);
GLAPI PFNGLISSYNCPROC glad_glIsSync;
typedef void (APIENTRYP      PFNGLDELETESYNCPROC)(GLsync);
GLAPI PFNGLDELETESYNCPROC glad_glDeleteSync;
typedef GLenum (APIENTRYP    PFNGLCLIENTWAITSYNCPROC)(GLsync, GLbitfield, GLuint64);
GLAPI PFNGLCLIENTWAITSYNCPROC glad_glClientWaitSync;
typedef void (APIENTRYP      PFNGLWAITSYNCPROC)(GLsync, GLbitfield, GLuint64);
GLAPI PFNGLWAITSYNCPROC glad_glWaitSync;
typedef void (APIENTRYP      PFNGLGETINTEGER64VPROC)(GLenum, GLint64*);
GLAPI PFNGLGETINTEGER64VPROC glad_glGetInteger64v;
typedef void (APIENTRYP      PFNGLGETSYNCIVPROC)(GLsync, GLenum, GLsizei, GLsizei*, GLint*);
GLAPI PFNGLGETSYNCIVPROC glad_glGetSynciv;
typedef void (APIENTRYP      PFNGLGETINTEGER64I_VPROC)(GLenum, GLuint, GLint64*);
GLAPI PFNGLGETINTEGER64I_VPROC glad_glGetInteger64i_v;
typedef void (APIENTRYP      PFNGLGETBUFFERPARAMETERI64VPROC)(GLenum, GLenum, GLint64*);
GLAPI PFNGLGETBUFFERPARAMETERI64VPROC glad_glGetBufferParameteri64v;
typedef void (APIENTRYP      PFNGLFRAMEBUFFERTEXTUREPROC)(GLenum, GLenum, GLuint, GLint);
GLAPI PFNGLFRAMEBUFFERTEXTUREPROC glad_glFramebufferTexture;
typedef void (APIENTRYP      PFNGLTEXIMAGE2DMULTISAMPLEPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_glTexImage2DMultisample;
typedef void (APIENTRYP      PFNGLTEXIMAGE3DMULTISAMPLEPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXIMAGE3DMULTISAMPLEPROC glad_glTexImage3DMultisample;
typedef void (APIENTRYP      PFNGLGETMULTISAMPLEFVPROC)(GLenum, GLuint, GLfloat*);
GLAPI PFNGLGETMULTISAMPLEFVPROC glad_glGetMultisamplefv;
typedef void (APIENTRYP      PFNGLSAMPLEMASKIPROC)(GLuint, GLbitfield);
GLAPI PFNGLSAMPLEMASKIPROC glad_glSampleMaski;
#endif
#ifndef GL_VERSION_3_3
	#define GL_VERSION_3_3 1
GLAPI int GLAD_GL_VERSION_3_3;
typedef void (APIENTRYP      PFNGLBINDFRAGDATALOCATIONINDEXEDPROC)(GLuint, GLuint, GLuint, const GLchar*);
GLAPI PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glad_glBindFragDataLocationIndexed;
typedef GLint (APIENTRYP     PFNGLGETFRAGDATAINDEXPROC)(GLuint, const GLchar*);
GLAPI PFNGLGETFRAGDATAINDEXPROC glad_glGetFragDataIndex;
typedef void (APIENTRYP      PFNGLGENSAMPLERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENSAMPLERSPROC glad_glGenSamplers;
typedef void (APIENTRYP      PFNGLDELETESAMPLERSPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETESAMPLERSPROC glad_glDeleteSamplers;
typedef GLboolean (APIENTRYP PFNGLISSAMPLERPROC)(GLuint);
GLAPI PFNGLISSAMPLERPROC glad_glIsSampler;
typedef void (APIENTRYP      PFNGLBINDSAMPLERPROC)(GLuint, GLuint);
GLAPI PFNGLBINDSAMPLERPROC glad_glBindSampler;
typedef void (APIENTRYP      PFNGLSAMPLERPARAMETERIPROC)(GLuint, GLenum, GLint);
GLAPI PFNGLSAMPLERPARAMETERIPROC glad_glSamplerParameteri;
typedef void (APIENTRYP      PFNGLSAMPLERPARAMETERIVPROC)(GLuint, GLenum, const GLint*);
GLAPI PFNGLSAMPLERPARAMETERIVPROC glad_glSamplerParameteriv;
typedef void (APIENTRYP      PFNGLSAMPLERPARAMETERFPROC)(GLuint, GLenum, GLfloat);
GLAPI PFNGLSAMPLERPARAMETERFPROC glad_glSamplerParameterf;
typedef void (APIENTRYP      PFNGLSAMPLERPARAMETERFVPROC)(GLuint, GLenum, const GLfloat*);
GLAPI PFNGLSAMPLERPARAMETERFVPROC glad_glSamplerParameterfv;
typedef void (APIENTRYP      PFNGLSAMPLERPARAMETERIIVPROC)(GLuint, GLenum, const GLint*);
GLAPI PFNGLSAMPLERPARAMETERIIVPROC glad_glSamplerParameterIiv;
typedef void (APIENTRYP      PFNGLSAMPLERPARAMETERIUIVPROC)(GLuint, GLenum, const GLuint*);
GLAPI PFNGLSAMPLERPARAMETERIUIVPROC glad_glSamplerParameterIuiv;
typedef void (APIENTRYP      PFNGLGETSAMPLERPARAMETERIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETSAMPLERPARAMETERIVPROC glad_glGetSamplerParameteriv;
typedef void (APIENTRYP      PFNGLGETSAMPLERPARAMETERIIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETSAMPLERPARAMETERIIVPROC glad_glGetSamplerParameterIiv;
typedef void (APIENTRYP      PFNGLGETSAMPLERPARAMETERFVPROC)(GLuint, GLenum, GLfloat*);
GLAPI PFNGLGETSAMPLERPARAMETERFVPROC glad_glGetSamplerParameterfv;
typedef void (APIENTRYP      PFNGLGETSAMPLERPARAMETERIUIVPROC)(GLuint, GLenum, GLuint*);
GLAPI PFNGLGETSAMPLERPARAMETERIUIVPROC glad_glGetSamplerParameterIuiv;
typedef void (APIENTRYP      PFNGLQUERYCOUNTERPROC)(GLuint, GLenum);
GLAPI PFNGLQUERYCOUNTERPROC glad_glQueryCounter;
typedef void (APIENTRYP      PFNGLGETQUERYOBJECTI64VPROC)(GLuint, GLenum, GLint64*);
GLAPI PFNGLGETQUERYOBJECTI64VPROC glad_glGetQueryObjecti64v;
typedef void (APIENTRYP      PFNGLGETQUERYOBJECTUI64VPROC)(GLuint, GLenum, GLuint64*);
GLAPI PFNGLGETQUERYOBJECTUI64VPROC glad_glGetQueryObjectui64v;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBDIVISORPROC)(GLuint, GLuint);
GLAPI PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP1UIPROC)(GLuint, GLenum, GLboolean, GLuint);
GLAPI PFNGLVERTEXATTRIBP1UIPROC glad_glVertexAttribP1ui;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP1UIVPROC)(GLuint, GLenum, GLboolean, const GLuint*);
GLAPI PFNGLVERTEXATTRIBP1UIVPROC glad_glVertexAttribP1uiv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP2UIPROC)(GLuint, GLenum, GLboolean, GLuint);
GLAPI PFNGLVERTEXATTRIBP2UIPROC glad_glVertexAttribP2ui;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP2UIVPROC)(GLuint, GLenum, GLboolean, const GLuint*);
GLAPI PFNGLVERTEXATTRIBP2UIVPROC glad_glVertexAttribP2uiv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP3UIPROC)(GLuint, GLenum, GLboolean, GLuint);
GLAPI PFNGLVERTEXATTRIBP3UIPROC glad_glVertexAttribP3ui;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP3UIVPROC)(GLuint, GLenum, GLboolean, const GLuint*);
GLAPI PFNGLVERTEXATTRIBP3UIVPROC glad_glVertexAttribP3uiv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP4UIPROC)(GLuint, GLenum, GLboolean, GLuint);
GLAPI PFNGLVERTEXATTRIBP4UIPROC glad_glVertexAttribP4ui;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBP4UIVPROC)(GLuint, GLenum, GLboolean, const GLuint*);
GLAPI PFNGLVERTEXATTRIBP4UIVPROC glad_glVertexAttribP4uiv;
typedef void (APIENTRYP      PFNGLVERTEXP2UIPROC)(GLenum, GLuint);
GLAPI PFNGLVERTEXP2UIPROC glad_glVertexP2ui;
typedef void (APIENTRYP      PFNGLVERTEXP2UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLVERTEXP2UIVPROC glad_glVertexP2uiv;
typedef void (APIENTRYP      PFNGLVERTEXP3UIPROC)(GLenum, GLuint);
GLAPI PFNGLVERTEXP3UIPROC glad_glVertexP3ui;
typedef void (APIENTRYP      PFNGLVERTEXP3UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLVERTEXP3UIVPROC glad_glVertexP3uiv;
typedef void (APIENTRYP      PFNGLVERTEXP4UIPROC)(GLenum, GLuint);
GLAPI PFNGLVERTEXP4UIPROC glad_glVertexP4ui;
typedef void (APIENTRYP      PFNGLVERTEXP4UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLVERTEXP4UIVPROC glad_glVertexP4uiv;
typedef void (APIENTRYP      PFNGLTEXCOORDP1UIPROC)(GLenum, GLuint);
GLAPI PFNGLTEXCOORDP1UIPROC glad_glTexCoordP1ui;
typedef void (APIENTRYP      PFNGLTEXCOORDP1UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLTEXCOORDP1UIVPROC glad_glTexCoordP1uiv;
typedef void (APIENTRYP      PFNGLTEXCOORDP2UIPROC)(GLenum, GLuint);
GLAPI PFNGLTEXCOORDP2UIPROC glad_glTexCoordP2ui;
typedef void (APIENTRYP      PFNGLTEXCOORDP2UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLTEXCOORDP2UIVPROC glad_glTexCoordP2uiv;
typedef void (APIENTRYP      PFNGLTEXCOORDP3UIPROC)(GLenum, GLuint);
GLAPI PFNGLTEXCOORDP3UIPROC glad_glTexCoordP3ui;
typedef void (APIENTRYP      PFNGLTEXCOORDP3UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLTEXCOORDP3UIVPROC glad_glTexCoordP3uiv;
typedef void (APIENTRYP      PFNGLTEXCOORDP4UIPROC)(GLenum, GLuint);
GLAPI PFNGLTEXCOORDP4UIPROC glad_glTexCoordP4ui;
typedef void (APIENTRYP      PFNGLTEXCOORDP4UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLTEXCOORDP4UIVPROC glad_glTexCoordP4uiv;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP1UIPROC)(GLenum, GLenum, GLuint);
GLAPI PFNGLMULTITEXCOORDP1UIPROC glad_glMultiTexCoordP1ui;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP1UIVPROC)(GLenum, GLenum, const GLuint*);
GLAPI PFNGLMULTITEXCOORDP1UIVPROC glad_glMultiTexCoordP1uiv;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP2UIPROC)(GLenum, GLenum, GLuint);
GLAPI PFNGLMULTITEXCOORDP2UIPROC glad_glMultiTexCoordP2ui;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP2UIVPROC)(GLenum, GLenum, const GLuint*);
GLAPI PFNGLMULTITEXCOORDP2UIVPROC glad_glMultiTexCoordP2uiv;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP3UIPROC)(GLenum, GLenum, GLuint);
GLAPI PFNGLMULTITEXCOORDP3UIPROC glad_glMultiTexCoordP3ui;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP3UIVPROC)(GLenum, GLenum, const GLuint*);
GLAPI PFNGLMULTITEXCOORDP3UIVPROC glad_glMultiTexCoordP3uiv;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP4UIPROC)(GLenum, GLenum, GLuint);
GLAPI PFNGLMULTITEXCOORDP4UIPROC glad_glMultiTexCoordP4ui;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDP4UIVPROC)(GLenum, GLenum, const GLuint*);
GLAPI PFNGLMULTITEXCOORDP4UIVPROC glad_glMultiTexCoordP4uiv;
typedef void (APIENTRYP      PFNGLNORMALP3UIPROC)(GLenum, GLuint);
GLAPI PFNGLNORMALP3UIPROC glad_glNormalP3ui;
typedef void (APIENTRYP      PFNGLNORMALP3UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLNORMALP3UIVPROC glad_glNormalP3uiv;
typedef void (APIENTRYP      PFNGLCOLORP3UIPROC)(GLenum, GLuint);
GLAPI PFNGLCOLORP3UIPROC glad_glColorP3ui;
typedef void (APIENTRYP      PFNGLCOLORP3UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLCOLORP3UIVPROC glad_glColorP3uiv;
typedef void (APIENTRYP      PFNGLCOLORP4UIPROC)(GLenum, GLuint);
GLAPI PFNGLCOLORP4UIPROC glad_glColorP4ui;
typedef void (APIENTRYP      PFNGLCOLORP4UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLCOLORP4UIVPROC glad_glColorP4uiv;
typedef void (APIENTRYP      PFNGLSECONDARYCOLORP3UIPROC)(GLenum, GLuint);
GLAPI PFNGLSECONDARYCOLORP3UIPROC glad_glSecondaryColorP3ui;
typedef void (APIENTRYP      PFNGLSECONDARYCOLORP3UIVPROC)(GLenum, const GLuint*);
GLAPI PFNGLSECONDARYCOLORP3UIVPROC glad_glSecondaryColorP3uiv;
#endif
#ifndef GL_VERSION_4_0
	#define GL_VERSION_4_0 1
GLAPI int GLAD_GL_VERSION_4_0;
typedef void (APIENTRYP      PFNGLMINSAMPLESHADINGPROC)(GLfloat);
GLAPI PFNGLMINSAMPLESHADINGPROC glad_glMinSampleShading;
typedef void (APIENTRYP      PFNGLBLENDEQUATIONIPROC)(GLuint, GLenum);
GLAPI PFNGLBLENDEQUATIONIPROC glad_glBlendEquationi;
typedef void (APIENTRYP      PFNGLBLENDEQUATIONSEPARATEIPROC)(GLuint, GLenum, GLenum);
GLAPI PFNGLBLENDEQUATIONSEPARATEIPROC glad_glBlendEquationSeparatei;
typedef void (APIENTRYP      PFNGLBLENDFUNCIPROC)(GLuint, GLenum, GLenum);
GLAPI PFNGLBLENDFUNCIPROC glad_glBlendFunci;
typedef void (APIENTRYP      PFNGLBLENDFUNCSEPARATEIPROC)(GLuint, GLenum, GLenum, GLenum, GLenum);
GLAPI PFNGLBLENDFUNCSEPARATEIPROC glad_glBlendFuncSeparatei;
typedef void (APIENTRYP      PFNGLDRAWARRAYSINDIRECTPROC)(GLenum, const void*);
GLAPI PFNGLDRAWARRAYSINDIRECTPROC glad_glDrawArraysIndirect;
typedef void (APIENTRYP      PFNGLDRAWELEMENTSINDIRECTPROC)(GLenum, GLenum, const void*);
GLAPI PFNGLDRAWELEMENTSINDIRECTPROC glad_glDrawElementsIndirect;
typedef void (APIENTRYP      PFNGLUNIFORM1DPROC)(GLint, GLdouble);
GLAPI PFNGLUNIFORM1DPROC glad_glUniform1d;
typedef void (APIENTRYP      PFNGLUNIFORM2DPROC)(GLint, GLdouble, GLdouble);
GLAPI PFNGLUNIFORM2DPROC glad_glUniform2d;
typedef void (APIENTRYP      PFNGLUNIFORM3DPROC)(GLint, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLUNIFORM3DPROC glad_glUniform3d;
typedef void (APIENTRYP      PFNGLUNIFORM4DPROC)(GLint, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLUNIFORM4DPROC glad_glUniform4d;
typedef void (APIENTRYP      PFNGLUNIFORM1DVPROC)(GLint, GLsizei, const GLdouble*);
GLAPI PFNGLUNIFORM1DVPROC glad_glUniform1dv;
typedef void (APIENTRYP      PFNGLUNIFORM2DVPROC)(GLint, GLsizei, const GLdouble*);
GLAPI PFNGLUNIFORM2DVPROC glad_glUniform2dv;
typedef void (APIENTRYP      PFNGLUNIFORM3DVPROC)(GLint, GLsizei, const GLdouble*);
GLAPI PFNGLUNIFORM3DVPROC glad_glUniform3dv;
typedef void (APIENTRYP      PFNGLUNIFORM4DVPROC)(GLint, GLsizei, const GLdouble*);
GLAPI PFNGLUNIFORM4DVPROC glad_glUniform4dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX2DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX2DVPROC glad_glUniformMatrix2dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX3DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX3DVPROC glad_glUniformMatrix3dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX4DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX4DVPROC glad_glUniformMatrix4dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX2X3DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX2X3DVPROC glad_glUniformMatrix2x3dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX2X4DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX2X4DVPROC glad_glUniformMatrix2x4dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX3X2DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX3X2DVPROC glad_glUniformMatrix3x2dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX3X4DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX3X4DVPROC glad_glUniformMatrix3x4dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX4X2DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX4X2DVPROC glad_glUniformMatrix4x2dv;
typedef void (APIENTRYP      PFNGLUNIFORMMATRIX4X3DVPROC)(GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLUNIFORMMATRIX4X3DVPROC glad_glUniformMatrix4x3dv;
typedef void (APIENTRYP      PFNGLGETUNIFORMDVPROC)(GLuint, GLint, GLdouble*);
GLAPI PFNGLGETUNIFORMDVPROC glad_glGetUniformdv;
typedef GLint (APIENTRYP     PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC)(GLuint, GLenum, const GLchar*);
GLAPI PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC glad_glGetSubroutineUniformLocation;
typedef GLuint (APIENTRYP    PFNGLGETSUBROUTINEINDEXPROC)(GLuint, GLenum, const GLchar*);
GLAPI PFNGLGETSUBROUTINEINDEXPROC glad_glGetSubroutineIndex;
typedef void (APIENTRYP      PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC)(GLuint, GLenum, GLuint, GLenum, GLint*);
GLAPI PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC glad_glGetActiveSubroutineUniformiv;
typedef void (APIENTRYP      PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC)(GLuint, GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC glad_glGetActiveSubroutineUniformName;
typedef void (APIENTRYP      PFNGLGETACTIVESUBROUTINENAMEPROC)(GLuint, GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETACTIVESUBROUTINENAMEPROC glad_glGetActiveSubroutineName;
typedef void (APIENTRYP      PFNGLUNIFORMSUBROUTINESUIVPROC)(GLenum, GLsizei, const GLuint*);
GLAPI PFNGLUNIFORMSUBROUTINESUIVPROC glad_glUniformSubroutinesuiv;
typedef void (APIENTRYP      PFNGLGETUNIFORMSUBROUTINEUIVPROC)(GLenum, GLint, GLuint*);
GLAPI PFNGLGETUNIFORMSUBROUTINEUIVPROC glad_glGetUniformSubroutineuiv;
typedef void (APIENTRYP      PFNGLGETPROGRAMSTAGEIVPROC)(GLuint, GLenum, GLenum, GLint*);
GLAPI PFNGLGETPROGRAMSTAGEIVPROC glad_glGetProgramStageiv;
typedef void (APIENTRYP      PFNGLPATCHPARAMETERIPROC)(GLenum, GLint);
GLAPI PFNGLPATCHPARAMETERIPROC glad_glPatchParameteri;
typedef void (APIENTRYP      PFNGLPATCHPARAMETERFVPROC)(GLenum, const GLfloat*);
GLAPI PFNGLPATCHPARAMETERFVPROC glad_glPatchParameterfv;
typedef void (APIENTRYP      PFNGLBINDTRANSFORMFEEDBACKPROC)(GLenum, GLuint);
GLAPI PFNGLBINDTRANSFORMFEEDBACKPROC glad_glBindTransformFeedback;
typedef void (APIENTRYP      PFNGLDELETETRANSFORMFEEDBACKSPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETETRANSFORMFEEDBACKSPROC glad_glDeleteTransformFeedbacks;
typedef void (APIENTRYP      PFNGLGENTRANSFORMFEEDBACKSPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENTRANSFORMFEEDBACKSPROC glad_glGenTransformFeedbacks;
typedef GLboolean (APIENTRYP PFNGLISTRANSFORMFEEDBACKPROC)(GLuint);
GLAPI PFNGLISTRANSFORMFEEDBACKPROC glad_glIsTransformFeedback;
typedef void (APIENTRYP      PFNGLPAUSETRANSFORMFEEDBACKPROC)();
GLAPI PFNGLPAUSETRANSFORMFEEDBACKPROC glad_glPauseTransformFeedback;
typedef void (APIENTRYP      PFNGLRESUMETRANSFORMFEEDBACKPROC)();
GLAPI PFNGLRESUMETRANSFORMFEEDBACKPROC glad_glResumeTransformFeedback;
typedef void (APIENTRYP      PFNGLDRAWTRANSFORMFEEDBACKPROC)(GLenum, GLuint);
GLAPI PFNGLDRAWTRANSFORMFEEDBACKPROC glad_glDrawTransformFeedback;
typedef void (APIENTRYP      PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC)(GLenum, GLuint, GLuint);
GLAPI PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC glad_glDrawTransformFeedbackStream;
typedef void (APIENTRYP      PFNGLBEGINQUERYINDEXEDPROC)(GLenum, GLuint, GLuint);
GLAPI PFNGLBEGINQUERYINDEXEDPROC glad_glBeginQueryIndexed;
typedef void (APIENTRYP      PFNGLENDQUERYINDEXEDPROC)(GLenum, GLuint);
GLAPI PFNGLENDQUERYINDEXEDPROC glad_glEndQueryIndexed;
typedef void (APIENTRYP      PFNGLGETQUERYINDEXEDIVPROC)(GLenum, GLuint, GLenum, GLint*);
GLAPI PFNGLGETQUERYINDEXEDIVPROC glad_glGetQueryIndexediv;
#endif
#ifndef GL_VERSION_4_1
	#define GL_VERSION_4_1 1
GLAPI int GLAD_GL_VERSION_4_1;
typedef void (APIENTRYP      PFNGLRELEASESHADERCOMPILERPROC)();
GLAPI PFNGLRELEASESHADERCOMPILERPROC glad_glReleaseShaderCompiler;
typedef void (APIENTRYP      PFNGLSHADERBINARYPROC)(GLsizei, const GLuint*, GLenum, const void*, GLsizei);
GLAPI PFNGLSHADERBINARYPROC glad_glShaderBinary;
typedef void (APIENTRYP      PFNGLGETSHADERPRECISIONFORMATPROC)(GLenum, GLenum, GLint*, GLint*);
GLAPI PFNGLGETSHADERPRECISIONFORMATPROC glad_glGetShaderPrecisionFormat;
typedef void (APIENTRYP      PFNGLDEPTHRANGEFPROC)(GLfloat, GLfloat);
GLAPI PFNGLDEPTHRANGEFPROC glad_glDepthRangef;
typedef void (APIENTRYP      PFNGLCLEARDEPTHFPROC)(GLfloat);
GLAPI PFNGLCLEARDEPTHFPROC glad_glClearDepthf;
typedef void (APIENTRYP      PFNGLGETPROGRAMBINARYPROC)(GLuint, GLsizei, GLsizei*, GLenum*, void*);
GLAPI PFNGLGETPROGRAMBINARYPROC glad_glGetProgramBinary;
typedef void (APIENTRYP      PFNGLPROGRAMBINARYPROC)(GLuint, GLenum, const void*, GLsizei);
GLAPI PFNGLPROGRAMBINARYPROC glad_glProgramBinary;
typedef void (APIENTRYP      PFNGLPROGRAMPARAMETERIPROC)(GLuint, GLenum, GLint);
GLAPI PFNGLPROGRAMPARAMETERIPROC glad_glProgramParameteri;
typedef void (APIENTRYP      PFNGLUSEPROGRAMSTAGESPROC)(GLuint, GLbitfield, GLuint);
GLAPI PFNGLUSEPROGRAMSTAGESPROC glad_glUseProgramStages;
typedef void (APIENTRYP      PFNGLACTIVESHADERPROGRAMPROC)(GLuint, GLuint);
GLAPI PFNGLACTIVESHADERPROGRAMPROC glad_glActiveShaderProgram;
typedef GLuint (APIENTRYP    PFNGLCREATESHADERPROGRAMVPROC)(GLenum, GLsizei, const GLchar**);
GLAPI PFNGLCREATESHADERPROGRAMVPROC glad_glCreateShaderProgramv;
typedef void (APIENTRYP      PFNGLBINDPROGRAMPIPELINEPROC)(GLuint);
GLAPI PFNGLBINDPROGRAMPIPELINEPROC glad_glBindProgramPipeline;
typedef void (APIENTRYP      PFNGLDELETEPROGRAMPIPELINESPROC)(GLsizei, const GLuint*);
GLAPI PFNGLDELETEPROGRAMPIPELINESPROC glad_glDeleteProgramPipelines;
typedef void (APIENTRYP      PFNGLGENPROGRAMPIPELINESPROC)(GLsizei, GLuint*);
GLAPI PFNGLGENPROGRAMPIPELINESPROC glad_glGenProgramPipelines;
typedef GLboolean (APIENTRYP PFNGLISPROGRAMPIPELINEPROC)(GLuint);
GLAPI PFNGLISPROGRAMPIPELINEPROC glad_glIsProgramPipeline;
typedef void (APIENTRYP      PFNGLGETPROGRAMPIPELINEIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETPROGRAMPIPELINEIVPROC glad_glGetProgramPipelineiv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1IPROC)(GLuint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM1IPROC glad_glProgramUniform1i;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1IVPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM1IVPROC glad_glProgramUniform1iv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1FPROC)(GLuint, GLint, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM1FPROC glad_glProgramUniform1f;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1FVPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM1FVPROC glad_glProgramUniform1fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1DPROC)(GLuint, GLint, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM1DPROC glad_glProgramUniform1d;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1DVPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM1DVPROC glad_glProgramUniform1dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1UIPROC)(GLuint, GLint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM1UIPROC glad_glProgramUniform1ui;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1UIVPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM1UIVPROC glad_glProgramUniform1uiv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2IPROC)(GLuint, GLint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM2IPROC glad_glProgramUniform2i;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2IVPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM2IVPROC glad_glProgramUniform2iv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2FPROC)(GLuint, GLint, GLfloat, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM2FPROC glad_glProgramUniform2f;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2FVPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM2FVPROC glad_glProgramUniform2fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2DPROC)(GLuint, GLint, GLdouble, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM2DPROC glad_glProgramUniform2d;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2DVPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM2DVPROC glad_glProgramUniform2dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2UIPROC)(GLuint, GLint, GLuint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM2UIPROC glad_glProgramUniform2ui;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2UIVPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM2UIVPROC glad_glProgramUniform2uiv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3IPROC)(GLuint, GLint, GLint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM3IPROC glad_glProgramUniform3i;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3IVPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM3IVPROC glad_glProgramUniform3iv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3FPROC)(GLuint, GLint, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM3FPROC glad_glProgramUniform3f;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3FVPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM3FVPROC glad_glProgramUniform3fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3DPROC)(GLuint, GLint, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM3DPROC glad_glProgramUniform3d;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3DVPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM3DVPROC glad_glProgramUniform3dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3UIPROC)(GLuint, GLint, GLuint, GLuint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM3UIPROC glad_glProgramUniform3ui;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3UIVPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM3UIVPROC glad_glProgramUniform3uiv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4IPROC)(GLuint, GLint, GLint, GLint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM4IPROC glad_glProgramUniform4i;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4IVPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM4IVPROC glad_glProgramUniform4iv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4FPROC)(GLuint, GLint, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM4FPROC glad_glProgramUniform4f;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4FVPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM4FVPROC glad_glProgramUniform4fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4DPROC)(GLuint, GLint, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM4DPROC glad_glProgramUniform4d;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4DVPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM4DVPROC glad_glProgramUniform4dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4UIPROC)(GLuint, GLint, GLuint, GLuint, GLuint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM4UIPROC glad_glProgramUniform4ui;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4UIVPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM4UIVPROC glad_glProgramUniform4uiv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2FVPROC glad_glProgramUniformMatrix2fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3FVPROC glad_glProgramUniformMatrix3fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4FVPROC glad_glProgramUniformMatrix4fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2DVPROC glad_glProgramUniformMatrix2dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3DVPROC glad_glProgramUniformMatrix3dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4DVPROC glad_glProgramUniformMatrix4dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glad_glProgramUniformMatrix2x3fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glad_glProgramUniformMatrix3x2fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glad_glProgramUniformMatrix2x4fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glad_glProgramUniformMatrix4x2fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glad_glProgramUniformMatrix3x4fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glad_glProgramUniformMatrix4x3fv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC glad_glProgramUniformMatrix2x3dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC glad_glProgramUniformMatrix3x2dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC glad_glProgramUniformMatrix2x4dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC glad_glProgramUniformMatrix4x2dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC glad_glProgramUniformMatrix3x4dv;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC glad_glProgramUniformMatrix4x3dv;
typedef void (APIENTRYP      PFNGLVALIDATEPROGRAMPIPELINEPROC)(GLuint);
GLAPI PFNGLVALIDATEPROGRAMPIPELINEPROC glad_glValidateProgramPipeline;
typedef void (APIENTRYP      PFNGLGETPROGRAMPIPELINEINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETPROGRAMPIPELINEINFOLOGPROC glad_glGetProgramPipelineInfoLog;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL1DPROC)(GLuint, GLdouble);
GLAPI PFNGLVERTEXATTRIBL1DPROC glad_glVertexAttribL1d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL2DPROC)(GLuint, GLdouble, GLdouble);
GLAPI PFNGLVERTEXATTRIBL2DPROC glad_glVertexAttribL2d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL3DPROC)(GLuint, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLVERTEXATTRIBL3DPROC glad_glVertexAttribL3d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL4DPROC)(GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLVERTEXATTRIBL4DPROC glad_glVertexAttribL4d;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL1DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIBL1DVPROC glad_glVertexAttribL1dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL2DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIBL2DVPROC glad_glVertexAttribL2dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL3DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIBL3DVPROC glad_glVertexAttribL3dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBL4DVPROC)(GLuint, const GLdouble*);
GLAPI PFNGLVERTEXATTRIBL4DVPROC glad_glVertexAttribL4dv;
typedef void (APIENTRYP      PFNGLVERTEXATTRIBLPOINTERPROC)(GLuint, GLint, GLenum, GLsizei, const void*);
GLAPI PFNGLVERTEXATTRIBLPOINTERPROC glad_glVertexAttribLPointer;
typedef void (APIENTRYP      PFNGLGETVERTEXATTRIBLDVPROC)(GLuint, GLenum, GLdouble*);
GLAPI PFNGLGETVERTEXATTRIBLDVPROC glad_glGetVertexAttribLdv;
typedef void (APIENTRYP      PFNGLVIEWPORTARRAYVPROC)(GLuint, GLsizei, const GLfloat*);
GLAPI PFNGLVIEWPORTARRAYVPROC glad_glViewportArrayv;
typedef void (APIENTRYP      PFNGLVIEWPORTINDEXEDFPROC)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLVIEWPORTINDEXEDFPROC glad_glViewportIndexedf;
typedef void (APIENTRYP      PFNGLVIEWPORTINDEXEDFVPROC)(GLuint, const GLfloat*);
GLAPI PFNGLVIEWPORTINDEXEDFVPROC glad_glViewportIndexedfv;
typedef void (APIENTRYP      PFNGLSCISSORARRAYVPROC)(GLuint, GLsizei, const GLint*);
GLAPI PFNGLSCISSORARRAYVPROC glad_glScissorArrayv;
typedef void (APIENTRYP      PFNGLSCISSORINDEXEDPROC)(GLuint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLSCISSORINDEXEDPROC glad_glScissorIndexed;
typedef void (APIENTRYP      PFNGLSCISSORINDEXEDVPROC)(GLuint, const GLint*);
GLAPI PFNGLSCISSORINDEXEDVPROC glad_glScissorIndexedv;
typedef void (APIENTRYP      PFNGLDEPTHRANGEARRAYVPROC)(GLuint, GLsizei, const GLdouble*);
GLAPI PFNGLDEPTHRANGEARRAYVPROC glad_glDepthRangeArrayv;
typedef void (APIENTRYP      PFNGLDEPTHRANGEINDEXEDPROC)(GLuint, GLdouble, GLdouble);
GLAPI PFNGLDEPTHRANGEINDEXEDPROC glad_glDepthRangeIndexed;
typedef void (APIENTRYP      PFNGLGETFLOATI_VPROC)(GLenum, GLuint, GLfloat*);
GLAPI PFNGLGETFLOATI_VPROC glad_glGetFloati_v;
typedef void (APIENTRYP      PFNGLGETDOUBLEI_VPROC)(GLenum, GLuint, GLdouble*);
GLAPI PFNGLGETDOUBLEI_VPROC glad_glGetDoublei_v;
#endif
#ifndef GL_VERSION_4_2
	#define GL_VERSION_4_2 1
GLAPI int GLAD_GL_VERSION_4_2;
typedef void (APIENTRYP PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC)(GLenum, GLint, GLsizei, GLsizei, GLuint);
GLAPI PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC glad_glDrawArraysInstancedBaseInstance;
typedef void (APIENTRYP PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC)(GLenum, GLsizei, GLenum, const void*, GLsizei, GLuint);
GLAPI PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC glad_glDrawElementsInstancedBaseInstance;
typedef void (APIENTRYP PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC)(GLenum, GLsizei, GLenum, const void*, GLsizei, GLint, GLuint);
GLAPI PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC glad_glDrawElementsInstancedBaseVertexBaseInstance;
typedef void (APIENTRYP PFNGLGETINTERNALFORMATIVPROC)(GLenum, GLenum, GLenum, GLsizei, GLint*);
GLAPI PFNGLGETINTERNALFORMATIVPROC glad_glGetInternalformativ;
typedef void (APIENTRYP PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC)(GLuint, GLuint, GLenum, GLint*);
GLAPI PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC glad_glGetActiveAtomicCounterBufferiv;
typedef void (APIENTRYP PFNGLBINDIMAGETEXTUREPROC)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
GLAPI PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture;
typedef void (APIENTRYP PFNGLMEMORYBARRIERPROC)(GLbitfield);
GLAPI PFNGLMEMORYBARRIERPROC glad_glMemoryBarrier;
typedef void (APIENTRYP PFNGLTEXSTORAGE1DPROC)(GLenum, GLsizei, GLenum, GLsizei);
GLAPI PFNGLTEXSTORAGE1DPROC glad_glTexStorage1D;
typedef void (APIENTRYP PFNGLTEXSTORAGE2DPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
GLAPI PFNGLTEXSTORAGE2DPROC glad_glTexStorage2D;
typedef void (APIENTRYP PFNGLTEXSTORAGE3DPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
GLAPI PFNGLTEXSTORAGE3DPROC glad_glTexStorage3D;
typedef void (APIENTRYP PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC)(GLenum, GLuint, GLsizei);
GLAPI PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC glad_glDrawTransformFeedbackInstanced;
typedef void (APIENTRYP PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC)(GLenum, GLuint, GLuint, GLsizei);
GLAPI PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC glad_glDrawTransformFeedbackStreamInstanced;
#endif
#ifndef GL_VERSION_4_3
	#define GL_VERSION_4_3 1
GLAPI int GLAD_GL_VERSION_4_3;
typedef void (APIENTRYP   PFNGLCLEARBUFFERDATAPROC)(GLenum, GLenum, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARBUFFERDATAPROC glad_glClearBufferData;
typedef void (APIENTRYP   PFNGLCLEARBUFFERSUBDATAPROC)(GLenum, GLenum, GLintptr, GLsizeiptr, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARBUFFERSUBDATAPROC glad_glClearBufferSubData;
typedef void (APIENTRYP   PFNGLDISPATCHCOMPUTEPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute;
typedef void (APIENTRYP   PFNGLDISPATCHCOMPUTEINDIRECTPROC)(GLintptr);
GLAPI PFNGLDISPATCHCOMPUTEINDIRECTPROC glad_glDispatchComputeIndirect;
typedef void (APIENTRYP   PFNGLCOPYIMAGESUBDATAPROC)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei);
GLAPI PFNGLCOPYIMAGESUBDATAPROC glad_glCopyImageSubData;
typedef void (APIENTRYP   PFNGLFRAMEBUFFERPARAMETERIPROC)(GLenum, GLenum, GLint);
GLAPI PFNGLFRAMEBUFFERPARAMETERIPROC glad_glFramebufferParameteri;
typedef void (APIENTRYP   PFNGLGETFRAMEBUFFERPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETFRAMEBUFFERPARAMETERIVPROC glad_glGetFramebufferParameteriv;
typedef void (APIENTRYP   PFNGLGETINTERNALFORMATI64VPROC)(GLenum, GLenum, GLenum, GLsizei, GLint64*);
GLAPI PFNGLGETINTERNALFORMATI64VPROC glad_glGetInternalformati64v;
typedef void (APIENTRYP   PFNGLINVALIDATETEXSUBIMAGEPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei);
GLAPI PFNGLINVALIDATETEXSUBIMAGEPROC glad_glInvalidateTexSubImage;
typedef void (APIENTRYP   PFNGLINVALIDATETEXIMAGEPROC)(GLuint, GLint);
GLAPI PFNGLINVALIDATETEXIMAGEPROC glad_glInvalidateTexImage;
typedef void (APIENTRYP   PFNGLINVALIDATEBUFFERSUBDATAPROC)(GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLINVALIDATEBUFFERSUBDATAPROC glad_glInvalidateBufferSubData;
typedef void (APIENTRYP   PFNGLINVALIDATEBUFFERDATAPROC)(GLuint);
GLAPI PFNGLINVALIDATEBUFFERDATAPROC glad_glInvalidateBufferData;
typedef void (APIENTRYP   PFNGLINVALIDATEFRAMEBUFFERPROC)(GLenum, GLsizei, const GLenum*);
GLAPI PFNGLINVALIDATEFRAMEBUFFERPROC glad_glInvalidateFramebuffer;
typedef void (APIENTRYP   PFNGLINVALIDATESUBFRAMEBUFFERPROC)(GLenum, GLsizei, const GLenum*, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLINVALIDATESUBFRAMEBUFFERPROC glad_glInvalidateSubFramebuffer;
typedef void (APIENTRYP   PFNGLMULTIDRAWARRAYSINDIRECTPROC)(GLenum, const void*, GLsizei, GLsizei);
GLAPI PFNGLMULTIDRAWARRAYSINDIRECTPROC glad_glMultiDrawArraysIndirect;
typedef void (APIENTRYP   PFNGLMULTIDRAWELEMENTSINDIRECTPROC)(GLenum, GLenum, const void*, GLsizei, GLsizei);
GLAPI PFNGLMULTIDRAWELEMENTSINDIRECTPROC glad_glMultiDrawElementsIndirect;
typedef void (APIENTRYP   PFNGLGETPROGRAMINTERFACEIVPROC)(GLuint, GLenum, GLenum, GLint*);
GLAPI PFNGLGETPROGRAMINTERFACEIVPROC glad_glGetProgramInterfaceiv;
typedef GLuint (APIENTRYP PFNGLGETPROGRAMRESOURCEINDEXPROC)(GLuint, GLenum, const GLchar*);
GLAPI PFNGLGETPROGRAMRESOURCEINDEXPROC glad_glGetProgramResourceIndex;
typedef void (APIENTRYP   PFNGLGETPROGRAMRESOURCENAMEPROC)(GLuint, GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETPROGRAMRESOURCENAMEPROC glad_glGetProgramResourceName;
typedef void (APIENTRYP   PFNGLGETPROGRAMRESOURCEIVPROC)(GLuint, GLenum, GLuint, GLsizei, const GLenum*, GLsizei, GLsizei*, GLint*);
GLAPI PFNGLGETPROGRAMRESOURCEIVPROC glad_glGetProgramResourceiv;
typedef GLint (APIENTRYP  PFNGLGETPROGRAMRESOURCELOCATIONPROC)(GLuint, GLenum, const GLchar*);
GLAPI PFNGLGETPROGRAMRESOURCELOCATIONPROC glad_glGetProgramResourceLocation;
typedef GLint (APIENTRYP  PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC)(GLuint, GLenum, const GLchar*);
GLAPI PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC glad_glGetProgramResourceLocationIndex;
typedef void (APIENTRYP   PFNGLSHADERSTORAGEBLOCKBINDINGPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLSHADERSTORAGEBLOCKBINDINGPROC glad_glShaderStorageBlockBinding;
typedef void (APIENTRYP   PFNGLTEXBUFFERRANGEPROC)(GLenum, GLenum, GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLTEXBUFFERRANGEPROC glad_glTexBufferRange;
typedef void (APIENTRYP   PFNGLTEXSTORAGE2DMULTISAMPLEPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXSTORAGE2DMULTISAMPLEPROC glad_glTexStorage2DMultisample;
typedef void (APIENTRYP   PFNGLTEXSTORAGE3DMULTISAMPLEPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXSTORAGE3DMULTISAMPLEPROC glad_glTexStorage3DMultisample;
typedef void (APIENTRYP   PFNGLTEXTUREVIEWPROC)(GLuint, GLenum, GLuint, GLenum, GLuint, GLuint, GLuint, GLuint);
GLAPI PFNGLTEXTUREVIEWPROC glad_glTextureView;
typedef void (APIENTRYP   PFNGLBINDVERTEXBUFFERPROC)(GLuint, GLuint, GLintptr, GLsizei);
GLAPI PFNGLBINDVERTEXBUFFERPROC glad_glBindVertexBuffer;
typedef void (APIENTRYP   PFNGLVERTEXATTRIBFORMATPROC)(GLuint, GLint, GLenum, GLboolean, GLuint);
GLAPI PFNGLVERTEXATTRIBFORMATPROC glad_glVertexAttribFormat;
typedef void (APIENTRYP   PFNGLVERTEXATTRIBIFORMATPROC)(GLuint, GLint, GLenum, GLuint);
GLAPI PFNGLVERTEXATTRIBIFORMATPROC glad_glVertexAttribIFormat;
typedef void (APIENTRYP   PFNGLVERTEXATTRIBLFORMATPROC)(GLuint, GLint, GLenum, GLuint);
GLAPI PFNGLVERTEXATTRIBLFORMATPROC glad_glVertexAttribLFormat;
typedef void (APIENTRYP   PFNGLVERTEXATTRIBBINDINGPROC)(GLuint, GLuint);
GLAPI PFNGLVERTEXATTRIBBINDINGPROC glad_glVertexAttribBinding;
typedef void (APIENTRYP   PFNGLVERTEXBINDINGDIVISORPROC)(GLuint, GLuint);
GLAPI PFNGLVERTEXBINDINGDIVISORPROC glad_glVertexBindingDivisor;
typedef void (APIENTRYP   PFNGLDEBUGMESSAGECONTROLPROC)(GLenum, GLenum, GLenum, GLsizei, const GLuint*, GLboolean);
GLAPI PFNGLDEBUGMESSAGECONTROLPROC glad_glDebugMessageControl;
typedef void (APIENTRYP   PFNGLDEBUGMESSAGEINSERTPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*);
GLAPI PFNGLDEBUGMESSAGEINSERTPROC glad_glDebugMessageInsert;
typedef void (APIENTRYP   PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC, const void*);
GLAPI PFNGLDEBUGMESSAGECALLBACKPROC glad_glDebugMessageCallback;
typedef GLuint (APIENTRYP PFNGLGETDEBUGMESSAGELOGPROC)(GLuint, GLsizei, GLenum*, GLenum*, GLuint*, GLenum*, GLsizei*, GLchar*);
GLAPI PFNGLGETDEBUGMESSAGELOGPROC glad_glGetDebugMessageLog;
typedef void (APIENTRYP   PFNGLPUSHDEBUGGROUPPROC)(GLenum, GLuint, GLsizei, const GLchar*);
GLAPI PFNGLPUSHDEBUGGROUPPROC glad_glPushDebugGroup;
typedef void (APIENTRYP   PFNGLPOPDEBUGGROUPPROC)();
GLAPI PFNGLPOPDEBUGGROUPPROC glad_glPopDebugGroup;
typedef void (APIENTRYP   PFNGLOBJECTLABELPROC)(GLenum, GLuint, GLsizei, const GLchar*);
GLAPI PFNGLOBJECTLABELPROC glad_glObjectLabel;
typedef void (APIENTRYP   PFNGLGETOBJECTLABELPROC)(GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETOBJECTLABELPROC glad_glGetObjectLabel;
typedef void (APIENTRYP   PFNGLOBJECTPTRLABELPROC)(const void*, GLsizei, const GLchar*);
GLAPI PFNGLOBJECTPTRLABELPROC glad_glObjectPtrLabel;
typedef void (APIENTRYP   PFNGLGETOBJECTPTRLABELPROC)(const void*, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETOBJECTPTRLABELPROC glad_glGetObjectPtrLabel;
#endif
#ifndef GL_VERSION_4_4
	#define GL_VERSION_4_4 1
GLAPI int GLAD_GL_VERSION_4_4;
typedef void (APIENTRYP PFNGLBUFFERSTORAGEPROC)(GLenum, GLsizeiptr, const void*, GLbitfield);
GLAPI PFNGLBUFFERSTORAGEPROC glad_glBufferStorage;
typedef void (APIENTRYP PFNGLCLEARTEXIMAGEPROC)(GLuint, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARTEXIMAGEPROC glad_glClearTexImage;
typedef void (APIENTRYP PFNGLCLEARTEXSUBIMAGEPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARTEXSUBIMAGEPROC glad_glClearTexSubImage;
typedef void (APIENTRYP PFNGLBINDBUFFERSBASEPROC)(GLenum, GLuint, GLsizei, const GLuint*);
GLAPI PFNGLBINDBUFFERSBASEPROC glad_glBindBuffersBase;
typedef void (APIENTRYP PFNGLBINDBUFFERSRANGEPROC)(GLenum, GLuint, GLsizei, const GLuint*, const GLintptr*, const GLsizeiptr*);
GLAPI PFNGLBINDBUFFERSRANGEPROC glad_glBindBuffersRange;
typedef void (APIENTRYP PFNGLBINDTEXTURESPROC)(GLuint, GLsizei, const GLuint*);
GLAPI PFNGLBINDTEXTURESPROC glad_glBindTextures;
typedef void (APIENTRYP PFNGLBINDSAMPLERSPROC)(GLuint, GLsizei, const GLuint*);
GLAPI PFNGLBINDSAMPLERSPROC glad_glBindSamplers;
typedef void (APIENTRYP PFNGLBINDIMAGETEXTURESPROC)(GLuint, GLsizei, const GLuint*);
GLAPI PFNGLBINDIMAGETEXTURESPROC glad_glBindImageTextures;
typedef void (APIENTRYP PFNGLBINDVERTEXBUFFERSPROC)(GLuint, GLsizei, const GLuint*, const GLintptr*, const GLsizei*);
GLAPI PFNGLBINDVERTEXBUFFERSPROC glad_glBindVertexBuffers;
#endif
#ifndef GL_VERSION_4_5
	#define GL_VERSION_4_5 1
GLAPI int GLAD_GL_VERSION_4_5;
typedef void (APIENTRYP      PFNGLCLIPCONTROLPROC)(GLenum, GLenum);
GLAPI PFNGLCLIPCONTROLPROC glad_glClipControl;
typedef void (APIENTRYP      PFNGLCREATETRANSFORMFEEDBACKSPROC)(GLsizei, GLuint*);
GLAPI PFNGLCREATETRANSFORMFEEDBACKSPROC glad_glCreateTransformFeedbacks;
typedef void (APIENTRYP      PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC glad_glTransformFeedbackBufferBase;
typedef void (APIENTRYP      PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC)(GLuint, GLuint, GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC glad_glTransformFeedbackBufferRange;
typedef void (APIENTRYP      PFNGLGETTRANSFORMFEEDBACKIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETTRANSFORMFEEDBACKIVPROC glad_glGetTransformFeedbackiv;
typedef void (APIENTRYP      PFNGLGETTRANSFORMFEEDBACKI_VPROC)(GLuint, GLenum, GLuint, GLint*);
GLAPI PFNGLGETTRANSFORMFEEDBACKI_VPROC glad_glGetTransformFeedbacki_v;
typedef void (APIENTRYP      PFNGLGETTRANSFORMFEEDBACKI64_VPROC)(GLuint, GLenum, GLuint, GLint64*);
GLAPI PFNGLGETTRANSFORMFEEDBACKI64_VPROC glad_glGetTransformFeedbacki64_v;
typedef void (APIENTRYP      PFNGLCREATEBUFFERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLCREATEBUFFERSPROC glad_glCreateBuffers;
typedef void (APIENTRYP      PFNGLNAMEDBUFFERSTORAGEPROC)(GLuint, GLsizeiptr, const void*, GLbitfield);
GLAPI PFNGLNAMEDBUFFERSTORAGEPROC glad_glNamedBufferStorage;
typedef void (APIENTRYP      PFNGLNAMEDBUFFERDATAPROC)(GLuint, GLsizeiptr, const void*, GLenum);
GLAPI PFNGLNAMEDBUFFERDATAPROC glad_glNamedBufferData;
typedef void (APIENTRYP      PFNGLNAMEDBUFFERSUBDATAPROC)(GLuint, GLintptr, GLsizeiptr, const void*);
GLAPI PFNGLNAMEDBUFFERSUBDATAPROC glad_glNamedBufferSubData;
typedef void (APIENTRYP      PFNGLCOPYNAMEDBUFFERSUBDATAPROC)(GLuint, GLuint, GLintptr, GLintptr, GLsizeiptr);
GLAPI PFNGLCOPYNAMEDBUFFERSUBDATAPROC glad_glCopyNamedBufferSubData;
typedef void (APIENTRYP      PFNGLCLEARNAMEDBUFFERDATAPROC)(GLuint, GLenum, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARNAMEDBUFFERDATAPROC glad_glClearNamedBufferData;
typedef void (APIENTRYP      PFNGLCLEARNAMEDBUFFERSUBDATAPROC)(GLuint, GLenum, GLintptr, GLsizeiptr, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARNAMEDBUFFERSUBDATAPROC glad_glClearNamedBufferSubData;
typedef void* (APIENTRYP     PFNGLMAPNAMEDBUFFERPROC)(GLuint, GLenum);
GLAPI PFNGLMAPNAMEDBUFFERPROC glad_glMapNamedBuffer;
typedef void* (APIENTRYP     PFNGLMAPNAMEDBUFFERRANGEPROC)(GLuint, GLintptr, GLsizeiptr, GLbitfield);
GLAPI PFNGLMAPNAMEDBUFFERRANGEPROC glad_glMapNamedBufferRange;
typedef GLboolean (APIENTRYP PFNGLUNMAPNAMEDBUFFERPROC)(GLuint);
GLAPI PFNGLUNMAPNAMEDBUFFERPROC glad_glUnmapNamedBuffer;
typedef void (APIENTRYP      PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC)(GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC glad_glFlushMappedNamedBufferRange;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERPARAMETERIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETNAMEDBUFFERPARAMETERIVPROC glad_glGetNamedBufferParameteriv;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERPARAMETERI64VPROC)(GLuint, GLenum, GLint64*);
GLAPI PFNGLGETNAMEDBUFFERPARAMETERI64VPROC glad_glGetNamedBufferParameteri64v;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERPOINTERVPROC)(GLuint, GLenum, void**);
GLAPI PFNGLGETNAMEDBUFFERPOINTERVPROC glad_glGetNamedBufferPointerv;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERSUBDATAPROC)(GLuint, GLintptr, GLsizeiptr, void*);
GLAPI PFNGLGETNAMEDBUFFERSUBDATAPROC glad_glGetNamedBufferSubData;
typedef void (APIENTRYP      PFNGLCREATEFRAMEBUFFERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLCREATEFRAMEBUFFERSPROC glad_glCreateFramebuffers;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC)(GLuint, GLenum, GLenum, GLuint);
GLAPI PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC glad_glNamedFramebufferRenderbuffer;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC)(GLuint, GLenum, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC glad_glNamedFramebufferParameteri;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTUREPROC)(GLuint, GLenum, GLuint, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTUREPROC glad_glNamedFramebufferTexture;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC)(GLuint, GLenum, GLuint, GLint, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC glad_glNamedFramebufferTextureLayer;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC)(GLuint, GLenum);
GLAPI PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC glad_glNamedFramebufferDrawBuffer;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC)(GLuint, GLsizei, const GLenum*);
GLAPI PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC glad_glNamedFramebufferDrawBuffers;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC)(GLuint, GLenum);
GLAPI PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC glad_glNamedFramebufferReadBuffer;
typedef void (APIENTRYP      PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC)(GLuint, GLsizei, const GLenum*);
GLAPI PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC glad_glInvalidateNamedFramebufferData;
typedef void (APIENTRYP      PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC)(GLuint, GLsizei, const GLenum*, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC glad_glInvalidateNamedFramebufferSubData;
typedef void (APIENTRYP      PFNGLCLEARNAMEDFRAMEBUFFERIVPROC)(GLuint, GLenum, GLint, const GLint*);
GLAPI PFNGLCLEARNAMEDFRAMEBUFFERIVPROC glad_glClearNamedFramebufferiv;
typedef void (APIENTRYP      PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC)(GLuint, GLenum, GLint, const GLuint*);
GLAPI PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC glad_glClearNamedFramebufferuiv;
typedef void (APIENTRYP      PFNGLCLEARNAMEDFRAMEBUFFERFVPROC)(GLuint, GLenum, GLint, const GLfloat*);
GLAPI PFNGLCLEARNAMEDFRAMEBUFFERFVPROC glad_glClearNamedFramebufferfv;
typedef void (APIENTRYP      PFNGLCLEARNAMEDFRAMEBUFFERFIPROC)(GLuint, GLenum, const GLfloat, GLint);
GLAPI PFNGLCLEARNAMEDFRAMEBUFFERFIPROC glad_glClearNamedFramebufferfi;
typedef void (APIENTRYP      PFNGLBLITNAMEDFRAMEBUFFERPROC)(GLuint, GLuint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
GLAPI PFNGLBLITNAMEDFRAMEBUFFERPROC glad_glBlitNamedFramebuffer;
typedef GLenum (APIENTRYP    PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC)(GLuint, GLenum);
GLAPI PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC glad_glCheckNamedFramebufferStatus;
typedef void (APIENTRYP      PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC glad_glGetNamedFramebufferParameteriv;
typedef void (APIENTRYP      PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLuint, GLenum, GLenum, GLint*);
GLAPI PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetNamedFramebufferAttachmentParameteriv;
typedef void (APIENTRYP      PFNGLCREATERENDERBUFFERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLCREATERENDERBUFFERSPROC glad_glCreateRenderbuffers;
typedef void (APIENTRYP      PFNGLNAMEDRENDERBUFFERSTORAGEPROC)(GLuint, GLenum, GLsizei, GLsizei);
GLAPI PFNGLNAMEDRENDERBUFFERSTORAGEPROC glad_glNamedRenderbufferStorage;
typedef void (APIENTRYP      PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLuint, GLsizei, GLenum, GLsizei, GLsizei);
GLAPI PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glNamedRenderbufferStorageMultisample;
typedef void (APIENTRYP      PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC glad_glGetNamedRenderbufferParameteriv;
typedef void (APIENTRYP      PFNGLCREATETEXTURESPROC)(GLenum, GLsizei, GLuint*);
GLAPI PFNGLCREATETEXTURESPROC glad_glCreateTextures;
typedef void (APIENTRYP      PFNGLTEXTUREBUFFERPROC)(GLuint, GLenum, GLuint);
GLAPI PFNGLTEXTUREBUFFERPROC glad_glTextureBuffer;
typedef void (APIENTRYP      PFNGLTEXTUREBUFFERRANGEPROC)(GLuint, GLenum, GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLTEXTUREBUFFERRANGEPROC glad_glTextureBufferRange;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE1DPROC)(GLuint, GLsizei, GLenum, GLsizei);
GLAPI PFNGLTEXTURESTORAGE1DPROC glad_glTextureStorage1D;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE2DPROC)(GLuint, GLsizei, GLenum, GLsizei, GLsizei);
GLAPI PFNGLTEXTURESTORAGE2DPROC glad_glTextureStorage2D;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE3DPROC)(GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
GLAPI PFNGLTEXTURESTORAGE3DPROC glad_glTextureStorage3D;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC)(GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC glad_glTextureStorage2DMultisample;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC)(GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC glad_glTextureStorage3DMultisample;
typedef void (APIENTRYP      PFNGLTEXTURESUBIMAGE1DPROC)(GLuint, GLint, GLint, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTURESUBIMAGE1DPROC glad_glTextureSubImage1D;
typedef void (APIENTRYP      PFNGLTEXTURESUBIMAGE2DPROC)(GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTURESUBIMAGE2DPROC glad_glTextureSubImage2D;
typedef void (APIENTRYP      PFNGLTEXTURESUBIMAGE3DPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTURESUBIMAGE3DPROC glad_glTextureSubImage3D;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC)(GLuint, GLint, GLint, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC glad_glCompressedTextureSubImage1D;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC)(GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC glad_glCompressedTextureSubImage2D;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC glad_glCompressedTextureSubImage3D;
typedef void (APIENTRYP      PFNGLCOPYTEXTURESUBIMAGE1DPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei);
GLAPI PFNGLCOPYTEXTURESUBIMAGE1DPROC glad_glCopyTextureSubImage1D;
typedef void (APIENTRYP      PFNGLCOPYTEXTURESUBIMAGE2DPROC)(GLuint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYTEXTURESUBIMAGE2DPROC glad_glCopyTextureSubImage2D;
typedef void (APIENTRYP      PFNGLCOPYTEXTURESUBIMAGE3DPROC)(GLuint, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYTEXTURESUBIMAGE3DPROC glad_glCopyTextureSubImage3D;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERFPROC)(GLuint, GLenum, GLfloat);
GLAPI PFNGLTEXTUREPARAMETERFPROC glad_glTextureParameterf;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERFVPROC)(GLuint, GLenum, const GLfloat*);
GLAPI PFNGLTEXTUREPARAMETERFVPROC glad_glTextureParameterfv;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIPROC)(GLuint, GLenum, GLint);
GLAPI PFNGLTEXTUREPARAMETERIPROC glad_glTextureParameteri;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIIVPROC)(GLuint, GLenum, const GLint*);
GLAPI PFNGLTEXTUREPARAMETERIIVPROC glad_glTextureParameterIiv;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIUIVPROC)(GLuint, GLenum, const GLuint*);
GLAPI PFNGLTEXTUREPARAMETERIUIVPROC glad_glTextureParameterIuiv;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIVPROC)(GLuint, GLenum, const GLint*);
GLAPI PFNGLTEXTUREPARAMETERIVPROC glad_glTextureParameteriv;
typedef void (APIENTRYP      PFNGLGENERATETEXTUREMIPMAPPROC)(GLuint);
GLAPI PFNGLGENERATETEXTUREMIPMAPPROC glad_glGenerateTextureMipmap;
typedef void (APIENTRYP      PFNGLBINDTEXTUREUNITPROC)(GLuint, GLuint);
GLAPI PFNGLBINDTEXTUREUNITPROC glad_glBindTextureUnit;
typedef void (APIENTRYP      PFNGLGETTEXTUREIMAGEPROC)(GLuint, GLint, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLGETTEXTUREIMAGEPROC glad_glGetTextureImage;
typedef void (APIENTRYP      PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC)(GLuint, GLint, GLsizei, void*);
GLAPI PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC glad_glGetCompressedTextureImage;
typedef void (APIENTRYP      PFNGLGETTEXTURELEVELPARAMETERFVPROC)(GLuint, GLint, GLenum, GLfloat*);
GLAPI PFNGLGETTEXTURELEVELPARAMETERFVPROC glad_glGetTextureLevelParameterfv;
typedef void (APIENTRYP      PFNGLGETTEXTURELEVELPARAMETERIVPROC)(GLuint, GLint, GLenum, GLint*);
GLAPI PFNGLGETTEXTURELEVELPARAMETERIVPROC glad_glGetTextureLevelParameteriv;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERFVPROC)(GLuint, GLenum, GLfloat*);
GLAPI PFNGLGETTEXTUREPARAMETERFVPROC glad_glGetTextureParameterfv;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERIIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETTEXTUREPARAMETERIIVPROC glad_glGetTextureParameterIiv;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERIUIVPROC)(GLuint, GLenum, GLuint*);
GLAPI PFNGLGETTEXTUREPARAMETERIUIVPROC glad_glGetTextureParameterIuiv;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETTEXTUREPARAMETERIVPROC glad_glGetTextureParameteriv;
typedef void (APIENTRYP      PFNGLCREATEVERTEXARRAYSPROC)(GLsizei, GLuint*);
GLAPI PFNGLCREATEVERTEXARRAYSPROC glad_glCreateVertexArrays;
typedef void (APIENTRYP      PFNGLDISABLEVERTEXARRAYATTRIBPROC)(GLuint, GLuint);
GLAPI PFNGLDISABLEVERTEXARRAYATTRIBPROC glad_glDisableVertexArrayAttrib;
typedef void (APIENTRYP      PFNGLENABLEVERTEXARRAYATTRIBPROC)(GLuint, GLuint);
GLAPI PFNGLENABLEVERTEXARRAYATTRIBPROC glad_glEnableVertexArrayAttrib;
typedef void (APIENTRYP      PFNGLVERTEXARRAYELEMENTBUFFERPROC)(GLuint, GLuint);
GLAPI PFNGLVERTEXARRAYELEMENTBUFFERPROC glad_glVertexArrayElementBuffer;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXBUFFERPROC)(GLuint, GLuint, GLuint, GLintptr, GLsizei);
GLAPI PFNGLVERTEXARRAYVERTEXBUFFERPROC glad_glVertexArrayVertexBuffer;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXBUFFERSPROC)(GLuint, GLuint, GLsizei, const GLuint*, const GLintptr*, const GLsizei*);
GLAPI PFNGLVERTEXARRAYVERTEXBUFFERSPROC glad_glVertexArrayVertexBuffers;
typedef void (APIENTRYP      PFNGLVERTEXARRAYATTRIBBINDINGPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXARRAYATTRIBBINDINGPROC glad_glVertexArrayAttribBinding;
typedef void (APIENTRYP      PFNGLVERTEXARRAYATTRIBFORMATPROC)(GLuint, GLuint, GLint, GLenum, GLboolean, GLuint);
GLAPI PFNGLVERTEXARRAYATTRIBFORMATPROC glad_glVertexArrayAttribFormat;
typedef void (APIENTRYP      PFNGLVERTEXARRAYATTRIBIFORMATPROC)(GLuint, GLuint, GLint, GLenum, GLuint);
GLAPI PFNGLVERTEXARRAYATTRIBIFORMATPROC glad_glVertexArrayAttribIFormat;
typedef void (APIENTRYP      PFNGLVERTEXARRAYATTRIBLFORMATPROC)(GLuint, GLuint, GLint, GLenum, GLuint);
GLAPI PFNGLVERTEXARRAYATTRIBLFORMATPROC glad_glVertexArrayAttribLFormat;
typedef void (APIENTRYP      PFNGLVERTEXARRAYBINDINGDIVISORPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXARRAYBINDINGDIVISORPROC glad_glVertexArrayBindingDivisor;
typedef void (APIENTRYP      PFNGLGETVERTEXARRAYIVPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETVERTEXARRAYIVPROC glad_glGetVertexArrayiv;
typedef void (APIENTRYP      PFNGLGETVERTEXARRAYINDEXEDIVPROC)(GLuint, GLuint, GLenum, GLint*);
GLAPI PFNGLGETVERTEXARRAYINDEXEDIVPROC glad_glGetVertexArrayIndexediv;
typedef void (APIENTRYP      PFNGLGETVERTEXARRAYINDEXED64IVPROC)(GLuint, GLuint, GLenum, GLint64*);
GLAPI PFNGLGETVERTEXARRAYINDEXED64IVPROC glad_glGetVertexArrayIndexed64iv;
typedef void (APIENTRYP      PFNGLCREATESAMPLERSPROC)(GLsizei, GLuint*);
GLAPI PFNGLCREATESAMPLERSPROC glad_glCreateSamplers;
typedef void (APIENTRYP      PFNGLCREATEPROGRAMPIPELINESPROC)(GLsizei, GLuint*);
GLAPI PFNGLCREATEPROGRAMPIPELINESPROC glad_glCreateProgramPipelines;
typedef void (APIENTRYP      PFNGLCREATEQUERIESPROC)(GLenum, GLsizei, GLuint*);
GLAPI PFNGLCREATEQUERIESPROC glad_glCreateQueries;
typedef void (APIENTRYP      PFNGLGETQUERYBUFFEROBJECTI64VPROC)(GLuint, GLuint, GLenum, GLintptr);
GLAPI PFNGLGETQUERYBUFFEROBJECTI64VPROC glad_glGetQueryBufferObjecti64v;
typedef void (APIENTRYP      PFNGLGETQUERYBUFFEROBJECTIVPROC)(GLuint, GLuint, GLenum, GLintptr);
GLAPI PFNGLGETQUERYBUFFEROBJECTIVPROC glad_glGetQueryBufferObjectiv;
typedef void (APIENTRYP      PFNGLGETQUERYBUFFEROBJECTUI64VPROC)(GLuint, GLuint, GLenum, GLintptr);
GLAPI PFNGLGETQUERYBUFFEROBJECTUI64VPROC glad_glGetQueryBufferObjectui64v;
typedef void (APIENTRYP      PFNGLGETQUERYBUFFEROBJECTUIVPROC)(GLuint, GLuint, GLenum, GLintptr);
GLAPI PFNGLGETQUERYBUFFEROBJECTUIVPROC glad_glGetQueryBufferObjectuiv;
typedef void (APIENTRYP      PFNGLMEMORYBARRIERBYREGIONPROC)(GLbitfield);
GLAPI PFNGLMEMORYBARRIERBYREGIONPROC glad_glMemoryBarrierByRegion;
typedef void (APIENTRYP      PFNGLGETTEXTURESUBIMAGEPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLGETTEXTURESUBIMAGEPROC glad_glGetTextureSubImage;
typedef void (APIENTRYP      PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLsizei, void*);
GLAPI PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC glad_glGetCompressedTextureSubImage;
typedef GLenum (APIENTRYP    PFNGLGETGRAPHICSRESETSTATUSPROC)();
GLAPI PFNGLGETGRAPHICSRESETSTATUSPROC glad_glGetGraphicsResetStatus;
typedef void (APIENTRYP      PFNGLGETNCOMPRESSEDTEXIMAGEPROC)(GLenum, GLint, GLsizei, void*);
GLAPI PFNGLGETNCOMPRESSEDTEXIMAGEPROC glad_glGetnCompressedTexImage;
typedef void (APIENTRYP      PFNGLGETNTEXIMAGEPROC)(GLenum, GLint, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLGETNTEXIMAGEPROC glad_glGetnTexImage;
typedef void (APIENTRYP      PFNGLGETNUNIFORMDVPROC)(GLuint, GLint, GLsizei, GLdouble*);
GLAPI PFNGLGETNUNIFORMDVPROC glad_glGetnUniformdv;
typedef void (APIENTRYP      PFNGLGETNUNIFORMFVPROC)(GLuint, GLint, GLsizei, GLfloat*);
GLAPI PFNGLGETNUNIFORMFVPROC glad_glGetnUniformfv;
typedef void (APIENTRYP      PFNGLGETNUNIFORMIVPROC)(GLuint, GLint, GLsizei, GLint*);
GLAPI PFNGLGETNUNIFORMIVPROC glad_glGetnUniformiv;
typedef void (APIENTRYP      PFNGLGETNUNIFORMUIVPROC)(GLuint, GLint, GLsizei, GLuint*);
GLAPI PFNGLGETNUNIFORMUIVPROC glad_glGetnUniformuiv;
typedef void (APIENTRYP      PFNGLREADNPIXELSPROC)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLREADNPIXELSPROC glad_glReadnPixels;
typedef void (APIENTRYP      PFNGLGETNMAPDVPROC)(GLenum, GLenum, GLsizei, GLdouble*);
GLAPI PFNGLGETNMAPDVPROC glad_glGetnMapdv;
typedef void (APIENTRYP      PFNGLGETNMAPFVPROC)(GLenum, GLenum, GLsizei, GLfloat*);
GLAPI PFNGLGETNMAPFVPROC glad_glGetnMapfv;
typedef void (APIENTRYP      PFNGLGETNMAPIVPROC)(GLenum, GLenum, GLsizei, GLint*);
GLAPI PFNGLGETNMAPIVPROC glad_glGetnMapiv;
typedef void (APIENTRYP      PFNGLGETNPIXELMAPFVPROC)(GLenum, GLsizei, GLfloat*);
GLAPI PFNGLGETNPIXELMAPFVPROC glad_glGetnPixelMapfv;
typedef void (APIENTRYP      PFNGLGETNPIXELMAPUIVPROC)(GLenum, GLsizei, GLuint*);
GLAPI PFNGLGETNPIXELMAPUIVPROC glad_glGetnPixelMapuiv;
typedef void (APIENTRYP      PFNGLGETNPIXELMAPUSVPROC)(GLenum, GLsizei, GLushort*);
GLAPI PFNGLGETNPIXELMAPUSVPROC glad_glGetnPixelMapusv;
typedef void (APIENTRYP      PFNGLGETNPOLYGONSTIPPLEPROC)(GLsizei, GLubyte*);
GLAPI PFNGLGETNPOLYGONSTIPPLEPROC glad_glGetnPolygonStipple;
typedef void (APIENTRYP      PFNGLGETNCOLORTABLEPROC)(GLenum, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLGETNCOLORTABLEPROC glad_glGetnColorTable;
typedef void (APIENTRYP      PFNGLGETNCONVOLUTIONFILTERPROC)(GLenum, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLGETNCONVOLUTIONFILTERPROC glad_glGetnConvolutionFilter;
typedef void (APIENTRYP      PFNGLGETNSEPARABLEFILTERPROC)(GLenum, GLenum, GLenum, GLsizei, void*, GLsizei, void*, void*);
GLAPI PFNGLGETNSEPARABLEFILTERPROC glad_glGetnSeparableFilter;
typedef void (APIENTRYP      PFNGLGETNHISTOGRAMPROC)(GLenum, GLboolean, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLGETNHISTOGRAMPROC glad_glGetnHistogram;
typedef void (APIENTRYP      PFNGLGETNMINMAXPROC)(GLenum, GLboolean, GLenum, GLenum, GLsizei, void*);
GLAPI PFNGLGETNMINMAXPROC glad_glGetnMinmax;
typedef void (APIENTRYP      PFNGLTEXTUREBARRIERPROC)();
GLAPI PFNGLTEXTUREBARRIERPROC glad_glTextureBarrier;
#endif
#if defined(__cplusplus) && defined(GLAD_INSTRUMENT_CALL)
struct glad_tag_glCopyTexImage1D {};
inline void glad_wrapped_glCopyTexImage1D(GLenum _target, GLint _level, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width, GLint _border) { GLAD_INSTRUMENT_CALL(glCopyTexImage1D, _target, _level, _internalformat, _x, _y, _width, _border); }
	#define glCopyTexImage1D glad_wrapped_glCopyTexImage1D
struct glad_tag_glVertexAttribI3ui {};
inline void glad_wrapped_glVertexAttribI3ui(GLuint _index, GLuint _x, GLuint _y, GLuint _z) { GLAD_INSTRUMENT_CALL(glVertexAttribI3ui, _index, _x, _y, _z); }
	#define glVertexAttribI3ui glad_wrapped_glVertexAttribI3ui
struct glad_tag_glVertexArrayElementBuffer {};
inline void glad_wrapped_glVertexArrayElementBuffer(GLuint _vaobj, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glVertexArrayElementBuffer, _vaobj, _buffer); }
	#define glVertexArrayElementBuffer glad_wrapped_glVertexArrayElementBuffer
struct glad_tag_glStencilMaskSeparate {};
inline void glad_wrapped_glStencilMaskSeparate(GLenum _face, GLuint _mask) { GLAD_INSTRUMENT_CALL(glStencilMaskSeparate, _face, _mask); }
	#define glStencilMaskSeparate glad_wrapped_glStencilMaskSeparate
struct glad_tag_glTextureStorage3DMultisample {};
inline void glad_wrapped_glTextureStorage3DMultisample(GLuint _texture, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTextureStorage3DMultisample, _texture, _samples, _internalformat, _width, _height, _depth, _fixedsamplelocations); }
	#define glTextureStorage3DMultisample glad_wrapped_glTextureStorage3DMultisample
struct glad_tag_glTextureParameterfv {};
inline void glad_wrapped_glTextureParameterfv(GLuint _texture, GLenum _pname, const GLfloat* _param) { GLAD_INSTRUMENT_CALL(glTextureParameterfv, _texture, _pname, _param); }
	#define glTextureParameterfv glad_wrapped_glTextureParameterfv
struct glad_tag_glMinSampleShading {};
inline void glad_wrapped_glMinSampleShading(GLfloat _value) { GLAD_INSTRUMENT_CALL(glMinSampleShading, _value); }
	#define glMinSampleShading glad_wrapped_glMinSampleShading
struct glad_tag_glFramebufferRenderbuffer {};
inline void glad_wrapped_glFramebufferRenderbuffer(GLenum _target, GLenum _attachment, GLenum _renderbuffertarget, GLuint _renderbuffer) { GLAD_INSTRUMENT_CALL(glFramebufferRenderbuffer, _target, _attachment, _renderbuffertarget, _renderbuffer); }
	#define glFramebufferRenderbuffer glad_wrapped_glFramebufferRenderbuffer
struct glad_tag_glUniformSubroutinesuiv {};
inline void glad_wrapped_glUniformSubroutinesuiv(GLenum _shadertype, GLsizei _count, const GLuint* _indices) { GLAD_INSTRUMENT_CALL(glUniformSubroutinesuiv, _shadertype, _count, _indices); }
	#define glUniformSubroutinesuiv glad_wrapped_glUniformSubroutinesuiv
struct glad_tag_glCompressedTexSubImage3D {};
inline void glad_wrapped_glCompressedTexSubImage3D(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTexSubImage3D, _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _imageSize, _data); }
	#define glCompressedTexSubImage3D glad_wrapped_glCompressedTexSubImage3D
struct glad_tag_glTexCoordP3uiv {};
inline void glad_wrapped_glTexCoordP3uiv(GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP3uiv, _type, _coords); }
	#define glTexCoordP3uiv glad_wrapped_glTexCoordP3uiv
struct glad_tag_glGetDoublei_v {};
inline void glad_wrapped_glGetDoublei_v(GLenum _target, GLuint _index, GLdouble* _data) { GLAD_INSTRUMENT_CALL(glGetDoublei_v, _target, _index, _data); }
	#define glGetDoublei_v glad_wrapped_glGetDoublei_v
struct glad_tag_glVertexAttrib1sv {};
inline void glad_wrapped_glVertexAttrib1sv(GLuint _index, const GLshort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib1sv, _index, _v); }
	#define glVertexAttrib1sv glad_wrapped_glVertexAttrib1sv
struct glad_tag_glVertexArrayVertexBuffers {};
inline void glad_wrapped_glVertexArrayVertexBuffers(GLuint _vaobj, GLuint _first, GLsizei _count, const GLuint* _buffers, const GLintptr* _offsets, const GLsizei* _strides) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexBuffers, _vaobj, _first, _count, _buffers, _offsets, _strides); }
	#define glVertexArrayVertexBuffers glad_wrapped_glVertexArrayVertexBuffers
struct glad_tag_glBindSampler {};
inline void glad_wrapped_glBindSampler(GLuint _unit, GLuint _sampler) { GLAD_INSTRUMENT_CALL(glBindSampler, _unit, _sampler); }
	#define glBindSampler glad_wrapped_glBindSampler
struct glad_tag_glLineWidth {};
inline void glad_wrapped_glLineWidth(GLfloat _width) { GLAD_INSTRUMENT_CALL(glLineWidth, _width); }
	#define glLineWidth glad_wrapped_glLineWidth
struct glad_tag_glColorP3uiv {};
inline void glad_wrapped_glColorP3uiv(GLenum _type, const GLuint* _color) { GLAD_INSTRUMENT_CALL(glColorP3uiv, _type, _color); }
	#define glColorP3uiv glad_wrapped_glColorP3uiv
struct glad_tag_glGetIntegeri_v {};
inline void glad_wrapped_glGetIntegeri_v(GLenum _target, GLuint _index, GLint* _data) { GLAD_INSTRUMENT_CALL(glGetIntegeri_v, _target, _index, _data); }
	#define glGetIntegeri_v glad_wrapped_glGetIntegeri_v
struct glad_tag_glCompileShader {};
inline void glad_wrapped_glCompileShader(GLuint _shader) { GLAD_INSTRUMENT_CALL(glCompileShader, _shader); }
	#define glCompileShader glad_wrapped_glCompileShader
struct glad_tag_glGetTransformFeedbackVarying {};
inline void glad_wrapped_glGetTransformFeedbackVarying(GLuint _program, GLuint _index, GLsizei _bufSize, GLsizei* _length, GLsizei* _size, GLenum* _type, GLchar* _name) { GLAD_INSTRUMENT_CALL(glGetTransformFeedbackVarying, _program, _index, _bufSize, _length, _size, _type, _name); }
	#define glGetTransformFeedbackVarying glad_wrapped_glGetTransformFeedbackVarying
struct glad_tag_glCompressedTextureSubImage3D {};
inline void glad_wrapped_glCompressedTextureSubImage3D(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTextureSubImage3D, _texture, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _imageSize, _data); }
	#define glCompressedTextureSubImage3D glad_wrapped_glCompressedTextureSubImage3D
struct glad_tag_glGetCompressedTextureImage {};
inline void glad_wrapped_glGetCompressedTextureImage(GLuint _texture, GLint _level, GLsizei _bufSize, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetCompressedTextureImage, _texture, _level, _bufSize, _pixels); }
	#define glGetCompressedTextureImage glad_wrapped_glGetCompressedTextureImage
struct glad_tag_glGetnMapfv {};
inline void glad_wrapped_glGetnMapfv(GLenum _target, GLenum _query, GLsizei _bufSize, GLfloat* _v) { GLAD_INSTRUMENT_CALL(glGetnMapfv, _target, _query, _bufSize, _v); }
	#define glGetnMapfv glad_wrapped_glGetnMapfv
struct glad_tag_glTransformFeedbackBufferRange {};
inline void glad_wrapped_glTransformFeedbackBufferRange(GLuint _xfb, GLuint _index, GLuint _buffer, GLintptr _offset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glTransformFeedbackBufferRange, _xfb, _index, _buffer, _offset, _size); }
	#define glTransformFeedbackBufferRange glad_wrapped_glTransformFeedbackBufferRange
struct glad_tag_glGetTextureImage {};
inline void glad_wrapped_glGetTextureImage(GLuint _texture, GLint _level, GLenum _format, GLenum _type, GLsizei _bufSize, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetTextureImage, _texture, _level, _format, _type, _bufSize, _pixels); }
	#define glGetTextureImage glad_wrapped_glGetTextureImage
struct glad_tag_glDepthRangef {};
inline void glad_wrapped_glDepthRangef(GLfloat _n, GLfloat _f) { GLAD_INSTRUMENT_CALL(glDepthRangef, _n, _f); }
	#define glDepthRangef glad_wrapped_glDepthRangef
struct glad_tag_glVertexAttribIPointer {};
inline void glad_wrapped_glVertexAttribIPointer(GLuint _index, GLint _size, GLenum _type, GLsizei _stride, const void* _pointer) { GLAD_INSTRUMENT_CALL(glVertexAttribIPointer, _index, _size, _type, _stride, _pointer); }
	#define glVertexAttribIPointer glad_wrapped_glVertexAttribIPointer
struct glad_tag_glMultiTexCoordP3ui {};
inline void glad_wrapped_glMultiTexCoordP3ui(GLenum _texture, GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP3ui, _texture, _type, _coords); }
	#define glMultiTexCoordP3ui glad_wrapped_glMultiTexCoordP3ui
struct glad_tag_glGetNamedBufferParameteriv {};
inline void glad_wrapped_glGetNamedBufferParameteriv(GLuint _buffer, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedBufferParameteriv, _buffer, _pname, _params); }
	#define glGetNamedBufferParameteriv glad_wrapped_glGetNamedBufferParameteriv
struct glad_tag_glVertexP4ui {};
inline void glad_wrapped_glVertexP4ui(GLenum _type, GLuint _value) { GLAD_INSTRUMENT_CALL(glVertexP4ui, _type, _value); }
	#define glVertexP4ui glad_wrapped_glVertexP4ui
struct glad_tag_glDrawElementsInstancedBaseInstance {};
inline void glad_wrapped_glDrawElementsInstancedBaseInstance(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices, GLsizei _instancecount, GLuint _baseinstance) { GLAD_INSTRUMENT_CALL(glDrawElementsInstancedBaseInstance, _mode, _count, _type, _indices, _instancecount, _baseinstance); }
	#define glDrawElementsInstancedBaseInstance glad_wrapped_glDrawElementsInstancedBaseInstance
struct glad_tag_glEnablei {};
inline void glad_wrapped_glEnablei(GLenum _target, GLuint _index) { GLAD_INSTRUMENT_CALL(glEnablei, _target, _index); }
	#define glEnablei glad_wrapped_glEnablei
struct glad_tag_glVertexAttribP4ui {};
inline void glad_wrapped_glVertexAttribP4ui(GLuint _index, GLenum _type, GLboolean _normalized, GLuint _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP4ui, _index, _type, _normalized, _value); }
	#define glVertexAttribP4ui glad_wrapped_glVertexAttribP4ui
struct glad_tag_glCreateShader {};
inline GLuint glad_wrapped_glCreateShader(GLenum _type) { return GLAD_INSTRUMENT_CALL(glCreateShader, _type); }
	#define glCreateShader glad_wrapped_glCreateShader
struct glad_tag_glIsBuffer {};
inline GLboolean glad_wrapped_glIsBuffer(GLuint _buffer) { return GLAD_INSTRUMENT_CALL(glIsBuffer, _buffer); }
	#define glIsBuffer glad_wrapped_glIsBuffer
struct glad_tag_glGetMultisamplefv {};
inline void glad_wrapped_glGetMultisamplefv(GLenum _pname, GLuint _index, GLfloat* _val) { GLAD_INSTRUMENT_CALL(glGetMultisamplefv, _pname, _index, _val); }
	#define glGetMultisamplefv glad_wrapped_glGetMultisamplefv
struct glad_tag_glProgramUniformMatrix2dv {};
inline void glad_wrapped_glProgramUniformMatrix2dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2dv glad_wrapped_glProgramUniformMatrix2dv
struct glad_tag_glGenRenderbuffers {};
inline void glad_wrapped_glGenRenderbuffers(GLsizei _n, GLuint* _renderbuffers) { GLAD_INSTRUMENT_CALL(glGenRenderbuffers, _n, _renderbuffers); }
	#define glGenRenderbuffers glad_wrapped_glGenRenderbuffers
struct glad_tag_glCopyTexSubImage2D {};
inline void glad_wrapped_glCopyTexSubImage2D(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyTexSubImage2D, _target, _level, _xoffset, _yoffset, _x, _y, _width, _height); }
	#define glCopyTexSubImage2D glad_wrapped_glCopyTexSubImage2D
struct glad_tag_glCompressedTexImage2D {};
inline void glad_wrapped_glCompressedTexImage2D(GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLsizei _height, GLint _border, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTexImage2D, _target, _level, _internalformat, _width, _height, _border, _imageSize, _data); }
	#define glCompressedTexImage2D glad_wrapped_glCompressedTexImage2D
struct glad_tag_glVertexAttrib1f {};
inline void glad_wrapped_glVertexAttrib1f(GLuint _index, GLfloat _x) { GLAD_INSTRUMENT_CALL(glVertexAttrib1f, _index, _x); }
	#define glVertexAttrib1f glad_wrapped_glVertexAttrib1f
struct glad_tag_glBlendFuncSeparate {};
inline void glad_wrapped_glBlendFuncSeparate(GLenum _sfactorRGB, GLenum _dfactorRGB, GLenum _sfactorAlpha, GLenum _dfactorAlpha) { GLAD_INSTRUMENT_CALL(glBlendFuncSeparate, _sfactorRGB, _dfactorRGB, _sfactorAlpha, _dfactorAlpha); }
	#define glBlendFuncSeparate glad_wrapped_glBlendFuncSeparate
struct glad_tag_glProgramUniformMatrix4fv {};
inline void glad_wrapped_glProgramUniformMatrix4fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4fv glad_wrapped_glProgramUniformMatrix4fv
struct glad_tag_glClearNamedFramebufferfi {};
inline void glad_wrapped_glClearNamedFramebufferfi(GLuint _framebuffer, GLenum _buffer, const GLfloat _depth, GLint _stencil) { GLAD_INSTRUMENT_CALL(glClearNamedFramebufferfi, _framebuffer, _buffer, _depth, _stencil); }
	#define glClearNamedFramebufferfi glad_wrapped_glClearNamedFramebufferfi
struct glad_tag_glGetQueryBufferObjectuiv {};
inline void glad_wrapped_glGetQueryBufferObjectuiv(GLuint _id, GLuint _buffer, GLenum _pname, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glGetQueryBufferObjectuiv, _id, _buffer, _pname, _offset); }
	#define glGetQueryBufferObjectuiv glad_wrapped_glGetQueryBufferObjectuiv
struct glad_tag_glHint {};
inline void glad_wrapped_glHint(GLenum _target, GLenum _mode) { GLAD_INSTRUMENT_CALL(glHint, _target, _mode); }
	#define glHint glad_wrapped_glHint
struct glad_tag_glVertexAttrib1s {};
inline void glad_wrapped_glVertexAttrib1s(GLuint _index, GLshort _x) { GLAD_INSTRUMENT_CALL(glVertexAttrib1s, _index, _x); }
	#define glVertexAttrib1s glad_wrapped_glVertexAttrib1s
struct glad_tag_glSampleMaski {};
inline void glad_wrapped_glSampleMaski(GLuint _maskNumber, GLbitfield _mask) { GLAD_INSTRUMENT_CALL(glSampleMaski, _maskNumber, _mask); }
	#define glSampleMaski glad_wrapped_glSampleMaski
struct glad_tag_glVertexP2ui {};
inline void glad_wrapped_glVertexP2ui(GLenum _type, GLuint _value) { GLAD_INSTRUMENT_CALL(glVertexP2ui, _type, _value); }
	#define glVertexP2ui glad_wrapped_glVertexP2ui
struct glad_tag_glUniformMatrix3x2fv {};
inline void glad_wrapped_glUniformMatrix3x2fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix3x2fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix3x2fv glad_wrapped_glUniformMatrix3x2fv
struct glad_tag_glDebugMessageControl {};
inline void glad_wrapped_glDebugMessageControl(GLenum _source, GLenum _type, GLenum _severity, GLsizei _count, const GLuint* _ids, GLboolean _enabled) { GLAD_INSTRUMENT_CALL(glDebugMessageControl, _source, _type, _severity, _count, _ids, _enabled); }
	#define glDebugMessageControl glad_wrapped_glDebugMessageControl
struct glad_tag_glPointSize {};
inline void glad_wrapped_glPointSize(GLfloat _size) { GLAD_INSTRUMENT_CALL(glPointSize, _size); }
	#define glPointSize glad_wrapped_glPointSize
struct glad_tag_glBindTextureUnit {};
inline void glad_wrapped_glBindTextureUnit(GLuint _unit, GLuint _texture) { GLAD_INSTRUMENT_CALL(glBindTextureUnit, _unit, _texture); }
	#define glBindTextureUnit glad_wrapped_glBindTextureUnit
struct glad_tag_glVertexAttrib2dv {};
inline void glad_wrapped_glVertexAttrib2dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib2dv, _index, _v); }
	#define glVertexAttrib2dv glad_wrapped_glVertexAttrib2dv
struct glad_tag_glDeleteProgram {};
inline void glad_wrapped_glDeleteProgram(GLuint _program) { GLAD_INSTRUMENT_CALL(glDeleteProgram, _program); }
	#define glDeleteProgram glad_wrapped_glDeleteProgram
struct glad_tag_glVertexAttrib4Nuiv {};
inline void glad_wrapped_glVertexAttrib4Nuiv(GLuint _index, const GLuint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4Nuiv, _index, _v); }
	#define glVertexAttrib4Nuiv glad_wrapped_glVertexAttrib4Nuiv
struct glad_tag_glTexStorage2D {};
inline void glad_wrapped_glTexStorage2D(GLenum _target, GLsizei _levels, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glTexStorage2D, _target, _levels, _internalformat, _width, _height); }
	#define glTexStorage2D glad_wrapped_glTexStorage2D
struct glad_tag_glRenderbufferStorage {};
inline void glad_wrapped_glRenderbufferStorage(GLenum _target, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glRenderbufferStorage, _target, _internalformat, _width, _height); }
	#define glRenderbufferStorage glad_wrapped_glRenderbufferStorage
struct glad_tag_glWaitSync {};
inline void glad_wrapped_glWaitSync(GLsync _sync, GLbitfield _flags, GLuint64 _timeout) { GLAD_INSTRUMENT_CALL(glWaitSync, _sync, _flags, _timeout); }
	#define glWaitSync glad_wrapped_glWaitSync
struct glad_tag_glUniformMatrix4x3fv {};
inline void glad_wrapped_glUniformMatrix4x3fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix4x3fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix4x3fv glad_wrapped_glUniformMatrix4x3fv
struct glad_tag_glUniform3i {};
inline void glad_wrapped_glUniform3i(GLint _location, GLint _v0, GLint _v1, GLint _v2) { GLAD_INSTRUMENT_CALL(glUniform3i, _location, _v0, _v1, _v2); }
	#define glUniform3i glad_wrapped_glUniform3i
struct glad_tag_glClearBufferfv {};
inline void glad_wrapped_glClearBufferfv(GLenum _buffer, GLint _drawbuffer, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glClearBufferfv, _buffer, _drawbuffer, _value); }
	#define glClearBufferfv glad_wrapped_glClearBufferfv
struct glad_tag_glProgramUniform1ui {};
inline void glad_wrapped_glProgramUniform1ui(GLuint _program, GLint _location, GLuint _v0) { GLAD_INSTRUMENT_CALL(glProgramUniform1ui, _program, _location, _v0); }
	#define glProgramUniform1ui glad_wrapped_glProgramUniform1ui
struct glad_tag_glBlendEquationSeparatei {};
inline void glad_wrapped_glBlendEquationSeparatei(GLuint _buf, GLenum _modeRGB, GLenum _modeAlpha) { GLAD_INSTRUMENT_CALL(glBlendEquationSeparatei, _buf, _modeRGB, _modeAlpha); }
	#define glBlendEquationSeparatei glad_wrapped_glBlendEquationSeparatei
struct glad_tag_glGetnMapiv {};
inline void glad_wrapped_glGetnMapiv(GLenum _target, GLenum _query, GLsizei _bufSize, GLint* _v) { GLAD_INSTRUMENT_CALL(glGetnMapiv, _target, _query, _bufSize, _v); }
	#define glGetnMapiv glad_wrapped_glGetnMapiv
struct glad_tag_glTextureBarrier {};
inline void glad_wrapped_glTextureBarrier() { GLAD_INSTRUMENT_CALL(glTextureBarrier); }
	#define glTextureBarrier glad_wrapped_glTextureBarrier
struct glad_tag_glUniform3d {};
inline void glad_wrapped_glUniform3d(GLint _location, GLdouble _x, GLdouble _y, GLdouble _z) { GLAD_INSTRUMENT_CALL(glUniform3d, _location, _x, _y, _z); }
	#define glUniform3d glad_wrapped_glUniform3d
struct glad_tag_glUniform3f {};
inline void glad_wrapped_glUniform3f(GLint _location, GLfloat _v0, GLfloat _v1, GLfloat _v2) { GLAD_INSTRUMENT_CALL(glUniform3f, _location, _v0, _v1, _v2); }
	#define glUniform3f glad_wrapped_glUniform3f
struct glad_tag_glVertexAttrib4ubv {};
inline void glad_wrapped_glVertexAttrib4ubv(GLuint _index, const GLubyte* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4ubv, _index, _v); }
	#define glVertexAttrib4ubv glad_wrapped_glVertexAttrib4ubv
struct glad_tag_glGetBufferParameteriv {};
inline void glad_wrapped_glGetBufferParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetBufferParameteriv, _target, _pname, _params); }
	#define glGetBufferParameteriv glad_wrapped_glGetBufferParameteriv
struct glad_tag_glTexCoordP2ui {};
inline void glad_wrapped_glTexCoordP2ui(GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP2ui, _type, _coords); }
	#define glTexCoordP2ui glad_wrapped_glTexCoordP2ui
struct glad_tag_glColorMaski {};
inline void glad_wrapped_glColorMaski(GLuint _index, GLboolean _r, GLboolean _g, GLboolean _b, GLboolean _a) { GLAD_INSTRUMENT_CALL(glColorMaski, _index, _r, _g, _b, _a); }
	#define glColorMaski glad_wrapped_glColorMaski
struct glad_tag_glClearBufferfi {};
inline void glad_wrapped_glClearBufferfi(GLenum _buffer, GLint _drawbuffer, GLfloat _depth, GLint _stencil) { GLAD_INSTRUMENT_CALL(glClearBufferfi, _buffer, _drawbuffer, _depth, _stencil); }
	#define glClearBufferfi glad_wrapped_glClearBufferfi
struct glad_tag_glDrawArraysIndirect {};
inline void glad_wrapped_glDrawArraysIndirect(GLenum _mode, const void* _indirect) { GLAD_INSTRUMENT_CALL(glDrawArraysIndirect, _mode, _indirect); }
	#define glDrawArraysIndirect glad_wrapped_glDrawArraysIndirect
struct glad_tag_glGenVertexArrays {};
inline void glad_wrapped_glGenVertexArrays(GLsizei _n, GLuint* _arrays) { GLAD_INSTRUMENT_CALL(glGenVertexArrays, _n, _arrays); }
	#define glGenVertexArrays glad_wrapped_glGenVertexArrays
struct glad_tag_glPauseTransformFeedback {};
inline void glad_wrapped_glPauseTransformFeedback() { GLAD_INSTRUMENT_CALL(glPauseTransformFeedback); }
	#define glPauseTransformFeedback glad_wrapped_glPauseTransformFeedback
struct glad_tag_glMultiTexCoordP2ui {};
inline void glad_wrapped_glMultiTexCoordP2ui(GLenum _texture, GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP2ui, _texture, _type, _coords); }
	#define glMultiTexCoordP2ui glad_wrapped_glMultiTexCoordP2ui
struct glad_tag_glProgramUniformMatrix3x2dv {};
inline void glad_wrapped_glProgramUniformMatrix3x2dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x2dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x2dv glad_wrapped_glProgramUniformMatrix3x2dv
struct glad_tag_glCopyNamedBufferSubData {};
inline void glad_wrapped_glCopyNamedBufferSubData(GLuint _readBuffer, GLuint _writeBuffer, GLintptr _readOffset, GLintptr _writeOffset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glCopyNamedBufferSubData, _readBuffer, _writeBuffer, _readOffset, _writeOffset, _size); }
	#define glCopyNamedBufferSubData glad_wrapped_glCopyNamedBufferSubData
struct glad_tag_glProgramUniformMatrix3x2fv {};
inline void glad_wrapped_glProgramUniformMatrix3x2fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x2fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x2fv glad_wrapped_glProgramUniformMatrix3x2fv
struct glad_tag_glGetSamplerParameterIiv {};
inline void glad_wrapped_glGetSamplerParameterIiv(GLuint _sampler, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetSamplerParameterIiv, _sampler, _pname, _params); }
	#define glGetSamplerParameterIiv glad_wrapped_glGetSamplerParameterIiv
struct glad_tag_glGetFragDataIndex {};
inline GLint glad_wrapped_glGetFragDataIndex(GLuint _program, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetFragDataIndex, _program, _name); }
	#define glGetFragDataIndex glad_wrapped_glGetFragDataIndex
struct glad_tag_glVertexAttribL4d {};
inline void glad_wrapped_glVertexAttribL4d(GLuint _index, GLdouble _x, GLdouble _y, GLdouble _z, GLdouble _w) { GLAD_INSTRUMENT_CALL(glVertexAttribL4d, _index, _x, _y, _z, _w); }
	#define glVertexAttribL4d glad_wrapped_glVertexAttribL4d
struct glad_tag_glBindImageTexture {};
inline void glad_wrapped_glBindImageTexture(GLuint _unit, GLuint _texture, GLint _level, GLboolean _layered, GLint _layer, GLenum _access, GLenum _format) { GLAD_INSTRUMENT_CALL(glBindImageTexture, _unit, _texture, _level, _layered, _layer, _access, _format); }
	#define glBindImageTexture glad_wrapped_glBindImageTexture
struct glad_tag_glTextureParameteriv {};
inline void glad_wrapped_glTextureParameteriv(GLuint _texture, GLenum _pname, const GLint* _param) { GLAD_INSTRUMENT_CALL(glTextureParameteriv, _texture, _pname, _param); }
	#define glTextureParameteriv glad_wrapped_glTextureParameteriv
struct glad_tag_glGetQueryBufferObjecti64v {};
inline void glad_wrapped_glGetQueryBufferObjecti64v(GLuint _id, GLuint _buffer, GLenum _pname, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glGetQueryBufferObjecti64v, _id, _buffer, _pname, _offset); }
	#define glGetQueryBufferObjecti64v glad_wrapped_glGetQueryBufferObjecti64v
struct glad_tag_glGetVertexAttribdv {};
inline void glad_wrapped_glGetVertexAttribdv(GLuint _index, GLenum _pname, GLdouble* _params) { GLAD_INSTRUMENT_CALL(glGetVertexAttribdv, _index, _pname, _params); }
	#define glGetVertexAttribdv glad_wrapped_glGetVertexAttribdv
struct glad_tag_glActiveShaderProgram {};
inline void glad_wrapped_glActiveShaderProgram(GLuint _pipeline, GLuint _program) { GLAD_INSTRUMENT_CALL(glActiveShaderProgram, _pipeline, _program); }
	#define glActiveShaderProgram glad_wrapped_glActiveShaderProgram
struct glad_tag_glUniformMatrix3x4fv {};
inline void glad_wrapped_glUniformMatrix3x4fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix3x4fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix3x4fv glad_wrapped_glUniformMatrix3x4fv
struct glad_tag_glUniformMatrix3dv {};
inline void glad_wrapped_glUniformMatrix3dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix3dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix3dv glad_wrapped_glUniformMatrix3dv
struct glad_tag_glProgramUniformMatrix3x4dv {};
inline void glad_wrapped_glProgramUniformMatrix3x4dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x4dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x4dv glad_wrapped_glProgramUniformMatrix3x4dv
struct glad_tag_glNamedFramebufferTexture {};
inline void glad_wrapped_glNamedFramebufferTexture(GLuint _framebuffer, GLenum _attachment, GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTexture, _framebuffer, _attachment, _texture, _level); }
	#define glNamedFramebufferTexture glad_wrapped_glNamedFramebufferTexture
struct glad_tag_glGetTextureParameterfv {};
inline void glad_wrapped_glGetTextureParameterfv(GLuint _texture, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameterfv, _texture, _pname, _params); }
	#define glGetTextureParameterfv glad_wrapped_glGetTextureParameterfv
struct glad_tag_glInvalidateBufferSubData {};
inline void glad_wrapped_glInvalidateBufferSubData(GLuint _buffer, GLintptr _offset, GLsizeiptr _length) { GLAD_INSTRUMENT_CALL(glInvalidateBufferSubData, _buffer, _offset, _length); }
	#define glInvalidateBufferSubData glad_wrapped_glInvalidateBufferSubData
struct glad_tag_glResumeTransformFeedback {};
inline void glad_wrapped_glResumeTransformFeedback() { GLAD_INSTRUMENT_CALL(glResumeTransformFeedback); }
	#define glResumeTransformFeedback glad_wrapped_glResumeTransformFeedback
struct glad_tag_glMultiTexCoordP4ui {};
inline void glad_wrapped_glMultiTexCoordP4ui(GLenum _texture, GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP4ui, _texture, _type, _coords); }
	#define glMultiTexCoordP4ui glad_wrapped_glMultiTexCoordP4ui
struct glad_tag_glProgramUniformMatrix4x3fv {};
inline void glad_wrapped_glProgramUniformMatrix4x3fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x3fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x3fv glad_wrapped_glProgramUniformMatrix4x3fv
struct glad_tag_glViewportArrayv {};
inline void glad_wrapped_glViewportArrayv(GLuint _first, GLsizei _count, const GLfloat* _v) { GLAD_INSTRUMENT_CALL(glViewportArrayv, _first, _count, _v); }
	#define glViewportArrayv glad_wrapped_glViewportArrayv
struct glad_tag_glDeleteFramebuffers {};
inline void glad_wrapped_glDeleteFramebuffers(GLsizei _n, const GLuint* _framebuffers) { GLAD_INSTRUMENT_CALL(glDeleteFramebuffers, _n, _framebuffers); }
	#define glDeleteFramebuffers glad_wrapped_glDeleteFramebuffers
struct glad_tag_glDrawArrays {};
inline void glad_wrapped_glDrawArrays(GLenum _mode, GLint _first, GLsizei _count) { GLAD_INSTRUMENT_CALL(glDrawArrays, _mode, _first, _count); }
	#define glDrawArrays glad_wrapped_glDrawArrays
struct glad_tag_glUniform1ui {};
inline void glad_wrapped_glUniform1ui(GLint _location, GLuint _v0) { GLAD_INSTRUMENT_CALL(glUniform1ui, _location, _v0); }
	#define glUniform1ui glad_wrapped_glUniform1ui
struct glad_tag_glProgramUniform2uiv {};
inline void glad_wrapped_glProgramUniform2uiv(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2uiv, _program, _location, _count, _value); }
	#define glProgramUniform2uiv glad_wrapped_glProgramUniform2uiv
struct glad_tag_glVertexAttribI2i {};
inline void glad_wrapped_glVertexAttribI2i(GLuint _index, GLint _x, GLint _y) { GLAD_INSTRUMENT_CALL(glVertexAttribI2i, _index, _x, _y); }
	#define glVertexAttribI2i glad_wrapped_glVertexAttribI2i
struct glad_tag_glTexCoordP3ui {};
inline void glad_wrapped_glTexCoordP3ui(GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP3ui, _type, _coords); }
	#define glTexCoordP3ui glad_wrapped_glTexCoordP3ui
struct glad_tag_glVertexAttrib3d {};
inline void glad_wrapped_glVertexAttrib3d(GLuint _index, GLdouble _x, GLdouble _y, GLdouble _z) { GLAD_INSTRUMENT_CALL(glVertexAttrib3d, _index, _x, _y, _z); }
	#define glVertexAttrib3d glad_wrapped_glVertexAttrib3d
struct glad_tag_glClear {};
inline void glad_wrapped_glClear(GLbitfield _mask) { GLAD_INSTRUMENT_CALL(glClear, _mask); }
	#define glClear glad_wrapped_glClear
struct glad_tag_glProgramParameteri {};
inline void glad_wrapped_glProgramParameteri(GLuint _program, GLenum _pname, GLint _value) { GLAD_INSTRUMENT_CALL(glProgramParameteri, _program, _pname, _value); }
	#define glProgramParameteri glad_wrapped_glProgramParameteri
struct glad_tag_glGetActiveUniformName {};
inline void glad_wrapped_glGetActiveUniformName(GLuint _program, GLuint _uniformIndex, GLsizei _bufSize, GLsizei* _length, GLchar* _uniformName) { GLAD_INSTRUMENT_CALL(glGetActiveUniformName, _program, _uniformIndex, _bufSize, _length, _uniformName); }
	#define glGetActiveUniformName glad_wrapped_glGetActiveUniformName
struct glad_tag_glMemoryBarrier {};
inline void glad_wrapped_glMemoryBarrier(GLbitfield _barriers) { GLAD_INSTRUMENT_CALL(glMemoryBarrier, _barriers); }
	#define glMemoryBarrier glad_wrapped_glMemoryBarrier
struct glad_tag_glGetGraphicsResetStatus {};
inline GLenum glad_wrapped_glGetGraphicsResetStatus() { return GLAD_INSTRUMENT_CALL(glGetGraphicsResetStatus); }
	#define glGetGraphicsResetStatus glad_wrapped_glGetGraphicsResetStatus
struct glad_tag_glIsEnabled {};
inline GLboolean glad_wrapped_glIsEnabled(GLenum _cap) { return GLAD_INSTRUMENT_CALL(glIsEnabled, _cap); }
	#define glIsEnabled glad_wrapped_glIsEnabled
struct glad_tag_glStencilOp {};
inline void glad_wrapped_glStencilOp(GLenum _fail, GLenum _zfail, GLenum _zpass) { GLAD_INSTRUMENT_CALL(glStencilOp, _fail, _zfail, _zpass); }
	#define glStencilOp glad_wrapped_glStencilOp
struct glad_tag_glFramebufferTexture2D {};
inline void glad_wrapped_glFramebufferTexture2D(GLenum _target, GLenum _attachment, GLenum _textarget, GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glFramebufferTexture2D, _target, _attachment, _textarget, _texture, _level); }
	#define glFramebufferTexture2D glad_wrapped_glFramebufferTexture2D
struct glad_tag_glGetFramebufferAttachmentParameteriv {};
inline void glad_wrapped_glGetFramebufferAttachmentParameteriv(GLenum _target, GLenum _attachment, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetFramebufferAttachmentParameteriv, _target, _attachment, _pname, _params); }
	#define glGetFramebufferAttachmentParameteriv glad_wrapped_glGetFramebufferAttachmentParameteriv
struct glad_tag_glVertexAttrib4Nub {};
inline void glad_wrapped_glVertexAttrib4Nub(GLuint _index, GLubyte _x, GLubyte _y, GLubyte _z, GLubyte _w) { GLAD_INSTRUMENT_CALL(glVertexAttrib4Nub, _index, _x, _y, _z, _w); }
	#define glVertexAttrib4Nub glad_wrapped_glVertexAttrib4Nub
struct glad_tag_glMapNamedBufferRange {};
inline void* glad_wrapped_glMapNamedBufferRange(GLuint _buffer, GLintptr _offset, GLsizeiptr _length, GLbitfield _access) { return GLAD_INSTRUMENT_CALL(glMapNamedBufferRange, _buffer, _offset, _length, _access); }
	#define glMapNamedBufferRange glad_wrapped_glMapNamedBufferRange
struct glad_tag_glGetFragDataLocation {};
inline GLint glad_wrapped_glGetFragDataLocation(GLuint _program, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetFragDataLocation, _program, _name); }
	#define glGetFragDataLocation glad_wrapped_glGetFragDataLocation
struct glad_tag_glGetTextureParameterIiv {};
inline void glad_wrapped_glGetTextureParameterIiv(GLuint _texture, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameterIiv, _texture, _pname, _params); }
	#define glGetTextureParameterIiv glad_wrapped_glGetTextureParameterIiv
struct glad_tag_glTexImage1D {};
inline void glad_wrapped_glTexImage1D(GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTexImage1D, _target, _level, _internalformat, _width, _border, _format, _type, _pixels); }
	#define glTexImage1D glad_wrapped_glTexImage1D
struct glad_tag_glTexParameteriv {};
inline void glad_wrapped_glTexParameteriv(GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glTexParameteriv, _target, _pname, _params); }
	#define glTexParameteriv glad_wrapped_glTexParameteriv
struct glad_tag_glVertexArrayAttribIFormat {};
inline void glad_wrapped_glVertexArrayAttribIFormat(GLuint _vaobj, GLuint _attribindex, GLint _size, GLenum _type, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexArrayAttribIFormat, _vaobj, _attribindex, _size, _type, _relativeoffset); }
	#define glVertexArrayAttribIFormat glad_wrapped_glVertexArrayAttribIFormat
struct glad_tag_glVertexArrayVertexBuffer {};
inline void glad_wrapped_glVertexArrayVertexBuffer(GLuint _vaobj, GLuint _bindingindex, GLuint _buffer, GLintptr _offset, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexBuffer, _vaobj, _bindingindex, _buffer, _offset, _stride); }
	#define glVertexArrayVertexBuffer glad_wrapped_glVertexArrayVertexBuffer
struct glad_tag_glGetTexImage {};
inline void glad_wrapped_glGetTexImage(GLenum _target, GLint _level, GLenum _format, GLenum _type, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetTexImage, _target, _level, _format, _type, _pixels); }
	#define glGetTexImage glad_wrapped_glGetTexImage
struct glad_tag_glGetQueryObjecti64v {};
inline void glad_wrapped_glGetQueryObjecti64v(GLuint _id, GLenum _pname, GLint64* _params) { GLAD_INSTRUMENT_CALL(glGetQueryObjecti64v, _id, _pname, _params); }
	#define glGetQueryObjecti64v glad_wrapped_glGetQueryObjecti64v
struct glad_tag_glGenFramebuffers {};
inline void glad_wrapped_glGenFramebuffers(GLsizei _n, GLuint* _framebuffers) { GLAD_INSTRUMENT_CALL(glGenFramebuffers, _n, _framebuffers); }
	#define glGenFramebuffers glad_wrapped_glGenFramebuffers
struct glad_tag_glTransformFeedbackBufferBase {};
inline void glad_wrapped_glTransformFeedbackBufferBase(GLuint _xfb, GLuint _index, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glTransformFeedbackBufferBase, _xfb, _index, _buffer); }
	#define glTransformFeedbackBufferBase glad_wrapped_glTransformFeedbackBufferBase
struct glad_tag_glClearTexSubImage {};
inline void glad_wrapped_glClearTexSubImage(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearTexSubImage, _texture, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _type, _data); }
	#define glClearTexSubImage glad_wrapped_glClearTexSubImage
struct glad_tag_glGetAttachedShaders {};
inline void glad_wrapped_glGetAttachedShaders(GLuint _program, GLsizei _maxCount, GLsizei* _count, GLuint* _shaders) { GLAD_INSTRUMENT_CALL(glGetAttachedShaders, _program, _maxCount, _count, _shaders); }
	#define glGetAttachedShaders glad_wrapped_glGetAttachedShaders
struct glad_tag_glIsRenderbuffer {};
inline GLboolean glad_wrapped_glIsRenderbuffer(GLuint _renderbuffer) { return GLAD_INSTRUMENT_CALL(glIsRenderbuffer, _renderbuffer); }
	#define glIsRenderbuffer glad_wrapped_glIsRenderbuffer
struct glad_tag_glDeleteVertexArrays {};
inline void glad_wrapped_glDeleteVertexArrays(GLsizei _n, const GLuint* _arrays) { GLAD_INSTRUMENT_CALL(glDeleteVertexArrays, _n, _arrays); }
	#define glDeleteVertexArrays glad_wrapped_glDeleteVertexArrays
struct glad_tag_glBindVertexBuffers {};
inline void glad_wrapped_glBindVertexBuffers(GLuint _first, GLsizei _count, const GLuint* _buffers, const GLintptr* _offsets, const GLsizei* _strides) { GLAD_INSTRUMENT_CALL(glBindVertexBuffers, _first, _count, _buffers, _offsets, _strides); }
	#define glBindVertexBuffers glad_wrapped_glBindVertexBuffers
struct glad_tag_glProgramUniform1uiv {};
inline void glad_wrapped_glProgramUniform1uiv(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1uiv, _program, _location, _count, _value); }
	#define glProgramUniform1uiv glad_wrapped_glProgramUniform1uiv
struct glad_tag_glIsVertexArray {};
inline GLboolean glad_wrapped_glIsVertexArray(GLuint _array) { return GLAD_INSTRUMENT_CALL(glIsVertexArray, _array); }
	#define glIsVertexArray glad_wrapped_glIsVertexArray
struct glad_tag_glDisableVertexAttribArray {};
inline void glad_wrapped_glDisableVertexAttribArray(GLuint _index) { GLAD_INSTRUMENT_CALL(glDisableVertexAttribArray, _index); }
	#define glDisableVertexAttribArray glad_wrapped_glDisableVertexAttribArray
struct glad_tag_glProgramUniform2iv {};
inline void glad_wrapped_glProgramUniform2iv(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2iv, _program, _location, _count, _value); }
	#define glProgramUniform2iv glad_wrapped_glProgramUniform2iv
struct glad_tag_glGetQueryiv {};
inline void glad_wrapped_glGetQueryiv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetQueryiv, _target, _pname, _params); }
	#define glGetQueryiv glad_wrapped_glGetQueryiv
struct glad_tag_glGetTransformFeedbackiv {};
inline void glad_wrapped_glGetTransformFeedbackiv(GLuint _xfb, GLenum _pname, GLint* _param) { GLAD_INSTRUMENT_CALL(glGetTransformFeedbackiv, _xfb, _pname, _param); }
	#define glGetTransformFeedbackiv glad_wrapped_glGetTransformFeedbackiv
struct glad_tag_glBlitNamedFramebuffer {};
inline void glad_wrapped_glBlitNamedFramebuffer(GLuint _readFramebuffer, GLuint _drawFramebuffer, GLint _srcX0, GLint _srcY0, GLint _srcX1, GLint _srcY1, GLint _dstX0, GLint _dstY0, GLint _dstX1, GLint _dstY1, GLbitfield _mask, GLenum _filter) { GLAD_INSTRUMENT_CALL(glBlitNamedFramebuffer, _readFramebuffer, _drawFramebuffer, _srcX0, _srcY0, _srcX1, _srcY1, _dstX0, _dstY0, _dstX1, _dstY1, _mask, _filter); }
	#define glBlitNamedFramebuffer glad_wrapped_glBlitNamedFramebuffer
struct glad_tag_glVertexArrayAttribLFormat {};
inline void glad_wrapped_glVertexArrayAttribLFormat(GLuint _vaobj, GLuint _attribindex, GLint _size, GLenum _type, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexArrayAttribLFormat, _vaobj, _attribindex, _size, _type, _relativeoffset); }
	#define glVertexArrayAttribLFormat glad_wrapped_glVertexArrayAttribLFormat
struct glad_tag_glCreateQueries {};
inline void glad_wrapped_glCreateQueries(GLenum _target, GLsizei _n, GLuint* _ids) { GLAD_INSTRUMENT_CALL(glCreateQueries, _target, _n, _ids); }
	#define glCreateQueries glad_wrapped_glCreateQueries
struct glad_tag_glGetSamplerParameterfv {};
inline void glad_wrapped_glGetSamplerParameterfv(GLuint _sampler, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetSamplerParameterfv, _sampler, _pname, _params); }
	#define glGetSamplerParameterfv glad_wrapped_glGetSamplerParameterfv
struct glad_tag_glShaderStorageBlockBinding {};
inline void glad_wrapped_glShaderStorageBlockBinding(GLuint _program, GLuint _storageBlockIndex, GLuint _storageBlockBinding) { GLAD_INSTRUMENT_CALL(glShaderStorageBlockBinding, _program, _storageBlockIndex, _storageBlockBinding); }
	#define glShaderStorageBlockBinding glad_wrapped_glShaderStorageBlockBinding
struct glad_tag_glProgramUniformMatrix4x2dv {};
inline void glad_wrapped_glProgramUniformMatrix4x2dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x2dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x2dv glad_wrapped_glProgramUniformMatrix4x2dv
struct glad_tag_glGetUniformIndices {};
inline void glad_wrapped_glGetUniformIndices(GLuint _program, GLsizei _uniformCount, const GLchar** _uniformNames, GLuint* _uniformIndices) { GLAD_INSTRUMENT_CALL(glGetUniformIndices, _program, _uniformCount, _uniformNames, _uniformIndices); }
	#define glGetUniformIndices glad_wrapped_glGetUniformIndices
struct glad_tag_glIsShader {};
inline GLboolean glad_wrapped_glIsShader(GLuint _shader) { return GLAD_INSTRUMENT_CALL(glIsShader, _shader); }
	#define glIsShader glad_wrapped_glIsShader
struct glad_tag_glVertexAttribI4ubv {};
inline void glad_wrapped_glVertexAttribI4ubv(GLuint _index, const GLubyte* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI4ubv, _index, _v); }
	#define glVertexAttribI4ubv glad_wrapped_glVertexAttribI4ubv
struct glad_tag_glBeginQueryIndexed {};
inline void glad_wrapped_glBeginQueryIndexed(GLenum _target, GLuint _index, GLuint _id) { GLAD_INSTRUMENT_CALL(glBeginQueryIndexed, _target, _index, _id); }
	#define glBeginQueryIndexed glad_wrapped_glBeginQueryIndexed
struct glad_tag_glPointParameteriv {};
inline void glad_wrapped_glPointParameteriv(GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glPointParameteriv, _pname, _params); }
	#define glPointParameteriv glad_wrapped_glPointParameteriv
struct glad_tag_glEnable {};
inline void glad_wrapped_glEnable(GLenum _cap) { GLAD_INSTRUMENT_CALL(glEnable, _cap); }
	#define glEnable glad_wrapped_glEnable
struct glad_tag_glGetActiveUniformsiv {};
inline void glad_wrapped_glGetActiveUniformsiv(GLuint _program, GLsizei _uniformCount, const GLuint* _uniformIndices, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetActiveUniformsiv, _program, _uniformCount, _uniformIndices, _pname, _params); }
	#define glGetActiveUniformsiv glad_wrapped_glGetActiveUniformsiv
struct glad_tag_glVertexArrayAttribBinding {};
inline void glad_wrapped_glVertexArrayAttribBinding(GLuint _vaobj, GLuint _attribindex, GLuint _bindingindex) { GLAD_INSTRUMENT_CALL(glVertexArrayAttribBinding, _vaobj, _attribindex, _bindingindex); }
	#define glVertexArrayAttribBinding glad_wrapped_glVertexArrayAttribBinding
struct glad_tag_glTextureStorage1D {};
inline void glad_wrapped_glTextureStorage1D(GLuint _texture, GLsizei _levels, GLenum _internalformat, GLsizei _width) { GLAD_INSTRUMENT_CALL(glTextureStorage1D, _texture, _levels, _internalformat, _width); }
	#define glTextureStorage1D glad_wrapped_glTextureStorage1D
struct glad_tag_glMemoryBarrierByRegion {};
inline void glad_wrapped_glMemoryBarrierByRegion(GLbitfield _barriers) { GLAD_INSTRUMENT_CALL(glMemoryBarrierByRegion, _barriers); }
	#define glMemoryBarrierByRegion glad_wrapped_glMemoryBarrierByRegion
struct glad_tag_glBlendEquationi {};
inline void glad_wrapped_glBlendEquationi(GLuint _buf, GLenum _mode) { GLAD_INSTRUMENT_CALL(glBlendEquationi, _buf, _mode); }
	#define glBlendEquationi glad_wrapped_glBlendEquationi
struct glad_tag_glGetAttribLocation {};
inline GLint glad_wrapped_glGetAttribLocation(GLuint _program, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetAttribLocation, _program, _name); }
	#define glGetAttribLocation glad_wrapped_glGetAttribLocation
struct glad_tag_glVertexAttrib4dv {};
inline void glad_wrapped_glVertexAttrib4dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4dv, _index, _v); }
	#define glVertexAttrib4dv glad_wrapped_glVertexAttrib4dv
struct glad_tag_glGetTextureParameteriv {};
inline void glad_wrapped_glGetTextureParameteriv(GLuint _texture, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameteriv, _texture, _pname, _params); }
	#define glGetTextureParameteriv glad_wrapped_glGetTextureParameteriv
struct glad_tag_glGetProgramInterfaceiv {};
inline void glad_wrapped_glGetProgramInterfaceiv(GLuint _program, GLenum _programInterface, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetProgramInterfaceiv, _program, _programInterface, _pname, _params); }
	#define glGetProgramInterfaceiv glad_wrapped_glGetProgramInterfaceiv
struct glad_tag_glUniform2dv {};
inline void glad_wrapped_glUniform2dv(GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniform2dv, _location, _count, _value); }
	#define glUniform2dv glad_wrapped_glUniform2dv
struct glad_tag_glMapNamedBuffer {};
inline void* glad_wrapped_glMapNamedBuffer(GLuint _buffer, GLenum _access) { return GLAD_INSTRUMENT_CALL(glMapNamedBuffer, _buffer, _access); }
	#define glMapNamedBuffer glad_wrapped_glMapNamedBuffer
struct glad_tag_glMultiTexCoordP3uiv {};
inline void glad_wrapped_glMultiTexCoordP3uiv(GLenum _texture, GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP3uiv, _texture, _type, _coords); }
	#define glMultiTexCoordP3uiv glad_wrapped_glMultiTexCoordP3uiv
struct glad_tag_glVertexAttribP3ui {};
inline void glad_wrapped_glVertexAttribP3ui(GLuint _index, GLenum _type, GLboolean _normalized, GLuint _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP3ui, _index, _type, _normalized, _value); }
	#define glVertexAttribP3ui glad_wrapped_glVertexAttribP3ui
struct glad_tag_glVertexAttribL1dv {};
inline void glad_wrapped_glVertexAttribL1dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribL1dv, _index, _v); }
	#define glVertexAttribL1dv glad_wrapped_glVertexAttribL1dv
struct glad_tag_glTextureBufferRange {};
inline void glad_wrapped_glTextureBufferRange(GLuint _texture, GLenum _internalformat, GLuint _buffer, GLintptr _offset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glTextureBufferRange, _texture, _internalformat, _buffer, _offset, _size); }
	#define glTextureBufferRange glad_wrapped_glTextureBufferRange
struct glad_tag_glGetnUniformdv {};
inline void glad_wrapped_glGetnUniformdv(GLuint _program, GLint _location, GLsizei _bufSize, GLdouble* _params) { GLAD_INSTRUMENT_CALL(glGetnUniformdv, _program, _location, _bufSize, _params); }
	#define glGetnUniformdv glad_wrapped_glGetnUniformdv
struct glad_tag_glProgramUniform3ui {};
inline void glad_wrapped_glProgramUniform3ui(GLuint _program, GLint _location, GLuint _v0, GLuint _v1, GLuint _v2) { GLAD_INSTRUMENT_CALL(glProgramUniform3ui, _program, _location, _v0, _v1, _v2); }
	#define glProgramUniform3ui glad_wrapped_glProgramUniform3ui
struct glad_tag_glVertexBindingDivisor {};
inline void glad_wrapped_glVertexBindingDivisor(GLuint _bindingindex, GLuint _divisor) { GLAD_INSTRUMENT_CALL(glVertexBindingDivisor, _bindingindex, _divisor); }
	#define glVertexBindingDivisor glad_wrapped_glVertexBindingDivisor
struct glad_tag_glGetUniformfv {};
inline void glad_wrapped_glGetUniformfv(GLuint _program, GLint _location, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetUniformfv, _program, _location, _params); }
	#define glGetUniformfv glad_wrapped_glGetUniformfv
struct glad_tag_glGetUniformuiv {};
inline void glad_wrapped_glGetUniformuiv(GLuint _program, GLint _location, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetUniformuiv, _program, _location, _params); }
	#define glGetUniformuiv glad_wrapped_glGetUniformuiv
struct glad_tag_glProgramUniformMatrix2x3fv {};
inline void glad_wrapped_glProgramUniformMatrix2x3fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x3fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x3fv glad_wrapped_glProgramUniformMatrix2x3fv
struct glad_tag_glGetVertexAttribIiv {};
inline void glad_wrapped_glGetVertexAttribIiv(GLuint _index, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetVertexAttribIiv, _index, _pname, _params); }
	#define glGetVertexAttribIiv glad_wrapped_glGetVertexAttribIiv
struct glad_tag_glVertexArrayBindingDivisor {};
inline void glad_wrapped_glVertexArrayBindingDivisor(GLuint _vaobj, GLuint _bindingindex, GLuint _divisor) { GLAD_INSTRUMENT_CALL(glVertexArrayBindingDivisor, _vaobj, _bindingindex, _divisor); }
	#define glVertexArrayBindingDivisor glad_wrapped_glVertexArrayBindingDivisor
struct glad_tag_glDrawBuffer {};
inline void glad_wrapped_glDrawBuffer(GLenum _buf) { GLAD_INSTRUMENT_CALL(glDrawBuffer, _buf); }
	#define glDrawBuffer glad_wrapped_glDrawBuffer
struct glad_tag_glEndQueryIndexed {};
inline void glad_wrapped_glEndQueryIndexed(GLenum _target, GLuint _index) { GLAD_INSTRUMENT_CALL(glEndQueryIndexed, _target, _index); }
	#define glEndQueryIndexed glad_wrapped_glEndQueryIndexed
struct glad_tag_glGetnPixelMapusv {};
inline void glad_wrapped_glGetnPixelMapusv(GLenum _map, GLsizei _bufSize, GLushort* _values) { GLAD_INSTRUMENT_CALL(glGetnPixelMapusv, _map, _bufSize, _values); }
	#define glGetnPixelMapusv glad_wrapped_glGetnPixelMapusv
struct glad_tag_glClearBufferuiv {};
inline void glad_wrapped_glClearBufferuiv(GLenum _buffer, GLint _drawbuffer, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glClearBufferuiv, _buffer, _drawbuffer, _value); }
	#define glClearBufferuiv glad_wrapped_glClearBufferuiv
struct glad_tag_glDrawElementsInstanced {};
inline void glad_wrapped_glDrawElementsInstanced(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices, GLsizei _instancecount) { GLAD_INSTRUMENT_CALL(glDrawElementsInstanced, _mode, _count, _type, _indices, _instancecount); }
	#define glDrawElementsInstanced glad_wrapped_glDrawElementsInstanced
struct glad_tag_glProgramUniform1i {};
inline void glad_wrapped_glProgramUniform1i(GLuint _program, GLint _location, GLint _v0) { GLAD_INSTRUMENT_CALL(glProgramUniform1i, _program, _location, _v0); }
	#define glProgramUniform1i glad_wrapped_glProgramUniform1i
struct glad_tag_glPatchParameteri {};
inline void glad_wrapped_glPatchParameteri(GLenum _pname, GLint _value) { GLAD_INSTRUMENT_CALL(glPatchParameteri, _pname, _value); }
	#define glPatchParameteri glad_wrapped_glPatchParameteri
struct glad_tag_glProgramUniform1d {};
inline void glad_wrapped_glProgramUniform1d(GLuint _program, GLint _location, GLdouble _v0) { GLAD_INSTRUMENT_CALL(glProgramUniform1d, _program, _location, _v0); }
	#define glProgramUniform1d glad_wrapped_glProgramUniform1d
struct glad_tag_glProgramUniform1f {};
inline void glad_wrapped_glProgramUniform1f(GLuint _program, GLint _location, GLfloat _v0) { GLAD_INSTRUMENT_CALL(glProgramUniform1f, _program, _location, _v0); }
	#define glProgramUniform1f glad_wrapped_glProgramUniform1f
struct glad_tag_glGetNamedFramebufferParameteriv {};
inline void glad_wrapped_glGetNamedFramebufferParameteriv(GLuint _framebuffer, GLenum _pname, GLint* _param) { GLAD_INSTRUMENT_CALL(glGetNamedFramebufferParameteriv, _framebuffer, _pname, _param); }
	#define glGetNamedFramebufferParameteriv glad_wrapped_glGetNamedFramebufferParameteriv
struct glad_tag_glFlush {};
inline void glad_wrapped_glFlush() { GLAD_INSTRUMENT_CALL(glFlush); }
	#define glFlush glad_wrapped_glFlush
struct glad_tag_glGetRenderbufferParameteriv {};
inline void glad_wrapped_glGetRenderbufferParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetRenderbufferParameteriv, _target, _pname, _params); }
	#define glGetRenderbufferParameteriv glad_wrapped_glGetRenderbufferParameteriv
struct glad_tag_glProgramUniform3iv {};
inline void glad_wrapped_glProgramUniform3iv(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3iv, _program, _location, _count, _value); }
	#define glProgramUniform3iv glad_wrapped_glProgramUniform3iv
struct glad_tag_glGetDebugMessageLog {};
inline GLuint glad_wrapped_glGetDebugMessageLog(GLuint _count, GLsizei _bufSize, GLenum* _sources, GLenum* _types, GLuint* _ids, GLenum* _severities, GLsizei* _lengths, GLchar* _messageLog) { return GLAD_INSTRUMENT_CALL(glGetDebugMessageLog, _count, _bufSize, _sources, _types, _ids, _severities, _lengths, _messageLog); }
	#define glGetDebugMessageLog glad_wrapped_glGetDebugMessageLog
struct glad_tag_glNamedRenderbufferStorage {};
inline void glad_wrapped_glNamedRenderbufferStorage(GLuint _renderbuffer, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glNamedRenderbufferStorage, _renderbuffer, _internalformat, _width, _height); }
	#define glNamedRenderbufferStorage glad_wrapped_glNamedRenderbufferStorage
struct glad_tag_glGetNamedFramebufferAttachmentParameteriv {};
inline void glad_wrapped_glGetNamedFramebufferAttachmentParameteriv(GLuint _framebuffer, GLenum _attachment, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedFramebufferAttachmentParameteriv, _framebuffer, _attachment, _pname, _params); }
	#define glGetNamedFramebufferAttachmentParameteriv glad_wrapped_glGetNamedFramebufferAttachmentParameteriv
struct glad_tag_glGetVertexAttribPointerv {};
inline void glad_wrapped_glGetVertexAttribPointerv(GLuint _index, GLenum _pname, void** _pointer) { GLAD_INSTRUMENT_CALL(glGetVertexAttribPointerv, _index, _pname, _pointer); }
	#define glGetVertexAttribPointerv glad_wrapped_glGetVertexAttribPointerv
struct glad_tag_glFenceSync {};
inline GLsync glad_wrapped_glFenceSync(GLenum _condition, GLbitfield _flags) { return GLAD_INSTRUMENT_CALL(glFenceSync, _condition, _flags); }
	#define glFenceSync glad_wrapped_glFenceSync
struct glad_tag_glColorP3ui {};
inline void glad_wrapped_glColorP3ui(GLenum _type, GLuint _color) { GLAD_INSTRUMENT_CALL(glColorP3ui, _type, _color); }
	#define glColorP3ui glad_wrapped_glColorP3ui
struct glad_tag_glDrawElementsInstancedBaseVertexBaseInstance {};
inline void glad_wrapped_glDrawElementsInstancedBaseVertexBaseInstance(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices, GLsizei _instancecount, GLint _basevertex, GLuint _baseinstance) { GLAD_INSTRUMENT_CALL(glDrawElementsInstancedBaseVertexBaseInstance, _mode, _count, _type, _indices, _instancecount, _basevertex, _baseinstance); }
	#define glDrawElementsInstancedBaseVertexBaseInstance glad_wrapped_glDrawElementsInstancedBaseVertexBaseInstance
struct glad_tag_glVertexAttrib3sv {};
inline void glad_wrapped_glVertexAttrib3sv(GLuint _index, const GLshort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib3sv, _index, _v); }
	#define glVertexAttrib3sv glad_wrapped_glVertexAttrib3sv
struct glad_tag_glBeginConditionalRender {};
inline void glad_wrapped_glBeginConditionalRender(GLuint _id, GLenum _mode) { GLAD_INSTRUMENT_CALL(glBeginConditionalRender, _id, _mode); }
	#define glBeginConditionalRender glad_wrapped_glBeginConditionalRender
struct glad_tag_glValidateProgramPipeline {};
inline void glad_wrapped_glValidateProgramPipeline(GLuint _pipeline) { GLAD_INSTRUMENT_CALL(glValidateProgramPipeline, _pipeline); }
	#define glValidateProgramPipeline glad_wrapped_glValidateProgramPipeline
struct glad_tag_glGetnMinmax {};
inline void glad_wrapped_glGetnMinmax(GLenum _target, GLboolean _reset, GLenum _format, GLenum _type, GLsizei _bufSize, void* _values) { GLAD_INSTRUMENT_CALL(glGetnMinmax, _target, _reset, _format, _type, _bufSize, _values); }
	#define glGetnMinmax glad_wrapped_glGetnMinmax
struct glad_tag_glGetTexLevelParameteriv {};
inline void glad_wrapped_glGetTexLevelParameteriv(GLenum _target, GLint _level, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTexLevelParameteriv, _target, _level, _pname, _params); }
	#define glGetTexLevelParameteriv glad_wrapped_glGetTexLevelParameteriv
struct glad_tag_glMultiTexCoordP4uiv {};
inline void glad_wrapped_glMultiTexCoordP4uiv(GLenum _texture, GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP4uiv, _texture, _type, _coords); }
	#define glMultiTexCoordP4uiv glad_wrapped_glMultiTexCoordP4uiv
struct glad_tag_glTexStorage3DMultisample {};
inline void glad_wrapped_glTexStorage3DMultisample(GLenum _target, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTexStorage3DMultisample, _target, _samples, _internalformat, _width, _height, _depth, _fixedsamplelocations); }
	#define glTexStorage3DMultisample glad_wrapped_glTexStorage3DMultisample
struct glad_tag_glStencilFuncSeparate {};
inline void glad_wrapped_glStencilFuncSeparate(GLenum _face, GLenum _func, GLint _ref, GLuint _mask) { GLAD_INSTRUMENT_CALL(glStencilFuncSeparate, _face, _func, _ref, _mask); }
	#define glStencilFuncSeparate glad_wrapped_glStencilFuncSeparate
struct glad_tag_glDisableVertexArrayAttrib {};
inline void glad_wrapped_glDisableVertexArrayAttrib(GLuint _vaobj, GLuint _index) { GLAD_INSTRUMENT_CALL(glDisableVertexArrayAttrib, _vaobj, _index); }
	#define glDisableVertexArrayAttrib glad_wrapped_glDisableVertexArrayAttrib
struct glad_tag_glGenSamplers {};
inline void glad_wrapped_glGenSamplers(GLsizei _count, GLuint* _samplers) { GLAD_INSTRUMENT_CALL(glGenSamplers, _count, _samplers); }
	#define glGenSamplers glad_wrapped_glGenSamplers
struct glad_tag_glClampColor {};
inline void glad_wrapped_glClampColor(GLenum _target, GLenum _clamp) { GLAD_INSTRUMENT_CALL(glClampColor, _target, _clamp); }
	#define glClampColor glad_wrapped_glClampColor
struct glad_tag_glUniform4iv {};
inline void glad_wrapped_glUniform4iv(GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glUniform4iv, _location, _count, _value); }
	#define glUniform4iv glad_wrapped_glUniform4iv
struct glad_tag_glClearStencil {};
inline void glad_wrapped_glClearStencil(GLint _s) { GLAD_INSTRUMENT_CALL(glClearStencil, _s); }
	#define glClearStencil glad_wrapped_glClearStencil
struct glad_tag_glTexCoordP1uiv {};
inline void glad_wrapped_glTexCoordP1uiv(GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP1uiv, _type, _coords); }
	#define glTexCoordP1uiv glad_wrapped_glTexCoordP1uiv
struct glad_tag_glGetNamedRenderbufferParameteriv {};
inline void glad_wrapped_glGetNamedRenderbufferParameteriv(GLuint _renderbuffer, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedRenderbufferParameteriv, _renderbuffer, _pname, _params); }
	#define glGetNamedRenderbufferParameteriv glad_wrapped_glGetNamedRenderbufferParameteriv
struct glad_tag_glDrawTransformFeedbackInstanced {};
inline void glad_wrapped_glDrawTransformFeedbackInstanced(GLenum _mode, GLuint _id, GLsizei _instancecount) { GLAD_INSTRUMENT_CALL(glDrawTransformFeedbackInstanced, _mode, _id, _instancecount); }
	#define glDrawTransformFeedbackInstanced glad_wrapped_glDrawTransformFeedbackInstanced
struct glad_tag_glGenTextures {};
inline void glad_wrapped_glGenTextures(GLsizei _n, GLuint* _textures) { GLAD_INSTRUMENT_CALL(glGenTextures, _n, _textures); }
	#define glGenTextures glad_wrapped_glGenTextures
struct glad_tag_glTextureStorage2DMultisample {};
inline void glad_wrapped_glTextureStorage2DMultisample(GLuint _texture, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTextureStorage2DMultisample, _texture, _samples, _internalformat, _width, _height, _fixedsamplelocations); }
	#define glTextureStorage2DMultisample glad_wrapped_glTextureStorage2DMultisample
struct glad_tag_glDrawTransformFeedback {};
inline void glad_wrapped_glDrawTransformFeedback(GLenum _mode, GLuint _id) { GLAD_INSTRUMENT_CALL(glDrawTransformFeedback, _mode, _id); }
	#define glDrawTransformFeedback glad_wrapped_glDrawTransformFeedback
struct glad_tag_glUniform1dv {};
inline void glad_wrapped_glUniform1dv(GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniform1dv, _location, _count, _value); }
	#define glUniform1dv glad_wrapped_glUniform1dv
struct glad_tag_glGetTexParameterIuiv {};
inline void glad_wrapped_glGetTexParameterIuiv(GLenum _target, GLenum _pname, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetTexParameterIuiv, _target, _pname, _params); }
	#define glGetTexParameterIuiv glad_wrapped_glGetTexParameterIuiv
struct glad_tag_glGetTransformFeedbacki_v {};
inline void glad_wrapped_glGetTransformFeedbacki_v(GLuint _xfb, GLenum _pname, GLuint _index, GLint* _param) { GLAD_INSTRUMENT_CALL(glGetTransformFeedbacki_v, _xfb, _pname, _index, _param); }
	#define glGetTransformFeedbacki_v glad_wrapped_glGetTransformFeedbacki_v
struct glad_tag_glVertexAttrib4Nbv {};
inline void glad_wrapped_glVertexAttrib4Nbv(GLuint _index, const GLbyte* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4Nbv, _index, _v); }
	#define glVertexAttrib4Nbv glad_wrapped_glVertexAttrib4Nbv
struct glad_tag_glClearNamedFramebufferuiv {};
inline void glad_wrapped_glClearNamedFramebufferuiv(GLuint _framebuffer, GLenum _buffer, GLint _drawbuffer, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glClearNamedFramebufferuiv, _framebuffer, _buffer, _drawbuffer, _value); }
	#define glClearNamedFramebufferuiv glad_wrapped_glClearNamedFramebufferuiv
struct glad_tag_glIsSync {};
inline GLboolean glad_wrapped_glIsSync(GLsync _sync) { return GLAD_INSTRUMENT_CALL(glIsSync, _sync); }
	#define glIsSync glad_wrapped_glIsSync
struct glad_tag_glClearNamedFramebufferiv {};
inline void glad_wrapped_glClearNamedFramebufferiv(GLuint _framebuffer, GLenum _buffer, GLint _drawbuffer, const GLint* _value) { GLAD_INSTRUMENT_CALL(glClearNamedFramebufferiv, _framebuffer, _buffer, _drawbuffer, _value); }
	#define glClearNamedFramebufferiv glad_wrapped_glClearNamedFramebufferiv
struct glad_tag_glGetActiveUniformBlockName {};
inline void glad_wrapped_glGetActiveUniformBlockName(GLuint _program, GLuint _uniformBlockIndex, GLsizei _bufSize, GLsizei* _length, GLchar* _uniformBlockName) { GLAD_INSTRUMENT_CALL(glGetActiveUniformBlockName, _program, _uniformBlockIndex, _bufSize, _length, _uniformBlockName); }
	#define glGetActiveUniformBlockName glad_wrapped_glGetActiveUniformBlockName
struct glad_tag_glUniform2i {};
inline void glad_wrapped_glUniform2i(GLint _location, GLint _v0, GLint _v1) { GLAD_INSTRUMENT_CALL(glUniform2i, _location, _v0, _v1); }
	#define glUniform2i glad_wrapped_glUniform2i
struct glad_tag_glUniform2f {};
inline void glad_wrapped_glUniform2f(GLint _location, GLfloat _v0, GLfloat _v1) { GLAD_INSTRUMENT_CALL(glUniform2f, _location, _v0, _v1); }
	#define glUniform2f glad_wrapped_glUniform2f
struct glad_tag_glUniform2d {};
inline void glad_wrapped_glUniform2d(GLint _location, GLdouble _x, GLdouble _y) { GLAD_INSTRUMENT_CALL(glUniform2d, _location, _x, _y); }
	#define glUniform2d glad_wrapped_glUniform2d
struct glad_tag_glTexCoordP4ui {};
inline void glad_wrapped_glTexCoordP4ui(GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP4ui, _type, _coords); }
	#define glTexCoordP4ui glad_wrapped_glTexCoordP4ui
struct glad_tag_glGetProgramiv {};
inline void glad_wrapped_glGetProgramiv(GLuint _program, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetProgramiv, _program, _pname, _params); }
	#define glGetProgramiv glad_wrapped_glGetProgramiv
struct glad_tag_glVertexAttribPointer {};
inline void glad_wrapped_glVertexAttribPointer(GLuint _index, GLint _size, GLenum _type, GLboolean _normalized, GLsizei _stride, const void* _pointer) { GLAD_INSTRUMENT_CALL(glVertexAttribPointer, _index, _size, _type, _normalized, _stride, _pointer); }
	#define glVertexAttribPointer glad_wrapped_glVertexAttribPointer
struct glad_tag_glFramebufferTextureLayer {};
inline void glad_wrapped_glFramebufferTextureLayer(GLenum _target, GLenum _attachment, GLuint _texture, GLint _level, GLint _layer) { GLAD_INSTRUMENT_CALL(glFramebufferTextureLayer, _target, _attachment, _texture, _level, _layer); }
	#define glFramebufferTextureLayer glad_wrapped_glFramebufferTextureLayer
struct glad_tag_glProgramUniform4fv {};
inline void glad_wrapped_glProgramUniform4fv(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4fv, _program, _location, _count, _value); }
	#define glProgramUniform4fv glad_wrapped_glProgramUniform4fv
struct glad_tag_glGetObjectPtrLabel {};
inline void glad_wrapped_glGetObjectPtrLabel(const void* _ptr, GLsizei _bufSize, GLsizei* _length, GLchar* _label) { GLAD_INSTRUMENT_CALL(glGetObjectPtrLabel, _ptr, _bufSize, _length, _label); }
	#define glGetObjectPtrLabel glad_wrapped_glGetObjectPtrLabel
struct glad_tag_glTextureParameteri {};
inline void glad_wrapped_glTextureParameteri(GLuint _texture, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glTextureParameteri, _texture, _pname, _param); }
	#define glTextureParameteri glad_wrapped_glTextureParameteri
struct glad_tag_glTextureParameterf {};
inline void glad_wrapped_glTextureParameterf(GLuint _texture, GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glTextureParameterf, _texture, _pname, _param); }
	#define glTextureParameterf glad_wrapped_glTextureParameterf
struct glad_tag_glFlushMappedBufferRange {};
inline void glad_wrapped_glFlushMappedBufferRange(GLenum _target, GLintptr _offset, GLsizeiptr _length) { GLAD_INSTRUMENT_CALL(glFlushMappedBufferRange, _target, _offset, _length); }
	#define glFlushMappedBufferRange glad_wrapped_glFlushMappedBufferRange
struct glad_tag_glProgramUniform2fv {};
inline void glad_wrapped_glProgramUniform2fv(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2fv, _program, _location, _count, _value); }
	#define glProgramUniform2fv glad_wrapped_glProgramUniform2fv
struct glad_tag_glUniformMatrix2x3dv {};
inline void glad_wrapped_glUniformMatrix2x3dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix2x3dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix2x3dv glad_wrapped_glUniformMatrix2x3dv
struct glad_tag_glProgramUniformMatrix4dv {};
inline void glad_wrapped_glProgramUniformMatrix4dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4dv glad_wrapped_glProgramUniformMatrix4dv
struct glad_tag_glVertexAttribL3d {};
inline void glad_wrapped_glVertexAttribL3d(GLuint _index, GLdouble _x, GLdouble _y, GLdouble _z) { GLAD_INSTRUMENT_CALL(glVertexAttribL3d, _index, _x, _y, _z); }
	#define glVertexAttribL3d glad_wrapped_glVertexAttribL3d
struct glad_tag_glProgramUniformMatrix2x4dv {};
inline void glad_wrapped_glProgramUniformMatrix2x4dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x4dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x4dv glad_wrapped_glProgramUniformMatrix2x4dv
struct glad_tag_glDispatchCompute {};
inline void glad_wrapped_glDispatchCompute(GLuint _num_groups_x, GLuint _num_groups_y, GLuint _num_groups_z) { GLAD_INSTRUMENT_CALL(glDispatchCompute, _num_groups_x, _num_groups_y, _num_groups_z); }
	#define glDispatchCompute glad_wrapped_glDispatchCompute
struct glad_tag_glGenQueries {};
inline void glad_wrapped_glGenQueries(GLsizei _n, GLuint* _ids) { GLAD_INSTRUMENT_CALL(glGenQueries, _n, _ids); }
	#define glGenQueries glad_wrapped_glGenQueries
struct glad_tag_glVertexAttribP1ui {};
inline void glad_wrapped_glVertexAttribP1ui(GLuint _index, GLenum _type, GLboolean _normalized, GLuint _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP1ui, _index, _type, _normalized, _value); }
	#define glVertexAttribP1ui glad_wrapped_glVertexAttribP1ui
struct glad_tag_glTexSubImage3D {};
inline void glad_wrapped_glTexSubImage3D(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTexSubImage3D, _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _type, _pixels); }
	#define glTexSubImage3D glad_wrapped_glTexSubImage3D
struct glad_tag_glGetInteger64i_v {};
inline void glad_wrapped_glGetInteger64i_v(GLenum _target, GLuint _index, GLint64* _data) { GLAD_INSTRUMENT_CALL(glGetInteger64i_v, _target, _index, _data); }
	#define glGetInteger64i_v glad_wrapped_glGetInteger64i_v
struct glad_tag_glDeleteSamplers {};
inline void glad_wrapped_glDeleteSamplers(GLsizei _count, const GLuint* _samplers) { GLAD_INSTRUMENT_CALL(glDeleteSamplers, _count, _samplers); }
	#define glDeleteSamplers glad_wrapped_glDeleteSamplers
struct glad_tag_glCopyTexImage2D {};
inline void glad_wrapped_glCopyTexImage2D(GLenum _target, GLint _level, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width, GLsizei _height, GLint _border) { GLAD_INSTRUMENT_CALL(glCopyTexImage2D, _target, _level, _internalformat, _x, _y, _width, _height, _border); }
	#define glCopyTexImage2D glad_wrapped_glCopyTexImage2D
struct glad_tag_glGetTextureSubImage {};
inline void glad_wrapped_glGetTextureSubImage(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLenum _type, GLsizei _bufSize, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetTextureSubImage, _texture, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _type, _bufSize, _pixels); }
	#define glGetTextureSubImage glad_wrapped_glGetTextureSubImage
struct glad_tag_glBlitFramebuffer {};
inline void glad_wrapped_glBlitFramebuffer(GLint _srcX0, GLint _srcY0, GLint _srcX1, GLint _srcY1, GLint _dstX0, GLint _dstY0, GLint _dstX1, GLint _dstY1, GLbitfield _mask, GLenum _filter) { GLAD_INSTRUMENT_CALL(glBlitFramebuffer, _srcX0, _srcY0, _srcX1, _srcY1, _dstX0, _dstY0, _dstX1, _dstY1, _mask, _filter); }
	#define glBlitFramebuffer glad_wrapped_glBlitFramebuffer
struct glad_tag_glIsEnabledi {};
inline GLboolean glad_wrapped_glIsEnabledi(GLenum _target, GLuint _index) { return GLAD_INSTRUMENT_CALL(glIsEnabledi, _target, _index); }
	#define glIsEnabledi glad_wrapped_glIsEnabledi
struct glad_tag_glBindBuffersRange {};
inline void glad_wrapped_glBindBuffersRange(GLenum _target, GLuint _first, GLsizei _count, const GLuint* _buffers, const GLintptr* _offsets, const GLsizeiptr* _sizes) { GLAD_INSTRUMENT_CALL(glBindBuffersRange, _target, _first, _count, _buffers, _offsets, _sizes); }
	#define glBindBuffersRange glad_wrapped_glBindBuffersRange
struct glad_tag_glSecondaryColorP3ui {};
inline void glad_wrapped_glSecondaryColorP3ui(GLenum _type, GLuint _color) { GLAD_INSTRUMENT_CALL(glSecondaryColorP3ui, _type, _color); }
	#define glSecondaryColorP3ui glad_wrapped_glSecondaryColorP3ui
struct glad_tag_glBindFragDataLocationIndexed {};
inline void glad_wrapped_glBindFragDataLocationIndexed(GLuint _program, GLuint _colorNumber, GLuint _index, const GLchar* _name) { GLAD_INSTRUMENT_CALL(glBindFragDataLocationIndexed, _program, _colorNumber, _index, _name); }
	#define glBindFragDataLocationIndexed glad_wrapped_glBindFragDataLocationIndexed
struct glad_tag_glCopyImageSubData {};
inline void glad_wrapped_glCopyImageSubData(GLuint _srcName, GLenum _srcTarget, GLint _srcLevel, GLint _srcX, GLint _srcY, GLint _srcZ, GLuint _dstName, GLenum _dstTarget, GLint _dstLevel, GLint _dstX, GLint _dstY, GLint _dstZ, GLsizei _srcWidth, GLsizei _srcHeight, GLsizei _srcDepth) { GLAD_INSTRUMENT_CALL(glCopyImageSubData, _srcName, _srcTarget, _srcLevel, _srcX, _srcY, _srcZ, _dstName, _dstTarget, _dstLevel, _dstX, _dstY, _dstZ, _srcWidth, _srcHeight, _srcDepth); }
	#define glCopyImageSubData glad_wrapped_glCopyImageSubData
struct glad_tag_glUniform2iv {};
inline void glad_wrapped_glUniform2iv(GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glUniform2iv, _location, _count, _value); }
	#define glUniform2iv glad_wrapped_glUniform2iv
struct glad_tag_glVertexAttrib1fv {};
inline void glad_wrapped_glVertexAttrib1fv(GLuint _index, const GLfloat* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib1fv, _index, _v); }
	#define glVertexAttrib1fv glad_wrapped_glVertexAttrib1fv
struct glad_tag_glUniform4uiv {};
inline void glad_wrapped_glUniform4uiv(GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glUniform4uiv, _location, _count, _value); }
	#define glUniform4uiv glad_wrapped_glUniform4uiv
struct glad_tag_glProgramUniform2dv {};
inline void glad_wrapped_glProgramUniform2dv(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2dv, _program, _location, _count, _value); }
	#define glProgramUniform2dv glad_wrapped_glProgramUniform2dv
struct glad_tag_glTextureSubImage3D {};
inline void glad_wrapped_glTextureSubImage3D(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureSubImage3D, _texture, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _type, _pixels); }
	#define glTextureSubImage3D glad_wrapped_glTextureSubImage3D
struct glad_tag_glFramebufferTexture1D {};
inline void glad_wrapped_glFramebufferTexture1D(GLenum _target, GLenum _attachment, GLenum _textarget, GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glFramebufferTexture1D, _target, _attachment, _textarget, _texture, _level); }
	#define glFramebufferTexture1D glad_wrapped_glFramebufferTexture1D
struct glad_tag_glGetShaderiv {};
inline void glad_wrapped_glGetShaderiv(GLuint _shader, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetShaderiv, _shader, _pname, _params); }
	#define glGetShaderiv glad_wrapped_glGetShaderiv
struct glad_tag_glProgramUniformMatrix3fv {};
inline void glad_wrapped_glProgramUniformMatrix3fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3fv glad_wrapped_glProgramUniformMatrix3fv
struct glad_tag_glObjectPtrLabel {};
inline void glad_wrapped_glObjectPtrLabel(const void* _ptr, GLsizei _length, const GLchar* _label) { GLAD_INSTRUMENT_CALL(glObjectPtrLabel, _ptr, _length, _label); }
	#define glObjectPtrLabel glad_wrapped_glObjectPtrLabel
struct glad_tag_glInvalidateFramebuffer {};
inline void glad_wrapped_glInvalidateFramebuffer(GLenum _target, GLsizei _numAttachments, const GLenum* _attachments) { GLAD_INSTRUMENT_CALL(glInvalidateFramebuffer, _target, _numAttachments, _attachments); }
	#define glInvalidateFramebuffer glad_wrapped_glInvalidateFramebuffer
struct glad_tag_glBindTextures {};
inline void glad_wrapped_glBindTextures(GLuint _first, GLsizei _count, const GLuint* _textures) { GLAD_INSTRUMENT_CALL(glBindTextures, _first, _count, _textures); }
	#define glBindTextures glad_wrapped_glBindTextures
struct glad_tag_glBindFragDataLocation {};
inline void glad_wrapped_glBindFragDataLocation(GLuint _program, GLuint _color, const GLchar* _name) { GLAD_INSTRUMENT_CALL(glBindFragDataLocation, _program, _color, _name); }
	#define glBindFragDataLocation glad_wrapped_glBindFragDataLocation
struct glad_tag_glNamedBufferStorage {};
inline void glad_wrapped_glNamedBufferStorage(GLuint _buffer, GLsizeiptr _size, const void* _data, GLbitfield _flags) { GLAD_INSTRUMENT_CALL(glNamedBufferStorage, _buffer, _size, _data, _flags); }
	#define glNamedBufferStorage glad_wrapped_glNamedBufferStorage
struct glad_tag_glScissorArrayv {};
inline void glad_wrapped_glScissorArrayv(GLuint _first, GLsizei _count, const GLint* _v) { GLAD_INSTRUMENT_CALL(glScissorArrayv, _first, _count, _v); }
	#define glScissorArrayv glad_wrapped_glScissorArrayv
struct glad_tag_glPolygonOffset {};
inline void glad_wrapped_glPolygonOffset(GLfloat _factor, GLfloat _units) { GLAD_INSTRUMENT_CALL(glPolygonOffset, _factor, _units); }
	#define glPolygonOffset glad_wrapped_glPolygonOffset
struct glad_tag_glGetDoublev {};
inline void glad_wrapped_glGetDoublev(GLenum _pname, GLdouble* _data) { GLAD_INSTRUMENT_CALL(glGetDoublev, _pname, _data); }
	#define glGetDoublev glad_wrapped_glGetDoublev
struct glad_tag_glVertexAttrib1d {};
inline void glad_wrapped_glVertexAttrib1d(GLuint _index, GLdouble _x) { GLAD_INSTRUMENT_CALL(glVertexAttrib1d, _index, _x); }
	#define glVertexAttrib1d glad_wrapped_glVertexAttrib1d
struct glad_tag_glUniform4dv {};
inline void glad_wrapped_glUniform4dv(GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniform4dv, _location, _count, _value); }
	#define glUniform4dv glad_wrapped_glUniform4dv
struct glad_tag_glProgramUniform3dv {};
inline void glad_wrapped_glProgramUniform3dv(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3dv, _program, _location, _count, _value); }
	#define glProgramUniform3dv glad_wrapped_glProgramUniform3dv
struct glad_tag_glGetUniformiv {};
inline void glad_wrapped_glGetUniformiv(GLuint _program, GLint _location, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetUniformiv, _program, _location, _params); }
	#define glGetUniformiv glad_wrapped_glGetUniformiv
struct glad_tag_glInvalidateBufferData {};
inline void glad_wrapped_glInvalidateBufferData(GLuint _buffer) { GLAD_INSTRUMENT_CALL(glInvalidateBufferData, _buffer); }
	#define glInvalidateBufferData glad_wrapped_glInvalidateBufferData
struct glad_tag_glGetnColorTable {};
inline void glad_wrapped_glGetnColorTable(GLenum _target, GLenum _format, GLenum _type, GLsizei _bufSize, void* _table) { GLAD_INSTRUMENT_CALL(glGetnColorTable, _target, _format, _type, _bufSize, _table); }
	#define glGetnColorTable glad_wrapped_glGetnColorTable
struct glad_tag_glCompressedTextureSubImage1D {};
inline void glad_wrapped_glCompressedTextureSubImage1D(GLuint _texture, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTextureSubImage1D, _texture, _level, _xoffset, _width, _format, _imageSize, _data); }
	#define glCompressedTextureSubImage1D glad_wrapped_glCompressedTextureSubImage1D
struct glad_tag_glMultiTexCoordP1uiv {};
inline void glad_wrapped_glMultiTexCoordP1uiv(GLenum _texture, GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP1uiv, _texture, _type, _coords); }
	#define glMultiTexCoordP1uiv glad_wrapped_glMultiTexCoordP1uiv
struct glad_tag_glUniform3fv {};
inline void glad_wrapped_glUniform3fv(GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniform3fv, _location, _count, _value); }
	#define glUniform3fv glad_wrapped_glUniform3fv
struct glad_tag_glMultiDrawElementsIndirect {};
inline void glad_wrapped_glMultiDrawElementsIndirect(GLenum _mode, GLenum _type, const void* _indirect, GLsizei _drawcount, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glMultiDrawElementsIndirect, _mode, _type, _indirect, _drawcount, _stride); }
	#define glMultiDrawElementsIndirect glad_wrapped_glMultiDrawElementsIndirect
struct glad_tag_glDepthRange {};
inline void glad_wrapped_glDepthRange(GLdouble _near, GLdouble _far) { GLAD_INSTRUMENT_CALL(glDepthRange, _near, _far); }
	#define glDepthRange glad_wrapped_glDepthRange
struct glad_tag_glInvalidateSubFramebuffer {};
inline void glad_wrapped_glInvalidateSubFramebuffer(GLenum _target, GLsizei _numAttachments, const GLenum* _attachments, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glInvalidateSubFramebuffer, _target, _numAttachments, _attachments, _x, _y, _width, _height); }
	#define glInvalidateSubFramebuffer glad_wrapped_glInvalidateSubFramebuffer
struct glad_tag_glMapBuffer {};
inline void* glad_wrapped_glMapBuffer(GLenum _target, GLenum _access) { return GLAD_INSTRUMENT_CALL(glMapBuffer, _target, _access); }
	#define glMapBuffer glad_wrapped_glMapBuffer
struct glad_tag_glClearTexImage {};
inline void glad_wrapped_glClearTexImage(GLuint _texture, GLint _level, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearTexImage, _texture, _level, _format, _type, _data); }
	#define glClearTexImage glad_wrapped_glClearTexImage
struct glad_tag_glVertexAttribLFormat {};
inline void glad_wrapped_glVertexAttribLFormat(GLuint _attribindex, GLint _size, GLenum _type, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexAttribLFormat, _attribindex, _size, _type, _relativeoffset); }
	#define glVertexAttribLFormat glad_wrapped_glVertexAttribLFormat
struct glad_tag_glCompressedTexImage3D {};
inline void glad_wrapped_glCompressedTexImage3D(GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTexImage3D, _target, _level, _internalformat, _width, _height, _depth, _border, _imageSize, _data); }
	#define glCompressedTexImage3D glad_wrapped_glCompressedTexImage3D
struct glad_tag_glDeleteSync {};
inline void glad_wrapped_glDeleteSync(GLsync _sync) { GLAD_INSTRUMENT_CALL(glDeleteSync, _sync); }
	#define glDeleteSync glad_wrapped_glDeleteSync
struct glad_tag_glCopyTexSubImage3D {};
inline void glad_wrapped_glCopyTexSubImage3D(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyTexSubImage3D, _target, _level, _xoffset, _yoffset, _zoffset, _x, _y, _width, _height); }
	#define glCopyTexSubImage3D glad_wrapped_glCopyTexSubImage3D
struct glad_tag_glGetTransformFeedbacki64_v {};
inline void glad_wrapped_glGetTransformFeedbacki64_v(GLuint _xfb, GLenum _pname, GLuint _index, GLint64* _param) { GLAD_INSTRUMENT_CALL(glGetTransformFeedbacki64_v, _xfb, _pname, _index, _param); }
	#define glGetTransformFeedbacki64_v glad_wrapped_glGetTransformFeedbacki64_v
struct glad_tag_glUniformMatrix4dv {};
inline void glad_wrapped_glUniformMatrix4dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix4dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix4dv glad_wrapped_glUniformMatrix4dv
struct glad_tag_glGetVertexAttribiv {};
inline void glad_wrapped_glGetVertexAttribiv(GLuint _index, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetVertexAttribiv, _index, _pname, _params); }
	#define glGetVertexAttribiv glad_wrapped_glGetVertexAttribiv
struct glad_tag_glUniformMatrix4x2dv {};
inline void glad_wrapped_glUniformMatrix4x2dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix4x2dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix4x2dv glad_wrapped_glUniformMatrix4x2dv
struct glad_tag_glMultiDrawElements {};
inline void glad_wrapped_glMultiDrawElements(GLenum _mode, const GLsizei* _count, GLenum _type, const void** _indices, GLsizei _drawcount) { GLAD_INSTRUMENT_CALL(glMultiDrawElements, _mode, _count, _type, _indices, _drawcount); }
	#define glMultiDrawElements glad_wrapped_glMultiDrawElements
struct glad_tag_glVertexAttrib3fv {};
inline void glad_wrapped_glVertexAttrib3fv(GLuint _index, const GLfloat* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib3fv, _index, _v); }
	#define glVertexAttrib3fv glad_wrapped_glVertexAttrib3fv
struct glad_tag_glUniform3iv {};
inline void glad_wrapped_glUniform3iv(GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glUniform3iv, _location, _count, _value); }
	#define glUniform3iv glad_wrapped_glUniform3iv
struct glad_tag_glPolygonMode {};
inline void glad_wrapped_glPolygonMode(GLenum _face, GLenum _mode) { GLAD_INSTRUMENT_CALL(glPolygonMode, _face, _mode); }
	#define glPolygonMode glad_wrapped_glPolygonMode
struct glad_tag_glDrawBuffers {};
inline void glad_wrapped_glDrawBuffers(GLsizei _n, const GLenum* _bufs) { GLAD_INSTRUMENT_CALL(glDrawBuffers, _n, _bufs); }
	#define glDrawBuffers glad_wrapped_glDrawBuffers
struct glad_tag_glGetnHistogram {};
inline void glad_wrapped_glGetnHistogram(GLenum _target, GLboolean _reset, GLenum _format, GLenum _type, GLsizei _bufSize, void* _values) { GLAD_INSTRUMENT_CALL(glGetnHistogram, _target, _reset, _format, _type, _bufSize, _values); }
	#define glGetnHistogram glad_wrapped_glGetnHistogram
struct glad_tag_glGetActiveUniformBlockiv {};
inline void glad_wrapped_glGetActiveUniformBlockiv(GLuint _program, GLuint _uniformBlockIndex, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetActiveUniformBlockiv, _program, _uniformBlockIndex, _pname, _params); }
	#define glGetActiveUniformBlockiv glad_wrapped_glGetActiveUniformBlockiv
struct glad_tag_glNamedFramebufferReadBuffer {};
inline void glad_wrapped_glNamedFramebufferReadBuffer(GLuint _framebuffer, GLenum _src) { GLAD_INSTRUMENT_CALL(glNamedFramebufferReadBuffer, _framebuffer, _src); }
	#define glNamedFramebufferReadBuffer glad_wrapped_glNamedFramebufferReadBuffer
struct glad_tag_glProgramUniform4iv {};
inline void glad_wrapped_glProgramUniform4iv(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4iv, _program, _location, _count, _value); }
	#define glProgramUniform4iv glad_wrapped_glProgramUniform4iv
struct glad_tag_glGetProgramBinary {};
inline void glad_wrapped_glGetProgramBinary(GLuint _program, GLsizei _bufSize, GLsizei* _length, GLenum* _binaryFormat, void* _binary) { GLAD_INSTRUMENT_CALL(glGetProgramBinary, _program, _bufSize, _length, _binaryFormat, _binary); }
	#define glGetProgramBinary glad_wrapped_glGetProgramBinary
struct glad_tag_glUseProgram {};
inline void glad_wrapped_glUseProgram(GLuint _program) { GLAD_INSTRUMENT_CALL(glUseProgram, _program); }
	#define glUseProgram glad_wrapped_glUseProgram
struct glad_tag_glGetProgramInfoLog {};
inline void glad_wrapped_glGetProgramInfoLog(GLuint _program, GLsizei _bufSize, GLsizei* _length, GLchar* _infoLog) { GLAD_INSTRUMENT_CALL(glGetProgramInfoLog, _program, _bufSize, _length, _infoLog); }
	#define glGetProgramInfoLog glad_wrapped_glGetProgramInfoLog
struct glad_tag_glBindTransformFeedback {};
inline void glad_wrapped_glBindTransformFeedback(GLenum _target, GLuint _id) { GLAD_INSTRUMENT_CALL(glBindTransformFeedback, _target, _id); }
	#define glBindTransformFeedback glad_wrapped_glBindTransformFeedback
struct glad_tag_glBindVertexArray {};
inline void glad_wrapped_glBindVertexArray(GLuint _array) { GLAD_INSTRUMENT_CALL(glBindVertexArray, _array); }
	#define glBindVertexArray glad_wrapped_glBindVertexArray
struct glad_tag_glDeleteBuffers {};
inline void glad_wrapped_glDeleteBuffers(GLsizei _n, const GLuint* _buffers) { GLAD_INSTRUMENT_CALL(glDeleteBuffers, _n, _buffers); }
	#define glDeleteBuffers glad_wrapped_glDeleteBuffers
struct glad_tag_glGenerateTextureMipmap {};
inline void glad_wrapped_glGenerateTextureMipmap(GLuint _texture) { GLAD_INSTRUMENT_CALL(glGenerateTextureMipmap, _texture); }
	#define glGenerateTextureMipmap glad_wrapped_glGenerateTextureMipmap
struct glad_tag_glSamplerParameterIiv {};
inline void glad_wrapped_glSamplerParameterIiv(GLuint _sampler, GLenum _pname, const GLint* _param) { GLAD_INSTRUMENT_CALL(glSamplerParameterIiv, _sampler, _pname, _param); }
	#define glSamplerParameterIiv glad_wrapped_glSamplerParameterIiv
struct glad_tag_glMultiDrawElementsBaseVertex {};
inline void glad_wrapped_glMultiDrawElementsBaseVertex(GLenum _mode, const GLsizei* _count, GLenum _type, const void** _indices, GLsizei _drawcount, const GLint* _basevertex) { GLAD_INSTRUMENT_CALL(glMultiDrawElementsBaseVertex, _mode, _count, _type, _indices, _drawcount, _basevertex); }
	#define glMultiDrawElementsBaseVertex glad_wrapped_glMultiDrawElementsBaseVertex
struct glad_tag_glNamedBufferSubData {};
inline void glad_wrapped_glNamedBufferSubData(GLuint _buffer, GLintptr _offset, GLsizeiptr _size, const void* _data) { GLAD_INSTRUMENT_CALL(glNamedBufferSubData, _buffer, _offset, _size, _data); }
	#define glNamedBufferSubData glad_wrapped_glNamedBufferSubData
struct glad_tag_glTextureStorage2D {};
inline void glad_wrapped_glTextureStorage2D(GLuint _texture, GLsizei _levels, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glTextureStorage2D, _texture, _levels, _internalformat, _width, _height); }
	#define glTextureStorage2D glad_wrapped_glTextureStorage2D
struct glad_tag_glGetnConvolutionFilter {};
inline void glad_wrapped_glGetnConvolutionFilter(GLenum _target, GLenum _format, GLenum _type, GLsizei _bufSize, void* _image) { GLAD_INSTRUMENT_CALL(glGetnConvolutionFilter, _target, _format, _type, _bufSize, _image); }
	#define glGetnConvolutionFilter glad_wrapped_glGetnConvolutionFilter
struct glad_tag_glUniform2uiv {};
inline void glad_wrapped_glUniform2uiv(GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glUniform2uiv, _location, _count, _value); }
	#define glUniform2uiv glad_wrapped_glUniform2uiv
struct glad_tag_glCompressedTexSubImage1D {};
inline void glad_wrapped_glCompressedTexSubImage1D(GLenum _target, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTexSubImage1D, _target, _level, _xoffset, _width, _format, _imageSize, _data); }
	#define glCompressedTexSubImage1D glad_wrapped_glCompressedTexSubImage1D
struct glad_tag_glFinish {};
inline void glad_wrapped_glFinish() { GLAD_INSTRUMENT_CALL(glFinish); }
	#define glFinish glad_wrapped_glFinish
struct glad_tag_glDepthRangeIndexed {};
inline void glad_wrapped_glDepthRangeIndexed(GLuint _index, GLdouble _n, GLdouble _f) { GLAD_INSTRUMENT_CALL(glDepthRangeIndexed, _index, _n, _f); }
	#define glDepthRangeIndexed glad_wrapped_glDepthRangeIndexed
struct glad_tag_glDeleteShader {};
inline void glad_wrapped_glDeleteShader(GLuint _shader) { GLAD_INSTRUMENT_CALL(glDeleteShader, _shader); }
	#define glDeleteShader glad_wrapped_glDeleteShader
struct glad_tag_glGetInternalformati64v {};
inline void glad_wrapped_glGetInternalformati64v(GLenum _target, GLenum _internalformat, GLenum _pname, GLsizei _bufSize, GLint64* _params) { GLAD_INSTRUMENT_CALL(glGetInternalformati64v, _target, _internalformat, _pname, _bufSize, _params); }
	#define glGetInternalformati64v glad_wrapped_glGetInternalformati64v
struct glad_tag_glCopyTextureSubImage1D {};
inline void glad_wrapped_glCopyTextureSubImage1D(GLuint _texture, GLint _level, GLint _xoffset, GLint _x, GLint _y, GLsizei _width) { GLAD_INSTRUMENT_CALL(glCopyTextureSubImage1D, _texture, _level, _xoffset, _x, _y, _width); }
	#define glCopyTextureSubImage1D glad_wrapped_glCopyTextureSubImage1D
struct glad_tag_glPushDebugGroup {};
inline void glad_wrapped_glPushDebugGroup(GLenum _source, GLuint _id, GLsizei _length, const GLchar* _message) { GLAD_INSTRUMENT_CALL(glPushDebugGroup, _source, _id, _length, _message); }
	#define glPushDebugGroup glad_wrapped_glPushDebugGroup
struct glad_tag_glVertexAttrib4Nsv {};
inline void glad_wrapped_glVertexAttrib4Nsv(GLuint _index, const GLshort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4Nsv, _index, _v); }
	#define glVertexAttrib4Nsv glad_wrapped_glVertexAttrib4Nsv
struct glad_tag_glGetProgramResourceLocationIndex {};
inline GLint glad_wrapped_glGetProgramResourceLocationIndex(GLuint _program, GLenum _programInterface, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetProgramResourceLocationIndex, _program, _programInterface, _name); }
	#define glGetProgramResourceLocationIndex glad_wrapped_glGetProgramResourceLocationIndex
struct glad_tag_glTextureParameterIuiv {};
inline void glad_wrapped_glTextureParameterIuiv(GLuint _texture, GLenum _pname, const GLuint* _params) { GLAD_INSTRUMENT_CALL(glTextureParameterIuiv, _texture, _pname, _params); }
	#define glTextureParameterIuiv glad_wrapped_glTextureParameterIuiv
struct glad_tag_glViewport {};
inline void glad_wrapped_glViewport(GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glViewport, _x, _y, _width, _height); }
	#define glViewport glad_wrapped_glViewport
struct glad_tag_glUniform1uiv {};
inline void glad_wrapped_glUniform1uiv(GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glUniform1uiv, _location, _count, _value); }
	#define glUniform1uiv glad_wrapped_glUniform1uiv
struct glad_tag_glTransformFeedbackVaryings {};
inline void glad_wrapped_glTransformFeedbackVaryings(GLuint _program, GLsizei _count, const GLchar** _varyings, GLenum _bufferMode) { GLAD_INSTRUMENT_CALL(glTransformFeedbackVaryings, _program, _count, _varyings, _bufferMode); }
	#define glTransformFeedbackVaryings glad_wrapped_glTransformFeedbackVaryings
struct glad_tag_glUniform2ui {};
inline void glad_wrapped_glUniform2ui(GLint _location, GLuint _v0, GLuint _v1) { GLAD_INSTRUMENT_CALL(glUniform2ui, _location, _v0, _v1); }
	#define glUniform2ui glad_wrapped_glUniform2ui
struct glad_tag_glGetnMapdv {};
inline void glad_wrapped_glGetnMapdv(GLenum _target, GLenum _query, GLsizei _bufSize, GLdouble* _v) { GLAD_INSTRUMENT_CALL(glGetnMapdv, _target, _query, _bufSize, _v); }
	#define glGetnMapdv glad_wrapped_glGetnMapdv
struct glad_tag_glDebugMessageCallback {};
inline void glad_wrapped_glDebugMessageCallback(GLDEBUGPROC _callback, const void* _userParam) { GLAD_INSTRUMENT_CALL(glDebugMessageCallback, _callback, _userParam); }
	#define glDebugMessageCallback glad_wrapped_glDebugMessageCallback
struct glad_tag_glVertexAttribI3i {};
inline void glad_wrapped_glVertexAttribI3i(GLuint _index, GLint _x, GLint _y, GLint _z) { GLAD_INSTRUMENT_CALL(glVertexAttribI3i, _index, _x, _y, _z); }
	#define glVertexAttribI3i glad_wrapped_glVertexAttribI3i
struct glad_tag_glInvalidateTexImage {};
inline void glad_wrapped_glInvalidateTexImage(GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glInvalidateTexImage, _texture, _level); }
	#define glInvalidateTexImage glad_wrapped_glInvalidateTexImage
struct glad_tag_glVertexAttribFormat {};
inline void glad_wrapped_glVertexAttribFormat(GLuint _attribindex, GLint _size, GLenum _type, GLboolean _normalized, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexAttribFormat, _attribindex, _size, _type, _normalized, _relativeoffset); }
	#define glVertexAttribFormat glad_wrapped_glVertexAttribFormat
struct glad_tag_glClearDepth {};
inline void glad_wrapped_glClearDepth(GLdouble _depth) { GLAD_INSTRUMENT_CALL(glClearDepth, _depth); }
	#define glClearDepth glad_wrapped_glClearDepth
struct glad_tag_glVertexAttribI4usv {};
inline void glad_wrapped_glVertexAttribI4usv(GLuint _index, const GLushort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI4usv, _index, _v); }
	#define glVertexAttribI4usv glad_wrapped_glVertexAttribI4usv
struct glad_tag_glTexParameterf {};
inline void glad_wrapped_glTexParameterf(GLenum _target, GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glTexParameterf, _target, _pname, _param); }
	#define glTexParameterf glad_wrapped_glTexParameterf
struct glad_tag_glVertexAttribBinding {};
inline void glad_wrapped_glVertexAttribBinding(GLuint _attribindex, GLuint _bindingindex) { GLAD_INSTRUMENT_CALL(glVertexAttribBinding, _attribindex, _bindingindex); }
	#define glVertexAttribBinding glad_wrapped_glVertexAttribBinding
struct glad_tag_glTexParameteri {};
inline void glad_wrapped_glTexParameteri(GLenum _target, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glTexParameteri, _target, _pname, _param); }
	#define glTexParameteri glad_wrapped_glTexParameteri
struct glad_tag_glGetActiveSubroutineUniformiv {};
inline void glad_wrapped_glGetActiveSubroutineUniformiv(GLuint _program, GLenum _shadertype, GLuint _index, GLenum _pname, GLint* _values) { GLAD_INSTRUMENT_CALL(glGetActiveSubroutineUniformiv, _program, _shadertype, _index, _pname, _values); }
	#define glGetActiveSubroutineUniformiv glad_wrapped_glGetActiveSubroutineUniformiv
struct glad_tag_glGetShaderSource {};
inline void glad_wrapped_glGetShaderSource(GLuint _shader, GLsizei _bufSize, GLsizei* _length, GLchar* _source) { GLAD_INSTRUMENT_CALL(glGetShaderSource, _shader, _bufSize, _length, _source); }
	#define glGetShaderSource glad_wrapped_glGetShaderSource
struct glad_tag_glGetnTexImage {};
inline void glad_wrapped_glGetnTexImage(GLenum _target, GLint _level, GLenum _format, GLenum _type, GLsizei _bufSize, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetnTexImage, _target, _level, _format, _type, _bufSize, _pixels); }
	#define glGetnTexImage glad_wrapped_glGetnTexImage
struct glad_tag_glTexBuffer {};
inline void glad_wrapped_glTexBuffer(GLenum _target, GLenum _internalformat, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glTexBuffer, _target, _internalformat, _buffer); }
	#define glTexBuffer glad_wrapped_glTexBuffer
struct glad_tag_glPixelStorei {};
inline void glad_wrapped_glPixelStorei(GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glPixelStorei, _pname, _param); }
	#define glPixelStorei glad_wrapped_glPixelStorei
struct glad_tag_glValidateProgram {};
inline void glad_wrapped_glValidateProgram(GLuint _program) { GLAD_INSTRUMENT_CALL(glValidateProgram, _program); }
	#define glValidateProgram glad_wrapped_glValidateProgram
struct glad_tag_glPixelStoref {};
inline void glad_wrapped_glPixelStoref(GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glPixelStoref, _pname, _param); }
	#define glPixelStoref glad_wrapped_glPixelStoref
struct glad_tag_glCreateBuffers {};
inline void glad_wrapped_glCreateBuffers(GLsizei _n, GLuint* _buffers) { GLAD_INSTRUMENT_CALL(glCreateBuffers, _n, _buffers); }
	#define glCreateBuffers glad_wrapped_glCreateBuffers
struct glad_tag_glGetBooleani_v {};
inline void glad_wrapped_glGetBooleani_v(GLenum _target, GLuint _index, GLboolean* _data) { GLAD_INSTRUMENT_CALL(glGetBooleani_v, _target, _index, _data); }
	#define glGetBooleani_v glad_wrapped_glGetBooleani_v
struct glad_tag_glClipControl {};
inline void glad_wrapped_glClipControl(GLenum _origin, GLenum _depth) { GLAD_INSTRUMENT_CALL(glClipControl, _origin, _depth); }
	#define glClipControl glad_wrapped_glClipControl
struct glad_tag_glMultiTexCoordP2uiv {};
inline void glad_wrapped_glMultiTexCoordP2uiv(GLenum _texture, GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP2uiv, _texture, _type, _coords); }
	#define glMultiTexCoordP2uiv glad_wrapped_glMultiTexCoordP2uiv
struct glad_tag_glGenProgramPipelines {};
inline void glad_wrapped_glGenProgramPipelines(GLsizei _n, GLuint* _pipelines) { GLAD_INSTRUMENT_CALL(glGenProgramPipelines, _n, _pipelines); }
	#define glGenProgramPipelines glad_wrapped_glGenProgramPipelines
struct glad_tag_glGetInternalformativ {};
inline void glad_wrapped_glGetInternalformativ(GLenum _target, GLenum _internalformat, GLenum _pname, GLsizei _bufSize, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetInternalformativ, _target, _internalformat, _pname, _bufSize, _params); }
	#define glGetInternalformativ glad_wrapped_glGetInternalformativ
struct glad_tag_glCopyTextureSubImage3D {};
inline void glad_wrapped_glCopyTextureSubImage3D(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyTextureSubImage3D, _texture, _level, _xoffset, _yoffset, _zoffset, _x, _y, _width, _height); }
	#define glCopyTextureSubImage3D glad_wrapped_glCopyTextureSubImage3D
struct glad_tag_glVertexAttribP1uiv {};
inline void glad_wrapped_glVertexAttribP1uiv(GLuint _index, GLenum _type, GLboolean _normalized, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP1uiv, _index, _type, _normalized, _value); }
	#define glVertexAttribP1uiv glad_wrapped_glVertexAttribP1uiv
struct glad_tag_glLinkProgram {};
inline void glad_wrapped_glLinkProgram(GLuint _program) { GLAD_INSTRUMENT_CALL(glLinkProgram, _program); }
	#define glLinkProgram glad_wrapped_glLinkProgram
struct glad_tag_glBindTexture {};
inline void glad_wrapped_glBindTexture(GLenum _target, GLuint _texture) { GLAD_INSTRUMENT_CALL(glBindTexture, _target, _texture); }
	#define glBindTexture glad_wrapped_glBindTexture
struct glad_tag_glMultiDrawArraysIndirect {};
inline void glad_wrapped_glMultiDrawArraysIndirect(GLenum _mode, const void* _indirect, GLsizei _drawcount, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glMultiDrawArraysIndirect, _mode, _indirect, _drawcount, _stride); }
	#define glMultiDrawArraysIndirect glad_wrapped_glMultiDrawArraysIndirect
struct glad_tag_glGetObjectLabel {};
inline void glad_wrapped_glGetObjectLabel(GLenum _identifier, GLuint _name, GLsizei _bufSize, GLsizei* _length, GLchar* _label) { GLAD_INSTRUMENT_CALL(glGetObjectLabel, _identifier, _name, _bufSize, _length, _label); }
	#define glGetObjectLabel glad_wrapped_glGetObjectLabel
struct glad_tag_glGetProgramPipelineInfoLog {};
inline void glad_wrapped_glGetProgramPipelineInfoLog(GLuint _pipeline, GLsizei _bufSize, GLsizei* _length, GLchar* _infoLog) { GLAD_INSTRUMENT_CALL(glGetProgramPipelineInfoLog, _pipeline, _bufSize, _length, _infoLog); }
	#define glGetProgramPipelineInfoLog glad_wrapped_glGetProgramPipelineInfoLog
struct glad_tag_glGetString {};
inline const GLubyte* glad_wrapped_glGetString(GLenum _name) { return GLAD_INSTRUMENT_CALL(glGetString, _name); }
	#define glGetString glad_wrapped_glGetString
struct glad_tag_glVertexAttribP2uiv {};
inline void glad_wrapped_glVertexAttribP2uiv(GLuint _index, GLenum _type, GLboolean _normalized, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP2uiv, _index, _type, _normalized, _value); }
	#define glVertexAttribP2uiv glad_wrapped_glVertexAttribP2uiv
struct glad_tag_glDetachShader {};
inline void glad_wrapped_glDetachShader(GLuint _program, GLuint _shader) { GLAD_INSTRUMENT_CALL(glDetachShader, _program, _shader); }
	#define glDetachShader glad_wrapped_glDetachShader
struct glad_tag_glProgramUniform3i {};
inline void glad_wrapped_glProgramUniform3i(GLuint _program, GLint _location, GLint _v0, GLint _v1, GLint _v2) { GLAD_INSTRUMENT_CALL(glProgramUniform3i, _program, _location, _v0, _v1, _v2); }
	#define glProgramUniform3i glad_wrapped_glProgramUniform3i
struct glad_tag_glUniformMatrix3x4dv {};
inline void glad_wrapped_glUniformMatrix3x4dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix3x4dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix3x4dv glad_wrapped_glUniformMatrix3x4dv
struct glad_tag_glEndQuery {};
inline void glad_wrapped_glEndQuery(GLenum _target) { GLAD_INSTRUMENT_CALL(glEndQuery, _target); }
	#define glEndQuery glad_wrapped_glEndQuery
struct glad_tag_glNormalP3ui {};
inline void glad_wrapped_glNormalP3ui(GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glNormalP3ui, _type, _coords); }
	#define glNormalP3ui glad_wrapped_glNormalP3ui
struct glad_tag_glFramebufferParameteri {};
inline void glad_wrapped_glFramebufferParameteri(GLenum _target, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glFramebufferParameteri, _target, _pname, _param); }
	#define glFramebufferParameteri glad_wrapped_glFramebufferParameteri
struct glad_tag_glGetProgramResourceName {};
inline void glad_wrapped_glGetProgramResourceName(GLuint _program, GLenum _programInterface, GLuint _index, GLsizei _bufSize, GLsizei* _length, GLchar* _name) { GLAD_INSTRUMENT_CALL(glGetProgramResourceName, _program, _programInterface, _index, _bufSize, _length, _name); }
	#define glGetProgramResourceName glad_wrapped_glGetProgramResourceName
struct glad_tag_glUniformMatrix4x3dv {};
inline void glad_wrapped_glUniformMatrix4x3dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix4x3dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix4x3dv glad_wrapped_glUniformMatrix4x3dv
struct glad_tag_glDepthRangeArrayv {};
inline void glad_wrapped_glDepthRangeArrayv(GLuint _first, GLsizei _count, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glDepthRangeArrayv, _first, _count, _v); }
	#define glDepthRangeArrayv glad_wrapped_glDepthRangeArrayv
struct glad_tag_glVertexAttribI2ui {};
inline void glad_wrapped_glVertexAttribI2ui(GLuint _index, GLuint _x, GLuint _y) { GLAD_INSTRUMENT_CALL(glVertexAttribI2ui, _index, _x, _y); }
	#define glVertexAttribI2ui glad_wrapped_glVertexAttribI2ui
struct glad_tag_glGetProgramResourceLocation {};
inline GLint glad_wrapped_glGetProgramResourceLocation(GLuint _program, GLenum _programInterface, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetProgramResourceLocation, _program, _programInterface, _name); }
	#define glGetProgramResourceLocation glad_wrapped_glGetProgramResourceLocation
struct glad_tag_glDeleteTextures {};
inline void glad_wrapped_glDeleteTextures(GLsizei _n, const GLuint* _textures) { GLAD_INSTRUMENT_CALL(glDeleteTextures, _n, _textures); }
	#define glDeleteTextures glad_wrapped_glDeleteTextures
struct glad_tag_glGetActiveAtomicCounterBufferiv {};
inline void glad_wrapped_glGetActiveAtomicCounterBufferiv(GLuint _program, GLuint _bufferIndex, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetActiveAtomicCounterBufferiv, _program, _bufferIndex, _pname, _params); }
	#define glGetActiveAtomicCounterBufferiv glad_wrapped_glGetActiveAtomicCounterBufferiv
struct glad_tag_glStencilOpSeparate {};
inline void glad_wrapped_glStencilOpSeparate(GLenum _face, GLenum _sfail, GLenum _dpfail, GLenum _dppass) { GLAD_INSTRUMENT_CALL(glStencilOpSeparate, _face, _sfail, _dpfail, _dppass); }
	#define glStencilOpSeparate glad_wrapped_glStencilOpSeparate
struct glad_tag_glDeleteQueries {};
inline void glad_wrapped_glDeleteQueries(GLsizei _n, const GLuint* _ids) { GLAD_INSTRUMENT_CALL(glDeleteQueries, _n, _ids); }
	#define glDeleteQueries glad_wrapped_glDeleteQueries
struct glad_tag_glNormalP3uiv {};
inline void glad_wrapped_glNormalP3uiv(GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glNormalP3uiv, _type, _coords); }
	#define glNormalP3uiv glad_wrapped_glNormalP3uiv
struct glad_tag_glVertexAttrib4f {};
inline void glad_wrapped_glVertexAttrib4f(GLuint _index, GLfloat _x, GLfloat _y, GLfloat _z, GLfloat _w) { GLAD_INSTRUMENT_CALL(glVertexAttrib4f, _index, _x, _y, _z, _w); }
	#define glVertexAttrib4f glad_wrapped_glVertexAttrib4f
struct glad_tag_glVertexAttrib4d {};
inline void glad_wrapped_glVertexAttrib4d(GLuint _index, GLdouble _x, GLdouble _y, GLdouble _z, GLdouble _w) { GLAD_INSTRUMENT_CALL(glVertexAttrib4d, _index, _x, _y, _z, _w); }
	#define glVertexAttrib4d glad_wrapped_glVertexAttrib4d
struct glad_tag_glViewportIndexedfv {};
inline void glad_wrapped_glViewportIndexedfv(GLuint _index, const GLfloat* _v) { GLAD_INSTRUMENT_CALL(glViewportIndexedfv, _index, _v); }
	#define glViewportIndexedfv glad_wrapped_glViewportIndexedfv
struct glad_tag_glBindBuffersBase {};
inline void glad_wrapped_glBindBuffersBase(GLenum _target, GLuint _first, GLsizei _count, const GLuint* _buffers) { GLAD_INSTRUMENT_CALL(glBindBuffersBase, _target, _first, _count, _buffers); }
	#define glBindBuffersBase glad_wrapped_glBindBuffersBase
struct glad_tag_glVertexAttribL4dv {};
inline void glad_wrapped_glVertexAttribL4dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribL4dv, _index, _v); }
	#define glVertexAttribL4dv glad_wrapped_glVertexAttribL4dv
struct glad_tag_glGetTexParameteriv {};
inline void glad_wrapped_glGetTexParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTexParameteriv, _target, _pname, _params); }
	#define glGetTexParameteriv glad_wrapped_glGetTexParameteriv
struct glad_tag_glCreateVertexArrays {};
inline void glad_wrapped_glCreateVertexArrays(GLsizei _n, GLuint* _arrays) { GLAD_INSTRUMENT_CALL(glCreateVertexArrays, _n, _arrays); }
	#define glCreateVertexArrays glad_wrapped_glCreateVertexArrays
struct glad_tag_glProgramUniform1dv {};
inline void glad_wrapped_glProgramUniform1dv(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1dv, _program, _location, _count, _value); }
	#define glProgramUniform1dv glad_wrapped_glProgramUniform1dv
struct glad_tag_glVertexAttrib4s {};
inline void glad_wrapped_glVertexAttrib4s(GLuint _index, GLshort _x, GLshort _y, GLshort _z, GLshort _w) { GLAD_INSTRUMENT_CALL(glVertexAttrib4s, _index, _x, _y, _z, _w); }
	#define glVertexAttrib4s glad_wrapped_glVertexAttrib4s
struct glad_tag_glDrawElementsBaseVertex {};
inline void glad_wrapped_glDrawElementsBaseVertex(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices, GLint _basevertex) { GLAD_INSTRUMENT_CALL(glDrawElementsBaseVertex, _mode, _count, _type, _indices, _basevertex); }
	#define glDrawElementsBaseVertex glad_wrapped_glDrawElementsBaseVertex
struct glad_tag_glSampleCoverage {};
inline void glad_wrapped_glSampleCoverage(GLfloat _value, GLboolean _invert) { GLAD_INSTRUMENT_CALL(glSampleCoverage, _value, _invert); }
	#define glSampleCoverage glad_wrapped_glSampleCoverage
struct glad_tag_glSamplerParameteri {};
inline void glad_wrapped_glSamplerParameteri(GLuint _sampler, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glSamplerParameteri, _sampler, _pname, _param); }
	#define glSamplerParameteri glad_wrapped_glSamplerParameteri
struct glad_tag_glClearBufferSubData {};
inline void glad_wrapped_glClearBufferSubData(GLenum _target, GLenum _internalformat, GLintptr _offset, GLsizeiptr _size, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearBufferSubData, _target, _internalformat, _offset, _size, _format, _type, _data); }
	#define glClearBufferSubData glad_wrapped_glClearBufferSubData
struct glad_tag_glSamplerParameterf {};
inline void glad_wrapped_glSamplerParameterf(GLuint _sampler, GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glSamplerParameterf, _sampler, _pname, _param); }
	#define glSamplerParameterf glad_wrapped_glSamplerParameterf
struct glad_tag_glTexStorage1D {};
inline void glad_wrapped_glTexStorage1D(GLenum _target, GLsizei _levels, GLenum _internalformat, GLsizei _width) { GLAD_INSTRUMENT_CALL(glTexStorage1D, _target, _levels, _internalformat, _width); }
	#define glTexStorage1D glad_wrapped_glTexStorage1D
struct glad_tag_glUniform1f {};
inline void glad_wrapped_glUniform1f(GLint _location, GLfloat _v0) { GLAD_INSTRUMENT_CALL(glUniform1f, _location, _v0); }
	#define glUniform1f glad_wrapped_glUniform1f
struct glad_tag_glGetVertexAttribfv {};
inline void glad_wrapped_glGetVertexAttribfv(GLuint _index, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetVertexAttribfv, _index, _pname, _params); }
	#define glGetVertexAttribfv glad_wrapped_glGetVertexAttribfv
struct glad_tag_glUniform1d {};
inline void glad_wrapped_glUniform1d(GLint _location, GLdouble _x) { GLAD_INSTRUMENT_CALL(glUniform1d, _location, _x); }
	#define glUniform1d glad_wrapped_glUniform1d
struct glad_tag_glGetCompressedTexImage {};
inline void glad_wrapped_glGetCompressedTexImage(GLenum _target, GLint _level, void* _img) { GLAD_INSTRUMENT_CALL(glGetCompressedTexImage, _target, _level, _img); }
	#define glGetCompressedTexImage glad_wrapped_glGetCompressedTexImage
struct glad_tag_glGetnCompressedTexImage {};
inline void glad_wrapped_glGetnCompressedTexImage(GLenum _target, GLint _lod, GLsizei _bufSize, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetnCompressedTexImage, _target, _lod, _bufSize, _pixels); }
	#define glGetnCompressedTexImage glad_wrapped_glGetnCompressedTexImage
struct glad_tag_glUniform1i {};
inline void glad_wrapped_glUniform1i(GLint _location, GLint _v0) { GLAD_INSTRUMENT_CALL(glUniform1i, _location, _v0); }
	#define glUniform1i glad_wrapped_glUniform1i
struct glad_tag_glGetActiveAttrib {};
inline void glad_wrapped_glGetActiveAttrib(GLuint _program, GLuint _index, GLsizei _bufSize, GLsizei* _length, GLint* _size, GLenum* _type, GLchar* _name) { GLAD_INSTRUMENT_CALL(glGetActiveAttrib, _program, _index, _bufSize, _length, _size, _type, _name); }
	#define glGetActiveAttrib glad_wrapped_glGetActiveAttrib
struct glad_tag_glTexSubImage2D {};
inline void glad_wrapped_glTexSubImage2D(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTexSubImage2D, _target, _level, _xoffset, _yoffset, _width, _height, _format, _type, _pixels); }
	#define glTexSubImage2D glad_wrapped_glTexSubImage2D
struct glad_tag_glDisable {};
inline void glad_wrapped_glDisable(GLenum _cap) { GLAD_INSTRUMENT_CALL(glDisable, _cap); }
	#define glDisable glad_wrapped_glDisable
struct glad_tag_glLogicOp {};
inline void glad_wrapped_glLogicOp(GLenum _opcode) { GLAD_INSTRUMENT_CALL(glLogicOp, _opcode); }
	#define glLogicOp glad_wrapped_glLogicOp
struct glad_tag_glProgramUniformMatrix3x4fv {};
inline void glad_wrapped_glProgramUniformMatrix3x4fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x4fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x4fv glad_wrapped_glProgramUniformMatrix3x4fv
struct glad_tag_glGetTextureParameterIuiv {};
inline void glad_wrapped_glGetTextureParameterIuiv(GLuint _texture, GLenum _pname, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameterIuiv, _texture, _pname, _params); }
	#define glGetTextureParameterIuiv glad_wrapped_glGetTextureParameterIuiv
struct glad_tag_glProgramUniform4uiv {};
inline void glad_wrapped_glProgramUniform4uiv(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4uiv, _program, _location, _count, _value); }
	#define glProgramUniform4uiv glad_wrapped_glProgramUniform4uiv
struct glad_tag_glUniform4ui {};
inline void glad_wrapped_glUniform4ui(GLint _location, GLuint _v0, GLuint _v1, GLuint _v2, GLuint _v3) { GLAD_INSTRUMENT_CALL(glUniform4ui, _location, _v0, _v1, _v2, _v3); }
	#define glUniform4ui glad_wrapped_glUniform4ui
struct glad_tag_glCopyTextureSubImage2D {};
inline void glad_wrapped_glCopyTextureSubImage2D(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyTextureSubImage2D, _texture, _level, _xoffset, _yoffset, _x, _y, _width, _height); }
	#define glCopyTextureSubImage2D glad_wrapped_glCopyTextureSubImage2D
struct glad_tag_glBindFramebuffer {};
inline void glad_wrapped_glBindFramebuffer(GLenum _target, GLuint _framebuffer) { GLAD_INSTRUMENT_CALL(glBindFramebuffer, _target, _framebuffer); }
	#define glBindFramebuffer glad_wrapped_glBindFramebuffer
struct glad_tag_glCullFace {};
inline void glad_wrapped_glCullFace(GLenum _mode) { GLAD_INSTRUMENT_CALL(glCullFace, _mode); }
	#define glCullFace glad_wrapped_glCullFace
struct glad_tag_glProgramUniform4i {};
inline void glad_wrapped_glProgramUniform4i(GLuint _program, GLint _location, GLint _v0, GLint _v1, GLint _v2, GLint _v3) { GLAD_INSTRUMENT_CALL(glProgramUniform4i, _program, _location, _v0, _v1, _v2, _v3); }
	#define glProgramUniform4i glad_wrapped_glProgramUniform4i
struct glad_tag_glProgramUniform4f {};
inline void glad_wrapped_glProgramUniform4f(GLuint _program, GLint _location, GLfloat _v0, GLfloat _v1, GLfloat _v2, GLfloat _v3) { GLAD_INSTRUMENT_CALL(glProgramUniform4f, _program, _location, _v0, _v1, _v2, _v3); }
	#define glProgramUniform4f glad_wrapped_glProgramUniform4f
struct glad_tag_glViewportIndexedf {};
inline void glad_wrapped_glViewportIndexedf(GLuint _index, GLfloat _x, GLfloat _y, GLfloat _w, GLfloat _h) { GLAD_INSTRUMENT_CALL(glViewportIndexedf, _index, _x, _y, _w, _h); }
	#define glViewportIndexedf glad_wrapped_glViewportIndexedf
struct glad_tag_glProgramUniform4d {};
inline void glad_wrapped_glProgramUniform4d(GLuint _program, GLint _location, GLdouble _v0, GLdouble _v1, GLdouble _v2, GLdouble _v3) { GLAD_INSTRUMENT_CALL(glProgramUniform4d, _program, _location, _v0, _v1, _v2, _v3); }
	#define glProgramUniform4d glad_wrapped_glProgramUniform4d
struct glad_tag_glTextureParameterIiv {};
inline void glad_wrapped_glTextureParameterIiv(GLuint _texture, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glTextureParameterIiv, _texture, _pname, _params); }
	#define glTextureParameterIiv glad_wrapped_glTextureParameterIiv
struct glad_tag_glGetStringi {};
inline const GLubyte* glad_wrapped_glGetStringi(GLenum _name, GLuint _index) { return GLAD_INSTRUMENT_CALL(glGetStringi, _name, _index); }
	#define glGetStringi glad_wrapped_glGetStringi
struct glad_tag_glTextureSubImage2D {};
inline void glad_wrapped_glTextureSubImage2D(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureSubImage2D, _texture, _level, _xoffset, _yoffset, _width, _height, _format, _type, _pixels); }
	#define glTextureSubImage2D glad_wrapped_glTextureSubImage2D
struct glad_tag_glScissorIndexed {};
inline void glad_wrapped_glScissorIndexed(GLuint _index, GLint _left, GLint _bottom, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glScissorIndexed, _index, _left, _bottom, _width, _height); }
	#define glScissorIndexed glad_wrapped_glScissorIndexed
struct glad_tag_glDrawTransformFeedbackStream {};
inline void glad_wrapped_glDrawTransformFeedbackStream(GLenum _mode, GLuint _id, GLuint _stream) { GLAD_INSTRUMENT_CALL(glDrawTransformFeedbackStream, _mode, _id, _stream); }
	#define glDrawTransformFeedbackStream glad_wrapped_glDrawTransformFeedbackStream
struct glad_tag_glAttachShader {};
inline void glad_wrapped_glAttachShader(GLuint _program, GLuint _shader) { GLAD_INSTRUMENT_CALL(glAttachShader, _program, _shader); }
	#define glAttachShader glad_wrapped_glAttachShader
struct glad_tag_glQueryCounter {};
inline void glad_wrapped_glQueryCounter(GLuint _id, GLenum _target) { GLAD_INSTRUMENT_CALL(glQueryCounter, _id, _target); }
	#define glQueryCounter glad_wrapped_glQueryCounter
struct glad_tag_glProvokingVertex {};
inline void glad_wrapped_glProvokingVertex(GLenum _mode) { GLAD_INSTRUMENT_CALL(glProvokingVertex, _mode); }
	#define glProvokingVertex glad_wrapped_glProvokingVertex
struct glad_tag_glShaderBinary {};
inline void glad_wrapped_glShaderBinary(GLsizei _count, const GLuint* _shaders, GLenum _binaryformat, const void* _binary, GLsizei _length) { GLAD_INSTRUMENT_CALL(glShaderBinary, _count, _shaders, _binaryformat, _binary, _length); }
	#define glShaderBinary glad_wrapped_glShaderBinary
struct glad_tag_glUnmapNamedBuffer {};
inline GLboolean glad_wrapped_glUnmapNamedBuffer(GLuint _buffer) { return GLAD_INSTRUMENT_CALL(glUnmapNamedBuffer, _buffer); }
	#define glUnmapNamedBuffer glad_wrapped_glUnmapNamedBuffer
struct glad_tag_glDrawElements {};
inline void glad_wrapped_glDrawElements(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices) { GLAD_INSTRUMENT_CALL(glDrawElements, _mode, _count, _type, _indices); }
	#define glDrawElements glad_wrapped_glDrawElements
struct glad_tag_glNamedRenderbufferStorageMultisample {};
inline void glad_wrapped_glNamedRenderbufferStorageMultisample(GLuint _renderbuffer, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glNamedRenderbufferStorageMultisample, _renderbuffer, _samples, _internalformat, _width, _height); }
	#define glNamedRenderbufferStorageMultisample glad_wrapped_glNamedRenderbufferStorageMultisample
struct glad_tag_glVertexAttribI4sv {};
inline void glad_wrapped_glVertexAttribI4sv(GLuint _index, const GLshort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI4sv, _index, _v); }
	#define glVertexAttribI4sv glad_wrapped_glVertexAttribI4sv
struct glad_tag_glClearNamedBufferData {};
inline void glad_wrapped_glClearNamedBufferData(GLuint _buffer, GLenum _internalformat, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearNamedBufferData, _buffer, _internalformat, _format, _type, _data); }
	#define glClearNamedBufferData glad_wrapped_glClearNamedBufferData
struct glad_tag_glUniform1iv {};
inline void glad_wrapped_glUniform1iv(GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glUniform1iv, _location, _count, _value); }
	#define glUniform1iv glad_wrapped_glUniform1iv
struct glad_tag_glCreateShaderProgramv {};
inline GLuint glad_wrapped_glCreateShaderProgramv(GLenum _type, GLsizei _count, const GLchar** _strings) { return GLAD_INSTRUMENT_CALL(glCreateShaderProgramv, _type, _count, _strings); }
	#define glCreateShaderProgramv glad_wrapped_glCreateShaderProgramv
struct glad_tag_glGetQueryObjectiv {};
inline void glad_wrapped_glGetQueryObjectiv(GLuint _id, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetQueryObjectiv, _id, _pname, _params); }
	#define glGetQueryObjectiv glad_wrapped_glGetQueryObjectiv
struct glad_tag_glReadBuffer {};
inline void glad_wrapped_glReadBuffer(GLenum _src) { GLAD_INSTRUMENT_CALL(glReadBuffer, _src); }
	#define glReadBuffer glad_wrapped_glReadBuffer
struct glad_tag_glTexParameterIuiv {};
inline void glad_wrapped_glTexParameterIuiv(GLenum _target, GLenum _pname, const GLuint* _params) { GLAD_INSTRUMENT_CALL(glTexParameterIuiv, _target, _pname, _params); }
	#define glTexParameterIuiv glad_wrapped_glTexParameterIuiv
struct glad_tag_glDrawArraysInstanced {};
inline void glad_wrapped_glDrawArraysInstanced(GLenum _mode, GLint _first, GLsizei _count, GLsizei _instancecount) { GLAD_INSTRUMENT_CALL(glDrawArraysInstanced, _mode, _first, _count, _instancecount); }
	#define glDrawArraysInstanced glad_wrapped_glDrawArraysInstanced
struct glad_tag_glGenerateMipmap {};
inline void glad_wrapped_glGenerateMipmap(GLenum _target) { GLAD_INSTRUMENT_CALL(glGenerateMipmap, _target); }
	#define glGenerateMipmap glad_wrapped_glGenerateMipmap
struct glad_tag_glCompressedTextureSubImage2D {};
inline void glad_wrapped_glCompressedTextureSubImage2D(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTextureSubImage2D, _texture, _level, _xoffset, _yoffset, _width, _height, _format, _imageSize, _data); }
	#define glCompressedTextureSubImage2D glad_wrapped_glCompressedTextureSubImage2D
struct glad_tag_glProgramUniformMatrix2fv {};
inline void glad_wrapped_glProgramUniformMatrix2fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2fv glad_wrapped_glProgramUniformMatrix2fv
struct glad_tag_glSamplerParameteriv {};
inline void glad_wrapped_glSamplerParameteriv(GLuint _sampler, GLenum _pname, const GLint* _param) { GLAD_INSTRUMENT_CALL(glSamplerParameteriv, _sampler, _pname, _param); }
	#define glSamplerParameteriv glad_wrapped_glSamplerParameteriv
struct glad_tag_glVertexAttrib3f {};
inline void glad_wrapped_glVertexAttrib3f(GLuint _index, GLfloat _x, GLfloat _y, GLfloat _z) { GLAD_INSTRUMENT_CALL(glVertexAttrib3f, _index, _x, _y, _z); }
	#define glVertexAttrib3f glad_wrapped_glVertexAttrib3f
struct glad_tag_glVertexAttrib4uiv {};
inline void glad_wrapped_glVertexAttrib4uiv(GLuint _index, const GLuint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4uiv, _index, _v); }
	#define glVertexAttrib4uiv glad_wrapped_glVertexAttrib4uiv
struct glad_tag_glPointParameteri {};
inline void glad_wrapped_glPointParameteri(GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glPointParameteri, _pname, _param); }
	#define glPointParameteri glad_wrapped_glPointParameteri
struct glad_tag_glBlendColor {};
inline void glad_wrapped_glBlendColor(GLfloat _red, GLfloat _green, GLfloat _blue, GLfloat _alpha) { GLAD_INSTRUMENT_CALL(glBlendColor, _red, _green, _blue, _alpha); }
	#define glBlendColor glad_wrapped_glBlendColor
struct glad_tag_glSamplerParameterIuiv {};
inline void glad_wrapped_glSamplerParameterIuiv(GLuint _sampler, GLenum _pname, const GLuint* _param) { GLAD_INSTRUMENT_CALL(glSamplerParameterIuiv, _sampler, _pname, _param); }
	#define glSamplerParameterIuiv glad_wrapped_glSamplerParameterIuiv
struct glad_tag_glCheckNamedFramebufferStatus {};
inline GLenum glad_wrapped_glCheckNamedFramebufferStatus(GLuint _framebuffer, GLenum _target) { return GLAD_INSTRUMENT_CALL(glCheckNamedFramebufferStatus, _framebuffer, _target); }
	#define glCheckNamedFramebufferStatus glad_wrapped_glCheckNamedFramebufferStatus
struct glad_tag_glUnmapBuffer {};
inline GLboolean glad_wrapped_glUnmapBuffer(GLenum _target) { return GLAD_INSTRUMENT_CALL(glUnmapBuffer, _target); }
	#define glUnmapBuffer glad_wrapped_glUnmapBuffer
struct glad_tag_glPointParameterf {};
inline void glad_wrapped_glPointParameterf(GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glPointParameterf, _pname, _param); }
	#define glPointParameterf glad_wrapped_glPointParameterf
struct glad_tag_glProgramUniform1iv {};
inline void glad_wrapped_glProgramUniform1iv(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1iv, _program, _location, _count, _value); }
	#define glProgramUniform1iv glad_wrapped_glProgramUniform1iv
struct glad_tag_glGetVertexArrayiv {};
inline void glad_wrapped_glGetVertexArrayiv(GLuint _vaobj, GLenum _pname, GLint* _param) { GLAD_INSTRUMENT_CALL(glGetVertexArrayiv, _vaobj, _pname, _param); }
	#define glGetVertexArrayiv glad_wrapped_glGetVertexArrayiv
struct glad_tag_glVertexAttrib3s {};
inline void glad_wrapped_glVertexAttrib3s(GLuint _index, GLshort _x, GLshort _y, GLshort _z) { GLAD_INSTRUMENT_CALL(glVertexAttrib3s, _index, _x, _y, _z); }
	#define glVertexAttrib3s glad_wrapped_glVertexAttrib3s
struct glad_tag_glBindRenderbuffer {};
inline void glad_wrapped_glBindRenderbuffer(GLenum _target, GLuint _renderbuffer) { GLAD_INSTRUMENT_CALL(glBindRenderbuffer, _target, _renderbuffer); }
	#define glBindRenderbuffer glad_wrapped_glBindRenderbuffer
struct glad_tag_glVertexAttribP4uiv {};
inline void glad_wrapped_glVertexAttribP4uiv(GLuint _index, GLenum _type, GLboolean _normalized, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP4uiv, _index, _type, _normalized, _value); }
	#define glVertexAttribP4uiv glad_wrapped_glVertexAttribP4uiv
struct glad_tag_glGetProgramStageiv {};
inline void glad_wrapped_glGetProgramStageiv(GLuint _program, GLenum _shadertype, GLenum _pname, GLint* _values) { GLAD_INSTRUMENT_CALL(glGetProgramStageiv, _program, _shadertype, _pname, _values); }
	#define glGetProgramStageiv glad_wrapped_glGetProgramStageiv
struct glad_tag_glIsProgram {};
inline GLboolean glad_wrapped_glIsProgram(GLuint _program) { return GLAD_INSTRUMENT_CALL(glIsProgram, _program); }
	#define glIsProgram glad_wrapped_glIsProgram
struct glad_tag_glVertexAttrib4bv {};
inline void glad_wrapped_glVertexAttrib4bv(GLuint _index, const GLbyte* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4bv, _index, _v); }
	#define glVertexAttrib4bv glad_wrapped_glVertexAttrib4bv
struct glad_tag_glTextureStorage3D {};
inline void glad_wrapped_glTextureStorage3D(GLuint _texture, GLsizei _levels, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth) { GLAD_INSTRUMENT_CALL(glTextureStorage3D, _texture, _levels, _internalformat, _width, _height, _depth); }
	#define glTextureStorage3D glad_wrapped_glTextureStorage3D
struct glad_tag_glUniformMatrix3x2dv {};
inline void glad_wrapped_glUniformMatrix3x2dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix3x2dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix3x2dv glad_wrapped_glUniformMatrix3x2dv
struct glad_tag_glVertexAttrib4fv {};
inline void glad_wrapped_glVertexAttrib4fv(GLuint _index, const GLfloat* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4fv, _index, _v); }
	#define glVertexAttrib4fv glad_wrapped_glVertexAttrib4fv
struct glad_tag_glProgramUniformMatrix2x3dv {};
inline void glad_wrapped_glProgramUniformMatrix2x3dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x3dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x3dv glad_wrapped_glProgramUniformMatrix2x3dv
struct glad_tag_glIsTransformFeedback {};
inline GLboolean glad_wrapped_glIsTransformFeedback(GLuint _id) { return GLAD_INSTRUMENT_CALL(glIsTransformFeedback, _id); }
	#define glIsTransformFeedback glad_wrapped_glIsTransformFeedback
struct glad_tag_glUniform4i {};
inline void glad_wrapped_glUniform4i(GLint _location, GLint _v0, GLint _v1, GLint _v2, GLint _v3) { GLAD_INSTRUMENT_CALL(glUniform4i, _location, _v0, _v1, _v2, _v3); }
	#define glUniform4i glad_wrapped_glUniform4i
struct glad_tag_glActiveTexture {};
inline void glad_wrapped_glActiveTexture(GLenum _texture) { GLAD_INSTRUMENT_CALL(glActiveTexture, _texture); }
	#define glActiveTexture glad_wrapped_glActiveTexture
struct glad_tag_glEnableVertexAttribArray {};
inline void glad_wrapped_glEnableVertexAttribArray(GLuint _index) { GLAD_INSTRUMENT_CALL(glEnableVertexAttribArray, _index); }
	#define glEnableVertexAttribArray glad_wrapped_glEnableVertexAttribArray
struct glad_tag_glIsProgramPipeline {};
inline GLboolean glad_wrapped_glIsProgramPipeline(GLuint _pipeline) { return GLAD_INSTRUMENT_CALL(glIsProgramPipeline, _pipeline); }
	#define glIsProgramPipeline glad_wrapped_glIsProgramPipeline
struct glad_tag_glReadPixels {};
inline void glad_wrapped_glReadPixels(GLint _x, GLint _y, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, void* _pixels) { GLAD_INSTRUMENT_CALL(glReadPixels, _x, _y, _width, _height, _format, _type, _pixels); }
	#define glReadPixels glad_wrapped_glReadPixels
struct glad_tag_glVertexAttribI3iv {};
inline void glad_wrapped_glVertexAttribI3iv(GLuint _index, const GLint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI3iv, _index, _v); }
	#define glVertexAttribI3iv glad_wrapped_glVertexAttribI3iv
struct glad_tag_glUniform4f {};
inline void glad_wrapped_glUniform4f(GLint _location, GLfloat _v0, GLfloat _v1, GLfloat _v2, GLfloat _v3) { GLAD_INSTRUMENT_CALL(glUniform4f, _location, _v0, _v1, _v2, _v3); }
	#define glUniform4f glad_wrapped_glUniform4f
struct glad_tag_glRenderbufferStorageMultisample {};
inline void glad_wrapped_glRenderbufferStorageMultisample(GLenum _target, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glRenderbufferStorageMultisample, _target, _samples, _internalformat, _width, _height); }
	#define glRenderbufferStorageMultisample glad_wrapped_glRenderbufferStorageMultisample
struct glad_tag_glCreateProgramPipelines {};
inline void glad_wrapped_glCreateProgramPipelines(GLsizei _n, GLuint* _pipelines) { GLAD_INSTRUMENT_CALL(glCreateProgramPipelines, _n, _pipelines); }
	#define glCreateProgramPipelines glad_wrapped_glCreateProgramPipelines
struct glad_tag_glUniformMatrix3fv {};
inline void glad_wrapped_glUniformMatrix3fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix3fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix3fv glad_wrapped_glUniformMatrix3fv
struct glad_tag_glVertexAttribLPointer {};
inline void glad_wrapped_glVertexAttribLPointer(GLuint _index, GLint _size, GLenum _type, GLsizei _stride, const void* _pointer) { GLAD_INSTRUMENT_CALL(glVertexAttribLPointer, _index, _size, _type, _stride, _pointer); }
	#define glVertexAttribLPointer glad_wrapped_glVertexAttribLPointer
struct glad_tag_glGetnUniformfv {};
inline void glad_wrapped_glGetnUniformfv(GLuint _program, GLint _location, GLsizei _bufSize, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetnUniformfv, _program, _location, _bufSize, _params); }
	#define glGetnUniformfv glad_wrapped_glGetnUniformfv
struct glad_tag_glDrawElementsInstancedBaseVertex {};
inline void glad_wrapped_glDrawElementsInstancedBaseVertex(GLenum _mode, GLsizei _count, GLenum _type, const void* _indices, GLsizei _instancecount, GLint _basevertex) { GLAD_INSTRUMENT_CALL(glDrawElementsInstancedBaseVertex, _mode, _count, _type, _indices, _instancecount, _basevertex); }
	#define glDrawElementsInstancedBaseVertex glad_wrapped_glDrawElementsInstancedBaseVertex
struct glad_tag_glVertexAttribL2dv {};
inline void glad_wrapped_glVertexAttribL2dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribL2dv, _index, _v); }
	#define glVertexAttribL2dv glad_wrapped_glVertexAttribL2dv
struct glad_tag_glEnableVertexArrayAttrib {};
inline void glad_wrapped_glEnableVertexArrayAttrib(GLuint _vaobj, GLuint _index) { GLAD_INSTRUMENT_CALL(glEnableVertexArrayAttrib, _vaobj, _index); }
	#define glEnableVertexArrayAttrib glad_wrapped_glEnableVertexArrayAttrib
struct glad_tag_glDrawTransformFeedbackStreamInstanced {};
inline void glad_wrapped_glDrawTransformFeedbackStreamInstanced(GLenum _mode, GLuint _id, GLuint _stream, GLsizei _instancecount) { GLAD_INSTRUMENT_CALL(glDrawTransformFeedbackStreamInstanced, _mode, _id, _stream, _instancecount); }
	#define glDrawTransformFeedbackStreamInstanced glad_wrapped_glDrawTransformFeedbackStreamInstanced
struct glad_tag_glGetActiveSubroutineName {};
inline void glad_wrapped_glGetActiveSubroutineName(GLuint _program, GLenum _shadertype, GLuint _index, GLsizei _bufsize, GLsizei* _length, GLchar* _name) { GLAD_INSTRUMENT_CALL(glGetActiveSubroutineName, _program, _shadertype, _index, _bufsize, _length, _name); }
	#define glGetActiveSubroutineName glad_wrapped_glGetActiveSubroutineName
struct glad_tag_glVertexAttribL2d {};
inline void glad_wrapped_glVertexAttribL2d(GLuint _index, GLdouble _x, GLdouble _y) { GLAD_INSTRUMENT_CALL(glVertexAttribL2d, _index, _x, _y); }
	#define glVertexAttribL2d glad_wrapped_glVertexAttribL2d
struct glad_tag_glStencilFunc {};
inline void glad_wrapped_glStencilFunc(GLenum _func, GLint _ref, GLuint _mask) { GLAD_INSTRUMENT_CALL(glStencilFunc, _func, _ref, _mask); }
	#define glStencilFunc glad_wrapped_glStencilFunc
struct glad_tag_glInvalidateNamedFramebufferData {};
inline void glad_wrapped_glInvalidateNamedFramebufferData(GLuint _framebuffer, GLsizei _numAttachments, const GLenum* _attachments) { GLAD_INSTRUMENT_CALL(glInvalidateNamedFramebufferData, _framebuffer, _numAttachments, _attachments); }
	#define glInvalidateNamedFramebufferData glad_wrapped_glInvalidateNamedFramebufferData
struct glad_tag_glPopDebugGroup {};
inline void glad_wrapped_glPopDebugGroup() { GLAD_INSTRUMENT_CALL(glPopDebugGroup); }
	#define glPopDebugGroup glad_wrapped_glPopDebugGroup
struct glad_tag_glUniformBlockBinding {};
inline void glad_wrapped_glUniformBlockBinding(GLuint _program, GLuint _uniformBlockIndex, GLuint _uniformBlockBinding) { GLAD_INSTRUMENT_CALL(glUniformBlockBinding, _program, _uniformBlockIndex, _uniformBlockBinding); }
	#define glUniformBlockBinding glad_wrapped_glUniformBlockBinding
struct glad_tag_glGetVertexArrayIndexediv {};
inline void glad_wrapped_glGetVertexArrayIndexediv(GLuint _vaobj, GLuint _index, GLenum _pname, GLint* _param) { GLAD_INSTRUMENT_CALL(glGetVertexArrayIndexediv, _vaobj, _index, _pname, _param); }
	#define glGetVertexArrayIndexediv glad_wrapped_glGetVertexArrayIndexediv
struct glad_tag_glColorP4ui {};
inline void glad_wrapped_glColorP4ui(GLenum _type, GLuint _color) { GLAD_INSTRUMENT_CALL(glColorP4ui, _type, _color); }
	#define glColorP4ui glad_wrapped_glColorP4ui
struct glad_tag_glUseProgramStages {};
inline void glad_wrapped_glUseProgramStages(GLuint _pipeline, GLbitfield _stages, GLuint _program) { GLAD_INSTRUMENT_CALL(glUseProgramStages, _pipeline, _stages, _program); }
	#define glUseProgramStages glad_wrapped_glUseProgramStages
struct glad_tag_glProgramUniform3f {};
inline void glad_wrapped_glProgramUniform3f(GLuint _program, GLint _location, GLfloat _v0, GLfloat _v1, GLfloat _v2) { GLAD_INSTRUMENT_CALL(glProgramUniform3f, _program, _location, _v0, _v1, _v2); }
	#define glProgramUniform3f glad_wrapped_glProgramUniform3f
struct glad_tag_glProgramUniform3d {};
inline void glad_wrapped_glProgramUniform3d(GLuint _program, GLint _location, GLdouble _v0, GLdouble _v1, GLdouble _v2) { GLAD_INSTRUMENT_CALL(glProgramUniform3d, _program, _location, _v0, _v1, _v2); }
	#define glProgramUniform3d glad_wrapped_glProgramUniform3d
struct glad_tag_glVertexAttribI4iv {};
inline void glad_wrapped_glVertexAttribI4iv(GLuint _index, const GLint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI4iv, _index, _v); }
	#define glVertexAttribI4iv glad_wrapped_glVertexAttribI4iv
struct glad_tag_glGetProgramPipelineiv {};
inline void glad_wrapped_glGetProgramPipelineiv(GLuint _pipeline, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetProgramPipelineiv, _pipeline, _pname, _params); }
	#define glGetProgramPipelineiv glad_wrapped_glGetProgramPipelineiv
struct glad_tag_glTexStorage3D {};
inline void glad_wrapped_glTexStorage3D(GLenum _target, GLsizei _levels, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth) { GLAD_INSTRUMENT_CALL(glTexStorage3D, _target, _levels, _internalformat, _width, _height, _depth); }
	#define glTexStorage3D glad_wrapped_glTexStorage3D
struct glad_tag_glNamedFramebufferDrawBuffer {};
inline void glad_wrapped_glNamedFramebufferDrawBuffer(GLuint _framebuffer, GLenum _buf) { GLAD_INSTRUMENT_CALL(glNamedFramebufferDrawBuffer, _framebuffer, _buf); }
	#define glNamedFramebufferDrawBuffer glad_wrapped_glNamedFramebufferDrawBuffer
struct glad_tag_glGetQueryIndexediv {};
inline void glad_wrapped_glGetQueryIndexediv(GLenum _target, GLuint _index, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetQueryIndexediv, _target, _index, _pname, _params); }
	#define glGetQueryIndexediv glad_wrapped_glGetQueryIndexediv
struct glad_tag_glGetShaderInfoLog {};
inline void glad_wrapped_glGetShaderInfoLog(GLuint _shader, GLsizei _bufSize, GLsizei* _length, GLchar* _infoLog) { GLAD_INSTRUMENT_CALL(glGetShaderInfoLog, _shader, _bufSize, _length, _infoLog); }
	#define glGetShaderInfoLog glad_wrapped_glGetShaderInfoLog
struct glad_tag_glObjectLabel {};
inline void glad_wrapped_glObjectLabel(GLenum _identifier, GLuint _name, GLsizei _length, const GLchar* _label) { GLAD_INSTRUMENT_CALL(glObjectLabel, _identifier, _name, _length, _label); }
	#define glObjectLabel glad_wrapped_glObjectLabel
struct glad_tag_glVertexAttribI4i {};
inline void glad_wrapped_glVertexAttribI4i(GLuint _index, GLint _x, GLint _y, GLint _z, GLint _w) { GLAD_INSTRUMENT_CALL(glVertexAttribI4i, _index, _x, _y, _z, _w); }
	#define glVertexAttribI4i glad_wrapped_glVertexAttribI4i
struct glad_tag_glGetBufferSubData {};
inline void glad_wrapped_glGetBufferSubData(GLenum _target, GLintptr _offset, GLsizeiptr _size, void* _data) { GLAD_INSTRUMENT_CALL(glGetBufferSubData, _target, _offset, _size, _data); }
	#define glGetBufferSubData glad_wrapped_glGetBufferSubData
struct glad_tag_glGetVertexAttribLdv {};
inline void glad_wrapped_glGetVertexAttribLdv(GLuint _index, GLenum _pname, GLdouble* _params) { GLAD_INSTRUMENT_CALL(glGetVertexAttribLdv, _index, _pname, _params); }
	#define glGetVertexAttribLdv glad_wrapped_glGetVertexAttribLdv
struct glad_tag_glGetnUniformuiv {};
inline void glad_wrapped_glGetnUniformuiv(GLuint _program, GLint _location, GLsizei _bufSize, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetnUniformuiv, _program, _location, _bufSize, _params); }
	#define glGetnUniformuiv glad_wrapped_glGetnUniformuiv
struct glad_tag_glGetQueryBufferObjectiv {};
inline void glad_wrapped_glGetQueryBufferObjectiv(GLuint _id, GLuint _buffer, GLenum _pname, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glGetQueryBufferObjectiv, _id, _buffer, _pname, _offset); }
	#define glGetQueryBufferObjectiv glad_wrapped_glGetQueryBufferObjectiv
struct glad_tag_glBlendEquationSeparate {};
inline void glad_wrapped_glBlendEquationSeparate(GLenum _modeRGB, GLenum _modeAlpha) { GLAD_INSTRUMENT_CALL(glBlendEquationSeparate, _modeRGB, _modeAlpha); }
	#define glBlendEquationSeparate glad_wrapped_glBlendEquationSeparate
struct glad_tag_glVertexAttribI1ui {};
inline void glad_wrapped_glVertexAttribI1ui(GLuint _index, GLuint _x) { GLAD_INSTRUMENT_CALL(glVertexAttribI1ui, _index, _x); }
	#define glVertexAttribI1ui glad_wrapped_glVertexAttribI1ui
struct glad_tag_glGenBuffers {};
inline void glad_wrapped_glGenBuffers(GLsizei _n, GLuint* _buffers) { GLAD_INSTRUMENT_CALL(glGenBuffers, _n, _buffers); }
	#define glGenBuffers glad_wrapped_glGenBuffers
struct glad_tag_glGetSubroutineIndex {};
inline GLuint glad_wrapped_glGetSubroutineIndex(GLuint _program, GLenum _shadertype, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetSubroutineIndex, _program, _shadertype, _name); }
	#define glGetSubroutineIndex glad_wrapped_glGetSubroutineIndex
struct glad_tag_glVertexAttrib2sv {};
inline void glad_wrapped_glVertexAttrib2sv(GLuint _index, const GLshort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib2sv, _index, _v); }
	#define glVertexAttrib2sv glad_wrapped_glVertexAttrib2sv
struct glad_tag_glGetnPolygonStipple {};
inline void glad_wrapped_glGetnPolygonStipple(GLsizei _bufSize, GLubyte* _pattern) { GLAD_INSTRUMENT_CALL(glGetnPolygonStipple, _bufSize, _pattern); }
	#define glGetnPolygonStipple glad_wrapped_glGetnPolygonStipple
struct glad_tag_glBlendFunc {};
inline void glad_wrapped_glBlendFunc(GLenum _sfactor, GLenum _dfactor) { GLAD_INSTRUMENT_CALL(glBlendFunc, _sfactor, _dfactor); }
	#define glBlendFunc glad_wrapped_glBlendFunc
struct glad_tag_glCreateProgram {};
inline GLuint glad_wrapped_glCreateProgram() { return GLAD_INSTRUMENT_CALL(glCreateProgram); }
	#define glCreateProgram glad_wrapped_glCreateProgram
struct glad_tag_glTexImage3D {};
inline void glad_wrapped_glTexImage3D(GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTexImage3D, _target, _level, _internalformat, _width, _height, _depth, _border, _format, _type, _pixels); }
	#define glTexImage3D glad_wrapped_glTexImage3D
struct glad_tag_glIsFramebuffer {};
inline GLboolean glad_wrapped_glIsFramebuffer(GLuint _framebuffer) { return GLAD_INSTRUMENT_CALL(glIsFramebuffer, _framebuffer); }
	#define glIsFramebuffer glad_wrapped_glIsFramebuffer
struct glad_tag_glClearNamedFramebufferfv {};
inline void glad_wrapped_glClearNamedFramebufferfv(GLuint _framebuffer, GLenum _buffer, GLint _drawbuffer, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glClearNamedFramebufferfv, _framebuffer, _buffer, _drawbuffer, _value); }
	#define glClearNamedFramebufferfv glad_wrapped_glClearNamedFramebufferfv
struct glad_tag_glGetNamedBufferSubData {};
inline void glad_wrapped_glGetNamedBufferSubData(GLuint _buffer, GLintptr _offset, GLsizeiptr _size, void* _data) { GLAD_INSTRUMENT_CALL(glGetNamedBufferSubData, _buffer, _offset, _size, _data); }
	#define glGetNamedBufferSubData glad_wrapped_glGetNamedBufferSubData
struct glad_tag_glPrimitiveRestartIndex {};
inline void glad_wrapped_glPrimitiveRestartIndex(GLuint _index) { GLAD_INSTRUMENT_CALL(glPrimitiveRestartIndex, _index); }
	#define glPrimitiveRestartIndex glad_wrapped_glPrimitiveRestartIndex
struct glad_tag_glFlushMappedNamedBufferRange {};
inline void glad_wrapped_glFlushMappedNamedBufferRange(GLuint _buffer, GLintptr _offset, GLsizeiptr _length) { GLAD_INSTRUMENT_CALL(glFlushMappedNamedBufferRange, _buffer, _offset, _length); }
	#define glFlushMappedNamedBufferRange glad_wrapped_glFlushMappedNamedBufferRange
struct glad_tag_glInvalidateTexSubImage {};
inline void glad_wrapped_glInvalidateTexSubImage(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth) { GLAD_INSTRUMENT_CALL(glInvalidateTexSubImage, _texture, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth); }
	#define glInvalidateTexSubImage glad_wrapped_glInvalidateTexSubImage
struct glad_tag_glBindImageTextures {};
inline void glad_wrapped_glBindImageTextures(GLuint _first, GLsizei _count, const GLuint* _textures) { GLAD_INSTRUMENT_CALL(glBindImageTextures, _first, _count, _textures); }
	#define glBindImageTextures glad_wrapped_glBindImageTextures
struct glad_tag_glGetInteger64v {};
inline void glad_wrapped_glGetInteger64v(GLenum _pname, GLint64* _data) { GLAD_INSTRUMENT_CALL(glGetInteger64v, _pname, _data); }
	#define glGetInteger64v glad_wrapped_glGetInteger64v
struct glad_tag_glBindProgramPipeline {};
inline void glad_wrapped_glBindProgramPipeline(GLuint _pipeline) { GLAD_INSTRUMENT_CALL(glBindProgramPipeline, _pipeline); }
	#define glBindProgramPipeline glad_wrapped_glBindProgramPipeline
struct glad_tag_glScissor {};
inline void glad_wrapped_glScissor(GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glScissor, _x, _y, _width, _height); }
	#define glScissor glad_wrapped_glScissor
struct glad_tag_glTexCoordP4uiv {};
inline void glad_wrapped_glTexCoordP4uiv(GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP4uiv, _type, _coords); }
	#define glTexCoordP4uiv glad_wrapped_glTexCoordP4uiv
struct glad_tag_glGetBooleanv {};
inline void glad_wrapped_glGetBooleanv(GLenum _pname, GLboolean* _data) { GLAD_INSTRUMENT_CALL(glGetBooleanv, _pname, _data); }
	#define glGetBooleanv glad_wrapped_glGetBooleanv
struct glad_tag_glNamedFramebufferRenderbuffer {};
inline void glad_wrapped_glNamedFramebufferRenderbuffer(GLuint _framebuffer, GLenum _attachment, GLenum _renderbuffertarget, GLuint _renderbuffer) { GLAD_INSTRUMENT_CALL(glNamedFramebufferRenderbuffer, _framebuffer, _attachment, _renderbuffertarget, _renderbuffer); }
	#define glNamedFramebufferRenderbuffer glad_wrapped_glNamedFramebufferRenderbuffer
struct glad_tag_glVertexP2uiv {};
inline void glad_wrapped_glVertexP2uiv(GLenum _type, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glVertexP2uiv, _type, _value); }
	#define glVertexP2uiv glad_wrapped_glVertexP2uiv
struct glad_tag_glUniform3uiv {};
inline void glad_wrapped_glUniform3uiv(GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glUniform3uiv, _location, _count, _value); }
	#define glUniform3uiv glad_wrapped_glUniform3uiv
struct glad_tag_glClearColor {};
inline void glad_wrapped_glClearColor(GLfloat _red, GLfloat _green, GLfloat _blue, GLfloat _alpha) { GLAD_INSTRUMENT_CALL(glClearColor, _red, _green, _blue, _alpha); }
	#define glClearColor glad_wrapped_glClearColor
struct glad_tag_glVertexAttrib4Niv {};
inline void glad_wrapped_glVertexAttrib4Niv(GLuint _index, const GLint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4Niv, _index, _v); }
	#define glVertexAttrib4Niv glad_wrapped_glVertexAttrib4Niv
struct glad_tag_glClearBufferiv {};
inline void glad_wrapped_glClearBufferiv(GLenum _buffer, GLint _drawbuffer, const GLint* _value) { GLAD_INSTRUMENT_CALL(glClearBufferiv, _buffer, _drawbuffer, _value); }
	#define glClearBufferiv glad_wrapped_glClearBufferiv
struct glad_tag_glGetBufferParameteri64v {};
inline void glad_wrapped_glGetBufferParameteri64v(GLenum _target, GLenum _pname, GLint64* _params) { GLAD_INSTRUMENT_CALL(glGetBufferParameteri64v, _target, _pname, _params); }
	#define glGetBufferParameteri64v glad_wrapped_glGetBufferParameteri64v
struct glad_tag_glProgramUniform4dv {};
inline void glad_wrapped_glProgramUniform4dv(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4dv, _program, _location, _count, _value); }
	#define glProgramUniform4dv glad_wrapped_glProgramUniform4dv
struct glad_tag_glColorP4uiv {};
inline void glad_wrapped_glColorP4uiv(GLenum _type, const GLuint* _color) { GLAD_INSTRUMENT_CALL(glColorP4uiv, _type, _color); }
	#define glColorP4uiv glad_wrapped_glColorP4uiv
struct glad_tag_glGetTextureLevelParameteriv {};
inline void glad_wrapped_glGetTextureLevelParameteriv(GLuint _texture, GLint _level, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureLevelParameteriv, _texture, _level, _pname, _params); }
	#define glGetTextureLevelParameteriv glad_wrapped_glGetTextureLevelParameteriv
struct glad_tag_glGetnUniformiv {};
inline void glad_wrapped_glGetnUniformiv(GLuint _program, GLint _location, GLsizei _bufSize, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetnUniformiv, _program, _location, _bufSize, _params); }
	#define glGetnUniformiv glad_wrapped_glGetnUniformiv
struct glad_tag_glVertexAttribI2uiv {};
inline void glad_wrapped_glVertexAttribI2uiv(GLuint _index, const GLuint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI2uiv, _index, _v); }
	#define glVertexAttribI2uiv glad_wrapped_glVertexAttribI2uiv
struct glad_tag_glUniform3ui {};
inline void glad_wrapped_glUniform3ui(GLint _location, GLuint _v0, GLuint _v1, GLuint _v2) { GLAD_INSTRUMENT_CALL(glUniform3ui, _location, _v0, _v1, _v2); }
	#define glUniform3ui glad_wrapped_glUniform3ui
struct glad_tag_glProgramUniform3uiv {};
inline void glad_wrapped_glProgramUniform3uiv(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3uiv, _program, _location, _count, _value); }
	#define glProgramUniform3uiv glad_wrapped_glProgramUniform3uiv
struct glad_tag_glVertexAttribI4uiv {};
inline void glad_wrapped_glVertexAttribI4uiv(GLuint _index, const GLuint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI4uiv, _index, _v); }
	#define glVertexAttribI4uiv glad_wrapped_glVertexAttribI4uiv
struct glad_tag_glPointParameterfv {};
inline void glad_wrapped_glPointParameterfv(GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glPointParameterfv, _pname, _params); }
	#define glPointParameterfv glad_wrapped_glPointParameterfv
struct glad_tag_glUniform2fv {};
inline void glad_wrapped_glUniform2fv(GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniform2fv, _location, _count, _value); }
	#define glUniform2fv glad_wrapped_glUniform2fv
struct glad_tag_glGetActiveSubroutineUniformName {};
inline void glad_wrapped_glGetActiveSubroutineUniformName(GLuint _program, GLenum _shadertype, GLuint _index, GLsizei _bufsize, GLsizei* _length, GLchar* _name) { GLAD_INSTRUMENT_CALL(glGetActiveSubroutineUniformName, _program, _shadertype, _index, _bufsize, _length, _name); }
	#define glGetActiveSubroutineUniformName glad_wrapped_glGetActiveSubroutineUniformName
struct glad_tag_glGetProgramResourceIndex {};
inline GLuint glad_wrapped_glGetProgramResourceIndex(GLuint _program, GLenum _programInterface, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetProgramResourceIndex, _program, _programInterface, _name); }
	#define glGetProgramResourceIndex glad_wrapped_glGetProgramResourceIndex
struct glad_tag_glDrawElementsIndirect {};
inline void glad_wrapped_glDrawElementsIndirect(GLenum _mode, GLenum _type, const void* _indirect) { GLAD_INSTRUMENT_CALL(glDrawElementsIndirect, _mode, _type, _indirect); }
	#define glDrawElementsIndirect glad_wrapped_glDrawElementsIndirect
struct glad_tag_glGetTextureLevelParameterfv {};
inline void glad_wrapped_glGetTextureLevelParameterfv(GLuint _texture, GLint _level, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetTextureLevelParameterfv, _texture, _level, _pname, _params); }
	#define glGetTextureLevelParameterfv glad_wrapped_glGetTextureLevelParameterfv
struct glad_tag_glGetNamedBufferPointerv {};
inline void glad_wrapped_glGetNamedBufferPointerv(GLuint _buffer, GLenum _pname, void** _params) { GLAD_INSTRUMENT_CALL(glGetNamedBufferPointerv, _buffer, _pname, _params); }
	#define glGetNamedBufferPointerv glad_wrapped_glGetNamedBufferPointerv
struct glad_tag_glDispatchComputeIndirect {};
inline void glad_wrapped_glDispatchComputeIndirect(GLintptr _indirect) { GLAD_INSTRUMENT_CALL(glDispatchComputeIndirect, _indirect); }
	#define glDispatchComputeIndirect glad_wrapped_glDispatchComputeIndirect
struct glad_tag_glInvalidateNamedFramebufferSubData {};
inline void glad_wrapped_glInvalidateNamedFramebufferSubData(GLuint _framebuffer, GLsizei _numAttachments, const GLenum* _attachments, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glInvalidateNamedFramebufferSubData, _framebuffer, _numAttachments, _attachments, _x, _y, _width, _height); }
	#define glInvalidateNamedFramebufferSubData glad_wrapped_glInvalidateNamedFramebufferSubData
struct glad_tag_glGetSamplerParameterIuiv {};
inline void glad_wrapped_glGetSamplerParameterIuiv(GLuint _sampler, GLenum _pname, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetSamplerParameterIuiv, _sampler, _pname, _params); }
	#define glGetSamplerParameterIuiv glad_wrapped_glGetSamplerParameterIuiv
struct glad_tag_glBindBufferRange {};
inline void glad_wrapped_glBindBufferRange(GLenum _target, GLuint _index, GLuint _buffer, GLintptr _offset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glBindBufferRange, _target, _index, _buffer, _offset, _size); }
	#define glBindBufferRange glad_wrapped_glBindBufferRange
struct glad_tag_glTextureSubImage1D {};
inline void glad_wrapped_glTextureSubImage1D(GLuint _texture, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureSubImage1D, _texture, _level, _xoffset, _width, _format, _type, _pixels); }
	#define glTextureSubImage1D glad_wrapped_glTextureSubImage1D
struct glad_tag_glVertexAttribL3dv {};
inline void glad_wrapped_glVertexAttribL3dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribL3dv, _index, _v); }
	#define glVertexAttribL3dv glad_wrapped_glVertexAttribL3dv
struct glad_tag_glGetUniformdv {};
inline void glad_wrapped_glGetUniformdv(GLuint _program, GLint _location, GLdouble* _params) { GLAD_INSTRUMENT_CALL(glGetUniformdv, _program, _location, _params); }
	#define glGetUniformdv glad_wrapped_glGetUniformdv
struct glad_tag_glGetQueryBufferObjectui64v {};
inline void glad_wrapped_glGetQueryBufferObjectui64v(GLuint _id, GLuint _buffer, GLenum _pname, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glGetQueryBufferObjectui64v, _id, _buffer, _pname, _offset); }
	#define glGetQueryBufferObjectui64v glad_wrapped_glGetQueryBufferObjectui64v
struct glad_tag_glClearDepthf {};
inline void glad_wrapped_glClearDepthf(GLfloat _d) { GLAD_INSTRUMENT_CALL(glClearDepthf, _d); }
	#define glClearDepthf glad_wrapped_glClearDepthf
struct glad_tag_glCreateRenderbuffers {};
inline void glad_wrapped_glCreateRenderbuffers(GLsizei _n, GLuint* _renderbuffers) { GLAD_INSTRUMENT_CALL(glCreateRenderbuffers, _n, _renderbuffers); }
	#define glCreateRenderbuffers glad_wrapped_glCreateRenderbuffers
struct glad_tag_glUniformMatrix2x3fv {};
inline void glad_wrapped_glUniformMatrix2x3fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix2x3fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix2x3fv glad_wrapped_glUniformMatrix2x3fv
struct glad_tag_glCreateTextures {};
inline void glad_wrapped_glCreateTextures(GLenum _target, GLsizei _n, GLuint* _textures) { GLAD_INSTRUMENT_CALL(glCreateTextures, _target, _n, _textures); }
	#define glCreateTextures glad_wrapped_glCreateTextures
struct glad_tag_glGenTransformFeedbacks {};
inline void glad_wrapped_glGenTransformFeedbacks(GLsizei _n, GLuint* _ids) { GLAD_INSTRUMENT_CALL(glGenTransformFeedbacks, _n, _ids); }
	#define glGenTransformFeedbacks glad_wrapped_glGenTransformFeedbacks
struct glad_tag_glGetVertexAttribIuiv {};
inline void glad_wrapped_glGetVertexAttribIuiv(GLuint _index, GLenum _pname, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetVertexAttribIuiv, _index, _pname, _params); }
	#define glGetVertexAttribIuiv glad_wrapped_glGetVertexAttribIuiv
struct glad_tag_glVertexAttrib4Nusv {};
inline void glad_wrapped_glVertexAttrib4Nusv(GLuint _index, const GLushort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4Nusv, _index, _v); }
	#define glVertexAttrib4Nusv glad_wrapped_glVertexAttrib4Nusv
struct glad_tag_glProgramUniformMatrix4x3dv {};
inline void glad_wrapped_glProgramUniformMatrix4x3dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x3dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x3dv glad_wrapped_glProgramUniformMatrix4x3dv
struct glad_tag_glDepthFunc {};
inline void glad_wrapped_glDepthFunc(GLenum _func) { GLAD_INSTRUMENT_CALL(glDepthFunc, _func); }
	#define glDepthFunc glad_wrapped_glDepthFunc
struct glad_tag_glCompressedTexSubImage2D {};
inline void glad_wrapped_glCompressedTexSubImage2D(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTexSubImage2D, _target, _level, _xoffset, _yoffset, _width, _height, _format, _imageSize, _data); }
	#define glCompressedTexSubImage2D glad_wrapped_glCompressedTexSubImage2D
struct glad_tag_glProgramBinary {};
inline void glad_wrapped_glProgramBinary(GLuint _program, GLenum _binaryFormat, const void* _binary, GLsizei _length) { GLAD_INSTRUMENT_CALL(glProgramBinary, _program, _binaryFormat, _binary, _length); }
	#define glProgramBinary glad_wrapped_glProgramBinary
struct glad_tag_glVertexAttribI4bv {};
inline void glad_wrapped_glVertexAttribI4bv(GLuint _index, const GLbyte* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI4bv, _index, _v); }
	#define glVertexAttribI4bv glad_wrapped_glVertexAttribI4bv
struct glad_tag_glGetTexParameterfv {};
inline void glad_wrapped_glGetTexParameterfv(GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetTexParameterfv, _target, _pname, _params); }
	#define glGetTexParameterfv glad_wrapped_glGetTexParameterfv
struct glad_tag_glMultiTexCoordP1ui {};
inline void glad_wrapped_glMultiTexCoordP1ui(GLenum _texture, GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glMultiTexCoordP1ui, _texture, _type, _coords); }
	#define glMultiTexCoordP1ui glad_wrapped_glMultiTexCoordP1ui
struct glad_tag_glBufferStorage {};
inline void glad_wrapped_glBufferStorage(GLenum _target, GLsizeiptr _size, const void* _data, GLbitfield _flags) { GLAD_INSTRUMENT_CALL(glBufferStorage, _target, _size, _data, _flags); }
	#define glBufferStorage glad_wrapped_glBufferStorage
struct glad_tag_glClientWaitSync {};
inline GLenum glad_wrapped_glClientWaitSync(GLsync _sync, GLbitfield _flags, GLuint64 _timeout) { return GLAD_INSTRUMENT_CALL(glClientWaitSync, _sync, _flags, _timeout); }
	#define glClientWaitSync glad_wrapped_glClientWaitSync
struct glad_tag_glVertexAttribI4ui {};
inline void glad_wrapped_glVertexAttribI4ui(GLuint _index, GLuint _x, GLuint _y, GLuint _z, GLuint _w) { GLAD_INSTRUMENT_CALL(glVertexAttribI4ui, _index, _x, _y, _z, _w); }
	#define glVertexAttribI4ui glad_wrapped_glVertexAttribI4ui
struct glad_tag_glGetFloati_v {};
inline void glad_wrapped_glGetFloati_v(GLenum _target, GLuint _index, GLfloat* _data) { GLAD_INSTRUMENT_CALL(glGetFloati_v, _target, _index, _data); }
	#define glGetFloati_v glad_wrapped_glGetFloati_v
struct glad_tag_glColorMask {};
inline void glad_wrapped_glColorMask(GLboolean _red, GLboolean _green, GLboolean _blue, GLboolean _alpha) { GLAD_INSTRUMENT_CALL(glColorMask, _red, _green, _blue, _alpha); }
	#define glColorMask glad_wrapped_glColorMask
struct glad_tag_glTextureBuffer {};
inline void glad_wrapped_glTextureBuffer(GLuint _texture, GLenum _internalformat, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glTextureBuffer, _texture, _internalformat, _buffer); }
	#define glTextureBuffer glad_wrapped_glTextureBuffer
struct glad_tag_glTexParameterIiv {};
inline void glad_wrapped_glTexParameterIiv(GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glTexParameterIiv, _target, _pname, _params); }
	#define glTexParameterIiv glad_wrapped_glTexParameterIiv
struct glad_tag_glBlendEquation {};
inline void glad_wrapped_glBlendEquation(GLenum _mode) { GLAD_INSTRUMENT_CALL(glBlendEquation, _mode); }
	#define glBlendEquation glad_wrapped_glBlendEquation
struct glad_tag_glGetUniformLocation {};
inline GLint glad_wrapped_glGetUniformLocation(GLuint _program, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetUniformLocation, _program, _name); }
	#define glGetUniformLocation glad_wrapped_glGetUniformLocation
struct glad_tag_glUniformMatrix2x4dv {};
inline void glad_wrapped_glUniformMatrix2x4dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix2x4dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix2x4dv glad_wrapped_glUniformMatrix2x4dv
struct glad_tag_glVertexArrayAttribFormat {};
inline void glad_wrapped_glVertexArrayAttribFormat(GLuint _vaobj, GLuint _attribindex, GLint _size, GLenum _type, GLboolean _normalized, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexArrayAttribFormat, _vaobj, _attribindex, _size, _type, _normalized, _relativeoffset); }
	#define glVertexArrayAttribFormat glad_wrapped_glVertexArrayAttribFormat
struct glad_tag_glReadnPixels {};
inline void glad_wrapped_glReadnPixels(GLint _x, GLint _y, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, GLsizei _bufSize, void* _data) { GLAD_INSTRUMENT_CALL(glReadnPixels, _x, _y, _width, _height, _format, _type, _bufSize, _data); }
	#define glReadnPixels glad_wrapped_glReadnPixels
struct glad_tag_glNamedFramebufferDrawBuffers {};
inline void glad_wrapped_glNamedFramebufferDrawBuffers(GLuint _framebuffer, GLsizei _n, const GLenum* _bufs) { GLAD_INSTRUMENT_CALL(glNamedFramebufferDrawBuffers, _framebuffer, _n, _bufs); }
	#define glNamedFramebufferDrawBuffers glad_wrapped_glNamedFramebufferDrawBuffers
struct glad_tag_glEndTransformFeedback {};
inline void glad_wrapped_glEndTransformFeedback() { GLAD_INSTRUMENT_CALL(glEndTransformFeedback); }
	#define glEndTransformFeedback glad_wrapped_glEndTransformFeedback
struct glad_tag_glVertexAttrib4usv {};
inline void glad_wrapped_glVertexAttrib4usv(GLuint _index, const GLushort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4usv, _index, _v); }
	#define glVertexAttrib4usv glad_wrapped_glVertexAttrib4usv
struct glad_tag_glGetUniformSubroutineuiv {};
inline void glad_wrapped_glGetUniformSubroutineuiv(GLenum _shadertype, GLint _location, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetUniformSubroutineuiv, _shadertype, _location, _params); }
	#define glGetUniformSubroutineuiv glad_wrapped_glGetUniformSubroutineuiv
struct glad_tag_glUniform4fv {};
inline void glad_wrapped_glUniform4fv(GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniform4fv, _location, _count, _value); }
	#define glUniform4fv glad_wrapped_glUniform4fv
struct glad_tag_glBindVertexBuffer {};
inline void glad_wrapped_glBindVertexBuffer(GLuint _bindingindex, GLuint _buffer, GLintptr _offset, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glBindVertexBuffer, _bindingindex, _buffer, _offset, _stride); }
	#define glBindVertexBuffer glad_wrapped_glBindVertexBuffer
struct glad_tag_glDebugMessageInsert {};
inline void glad_wrapped_glDebugMessageInsert(GLenum _source, GLenum _type, GLuint _id, GLenum _severity, GLsizei _length, const GLchar* _buf) { GLAD_INSTRUMENT_CALL(glDebugMessageInsert, _source, _type, _id, _severity, _length, _buf); }
	#define glDebugMessageInsert glad_wrapped_glDebugMessageInsert
struct glad_tag_glCreateSamplers {};
inline void glad_wrapped_glCreateSamplers(GLsizei _n, GLuint* _samplers) { GLAD_INSTRUMENT_CALL(glCreateSamplers, _n, _samplers); }
	#define glCreateSamplers glad_wrapped_glCreateSamplers
struct glad_tag_glGetProgramResourceiv {};
inline void glad_wrapped_glGetProgramResourceiv(GLuint _program, GLenum _programInterface, GLuint _index, GLsizei _propCount, const GLenum* _props, GLsizei _bufSize, GLsizei* _length, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetProgramResourceiv, _program, _programInterface, _index, _propCount, _props, _bufSize, _length, _params); }
	#define glGetProgramResourceiv glad_wrapped_glGetProgramResourceiv
struct glad_tag_glClearBufferData {};
inline void glad_wrapped_glClearBufferData(GLenum _target, GLenum _internalformat, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearBufferData, _target, _internalformat, _format, _type, _data); }
	#define glClearBufferData glad_wrapped_glClearBufferData
struct glad_tag_glBeginTransformFeedback {};
inline void glad_wrapped_glBeginTransformFeedback(GLenum _primitiveMode) { GLAD_INSTRUMENT_CALL(glBeginTransformFeedback, _primitiveMode); }
	#define glBeginTransformFeedback glad_wrapped_glBeginTransformFeedback
struct glad_tag_glVertexAttribI1iv {};
inline void glad_wrapped_glVertexAttribI1iv(GLuint _index, const GLint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI1iv, _index, _v); }
	#define glVertexAttribI1iv glad_wrapped_glVertexAttribI1iv
struct glad_tag_glIsSampler {};
inline GLboolean glad_wrapped_glIsSampler(GLuint _sampler) { return GLAD_INSTRUMENT_CALL(glIsSampler, _sampler); }
	#define glIsSampler glad_wrapped_glIsSampler
struct glad_tag_glVertexP3ui {};
inline void glad_wrapped_glVertexP3ui(GLenum _type, GLuint _value) { GLAD_INSTRUMENT_CALL(glVertexP3ui, _type, _value); }
	#define glVertexP3ui glad_wrapped_glVertexP3ui
struct glad_tag_glVertexAttribDivisor {};
inline void glad_wrapped_glVertexAttribDivisor(GLuint _index, GLuint _divisor) { GLAD_INSTRUMENT_CALL(glVertexAttribDivisor, _index, _divisor); }
	#define glVertexAttribDivisor glad_wrapped_glVertexAttribDivisor
struct glad_tag_glBindSamplers {};
inline void glad_wrapped_glBindSamplers(GLuint _first, GLsizei _count, const GLuint* _samplers) { GLAD_INSTRUMENT_CALL(glBindSamplers, _first, _count, _samplers); }
	#define glBindSamplers glad_wrapped_glBindSamplers
struct glad_tag_glCompressedTexImage1D {};
inline void glad_wrapped_glCompressedTexImage1D(GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLint _border, GLsizei _imageSize, const void* _data) { GLAD_INSTRUMENT_CALL(glCompressedTexImage1D, _target, _level, _internalformat, _width, _border, _imageSize, _data); }
	#define glCompressedTexImage1D glad_wrapped_glCompressedTexImage1D
struct glad_tag_glDeleteTransformFeedbacks {};
inline void glad_wrapped_glDeleteTransformFeedbacks(GLsizei _n, const GLuint* _ids) { GLAD_INSTRUMENT_CALL(glDeleteTransformFeedbacks, _n, _ids); }
	#define glDeleteTransformFeedbacks glad_wrapped_glDeleteTransformFeedbacks
struct glad_tag_glCopyTexSubImage1D {};
inline void glad_wrapped_glCopyTexSubImage1D(GLenum _target, GLint _level, GLint _xoffset, GLint _x, GLint _y, GLsizei _width) { GLAD_INSTRUMENT_CALL(glCopyTexSubImage1D, _target, _level, _xoffset, _x, _y, _width); }
	#define glCopyTexSubImage1D glad_wrapped_glCopyTexSubImage1D
struct glad_tag_glDrawRangeElementsBaseVertex {};
inline void glad_wrapped_glDrawRangeElementsBaseVertex(GLenum _mode, GLuint _start, GLuint _end, GLsizei _count, GLenum _type, const void* _indices, GLint _basevertex) { GLAD_INSTRUMENT_CALL(glDrawRangeElementsBaseVertex, _mode, _start, _end, _count, _type, _indices, _basevertex); }
	#define glDrawRangeElementsBaseVertex glad_wrapped_glDrawRangeElementsBaseVertex
struct glad_tag_glCheckFramebufferStatus {};
inline GLenum glad_wrapped_glCheckFramebufferStatus(GLenum _target) { return GLAD_INSTRUMENT_CALL(glCheckFramebufferStatus, _target); }
	#define glCheckFramebufferStatus glad_wrapped_glCheckFramebufferStatus
struct glad_tag_glEndConditionalRender {};
inline void glad_wrapped_glEndConditionalRender() { GLAD_INSTRUMENT_CALL(glEndConditionalRender); }
	#define glEndConditionalRender glad_wrapped_glEndConditionalRender
struct glad_tag_glVertexP3uiv {};
inline void glad_wrapped_glVertexP3uiv(GLenum _type, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glVertexP3uiv, _type, _value); }
	#define glVertexP3uiv glad_wrapped_glVertexP3uiv
struct glad_tag_glBindAttribLocation {};
inline void glad_wrapped_glBindAttribLocation(GLuint _program, GLuint _index, const GLchar* _name) { GLAD_INSTRUMENT_CALL(glBindAttribLocation, _program, _index, _name); }
	#define glBindAttribLocation glad_wrapped_glBindAttribLocation
struct glad_tag_glUniformMatrix4x2fv {};
inline void glad_wrapped_glUniformMatrix4x2fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix4x2fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix4x2fv glad_wrapped_glUniformMatrix4x2fv
struct glad_tag_glUniformMatrix2dv {};
inline void glad_wrapped_glUniformMatrix2dv(GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix2dv, _location, _count, _transpose, _value); }
	#define glUniformMatrix2dv glad_wrapped_glUniformMatrix2dv
struct glad_tag_glBlendFunci {};
inline void glad_wrapped_glBlendFunci(GLuint _buf, GLenum _src, GLenum _dst) { GLAD_INSTRUMENT_CALL(glBlendFunci, _buf, _src, _dst); }
	#define glBlendFunci glad_wrapped_glBlendFunci
struct glad_tag_glVertexAttrib1dv {};
inline void glad_wrapped_glVertexAttrib1dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib1dv, _index, _v); }
	#define glVertexAttrib1dv glad_wrapped_glVertexAttrib1dv
struct glad_tag_glDrawRangeElements {};
inline void glad_wrapped_glDrawRangeElements(GLenum _mode, GLuint _start, GLuint _end, GLsizei _count, GLenum _type, const void* _indices) { GLAD_INSTRUMENT_CALL(glDrawRangeElements, _mode, _start, _end, _count, _type, _indices); }
	#define glDrawRangeElements glad_wrapped_glDrawRangeElements
struct glad_tag_glGetQueryObjectuiv {};
inline void glad_wrapped_glGetQueryObjectuiv(GLuint _id, GLenum _pname, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetQueryObjectuiv, _id, _pname, _params); }
	#define glGetQueryObjectuiv glad_wrapped_glGetQueryObjectuiv
struct glad_tag_glBindBufferBase {};
inline void glad_wrapped_glBindBufferBase(GLenum _target, GLuint _index, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glBindBufferBase, _target, _index, _buffer); }
	#define glBindBufferBase glad_wrapped_glBindBufferBase
struct glad_tag_glBufferSubData {};
inline void glad_wrapped_glBufferSubData(GLenum _target, GLintptr _offset, GLsizeiptr _size, const void* _data) { GLAD_INSTRUMENT_CALL(glBufferSubData, _target, _offset, _size, _data); }
	#define glBufferSubData glad_wrapped_glBufferSubData
struct glad_tag_glVertexAttrib4iv {};
inline void glad_wrapped_glVertexAttrib4iv(GLuint _index, const GLint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4iv, _index, _v); }
	#define glVertexAttrib4iv glad_wrapped_glVertexAttrib4iv
struct glad_tag_glMapBufferRange {};
inline void* glad_wrapped_glMapBufferRange(GLenum _target, GLintptr _offset, GLsizeiptr _length, GLbitfield _access) { return GLAD_INSTRUMENT_CALL(glMapBufferRange, _target, _offset, _length, _access); }
	#define glMapBufferRange glad_wrapped_glMapBufferRange
struct glad_tag_glFramebufferTexture {};
inline void glad_wrapped_glFramebufferTexture(GLenum _target, GLenum _attachment, GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glFramebufferTexture, _target, _attachment, _texture, _level); }
	#define glFramebufferTexture glad_wrapped_glFramebufferTexture
struct glad_tag_glBlendFuncSeparatei {};
inline void glad_wrapped_glBlendFuncSeparatei(GLuint _buf, GLenum _srcRGB, GLenum _dstRGB, GLenum _srcAlpha, GLenum _dstAlpha) { GLAD_INSTRUMENT_CALL(glBlendFuncSeparatei, _buf, _srcRGB, _dstRGB, _srcAlpha, _dstAlpha); }
	#define glBlendFuncSeparatei glad_wrapped_glBlendFuncSeparatei
struct glad_tag_glProgramUniformMatrix4x2fv {};
inline void glad_wrapped_glProgramUniformMatrix4x2fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x2fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x2fv glad_wrapped_glProgramUniformMatrix4x2fv
struct glad_tag_glVertexAttribL1d {};
inline void glad_wrapped_glVertexAttribL1d(GLuint _index, GLdouble _x) { GLAD_INSTRUMENT_CALL(glVertexAttribL1d, _index, _x); }
	#define glVertexAttribL1d glad_wrapped_glVertexAttribL1d
struct glad_tag_glMultiDrawArrays {};
inline void glad_wrapped_glMultiDrawArrays(GLenum _mode, const GLint* _first, const GLsizei* _count, GLsizei _drawcount) { GLAD_INSTRUMENT_CALL(glMultiDrawArrays, _mode, _first, _count, _drawcount); }
	#define glMultiDrawArrays glad_wrapped_glMultiDrawArrays
struct glad_tag_glVertexP4uiv {};
inline void glad_wrapped_glVertexP4uiv(GLenum _type, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glVertexP4uiv, _type, _value); }
	#define glVertexP4uiv glad_wrapped_glVertexP4uiv
struct glad_tag_glVertexAttribI2iv {};
inline void glad_wrapped_glVertexAttribI2iv(GLuint _index, const GLint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI2iv, _index, _v); }
	#define glVertexAttribI2iv glad_wrapped_glVertexAttribI2iv
struct glad_tag_glGetShaderPrecisionFormat {};
inline void glad_wrapped_glGetShaderPrecisionFormat(GLenum _shadertype, GLenum _precisiontype, GLint* _range, GLint* _precision) { GLAD_INSTRUMENT_CALL(glGetShaderPrecisionFormat, _shadertype, _precisiontype, _range, _precision); }
	#define glGetShaderPrecisionFormat glad_wrapped_glGetShaderPrecisionFormat
struct glad_tag_glTextureView {};
inline void glad_wrapped_glTextureView(GLuint _texture, GLenum _target, GLuint _origtexture, GLenum _internalformat, GLuint _minlevel, GLuint _numlevels, GLuint _minlayer, GLuint _numlayers) { GLAD_INSTRUMENT_CALL(glTextureView, _texture, _target, _origtexture, _internalformat, _minlevel, _numlevels, _minlayer, _numlayers); }
	#define glTextureView glad_wrapped_glTextureView
struct glad_tag_glDisablei {};
inline void glad_wrapped_glDisablei(GLenum _target, GLuint _index) { GLAD_INSTRUMENT_CALL(glDisablei, _target, _index); }
	#define glDisablei glad_wrapped_glDisablei
struct glad_tag_glProgramUniformMatrix2x4fv {};
inline void glad_wrapped_glProgramUniformMatrix2x4fv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x4fv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x4fv glad_wrapped_glProgramUniformMatrix2x4fv
struct glad_tag_glShaderSource {};
inline void glad_wrapped_glShaderSource(GLuint _shader, GLsizei _count, const GLchar** _string, const GLint* _length) { GLAD_INSTRUMENT_CALL(glShaderSource, _shader, _count, _string, _length); }
	#define glShaderSource glad_wrapped_glShaderSource
struct glad_tag_glGetnSeparableFilter {};
inline void glad_wrapped_glGetnSeparableFilter(GLenum _target, GLenum _format, GLenum _type, GLsizei _rowBufSize, void* _row, GLsizei _columnBufSize, void* _column, void* _span) { GLAD_INSTRUMENT_CALL(glGetnSeparableFilter, _target, _format, _type, _rowBufSize, _row, _columnBufSize, _column, _span); }
	#define glGetnSeparableFilter glad_wrapped_glGetnSeparableFilter
struct glad_tag_glDeleteRenderbuffers {};
inline void glad_wrapped_glDeleteRenderbuffers(GLsizei _n, const GLuint* _renderbuffers) { GLAD_INSTRUMENT_CALL(glDeleteRenderbuffers, _n, _renderbuffers); }
	#define glDeleteRenderbuffers glad_wrapped_glDeleteRenderbuffers
struct glad_tag_glVertexAttribI3uiv {};
inline void glad_wrapped_glVertexAttribI3uiv(GLuint _index, const GLuint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI3uiv, _index, _v); }
	#define glVertexAttribI3uiv glad_wrapped_glVertexAttribI3uiv
struct glad_tag_glReleaseShaderCompiler {};
inline void glad_wrapped_glReleaseShaderCompiler() { GLAD_INSTRUMENT_CALL(glReleaseShaderCompiler); }
	#define glReleaseShaderCompiler glad_wrapped_glReleaseShaderCompiler
struct glad_tag_glVertexAttribIFormat {};
inline void glad_wrapped_glVertexAttribIFormat(GLuint _attribindex, GLint _size, GLenum _type, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexAttribIFormat, _attribindex, _size, _type, _relativeoffset); }
	#define glVertexAttribIFormat glad_wrapped_glVertexAttribIFormat
struct glad_tag_glCreateFramebuffers {};
inline void glad_wrapped_glCreateFramebuffers(GLsizei _n, GLuint* _framebuffers) { GLAD_INSTRUMENT_CALL(glCreateFramebuffers, _n, _framebuffers); }
	#define glCreateFramebuffers glad_wrapped_glCreateFramebuffers
struct glad_tag_glGetSynciv {};
inline void glad_wrapped_glGetSynciv(GLsync _sync, GLenum _pname, GLsizei _bufSize, GLsizei* _length, GLint* _values) { GLAD_INSTRUMENT_CALL(glGetSynciv, _sync, _pname, _bufSize, _length, _values); }
	#define glGetSynciv glad_wrapped_glGetSynciv
struct glad_tag_glGetnPixelMapfv {};
inline void glad_wrapped_glGetnPixelMapfv(GLenum _map, GLsizei _bufSize, GLfloat* _values) { GLAD_INSTRUMENT_CALL(glGetnPixelMapfv, _map, _bufSize, _values); }
	#define glGetnPixelMapfv glad_wrapped_glGetnPixelMapfv
struct glad_tag_glTexCoordP2uiv {};
inline void glad_wrapped_glTexCoordP2uiv(GLenum _type, const GLuint* _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP2uiv, _type, _coords); }
	#define glTexCoordP2uiv glad_wrapped_glTexCoordP2uiv
struct glad_tag_glPatchParameterfv {};
inline void glad_wrapped_glPatchParameterfv(GLenum _pname, const GLfloat* _values) { GLAD_INSTRUMENT_CALL(glPatchParameterfv, _pname, _values); }
	#define glPatchParameterfv glad_wrapped_glPatchParameterfv
struct glad_tag_glProgramUniform2i {};
inline void glad_wrapped_glProgramUniform2i(GLuint _program, GLint _location, GLint _v0, GLint _v1) { GLAD_INSTRUMENT_CALL(glProgramUniform2i, _program, _location, _v0, _v1); }
	#define glProgramUniform2i glad_wrapped_glProgramUniform2i
struct glad_tag_glGetNamedBufferParameteri64v {};
inline void glad_wrapped_glGetNamedBufferParameteri64v(GLuint _buffer, GLenum _pname, GLint64* _params) { GLAD_INSTRUMENT_CALL(glGetNamedBufferParameteri64v, _buffer, _pname, _params); }
	#define glGetNamedBufferParameteri64v glad_wrapped_glGetNamedBufferParameteri64v
struct glad_tag_glBeginQuery {};
inline void glad_wrapped_glBeginQuery(GLenum _target, GLuint _id) { GLAD_INSTRUMENT_CALL(glBeginQuery, _target, _id); }
	#define glBeginQuery glad_wrapped_glBeginQuery
struct glad_tag_glUniformMatrix4fv {};
inline void glad_wrapped_glUniformMatrix4fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix4fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix4fv glad_wrapped_glUniformMatrix4fv
struct glad_tag_glBindBuffer {};
inline void glad_wrapped_glBindBuffer(GLenum _target, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glBindBuffer, _target, _buffer); }
	#define glBindBuffer glad_wrapped_glBindBuffer
struct glad_tag_glTexStorage2DMultisample {};
inline void glad_wrapped_glTexStorage2DMultisample(GLenum _target, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTexStorage2DMultisample, _target, _samples, _internalformat, _width, _height, _fixedsamplelocations); }
	#define glTexStorage2DMultisample glad_wrapped_glTexStorage2DMultisample
struct glad_tag_glProgramUniform2d {};
inline void glad_wrapped_glProgramUniform2d(GLuint _program, GLint _location, GLdouble _v0, GLdouble _v1) { GLAD_INSTRUMENT_CALL(glProgramUniform2d, _program, _location, _v0, _v1); }
	#define glProgramUniform2d glad_wrapped_glProgramUniform2d
struct glad_tag_glProgramUniform2f {};
inline void glad_wrapped_glProgramUniform2f(GLuint _program, GLint _location, GLfloat _v0, GLfloat _v1) { GLAD_INSTRUMENT_CALL(glProgramUniform2f, _program, _location, _v0, _v1); }
	#define glProgramUniform2f glad_wrapped_glProgramUniform2f
struct glad_tag_glUniformMatrix2fv {};
inline void glad_wrapped_glUniformMatrix2fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix2fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix2fv glad_wrapped_glUniformMatrix2fv
struct glad_tag_glUniformMatrix2x4fv {};
inline void glad_wrapped_glUniformMatrix2x4fv(GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniformMatrix2x4fv, _location, _count, _transpose, _value); }
	#define glUniformMatrix2x4fv glad_wrapped_glUniformMatrix2x4fv
struct glad_tag_glBufferData {};
inline void glad_wrapped_glBufferData(GLenum _target, GLsizeiptr _size, const void* _data, GLenum _usage) { GLAD_INSTRUMENT_CALL(glBufferData, _target, _size, _data, _usage); }
	#define glBufferData glad_wrapped_glBufferData
struct glad_tag_glGetTexParameterIiv {};
inline void glad_wrapped_glGetTexParameterIiv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTexParameterIiv, _target, _pname, _params); }
	#define glGetTexParameterIiv glad_wrapped_glGetTexParameterIiv
struct glad_tag_glTexCoordP1ui {};
inline void glad_wrapped_glTexCoordP1ui(GLenum _type, GLuint _coords) { GLAD_INSTRUMENT_CALL(glTexCoordP1ui, _type, _coords); }
	#define glTexCoordP1ui glad_wrapped_glTexCoordP1ui
struct glad_tag_glGetError {};
inline GLenum glad_wrapped_glGetError() { return GLAD_INSTRUMENT_CALL(glGetError); }
	#define glGetError glad_wrapped_glGetError
struct glad_tag_glCreateTransformFeedbacks {};
inline void glad_wrapped_glCreateTransformFeedbacks(GLsizei _n, GLuint* _ids) { GLAD_INSTRUMENT_CALL(glCreateTransformFeedbacks, _n, _ids); }
	#define glCreateTransformFeedbacks glad_wrapped_glCreateTransformFeedbacks
struct glad_tag_glVertexAttribP2ui {};
inline void glad_wrapped_glVertexAttribP2ui(GLuint _index, GLenum _type, GLboolean _normalized, GLuint _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP2ui, _index, _type, _normalized, _value); }
	#define glVertexAttribP2ui glad_wrapped_glVertexAttribP2ui
struct glad_tag_glGetFloatv {};
inline void glad_wrapped_glGetFloatv(GLenum _pname, GLfloat* _data) { GLAD_INSTRUMENT_CALL(glGetFloatv, _pname, _data); }
	#define glGetFloatv glad_wrapped_glGetFloatv
struct glad_tag_glTexSubImage1D {};
inline void glad_wrapped_glTexSubImage1D(GLenum _target, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTexSubImage1D, _target, _level, _xoffset, _width, _format, _type, _pixels); }
	#define glTexSubImage1D glad_wrapped_glTexSubImage1D
struct glad_tag_glVertexAttrib2fv {};
inline void glad_wrapped_glVertexAttrib2fv(GLuint _index, const GLfloat* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib2fv, _index, _v); }
	#define glVertexAttrib2fv glad_wrapped_glVertexAttrib2fv
struct glad_tag_glGetTexLevelParameterfv {};
inline void glad_wrapped_glGetTexLevelParameterfv(GLenum _target, GLint _level, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetTexLevelParameterfv, _target, _level, _pname, _params); }
	#define glGetTexLevelParameterfv glad_wrapped_glGetTexLevelParameterfv
struct glad_tag_glVertexAttribI1i {};
inline void glad_wrapped_glVertexAttribI1i(GLuint _index, GLint _x) { GLAD_INSTRUMENT_CALL(glVertexAttribI1i, _index, _x); }
	#define glVertexAttribI1i glad_wrapped_glVertexAttribI1i
struct glad_tag_glVertexAttribP3uiv {};
inline void glad_wrapped_glVertexAttribP3uiv(GLuint _index, GLenum _type, GLboolean _normalized, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glVertexAttribP3uiv, _index, _type, _normalized, _value); }
	#define glVertexAttribP3uiv glad_wrapped_glVertexAttribP3uiv
struct glad_tag_glUniform4d {};
inline void glad_wrapped_glUniform4d(GLint _location, GLdouble _x, GLdouble _y, GLdouble _z, GLdouble _w) { GLAD_INSTRUMENT_CALL(glUniform4d, _location, _x, _y, _z, _w); }
	#define glUniform4d glad_wrapped_glUniform4d
struct glad_tag_glSecondaryColorP3uiv {};
inline void glad_wrapped_glSecondaryColorP3uiv(GLenum _type, const GLuint* _color) { GLAD_INSTRUMENT_CALL(glSecondaryColorP3uiv, _type, _color); }
	#define glSecondaryColorP3uiv glad_wrapped_glSecondaryColorP3uiv
struct glad_tag_glGetIntegerv {};
inline void glad_wrapped_glGetIntegerv(GLenum _pname, GLint* _data) { GLAD_INSTRUMENT_CALL(glGetIntegerv, _pname, _data); }
	#define glGetIntegerv glad_wrapped_glGetIntegerv
struct glad_tag_glGetVertexArrayIndexed64iv {};
inline void glad_wrapped_glGetVertexArrayIndexed64iv(GLuint _vaobj, GLuint _index, GLenum _pname, GLint64* _param) { GLAD_INSTRUMENT_CALL(glGetVertexArrayIndexed64iv, _vaobj, _index, _pname, _param); }
	#define glGetVertexArrayIndexed64iv glad_wrapped_glGetVertexArrayIndexed64iv
struct glad_tag_glGetBufferPointerv {};
inline void glad_wrapped_glGetBufferPointerv(GLenum _target, GLenum _pname, void** _params) { GLAD_INSTRUMENT_CALL(glGetBufferPointerv, _target, _pname, _params); }
	#define glGetBufferPointerv glad_wrapped_glGetBufferPointerv
struct glad_tag_glProgramUniformMatrix3dv {};
inline void glad_wrapped_glProgramUniformMatrix3dv(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3dv, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3dv glad_wrapped_glProgramUniformMatrix3dv
struct glad_tag_glFramebufferTexture3D {};
inline void glad_wrapped_glFramebufferTexture3D(GLenum _target, GLenum _attachment, GLenum _textarget, GLuint _texture, GLint _level, GLint _zoffset) { GLAD_INSTRUMENT_CALL(glFramebufferTexture3D, _target, _attachment, _textarget, _texture, _level, _zoffset); }
	#define glFramebufferTexture3D glad_wrapped_glFramebufferTexture3D
struct glad_tag_glIsQuery {};
inline GLboolean glad_wrapped_glIsQuery(GLuint _id) { return GLAD_INSTRUMENT_CALL(glIsQuery, _id); }
	#define glIsQuery glad_wrapped_glIsQuery
struct glad_tag_glProgramUniform2ui {};
inline void glad_wrapped_glProgramUniform2ui(GLuint _program, GLint _location, GLuint _v0, GLuint _v1) { GLAD_INSTRUMENT_CALL(glProgramUniform2ui, _program, _location, _v0, _v1); }
	#define glProgramUniform2ui glad_wrapped_glProgramUniform2ui
struct glad_tag_glProgramUniform4ui {};
inline void glad_wrapped_glProgramUniform4ui(GLuint _program, GLint _location, GLuint _v0, GLuint _v1, GLuint _v2, GLuint _v3) { GLAD_INSTRUMENT_CALL(glProgramUniform4ui, _program, _location, _v0, _v1, _v2, _v3); }
	#define glProgramUniform4ui glad_wrapped_glProgramUniform4ui
struct glad_tag_glVertexAttrib4sv {};
inline void glad_wrapped_glVertexAttrib4sv(GLuint _index, const GLshort* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4sv, _index, _v); }
	#define glVertexAttrib4sv glad_wrapped_glVertexAttrib4sv
struct glad_tag_glTexImage2D {};
inline void glad_wrapped_glTexImage2D(GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLsizei _height, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTexImage2D, _target, _level, _internalformat, _width, _height, _border, _format, _type, _pixels); }
	#define glTexImage2D glad_wrapped_glTexImage2D
struct glad_tag_glStencilMask {};
inline void glad_wrapped_glStencilMask(GLuint _mask) { GLAD_INSTRUMENT_CALL(glStencilMask, _mask); }
	#define glStencilMask glad_wrapped_glStencilMask
struct glad_tag_glSamplerParameterfv {};
inline void glad_wrapped_glSamplerParameterfv(GLuint _sampler, GLenum _pname, const GLfloat* _param) { GLAD_INSTRUMENT_CALL(glSamplerParameterfv, _sampler, _pname, _param); }
	#define glSamplerParameterfv glad_wrapped_glSamplerParameterfv
struct glad_tag_glIsTexture {};
inline GLboolean glad_wrapped_glIsTexture(GLuint _texture) { return GLAD_INSTRUMENT_CALL(glIsTexture, _texture); }
	#define glIsTexture glad_wrapped_glIsTexture
struct glad_tag_glNamedBufferData {};
inline void glad_wrapped_glNamedBufferData(GLuint _buffer, GLsizeiptr _size, const void* _data, GLenum _usage) { GLAD_INSTRUMENT_CALL(glNamedBufferData, _buffer, _size, _data, _usage); }
	#define glNamedBufferData glad_wrapped_glNamedBufferData
struct glad_tag_glUniform1fv {};
inline void glad_wrapped_glUniform1fv(GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glUniform1fv, _location, _count, _value); }
	#define glUniform1fv glad_wrapped_glUniform1fv
struct glad_tag_glVertexAttrib4Nubv {};
inline void glad_wrapped_glVertexAttrib4Nubv(GLuint _index, const GLubyte* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib4Nubv, _index, _v); }
	#define glVertexAttrib4Nubv glad_wrapped_glVertexAttrib4Nubv
struct glad_tag_glClearNamedBufferSubData {};
inline void glad_wrapped_glClearNamedBufferSubData(GLuint _buffer, GLenum _internalformat, GLintptr _offset, GLsizeiptr _size, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearNamedBufferSubData, _buffer, _internalformat, _offset, _size, _format, _type, _data); }
	#define glClearNamedBufferSubData glad_wrapped_glClearNamedBufferSubData
struct glad_tag_glTexParameterfv {};
inline void glad_wrapped_glTexParameterfv(GLenum _target, GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glTexParameterfv, _target, _pname, _params); }
	#define glTexParameterfv glad_wrapped_glTexParameterfv
struct glad_tag_glScissorIndexedv {};
inline void glad_wrapped_glScissorIndexedv(GLuint _index, const GLint* _v) { GLAD_INSTRUMENT_CALL(glScissorIndexedv, _index, _v); }
	#define glScissorIndexedv glad_wrapped_glScissorIndexedv
struct glad_tag_glUniform3dv {};
inline void glad_wrapped_glUniform3dv(GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glUniform3dv, _location, _count, _value); }
	#define glUniform3dv glad_wrapped_glUniform3dv
struct glad_tag_glGetnPixelMapuiv {};
inline void glad_wrapped_glGetnPixelMapuiv(GLenum _map, GLsizei _bufSize, GLuint* _values) { GLAD_INSTRUMENT_CALL(glGetnPixelMapuiv, _map, _bufSize, _values); }
	#define glGetnPixelMapuiv glad_wrapped_glGetnPixelMapuiv
struct glad_tag_glProgramUniform3fv {};
inline void glad_wrapped_glProgramUniform3fv(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3fv, _program, _location, _count, _value); }
	#define glProgramUniform3fv glad_wrapped_glProgramUniform3fv
struct glad_tag_glGetSubroutineUniformLocation {};
inline GLint glad_wrapped_glGetSubroutineUniformLocation(GLuint _program, GLenum _shadertype, const GLchar* _name) { return GLAD_INSTRUMENT_CALL(glGetSubroutineUniformLocation, _program, _shadertype, _name); }
	#define glGetSubroutineUniformLocation glad_wrapped_glGetSubroutineUniformLocation
struct glad_tag_glGetFramebufferParameteriv {};
inline void glad_wrapped_glGetFramebufferParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetFramebufferParameteriv, _target, _pname, _params); }
	#define glGetFramebufferParameteriv glad_wrapped_glGetFramebufferParameteriv
struct glad_tag_glGetSamplerParameteriv {};
inline void glad_wrapped_glGetSamplerParameteriv(GLuint _sampler, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetSamplerParameteriv, _sampler, _pname, _params); }
	#define glGetSamplerParameteriv glad_wrapped_glGetSamplerParameteriv
struct glad_tag_glGetCompressedTextureSubImage {};
inline void glad_wrapped_glGetCompressedTextureSubImage(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLsizei _bufSize, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetCompressedTextureSubImage, _texture, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _bufSize, _pixels); }
	#define glGetCompressedTextureSubImage glad_wrapped_glGetCompressedTextureSubImage
struct glad_tag_glCopyBufferSubData {};
inline void glad_wrapped_glCopyBufferSubData(GLenum _readTarget, GLenum _writeTarget, GLintptr _readOffset, GLintptr _writeOffset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glCopyBufferSubData, _readTarget, _writeTarget, _readOffset, _writeOffset, _size); }
	#define glCopyBufferSubData glad_wrapped_glCopyBufferSubData
struct glad_tag_glVertexAttribI1uiv {};
inline void glad_wrapped_glVertexAttribI1uiv(GLuint _index, const GLuint* _v) { GLAD_INSTRUMENT_CALL(glVertexAttribI1uiv, _index, _v); }
	#define glVertexAttribI1uiv glad_wrapped_glVertexAttribI1uiv
struct glad_tag_glVertexAttrib2d {};
inline void glad_wrapped_glVertexAttrib2d(GLuint _index, GLdouble _x, GLdouble _y) { GLAD_INSTRUMENT_CALL(glVertexAttrib2d, _index, _x, _y); }
	#define glVertexAttrib2d glad_wrapped_glVertexAttrib2d
struct glad_tag_glVertexAttrib2f {};
inline void glad_wrapped_glVertexAttrib2f(GLuint _index, GLfloat _x, GLfloat _y) { GLAD_INSTRUMENT_CALL(glVertexAttrib2f, _index, _x, _y); }
	#define glVertexAttrib2f glad_wrapped_glVertexAttrib2f
struct glad_tag_glVertexAttrib3dv {};
inline void glad_wrapped_glVertexAttrib3dv(GLuint _index, const GLdouble* _v) { GLAD_INSTRUMENT_CALL(glVertexAttrib3dv, _index, _v); }
	#define glVertexAttrib3dv glad_wrapped_glVertexAttrib3dv
struct glad_tag_glGetQueryObjectui64v {};
inline void glad_wrapped_glGetQueryObjectui64v(GLuint _id, GLenum _pname, GLuint64* _params) { GLAD_INSTRUMENT_CALL(glGetQueryObjectui64v, _id, _pname, _params); }
	#define glGetQueryObjectui64v glad_wrapped_glGetQueryObjectui64v
struct glad_tag_glDepthMask {};
inline void glad_wrapped_glDepthMask(GLboolean _flag) { GLAD_INSTRUMENT_CALL(glDepthMask, _flag); }
	#define glDepthMask glad_wrapped_glDepthMask
struct glad_tag_glVertexAttrib2s {};
inline void glad_wrapped_glVertexAttrib2s(GLuint _index, GLshort _x, GLshort _y) { GLAD_INSTRUMENT_CALL(glVertexAttrib2s, _index, _x, _y); }
	#define glVertexAttrib2s glad_wrapped_glVertexAttrib2s
struct glad_tag_glTexImage3DMultisample {};
inline void glad_wrapped_glTexImage3DMultisample(GLenum _target, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTexImage3DMultisample, _target, _samples, _internalformat, _width, _height, _depth, _fixedsamplelocations); }
	#define glTexImage3DMultisample glad_wrapped_glTexImage3DMultisample
struct glad_tag_glProgramUniform1fv {};
inline void glad_wrapped_glProgramUniform1fv(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1fv, _program, _location, _count, _value); }
	#define glProgramUniform1fv glad_wrapped_glProgramUniform1fv
struct glad_tag_glGetUniformBlockIndex {};
inline GLuint glad_wrapped_glGetUniformBlockIndex(GLuint _program, const GLchar* _uniformBlockName) { return GLAD_INSTRUMENT_CALL(glGetUniformBlockIndex, _program, _uniformBlockName); }
	#define glGetUniformBlockIndex glad_wrapped_glGetUniformBlockIndex
struct glad_tag_glTexImage2DMultisample {};
inline void glad_wrapped_glTexImage2DMultisample(GLenum _target, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTexImage2DMultisample, _target, _samples, _internalformat, _width, _height, _fixedsamplelocations); }
	#define glTexImage2DMultisample glad_wrapped_glTexImage2DMultisample
struct glad_tag_glGetActiveUniform {};
inline void glad_wrapped_glGetActiveUniform(GLuint _program, GLuint _index, GLsizei _bufSize, GLsizei* _length, GLint* _size, GLenum* _type, GLchar* _name) { GLAD_INSTRUMENT_CALL(glGetActiveUniform, _program, _index, _bufSize, _length, _size, _type, _name); }
	#define glGetActiveUniform glad_wrapped_glGetActiveUniform
struct glad_tag_glFrontFace {};
inline void glad_wrapped_glFrontFace(GLenum _mode) { GLAD_INSTRUMENT_CALL(glFrontFace, _mode); }
	#define glFrontFace glad_wrapped_glFrontFace
struct glad_tag_glTexBufferRange {};
inline void glad_wrapped_glTexBufferRange(GLenum _target, GLenum _internalformat, GLuint _buffer, GLintptr _offset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glTexBufferRange, _target, _internalformat, _buffer, _offset, _size); }
	#define glTexBufferRange glad_wrapped_glTexBufferRange
struct glad_tag_glNamedFramebufferTextureLayer {};
inline void glad_wrapped_glNamedFramebufferTextureLayer(GLuint _framebuffer, GLenum _attachment, GLuint _texture, GLint _level, GLint _layer) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTextureLayer, _framebuffer, _attachment, _texture, _level, _layer); }
	#define glNamedFramebufferTextureLayer glad_wrapped_glNamedFramebufferTextureLayer
struct glad_tag_glNamedFramebufferParameteri {};
inline void glad_wrapped_glNamedFramebufferParameteri(GLuint _framebuffer, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glNamedFramebufferParameteri, _framebuffer, _pname, _param); }
	#define glNamedFramebufferParameteri glad_wrapped_glNamedFramebufferParameteri
struct glad_tag_glDrawArraysInstancedBaseInstance {};
inline void glad_wrapped_glDrawArraysInstancedBaseInstance(GLenum _mode, GLint _first, GLsizei _count, GLsizei _instancecount, GLuint _baseinstance) { GLAD_INSTRUMENT_CALL(glDrawArraysInstancedBaseInstance, _mode, _first, _count, _instancecount, _baseinstance); }
	#define glDrawArraysInstancedBaseInstance glad_wrapped_glDrawArraysInstancedBaseInstance
struct glad_tag_glDeleteProgramPipelines {};
inline void glad_wrapped_glDeleteProgramPipelines(GLsizei _n, const GLuint* _pipelines) { GLAD_INSTRUMENT_CALL(glDeleteProgramPipelines, _n, _pipelines); }
	#define glDeleteProgramPipelines                           glad_wrapped_glDeleteProgramPipelines
#else
	#define glCopyTexImage1D                                   glad_glCopyTexImage1D
	#define glVertexAttribI3ui                                 glad_glVertexAttribI3ui
	#define glVertexArrayElementBuffer                         glad_glVertexArrayElementBuffer
	#define glStencilMaskSeparate                              glad_glStencilMaskSeparate
	#define glTextureStorage3DMultisample                      glad_glTextureStorage3DMultisample
	#define glTextureParameterfv                               glad_glTextureParameterfv
	#define glMinSampleShading                                 glad_glMinSampleShading
	#define glFramebufferRenderbuffer                          glad_glFramebufferRenderbuffer
	#define glUniformSubroutinesuiv                            glad_glUniformSubroutinesuiv
	#define glCompressedTexSubImage3D                          glad_glCompressedTexSubImage3D
	#define glTexCoordP3uiv                                    glad_glTexCoordP3uiv
	#define glGetDoublei_v                                     glad_glGetDoublei_v
	#define glVertexAttrib1sv                                  glad_glVertexAttrib1sv
	#define glVertexArrayVertexBuffers                         glad_glVertexArrayVertexBuffers
	#define glBindSampler                                      glad_glBindSampler
	#define glLineWidth                                        glad_glLineWidth
	#define glColorP3uiv                                       glad_glColorP3uiv
	#define glGetIntegeri_v                                    glad_glGetIntegeri_v
	#define glCompileShader                                    glad_glCompileShader
	#define glGetTransformFeedbackVarying                      glad_glGetTransformFeedbackVarying
	#define glCompressedTextureSubImage3D                      glad_glCompressedTextureSubImage3D
	#define glGetCompressedTextureImage                        glad_glGetCompressedTextureImage
	#define glGetnMapfv                                        glad_glGetnMapfv
	#define glTransformFeedbackBufferRange                     glad_glTransformFeedbackBufferRange
	#define glGetTextureImage                                  glad_glGetTextureImage
	#define glDepthRangef                                      glad_glDepthRangef
	#define glVertexAttribIPointer                             glad_glVertexAttribIPointer
	#define glMultiTexCoordP3ui                                glad_glMultiTexCoordP3ui
	#define glGetNamedBufferParameteriv                        glad_glGetNamedBufferParameteriv
	#define glVertexP4ui                                       glad_glVertexP4ui
	#define glDrawElementsInstancedBaseInstance                glad_glDrawElementsInstancedBaseInstance
	#define glEnablei                                          glad_glEnablei
	#define glVertexAttribP4ui                                 glad_glVertexAttribP4ui
	#define glCreateShader                                     glad_glCreateShader
	#define glIsBuffer                                         glad_glIsBuffer
	#define glGetMultisamplefv                                 glad_glGetMultisamplefv
	#define glProgramUniformMatrix2dv                          glad_glProgramUniformMatrix2dv
	#define glGenRenderbuffers                                 glad_glGenRenderbuffers
	#define glCopyTexSubImage2D                                glad_glCopyTexSubImage2D
	#define glCompressedTexImage2D                             glad_glCompressedTexImage2D
	#define glVertexAttrib1f                                   glad_glVertexAttrib1f
	#define glBlendFuncSeparate                                glad_glBlendFuncSeparate
	#define glProgramUniformMatrix4fv                          glad_glProgramUniformMatrix4fv
	#define glClearNamedFramebufferfi                          glad_glClearNamedFramebufferfi
	#define glGetQueryBufferObjectuiv                          glad_glGetQueryBufferObjectuiv
	#define glHint                                             glad_glHint
	#define glVertexAttrib1s                                   glad_glVertexAttrib1s
	#define glSampleMaski                                      glad_glSampleMaski
	#define glVertexP2ui                                       glad_glVertexP2ui
	#define glUniformMatrix3x2fv                               glad_glUniformMatrix3x2fv
	#define glDebugMessageControl                              glad_glDebugMessageControl
	#define glPointSize                                        glad_glPointSize
	#define glBindTextureUnit                                  glad_glBindTextureUnit
	#define glVertexAttrib2dv                                  glad_glVertexAttrib2dv
	#define glDeleteProgram                                    glad_glDeleteProgram
	#define glVertexAttrib4Nuiv                                glad_glVertexAttrib4Nuiv
	#define glTexStorage2D                                     glad_glTexStorage2D
	#define glRenderbufferStorage                              glad_glRenderbufferStorage
	#define glWaitSync                                         glad_glWaitSync
	#define glUniformMatrix4x3fv                               glad_glUniformMatrix4x3fv
	#define glUniform3i                                        glad_glUniform3i
	#define glClearBufferfv                                    glad_glClearBufferfv
	#define glProgramUniform1ui                                glad_glProgramUniform1ui
	#define glBlendEquationSeparatei                           glad_glBlendEquationSeparatei
	#define glGetnMapiv                                        glad_glGetnMapiv
	#define glTextureBarrier                                   glad_glTextureBarrier
	#define glUniform3d                                        glad_glUniform3d
	#define glUniform3f                                        glad_glUniform3f
	#define glVertexAttrib4ubv                                 glad_glVertexAttrib4ubv
	#define glGetBufferParameteriv                             glad_glGetBufferParameteriv
	#define glTexCoordP2ui                                     glad_glTexCoordP2ui
	#define glColorMaski                                       glad_glColorMaski
	#define glClearBufferfi                                    glad_glClearBufferfi
	#define glDrawArraysIndirect                               glad_glDrawArraysIndirect
	#define glGenVertexArrays                                  glad_glGenVertexArrays
	#define glPauseTransformFeedback                           glad_glPauseTransformFeedback
	#define glMultiTexCoordP2ui                                glad_glMultiTexCoordP2ui
	#define glProgramUniformMatrix3x2dv                        glad_glProgramUniformMatrix3x2dv
	#define glCopyNamedBufferSubData                           glad_glCopyNamedBufferSubData
	#define glProgramUniformMatrix3x2fv                        glad_glProgramUniformMatrix3x2fv
	#define glGetSamplerParameterIiv                           glad_glGetSamplerParameterIiv
	#define glGetFragDataIndex                                 glad_glGetFragDataIndex
	#define glVertexAttribL4d                                  glad_glVertexAttribL4d
	#define glBindImageTexture                                 glad_glBindImageTexture
	#define glTextureParameteriv                               glad_glTextureParameteriv
	#define glGetQueryBufferObjecti64v                         glad_glGetQueryBufferObjecti64v
	#define glGetVertexAttribdv                                glad_glGetVertexAttribdv
	#define glActiveShaderProgram                              glad_glActiveShaderProgram
	#define glUniformMatrix3x4fv                               glad_glUniformMatrix3x4fv
	#define glUniformMatrix3dv                                 glad_glUniformMatrix3dv
	#define glProgramUniformMatrix3x4dv                        glad_glProgramUniformMatrix3x4dv
	#define glNamedFramebufferTexture                          glad_glNamedFramebufferTexture
	#define glGetTextureParameterfv                            glad_glGetTextureParameterfv
	#define glInvalidateBufferSubData                          glad_glInvalidateBufferSubData
	#define glResumeTransformFeedback                          glad_glResumeTransformFeedback
	#define glMultiTexCoordP4ui                                glad_glMultiTexCoordP4ui
	#define glProgramUniformMatrix4x3fv                        glad_glProgramUniformMatrix4x3fv
	#define glViewportArrayv                                   glad_glViewportArrayv
	#define glDeleteFramebuffers                               glad_glDeleteFramebuffers
	#define glDrawArrays                                       glad_glDrawArrays
	#define glUniform1ui                                       glad_glUniform1ui
	#define glProgramUniform2uiv                               glad_glProgramUniform2uiv
	#define glVertexAttribI2i                                  glad_glVertexAttribI2i
	#define glTexCoordP3ui                                     glad_glTexCoordP3ui
	#define glVertexAttrib3d                                   glad_glVertexAttrib3d
	#define glClear                                            glad_glClear
	#define glProgramParameteri                                glad_glProgramParameteri
	#define glGetActiveUniformName                             glad_glGetActiveUniformName
	#define glMemoryBarrier                                    glad_glMemoryBarrier
	#define glGetGraphicsResetStatus                           glad_glGetGraphicsResetStatus
	#define glIsEnabled                                        glad_glIsEnabled
	#define glStencilOp                                        glad_glStencilOp
	#define glFramebufferTexture2D                             glad_glFramebufferTexture2D
	#define glGetFramebufferAttachmentParameteriv              glad_glGetFramebufferAttachmentParameteriv
	#define glVertexAttrib4Nub                                 glad_glVertexAttrib4Nub
	#define glMapNamedBufferRange                              glad_glMapNamedBufferRange
	#define glGetFragDataLocation                              glad_glGetFragDataLocation
	#define glGetTextureParameterIiv                           glad_glGetTextureParameterIiv
	#define glTexImage1D                                       glad_glTexImage1D
	#define glTexParameteriv                                   glad_glTexParameteriv
	#define glVertexArrayAttribIFormat                         glad_glVertexArrayAttribIFormat
	#define glVertexArrayVertexBuffer                          glad_glVertexArrayVertexBuffer
	#define glGetTexImage                                      glad_glGetTexImage
	#define glGetQueryObjecti64v                               glad_glGetQueryObjecti64v
	#define glGenFramebuffers                                  glad_glGenFramebuffers
	#define glTransformFeedbackBufferBase                      glad_glTransformFeedbackBufferBase
	#define glClearTexSubImage                                 glad_glClearTexSubImage
	#define glGetAttachedShaders                               glad_glGetAttachedShaders
	#define glIsRenderbuffer                                   glad_glIsRenderbuffer
	#define glDeleteVertexArrays                               glad_glDeleteVertexArrays
	#define glBindVertexBuffers                                glad_glBindVertexBuffers
	#define glProgramUniform1uiv                               glad_glProgramUniform1uiv
	#define glIsVertexArray                                    glad_glIsVertexArray
	#define glDisableVertexAttribArray                         glad_glDisableVertexAttribArray
	#define glProgramUniform2iv                                glad_glProgramUniform2iv
	#define glGetQueryiv                                       glad_glGetQueryiv
	#define glGetTransformFeedbackiv                           glad_glGetTransformFeedbackiv
	#define glBlitNamedFramebuffer                             glad_glBlitNamedFramebuffer
	#define glVertexArrayAttribLFormat                         glad_glVertexArrayAttribLFormat
	#define glCreateQueries                                    glad_glCreateQueries
	#define glGetSamplerParameterfv                            glad_glGetSamplerParameterfv
	#define glShaderStorageBlockBinding                        glad_glShaderStorageBlockBinding
	#define glProgramUniformMatrix4x2dv                        glad_glProgramUniformMatrix4x2dv
	#define glGetUniformIndices                                glad_glGetUniformIndices
	#define glIsShader                                         glad_glIsShader
	#define glVertexAttribI4ubv                                glad_glVertexAttribI4ubv
	#define glBeginQueryIndexed                                glad_glBeginQueryIndexed
	#define glPointParameteriv                                 glad_glPointParameteriv
	#define glEnable                                           glad_glEnable
	#define glGetActiveUniformsiv                              glad_glGetActiveUniformsiv
	#define glVertexArrayAttribBinding                         glad_glVertexArrayAttribBinding
	#define glTextureStorage1D                                 glad_glTextureStorage1D
	#define glMemoryBarrierByRegion                            glad_glMemoryBarrierByRegion
	#define glBlendEquationi                                   glad_glBlendEquationi
	#define glGetAttribLocation                                glad_glGetAttribLocation
	#define glVertexAttrib4dv                                  glad_glVertexAttrib4dv
	#define glGetTextureParameteriv                            glad_glGetTextureParameteriv
	#define glGetProgramInterfaceiv                            glad_glGetProgramInterfaceiv
	#define glUniform2dv                                       glad_glUniform2dv
	#define glMapNamedBuffer                                   glad_glMapNamedBuffer
	#define glMultiTexCoordP3uiv                               glad_glMultiTexCoordP3uiv
	#define glVertexAttribP3ui                                 glad_glVertexAttribP3ui
	#define glVertexAttribL1dv                                 glad_glVertexAttribL1dv
	#define glTextureBufferRange                               glad_glTextureBufferRange
	#define glGetnUniformdv                                    glad_glGetnUniformdv
	#define glProgramUniform3ui                                glad_glProgramUniform3ui
	#define glVertexBindingDivisor                             glad_glVertexBindingDivisor
	#define glGetUniformfv                                     glad_glGetUniformfv
	#define glGetUniformuiv                                    glad_glGetUniformuiv
	#define glProgramUniformMatrix2x3fv                        glad_glProgramUniformMatrix2x3fv
	#define glGetVertexAttribIiv                               glad_glGetVertexAttribIiv
	#define glVertexArrayBindingDivisor                        glad_glVertexArrayBindingDivisor
	#define glDrawBuffer                                       glad_glDrawBuffer
	#define glEndQueryIndexed                                  glad_glEndQueryIndexed
	#define glGetnPixelMapusv                                  glad_glGetnPixelMapusv
	#define glClearBufferuiv                                   glad_glClearBufferuiv
	#define glDrawElementsInstanced                            glad_glDrawElementsInstanced
	#define glProgramUniform1i                                 glad_glProgramUniform1i
	#define glPatchParameteri                                  glad_glPatchParameteri
	#define glProgramUniform1d                                 glad_glProgramUniform1d
	#define glProgramUniform1f                                 glad_glProgramUniform1f
	#define glGetNamedFramebufferParameteriv                   glad_glGetNamedFramebufferParameteriv
	#define glFlush                                            glad_glFlush
	#define glGetRenderbufferParameteriv                       glad_glGetRenderbufferParameteriv
	#define glProgramUniform3iv                                glad_glProgramUniform3iv
	#define glGetDebugMessageLog                               glad_glGetDebugMessageLog
	#define glNamedRenderbufferStorage                         glad_glNamedRenderbufferStorage
	#define glGetNamedFramebufferAttachmentParameteriv         glad_glGetNamedFramebufferAttachmentParameteriv
	#define glGetVertexAttribPointerv                          glad_glGetVertexAttribPointerv
	#define glFenceSync                                        glad_glFenceSync
	#define glColorP3ui                                        glad_glColorP3ui
	#define glDrawElementsInstancedBaseVertexBaseInstance      glad_glDrawElementsInstancedBaseVertexBaseInstance
	#define glVertexAttrib3sv                                  glad_glVertexAttrib3sv
	#define glBeginConditionalRender                           glad_glBeginConditionalRender
	#define glValidateProgramPipeline                          glad_glValidateProgramPipeline
	#define glGetnMinmax                                       glad_glGetnMinmax
	#define glGetTexLevelParameteriv                           glad_glGetTexLevelParameteriv
	#define glMultiTexCoordP4uiv                               glad_glMultiTexCoordP4uiv
	#define glTexStorage3DMultisample                          glad_glTexStorage3DMultisample
	#define glStencilFuncSeparate                              glad_glStencilFuncSeparate
	#define glDisableVertexArrayAttrib                         glad_glDisableVertexArrayAttrib
	#define glGenSamplers                                      glad_glGenSamplers
	#define glClampColor                                       glad_glClampColor
	#define glUniform4iv                                       glad_glUniform4iv
	#define glClearStencil                                     glad_glClearStencil
	#define glTexCoordP1uiv                                    glad_glTexCoordP1uiv
	#define glGetNamedRenderbufferParameteriv                  glad_glGetNamedRenderbufferParameteriv
	#define glDrawTransformFeedbackInstanced                   glad_glDrawTransformFeedbackInstanced
	#define glGenTextures                                      glad_glGenTextures
	#define glTextureStorage2DMultisample                      glad_glTextureStorage2DMultisample
	#define glDrawTransformFeedback                            glad_glDrawTransformFeedback
	#define glUniform1dv                                       glad_glUniform1dv
	#define glGetTexParameterIuiv                              glad_glGetTexParameterIuiv
	#define glGetTransformFeedbacki_v                          glad_glGetTransformFeedbacki_v
	#define glVertexAttrib4Nbv                                 glad_glVertexAttrib4Nbv
	#define glClearNamedFramebufferuiv                         glad_glClearNamedFramebufferuiv
	#define glIsSync                                           glad_glIsSync
	#define glClearNamedFramebufferiv                          glad_glClearNamedFramebufferiv
	#define glGetActiveUniformBlockName                        glad_glGetActiveUniformBlockName
	#define glUniform2i                                        glad_glUniform2i
	#define glUniform2f                                        glad_glUniform2f
	#define glUniform2d                                        glad_glUniform2d
	#define glTexCoordP4ui                                     glad_glTexCoordP4ui
	#define glGetProgramiv                                     glad_glGetProgramiv
	#define glVertexAttribPointer                              glad_glVertexAttribPointer
	#define glFramebufferTextureLayer                          glad_glFramebufferTextureLayer
	#define glProgramUniform4fv                                glad_glProgramUniform4fv
	#define glGetObjectPtrLabel                                glad_glGetObjectPtrLabel
	#define glTextureParameteri                                glad_glTextureParameteri
	#define glTextureParameterf                                glad_glTextureParameterf
	#define glFlushMappedBufferRange                           glad_glFlushMappedBufferRange
	#define glProgramUniform2fv                                glad_glProgramUniform2fv
	#define glUniformMatrix2x3dv                               glad_glUniformMatrix2x3dv
	#define glProgramUniformMatrix4dv                          glad_glProgramUniformMatrix4dv
	#define glVertexAttribL3d                                  glad_glVertexAttribL3d
	#define glProgramUniformMatrix2x4dv                        glad_glProgramUniformMatrix2x4dv
	#define glDispatchCompute                                  glad_glDispatchCompute
	#define glGenQueries                                       glad_glGenQueries
	#define glVertexAttribP1ui                                 glad_glVertexAttribP1ui
	#define glTexSubImage3D                                    glad_glTexSubImage3D
	#define glGetInteger64i_v                                  glad_glGetInteger64i_v
	#define glDeleteSamplers                                   glad_glDeleteSamplers
	#define glCopyTexImage2D                                   glad_glCopyTexImage2D
	#define glGetTextureSubImage                               glad_glGetTextureSubImage
	#define glBlitFramebuffer                                  glad_glBlitFramebuffer
	#define glIsEnabledi                                       glad_glIsEnabledi
	#define glBindBuffersRange                                 glad_glBindBuffersRange
	#define glSecondaryColorP3ui                               glad_glSecondaryColorP3ui
	#define glBindFragDataLocationIndexed                      glad_glBindFragDataLocationIndexed
	#define glCopyImageSubData                                 glad_glCopyImageSubData
	#define glUniform2iv                                       glad_glUniform2iv
	#define glVertexAttrib1fv                                  glad_glVertexAttrib1fv
	#define glUniform4uiv                                      glad_glUniform4uiv
	#define glProgramUniform2dv                                glad_glProgramUniform2dv
	#define glTextureSubImage3D                                glad_glTextureSubImage3D
	#define glFramebufferTexture1D                             glad_glFramebufferTexture1D
	#define glGetShaderiv                                      glad_glGetShaderiv
	#define glProgramUniformMatrix3fv                          glad_glProgramUniformMatrix3fv
	#define glObjectPtrLabel                                   glad_glObjectPtrLabel
	#define glInvalidateFramebuffer                            glad_glInvalidateFramebuffer
	#define glBindTextures                                     glad_glBindTextures
	#define glBindFragDataLocation                             glad_glBindFragDataLocation
	#define glNamedBufferStorage                               glad_glNamedBufferStorage
	#define glScissorArrayv                                    glad_glScissorArrayv
	#define glPolygonOffset                                    glad_glPolygonOffset
	#define glGetDoublev                                       glad_glGetDoublev
	#define glVertexAttrib1d                                   glad_glVertexAttrib1d
	#define glUniform4dv                                       glad_glUniform4dv
	#define glProgramUniform3dv                                glad_glProgramUniform3dv
	#define glGetUniformiv                                     glad_glGetUniformiv
	#define glInvalidateBufferData                             glad_glInvalidateBufferData
	#define glGetnColorTable                                   glad_glGetnColorTable
	#define glCompressedTextureSubImage1D                      glad_glCompressedTextureSubImage1D
	#define glMultiTexCoordP1uiv                               glad_glMultiTexCoordP1uiv
	#define glUniform3fv                                       glad_glUniform3fv
	#define glMultiDrawElementsIndirect                        glad_glMultiDrawElementsIndirect
	#define glDepthRange                                       glad_glDepthRange
	#define glInvalidateSubFramebuffer                         glad_glInvalidateSubFramebuffer
	#define glMapBuffer                                        glad_glMapBuffer
	#define glClearTexImage                                    glad_glClearTexImage
	#define glVertexAttribLFormat                              glad_glVertexAttribLFormat
	#define glCompressedTexImage3D                             glad_glCompressedTexImage3D
	#define glDeleteSync                                       glad_glDeleteSync
	#define glCopyTexSubImage3D                                glad_glCopyTexSubImage3D
	#define glGetTransformFeedbacki64_v                        glad_glGetTransformFeedbacki64_v
	#define glUniformMatrix4dv                                 glad_glUniformMatrix4dv
	#define glGetVertexAttribiv                                glad_glGetVertexAttribiv
	#define glUniformMatrix4x2dv                               glad_glUniformMatrix4x2dv
	#define glMultiDrawElements                                glad_glMultiDrawElements
	#define glVertexAttrib3fv                                  glad_glVertexAttrib3fv
	#define glUniform3iv                                       glad_glUniform3iv
	#define glPolygonMode                                      glad_glPolygonMode
	#define glDrawBuffers                                      glad_glDrawBuffers
	#define glGetnHistogram                                    glad_glGetnHistogram
	#define glGetActiveUniformBlockiv                          glad_glGetActiveUniformBlockiv
	#define glNamedFramebufferReadBuffer                       glad_glNamedFramebufferReadBuffer
	#define glProgramUniform4iv                                glad_glProgramUniform4iv
	#define glGetProgramBinary                                 glad_glGetProgramBinary
	#define glUseProgram                                       glad_glUseProgram
	#define glGetProgramInfoLog                                glad_glGetProgramInfoLog
	#define glBindTransformFeedback                            glad_glBindTransformFeedback
	#define glBindVertexArray                                  glad_glBindVertexArray
	#define glDeleteBuffers                                    glad_glDeleteBuffers
	#define glGenerateTextureMipmap                            glad_glGenerateTextureMipmap
	#define glSamplerParameterIiv                              glad_glSamplerParameterIiv
	#define glMultiDrawElementsBaseVertex                      glad_glMultiDrawElementsBaseVertex
	#define glNamedBufferSubData                               glad_glNamedBufferSubData
	#define glTextureStorage2D                                 glad_glTextureStorage2D
	#define glGetnConvolutionFilter                            glad_glGetnConvolutionFilter
	#define glUniform2uiv                                      glad_glUniform2uiv
	#define glCompressedTexSubImage1D                          glad_glCompressedTexSubImage1D
	#define glFinish                                           glad_glFinish
	#define glDepthRangeIndexed                                glad_glDepthRangeIndexed
	#define glDeleteShader                                     glad_glDeleteShader
	#define glGetInternalformati64v                            glad_glGetInternalformati64v
	#define glCopyTextureSubImage1D                            glad_glCopyTextureSubImage1D
	#define glPushDebugGroup                                   glad_glPushDebugGroup
	#define glVertexAttrib4Nsv                                 glad_glVertexAttrib4Nsv
	#define glGetProgramResourceLocationIndex                  glad_glGetProgramResourceLocationIndex
	#define glTextureParameterIuiv                             glad_glTextureParameterIuiv
	#define glViewport                                         glad_glViewport
	#define glUniform1uiv                                      glad_glUniform1uiv
	#define glTransformFeedbackVaryings                        glad_glTransformFeedbackVaryings
	#define glUniform2ui                                       glad_glUniform2ui
	#define glGetnMapdv                                        glad_glGetnMapdv
	#define glDebugMessageCallback                             glad_glDebugMessageCallback
	#define glVertexAttribI3i                                  glad_glVertexAttribI3i
	#define glInvalidateTexImage                               glad_glInvalidateTexImage
	#define glVertexAttribFormat                               glad_glVertexAttribFormat
	#define glClearDepth                                       glad_glClearDepth
	#define glVertexAttribI4usv                                glad_glVertexAttribI4usv
	#define glTexParameterf                                    glad_glTexParameterf
	#define glVertexAttribBinding                              glad_glVertexAttribBinding
	#define glTexParameteri                                    glad_glTexParameteri
	#define glGetActiveSubroutineUniformiv                     glad_glGetActiveSubroutineUniformiv
	#define glGetShaderSource                                  glad_glGetShaderSource
	#define glGetnTexImage                                     glad_glGetnTexImage
	#define glTexBuffer                                        glad_glTexBuffer
	#define glPixelStorei                                      glad_glPixelStorei
	#define glValidateProgram                                  glad_glValidateProgram
	#define glPixelStoref                                      glad_glPixelStoref
	#define glCreateBuffers                                    glad_glCreateBuffers
	#define glGetBooleani_v                                    glad_glGetBooleani_v
	#define glClipControl                                      glad_glClipControl
	#define glMultiTexCoordP2uiv                               glad_glMultiTexCoordP2uiv
	#define glGenProgramPipelines                              glad_glGenProgramPipelines
	#define glGetInternalformativ                              glad_glGetInternalformativ
	#define glCopyTextureSubImage3D                            glad_glCopyTextureSubImage3D
	#define glVertexAttribP1uiv                                glad_glVertexAttribP1uiv
	#define glLinkProgram                                      glad_glLinkProgram
	#define glBindTexture                                      glad_glBindTexture
	#define glMultiDrawArraysIndirect                          glad_glMultiDrawArraysIndirect
	#define glGetObjectLabel                                   glad_glGetObjectLabel
	#define glGetProgramPipelineInfoLog                        glad_glGetProgramPipelineInfoLog
	#define glGetString                                        glad_glGetString
	#define glVertexAttribP2uiv                                glad_glVertexAttribP2uiv
	#define glDetachShader                                     glad_glDetachShader
	#define glProgramUniform3i                                 glad_glProgramUniform3i
	#define glUniformMatrix3x4dv                               glad_glUniformMatrix3x4dv
	#define glEndQuery                                         glad_glEndQuery
	#define glNormalP3ui                                       glad_glNormalP3ui
	#define glFramebufferParameteri                            glad_glFramebufferParameteri
	#define glGetProgramResourceName                           glad_glGetProgramResourceName
	#define glUniformMatrix4x3dv                               glad_glUniformMatrix4x3dv
	#define glDepthRangeArrayv                                 glad_glDepthRangeArrayv
	#define glVertexAttribI2ui                                 glad_glVertexAttribI2ui
	#define glGetProgramResourceLocation                       glad_glGetProgramResourceLocation
	#define glDeleteTextures                                   glad_glDeleteTextures
	#define glGetActiveAtomicCounterBufferiv                   glad_glGetActiveAtomicCounterBufferiv
	#define glStencilOpSeparate                                glad_glStencilOpSeparate
	#define glDeleteQueries                                    glad_glDeleteQueries
	#define glNormalP3uiv                                      glad_glNormalP3uiv
	#define glVertexAttrib4f                                   glad_glVertexAttrib4f
	#define glVertexAttrib4d                                   glad_glVertexAttrib4d
	#define glViewportIndexedfv                                glad_glViewportIndexedfv
	#define glBindBuffersBase                                  glad_glBindBuffersBase
	#define glVertexAttribL4dv                                 glad_glVertexAttribL4dv
	#define glGetTexParameteriv                                glad_glGetTexParameteriv
	#define glCreateVertexArrays                               glad_glCreateVertexArrays
	#define glProgramUniform1dv                                glad_glProgramUniform1dv
	#define glVertexAttrib4s                                   glad_glVertexAttrib4s
	#define glDrawElementsBaseVertex                           glad_glDrawElementsBaseVertex
	#define glSampleCoverage                                   glad_glSampleCoverage
	#define glSamplerParameteri                                glad_glSamplerParameteri
	#define glClearBufferSubData                               glad_glClearBufferSubData
	#define glSamplerParameterf                                glad_glSamplerParameterf
	#define glTexStorage1D                                     glad_glTexStorage1D
	#define glUniform1f                                        glad_glUniform1f
	#define glGetVertexAttribfv                                glad_glGetVertexAttribfv
	#define glUniform1d                                        glad_glUniform1d
	#define glGetCompressedTexImage                            glad_glGetCompressedTexImage
	#define glGetnCompressedTexImage                           glad_glGetnCompressedTexImage
	#define glUniform1i                                        glad_glUniform1i
	#define glGetActiveAttrib                                  glad_glGetActiveAttrib
	#define glTexSubImage2D                                    glad_glTexSubImage2D
	#define glDisable                                          glad_glDisable
	#define glLogicOp                                          glad_glLogicOp
	#define glProgramUniformMatrix3x4fv                        glad_glProgramUniformMatrix3x4fv
	#define glGetTextureParameterIuiv                          glad_glGetTextureParameterIuiv
	#define glProgramUniform4uiv                               glad_glProgramUniform4uiv
	#define glUniform4ui                                       glad_glUniform4ui
	#define glCopyTextureSubImage2D                            glad_glCopyTextureSubImage2D
	#define glBindFramebuffer                                  glad_glBindFramebuffer
	#define glCullFace                                         glad_glCullFace
	#define glProgramUniform4i                                 glad_glProgramUniform4i
	#define glProgramUniform4f                                 glad_glProgramUniform4f
	#define glViewportIndexedf                                 glad_glViewportIndexedf
	#define glProgramUniform4d                                 glad_glProgramUniform4d
	#define glTextureParameterIiv                              glad_glTextureParameterIiv
	#define glGetStringi                                       glad_glGetStringi
	#define glTextureSubImage2D                                glad_glTextureSubImage2D
	#define glScissorIndexed                                   glad_glScissorIndexed
	#define glDrawTransformFeedbackStream                      glad_glDrawTransformFeedbackStream
	#define glAttachShader                                     glad_glAttachShader
	#define glQueryCounter                                     glad_glQueryCounter
	#define glProvokingVertex                                  glad_glProvokingVertex
	#define glShaderBinary                                     glad_glShaderBinary
	#define glUnmapNamedBuffer                                 glad_glUnmapNamedBuffer
	#define glDrawElements                                     glad_glDrawElements
	#define glNamedRenderbufferStorageMultisample              glad_glNamedRenderbufferStorageMultisample
	#define glVertexAttribI4sv                                 glad_glVertexAttribI4sv
	#define glClearNamedBufferData                             glad_glClearNamedBufferData
	#define glUniform1iv                                       glad_glUniform1iv
	#define glCreateShaderProgramv                             glad_glCreateShaderProgramv
	#define glGetQueryObjectiv                                 glad_glGetQueryObjectiv
	#define glReadBuffer                                       glad_glReadBuffer
	#define glTexParameterIuiv                                 glad_glTexParameterIuiv
	#define glDrawArraysInstanced                              glad_glDrawArraysInstanced
	#define glGenerateMipmap                                   glad_glGenerateMipmap
	#define glCompressedTextureSubImage2D                      glad_glCompressedTextureSubImage2D
	#define glProgramUniformMatrix2fv                          glad_glProgramUniformMatrix2fv
	#define glSamplerParameteriv                               glad_glSamplerParameteriv
	#define glVertexAttrib3f                                   glad_glVertexAttrib3f
	#define glVertexAttrib4uiv                                 glad_glVertexAttrib4uiv
	#define glPointParameteri                                  glad_glPointParameteri
	#define glBlendColor                                       glad_glBlendColor
	#define glSamplerParameterIuiv                             glad_glSamplerParameterIuiv
	#define glCheckNamedFramebufferStatus                      glad_glCheckNamedFramebufferStatus
	#define glUnmapBuffer                                      glad_glUnmapBuffer
	#define glPointParameterf                                  glad_glPointParameterf
	#define glProgramUniform1iv                                glad_glProgramUniform1iv
	#define glGetVertexArrayiv                                 glad_glGetVertexArrayiv
	#define glVertexAttrib3s                                   glad_glVertexAttrib3s
	#define glBindRenderbuffer                                 glad_glBindRenderbuffer
	#define glVertexAttribP4uiv                                glad_glVertexAttribP4uiv
	#define glGetProgramStageiv                                glad_glGetProgramStageiv
	#define glIsProgram                                        glad_glIsProgram
	#define glVertexAttrib4bv                                  glad_glVertexAttrib4bv
	#define glTextureStorage3D                                 glad_glTextureStorage3D
	#define glUniformMatrix3x2dv                               glad_glUniformMatrix3x2dv
	#define glVertexAttrib4fv                                  glad_glVertexAttrib4fv
	#define glProgramUniformMatrix2x3dv                        glad_glProgramUniformMatrix2x3dv
	#define glIsTransformFeedback                              glad_glIsTransformFeedback
	#define glUniform4i                                        glad_glUniform4i
	#define glActiveTexture                                    glad_glActiveTexture
	#define glEnableVertexAttribArray                          glad_glEnableVertexAttribArray
	#define glIsProgramPipeline                                glad_glIsProgramPipeline
	#define glReadPixels                                       glad_glReadPixels
	#define glVertexAttribI3iv                                 glad_glVertexAttribI3iv
	#define glUniform4f                                        glad_glUniform4f
	#define glRenderbufferStorageMultisample                   glad_glRenderbufferStorageMultisample
	#define glCreateProgramPipelines                           glad_glCreateProgramPipelines
	#define glUniformMatrix3fv                                 glad_glUniformMatrix3fv
	#define glVertexAttribLPointer                             glad_glVertexAttribLPointer
	#define glGetnUniformfv                                    glad_glGetnUniformfv
	#define glDrawElementsInstancedBaseVertex                  glad_glDrawElementsInstancedBaseVertex
	#define glVertexAttribL2dv                                 glad_glVertexAttribL2dv
	#define glEnableVertexArrayAttrib                          glad_glEnableVertexArrayAttrib
	#define glDrawTransformFeedbackStreamInstanced             glad_glDrawTransformFeedbackStreamInstanced
	#define glGetActiveSubroutineName                          glad_glGetActiveSubroutineName
	#define glVertexAttribL2d                                  glad_glVertexAttribL2d
	#define glStencilFunc                                      glad_glStencilFunc
	#define glInvalidateNamedFramebufferData                   glad_glInvalidateNamedFramebufferData
	#define glPopDebugGroup                                    glad_glPopDebugGroup
	#define glUniformBlockBinding                              glad_glUniformBlockBinding
	#define glGetVertexArrayIndexediv                          glad_glGetVertexArrayIndexediv
	#define glColorP4ui                                        glad_glColorP4ui
	#define glUseProgramStages                                 glad_glUseProgramStages
	#define glProgramUniform3f                                 glad_glProgramUniform3f
	#define glProgramUniform3d                                 glad_glProgramUniform3d
	#define glVertexAttribI4iv                                 glad_glVertexAttribI4iv
	#define glGetProgramPipelineiv                             glad_glGetProgramPipelineiv
	#define glTexStorage3D                                     glad_glTexStorage3D
	#define glNamedFramebufferDrawBuffer                       glad_glNamedFramebufferDrawBuffer
	#define glGetQueryIndexediv                                glad_glGetQueryIndexediv
	#define glGetShaderInfoLog                                 glad_glGetShaderInfoLog
	#define glObjectLabel                                      glad_glObjectLabel
	#define glVertexAttribI4i                                  glad_glVertexAttribI4i
	#define glGetBufferSubData                                 glad_glGetBufferSubData
	#define glGetVertexAttribLdv                               glad_glGetVertexAttribLdv
	#define glGetnUniformuiv                                   glad_glGetnUniformuiv
	#define glGetQueryBufferObjectiv                           glad_glGetQueryBufferObjectiv
	#define glBlendEquationSeparate                            glad_glBlendEquationSeparate
	#define glVertexAttribI1ui                                 glad_glVertexAttribI1ui
	#define glGenBuffers                                       glad_glGenBuffers
	#define glGetSubroutineIndex                               glad_glGetSubroutineIndex
	#define glVertexAttrib2sv                                  glad_glVertexAttrib2sv
	#define glGetnPolygonStipple                               glad_glGetnPolygonStipple
	#define glBlendFunc                                        glad_glBlendFunc
	#define glCreateProgram                                    glad_glCreateProgram
	#define glTexImage3D                                       glad_glTexImage3D
	#define glIsFramebuffer                                    glad_glIsFramebuffer
	#define glClearNamedFramebufferfv                          glad_glClearNamedFramebufferfv
	#define glGetNamedBufferSubData                            glad_glGetNamedBufferSubData
	#define glPrimitiveRestartIndex                            glad_glPrimitiveRestartIndex
	#define glFlushMappedNamedBufferRange                      glad_glFlushMappedNamedBufferRange
	#define glInvalidateTexSubImage                            glad_glInvalidateTexSubImage
	#define glBindImageTextures                                glad_glBindImageTextures
	#define glGetInteger64v                                    glad_glGetInteger64v
	#define glBindProgramPipeline                              glad_glBindProgramPipeline
	#define glScissor                                          glad_glScissor
	#define glTexCoordP4uiv                                    glad_glTexCoordP4uiv
	#define glGetBooleanv                                      glad_glGetBooleanv
	#define glNamedFramebufferRenderbuffer                     glad_glNamedFramebufferRenderbuffer
	#define glVertexP2uiv                                      glad_glVertexP2uiv
	#define glUniform3uiv                                      glad_glUniform3uiv
	#define glClearColor                                       glad_glClearColor
	#define glVertexAttrib4Niv                                 glad_glVertexAttrib4Niv
	#define glClearBufferiv                                    glad_glClearBufferiv
	#define glGetBufferParameteri64v                           glad_glGetBufferParameteri64v
	#define glProgramUniform4dv                                glad_glProgramUniform4dv
	#define glColorP4uiv                                       glad_glColorP4uiv
	#define glGetTextureLevelParameteriv                       glad_glGetTextureLevelParameteriv
	#define glGetnUniformiv                                    glad_glGetnUniformiv
	#define glVertexAttribI2uiv                                glad_glVertexAttribI2uiv
	#define glUniform3ui                                       glad_glUniform3ui
	#define glProgramUniform3uiv                               glad_glProgramUniform3uiv
	#define glVertexAttribI4uiv                                glad_glVertexAttribI4uiv
	#define glPointParameterfv                                 glad_glPointParameterfv
	#define glUniform2fv                                       glad_glUniform2fv
	#define glGetActiveSubroutineUniformName                   glad_glGetActiveSubroutineUniformName
	#define glGetProgramResourceIndex                          glad_glGetProgramResourceIndex
	#define glDrawElementsIndirect                             glad_glDrawElementsIndirect
	#define glGetTextureLevelParameterfv                       glad_glGetTextureLevelParameterfv
	#define glGetNamedBufferPointerv                           glad_glGetNamedBufferPointerv
	#define glDispatchComputeIndirect                          glad_glDispatchComputeIndirect
	#define glInvalidateNamedFramebufferSubData                glad_glInvalidateNamedFramebufferSubData
	#define glGetSamplerParameterIuiv                          glad_glGetSamplerParameterIuiv
	#define glBindBufferRange                                  glad_glBindBufferRange
	#define glTextureSubImage1D                                glad_glTextureSubImage1D
	#define glVertexAttribL3dv                                 glad_glVertexAttribL3dv
	#define glGetUniformdv                                     glad_glGetUniformdv
	#define glGetQueryBufferObjectui64v                        glad_glGetQueryBufferObjectui64v
	#define glClearDepthf                                      glad_glClearDepthf
	#define glCreateRenderbuffers                              glad_glCreateRenderbuffers
	#define glUniformMatrix2x3fv                               glad_glUniformMatrix2x3fv
	#define glCreateTextures                                   glad_glCreateTextures
	#define glGenTransformFeedbacks                            glad_glGenTransformFeedbacks
	#define glGetVertexAttribIuiv                              glad_glGetVertexAttribIuiv
	#define glVertexAttrib4Nusv                                glad_glVertexAttrib4Nusv
	#define glProgramUniformMatrix4x3dv                        glad_glProgramUniformMatrix4x3dv
	#define glDepthFunc                                        glad_glDepthFunc
	#define glCompressedTexSubImage2D                          glad_glCompressedTexSubImage2D
	#define glProgramBinary                                    glad_glProgramBinary
	#define glVertexAttribI4bv                                 glad_glVertexAttribI4bv
	#define glGetTexParameterfv                                glad_glGetTexParameterfv
	#define glMultiTexCoordP1ui                                glad_glMultiTexCoordP1ui
	#define glBufferStorage                                    glad_glBufferStorage
	#define glClientWaitSync                                   glad_glClientWaitSync
	#define glVertexAttribI4ui                                 glad_glVertexAttribI4ui
	#define glGetFloati_v                                      glad_glGetFloati_v
	#define glColorMask                                        glad_glColorMask
	#define glTextureBuffer                                    glad_glTextureBuffer
	#define glTexParameterIiv                                  glad_glTexParameterIiv
	#define glBlendEquation                                    glad_glBlendEquation
	#define glGetUniformLocation                               glad_glGetUniformLocation
	#define glUniformMatrix2x4dv                               glad_glUniformMatrix2x4dv
	#define glVertexArrayAttribFormat                          glad_glVertexArrayAttribFormat
	#define glReadnPixels                                      glad_glReadnPixels
	#define glNamedFramebufferDrawBuffers                      glad_glNamedFramebufferDrawBuffers
	#define glEndTransformFeedback                             glad_glEndTransformFeedback
	#define glVertexAttrib4usv                                 glad_glVertexAttrib4usv
	#define glGetUniformSubroutineuiv                          glad_glGetUniformSubroutineuiv
	#define glUniform4fv                                       glad_glUniform4fv
	#define glBindVertexBuffer                                 glad_glBindVertexBuffer
	#define glDebugMessageInsert                               glad_glDebugMessageInsert
	#define glCreateSamplers                                   glad_glCreateSamplers
	#define glGetProgramResourceiv                             glad_glGetProgramResourceiv
	#define glClearBufferData                                  glad_glClearBufferData
	#define glBeginTransformFeedback                           glad_glBeginTransformFeedback
	#define glVertexAttribI1iv                                 glad_glVertexAttribI1iv
	#define glIsSampler                                        glad_glIsSampler
	#define glVertexP3ui                                       glad_glVertexP3ui
	#define glVertexAttribDivisor                              glad_glVertexAttribDivisor
	#define glBindSamplers                                     glad_glBindSamplers
	#define glCompressedTexImage1D                             glad_glCompressedTexImage1D
	#define glDeleteTransformFeedbacks                         glad_glDeleteTransformFeedbacks
	#define glCopyTexSubImage1D                                glad_glCopyTexSubImage1D
	#define glDrawRangeElementsBaseVertex                      glad_glDrawRangeElementsBaseVertex
	#define glCheckFramebufferStatus                           glad_glCheckFramebufferStatus
	#define glEndConditionalRender                             glad_glEndConditionalRender
	#define glVertexP3uiv                                      glad_glVertexP3uiv
	#define glBindAttribLocation                               glad_glBindAttribLocation
	#define glUniformMatrix4x2fv                               glad_glUniformMatrix4x2fv
	#define glUniformMatrix2dv                                 glad_glUniformMatrix2dv
	#define glBlendFunci                                       glad_glBlendFunci
	#define glVertexAttrib1dv                                  glad_glVertexAttrib1dv
	#define glDrawRangeElements                                glad_glDrawRangeElements
	#define glGetQueryObjectuiv                                glad_glGetQueryObjectuiv
	#define glBindBufferBase                                   glad_glBindBufferBase
	#define glBufferSubData                                    glad_glBufferSubData
	#define glVertexAttrib4iv                                  glad_glVertexAttrib4iv
	#define glMapBufferRange                                   glad_glMapBufferRange
	#define glFramebufferTexture                               glad_glFramebufferTexture
	#define glBlendFuncSeparatei                               glad_glBlendFuncSeparatei
	#define glProgramUniformMatrix4x2fv                        glad_glProgramUniformMatrix4x2fv
	#define glVertexAttribL1d                                  glad_glVertexAttribL1d
	#define glMultiDrawArrays                                  glad_glMultiDrawArrays
	#define glVertexP4uiv                                      glad_glVertexP4uiv
	#define glVertexAttribI2iv                                 glad_glVertexAttribI2iv
	#define glGetShaderPrecisionFormat                         glad_glGetShaderPrecisionFormat
	#define glTextureView                                      glad_glTextureView
	#define glDisablei                                         glad_glDisablei
	#define glProgramUniformMatrix2x4fv                        glad_glProgramUniformMatrix2x4fv
	#define glShaderSource                                     glad_glShaderSource
	#define glGetnSeparableFilter                              glad_glGetnSeparableFilter
	#define glDeleteRenderbuffers                              glad_glDeleteRenderbuffers
	#define glVertexAttribI3uiv                                glad_glVertexAttribI3uiv
	#define glReleaseShaderCompiler                            glad_glReleaseShaderCompiler
	#define glVertexAttribIFormat                              glad_glVertexAttribIFormat
	#define glCreateFramebuffers                               glad_glCreateFramebuffers
	#define glGetSynciv                                        glad_glGetSynciv
	#define glGetnPixelMapfv                                   glad_glGetnPixelMapfv
	#define glTexCoordP2uiv                                    glad_glTexCoordP2uiv
	#define glPatchParameterfv                                 glad_glPatchParameterfv
	#define glProgramUniform2i                                 glad_glProgramUniform2i
	#define glGetNamedBufferParameteri64v                      glad_glGetNamedBufferParameteri64v
	#define glBeginQuery                                       glad_glBeginQuery
	#define glUniformMatrix4fv                                 glad_glUniformMatrix4fv
	#define glBindBuffer                                       glad_glBindBuffer
	#define glTexStorage2DMultisample                          glad_glTexStorage2DMultisample
	#define glProgramUniform2d                                 glad_glProgramUniform2d
	#define glProgramUniform2f                                 glad_glProgramUniform2f
	#define glUniformMatrix2fv                                 glad_glUniformMatrix2fv
	#define glUniformMatrix2x4fv                               glad_glUniformMatrix2x4fv
	#define glBufferData                                       glad_glBufferData
	#define glGetTexParameterIiv                               glad_glGetTexParameterIiv
	#define glTexCoordP1ui                                     glad_glTexCoordP1ui
	#define glGetError                                         glad_glGetError
	#define glCreateTransformFeedbacks                         glad_glCreateTransformFeedbacks
	#define glVertexAttribP2ui                                 glad_glVertexAttribP2ui
	#define glGetFloatv                                        glad_glGetFloatv
	#define glTexSubImage1D                                    glad_glTexSubImage1D
	#define glVertexAttrib2fv                                  glad_glVertexAttrib2fv
	#define glGetTexLevelParameterfv                           glad_glGetTexLevelParameterfv
	#define glVertexAttribI1i                                  glad_glVertexAttribI1i
	#define glVertexAttribP3uiv                                glad_glVertexAttribP3uiv
	#define glUniform4d                                        glad_glUniform4d
	#define glSecondaryColorP3uiv                              glad_glSecondaryColorP3uiv
	#define glGetIntegerv                                      glad_glGetIntegerv
	#define glGetVertexArrayIndexed64iv                        glad_glGetVertexArrayIndexed64iv
	#define glGetBufferPointerv                                glad_glGetBufferPointerv
	#define glProgramUniformMatrix3dv                          glad_glProgramUniformMatrix3dv
	#define glFramebufferTexture3D                             glad_glFramebufferTexture3D
	#define glIsQuery                                          glad_glIsQuery
	#define glProgramUniform2ui                                glad_glProgramUniform2ui
	#define glProgramUniform4ui                                glad_glProgramUniform4ui
	#define glVertexAttrib4sv                                  glad_glVertexAttrib4sv
	#define glTexImage2D                                       glad_glTexImage2D
	#define glStencilMask                                      glad_glStencilMask
	#define glSamplerParameterfv                               glad_glSamplerParameterfv
	#define glIsTexture                                        glad_glIsTexture
	#define glNamedBufferData                                  glad_glNamedBufferData
	#define glUniform1fv                                       glad_glUniform1fv
	#define glVertexAttrib4Nubv                                glad_glVertexAttrib4Nubv
	#define glClearNamedBufferSubData                          glad_glClearNamedBufferSubData
	#define glTexParameterfv                                   glad_glTexParameterfv
	#define glScissorIndexedv                                  glad_glScissorIndexedv
	#define glUniform3dv                                       glad_glUniform3dv
	#define glGetnPixelMapuiv                                  glad_glGetnPixelMapuiv
	#define glProgramUniform3fv                                glad_glProgramUniform3fv
	#define glGetSubroutineUniformLocation                     glad_glGetSubroutineUniformLocation
	#define glGetFramebufferParameteriv                        glad_glGetFramebufferParameteriv
	#define glGetSamplerParameteriv                            glad_glGetSamplerParameteriv
	#define glGetCompressedTextureSubImage                     glad_glGetCompressedTextureSubImage
	#define glCopyBufferSubData                                glad_glCopyBufferSubData
	#define glVertexAttribI1uiv                                glad_glVertexAttribI1uiv
	#define glVertexAttrib2d                                   glad_glVertexAttrib2d
	#define glVertexAttrib2f                                   glad_glVertexAttrib2f
	#define glVertexAttrib3dv                                  glad_glVertexAttrib3dv
	#define glGetQueryObjectui64v                              glad_glGetQueryObjectui64v
	#define glDepthMask                                        glad_glDepthMask
	#define glVertexAttrib2s                                   glad_glVertexAttrib2s
	#define glTexImage3DMultisample                            glad_glTexImage3DMultisample
	#define glProgramUniform1fv                                glad_glProgramUniform1fv
	#define glGetUniformBlockIndex                             glad_glGetUniformBlockIndex
	#define glTexImage2DMultisample                            glad_glTexImage2DMultisample
	#define glGetActiveUniform                                 glad_glGetActiveUniform
	#define glFrontFace                                        glad_glFrontFace
	#define glTexBufferRange                                   glad_glTexBufferRange
	#define glNamedFramebufferTextureLayer                     glad_glNamedFramebufferTextureLayer
	#define glNamedFramebufferParameteri                       glad_glNamedFramebufferParameteri
	#define glDrawArraysInstancedBaseInstance                  glad_glDrawArraysInstancedBaseInstance
	#define glDeleteProgramPipelines                           glad_glDeleteProgramPipelines
#endif
#define GL_BLEND_COLOR                                       0x8005
#define GL_BLEND_EQUATION                                    0x8009
#define GL_CONVOLUTION_1D                                    0x8010
#define GL_CONVOLUTION_2D                                    0x8011
#define GL_SEPARABLE_2D                                      0x8012
#define GL_CONVOLUTION_BORDER_MODE                           0x8013
#define GL_CONVOLUTION_FILTER_SCALE                          0x8014
#define GL_CONVOLUTION_FILTER_BIAS                           0x8015
#define GL_REDUCE                                            0x8016
#define GL_CONVOLUTION_FORMAT                                0x8017
#define GL_CONVOLUTION_WIDTH                                 0x8018
#define GL_CONVOLUTION_HEIGHT                                0x8019
#define GL_MAX_CONVOLUTION_WIDTH                             0x801A
#define GL_MAX_CONVOLUTION_HEIGHT                            0x801B
#define GL_POST_CONVOLUTION_RED_SCALE                        0x801C
#define GL_POST_CONVOLUTION_GREEN_SCALE                      0x801D
#define GL_POST_CONVOLUTION_BLUE_SCALE                       0x801E
#define GL_POST_CONVOLUTION_ALPHA_SCALE                      0x801F
#define GL_POST_CONVOLUTION_RED_BIAS                         0x8020
#define GL_POST_CONVOLUTION_GREEN_BIAS                       0x8021
#define GL_POST_CONVOLUTION_BLUE_BIAS                        0x8022
#define GL_POST_CONVOLUTION_ALPHA_BIAS                       0x8023
#define GL_HISTOGRAM                                         0x8024
#define GL_PROXY_HISTOGRAM                                   0x8025
#define GL_HISTOGRAM_WIDTH                                   0x8026
#define GL_HISTOGRAM_FORMAT                                  0x8027
#define GL_HISTOGRAM_RED_SIZE                                0x8028
#define GL_HISTOGRAM_GREEN_SIZE                              0x8029
#define GL_HISTOGRAM_BLUE_SIZE                               0x802A
#define GL_HISTOGRAM_ALPHA_SIZE                              0x802B
#define GL_HISTOGRAM_LUMINANCE_SIZE                          0x802C
#define GL_HISTOGRAM_SINK                                    0x802D
#define GL_MINMAX                                            0x802E
#define GL_MINMAX_FORMAT                                     0x802F
#define GL_MINMAX_SINK                                       0x8030
#define GL_TABLE_TOO_LARGE                                   0x8031
#define GL_COLOR_MATRIX                                      0x80B1
#define GL_COLOR_MATRIX_STACK_DEPTH                          0x80B2
#define GL_MAX_COLOR_MATRIX_STACK_DEPTH                      0x80B3
#define GL_POST_COLOR_MATRIX_RED_SCALE                       0x80B4
#define GL_POST_COLOR_MATRIX_GREEN_SCALE                     0x80B5
#define GL_POST_COLOR_MATRIX_BLUE_SCALE                      0x80B6
#define GL_POST_COLOR_MATRIX_ALPHA_SCALE                     0x80B7
#define GL_POST_COLOR_MATRIX_RED_BIAS                        0x80B8
#define GL_POST_COLOR_MATRIX_GREEN_BIAS                      0x80B9
#define GL_POST_COLOR_MATRIX_BLUE_BIAS                       0x80BA
#define GL_POST_COLOR_MATRIX_ALPHA_BIAS                      0x80BB
#define GL_COLOR_TABLE                                       0x80D0
#define GL_POST_CONVOLUTION_COLOR_TABLE                      0x80D1
#define GL_POST_COLOR_MATRIX_COLOR_TABLE                     0x80D2
#define GL_PROXY_COLOR_TABLE                                 0x80D3
#define GL_PROXY_POST_CONVOLUTION_COLOR_TABLE                0x80D4
#define GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE               0x80D5
#define GL_COLOR_TABLE_SCALE                                 0x80D6
#define GL_COLOR_TABLE_BIAS                                  0x80D7
#define GL_COLOR_TABLE_FORMAT                                0x80D8
#define GL_COLOR_TABLE_WIDTH                                 0x80D9
#define GL_COLOR_TABLE_RED_SIZE                              0x80DA
#define GL_COLOR_TABLE_GREEN_SIZE                            0x80DB
#define GL_COLOR_TABLE_BLUE_SIZE                             0x80DC
#define GL_COLOR_TABLE_ALPHA_SIZE                            0x80DD
#define GL_COLOR_TABLE_LUMINANCE_SIZE                        0x80DE
#define GL_COLOR_TABLE_INTENSITY_SIZE                        0x80DF
#define GL_CONSTANT_BORDER                                   0x8151
#define GL_REPLICATE_BORDER                                  0x8153
#define GL_CONVOLUTION_BORDER_COLOR                          0x8154
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                      0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                     0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                     0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                     0x83F3
#define GL_SRGB_EXT                                          0x8C40
#define GL_SRGB8_EXT                                         0x8C41
#define GL_SRGB_ALPHA_EXT                                    0x8C42
#define GL_SRGB8_ALPHA8_EXT                                  0x8C43
#define GL_SLUMINANCE_ALPHA_EXT                              0x8C44
#define GL_SLUMINANCE8_ALPHA8_EXT                            0x8C45
#define GL_SLUMINANCE_EXT                                    0x8C46
#define GL_SLUMINANCE8_EXT                                   0x8C47
#define GL_COMPRESSED_SRGB_EXT                               0x8C48
#define GL_COMPRESSED_SRGB_ALPHA_EXT                         0x8C49
#define GL_COMPRESSED_SLUMINANCE_EXT                         0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA_EXT                   0x8C4B
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT                     0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT               0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT               0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT               0x8C4F
#define GL_TEXTURE_MAX_ANISOTROPY_EXT                        0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT                    0x84FF
#define GL_COMPRESSED_RGBA_BPTC_UNORM_ARB                    0x8E8C
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB              0x8E8D
#define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB              0x8E8E
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB            0x8E8F
#define GL_VERTEX_ARRAY                                      0x8074
#define GL_STACK_OVERFLOW                                    0x0503
#define GL_STACK_UNDERFLOW                                   0x0504
#define GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR                      0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_KHR              0x8243
#define GL_DEBUG_CALLBACK_FUNCTION_KHR                       0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM_KHR                     0x8245
#define GL_DEBUG_SOURCE_API_KHR                              0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR                    0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER_KHR                  0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY_KHR                      0x8249
#define GL_DEBUG_SOURCE_APPLICATION_KHR                      0x824A
#define GL_DEBUG_SOURCE_OTHER_KHR                            0x824B
#define GL_DEBUG_TYPE_ERROR_KHR                              0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR                0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR                 0x824E
#define GL_DEBUG_TYPE_PORTABILITY_KHR                        0x824F
#define GL_DEBUG_TYPE_PERFORMANCE_KHR                        0x8250
#define GL_DEBUG_TYPE_OTHER_KHR                              0x8251
#define GL_DEBUG_TYPE_MARKER_KHR                             0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP_KHR                         0x8269
#define GL_DEBUG_TYPE_POP_GROUP_KHR                          0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION_KHR                   0x826B
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH_KHR                   0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH_KHR                       0x826D
#define GL_BUFFER_KHR                                        0x82E0
#define GL_SHADER_KHR                                        0x82E1
#define GL_PROGRAM_KHR                                       0x82E2
#define GL_VERTEX_ARRAY_KHR                                  0x8074
#define GL_QUERY_KHR                                         0x82E3
#define GL_PROGRAM_PIPELINE_KHR                              0x82E4
#define GL_SAMPLER_KHR                                       0x82E6
#define GL_MAX_LABEL_LENGTH_KHR                              0x82E8
#define GL_MAX_DEBUG_MESSAGE_LENGTH_KHR                      0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES_KHR                     0x9144
#define GL_DEBUG_LOGGED_MESSAGES_KHR                         0x9145
#define GL_DEBUG_SEVERITY_HIGH_KHR                           0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_KHR                         0x9147
#define GL_DEBUG_SEVERITY_LOW_KHR                            0x9148
#define GL_DEBUG_OUTPUT_KHR                                  0x92E0
#define GL_CONTEXT_FLAG_DEBUG_BIT_KHR                        0x00000002
#define GL_STACK_OVERFLOW_KHR                                0x0503
#define GL_STACK_UNDERFLOW_KHR                               0x0504
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX              0x9047
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX        0x9048
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX      0x9049
#define GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX                0x904A
#define GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX                0x904B
#define GL_VBO_FREE_MEMORY_ATI                               0x87FB
#define GL_TEXTURE_FREE_MEMORY_ATI                           0x87FC
#define GL_RENDERBUFFER_FREE_MEMORY_ATI                      0x87FD
#define GL_SRGB_DECODE_ARB                                   0x8299
#define GL_PROGRAM_MATRIX_EXT                                0x8E2D
#define GL_TRANSPOSE_PROGRAM_MATRIX_EXT                      0x8E2E
#define GL_PROGRAM_MATRIX_STACK_DEPTH_EXT                    0x8E2F
#define GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV                    0x8F1E
#define GL_ELEMENT_ARRAY_UNIFIED_NV                          0x8F1F
#define GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV                    0x8F20
#define GL_VERTEX_ARRAY_ADDRESS_NV                           0x8F21
#define GL_NORMAL_ARRAY_ADDRESS_NV                           0x8F22
#define GL_COLOR_ARRAY_ADDRESS_NV                            0x8F23
#define GL_INDEX_ARRAY_ADDRESS_NV                            0x8F24
#define GL_TEXTURE_COORD_ARRAY_ADDRESS_NV                    0x8F25
#define GL_EDGE_FLAG_ARRAY_ADDRESS_NV                        0x8F26
#define GL_SECONDARY_COLOR_ARRAY_ADDRESS_NV                  0x8F27
#define GL_FOG_COORD_ARRAY_ADDRESS_NV                        0x8F28
#define GL_ELEMENT_ARRAY_ADDRESS_NV                          0x8F29
#define GL_VERTEX_ATTRIB_ARRAY_LENGTH_NV                     0x8F2A
#define GL_VERTEX_ARRAY_LENGTH_NV                            0x8F2B
#define GL_NORMAL_ARRAY_LENGTH_NV                            0x8F2C
#define GL_COLOR_ARRAY_LENGTH_NV                             0x8F2D
#define GL_INDEX_ARRAY_LENGTH_NV                             0x8F2E
#define GL_TEXTURE_COORD_ARRAY_LENGTH_NV                     0x8F2F
#define GL_EDGE_FLAG_ARRAY_LENGTH_NV                         0x8F30
#define GL_SECONDARY_COLOR_ARRAY_LENGTH_NV                   0x8F31
#define GL_FOG_COORD_ARRAY_LENGTH_NV                         0x8F32
#define GL_ELEMENT_ARRAY_LENGTH_NV                           0x8F33
#define GL_DRAW_INDIRECT_UNIFIED_NV                          0x8F40
#define GL_DRAW_INDIRECT_ADDRESS_NV                          0x8F41
#define GL_DRAW_INDIRECT_LENGTH_NV                           0x8F42
#define GL_UNIFORM_BUFFER_UNIFIED_NV                         0x936E
#define GL_UNIFORM_BUFFER_ADDRESS_NV                         0x936F
#define GL_UNIFORM_BUFFER_LENGTH_NV                          0x9370
#define GL_BUFFER_GPU_ADDRESS_NV                             0x8F1D
#define GL_GPU_ADDRESS_NV                                    0x8F34
#define GL_MAX_SHADER_BUFFER_ADDRESS_NV                      0x8F35
#define GL_MAX_IMAGE_UNITS_EXT                               0x8F38
#define GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS_EXT 0x8F39
#define GL_IMAGE_BINDING_NAME_EXT                            0x8F3A
#define GL_IMAGE_BINDING_LEVEL_EXT                           0x8F3B
#define GL_IMAGE_BINDING_LAYERED_EXT                         0x8F3C
#define GL_IMAGE_BINDING_LAYER_EXT                           0x8F3D
#define GL_IMAGE_BINDING_ACCESS_EXT                          0x8F3E
#define GL_IMAGE_1D_EXT                                      0x904C
#define GL_IMAGE_2D_EXT                                      0x904D
#define GL_IMAGE_3D_EXT                                      0x904E
#define GL_IMAGE_2D_RECT_EXT                                 0x904F
#define GL_IMAGE_CUBE_EXT                                    0x9050
#define GL_IMAGE_BUFFER_EXT                                  0x9051
#define GL_IMAGE_1D_ARRAY_EXT                                0x9052
#define GL_IMAGE_2D_ARRAY_EXT                                0x9053
#define GL_IMAGE_CUBE_MAP_ARRAY_EXT                          0x9054
#define GL_IMAGE_2D_MULTISAMPLE_EXT                          0x9055
#define GL_IMAGE_2D_MULTISAMPLE_ARRAY_EXT                    0x9056
#define GL_INT_IMAGE_1D_EXT                                  0x9057
#define GL_INT_IMAGE_2D_EXT                                  0x9058
#define GL_INT_IMAGE_3D_EXT                                  0x9059
#define GL_INT_IMAGE_2D_RECT_EXT                             0x905A
#define GL_INT_IMAGE_CUBE_EXT                                0x905B
#define GL_INT_IMAGE_BUFFER_EXT                              0x905C
#define GL_INT_IMAGE_1D_ARRAY_EXT                            0x905D
#define GL_INT_IMAGE_2D_ARRAY_EXT                            0x905E
#define GL_INT_IMAGE_CUBE_MAP_ARRAY_EXT                      0x905F
#define GL_INT_IMAGE_2D_MULTISAMPLE_EXT                      0x9060
#define GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY_EXT                0x9061
#define GL_UNSIGNED_INT_IMAGE_1D_EXT                         0x9062
#define GL_UNSIGNED_INT_IMAGE_2D_EXT                         0x9063
#define GL_UNSIGNED_INT_IMAGE_3D_EXT                         0x9064
#define GL_UNSIGNED_INT_IMAGE_2D_RECT_EXT                    0x9065
#define GL_UNSIGNED_INT_IMAGE_CUBE_EXT                       0x9066
#define GL_UNSIGNED_INT_IMAGE_BUFFER_EXT                     0x9067
#define GL_UNSIGNED_INT_IMAGE_1D_ARRAY_EXT                   0x9068
#define GL_UNSIGNED_INT_IMAGE_2D_ARRAY_EXT                   0x9069
#define GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_EXT             0x906A
#define GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_EXT             0x906B
#define GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY_EXT       0x906C
#define GL_MAX_IMAGE_SAMPLES_EXT                             0x906D
#define GL_IMAGE_BINDING_FORMAT_EXT                          0x906E
#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_EXT               0x00000001
#define GL_ELEMENT_ARRAY_BARRIER_BIT_EXT                     0x00000002
#define GL_UNIFORM_BARRIER_BIT_EXT                           0x00000004
#define GL_TEXTURE_FETCH_BARRIER_BIT_EXT                     0x00000008
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT               0x00000020
#define GL_COMMAND_BARRIER_BIT_EXT                           0x00000040
#define GL_PIXEL_BUFFER_BARRIER_BIT_EXT                      0x00000080
#define GL_TEXTURE_UPDATE_BARRIER_BIT_EXT                    0x00000100
#define GL_BUFFER_UPDATE_BARRIER_BIT_EXT                     0x00000200
#define GL_FRAMEBUFFER_BARRIER_BIT_EXT                       0x00000400
#define GL_TRANSFORM_FEEDBACK_BARRIER_BIT_EXT                0x00000800
#define GL_ATOMIC_COUNTER_BARRIER_BIT_EXT                    0x00001000
#define GL_ALL_BARRIER_BITS_EXT                              0xFFFFFFFF
#define GL_DEPTH_COMPONENT32F_NV                             0x8DAB
#define GL_DEPTH32F_STENCIL8_NV                              0x8DAC
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV_NV                 0x8DAD
#define GL_DEPTH_BUFFER_FLOAT_MODE_NV                        0x8DAF
#ifndef GL_ARB_imaging
	#define GL_ARB_imaging                                     1
GLAPI int GLAD_GL_ARB_imaging;
typedef void (APIENTRYP PFNGLCOLORTABLEPROC)(GLenum, GLenum, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLCOLORTABLEPROC glad_glColorTable;
typedef void (APIENTRYP PFNGLCOLORTABLEPARAMETERFVPROC)(GLenum, GLenum, const GLfloat*);
GLAPI PFNGLCOLORTABLEPARAMETERFVPROC glad_glColorTableParameterfv;
typedef void (APIENTRYP PFNGLCOLORTABLEPARAMETERIVPROC)(GLenum, GLenum, const GLint*);
GLAPI PFNGLCOLORTABLEPARAMETERIVPROC glad_glColorTableParameteriv;
typedef void (APIENTRYP PFNGLCOPYCOLORTABLEPROC)(GLenum, GLenum, GLint, GLint, GLsizei);
GLAPI PFNGLCOPYCOLORTABLEPROC glad_glCopyColorTable;
typedef void (APIENTRYP PFNGLGETCOLORTABLEPROC)(GLenum, GLenum, GLenum, void*);
GLAPI PFNGLGETCOLORTABLEPROC glad_glGetColorTable;
typedef void (APIENTRYP PFNGLGETCOLORTABLEPARAMETERFVPROC)(GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETCOLORTABLEPARAMETERFVPROC glad_glGetColorTableParameterfv;
typedef void (APIENTRYP PFNGLGETCOLORTABLEPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETCOLORTABLEPARAMETERIVPROC glad_glGetColorTableParameteriv;
typedef void (APIENTRYP PFNGLCOLORSUBTABLEPROC)(GLenum, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLCOLORSUBTABLEPROC glad_glColorSubTable;
typedef void (APIENTRYP PFNGLCOPYCOLORSUBTABLEPROC)(GLenum, GLsizei, GLint, GLint, GLsizei);
GLAPI PFNGLCOPYCOLORSUBTABLEPROC glad_glCopyColorSubTable;
typedef void (APIENTRYP PFNGLCONVOLUTIONFILTER1DPROC)(GLenum, GLenum, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLCONVOLUTIONFILTER1DPROC glad_glConvolutionFilter1D;
typedef void (APIENTRYP PFNGLCONVOLUTIONFILTER2DPROC)(GLenum, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLCONVOLUTIONFILTER2DPROC glad_glConvolutionFilter2D;
typedef void (APIENTRYP PFNGLCONVOLUTIONPARAMETERFPROC)(GLenum, GLenum, GLfloat);
GLAPI PFNGLCONVOLUTIONPARAMETERFPROC glad_glConvolutionParameterf;
typedef void (APIENTRYP PFNGLCONVOLUTIONPARAMETERFVPROC)(GLenum, GLenum, const GLfloat*);
GLAPI PFNGLCONVOLUTIONPARAMETERFVPROC glad_glConvolutionParameterfv;
typedef void (APIENTRYP PFNGLCONVOLUTIONPARAMETERIPROC)(GLenum, GLenum, GLint);
GLAPI PFNGLCONVOLUTIONPARAMETERIPROC glad_glConvolutionParameteri;
typedef void (APIENTRYP PFNGLCONVOLUTIONPARAMETERIVPROC)(GLenum, GLenum, const GLint*);
GLAPI PFNGLCONVOLUTIONPARAMETERIVPROC glad_glConvolutionParameteriv;
typedef void (APIENTRYP PFNGLCOPYCONVOLUTIONFILTER1DPROC)(GLenum, GLenum, GLint, GLint, GLsizei);
GLAPI PFNGLCOPYCONVOLUTIONFILTER1DPROC glad_glCopyConvolutionFilter1D;
typedef void (APIENTRYP PFNGLCOPYCONVOLUTIONFILTER2DPROC)(GLenum, GLenum, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYCONVOLUTIONFILTER2DPROC glad_glCopyConvolutionFilter2D;
typedef void (APIENTRYP PFNGLGETCONVOLUTIONFILTERPROC)(GLenum, GLenum, GLenum, void*);
GLAPI PFNGLGETCONVOLUTIONFILTERPROC glad_glGetConvolutionFilter;
typedef void (APIENTRYP PFNGLGETCONVOLUTIONPARAMETERFVPROC)(GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETCONVOLUTIONPARAMETERFVPROC glad_glGetConvolutionParameterfv;
typedef void (APIENTRYP PFNGLGETCONVOLUTIONPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETCONVOLUTIONPARAMETERIVPROC glad_glGetConvolutionParameteriv;
typedef void (APIENTRYP PFNGLGETSEPARABLEFILTERPROC)(GLenum, GLenum, GLenum, void*, void*, void*);
GLAPI PFNGLGETSEPARABLEFILTERPROC glad_glGetSeparableFilter;
typedef void (APIENTRYP PFNGLSEPARABLEFILTER2DPROC)(GLenum, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void*, const void*);
GLAPI PFNGLSEPARABLEFILTER2DPROC glad_glSeparableFilter2D;
typedef void (APIENTRYP PFNGLGETHISTOGRAMPROC)(GLenum, GLboolean, GLenum, GLenum, void*);
GLAPI PFNGLGETHISTOGRAMPROC glad_glGetHistogram;
typedef void (APIENTRYP PFNGLGETHISTOGRAMPARAMETERFVPROC)(GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETHISTOGRAMPARAMETERFVPROC glad_glGetHistogramParameterfv;
typedef void (APIENTRYP PFNGLGETHISTOGRAMPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETHISTOGRAMPARAMETERIVPROC glad_glGetHistogramParameteriv;
typedef void (APIENTRYP PFNGLGETMINMAXPROC)(GLenum, GLboolean, GLenum, GLenum, void*);
GLAPI PFNGLGETMINMAXPROC glad_glGetMinmax;
typedef void (APIENTRYP PFNGLGETMINMAXPARAMETERFVPROC)(GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETMINMAXPARAMETERFVPROC glad_glGetMinmaxParameterfv;
typedef void (APIENTRYP PFNGLGETMINMAXPARAMETERIVPROC)(GLenum, GLenum, GLint*);
GLAPI PFNGLGETMINMAXPARAMETERIVPROC glad_glGetMinmaxParameteriv;
typedef void (APIENTRYP PFNGLHISTOGRAMPROC)(GLenum, GLsizei, GLenum, GLboolean);
GLAPI PFNGLHISTOGRAMPROC glad_glHistogram;
typedef void (APIENTRYP PFNGLMINMAXPROC)(GLenum, GLenum, GLboolean);
GLAPI PFNGLMINMAXPROC glad_glMinmax;
typedef void (APIENTRYP PFNGLRESETHISTOGRAMPROC)(GLenum);
GLAPI PFNGLRESETHISTOGRAMPROC glad_glResetHistogram;
typedef void (APIENTRYP PFNGLRESETMINMAXPROC)(GLenum);
GLAPI PFNGLRESETMINMAXPROC glad_glResetMinmax;
#endif
#ifndef GL_EXT_texture_compression_s3tc
	#define GL_EXT_texture_compression_s3tc 1
GLAPI int GLAD_GL_EXT_texture_compression_s3tc;
#endif
#ifndef GL_EXT_texture_sRGB
	#define GL_EXT_texture_sRGB 1
GLAPI int GLAD_GL_EXT_texture_sRGB;
#endif
#ifndef GL_EXT_texture_filter_anisotropic
	#define GL_EXT_texture_filter_anisotropic 1
GLAPI int GLAD_GL_EXT_texture_filter_anisotropic;
#endif
#ifndef GL_ARB_texture_compression_bptc
	#define GL_ARB_texture_compression_bptc 1
GLAPI int GLAD_GL_ARB_texture_compression_bptc;
#endif
#ifndef GL_ARB_compute_shader
	#define GL_ARB_compute_shader 1
GLAPI int GLAD_GL_ARB_compute_shader;
#endif
#ifndef GL_GREMEDY_string_marker
	#define GL_GREMEDY_string_marker 1
GLAPI int GLAD_GL_GREMEDY_string_marker;
typedef void (APIENTRYP PFNGLSTRINGMARKERGREMEDYPROC)(GLsizei, const void*);
GLAPI PFNGLSTRINGMARKERGREMEDYPROC glad_glStringMarkerGREMEDY;
#endif
#ifndef GL_KHR_debug
	#define GL_KHR_debug 1
GLAPI int GLAD_GL_KHR_debug;
typedef void (APIENTRYP   PFNGLGETPOINTERVPROC)(GLenum, void**);
GLAPI PFNGLGETPOINTERVPROC glad_glGetPointerv;
typedef void (APIENTRYP   PFNGLDEBUGMESSAGECONTROLKHRPROC)(GLenum, GLenum, GLenum, GLsizei, const GLuint*, GLboolean);
GLAPI PFNGLDEBUGMESSAGECONTROLKHRPROC glad_glDebugMessageControlKHR;
typedef void (APIENTRYP   PFNGLDEBUGMESSAGEINSERTKHRPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*);
GLAPI PFNGLDEBUGMESSAGEINSERTKHRPROC glad_glDebugMessageInsertKHR;
typedef void (APIENTRYP   PFNGLDEBUGMESSAGECALLBACKKHRPROC)(GLDEBUGPROCKHR, const void*);
GLAPI PFNGLDEBUGMESSAGECALLBACKKHRPROC glad_glDebugMessageCallbackKHR;
typedef GLuint (APIENTRYP PFNGLGETDEBUGMESSAGELOGKHRPROC)(GLuint, GLsizei, GLenum*, GLenum*, GLuint*, GLenum*, GLsizei*, GLchar*);
GLAPI PFNGLGETDEBUGMESSAGELOGKHRPROC glad_glGetDebugMessageLogKHR;
typedef void (APIENTRYP   PFNGLPUSHDEBUGGROUPKHRPROC)(GLenum, GLuint, GLsizei, const GLchar*);
GLAPI PFNGLPUSHDEBUGGROUPKHRPROC glad_glPushDebugGroupKHR;
typedef void (APIENTRYP   PFNGLPOPDEBUGGROUPKHRPROC)();
GLAPI PFNGLPOPDEBUGGROUPKHRPROC glad_glPopDebugGroupKHR;
typedef void (APIENTRYP   PFNGLOBJECTLABELKHRPROC)(GLenum, GLuint, GLsizei, const GLchar*);
GLAPI PFNGLOBJECTLABELKHRPROC glad_glObjectLabelKHR;
typedef void (APIENTRYP   PFNGLGETOBJECTLABELKHRPROC)(GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETOBJECTLABELKHRPROC glad_glGetObjectLabelKHR;
typedef void (APIENTRYP   PFNGLOBJECTPTRLABELKHRPROC)(const void*, GLsizei, const GLchar*);
GLAPI PFNGLOBJECTPTRLABELKHRPROC glad_glObjectPtrLabelKHR;
typedef void (APIENTRYP   PFNGLGETOBJECTPTRLABELKHRPROC)(const void*, GLsizei, GLsizei*, GLchar*);
GLAPI PFNGLGETOBJECTPTRLABELKHRPROC glad_glGetObjectPtrLabelKHR;
typedef void (APIENTRYP   PFNGLGETPOINTERVKHRPROC)(GLenum, void**);
GLAPI PFNGLGETPOINTERVKHRPROC glad_glGetPointervKHR;
#endif
#ifndef GL_NVX_gpu_memory_info
	#define GL_NVX_gpu_memory_info 1
GLAPI int GLAD_GL_NVX_gpu_memory_info;
#endif
#ifndef GL_ATI_meminfo
	#define GL_ATI_meminfo 1
GLAPI int GLAD_GL_ATI_meminfo;
#endif
#ifndef GL_ARB_internalformat_query
	#define GL_ARB_internalformat_query 1
GLAPI int GLAD_GL_ARB_internalformat_query;
#endif
#ifndef GL_ARB_internalformat_query2
	#define GL_ARB_internalformat_query2 1
GLAPI int GLAD_GL_ARB_internalformat_query2;
#endif
#ifndef GL_EXT_direct_state_access
	#define GL_EXT_direct_state_access 1
GLAPI int GLAD_GL_EXT_direct_state_access;
typedef void (APIENTRYP      PFNGLMATRIXLOADFEXTPROC)(GLenum, const GLfloat*);
GLAPI PFNGLMATRIXLOADFEXTPROC glad_glMatrixLoadfEXT;
typedef void (APIENTRYP      PFNGLMATRIXLOADDEXTPROC)(GLenum, const GLdouble*);
GLAPI PFNGLMATRIXLOADDEXTPROC glad_glMatrixLoaddEXT;
typedef void (APIENTRYP      PFNGLMATRIXMULTFEXTPROC)(GLenum, const GLfloat*);
GLAPI PFNGLMATRIXMULTFEXTPROC glad_glMatrixMultfEXT;
typedef void (APIENTRYP      PFNGLMATRIXMULTDEXTPROC)(GLenum, const GLdouble*);
GLAPI PFNGLMATRIXMULTDEXTPROC glad_glMatrixMultdEXT;
typedef void (APIENTRYP      PFNGLMATRIXLOADIDENTITYEXTPROC)(GLenum);
GLAPI PFNGLMATRIXLOADIDENTITYEXTPROC glad_glMatrixLoadIdentityEXT;
typedef void (APIENTRYP      PFNGLMATRIXROTATEFEXTPROC)(GLenum, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLMATRIXROTATEFEXTPROC glad_glMatrixRotatefEXT;
typedef void (APIENTRYP      PFNGLMATRIXROTATEDEXTPROC)(GLenum, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLMATRIXROTATEDEXTPROC glad_glMatrixRotatedEXT;
typedef void (APIENTRYP      PFNGLMATRIXSCALEFEXTPROC)(GLenum, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLMATRIXSCALEFEXTPROC glad_glMatrixScalefEXT;
typedef void (APIENTRYP      PFNGLMATRIXSCALEDEXTPROC)(GLenum, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLMATRIXSCALEDEXTPROC glad_glMatrixScaledEXT;
typedef void (APIENTRYP      PFNGLMATRIXTRANSLATEFEXTPROC)(GLenum, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLMATRIXTRANSLATEFEXTPROC glad_glMatrixTranslatefEXT;
typedef void (APIENTRYP      PFNGLMATRIXTRANSLATEDEXTPROC)(GLenum, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLMATRIXTRANSLATEDEXTPROC glad_glMatrixTranslatedEXT;
typedef void (APIENTRYP      PFNGLMATRIXFRUSTUMEXTPROC)(GLenum, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLMATRIXFRUSTUMEXTPROC glad_glMatrixFrustumEXT;
typedef void (APIENTRYP      PFNGLMATRIXORTHOEXTPROC)(GLenum, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLMATRIXORTHOEXTPROC glad_glMatrixOrthoEXT;
typedef void (APIENTRYP      PFNGLMATRIXPOPEXTPROC)(GLenum);
GLAPI PFNGLMATRIXPOPEXTPROC glad_glMatrixPopEXT;
typedef void (APIENTRYP      PFNGLMATRIXPUSHEXTPROC)(GLenum);
GLAPI PFNGLMATRIXPUSHEXTPROC glad_glMatrixPushEXT;
typedef void (APIENTRYP      PFNGLCLIENTATTRIBDEFAULTEXTPROC)(GLbitfield);
GLAPI PFNGLCLIENTATTRIBDEFAULTEXTPROC glad_glClientAttribDefaultEXT;
typedef void (APIENTRYP      PFNGLPUSHCLIENTATTRIBDEFAULTEXTPROC)(GLbitfield);
GLAPI PFNGLPUSHCLIENTATTRIBDEFAULTEXTPROC glad_glPushClientAttribDefaultEXT;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERFEXTPROC)(GLuint, GLenum, GLenum, GLfloat);
GLAPI PFNGLTEXTUREPARAMETERFEXTPROC glad_glTextureParameterfEXT;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERFVEXTPROC)(GLuint, GLenum, GLenum, const GLfloat*);
GLAPI PFNGLTEXTUREPARAMETERFVEXTPROC glad_glTextureParameterfvEXT;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIEXTPROC)(GLuint, GLenum, GLenum, GLint);
GLAPI PFNGLTEXTUREPARAMETERIEXTPROC glad_glTextureParameteriEXT;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIVEXTPROC)(GLuint, GLenum, GLenum, const GLint*);
GLAPI PFNGLTEXTUREPARAMETERIVEXTPROC glad_glTextureParameterivEXT;
typedef void (APIENTRYP      PFNGLTEXTUREIMAGE1DEXTPROC)(GLuint, GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTUREIMAGE1DEXTPROC glad_glTextureImage1DEXT;
typedef void (APIENTRYP      PFNGLTEXTUREIMAGE2DEXTPROC)(GLuint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTUREIMAGE2DEXTPROC glad_glTextureImage2DEXT;
typedef void (APIENTRYP      PFNGLTEXTURESUBIMAGE1DEXTPROC)(GLuint, GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTURESUBIMAGE1DEXTPROC glad_glTextureSubImage1DEXT;
typedef void (APIENTRYP      PFNGLTEXTURESUBIMAGE2DEXTPROC)(GLuint, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTURESUBIMAGE2DEXTPROC glad_glTextureSubImage2DEXT;
typedef void (APIENTRYP      PFNGLCOPYTEXTUREIMAGE1DEXTPROC)(GLuint, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint);
GLAPI PFNGLCOPYTEXTUREIMAGE1DEXTPROC glad_glCopyTextureImage1DEXT;
typedef void (APIENTRYP      PFNGLCOPYTEXTUREIMAGE2DEXTPROC)(GLuint, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
GLAPI PFNGLCOPYTEXTUREIMAGE2DEXTPROC glad_glCopyTextureImage2DEXT;
typedef void (APIENTRYP      PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei);
GLAPI PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC glad_glCopyTextureSubImage1DEXT;
typedef void (APIENTRYP      PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC glad_glCopyTextureSubImage2DEXT;
typedef void (APIENTRYP      PFNGLGETTEXTUREIMAGEEXTPROC)(GLuint, GLenum, GLint, GLenum, GLenum, void*);
GLAPI PFNGLGETTEXTUREIMAGEEXTPROC glad_glGetTextureImageEXT;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERFVEXTPROC)(GLuint, GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETTEXTUREPARAMETERFVEXTPROC glad_glGetTextureParameterfvEXT;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERIVEXTPROC)(GLuint, GLenum, GLenum, GLint*);
GLAPI PFNGLGETTEXTUREPARAMETERIVEXTPROC glad_glGetTextureParameterivEXT;
typedef void (APIENTRYP      PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC)(GLuint, GLenum, GLint, GLenum, GLfloat*);
GLAPI PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC glad_glGetTextureLevelParameterfvEXT;
typedef void (APIENTRYP      PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC)(GLuint, GLenum, GLint, GLenum, GLint*);
GLAPI PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC glad_glGetTextureLevelParameterivEXT;
typedef void (APIENTRYP      PFNGLTEXTUREIMAGE3DEXTPROC)(GLuint, GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTUREIMAGE3DEXTPROC glad_glTextureImage3DEXT;
typedef void (APIENTRYP      PFNGLTEXTURESUBIMAGE3DEXTPROC)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLTEXTURESUBIMAGE3DEXTPROC glad_glTextureSubImage3DEXT;
typedef void (APIENTRYP      PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC glad_glCopyTextureSubImage3DEXT;
typedef void (APIENTRYP      PFNGLBINDMULTITEXTUREEXTPROC)(GLenum, GLenum, GLuint);
GLAPI PFNGLBINDMULTITEXTUREEXTPROC glad_glBindMultiTextureEXT;
typedef void (APIENTRYP      PFNGLMULTITEXCOORDPOINTEREXTPROC)(GLenum, GLint, GLenum, GLsizei, const void*);
GLAPI PFNGLMULTITEXCOORDPOINTEREXTPROC glad_glMultiTexCoordPointerEXT;
typedef void (APIENTRYP      PFNGLMULTITEXENVFEXTPROC)(GLenum, GLenum, GLenum, GLfloat);
GLAPI PFNGLMULTITEXENVFEXTPROC glad_glMultiTexEnvfEXT;
typedef void (APIENTRYP      PFNGLMULTITEXENVFVEXTPROC)(GLenum, GLenum, GLenum, const GLfloat*);
GLAPI PFNGLMULTITEXENVFVEXTPROC glad_glMultiTexEnvfvEXT;
typedef void (APIENTRYP      PFNGLMULTITEXENVIEXTPROC)(GLenum, GLenum, GLenum, GLint);
GLAPI PFNGLMULTITEXENVIEXTPROC glad_glMultiTexEnviEXT;
typedef void (APIENTRYP      PFNGLMULTITEXENVIVEXTPROC)(GLenum, GLenum, GLenum, const GLint*);
GLAPI PFNGLMULTITEXENVIVEXTPROC glad_glMultiTexEnvivEXT;
typedef void (APIENTRYP      PFNGLMULTITEXGENDEXTPROC)(GLenum, GLenum, GLenum, GLdouble);
GLAPI PFNGLMULTITEXGENDEXTPROC glad_glMultiTexGendEXT;
typedef void (APIENTRYP      PFNGLMULTITEXGENDVEXTPROC)(GLenum, GLenum, GLenum, const GLdouble*);
GLAPI PFNGLMULTITEXGENDVEXTPROC glad_glMultiTexGendvEXT;
typedef void (APIENTRYP      PFNGLMULTITEXGENFEXTPROC)(GLenum, GLenum, GLenum, GLfloat);
GLAPI PFNGLMULTITEXGENFEXTPROC glad_glMultiTexGenfEXT;
typedef void (APIENTRYP      PFNGLMULTITEXGENFVEXTPROC)(GLenum, GLenum, GLenum, const GLfloat*);
GLAPI PFNGLMULTITEXGENFVEXTPROC glad_glMultiTexGenfvEXT;
typedef void (APIENTRYP      PFNGLMULTITEXGENIEXTPROC)(GLenum, GLenum, GLenum, GLint);
GLAPI PFNGLMULTITEXGENIEXTPROC glad_glMultiTexGeniEXT;
typedef void (APIENTRYP      PFNGLMULTITEXGENIVEXTPROC)(GLenum, GLenum, GLenum, const GLint*);
GLAPI PFNGLMULTITEXGENIVEXTPROC glad_glMultiTexGenivEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXENVFVEXTPROC)(GLenum, GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETMULTITEXENVFVEXTPROC glad_glGetMultiTexEnvfvEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXENVIVEXTPROC)(GLenum, GLenum, GLenum, GLint*);
GLAPI PFNGLGETMULTITEXENVIVEXTPROC glad_glGetMultiTexEnvivEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXGENDVEXTPROC)(GLenum, GLenum, GLenum, GLdouble*);
GLAPI PFNGLGETMULTITEXGENDVEXTPROC glad_glGetMultiTexGendvEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXGENFVEXTPROC)(GLenum, GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETMULTITEXGENFVEXTPROC glad_glGetMultiTexGenfvEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXGENIVEXTPROC)(GLenum, GLenum, GLenum, GLint*);
GLAPI PFNGLGETMULTITEXGENIVEXTPROC glad_glGetMultiTexGenivEXT;
typedef void (APIENTRYP      PFNGLMULTITEXPARAMETERIEXTPROC)(GLenum, GLenum, GLenum, GLint);
GLAPI PFNGLMULTITEXPARAMETERIEXTPROC glad_glMultiTexParameteriEXT;
typedef void (APIENTRYP      PFNGLMULTITEXPARAMETERIVEXTPROC)(GLenum, GLenum, GLenum, const GLint*);
GLAPI PFNGLMULTITEXPARAMETERIVEXTPROC glad_glMultiTexParameterivEXT;
typedef void (APIENTRYP      PFNGLMULTITEXPARAMETERFEXTPROC)(GLenum, GLenum, GLenum, GLfloat);
GLAPI PFNGLMULTITEXPARAMETERFEXTPROC glad_glMultiTexParameterfEXT;
typedef void (APIENTRYP      PFNGLMULTITEXPARAMETERFVEXTPROC)(GLenum, GLenum, GLenum, const GLfloat*);
GLAPI PFNGLMULTITEXPARAMETERFVEXTPROC glad_glMultiTexParameterfvEXT;
typedef void (APIENTRYP      PFNGLMULTITEXIMAGE1DEXTPROC)(GLenum, GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLMULTITEXIMAGE1DEXTPROC glad_glMultiTexImage1DEXT;
typedef void (APIENTRYP      PFNGLMULTITEXIMAGE2DEXTPROC)(GLenum, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLMULTITEXIMAGE2DEXTPROC glad_glMultiTexImage2DEXT;
typedef void (APIENTRYP      PFNGLMULTITEXSUBIMAGE1DEXTPROC)(GLenum, GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLMULTITEXSUBIMAGE1DEXTPROC glad_glMultiTexSubImage1DEXT;
typedef void (APIENTRYP      PFNGLMULTITEXSUBIMAGE2DEXTPROC)(GLenum, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLMULTITEXSUBIMAGE2DEXTPROC glad_glMultiTexSubImage2DEXT;
typedef void (APIENTRYP      PFNGLCOPYMULTITEXIMAGE1DEXTPROC)(GLenum, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint);
GLAPI PFNGLCOPYMULTITEXIMAGE1DEXTPROC glad_glCopyMultiTexImage1DEXT;
typedef void (APIENTRYP      PFNGLCOPYMULTITEXIMAGE2DEXTPROC)(GLenum, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
GLAPI PFNGLCOPYMULTITEXIMAGE2DEXTPROC glad_glCopyMultiTexImage2DEXT;
typedef void (APIENTRYP      PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC)(GLenum, GLenum, GLint, GLint, GLint, GLint, GLsizei);
GLAPI PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC glad_glCopyMultiTexSubImage1DEXT;
typedef void (APIENTRYP      PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC)(GLenum, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC glad_glCopyMultiTexSubImage2DEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXIMAGEEXTPROC)(GLenum, GLenum, GLint, GLenum, GLenum, void*);
GLAPI PFNGLGETMULTITEXIMAGEEXTPROC glad_glGetMultiTexImageEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXPARAMETERFVEXTPROC)(GLenum, GLenum, GLenum, GLfloat*);
GLAPI PFNGLGETMULTITEXPARAMETERFVEXTPROC glad_glGetMultiTexParameterfvEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXPARAMETERIVEXTPROC)(GLenum, GLenum, GLenum, GLint*);
GLAPI PFNGLGETMULTITEXPARAMETERIVEXTPROC glad_glGetMultiTexParameterivEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC)(GLenum, GLenum, GLint, GLenum, GLfloat*);
GLAPI PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC glad_glGetMultiTexLevelParameterfvEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC)(GLenum, GLenum, GLint, GLenum, GLint*);
GLAPI PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC glad_glGetMultiTexLevelParameterivEXT;
typedef void (APIENTRYP      PFNGLMULTITEXIMAGE3DEXTPROC)(GLenum, GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI PFNGLMULTITEXIMAGE3DEXTPROC glad_glMultiTexImage3DEXT;
typedef void (APIENTRYP      PFNGLMULTITEXSUBIMAGE3DEXTPROC)(GLenum, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLAPI PFNGLMULTITEXSUBIMAGE3DEXTPROC glad_glMultiTexSubImage3DEXT;
typedef void (APIENTRYP      PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC)(GLenum, GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
GLAPI PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC glad_glCopyMultiTexSubImage3DEXT;
typedef void (APIENTRYP      PFNGLENABLECLIENTSTATEINDEXEDEXTPROC)(GLenum, GLuint);
GLAPI PFNGLENABLECLIENTSTATEINDEXEDEXTPROC glad_glEnableClientStateIndexedEXT;
typedef void (APIENTRYP      PFNGLDISABLECLIENTSTATEINDEXEDEXTPROC)(GLenum, GLuint);
GLAPI PFNGLDISABLECLIENTSTATEINDEXEDEXTPROC glad_glDisableClientStateIndexedEXT;
typedef void (APIENTRYP      PFNGLGETFLOATINDEXEDVEXTPROC)(GLenum, GLuint, GLfloat*);
GLAPI PFNGLGETFLOATINDEXEDVEXTPROC glad_glGetFloatIndexedvEXT;
typedef void (APIENTRYP      PFNGLGETDOUBLEINDEXEDVEXTPROC)(GLenum, GLuint, GLdouble*);
GLAPI PFNGLGETDOUBLEINDEXEDVEXTPROC glad_glGetDoubleIndexedvEXT;
typedef void (APIENTRYP      PFNGLGETPOINTERINDEXEDVEXTPROC)(GLenum, GLuint, void**);
GLAPI PFNGLGETPOINTERINDEXEDVEXTPROC glad_glGetPointerIndexedvEXT;
typedef void (APIENTRYP      PFNGLENABLEINDEXEDEXTPROC)(GLenum, GLuint);
GLAPI PFNGLENABLEINDEXEDEXTPROC glad_glEnableIndexedEXT;
typedef void (APIENTRYP      PFNGLDISABLEINDEXEDEXTPROC)(GLenum, GLuint);
GLAPI PFNGLDISABLEINDEXEDEXTPROC glad_glDisableIndexedEXT;
typedef GLboolean (APIENTRYP PFNGLISENABLEDINDEXEDEXTPROC)(GLenum, GLuint);
GLAPI PFNGLISENABLEDINDEXEDEXTPROC glad_glIsEnabledIndexedEXT;
typedef void (APIENTRYP      PFNGLGETINTEGERINDEXEDVEXTPROC)(GLenum, GLuint, GLint*);
GLAPI PFNGLGETINTEGERINDEXEDVEXTPROC glad_glGetIntegerIndexedvEXT;
typedef void (APIENTRYP      PFNGLGETBOOLEANINDEXEDVEXTPROC)(GLenum, GLuint, GLboolean*);
GLAPI PFNGLGETBOOLEANINDEXEDVEXTPROC glad_glGetBooleanIndexedvEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC)(GLuint, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC glad_glCompressedTextureImage3DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC)(GLuint, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC glad_glCompressedTextureImage2DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC)(GLuint, GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC glad_glCompressedTextureImage1DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC glad_glCompressedTextureSubImage3DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC)(GLuint, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC glad_glCompressedTextureSubImage2DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC)(GLuint, GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC glad_glCompressedTextureSubImage1DEXT;
typedef void (APIENTRYP      PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC)(GLuint, GLenum, GLint, void*);
GLAPI PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC glad_glGetCompressedTextureImageEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC)(GLenum, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC glad_glCompressedMultiTexImage3DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC)(GLenum, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC glad_glCompressedMultiTexImage2DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC)(GLenum, GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC glad_glCompressedMultiTexImage1DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC)(GLenum, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC glad_glCompressedMultiTexSubImage3DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC)(GLenum, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC glad_glCompressedMultiTexSubImage2DEXT;
typedef void (APIENTRYP      PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC)(GLenum, GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void*);
GLAPI PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC glad_glCompressedMultiTexSubImage1DEXT;
typedef void (APIENTRYP      PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC)(GLenum, GLenum, GLint, void*);
GLAPI PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC glad_glGetCompressedMultiTexImageEXT;
typedef void (APIENTRYP      PFNGLMATRIXLOADTRANSPOSEFEXTPROC)(GLenum, const GLfloat*);
GLAPI PFNGLMATRIXLOADTRANSPOSEFEXTPROC glad_glMatrixLoadTransposefEXT;
typedef void (APIENTRYP      PFNGLMATRIXLOADTRANSPOSEDEXTPROC)(GLenum, const GLdouble*);
GLAPI PFNGLMATRIXLOADTRANSPOSEDEXTPROC glad_glMatrixLoadTransposedEXT;
typedef void (APIENTRYP      PFNGLMATRIXMULTTRANSPOSEFEXTPROC)(GLenum, const GLfloat*);
GLAPI PFNGLMATRIXMULTTRANSPOSEFEXTPROC glad_glMatrixMultTransposefEXT;
typedef void (APIENTRYP      PFNGLMATRIXMULTTRANSPOSEDEXTPROC)(GLenum, const GLdouble*);
GLAPI PFNGLMATRIXMULTTRANSPOSEDEXTPROC glad_glMatrixMultTransposedEXT;
typedef void (APIENTRYP      PFNGLNAMEDBUFFERDATAEXTPROC)(GLuint, GLsizeiptr, const void*, GLenum);
GLAPI PFNGLNAMEDBUFFERDATAEXTPROC glad_glNamedBufferDataEXT;
typedef void (APIENTRYP      PFNGLNAMEDBUFFERSUBDATAEXTPROC)(GLuint, GLintptr, GLsizeiptr, const void*);
GLAPI PFNGLNAMEDBUFFERSUBDATAEXTPROC glad_glNamedBufferSubDataEXT;
typedef void* (APIENTRYP     PFNGLMAPNAMEDBUFFEREXTPROC)(GLuint, GLenum);
GLAPI PFNGLMAPNAMEDBUFFEREXTPROC glad_glMapNamedBufferEXT;
typedef GLboolean (APIENTRYP PFNGLUNMAPNAMEDBUFFEREXTPROC)(GLuint);
GLAPI PFNGLUNMAPNAMEDBUFFEREXTPROC glad_glUnmapNamedBufferEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC glad_glGetNamedBufferParameterivEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERPOINTERVEXTPROC)(GLuint, GLenum, void**);
GLAPI PFNGLGETNAMEDBUFFERPOINTERVEXTPROC glad_glGetNamedBufferPointervEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERSUBDATAEXTPROC)(GLuint, GLintptr, GLsizeiptr, void*);
GLAPI PFNGLGETNAMEDBUFFERSUBDATAEXTPROC glad_glGetNamedBufferSubDataEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1FEXTPROC)(GLuint, GLint, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM1FEXTPROC glad_glProgramUniform1fEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2FEXTPROC)(GLuint, GLint, GLfloat, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM2FEXTPROC glad_glProgramUniform2fEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3FEXTPROC)(GLuint, GLint, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM3FEXTPROC glad_glProgramUniform3fEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4FEXTPROC)(GLuint, GLint, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLPROGRAMUNIFORM4FEXTPROC glad_glProgramUniform4fEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1IEXTPROC)(GLuint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM1IEXTPROC glad_glProgramUniform1iEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2IEXTPROC)(GLuint, GLint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM2IEXTPROC glad_glProgramUniform2iEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3IEXTPROC)(GLuint, GLint, GLint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM3IEXTPROC glad_glProgramUniform3iEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4IEXTPROC)(GLuint, GLint, GLint, GLint, GLint, GLint);
GLAPI PFNGLPROGRAMUNIFORM4IEXTPROC glad_glProgramUniform4iEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1FVEXTPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM1FVEXTPROC glad_glProgramUniform1fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2FVEXTPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM2FVEXTPROC glad_glProgramUniform2fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3FVEXTPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM3FVEXTPROC glad_glProgramUniform3fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4FVEXTPROC)(GLuint, GLint, GLsizei, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORM4FVEXTPROC glad_glProgramUniform4fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1IVEXTPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM1IVEXTPROC glad_glProgramUniform1ivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2IVEXTPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM2IVEXTPROC glad_glProgramUniform2ivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3IVEXTPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM3IVEXTPROC glad_glProgramUniform3ivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4IVEXTPROC)(GLuint, GLint, GLsizei, const GLint*);
GLAPI PFNGLPROGRAMUNIFORM4IVEXTPROC glad_glProgramUniform4ivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC glad_glProgramUniformMatrix2fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC glad_glProgramUniformMatrix3fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC glad_glProgramUniformMatrix4fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC glad_glProgramUniformMatrix2x3fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC glad_glProgramUniformMatrix3x2fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC glad_glProgramUniformMatrix2x4fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC glad_glProgramUniformMatrix4x2fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC glad_glProgramUniformMatrix3x4fvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLfloat*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC glad_glProgramUniformMatrix4x3fvEXT;
typedef void (APIENTRYP      PFNGLTEXTUREBUFFEREXTPROC)(GLuint, GLenum, GLenum, GLuint);
GLAPI PFNGLTEXTUREBUFFEREXTPROC glad_glTextureBufferEXT;
typedef void (APIENTRYP      PFNGLMULTITEXBUFFEREXTPROC)(GLenum, GLenum, GLenum, GLuint);
GLAPI PFNGLMULTITEXBUFFEREXTPROC glad_glMultiTexBufferEXT;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIIVEXTPROC)(GLuint, GLenum, GLenum, const GLint*);
GLAPI PFNGLTEXTUREPARAMETERIIVEXTPROC glad_glTextureParameterIivEXT;
typedef void (APIENTRYP      PFNGLTEXTUREPARAMETERIUIVEXTPROC)(GLuint, GLenum, GLenum, const GLuint*);
GLAPI PFNGLTEXTUREPARAMETERIUIVEXTPROC glad_glTextureParameterIuivEXT;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERIIVEXTPROC)(GLuint, GLenum, GLenum, GLint*);
GLAPI PFNGLGETTEXTUREPARAMETERIIVEXTPROC glad_glGetTextureParameterIivEXT;
typedef void (APIENTRYP      PFNGLGETTEXTUREPARAMETERIUIVEXTPROC)(GLuint, GLenum, GLenum, GLuint*);
GLAPI PFNGLGETTEXTUREPARAMETERIUIVEXTPROC glad_glGetTextureParameterIuivEXT;
typedef void (APIENTRYP      PFNGLMULTITEXPARAMETERIIVEXTPROC)(GLenum, GLenum, GLenum, const GLint*);
GLAPI PFNGLMULTITEXPARAMETERIIVEXTPROC glad_glMultiTexParameterIivEXT;
typedef void (APIENTRYP      PFNGLMULTITEXPARAMETERIUIVEXTPROC)(GLenum, GLenum, GLenum, const GLuint*);
GLAPI PFNGLMULTITEXPARAMETERIUIVEXTPROC glad_glMultiTexParameterIuivEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXPARAMETERIIVEXTPROC)(GLenum, GLenum, GLenum, GLint*);
GLAPI PFNGLGETMULTITEXPARAMETERIIVEXTPROC glad_glGetMultiTexParameterIivEXT;
typedef void (APIENTRYP      PFNGLGETMULTITEXPARAMETERIUIVEXTPROC)(GLenum, GLenum, GLenum, GLuint*);
GLAPI PFNGLGETMULTITEXPARAMETERIUIVEXTPROC glad_glGetMultiTexParameterIuivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1UIEXTPROC)(GLuint, GLint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM1UIEXTPROC glad_glProgramUniform1uiEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2UIEXTPROC)(GLuint, GLint, GLuint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM2UIEXTPROC glad_glProgramUniform2uiEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3UIEXTPROC)(GLuint, GLint, GLuint, GLuint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM3UIEXTPROC glad_glProgramUniform3uiEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4UIEXTPROC)(GLuint, GLint, GLuint, GLuint, GLuint, GLuint);
GLAPI PFNGLPROGRAMUNIFORM4UIEXTPROC glad_glProgramUniform4uiEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1UIVEXTPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM1UIVEXTPROC glad_glProgramUniform1uivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2UIVEXTPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM2UIVEXTPROC glad_glProgramUniform2uivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3UIVEXTPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM3UIVEXTPROC glad_glProgramUniform3uivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4UIVEXTPROC)(GLuint, GLint, GLsizei, const GLuint*);
GLAPI PFNGLPROGRAMUNIFORM4UIVEXTPROC glad_glProgramUniform4uivEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC)(GLuint, GLenum, GLuint, GLsizei, const GLfloat*);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC glad_glNamedProgramLocalParameters4fvEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC)(GLuint, GLenum, GLuint, GLint, GLint, GLint, GLint);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC glad_glNamedProgramLocalParameterI4iEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC)(GLuint, GLenum, GLuint, const GLint*);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC glad_glNamedProgramLocalParameterI4ivEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC)(GLuint, GLenum, GLuint, GLsizei, const GLint*);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC glad_glNamedProgramLocalParametersI4ivEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC)(GLuint, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC glad_glNamedProgramLocalParameterI4uiEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC)(GLuint, GLenum, GLuint, const GLuint*);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC glad_glNamedProgramLocalParameterI4uivEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC)(GLuint, GLenum, GLuint, GLsizei, const GLuint*);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC glad_glNamedProgramLocalParametersI4uivEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC)(GLuint, GLenum, GLuint, GLint*);
GLAPI PFNGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC glad_glGetNamedProgramLocalParameterIivEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC)(GLuint, GLenum, GLuint, GLuint*);
GLAPI PFNGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC glad_glGetNamedProgramLocalParameterIuivEXT;
typedef void (APIENTRYP      PFNGLENABLECLIENTSTATEIEXTPROC)(GLenum, GLuint);
GLAPI PFNGLENABLECLIENTSTATEIEXTPROC glad_glEnableClientStateiEXT;
typedef void (APIENTRYP      PFNGLDISABLECLIENTSTATEIEXTPROC)(GLenum, GLuint);
GLAPI PFNGLDISABLECLIENTSTATEIEXTPROC glad_glDisableClientStateiEXT;
typedef void (APIENTRYP      PFNGLGETFLOATI_VEXTPROC)(GLenum, GLuint, GLfloat*);
GLAPI PFNGLGETFLOATI_VEXTPROC glad_glGetFloati_vEXT;
typedef void (APIENTRYP      PFNGLGETDOUBLEI_VEXTPROC)(GLenum, GLuint, GLdouble*);
GLAPI PFNGLGETDOUBLEI_VEXTPROC glad_glGetDoublei_vEXT;
typedef void (APIENTRYP      PFNGLGETPOINTERI_VEXTPROC)(GLenum, GLuint, void**);
GLAPI PFNGLGETPOINTERI_VEXTPROC glad_glGetPointeri_vEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMSTRINGEXTPROC)(GLuint, GLenum, GLenum, GLsizei, const void*);
GLAPI PFNGLNAMEDPROGRAMSTRINGEXTPROC glad_glNamedProgramStringEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC)(GLuint, GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC glad_glNamedProgramLocalParameter4dEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC)(GLuint, GLenum, GLuint, const GLdouble*);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC glad_glNamedProgramLocalParameter4dvEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC)(GLuint, GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC glad_glNamedProgramLocalParameter4fEXT;
typedef void (APIENTRYP      PFNGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC)(GLuint, GLenum, GLuint, const GLfloat*);
GLAPI PFNGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC glad_glNamedProgramLocalParameter4fvEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC)(GLuint, GLenum, GLuint, GLdouble*);
GLAPI PFNGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC glad_glGetNamedProgramLocalParameterdvEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC)(GLuint, GLenum, GLuint, GLfloat*);
GLAPI PFNGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC glad_glGetNamedProgramLocalParameterfvEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDPROGRAMIVEXTPROC)(GLuint, GLenum, GLenum, GLint*);
GLAPI PFNGLGETNAMEDPROGRAMIVEXTPROC glad_glGetNamedProgramivEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDPROGRAMSTRINGEXTPROC)(GLuint, GLenum, GLenum, void*);
GLAPI PFNGLGETNAMEDPROGRAMSTRINGEXTPROC glad_glGetNamedProgramStringEXT;
typedef void (APIENTRYP      PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC)(GLuint, GLenum, GLsizei, GLsizei);
GLAPI PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC glad_glNamedRenderbufferStorageEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC glad_glGetNamedRenderbufferParameterivEXT;
typedef void (APIENTRYP      PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(GLuint, GLsizei, GLenum, GLsizei, GLsizei);
GLAPI PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_glNamedRenderbufferStorageMultisampleEXT;
typedef void (APIENTRYP      PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC)(GLuint, GLsizei, GLsizei, GLenum, GLsizei, GLsizei);
GLAPI PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC glad_glNamedRenderbufferStorageMultisampleCoverageEXT;
typedef GLenum (APIENTRYP    PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC)(GLuint, GLenum);
GLAPI PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC glad_glCheckNamedFramebufferStatusEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC)(GLuint, GLenum, GLenum, GLuint, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC glad_glNamedFramebufferTexture1DEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC)(GLuint, GLenum, GLenum, GLuint, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC glad_glNamedFramebufferTexture2DEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC)(GLuint, GLenum, GLenum, GLuint, GLint, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC glad_glNamedFramebufferTexture3DEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC)(GLuint, GLenum, GLenum, GLuint);
GLAPI PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC glad_glNamedFramebufferRenderbufferEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC)(GLuint, GLenum, GLenum, GLint*);
GLAPI PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glad_glGetNamedFramebufferAttachmentParameterivEXT;
typedef void (APIENTRYP      PFNGLGENERATETEXTUREMIPMAPEXTPROC)(GLuint, GLenum);
GLAPI PFNGLGENERATETEXTUREMIPMAPEXTPROC glad_glGenerateTextureMipmapEXT;
typedef void (APIENTRYP      PFNGLGENERATEMULTITEXMIPMAPEXTPROC)(GLenum, GLenum);
GLAPI PFNGLGENERATEMULTITEXMIPMAPEXTPROC glad_glGenerateMultiTexMipmapEXT;
typedef void (APIENTRYP      PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC)(GLuint, GLenum);
GLAPI PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC glad_glFramebufferDrawBufferEXT;
typedef void (APIENTRYP      PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC)(GLuint, GLsizei, const GLenum*);
GLAPI PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC glad_glFramebufferDrawBuffersEXT;
typedef void (APIENTRYP      PFNGLFRAMEBUFFERREADBUFFEREXTPROC)(GLuint, GLenum);
GLAPI PFNGLFRAMEBUFFERREADBUFFEREXTPROC glad_glFramebufferReadBufferEXT;
typedef void (APIENTRYP      PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC glad_glGetFramebufferParameterivEXT;
typedef void (APIENTRYP      PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC)(GLuint, GLuint, GLintptr, GLintptr, GLsizeiptr);
GLAPI PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC glad_glNamedCopyBufferSubDataEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC)(GLuint, GLenum, GLuint, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC glad_glNamedFramebufferTextureEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC)(GLuint, GLenum, GLuint, GLint, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC glad_glNamedFramebufferTextureLayerEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC)(GLuint, GLenum, GLuint, GLint, GLenum);
GLAPI PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC glad_glNamedFramebufferTextureFaceEXT;
typedef void (APIENTRYP      PFNGLTEXTURERENDERBUFFEREXTPROC)(GLuint, GLenum, GLuint);
GLAPI PFNGLTEXTURERENDERBUFFEREXTPROC glad_glTextureRenderbufferEXT;
typedef void (APIENTRYP      PFNGLMULTITEXRENDERBUFFEREXTPROC)(GLenum, GLenum, GLuint);
GLAPI PFNGLMULTITEXRENDERBUFFEREXTPROC glad_glMultiTexRenderbufferEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXOFFSETEXTPROC)(GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYVERTEXOFFSETEXTPROC glad_glVertexArrayVertexOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYCOLOROFFSETEXTPROC)(GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYCOLOROFFSETEXTPROC glad_glVertexArrayColorOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC)(GLuint, GLuint, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC glad_glVertexArrayEdgeFlagOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYINDEXOFFSETEXTPROC)(GLuint, GLuint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYINDEXOFFSETEXTPROC glad_glVertexArrayIndexOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYNORMALOFFSETEXTPROC)(GLuint, GLuint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYNORMALOFFSETEXTPROC glad_glVertexArrayNormalOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYTEXCOORDOFFSETEXTPROC)(GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYTEXCOORDOFFSETEXTPROC glad_glVertexArrayTexCoordOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC)(GLuint, GLuint, GLenum, GLint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC glad_glVertexArrayMultiTexCoordOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYFOGCOORDOFFSETEXTPROC)(GLuint, GLuint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYFOGCOORDOFFSETEXTPROC glad_glVertexArrayFogCoordOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC)(GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC glad_glVertexArraySecondaryColorOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC)(GLuint, GLuint, GLuint, GLint, GLenum, GLboolean, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC glad_glVertexArrayVertexAttribOffsetEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC)(GLuint, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC glad_glVertexArrayVertexAttribIOffsetEXT;
typedef void (APIENTRYP      PFNGLENABLEVERTEXARRAYEXTPROC)(GLuint, GLenum);
GLAPI PFNGLENABLEVERTEXARRAYEXTPROC glad_glEnableVertexArrayEXT;
typedef void (APIENTRYP      PFNGLDISABLEVERTEXARRAYEXTPROC)(GLuint, GLenum);
GLAPI PFNGLDISABLEVERTEXARRAYEXTPROC glad_glDisableVertexArrayEXT;
typedef void (APIENTRYP      PFNGLENABLEVERTEXARRAYATTRIBEXTPROC)(GLuint, GLuint);
GLAPI PFNGLENABLEVERTEXARRAYATTRIBEXTPROC glad_glEnableVertexArrayAttribEXT;
typedef void (APIENTRYP      PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC)(GLuint, GLuint);
GLAPI PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC glad_glDisableVertexArrayAttribEXT;
typedef void (APIENTRYP      PFNGLGETVERTEXARRAYINTEGERVEXTPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETVERTEXARRAYINTEGERVEXTPROC glad_glGetVertexArrayIntegervEXT;
typedef void (APIENTRYP      PFNGLGETVERTEXARRAYPOINTERVEXTPROC)(GLuint, GLenum, void**);
GLAPI PFNGLGETVERTEXARRAYPOINTERVEXTPROC glad_glGetVertexArrayPointervEXT;
typedef void (APIENTRYP      PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC)(GLuint, GLuint, GLenum, GLint*);
GLAPI PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC glad_glGetVertexArrayIntegeri_vEXT;
typedef void (APIENTRYP      PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC)(GLuint, GLuint, GLenum, void**);
GLAPI PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC glad_glGetVertexArrayPointeri_vEXT;
typedef void* (APIENTRYP     PFNGLMAPNAMEDBUFFERRANGEEXTPROC)(GLuint, GLintptr, GLsizeiptr, GLbitfield);
GLAPI PFNGLMAPNAMEDBUFFERRANGEEXTPROC glad_glMapNamedBufferRangeEXT;
typedef void (APIENTRYP      PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC)(GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC glad_glFlushMappedNamedBufferRangeEXT;
typedef void (APIENTRYP      PFNGLNAMEDBUFFERSTORAGEEXTPROC)(GLuint, GLsizeiptr, const void*, GLbitfield);
GLAPI PFNGLNAMEDBUFFERSTORAGEEXTPROC glad_glNamedBufferStorageEXT;
typedef void (APIENTRYP      PFNGLCLEARNAMEDBUFFERDATAEXTPROC)(GLuint, GLenum, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARNAMEDBUFFERDATAEXTPROC glad_glClearNamedBufferDataEXT;
typedef void (APIENTRYP      PFNGLCLEARNAMEDBUFFERSUBDATAEXTPROC)(GLuint, GLenum, GLsizeiptr, GLsizeiptr, GLenum, GLenum, const void*);
GLAPI PFNGLCLEARNAMEDBUFFERSUBDATAEXTPROC glad_glClearNamedBufferSubDataEXT;
typedef void (APIENTRYP      PFNGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC)(GLuint, GLenum, GLint);
GLAPI PFNGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC glad_glNamedFramebufferParameteriEXT;
typedef void (APIENTRYP      PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC)(GLuint, GLenum, GLint*);
GLAPI PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC glad_glGetNamedFramebufferParameterivEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1DEXTPROC)(GLuint, GLint, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM1DEXTPROC glad_glProgramUniform1dEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2DEXTPROC)(GLuint, GLint, GLdouble, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM2DEXTPROC glad_glProgramUniform2dEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3DEXTPROC)(GLuint, GLint, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM3DEXTPROC glad_glProgramUniform3dEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4DEXTPROC)(GLuint, GLint, GLdouble, GLdouble, GLdouble, GLdouble);
GLAPI PFNGLPROGRAMUNIFORM4DEXTPROC glad_glProgramUniform4dEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM1DVEXTPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM1DVEXTPROC glad_glProgramUniform1dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM2DVEXTPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM2DVEXTPROC glad_glProgramUniform2dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM3DVEXTPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM3DVEXTPROC glad_glProgramUniform3dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORM4DVEXTPROC)(GLuint, GLint, GLsizei, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORM4DVEXTPROC glad_glProgramUniform4dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2DVEXTPROC glad_glProgramUniformMatrix2dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3DVEXTPROC glad_glProgramUniformMatrix3dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4DVEXTPROC glad_glProgramUniformMatrix4dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC glad_glProgramUniformMatrix2x3dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC glad_glProgramUniformMatrix2x4dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC glad_glProgramUniformMatrix3x2dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC glad_glProgramUniformMatrix3x4dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC glad_glProgramUniformMatrix4x2dvEXT;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC)(GLuint, GLint, GLsizei, GLboolean, const GLdouble*);
GLAPI PFNGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC glad_glProgramUniformMatrix4x3dvEXT;
typedef void (APIENTRYP      PFNGLTEXTUREBUFFERRANGEEXTPROC)(GLuint, GLenum, GLenum, GLuint, GLintptr, GLsizeiptr);
GLAPI PFNGLTEXTUREBUFFERRANGEEXTPROC glad_glTextureBufferRangeEXT;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE1DEXTPROC)(GLuint, GLenum, GLsizei, GLenum, GLsizei);
GLAPI PFNGLTEXTURESTORAGE1DEXTPROC glad_glTextureStorage1DEXT;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE2DEXTPROC)(GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei);
GLAPI PFNGLTEXTURESTORAGE2DEXTPROC glad_glTextureStorage2DEXT;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE3DEXTPROC)(GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
GLAPI PFNGLTEXTURESTORAGE3DEXTPROC glad_glTextureStorage3DEXT;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC)(GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC glad_glTextureStorage2DMultisampleEXT;
typedef void (APIENTRYP      PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC)(GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC glad_glTextureStorage3DMultisampleEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC)(GLuint, GLuint, GLuint, GLintptr, GLsizei);
GLAPI PFNGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC glad_glVertexArrayBindVertexBufferEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC)(GLuint, GLuint, GLint, GLenum, GLboolean, GLuint);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC glad_glVertexArrayVertexAttribFormatEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC)(GLuint, GLuint, GLint, GLenum, GLuint);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC glad_glVertexArrayVertexAttribIFormatEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC)(GLuint, GLuint, GLint, GLenum, GLuint);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC glad_glVertexArrayVertexAttribLFormatEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC glad_glVertexArrayVertexAttribBindingEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC glad_glVertexArrayVertexBindingDivisorEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC)(GLuint, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC glad_glVertexArrayVertexAttribLOffsetEXT;
typedef void (APIENTRYP      PFNGLTEXTUREPAGECOMMITMENTEXTPROC)(GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLboolean);
GLAPI PFNGLTEXTUREPAGECOMMITMENTEXTPROC glad_glTexturePageCommitmentEXT;
typedef void (APIENTRYP      PFNGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC)(GLuint, GLuint, GLuint);
GLAPI PFNGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC glad_glVertexArrayVertexAttribDivisorEXT;
#endif
#ifndef GL_NV_vertex_buffer_unified_memory
	#define GL_NV_vertex_buffer_unified_memory 1
GLAPI int GLAD_GL_NV_vertex_buffer_unified_memory;
typedef void (APIENTRYP PFNGLBUFFERADDRESSRANGENVPROC)(GLenum, GLuint, GLuint64EXT, GLsizeiptr);
GLAPI PFNGLBUFFERADDRESSRANGENVPROC glad_glBufferAddressRangeNV;
typedef void (APIENTRYP PFNGLVERTEXFORMATNVPROC)(GLint, GLenum, GLsizei);
GLAPI PFNGLVERTEXFORMATNVPROC glad_glVertexFormatNV;
typedef void (APIENTRYP PFNGLNORMALFORMATNVPROC)(GLenum, GLsizei);
GLAPI PFNGLNORMALFORMATNVPROC glad_glNormalFormatNV;
typedef void (APIENTRYP PFNGLCOLORFORMATNVPROC)(GLint, GLenum, GLsizei);
GLAPI PFNGLCOLORFORMATNVPROC glad_glColorFormatNV;
typedef void (APIENTRYP PFNGLINDEXFORMATNVPROC)(GLenum, GLsizei);
GLAPI PFNGLINDEXFORMATNVPROC glad_glIndexFormatNV;
typedef void (APIENTRYP PFNGLTEXCOORDFORMATNVPROC)(GLint, GLenum, GLsizei);
GLAPI PFNGLTEXCOORDFORMATNVPROC glad_glTexCoordFormatNV;
typedef void (APIENTRYP PFNGLEDGEFLAGFORMATNVPROC)(GLsizei);
GLAPI PFNGLEDGEFLAGFORMATNVPROC glad_glEdgeFlagFormatNV;
typedef void (APIENTRYP PFNGLSECONDARYCOLORFORMATNVPROC)(GLint, GLenum, GLsizei);
GLAPI PFNGLSECONDARYCOLORFORMATNVPROC glad_glSecondaryColorFormatNV;
typedef void (APIENTRYP PFNGLFOGCOORDFORMATNVPROC)(GLenum, GLsizei);
GLAPI PFNGLFOGCOORDFORMATNVPROC glad_glFogCoordFormatNV;
typedef void (APIENTRYP PFNGLVERTEXATTRIBFORMATNVPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei);
GLAPI PFNGLVERTEXATTRIBFORMATNVPROC glad_glVertexAttribFormatNV;
typedef void (APIENTRYP PFNGLVERTEXATTRIBIFORMATNVPROC)(GLuint, GLint, GLenum, GLsizei);
GLAPI PFNGLVERTEXATTRIBIFORMATNVPROC glad_glVertexAttribIFormatNV;
typedef void (APIENTRYP PFNGLGETINTEGERUI64I_VNVPROC)(GLenum, GLuint, GLuint64EXT*);
GLAPI PFNGLGETINTEGERUI64I_VNVPROC glad_glGetIntegerui64i_vNV;
#endif
#ifndef GL_NV_uniform_buffer_unified_memory
	#define GL_NV_uniform_buffer_unified_memory 1
GLAPI int GLAD_GL_NV_uniform_buffer_unified_memory;
#endif
#ifndef GL_NV_shader_buffer_load
	#define GL_NV_shader_buffer_load 1
GLAPI int GLAD_GL_NV_shader_buffer_load;
typedef void (APIENTRYP      PFNGLMAKEBUFFERRESIDENTNVPROC)(GLenum, GLenum);
GLAPI PFNGLMAKEBUFFERRESIDENTNVPROC glad_glMakeBufferResidentNV;
typedef void (APIENTRYP      PFNGLMAKEBUFFERNONRESIDENTNVPROC)(GLenum);
GLAPI PFNGLMAKEBUFFERNONRESIDENTNVPROC glad_glMakeBufferNonResidentNV;
typedef GLboolean (APIENTRYP PFNGLISBUFFERRESIDENTNVPROC)(GLenum);
GLAPI PFNGLISBUFFERRESIDENTNVPROC glad_glIsBufferResidentNV;
typedef void (APIENTRYP      PFNGLMAKENAMEDBUFFERRESIDENTNVPROC)(GLuint, GLenum);
GLAPI PFNGLMAKENAMEDBUFFERRESIDENTNVPROC glad_glMakeNamedBufferResidentNV;
typedef void (APIENTRYP      PFNGLMAKENAMEDBUFFERNONRESIDENTNVPROC)(GLuint);
GLAPI PFNGLMAKENAMEDBUFFERNONRESIDENTNVPROC glad_glMakeNamedBufferNonResidentNV;
typedef GLboolean (APIENTRYP PFNGLISNAMEDBUFFERRESIDENTNVPROC)(GLuint);
GLAPI PFNGLISNAMEDBUFFERRESIDENTNVPROC glad_glIsNamedBufferResidentNV;
typedef void (APIENTRYP      PFNGLGETBUFFERPARAMETERUI64VNVPROC)(GLenum, GLenum, GLuint64EXT*);
GLAPI PFNGLGETBUFFERPARAMETERUI64VNVPROC glad_glGetBufferParameterui64vNV;
typedef void (APIENTRYP      PFNGLGETNAMEDBUFFERPARAMETERUI64VNVPROC)(GLuint, GLenum, GLuint64EXT*);
GLAPI PFNGLGETNAMEDBUFFERPARAMETERUI64VNVPROC glad_glGetNamedBufferParameterui64vNV;
typedef void (APIENTRYP      PFNGLGETINTEGERUI64VNVPROC)(GLenum, GLuint64EXT*);
GLAPI PFNGLGETINTEGERUI64VNVPROC glad_glGetIntegerui64vNV;
typedef void (APIENTRYP      PFNGLUNIFORMUI64NVPROC)(GLint, GLuint64EXT);
GLAPI PFNGLUNIFORMUI64NVPROC glad_glUniformui64NV;
typedef void (APIENTRYP      PFNGLUNIFORMUI64VNVPROC)(GLint, GLsizei, const GLuint64EXT*);
GLAPI PFNGLUNIFORMUI64VNVPROC glad_glUniformui64vNV;
typedef void (APIENTRYP      PFNGLGETUNIFORMUI64VNVPROC)(GLuint, GLint, GLuint64EXT*);
GLAPI PFNGLGETUNIFORMUI64VNVPROC glad_glGetUniformui64vNV;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMUI64NVPROC)(GLuint, GLint, GLuint64EXT);
GLAPI PFNGLPROGRAMUNIFORMUI64NVPROC glad_glProgramUniformui64NV;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMUI64VNVPROC)(GLuint, GLint, GLsizei, const GLuint64EXT*);
GLAPI PFNGLPROGRAMUNIFORMUI64VNVPROC glad_glProgramUniformui64vNV;
#endif
#ifndef GL_NV_bindless_multi_draw_indirect
	#define GL_NV_bindless_multi_draw_indirect 1
GLAPI int GLAD_GL_NV_bindless_multi_draw_indirect;
typedef void (APIENTRYP PFNGLMULTIDRAWARRAYSINDIRECTBINDLESSNVPROC)(GLenum, const void*, GLsizei, GLsizei, GLint);
GLAPI PFNGLMULTIDRAWARRAYSINDIRECTBINDLESSNVPROC glad_glMultiDrawArraysIndirectBindlessNV;
typedef void (APIENTRYP PFNGLMULTIDRAWELEMENTSINDIRECTBINDLESSNVPROC)(GLenum, GLenum, const void*, GLsizei, GLsizei, GLint);
GLAPI PFNGLMULTIDRAWELEMENTSINDIRECTBINDLESSNVPROC glad_glMultiDrawElementsIndirectBindlessNV;
#endif
#ifndef GL_NV_bindless_multi_draw_indirect_count
	#define GL_NV_bindless_multi_draw_indirect_count 1
GLAPI int GLAD_GL_NV_bindless_multi_draw_indirect_count;
typedef void (APIENTRYP PFNGLMULTIDRAWARRAYSINDIRECTBINDLESSCOUNTNVPROC)(GLenum, const void*, GLsizei, GLsizei, GLsizei, GLint);
GLAPI PFNGLMULTIDRAWARRAYSINDIRECTBINDLESSCOUNTNVPROC glad_glMultiDrawArraysIndirectBindlessCountNV;
typedef void (APIENTRYP PFNGLMULTIDRAWELEMENTSINDIRECTBINDLESSCOUNTNVPROC)(GLenum, GLenum, const void*, GLsizei, GLsizei, GLsizei, GLint);
GLAPI PFNGLMULTIDRAWELEMENTSINDIRECTBINDLESSCOUNTNVPROC glad_glMultiDrawElementsIndirectBindlessCountNV;
#endif
#ifndef GL_NV_bindless_texture
	#define GL_NV_bindless_texture 1
GLAPI int GLAD_GL_NV_bindless_texture;
typedef GLuint64 (APIENTRYP  PFNGLGETTEXTUREHANDLENVPROC)(GLuint);
GLAPI PFNGLGETTEXTUREHANDLENVPROC glad_glGetTextureHandleNV;
typedef GLuint64 (APIENTRYP  PFNGLGETTEXTURESAMPLERHANDLENVPROC)(GLuint, GLuint);
GLAPI PFNGLGETTEXTURESAMPLERHANDLENVPROC glad_glGetTextureSamplerHandleNV;
typedef void (APIENTRYP      PFNGLMAKETEXTUREHANDLERESIDENTNVPROC)(GLuint64);
GLAPI PFNGLMAKETEXTUREHANDLERESIDENTNVPROC glad_glMakeTextureHandleResidentNV;
typedef void (APIENTRYP      PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC)(GLuint64);
GLAPI PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC glad_glMakeTextureHandleNonResidentNV;
typedef GLuint64 (APIENTRYP  PFNGLGETIMAGEHANDLENVPROC)(GLuint, GLint, GLboolean, GLint, GLenum);
GLAPI PFNGLGETIMAGEHANDLENVPROC glad_glGetImageHandleNV;
typedef void (APIENTRYP      PFNGLMAKEIMAGEHANDLERESIDENTNVPROC)(GLuint64, GLenum);
GLAPI PFNGLMAKEIMAGEHANDLERESIDENTNVPROC glad_glMakeImageHandleResidentNV;
typedef void (APIENTRYP      PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC)(GLuint64);
GLAPI PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC glad_glMakeImageHandleNonResidentNV;
typedef void (APIENTRYP      PFNGLUNIFORMHANDLEUI64NVPROC)(GLint, GLuint64);
GLAPI PFNGLUNIFORMHANDLEUI64NVPROC glad_glUniformHandleui64NV;
typedef void (APIENTRYP      PFNGLUNIFORMHANDLEUI64VNVPROC)(GLint, GLsizei, const GLuint64*);
GLAPI PFNGLUNIFORMHANDLEUI64VNVPROC glad_glUniformHandleui64vNV;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC)(GLuint, GLint, GLuint64);
GLAPI PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC glad_glProgramUniformHandleui64NV;
typedef void (APIENTRYP      PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC)(GLuint, GLint, GLsizei, const GLuint64*);
GLAPI PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC glad_glProgramUniformHandleui64vNV;
typedef GLboolean (APIENTRYP PFNGLISTEXTUREHANDLERESIDENTNVPROC)(GLuint64);
GLAPI PFNGLISTEXTUREHANDLERESIDENTNVPROC glad_glIsTextureHandleResidentNV;
typedef GLboolean (APIENTRYP PFNGLISIMAGEHANDLERESIDENTNVPROC)(GLuint64);
GLAPI PFNGLISIMAGEHANDLERESIDENTNVPROC glad_glIsImageHandleResidentNV;
#endif
#ifndef GL_EXT_shader_image_load_store
	#define GL_EXT_shader_image_load_store 1
GLAPI int GLAD_GL_EXT_shader_image_load_store;
typedef void (APIENTRYP PFNGLBINDIMAGETEXTUREEXTPROC)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLint);
GLAPI PFNGLBINDIMAGETEXTUREEXTPROC glad_glBindImageTextureEXT;
typedef void (APIENTRYP PFNGLMEMORYBARRIEREXTPROC)(GLbitfield);
GLAPI PFNGLMEMORYBARRIEREXTPROC glad_glMemoryBarrierEXT;
#endif
#ifndef GL_NV_depth_buffer_float
	#define GL_NV_depth_buffer_float 1
GLAPI int GLAD_GL_NV_depth_buffer_float;
typedef void (APIENTRYP PFNGLDEPTHRANGEDNVPROC)(GLdouble, GLdouble);
GLAPI PFNGLDEPTHRANGEDNVPROC glad_glDepthRangedNV;
typedef void (APIENTRYP PFNGLCLEARDEPTHDNVPROC)(GLdouble);
GLAPI PFNGLCLEARDEPTHDNVPROC glad_glClearDepthdNV;
typedef void (APIENTRYP PFNGLDEPTHBOUNDSDNVPROC)(GLdouble, GLdouble);
GLAPI PFNGLDEPTHBOUNDSDNVPROC glad_glDepthBoundsdNV;
#endif
#if defined(__cplusplus) && defined(GLAD_INSTRUMENT_CALL)
struct glad_tag_glMultiDrawArraysIndirectBindlessCountNV {};
inline void glad_wrapped_glMultiDrawArraysIndirectBindlessCountNV(GLenum _mode, const void* _indirect, GLsizei _drawCount, GLsizei _maxDrawCount, GLsizei _stride, GLint _vertexBufferCount) { GLAD_INSTRUMENT_CALL(glMultiDrawArraysIndirectBindlessCountNV, _mode, _indirect, _drawCount, _maxDrawCount, _stride, _vertexBufferCount); }
	#define glMultiDrawArraysIndirectBindlessCountNV glad_wrapped_glMultiDrawArraysIndirectBindlessCountNV
struct glad_tag_glVertexArrayEdgeFlagOffsetEXT {};
inline void glad_wrapped_glVertexArrayEdgeFlagOffsetEXT(GLuint _vaobj, GLuint _buffer, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayEdgeFlagOffsetEXT, _vaobj, _buffer, _stride, _offset); }
	#define glVertexArrayEdgeFlagOffsetEXT glad_wrapped_glVertexArrayEdgeFlagOffsetEXT
struct glad_tag_glGetIntegerIndexedvEXT {};
inline void glad_wrapped_glGetIntegerIndexedvEXT(GLenum _target, GLuint _index, GLint* _data) { GLAD_INSTRUMENT_CALL(glGetIntegerIndexedvEXT, _target, _index, _data); }
	#define glGetIntegerIndexedvEXT glad_wrapped_glGetIntegerIndexedvEXT
struct glad_tag_glCopyTextureImage1DEXT {};
inline void glad_wrapped_glCopyTextureImage1DEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width, GLint _border) { GLAD_INSTRUMENT_CALL(glCopyTextureImage1DEXT, _texture, _target, _level, _internalformat, _x, _y, _width, _border); }
	#define glCopyTextureImage1DEXT glad_wrapped_glCopyTextureImage1DEXT
struct glad_tag_glCompressedMultiTexSubImage1DEXT {};
inline void glad_wrapped_glCompressedMultiTexSubImage1DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedMultiTexSubImage1DEXT, _texunit, _target, _level, _xoffset, _width, _format, _imageSize, _bits); }
	#define glCompressedMultiTexSubImage1DEXT glad_wrapped_glCompressedMultiTexSubImage1DEXT
struct glad_tag_glMultiTexGendEXT {};
inline void glad_wrapped_glMultiTexGendEXT(GLenum _texunit, GLenum _coord, GLenum _pname, GLdouble _param) { GLAD_INSTRUMENT_CALL(glMultiTexGendEXT, _texunit, _coord, _pname, _param); }
	#define glMultiTexGendEXT glad_wrapped_glMultiTexGendEXT
struct glad_tag_glTextureStorage3DEXT {};
inline void glad_wrapped_glTextureStorage3DEXT(GLuint _texture, GLenum _target, GLsizei _levels, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth) { GLAD_INSTRUMENT_CALL(glTextureStorage3DEXT, _texture, _target, _levels, _internalformat, _width, _height, _depth); }
	#define glTextureStorage3DEXT glad_wrapped_glTextureStorage3DEXT
struct glad_tag_glVertexArrayBindVertexBufferEXT {};
inline void glad_wrapped_glVertexArrayBindVertexBufferEXT(GLuint _vaobj, GLuint _bindingindex, GLuint _buffer, GLintptr _offset, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glVertexArrayBindVertexBufferEXT, _vaobj, _bindingindex, _buffer, _offset, _stride); }
	#define glVertexArrayBindVertexBufferEXT glad_wrapped_glVertexArrayBindVertexBufferEXT
struct glad_tag_glDebugMessageCallbackKHR {};
inline void glad_wrapped_glDebugMessageCallbackKHR(GLDEBUGPROCKHR _callback, const void* _userParam) { GLAD_INSTRUMENT_CALL(glDebugMessageCallbackKHR, _callback, _userParam); }
	#define glDebugMessageCallbackKHR glad_wrapped_glDebugMessageCallbackKHR
struct glad_tag_glGetIntegerui64vNV {};
inline void glad_wrapped_glGetIntegerui64vNV(GLenum _value, GLuint64EXT* _result) { GLAD_INSTRUMENT_CALL(glGetIntegerui64vNV, _value, _result); }
	#define glGetIntegerui64vNV glad_wrapped_glGetIntegerui64vNV
struct glad_tag_glMultiTexParameteriEXT {};
inline void glad_wrapped_glMultiTexParameteriEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glMultiTexParameteriEXT, _texunit, _target, _pname, _param); }
	#define glMultiTexParameteriEXT glad_wrapped_glMultiTexParameteriEXT
struct glad_tag_glGetTextureHandleNV {};
inline GLuint64 glad_wrapped_glGetTextureHandleNV(GLuint _texture) { return GLAD_INSTRUMENT_CALL(glGetTextureHandleNV, _texture); }
	#define glGetTextureHandleNV glad_wrapped_glGetTextureHandleNV
struct glad_tag_glProgramUniformMatrix3x2dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix3x2dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x2dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x2dvEXT glad_wrapped_glProgramUniformMatrix3x2dvEXT
struct glad_tag_glGetNamedProgramLocalParameterfvEXT {};
inline void glad_wrapped_glGetNamedProgramLocalParameterfvEXT(GLuint _program, GLenum _target, GLuint _index, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetNamedProgramLocalParameterfvEXT, _program, _target, _index, _params); }
	#define glGetNamedProgramLocalParameterfvEXT glad_wrapped_glGetNamedProgramLocalParameterfvEXT
struct glad_tag_glEnableVertexArrayAttribEXT {};
inline void glad_wrapped_glEnableVertexArrayAttribEXT(GLuint _vaobj, GLuint _index) { GLAD_INSTRUMENT_CALL(glEnableVertexArrayAttribEXT, _vaobj, _index); }
	#define glEnableVertexArrayAttribEXT glad_wrapped_glEnableVertexArrayAttribEXT
struct glad_tag_glProgramUniform2uivEXT {};
inline void glad_wrapped_glProgramUniform2uivEXT(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2uivEXT, _program, _location, _count, _value); }
	#define glProgramUniform2uivEXT glad_wrapped_glProgramUniform2uivEXT
struct glad_tag_glGetTextureParameterivEXT {};
inline void glad_wrapped_glGetTextureParameterivEXT(GLuint _texture, GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameterivEXT, _texture, _target, _pname, _params); }
	#define glGetTextureParameterivEXT glad_wrapped_glGetTextureParameterivEXT
struct glad_tag_glCompressedMultiTexSubImage3DEXT {};
inline void glad_wrapped_glCompressedMultiTexSubImage3DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedMultiTexSubImage3DEXT, _texunit, _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _imageSize, _bits); }
	#define glCompressedMultiTexSubImage3DEXT glad_wrapped_glCompressedMultiTexSubImage3DEXT
struct glad_tag_glVertexArrayVertexAttribIFormatEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribIFormatEXT(GLuint _vaobj, GLuint _attribindex, GLint _size, GLenum _type, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribIFormatEXT, _vaobj, _attribindex, _size, _type, _relativeoffset); }
	#define glVertexArrayVertexAttribIFormatEXT glad_wrapped_glVertexArrayVertexAttribIFormatEXT
struct glad_tag_glProgramUniform4fEXT {};
inline void glad_wrapped_glProgramUniform4fEXT(GLuint _program, GLint _location, GLfloat _v0, GLfloat _v1, GLfloat _v2, GLfloat _v3) { GLAD_INSTRUMENT_CALL(glProgramUniform4fEXT, _program, _location, _v0, _v1, _v2, _v3); }
	#define glProgramUniform4fEXT glad_wrapped_glProgramUniform4fEXT
struct glad_tag_glMakeImageHandleNonResidentNV {};
inline void glad_wrapped_glMakeImageHandleNonResidentNV(GLuint64 _handle) { GLAD_INSTRUMENT_CALL(glMakeImageHandleNonResidentNV, _handle); }
	#define glMakeImageHandleNonResidentNV glad_wrapped_glMakeImageHandleNonResidentNV
struct glad_tag_glProgramUniform3ivEXT {};
inline void glad_wrapped_glProgramUniform3ivEXT(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3ivEXT, _program, _location, _count, _value); }
	#define glProgramUniform3ivEXT glad_wrapped_glProgramUniform3ivEXT
struct glad_tag_glGetFramebufferParameterivEXT {};
inline void glad_wrapped_glGetFramebufferParameterivEXT(GLuint _framebuffer, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetFramebufferParameterivEXT, _framebuffer, _pname, _params); }
	#define glGetFramebufferParameterivEXT glad_wrapped_glGetFramebufferParameterivEXT
struct glad_tag_glNamedBufferStorageEXT {};
inline void glad_wrapped_glNamedBufferStorageEXT(GLuint _buffer, GLsizeiptr _size, const void* _data, GLbitfield _flags) { GLAD_INSTRUMENT_CALL(glNamedBufferStorageEXT, _buffer, _size, _data, _flags); }
	#define glNamedBufferStorageEXT glad_wrapped_glNamedBufferStorageEXT
struct glad_tag_glNamedFramebufferTexture3DEXT {};
inline void glad_wrapped_glNamedFramebufferTexture3DEXT(GLuint _framebuffer, GLenum _attachment, GLenum _textarget, GLuint _texture, GLint _level, GLint _zoffset) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTexture3DEXT, _framebuffer, _attachment, _textarget, _texture, _level, _zoffset); }
	#define glNamedFramebufferTexture3DEXT glad_wrapped_glNamedFramebufferTexture3DEXT
struct glad_tag_glGetSeparableFilter {};
inline void glad_wrapped_glGetSeparableFilter(GLenum _target, GLenum _format, GLenum _type, void* _row, void* _column, void* _span) { GLAD_INSTRUMENT_CALL(glGetSeparableFilter, _target, _format, _type, _row, _column, _span); }
	#define glGetSeparableFilter glad_wrapped_glGetSeparableFilter
struct glad_tag_glProgramUniform4iEXT {};
inline void glad_wrapped_glProgramUniform4iEXT(GLuint _program, GLint _location, GLint _v0, GLint _v1, GLint _v2, GLint _v3) { GLAD_INSTRUMENT_CALL(glProgramUniform4iEXT, _program, _location, _v0, _v1, _v2, _v3); }
	#define glProgramUniform4iEXT glad_wrapped_glProgramUniform4iEXT
struct glad_tag_glGenerateTextureMipmapEXT {};
inline void glad_wrapped_glGenerateTextureMipmapEXT(GLuint _texture, GLenum _target) { GLAD_INSTRUMENT_CALL(glGenerateTextureMipmapEXT, _texture, _target); }
	#define glGenerateTextureMipmapEXT glad_wrapped_glGenerateTextureMipmapEXT
struct glad_tag_glProgramUniformMatrix3fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix3fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3fvEXT glad_wrapped_glProgramUniformMatrix3fvEXT
struct glad_tag_glEnableVertexArrayEXT {};
inline void glad_wrapped_glEnableVertexArrayEXT(GLuint _vaobj, GLenum _array) { GLAD_INSTRUMENT_CALL(glEnableVertexArrayEXT, _vaobj, _array); }
	#define glEnableVertexArrayEXT glad_wrapped_glEnableVertexArrayEXT
struct glad_tag_glTextureParameterivEXT {};
inline void glad_wrapped_glTextureParameterivEXT(GLuint _texture, GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glTextureParameterivEXT, _texture, _target, _pname, _params); }
	#define glTextureParameterivEXT glad_wrapped_glTextureParameterivEXT
struct glad_tag_glMultiTexRenderbufferEXT {};
inline void glad_wrapped_glMultiTexRenderbufferEXT(GLenum _texunit, GLenum _target, GLuint _renderbuffer) { GLAD_INSTRUMENT_CALL(glMultiTexRenderbufferEXT, _texunit, _target, _renderbuffer); }
	#define glMultiTexRenderbufferEXT glad_wrapped_glMultiTexRenderbufferEXT
struct glad_tag_glMultiTexParameterIivEXT {};
inline void glad_wrapped_glMultiTexParameterIivEXT(GLenum _texunit, GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glMultiTexParameterIivEXT, _texunit, _target, _pname, _params); }
	#define glMultiTexParameterIivEXT glad_wrapped_glMultiTexParameterIivEXT
struct glad_tag_glMatrixPushEXT {};
inline void glad_wrapped_glMatrixPushEXT(GLenum _mode) { GLAD_INSTRUMENT_CALL(glMatrixPushEXT, _mode); }
	#define glMatrixPushEXT glad_wrapped_glMatrixPushEXT
struct glad_tag_glDebugMessageInsertKHR {};
inline void glad_wrapped_glDebugMessageInsertKHR(GLenum _source, GLenum _type, GLuint _id, GLenum _severity, GLsizei _length, const GLchar* _buf) { GLAD_INSTRUMENT_CALL(glDebugMessageInsertKHR, _source, _type, _id, _severity, _length, _buf); }
	#define glDebugMessageInsertKHR glad_wrapped_glDebugMessageInsertKHR
struct glad_tag_glProgramUniform2fEXT {};
inline void glad_wrapped_glProgramUniform2fEXT(GLuint _program, GLint _location, GLfloat _v0, GLfloat _v1) { GLAD_INSTRUMENT_CALL(glProgramUniform2fEXT, _program, _location, _v0, _v1); }
	#define glProgramUniform2fEXT glad_wrapped_glProgramUniform2fEXT
struct glad_tag_glTextureParameterfEXT {};
inline void glad_wrapped_glTextureParameterfEXT(GLuint _texture, GLenum _target, GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glTextureParameterfEXT, _texture, _target, _pname, _param); }
	#define glTextureParameterfEXT glad_wrapped_glTextureParameterfEXT
struct glad_tag_glMultiDrawElementsIndirectBindlessNV {};
inline void glad_wrapped_glMultiDrawElementsIndirectBindlessNV(GLenum _mode, GLenum _type, const void* _indirect, GLsizei _drawCount, GLsizei _stride, GLint _vertexBufferCount) { GLAD_INSTRUMENT_CALL(glMultiDrawElementsIndirectBindlessNV, _mode, _type, _indirect, _drawCount, _stride, _vertexBufferCount); }
	#define glMultiDrawElementsIndirectBindlessNV glad_wrapped_glMultiDrawElementsIndirectBindlessNV
struct glad_tag_glNamedFramebufferTextureLayerEXT {};
inline void glad_wrapped_glNamedFramebufferTextureLayerEXT(GLuint _framebuffer, GLenum _attachment, GLuint _texture, GLint _level, GLint _layer) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTextureLayerEXT, _framebuffer, _attachment, _texture, _level, _layer); }
	#define glNamedFramebufferTextureLayerEXT glad_wrapped_glNamedFramebufferTextureLayerEXT
struct glad_tag_glObjectPtrLabelKHR {};
inline void glad_wrapped_glObjectPtrLabelKHR(const void* _ptr, GLsizei _length, const GLchar* _label) { GLAD_INSTRUMENT_CALL(glObjectPtrLabelKHR, _ptr, _length, _label); }
	#define glObjectPtrLabelKHR glad_wrapped_glObjectPtrLabelKHR
struct glad_tag_glVertexAttribFormatNV {};
inline void glad_wrapped_glVertexAttribFormatNV(GLuint _index, GLint _size, GLenum _type, GLboolean _normalized, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glVertexAttribFormatNV, _index, _size, _type, _normalized, _stride); }
	#define glVertexAttribFormatNV glad_wrapped_glVertexAttribFormatNV
struct glad_tag_glNamedBufferDataEXT {};
inline void glad_wrapped_glNamedBufferDataEXT(GLuint _buffer, GLsizeiptr _size, const void* _data, GLenum _usage) { GLAD_INSTRUMENT_CALL(glNamedBufferDataEXT, _buffer, _size, _data, _usage); }
	#define glNamedBufferDataEXT glad_wrapped_glNamedBufferDataEXT
struct glad_tag_glGetFloatIndexedvEXT {};
inline void glad_wrapped_glGetFloatIndexedvEXT(GLenum _target, GLuint _index, GLfloat* _data) { GLAD_INSTRUMENT_CALL(glGetFloatIndexedvEXT, _target, _index, _data); }
	#define glGetFloatIndexedvEXT glad_wrapped_glGetFloatIndexedvEXT
struct glad_tag_glMatrixMultfEXT {};
inline void glad_wrapped_glMatrixMultfEXT(GLenum _mode, const GLfloat* _m) { GLAD_INSTRUMENT_CALL(glMatrixMultfEXT, _mode, _m); }
	#define glMatrixMultfEXT glad_wrapped_glMatrixMultfEXT
struct glad_tag_glCompressedTextureSubImage3DEXT {};
inline void glad_wrapped_glCompressedTextureSubImage3DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedTextureSubImage3DEXT, _texture, _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _imageSize, _bits); }
	#define glCompressedTextureSubImage3DEXT glad_wrapped_glCompressedTextureSubImage3DEXT
struct glad_tag_glGetTextureLevelParameterivEXT {};
inline void glad_wrapped_glGetTextureLevelParameterivEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureLevelParameterivEXT, _texture, _target, _level, _pname, _params); }
	#define glGetTextureLevelParameterivEXT glad_wrapped_glGetTextureLevelParameterivEXT
struct glad_tag_glGetVertexArrayIntegeri_vEXT {};
inline void glad_wrapped_glGetVertexArrayIntegeri_vEXT(GLuint _vaobj, GLuint _index, GLenum _pname, GLint* _param) { GLAD_INSTRUMENT_CALL(glGetVertexArrayIntegeri_vEXT, _vaobj, _index, _pname, _param); }
	#define glGetVertexArrayIntegeri_vEXT glad_wrapped_glGetVertexArrayIntegeri_vEXT
struct glad_tag_glGetMultiTexParameterivEXT {};
inline void glad_wrapped_glGetMultiTexParameterivEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexParameterivEXT, _texunit, _target, _pname, _params); }
	#define glGetMultiTexParameterivEXT glad_wrapped_glGetMultiTexParameterivEXT
struct glad_tag_glProgramUniformMatrix3x4fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix3x4fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x4fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x4fvEXT glad_wrapped_glProgramUniformMatrix3x4fvEXT
struct glad_tag_glProgramUniform3iEXT {};
inline void glad_wrapped_glProgramUniform3iEXT(GLuint _program, GLint _location, GLint _v0, GLint _v1, GLint _v2) { GLAD_INSTRUMENT_CALL(glProgramUniform3iEXT, _program, _location, _v0, _v1, _v2); }
	#define glProgramUniform3iEXT glad_wrapped_glProgramUniform3iEXT
struct glad_tag_glBindImageTextureEXT {};
inline void glad_wrapped_glBindImageTextureEXT(GLuint _index, GLuint _texture, GLint _level, GLboolean _layered, GLint _layer, GLenum _access, GLint _format) { GLAD_INSTRUMENT_CALL(glBindImageTextureEXT, _index, _texture, _level, _layered, _layer, _access, _format); }
	#define glBindImageTextureEXT glad_wrapped_glBindImageTextureEXT
struct glad_tag_glNamedProgramLocalParameterI4ivEXT {};
inline void glad_wrapped_glNamedProgramLocalParameterI4ivEXT(GLuint _program, GLenum _target, GLuint _index, const GLint* _params) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameterI4ivEXT, _program, _target, _index, _params); }
	#define glNamedProgramLocalParameterI4ivEXT glad_wrapped_glNamedProgramLocalParameterI4ivEXT
struct glad_tag_glGetMultiTexParameterIuivEXT {};
inline void glad_wrapped_glGetMultiTexParameterIuivEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexParameterIuivEXT, _texunit, _target, _pname, _params); }
	#define glGetMultiTexParameterIuivEXT glad_wrapped_glGetMultiTexParameterIuivEXT
struct glad_tag_glTextureStorage2DMultisampleEXT {};
inline void glad_wrapped_glTextureStorage2DMultisampleEXT(GLuint _texture, GLenum _target, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTextureStorage2DMultisampleEXT, _texture, _target, _samples, _internalformat, _width, _height, _fixedsamplelocations); }
	#define glTextureStorage2DMultisampleEXT glad_wrapped_glTextureStorage2DMultisampleEXT
struct glad_tag_glMatrixOrthoEXT {};
inline void glad_wrapped_glMatrixOrthoEXT(GLenum _mode, GLdouble _left, GLdouble _right, GLdouble _bottom, GLdouble _top, GLdouble _zNear, GLdouble _zFar) { GLAD_INSTRUMENT_CALL(glMatrixOrthoEXT, _mode, _left, _right, _bottom, _top, _zNear, _zFar); }
	#define glMatrixOrthoEXT glad_wrapped_glMatrixOrthoEXT
struct glad_tag_glMatrixMultTransposedEXT {};
inline void glad_wrapped_glMatrixMultTransposedEXT(GLenum _mode, const GLdouble* _m) { GLAD_INSTRUMENT_CALL(glMatrixMultTransposedEXT, _mode, _m); }
	#define glMatrixMultTransposedEXT glad_wrapped_glMatrixMultTransposedEXT
struct glad_tag_glGetDoubleIndexedvEXT {};
inline void glad_wrapped_glGetDoubleIndexedvEXT(GLenum _target, GLuint _index, GLdouble* _data) { GLAD_INSTRUMENT_CALL(glGetDoubleIndexedvEXT, _target, _index, _data); }
	#define glGetDoubleIndexedvEXT glad_wrapped_glGetDoubleIndexedvEXT
struct glad_tag_glEnableClientStateiEXT {};
inline void glad_wrapped_glEnableClientStateiEXT(GLenum _array, GLuint _index) { GLAD_INSTRUMENT_CALL(glEnableClientStateiEXT, _array, _index); }
	#define glEnableClientStateiEXT glad_wrapped_glEnableClientStateiEXT
struct glad_tag_glCompressedTextureImage3DEXT {};
inline void glad_wrapped_glCompressedTextureImage3DEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedTextureImage3DEXT, _texture, _target, _level, _internalformat, _width, _height, _depth, _border, _imageSize, _bits); }
	#define glCompressedTextureImage3DEXT glad_wrapped_glCompressedTextureImage3DEXT
struct glad_tag_glColorSubTable {};
inline void glad_wrapped_glColorSubTable(GLenum _target, GLsizei _start, GLsizei _count, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glColorSubTable, _target, _start, _count, _format, _type, _data); }
	#define glColorSubTable glad_wrapped_glColorSubTable
struct glad_tag_glMultiTexGenfvEXT {};
inline void glad_wrapped_glMultiTexGenfvEXT(GLenum _texunit, GLenum _coord, GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glMultiTexGenfvEXT, _texunit, _coord, _pname, _params); }
	#define glMultiTexGenfvEXT glad_wrapped_glMultiTexGenfvEXT
struct glad_tag_glTexCoordFormatNV {};
inline void glad_wrapped_glTexCoordFormatNV(GLint _size, GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glTexCoordFormatNV, _size, _type, _stride); }
	#define glTexCoordFormatNV glad_wrapped_glTexCoordFormatNV
struct glad_tag_glMultiTexGenfEXT {};
inline void glad_wrapped_glMultiTexGenfEXT(GLenum _texunit, GLenum _coord, GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glMultiTexGenfEXT, _texunit, _coord, _pname, _param); }
	#define glMultiTexGenfEXT glad_wrapped_glMultiTexGenfEXT
struct glad_tag_glColorTableParameteriv {};
inline void glad_wrapped_glColorTableParameteriv(GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glColorTableParameteriv, _target, _pname, _params); }
	#define glColorTableParameteriv glad_wrapped_glColorTableParameteriv
struct glad_tag_glDebugMessageControlKHR {};
inline void glad_wrapped_glDebugMessageControlKHR(GLenum _source, GLenum _type, GLenum _severity, GLsizei _count, const GLuint* _ids, GLboolean _enabled) { GLAD_INSTRUMENT_CALL(glDebugMessageControlKHR, _source, _type, _severity, _count, _ids, _enabled); }
	#define glDebugMessageControlKHR glad_wrapped_glDebugMessageControlKHR
struct glad_tag_glCheckNamedFramebufferStatusEXT {};
inline GLenum glad_wrapped_glCheckNamedFramebufferStatusEXT(GLuint _framebuffer, GLenum _target) { return GLAD_INSTRUMENT_CALL(glCheckNamedFramebufferStatusEXT, _framebuffer, _target); }
	#define glCheckNamedFramebufferStatusEXT glad_wrapped_glCheckNamedFramebufferStatusEXT
struct glad_tag_glConvolutionParameteriv {};
inline void glad_wrapped_glConvolutionParameteriv(GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glConvolutionParameteriv, _target, _pname, _params); }
	#define glConvolutionParameteriv glad_wrapped_glConvolutionParameteriv
struct glad_tag_glVertexArrayVertexAttribFormatEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribFormatEXT(GLuint _vaobj, GLuint _attribindex, GLint _size, GLenum _type, GLboolean _normalized, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribFormatEXT, _vaobj, _attribindex, _size, _type, _normalized, _relativeoffset); }
	#define glVertexArrayVertexAttribFormatEXT glad_wrapped_glVertexArrayVertexAttribFormatEXT
struct glad_tag_glCopyMultiTexSubImage2DEXT {};
inline void glad_wrapped_glCopyMultiTexSubImage2DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyMultiTexSubImage2DEXT, _texunit, _target, _level, _xoffset, _yoffset, _x, _y, _width, _height); }
	#define glCopyMultiTexSubImage2DEXT glad_wrapped_glCopyMultiTexSubImage2DEXT
struct glad_tag_glProgramUniformMatrix3x4dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix3x4dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x4dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x4dvEXT glad_wrapped_glProgramUniformMatrix3x4dvEXT
struct glad_tag_glGetNamedBufferSubDataEXT {};
inline void glad_wrapped_glGetNamedBufferSubDataEXT(GLuint _buffer, GLintptr _offset, GLsizeiptr _size, void* _data) { GLAD_INSTRUMENT_CALL(glGetNamedBufferSubDataEXT, _buffer, _offset, _size, _data); }
	#define glGetNamedBufferSubDataEXT glad_wrapped_glGetNamedBufferSubDataEXT
struct glad_tag_glConvolutionFilter2D {};
inline void glad_wrapped_glConvolutionFilter2D(GLenum _target, GLenum _internalformat, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, const void* _image) { GLAD_INSTRUMENT_CALL(glConvolutionFilter2D, _target, _internalformat, _width, _height, _format, _type, _image); }
	#define glConvolutionFilter2D glad_wrapped_glConvolutionFilter2D
struct glad_tag_glProgramUniformMatrix4x3dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix4x3dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x3dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x3dvEXT glad_wrapped_glProgramUniformMatrix4x3dvEXT
struct glad_tag_glMatrixRotatedEXT {};
inline void glad_wrapped_glMatrixRotatedEXT(GLenum _mode, GLdouble _angle, GLdouble _x, GLdouble _y, GLdouble _z) { GLAD_INSTRUMENT_CALL(glMatrixRotatedEXT, _mode, _angle, _x, _y, _z); }
	#define glMatrixRotatedEXT glad_wrapped_glMatrixRotatedEXT
struct glad_tag_glMultiTexGeniEXT {};
inline void glad_wrapped_glMultiTexGeniEXT(GLenum _texunit, GLenum _coord, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glMultiTexGeniEXT, _texunit, _coord, _pname, _param); }
	#define glMultiTexGeniEXT glad_wrapped_glMultiTexGeniEXT
struct glad_tag_glMatrixTranslatefEXT {};
inline void glad_wrapped_glMatrixTranslatefEXT(GLenum _mode, GLfloat _x, GLfloat _y, GLfloat _z) { GLAD_INSTRUMENT_CALL(glMatrixTranslatefEXT, _mode, _x, _y, _z); }
	#define glMatrixTranslatefEXT glad_wrapped_glMatrixTranslatefEXT
struct glad_tag_glNamedProgramStringEXT {};
inline void glad_wrapped_glNamedProgramStringEXT(GLuint _program, GLenum _target, GLenum _format, GLsizei _len, const void* _string) { GLAD_INSTRUMENT_CALL(glNamedProgramStringEXT, _program, _target, _format, _len, _string); }
	#define glNamedProgramStringEXT glad_wrapped_glNamedProgramStringEXT
struct glad_tag_glGetMinmaxParameteriv {};
inline void glad_wrapped_glGetMinmaxParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetMinmaxParameteriv, _target, _pname, _params); }
	#define glGetMinmaxParameteriv glad_wrapped_glGetMinmaxParameteriv
struct glad_tag_glGetPointerv {};
inline void glad_wrapped_glGetPointerv(GLenum _pname, void** _params) { GLAD_INSTRUMENT_CALL(glGetPointerv, _pname, _params); }
	#define glGetPointerv glad_wrapped_glGetPointerv
struct glad_tag_glGetNamedBufferParameterivEXT {};
inline void glad_wrapped_glGetNamedBufferParameterivEXT(GLuint _buffer, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedBufferParameterivEXT, _buffer, _pname, _params); }
	#define glGetNamedBufferParameterivEXT glad_wrapped_glGetNamedBufferParameterivEXT
struct glad_tag_glMatrixScaledEXT {};
inline void glad_wrapped_glMatrixScaledEXT(GLenum _mode, GLdouble _x, GLdouble _y, GLdouble _z) { GLAD_INSTRUMENT_CALL(glMatrixScaledEXT, _mode, _x, _y, _z); }
	#define glMatrixScaledEXT glad_wrapped_glMatrixScaledEXT
struct glad_tag_glFlushMappedNamedBufferRangeEXT {};
inline void glad_wrapped_glFlushMappedNamedBufferRangeEXT(GLuint _buffer, GLintptr _offset, GLsizeiptr _length) { GLAD_INSTRUMENT_CALL(glFlushMappedNamedBufferRangeEXT, _buffer, _offset, _length); }
	#define glFlushMappedNamedBufferRangeEXT glad_wrapped_glFlushMappedNamedBufferRangeEXT
struct glad_tag_glGetColorTableParameterfv {};
inline void glad_wrapped_glGetColorTableParameterfv(GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetColorTableParameterfv, _target, _pname, _params); }
	#define glGetColorTableParameterfv glad_wrapped_glGetColorTableParameterfv
struct glad_tag_glDisableClientStateIndexedEXT {};
inline void glad_wrapped_glDisableClientStateIndexedEXT(GLenum _array, GLuint _index) { GLAD_INSTRUMENT_CALL(glDisableClientStateIndexedEXT, _array, _index); }
	#define glDisableClientStateIndexedEXT glad_wrapped_glDisableClientStateIndexedEXT
struct glad_tag_glMultiDrawArraysIndirectBindlessNV {};
inline void glad_wrapped_glMultiDrawArraysIndirectBindlessNV(GLenum _mode, const void* _indirect, GLsizei _drawCount, GLsizei _stride, GLint _vertexBufferCount) { GLAD_INSTRUMENT_CALL(glMultiDrawArraysIndirectBindlessNV, _mode, _indirect, _drawCount, _stride, _vertexBufferCount); }
	#define glMultiDrawArraysIndirectBindlessNV glad_wrapped_glMultiDrawArraysIndirectBindlessNV
struct glad_tag_glUniformui64NV {};
inline void glad_wrapped_glUniformui64NV(GLint _location, GLuint64EXT _value) { GLAD_INSTRUMENT_CALL(glUniformui64NV, _location, _value); }
	#define glUniformui64NV glad_wrapped_glUniformui64NV
struct glad_tag_glProgramUniform1dvEXT {};
inline void glad_wrapped_glProgramUniform1dvEXT(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1dvEXT, _program, _location, _count, _value); }
	#define glProgramUniform1dvEXT glad_wrapped_glProgramUniform1dvEXT
struct glad_tag_glProgramUniformMatrix2x4dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix2x4dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x4dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x4dvEXT glad_wrapped_glProgramUniformMatrix2x4dvEXT
struct glad_tag_glProgramUniform2iEXT {};
inline void glad_wrapped_glProgramUniform2iEXT(GLuint _program, GLint _location, GLint _v0, GLint _v1) { GLAD_INSTRUMENT_CALL(glProgramUniform2iEXT, _program, _location, _v0, _v1); }
	#define glProgramUniform2iEXT glad_wrapped_glProgramUniform2iEXT
struct glad_tag_glMatrixLoadfEXT {};
inline void glad_wrapped_glMatrixLoadfEXT(GLenum _mode, const GLfloat* _m) { GLAD_INSTRUMENT_CALL(glMatrixLoadfEXT, _mode, _m); }
	#define glMatrixLoadfEXT glad_wrapped_glMatrixLoadfEXT
struct glad_tag_glCompressedMultiTexImage2DEXT {};
inline void glad_wrapped_glCompressedMultiTexImage2DEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLsizei _height, GLint _border, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedMultiTexImage2DEXT, _texunit, _target, _level, _internalformat, _width, _height, _border, _imageSize, _bits); }
	#define glCompressedMultiTexImage2DEXT glad_wrapped_glCompressedMultiTexImage2DEXT
struct glad_tag_glGetConvolutionParameteriv {};
inline void glad_wrapped_glGetConvolutionParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetConvolutionParameteriv, _target, _pname, _params); }
	#define glGetConvolutionParameteriv glad_wrapped_glGetConvolutionParameteriv
struct glad_tag_glFramebufferReadBufferEXT {};
inline void glad_wrapped_glFramebufferReadBufferEXT(GLuint _framebuffer, GLenum _mode) { GLAD_INSTRUMENT_CALL(glFramebufferReadBufferEXT, _framebuffer, _mode); }
	#define glFramebufferReadBufferEXT glad_wrapped_glFramebufferReadBufferEXT
struct glad_tag_glMinmax {};
inline void glad_wrapped_glMinmax(GLenum _target, GLenum _internalformat, GLboolean _sink) { GLAD_INSTRUMENT_CALL(glMinmax, _target, _internalformat, _sink); }
	#define glMinmax glad_wrapped_glMinmax
struct glad_tag_glTextureStorage2DEXT {};
inline void glad_wrapped_glTextureStorage2DEXT(GLuint _texture, GLenum _target, GLsizei _levels, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glTextureStorage2DEXT, _texture, _target, _levels, _internalformat, _width, _height); }
	#define glTextureStorage2DEXT glad_wrapped_glTextureStorage2DEXT
struct glad_tag_glVertexArrayFogCoordOffsetEXT {};
inline void glad_wrapped_glVertexArrayFogCoordOffsetEXT(GLuint _vaobj, GLuint _buffer, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayFogCoordOffsetEXT, _vaobj, _buffer, _type, _stride, _offset); }
	#define glVertexArrayFogCoordOffsetEXT glad_wrapped_glVertexArrayFogCoordOffsetEXT
struct glad_tag_glProgramUniform4uiEXT {};
inline void glad_wrapped_glProgramUniform4uiEXT(GLuint _program, GLint _location, GLuint _v0, GLuint _v1, GLuint _v2, GLuint _v3) { GLAD_INSTRUMENT_CALL(glProgramUniform4uiEXT, _program, _location, _v0, _v1, _v2, _v3); }
	#define glProgramUniform4uiEXT glad_wrapped_glProgramUniform4uiEXT
struct glad_tag_glClientAttribDefaultEXT {};
inline void glad_wrapped_glClientAttribDefaultEXT(GLbitfield _mask) { GLAD_INSTRUMENT_CALL(glClientAttribDefaultEXT, _mask); }
	#define glClientAttribDefaultEXT glad_wrapped_glClientAttribDefaultEXT
struct glad_tag_glVertexAttribIFormatNV {};
inline void glad_wrapped_glVertexAttribIFormatNV(GLuint _index, GLint _size, GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glVertexAttribIFormatNV, _index, _size, _type, _stride); }
	#define glVertexAttribIFormatNV glad_wrapped_glVertexAttribIFormatNV
struct glad_tag_glProgramUniform3uivEXT {};
inline void glad_wrapped_glProgramUniform3uivEXT(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3uivEXT, _program, _location, _count, _value); }
	#define glProgramUniform3uivEXT glad_wrapped_glProgramUniform3uivEXT
struct glad_tag_glMakeTextureHandleResidentNV {};
inline void glad_wrapped_glMakeTextureHandleResidentNV(GLuint64 _handle) { GLAD_INSTRUMENT_CALL(glMakeTextureHandleResidentNV, _handle); }
	#define glMakeTextureHandleResidentNV glad_wrapped_glMakeTextureHandleResidentNV
struct glad_tag_glGetNamedBufferParameterui64vNV {};
inline void glad_wrapped_glGetNamedBufferParameterui64vNV(GLuint _buffer, GLenum _pname, GLuint64EXT* _params) { GLAD_INSTRUMENT_CALL(glGetNamedBufferParameterui64vNV, _buffer, _pname, _params); }
	#define glGetNamedBufferParameterui64vNV glad_wrapped_glGetNamedBufferParameterui64vNV
struct glad_tag_glGetBufferParameterui64vNV {};
inline void glad_wrapped_glGetBufferParameterui64vNV(GLenum _target, GLenum _pname, GLuint64EXT* _params) { GLAD_INSTRUMENT_CALL(glGetBufferParameterui64vNV, _target, _pname, _params); }
	#define glGetBufferParameterui64vNV glad_wrapped_glGetBufferParameterui64vNV
struct glad_tag_glMakeNamedBufferNonResidentNV {};
inline void glad_wrapped_glMakeNamedBufferNonResidentNV(GLuint _buffer) { GLAD_INSTRUMENT_CALL(glMakeNamedBufferNonResidentNV, _buffer); }
	#define glMakeNamedBufferNonResidentNV glad_wrapped_glMakeNamedBufferNonResidentNV
struct glad_tag_glUnmapNamedBufferEXT {};
inline GLboolean glad_wrapped_glUnmapNamedBufferEXT(GLuint _buffer) { return GLAD_INSTRUMENT_CALL(glUnmapNamedBufferEXT, _buffer); }
	#define glUnmapNamedBufferEXT glad_wrapped_glUnmapNamedBufferEXT
struct glad_tag_glMakeBufferResidentNV {};
inline void glad_wrapped_glMakeBufferResidentNV(GLenum _target, GLenum _access) { GLAD_INSTRUMENT_CALL(glMakeBufferResidentNV, _target, _access); }
	#define glMakeBufferResidentNV glad_wrapped_glMakeBufferResidentNV
struct glad_tag_glGetFloati_vEXT {};
inline void glad_wrapped_glGetFloati_vEXT(GLenum _pname, GLuint _index, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetFloati_vEXT, _pname, _index, _params); }
	#define glGetFloati_vEXT glad_wrapped_glGetFloati_vEXT
struct glad_tag_glBufferAddressRangeNV {};
inline void glad_wrapped_glBufferAddressRangeNV(GLenum _pname, GLuint _index, GLuint64EXT _address, GLsizeiptr _length) { GLAD_INSTRUMENT_CALL(glBufferAddressRangeNV, _pname, _index, _address, _length); }
	#define glBufferAddressRangeNV glad_wrapped_glBufferAddressRangeNV
struct glad_tag_glUniformui64vNV {};
inline void glad_wrapped_glUniformui64vNV(GLint _location, GLsizei _count, const GLuint64EXT* _value) { GLAD_INSTRUMENT_CALL(glUniformui64vNV, _location, _count, _value); }
	#define glUniformui64vNV glad_wrapped_glUniformui64vNV
struct glad_tag_glMultiTexEnvivEXT {};
inline void glad_wrapped_glMultiTexEnvivEXT(GLenum _texunit, GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glMultiTexEnvivEXT, _texunit, _target, _pname, _params); }
	#define glMultiTexEnvivEXT glad_wrapped_glMultiTexEnvivEXT
struct glad_tag_glCopyTextureImage2DEXT {};
inline void glad_wrapped_glCopyTextureImage2DEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width, GLsizei _height, GLint _border) { GLAD_INSTRUMENT_CALL(glCopyTextureImage2DEXT, _texture, _target, _level, _internalformat, _x, _y, _width, _height, _border); }
	#define glCopyTextureImage2DEXT glad_wrapped_glCopyTextureImage2DEXT
struct glad_tag_glCompressedTextureImage2DEXT {};
inline void glad_wrapped_glCompressedTextureImage2DEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLsizei _height, GLint _border, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedTextureImage2DEXT, _texture, _target, _level, _internalformat, _width, _height, _border, _imageSize, _bits); }
	#define glCompressedTextureImage2DEXT glad_wrapped_glCompressedTextureImage2DEXT
struct glad_tag_glProgramUniformui64vNV {};
inline void glad_wrapped_glProgramUniformui64vNV(GLuint _program, GLint _location, GLsizei _count, const GLuint64EXT* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformui64vNV, _program, _location, _count, _value); }
	#define glProgramUniformui64vNV glad_wrapped_glProgramUniformui64vNV
struct glad_tag_glDepthBoundsdNV {};
inline void glad_wrapped_glDepthBoundsdNV(GLdouble _zmin, GLdouble _zmax) { GLAD_INSTRUMENT_CALL(glDepthBoundsdNV, _zmin, _zmax); }
	#define glDepthBoundsdNV glad_wrapped_glDepthBoundsdNV
struct glad_tag_glGetVertexArrayPointeri_vEXT {};
inline void glad_wrapped_glGetVertexArrayPointeri_vEXT(GLuint _vaobj, GLuint _index, GLenum _pname, void** _param) { GLAD_INSTRUMENT_CALL(glGetVertexArrayPointeri_vEXT, _vaobj, _index, _pname, _param); }
	#define glGetVertexArrayPointeri_vEXT glad_wrapped_glGetVertexArrayPointeri_vEXT
struct glad_tag_glNamedProgramLocalParameter4dEXT {};
inline void glad_wrapped_glNamedProgramLocalParameter4dEXT(GLuint _program, GLenum _target, GLuint _index, GLdouble _x, GLdouble _y, GLdouble _z, GLdouble _w) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameter4dEXT, _program, _target, _index, _x, _y, _z, _w); }
	#define glNamedProgramLocalParameter4dEXT glad_wrapped_glNamedProgramLocalParameter4dEXT
struct glad_tag_glGetMinmax {};
inline void glad_wrapped_glGetMinmax(GLenum _target, GLboolean _reset, GLenum _format, GLenum _type, void* _values) { GLAD_INSTRUMENT_CALL(glGetMinmax, _target, _reset, _format, _type, _values); }
	#define glGetMinmax glad_wrapped_glGetMinmax
struct glad_tag_glFramebufferDrawBufferEXT {};
inline void glad_wrapped_glFramebufferDrawBufferEXT(GLuint _framebuffer, GLenum _mode) { GLAD_INSTRUMENT_CALL(glFramebufferDrawBufferEXT, _framebuffer, _mode); }
	#define glFramebufferDrawBufferEXT glad_wrapped_glFramebufferDrawBufferEXT
struct glad_tag_glCopyColorSubTable {};
inline void glad_wrapped_glCopyColorSubTable(GLenum _target, GLsizei _start, GLint _x, GLint _y, GLsizei _width) { GLAD_INSTRUMENT_CALL(glCopyColorSubTable, _target, _start, _x, _y, _width); }
	#define glCopyColorSubTable glad_wrapped_glCopyColorSubTable
struct glad_tag_glCopyMultiTexSubImage3DEXT {};
inline void glad_wrapped_glCopyMultiTexSubImage3DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyMultiTexSubImage3DEXT, _texunit, _target, _level, _xoffset, _yoffset, _zoffset, _x, _y, _width, _height); }
	#define glCopyMultiTexSubImage3DEXT glad_wrapped_glCopyMultiTexSubImage3DEXT
struct glad_tag_glProgramUniformMatrix2fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix2fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2fvEXT glad_wrapped_glProgramUniformMatrix2fvEXT
struct glad_tag_glGetTextureParameterIuivEXT {};
inline void glad_wrapped_glGetTextureParameterIuivEXT(GLuint _texture, GLenum _target, GLenum _pname, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameterIuivEXT, _texture, _target, _pname, _params); }
	#define glGetTextureParameterIuivEXT glad_wrapped_glGetTextureParameterIuivEXT
struct glad_tag_glGetMultiTexEnvivEXT {};
inline void glad_wrapped_glGetMultiTexEnvivEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexEnvivEXT, _texunit, _target, _pname, _params); }
	#define glGetMultiTexEnvivEXT glad_wrapped_glGetMultiTexEnvivEXT
struct glad_tag_glProgramUniform1ivEXT {};
inline void glad_wrapped_glProgramUniform1ivEXT(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1ivEXT, _program, _location, _count, _value); }
	#define glProgramUniform1ivEXT glad_wrapped_glProgramUniform1ivEXT
struct glad_tag_glCompressedTextureSubImage1DEXT {};
inline void glad_wrapped_glCompressedTextureSubImage1DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedTextureSubImage1DEXT, _texture, _target, _level, _xoffset, _width, _format, _imageSize, _bits); }
	#define glCompressedTextureSubImage1DEXT glad_wrapped_glCompressedTextureSubImage1DEXT
struct glad_tag_glResetMinmax {};
inline void glad_wrapped_glResetMinmax(GLenum _target) { GLAD_INSTRUMENT_CALL(glResetMinmax, _target); }
	#define glResetMinmax glad_wrapped_glResetMinmax
struct glad_tag_glCopyTextureSubImage2DEXT {};
inline void glad_wrapped_glCopyTextureSubImage2DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyTextureSubImage2DEXT, _texture, _target, _level, _xoffset, _yoffset, _x, _y, _width, _height); }
	#define glCopyTextureSubImage2DEXT glad_wrapped_glCopyTextureSubImage2DEXT
struct glad_tag_glProgramUniform4dvEXT {};
inline void glad_wrapped_glProgramUniform4dvEXT(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4dvEXT, _program, _location, _count, _value); }
	#define glProgramUniform4dvEXT glad_wrapped_glProgramUniform4dvEXT
struct glad_tag_glGetHistogramParameterfv {};
inline void glad_wrapped_glGetHistogramParameterfv(GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetHistogramParameterfv, _target, _pname, _params); }
	#define glGetHistogramParameterfv glad_wrapped_glGetHistogramParameterfv
struct glad_tag_glVertexFormatNV {};
inline void glad_wrapped_glVertexFormatNV(GLint _size, GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glVertexFormatNV, _size, _type, _stride); }
	#define glVertexFormatNV glad_wrapped_glVertexFormatNV
struct glad_tag_glVertexArrayNormalOffsetEXT {};
inline void glad_wrapped_glVertexArrayNormalOffsetEXT(GLuint _vaobj, GLuint _buffer, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayNormalOffsetEXT, _vaobj, _buffer, _type, _stride, _offset); }
	#define glVertexArrayNormalOffsetEXT glad_wrapped_glVertexArrayNormalOffsetEXT
struct glad_tag_glProgramUniformMatrix2x4fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix2x4fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x4fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x4fvEXT glad_wrapped_glProgramUniformMatrix2x4fvEXT
struct glad_tag_glMatrixTranslatedEXT {};
inline void glad_wrapped_glMatrixTranslatedEXT(GLenum _mode, GLdouble _x, GLdouble _y, GLdouble _z) { GLAD_INSTRUMENT_CALL(glMatrixTranslatedEXT, _mode, _x, _y, _z); }
	#define glMatrixTranslatedEXT glad_wrapped_glMatrixTranslatedEXT
struct glad_tag_glNamedFramebufferRenderbufferEXT {};
inline void glad_wrapped_glNamedFramebufferRenderbufferEXT(GLuint _framebuffer, GLenum _attachment, GLenum _renderbuffertarget, GLuint _renderbuffer) { GLAD_INSTRUMENT_CALL(glNamedFramebufferRenderbufferEXT, _framebuffer, _attachment, _renderbuffertarget, _renderbuffer); }
	#define glNamedFramebufferRenderbufferEXT glad_wrapped_glNamedFramebufferRenderbufferEXT
struct glad_tag_glVertexArrayTexCoordOffsetEXT {};
inline void glad_wrapped_glVertexArrayTexCoordOffsetEXT(GLuint _vaobj, GLuint _buffer, GLint _size, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayTexCoordOffsetEXT, _vaobj, _buffer, _size, _type, _stride, _offset); }
	#define glVertexArrayTexCoordOffsetEXT glad_wrapped_glVertexArrayTexCoordOffsetEXT
struct glad_tag_glGetHistogram {};
inline void glad_wrapped_glGetHistogram(GLenum _target, GLboolean _reset, GLenum _format, GLenum _type, void* _values) { GLAD_INSTRUMENT_CALL(glGetHistogram, _target, _reset, _format, _type, _values); }
	#define glGetHistogram glad_wrapped_glGetHistogram
struct glad_tag_glColorFormatNV {};
inline void glad_wrapped_glColorFormatNV(GLint _size, GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glColorFormatNV, _size, _type, _stride); }
	#define glColorFormatNV glad_wrapped_glColorFormatNV
struct glad_tag_glProgramUniformui64NV {};
inline void glad_wrapped_glProgramUniformui64NV(GLuint _program, GLint _location, GLuint64EXT _value) { GLAD_INSTRUMENT_CALL(glProgramUniformui64NV, _program, _location, _value); }
	#define glProgramUniformui64NV glad_wrapped_glProgramUniformui64NV
struct glad_tag_glProgramUniformMatrix4x2fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix4x2fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x2fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x2fvEXT glad_wrapped_glProgramUniformMatrix4x2fvEXT
struct glad_tag_glMatrixLoadTransposedEXT {};
inline void glad_wrapped_glMatrixLoadTransposedEXT(GLenum _mode, const GLdouble* _m) { GLAD_INSTRUMENT_CALL(glMatrixLoadTransposedEXT, _mode, _m); }
	#define glMatrixLoadTransposedEXT glad_wrapped_glMatrixLoadTransposedEXT
struct glad_tag_glGetPointeri_vEXT {};
inline void glad_wrapped_glGetPointeri_vEXT(GLenum _pname, GLuint _index, void** _params) { GLAD_INSTRUMENT_CALL(glGetPointeri_vEXT, _pname, _index, _params); }
	#define glGetPointeri_vEXT glad_wrapped_glGetPointeri_vEXT
struct glad_tag_glMatrixRotatefEXT {};
inline void glad_wrapped_glMatrixRotatefEXT(GLenum _mode, GLfloat _angle, GLfloat _x, GLfloat _y, GLfloat _z) { GLAD_INSTRUMENT_CALL(glMatrixRotatefEXT, _mode, _angle, _x, _y, _z); }
	#define glMatrixRotatefEXT glad_wrapped_glMatrixRotatefEXT
struct glad_tag_glMakeTextureHandleNonResidentNV {};
inline void glad_wrapped_glMakeTextureHandleNonResidentNV(GLuint64 _handle) { GLAD_INSTRUMENT_CALL(glMakeTextureHandleNonResidentNV, _handle); }
	#define glMakeTextureHandleNonResidentNV glad_wrapped_glMakeTextureHandleNonResidentNV
struct glad_tag_glGetMultiTexEnvfvEXT {};
inline void glad_wrapped_glGetMultiTexEnvfvEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexEnvfvEXT, _texunit, _target, _pname, _params); }
	#define glGetMultiTexEnvfvEXT glad_wrapped_glGetMultiTexEnvfvEXT
struct glad_tag_glProgramUniformMatrix4dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix4dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4dvEXT glad_wrapped_glProgramUniformMatrix4dvEXT
struct glad_tag_glMultiTexSubImage2DEXT {};
inline void glad_wrapped_glMultiTexSubImage2DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glMultiTexSubImage2DEXT, _texunit, _target, _level, _xoffset, _yoffset, _width, _height, _format, _type, _pixels); }
	#define glMultiTexSubImage2DEXT glad_wrapped_glMultiTexSubImage2DEXT
struct glad_tag_glProgramUniform1dEXT {};
inline void glad_wrapped_glProgramUniform1dEXT(GLuint _program, GLint _location, GLdouble _x) { GLAD_INSTRUMENT_CALL(glProgramUniform1dEXT, _program, _location, _x); }
	#define glProgramUniform1dEXT glad_wrapped_glProgramUniform1dEXT
struct glad_tag_glVertexArrayVertexAttribBindingEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribBindingEXT(GLuint _vaobj, GLuint _attribindex, GLuint _bindingindex) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribBindingEXT, _vaobj, _attribindex, _bindingindex); }
	#define glVertexArrayVertexAttribBindingEXT glad_wrapped_glVertexArrayVertexAttribBindingEXT
struct glad_tag_glGetPointervKHR {};
inline void glad_wrapped_glGetPointervKHR(GLenum _pname, void** _params) { GLAD_INSTRUMENT_CALL(glGetPointervKHR, _pname, _params); }
	#define glGetPointervKHR glad_wrapped_glGetPointervKHR
struct glad_tag_glTextureParameteriEXT {};
inline void glad_wrapped_glTextureParameteriEXT(GLuint _texture, GLenum _target, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glTextureParameteriEXT, _texture, _target, _pname, _param); }
	#define glTextureParameteriEXT glad_wrapped_glTextureParameteriEXT
struct glad_tag_glClearNamedBufferDataEXT {};
inline void glad_wrapped_glClearNamedBufferDataEXT(GLuint _buffer, GLenum _internalformat, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearNamedBufferDataEXT, _buffer, _internalformat, _format, _type, _data); }
	#define glClearNamedBufferDataEXT glad_wrapped_glClearNamedBufferDataEXT
struct glad_tag_glDisableIndexedEXT {};
inline void glad_wrapped_glDisableIndexedEXT(GLenum _target, GLuint _index) { GLAD_INSTRUMENT_CALL(glDisableIndexedEXT, _target, _index); }
	#define glDisableIndexedEXT glad_wrapped_glDisableIndexedEXT
struct glad_tag_glGetTextureLevelParameterfvEXT {};
inline void glad_wrapped_glGetTextureLevelParameterfvEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetTextureLevelParameterfvEXT, _texture, _target, _level, _pname, _params); }
	#define glGetTextureLevelParameterfvEXT glad_wrapped_glGetTextureLevelParameterfvEXT
struct glad_tag_glVertexArrayVertexAttribLOffsetEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribLOffsetEXT(GLuint _vaobj, GLuint _buffer, GLuint _index, GLint _size, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribLOffsetEXT, _vaobj, _buffer, _index, _size, _type, _stride, _offset); }
	#define glVertexArrayVertexAttribLOffsetEXT glad_wrapped_glVertexArrayVertexAttribLOffsetEXT
struct glad_tag_glGetNamedBufferPointervEXT {};
inline void glad_wrapped_glGetNamedBufferPointervEXT(GLuint _buffer, GLenum _pname, void** _params) { GLAD_INSTRUMENT_CALL(glGetNamedBufferPointervEXT, _buffer, _pname, _params); }
	#define glGetNamedBufferPointervEXT glad_wrapped_glGetNamedBufferPointervEXT
struct glad_tag_glPopDebugGroupKHR {};
inline void glad_wrapped_glPopDebugGroupKHR() { GLAD_INSTRUMENT_CALL(glPopDebugGroupKHR); }
	#define glPopDebugGroupKHR glad_wrapped_glPopDebugGroupKHR
struct glad_tag_glConvolutionFilter1D {};
inline void glad_wrapped_glConvolutionFilter1D(GLenum _target, GLenum _internalformat, GLsizei _width, GLenum _format, GLenum _type, const void* _image) { GLAD_INSTRUMENT_CALL(glConvolutionFilter1D, _target, _internalformat, _width, _format, _type, _image); }
	#define glConvolutionFilter1D glad_wrapped_glConvolutionFilter1D
struct glad_tag_glMatrixMultTransposefEXT {};
inline void glad_wrapped_glMatrixMultTransposefEXT(GLenum _mode, const GLfloat* _m) { GLAD_INSTRUMENT_CALL(glMatrixMultTransposefEXT, _mode, _m); }
	#define glMatrixMultTransposefEXT glad_wrapped_glMatrixMultTransposefEXT
struct glad_tag_glCopyTextureSubImage1DEXT {};
inline void glad_wrapped_glCopyTextureSubImage1DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLint _x, GLint _y, GLsizei _width) { GLAD_INSTRUMENT_CALL(glCopyTextureSubImage1DEXT, _texture, _target, _level, _xoffset, _x, _y, _width); }
	#define glCopyTextureSubImage1DEXT glad_wrapped_glCopyTextureSubImage1DEXT
struct glad_tag_glTextureSubImage2DEXT {};
inline void glad_wrapped_glTextureSubImage2DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureSubImage2DEXT, _texture, _target, _level, _xoffset, _yoffset, _width, _height, _format, _type, _pixels); }
	#define glTextureSubImage2DEXT glad_wrapped_glTextureSubImage2DEXT
struct glad_tag_glProgramUniformMatrix2x3dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix2x3dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x3dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x3dvEXT glad_wrapped_glProgramUniformMatrix2x3dvEXT
struct glad_tag_glTextureParameterfvEXT {};
inline void glad_wrapped_glTextureParameterfvEXT(GLuint _texture, GLenum _target, GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glTextureParameterfvEXT, _texture, _target, _pname, _params); }
	#define glTextureParameterfvEXT glad_wrapped_glTextureParameterfvEXT
struct glad_tag_glIsNamedBufferResidentNV {};
inline GLboolean glad_wrapped_glIsNamedBufferResidentNV(GLuint _buffer) { return GLAD_INSTRUMENT_CALL(glIsNamedBufferResidentNV, _buffer); }
	#define glIsNamedBufferResidentNV glad_wrapped_glIsNamedBufferResidentNV
struct glad_tag_glCopyMultiTexSubImage1DEXT {};
inline void glad_wrapped_glCopyMultiTexSubImage1DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLint _x, GLint _y, GLsizei _width) { GLAD_INSTRUMENT_CALL(glCopyMultiTexSubImage1DEXT, _texunit, _target, _level, _xoffset, _x, _y, _width); }
	#define glCopyMultiTexSubImage1DEXT glad_wrapped_glCopyMultiTexSubImage1DEXT
struct glad_tag_glMatrixMultdEXT {};
inline void glad_wrapped_glMatrixMultdEXT(GLenum _mode, const GLdouble* _m) { GLAD_INSTRUMENT_CALL(glMatrixMultdEXT, _mode, _m); }
	#define glMatrixMultdEXT glad_wrapped_glMatrixMultdEXT
struct glad_tag_glProgramUniformMatrix3x2fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix3x2fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3x2fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3x2fvEXT glad_wrapped_glProgramUniformMatrix3x2fvEXT
struct glad_tag_glObjectLabelKHR {};
inline void glad_wrapped_glObjectLabelKHR(GLenum _identifier, GLuint _name, GLsizei _length, const GLchar* _label) { GLAD_INSTRUMENT_CALL(glObjectLabelKHR, _identifier, _name, _length, _label); }
	#define glObjectLabelKHR glad_wrapped_glObjectLabelKHR
struct glad_tag_glMultiTexImage1DEXT {};
inline void glad_wrapped_glMultiTexImage1DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glMultiTexImage1DEXT, _texunit, _target, _level, _internalformat, _width, _border, _format, _type, _pixels); }
	#define glMultiTexImage1DEXT glad_wrapped_glMultiTexImage1DEXT
struct glad_tag_glEdgeFlagFormatNV {};
inline void glad_wrapped_glEdgeFlagFormatNV(GLsizei _stride) { GLAD_INSTRUMENT_CALL(glEdgeFlagFormatNV, _stride); }
	#define glEdgeFlagFormatNV glad_wrapped_glEdgeFlagFormatNV
struct glad_tag_glNamedProgramLocalParameterI4uivEXT {};
inline void glad_wrapped_glNamedProgramLocalParameterI4uivEXT(GLuint _program, GLenum _target, GLuint _index, const GLuint* _params) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameterI4uivEXT, _program, _target, _index, _params); }
	#define glNamedProgramLocalParameterI4uivEXT glad_wrapped_glNamedProgramLocalParameterI4uivEXT
struct glad_tag_glGetMultiTexParameterIivEXT {};
inline void glad_wrapped_glGetMultiTexParameterIivEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexParameterIivEXT, _texunit, _target, _pname, _params); }
	#define glGetMultiTexParameterIivEXT glad_wrapped_glGetMultiTexParameterIivEXT
struct glad_tag_glMemoryBarrierEXT {};
inline void glad_wrapped_glMemoryBarrierEXT(GLbitfield _barriers) { GLAD_INSTRUMENT_CALL(glMemoryBarrierEXT, _barriers); }
	#define glMemoryBarrierEXT glad_wrapped_glMemoryBarrierEXT
struct glad_tag_glGetMultiTexParameterfvEXT {};
inline void glad_wrapped_glGetMultiTexParameterfvEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexParameterfvEXT, _texunit, _target, _pname, _params); }
	#define glGetMultiTexParameterfvEXT glad_wrapped_glGetMultiTexParameterfvEXT
struct glad_tag_glCopyConvolutionFilter1D {};
inline void glad_wrapped_glCopyConvolutionFilter1D(GLenum _target, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width) { GLAD_INSTRUMENT_CALL(glCopyConvolutionFilter1D, _target, _internalformat, _x, _y, _width); }
	#define glCopyConvolutionFilter1D glad_wrapped_glCopyConvolutionFilter1D
struct glad_tag_glProgramUniform4ivEXT {};
inline void glad_wrapped_glProgramUniform4ivEXT(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4ivEXT, _program, _location, _count, _value); }
	#define glProgramUniform4ivEXT glad_wrapped_glProgramUniform4ivEXT
struct glad_tag_glColorTableParameterfv {};
inline void glad_wrapped_glColorTableParameterfv(GLenum _target, GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glColorTableParameterfv, _target, _pname, _params); }
	#define glColorTableParameterfv glad_wrapped_glColorTableParameterfv
struct glad_tag_glCopyMultiTexImage1DEXT {};
inline void glad_wrapped_glCopyMultiTexImage1DEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width, GLint _border) { GLAD_INSTRUMENT_CALL(glCopyMultiTexImage1DEXT, _texunit, _target, _level, _internalformat, _x, _y, _width, _border); }
	#define glCopyMultiTexImage1DEXT glad_wrapped_glCopyMultiTexImage1DEXT
struct glad_tag_glVertexArrayVertexOffsetEXT {};
inline void glad_wrapped_glVertexArrayVertexOffsetEXT(GLuint _vaobj, GLuint _buffer, GLint _size, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexOffsetEXT, _vaobj, _buffer, _size, _type, _stride, _offset); }
	#define glVertexArrayVertexOffsetEXT glad_wrapped_glVertexArrayVertexOffsetEXT
struct glad_tag_glNamedFramebufferParameteriEXT {};
inline void glad_wrapped_glNamedFramebufferParameteriEXT(GLuint _framebuffer, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glNamedFramebufferParameteriEXT, _framebuffer, _pname, _param); }
	#define glNamedFramebufferParameteriEXT glad_wrapped_glNamedFramebufferParameteriEXT
struct glad_tag_glTextureBufferRangeEXT {};
inline void glad_wrapped_glTextureBufferRangeEXT(GLuint _texture, GLenum _target, GLenum _internalformat, GLuint _buffer, GLintptr _offset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glTextureBufferRangeEXT, _texture, _target, _internalformat, _buffer, _offset, _size); }
	#define glTextureBufferRangeEXT glad_wrapped_glTextureBufferRangeEXT
struct glad_tag_glGetConvolutionParameterfv {};
inline void glad_wrapped_glGetConvolutionParameterfv(GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetConvolutionParameterfv, _target, _pname, _params); }
	#define glGetConvolutionParameterfv glad_wrapped_glGetConvolutionParameterfv
struct glad_tag_glCompressedMultiTexSubImage2DEXT {};
inline void glad_wrapped_glCompressedMultiTexSubImage2DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedMultiTexSubImage2DEXT, _texunit, _target, _level, _xoffset, _yoffset, _width, _height, _format, _imageSize, _bits); }
	#define glCompressedMultiTexSubImage2DEXT glad_wrapped_glCompressedMultiTexSubImage2DEXT
struct glad_tag_glGetHistogramParameteriv {};
inline void glad_wrapped_glGetHistogramParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetHistogramParameteriv, _target, _pname, _params); }
	#define glGetHistogramParameteriv glad_wrapped_glGetHistogramParameteriv
struct glad_tag_glTextureBufferEXT {};
inline void glad_wrapped_glTextureBufferEXT(GLuint _texture, GLenum _target, GLenum _internalformat, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glTextureBufferEXT, _texture, _target, _internalformat, _buffer); }
	#define glTextureBufferEXT glad_wrapped_glTextureBufferEXT
struct glad_tag_glGetMultiTexImageEXT {};
inline void glad_wrapped_glGetMultiTexImageEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _format, GLenum _type, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetMultiTexImageEXT, _texunit, _target, _level, _format, _type, _pixels); }
	#define glGetMultiTexImageEXT glad_wrapped_glGetMultiTexImageEXT
struct glad_tag_glGetPointerIndexedvEXT {};
inline void glad_wrapped_glGetPointerIndexedvEXT(GLenum _target, GLuint _index, void** _data) { GLAD_INSTRUMENT_CALL(glGetPointerIndexedvEXT, _target, _index, _data); }
	#define glGetPointerIndexedvEXT glad_wrapped_glGetPointerIndexedvEXT
struct glad_tag_glVertexArrayVertexBindingDivisorEXT {};
inline void glad_wrapped_glVertexArrayVertexBindingDivisorEXT(GLuint _vaobj, GLuint _bindingindex, GLuint _divisor) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexBindingDivisorEXT, _vaobj, _bindingindex, _divisor); }
	#define glVertexArrayVertexBindingDivisorEXT glad_wrapped_glVertexArrayVertexBindingDivisorEXT
struct glad_tag_glIsImageHandleResidentNV {};
inline GLboolean glad_wrapped_glIsImageHandleResidentNV(GLuint64 _handle) { return GLAD_INSTRUMENT_CALL(glIsImageHandleResidentNV, _handle); }
	#define glIsImageHandleResidentNV glad_wrapped_glIsImageHandleResidentNV
struct glad_tag_glProgramUniform2dvEXT {};
inline void glad_wrapped_glProgramUniform2dvEXT(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2dvEXT, _program, _location, _count, _value); }
	#define glProgramUniform2dvEXT glad_wrapped_glProgramUniform2dvEXT
struct glad_tag_glGetNamedFramebufferAttachmentParameterivEXT {};
inline void glad_wrapped_glGetNamedFramebufferAttachmentParameterivEXT(GLuint _framebuffer, GLenum _attachment, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedFramebufferAttachmentParameterivEXT, _framebuffer, _attachment, _pname, _params); }
	#define glGetNamedFramebufferAttachmentParameterivEXT glad_wrapped_glGetNamedFramebufferAttachmentParameterivEXT
struct glad_tag_glGetMultiTexGendvEXT {};
inline void glad_wrapped_glGetMultiTexGendvEXT(GLenum _texunit, GLenum _coord, GLenum _pname, GLdouble* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexGendvEXT, _texunit, _coord, _pname, _params); }
	#define glGetMultiTexGendvEXT glad_wrapped_glGetMultiTexGendvEXT
struct glad_tag_glClearNamedBufferSubDataEXT {};
inline void glad_wrapped_glClearNamedBufferSubDataEXT(GLuint _buffer, GLenum _internalformat, GLsizeiptr _offset, GLsizeiptr _size, GLenum _format, GLenum _type, const void* _data) { GLAD_INSTRUMENT_CALL(glClearNamedBufferSubDataEXT, _buffer, _internalformat, _offset, _size, _format, _type, _data); }
	#define glClearNamedBufferSubDataEXT glad_wrapped_glClearNamedBufferSubDataEXT
struct glad_tag_glGetTextureImageEXT {};
inline void glad_wrapped_glGetTextureImageEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _format, GLenum _type, void* _pixels) { GLAD_INSTRUMENT_CALL(glGetTextureImageEXT, _texture, _target, _level, _format, _type, _pixels); }
	#define glGetTextureImageEXT glad_wrapped_glGetTextureImageEXT
struct glad_tag_glPushClientAttribDefaultEXT {};
inline void glad_wrapped_glPushClientAttribDefaultEXT(GLbitfield _mask) { GLAD_INSTRUMENT_CALL(glPushClientAttribDefaultEXT, _mask); }
	#define glPushClientAttribDefaultEXT glad_wrapped_glPushClientAttribDefaultEXT
struct glad_tag_glCopyColorTable {};
inline void glad_wrapped_glCopyColorTable(GLenum _target, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width) { GLAD_INSTRUMENT_CALL(glCopyColorTable, _target, _internalformat, _x, _y, _width); }
	#define glCopyColorTable glad_wrapped_glCopyColorTable
struct glad_tag_glMultiTexParameterivEXT {};
inline void glad_wrapped_glMultiTexParameterivEXT(GLenum _texunit, GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glMultiTexParameterivEXT, _texunit, _target, _pname, _params); }
	#define glMultiTexParameterivEXT glad_wrapped_glMultiTexParameterivEXT
struct glad_tag_glMatrixLoadTransposefEXT {};
inline void glad_wrapped_glMatrixLoadTransposefEXT(GLenum _mode, const GLfloat* _m) { GLAD_INSTRUMENT_CALL(glMatrixLoadTransposefEXT, _mode, _m); }
	#define glMatrixLoadTransposefEXT glad_wrapped_glMatrixLoadTransposefEXT
struct glad_tag_glVertexArrayVertexAttribOffsetEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribOffsetEXT(GLuint _vaobj, GLuint _buffer, GLuint _index, GLint _size, GLenum _type, GLboolean _normalized, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribOffsetEXT, _vaobj, _buffer, _index, _size, _type, _normalized, _stride, _offset); }
	#define glVertexArrayVertexAttribOffsetEXT glad_wrapped_glVertexArrayVertexAttribOffsetEXT
struct glad_tag_glIndexFormatNV {};
inline void glad_wrapped_glIndexFormatNV(GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glIndexFormatNV, _type, _stride); }
	#define glIndexFormatNV glad_wrapped_glIndexFormatNV
struct glad_tag_glVertexArrayVertexAttribDivisorEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribDivisorEXT(GLuint _vaobj, GLuint _index, GLuint _divisor) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribDivisorEXT, _vaobj, _index, _divisor); }
	#define glVertexArrayVertexAttribDivisorEXT glad_wrapped_glVertexArrayVertexAttribDivisorEXT
struct glad_tag_glProgramUniformMatrix2dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix2dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2dvEXT glad_wrapped_glProgramUniformMatrix2dvEXT
struct glad_tag_glMapNamedBufferEXT {};
inline void* glad_wrapped_glMapNamedBufferEXT(GLuint _buffer, GLenum _access) { return GLAD_INSTRUMENT_CALL(glMapNamedBufferEXT, _buffer, _access); }
	#define glMapNamedBufferEXT glad_wrapped_glMapNamedBufferEXT
struct glad_tag_glMatrixLoaddEXT {};
inline void glad_wrapped_glMatrixLoaddEXT(GLenum _mode, const GLdouble* _m) { GLAD_INSTRUMENT_CALL(glMatrixLoaddEXT, _mode, _m); }
	#define glMatrixLoaddEXT glad_wrapped_glMatrixLoaddEXT
struct glad_tag_glDisableVertexArrayAttribEXT {};
inline void glad_wrapped_glDisableVertexArrayAttribEXT(GLuint _vaobj, GLuint _index) { GLAD_INSTRUMENT_CALL(glDisableVertexArrayAttribEXT, _vaobj, _index); }
	#define glDisableVertexArrayAttribEXT glad_wrapped_glDisableVertexArrayAttribEXT
struct glad_tag_glNamedProgramLocalParametersI4ivEXT {};
inline void glad_wrapped_glNamedProgramLocalParametersI4ivEXT(GLuint _program, GLenum _target, GLuint _index, GLsizei _count, const GLint* _params) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParametersI4ivEXT, _program, _target, _index, _count, _params); }
	#define glNamedProgramLocalParametersI4ivEXT glad_wrapped_glNamedProgramLocalParametersI4ivEXT
struct glad_tag_glGetTextureSamplerHandleNV {};
inline GLuint64 glad_wrapped_glGetTextureSamplerHandleNV(GLuint _texture, GLuint _sampler) { return GLAD_INSTRUMENT_CALL(glGetTextureSamplerHandleNV, _texture, _sampler); }
	#define glGetTextureSamplerHandleNV glad_wrapped_glGetTextureSamplerHandleNV
struct glad_tag_glGetColorTableParameteriv {};
inline void glad_wrapped_glGetColorTableParameteriv(GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetColorTableParameteriv, _target, _pname, _params); }
	#define glGetColorTableParameteriv glad_wrapped_glGetColorTableParameteriv
struct glad_tag_glGetColorTable {};
inline void glad_wrapped_glGetColorTable(GLenum _target, GLenum _format, GLenum _type, void* _table) { GLAD_INSTRUMENT_CALL(glGetColorTable, _target, _format, _type, _table); }
	#define glGetColorTable glad_wrapped_glGetColorTable
struct glad_tag_glVertexArrayColorOffsetEXT {};
inline void glad_wrapped_glVertexArrayColorOffsetEXT(GLuint _vaobj, GLuint _buffer, GLint _size, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayColorOffsetEXT, _vaobj, _buffer, _size, _type, _stride, _offset); }
	#define glVertexArrayColorOffsetEXT glad_wrapped_glVertexArrayColorOffsetEXT
struct glad_tag_glVertexArrayMultiTexCoordOffsetEXT {};
inline void glad_wrapped_glVertexArrayMultiTexCoordOffsetEXT(GLuint _vaobj, GLuint _buffer, GLenum _texunit, GLint _size, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayMultiTexCoordOffsetEXT, _vaobj, _buffer, _texunit, _size, _type, _stride, _offset); }
	#define glVertexArrayMultiTexCoordOffsetEXT glad_wrapped_glVertexArrayMultiTexCoordOffsetEXT
struct glad_tag_glGetMultiTexGenivEXT {};
inline void glad_wrapped_glGetMultiTexGenivEXT(GLenum _texunit, GLenum _coord, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexGenivEXT, _texunit, _coord, _pname, _params); }
	#define glGetMultiTexGenivEXT glad_wrapped_glGetMultiTexGenivEXT
struct glad_tag_glClearDepthdNV {};
inline void glad_wrapped_glClearDepthdNV(GLdouble _depth) { GLAD_INSTRUMENT_CALL(glClearDepthdNV, _depth); }
	#define glClearDepthdNV glad_wrapped_glClearDepthdNV
struct glad_tag_glBindMultiTextureEXT {};
inline void glad_wrapped_glBindMultiTextureEXT(GLenum _texunit, GLenum _target, GLuint _texture) { GLAD_INSTRUMENT_CALL(glBindMultiTextureEXT, _texunit, _target, _texture); }
	#define glBindMultiTextureEXT glad_wrapped_glBindMultiTextureEXT
struct glad_tag_glDisableVertexArrayEXT {};
inline void glad_wrapped_glDisableVertexArrayEXT(GLuint _vaobj, GLenum _array) { GLAD_INSTRUMENT_CALL(glDisableVertexArrayEXT, _vaobj, _array); }
	#define glDisableVertexArrayEXT glad_wrapped_glDisableVertexArrayEXT
struct glad_tag_glFramebufferDrawBuffersEXT {};
inline void glad_wrapped_glFramebufferDrawBuffersEXT(GLuint _framebuffer, GLsizei _n, const GLenum* _bufs) { GLAD_INSTRUMENT_CALL(glFramebufferDrawBuffersEXT, _framebuffer, _n, _bufs); }
	#define glFramebufferDrawBuffersEXT glad_wrapped_glFramebufferDrawBuffersEXT
struct glad_tag_glMapNamedBufferRangeEXT {};
inline void* glad_wrapped_glMapNamedBufferRangeEXT(GLuint _buffer, GLintptr _offset, GLsizeiptr _length, GLbitfield _access) { return GLAD_INSTRUMENT_CALL(glMapNamedBufferRangeEXT, _buffer, _offset, _length, _access); }
	#define glMapNamedBufferRangeEXT glad_wrapped_glMapNamedBufferRangeEXT
struct glad_tag_glNamedRenderbufferStorageMultisampleEXT {};
inline void glad_wrapped_glNamedRenderbufferStorageMultisampleEXT(GLuint _renderbuffer, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glNamedRenderbufferStorageMultisampleEXT, _renderbuffer, _samples, _internalformat, _width, _height); }
	#define glNamedRenderbufferStorageMultisampleEXT glad_wrapped_glNamedRenderbufferStorageMultisampleEXT
struct glad_tag_glConvolutionParameteri {};
inline void glad_wrapped_glConvolutionParameteri(GLenum _target, GLenum _pname, GLint _params) { GLAD_INSTRUMENT_CALL(glConvolutionParameteri, _target, _pname, _params); }
	#define glConvolutionParameteri glad_wrapped_glConvolutionParameteri
struct glad_tag_glConvolutionParameterf {};
inline void glad_wrapped_glConvolutionParameterf(GLenum _target, GLenum _pname, GLfloat _params) { GLAD_INSTRUMENT_CALL(glConvolutionParameterf, _target, _pname, _params); }
	#define glConvolutionParameterf glad_wrapped_glConvolutionParameterf
struct glad_tag_glProgramUniform2fvEXT {};
inline void glad_wrapped_glProgramUniform2fvEXT(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2fvEXT, _program, _location, _count, _value); }
	#define glProgramUniform2fvEXT glad_wrapped_glProgramUniform2fvEXT
struct glad_tag_glNamedProgramLocalParameterI4uiEXT {};
inline void glad_wrapped_glNamedProgramLocalParameterI4uiEXT(GLuint _program, GLenum _target, GLuint _index, GLuint _x, GLuint _y, GLuint _z, GLuint _w) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameterI4uiEXT, _program, _target, _index, _x, _y, _z, _w); }
	#define glNamedProgramLocalParameterI4uiEXT glad_wrapped_glNamedProgramLocalParameterI4uiEXT
struct glad_tag_glTextureParameterIuivEXT {};
inline void glad_wrapped_glTextureParameterIuivEXT(GLuint _texture, GLenum _target, GLenum _pname, const GLuint* _params) { GLAD_INSTRUMENT_CALL(glTextureParameterIuivEXT, _texture, _target, _pname, _params); }
	#define glTextureParameterIuivEXT glad_wrapped_glTextureParameterIuivEXT
struct glad_tag_glProgramUniform1iEXT {};
inline void glad_wrapped_glProgramUniform1iEXT(GLuint _program, GLint _location, GLint _v0) { GLAD_INSTRUMENT_CALL(glProgramUniform1iEXT, _program, _location, _v0); }
	#define glProgramUniform1iEXT glad_wrapped_glProgramUniform1iEXT
struct glad_tag_glMultiTexSubImage3DEXT {};
inline void glad_wrapped_glMultiTexSubImage3DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glMultiTexSubImage3DEXT, _texunit, _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _type, _pixels); }
	#define glMultiTexSubImage3DEXT glad_wrapped_glMultiTexSubImage3DEXT
struct glad_tag_glProgramUniformHandleui64vNV {};
inline void glad_wrapped_glProgramUniformHandleui64vNV(GLuint _program, GLint _location, GLsizei _count, const GLuint64* _values) { GLAD_INSTRUMENT_CALL(glProgramUniformHandleui64vNV, _program, _location, _count, _values); }
	#define glProgramUniformHandleui64vNV glad_wrapped_glProgramUniformHandleui64vNV
struct glad_tag_glNamedFramebufferTextureFaceEXT {};
inline void glad_wrapped_glNamedFramebufferTextureFaceEXT(GLuint _framebuffer, GLenum _attachment, GLuint _texture, GLint _level, GLenum _face) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTextureFaceEXT, _framebuffer, _attachment, _texture, _level, _face); }
	#define glNamedFramebufferTextureFaceEXT glad_wrapped_glNamedFramebufferTextureFaceEXT
struct glad_tag_glStringMarkerGREMEDY {};
inline void glad_wrapped_glStringMarkerGREMEDY(GLsizei _len, const void* _string) { GLAD_INSTRUMENT_CALL(glStringMarkerGREMEDY, _len, _string); }
	#define glStringMarkerGREMEDY glad_wrapped_glStringMarkerGREMEDY
struct glad_tag_glGetMinmaxParameterfv {};
inline void glad_wrapped_glGetMinmaxParameterfv(GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetMinmaxParameterfv, _target, _pname, _params); }
	#define glGetMinmaxParameterfv glad_wrapped_glGetMinmaxParameterfv
struct glad_tag_glProgramUniformHandleui64NV {};
inline void glad_wrapped_glProgramUniformHandleui64NV(GLuint _program, GLint _location, GLuint64 _value) { GLAD_INSTRUMENT_CALL(glProgramUniformHandleui64NV, _program, _location, _value); }
	#define glProgramUniformHandleui64NV glad_wrapped_glProgramUniformHandleui64NV
struct glad_tag_glCopyConvolutionFilter2D {};
inline void glad_wrapped_glCopyConvolutionFilter2D(GLenum _target, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyConvolutionFilter2D, _target, _internalformat, _x, _y, _width, _height); }
	#define glCopyConvolutionFilter2D glad_wrapped_glCopyConvolutionFilter2D
struct glad_tag_glGetDoublei_vEXT {};
inline void glad_wrapped_glGetDoublei_vEXT(GLenum _pname, GLuint _index, GLdouble* _params) { GLAD_INSTRUMENT_CALL(glGetDoublei_vEXT, _pname, _index, _params); }
	#define glGetDoublei_vEXT glad_wrapped_glGetDoublei_vEXT
struct glad_tag_glVertexArrayIndexOffsetEXT {};
inline void glad_wrapped_glVertexArrayIndexOffsetEXT(GLuint _vaobj, GLuint _buffer, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayIndexOffsetEXT, _vaobj, _buffer, _type, _stride, _offset); }
	#define glVertexArrayIndexOffsetEXT glad_wrapped_glVertexArrayIndexOffsetEXT
struct glad_tag_glDisableClientStateiEXT {};
inline void glad_wrapped_glDisableClientStateiEXT(GLenum _array, GLuint _index) { GLAD_INSTRUMENT_CALL(glDisableClientStateiEXT, _array, _index); }
	#define glDisableClientStateiEXT glad_wrapped_glDisableClientStateiEXT
struct glad_tag_glMultiTexImage3DEXT {};
inline void glad_wrapped_glMultiTexImage3DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glMultiTexImage3DEXT, _texunit, _target, _level, _internalformat, _width, _height, _depth, _border, _format, _type, _pixels); }
	#define glMultiTexImage3DEXT glad_wrapped_glMultiTexImage3DEXT
struct glad_tag_glNamedRenderbufferStorageMultisampleCoverageEXT {};
inline void glad_wrapped_glNamedRenderbufferStorageMultisampleCoverageEXT(GLuint _renderbuffer, GLsizei _coverageSamples, GLsizei _colorSamples, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glNamedRenderbufferStorageMultisampleCoverageEXT, _renderbuffer, _coverageSamples, _colorSamples, _internalformat, _width, _height); }
	#define glNamedRenderbufferStorageMultisampleCoverageEXT glad_wrapped_glNamedRenderbufferStorageMultisampleCoverageEXT
struct glad_tag_glMultiTexBufferEXT {};
inline void glad_wrapped_glMultiTexBufferEXT(GLenum _texunit, GLenum _target, GLenum _internalformat, GLuint _buffer) { GLAD_INSTRUMENT_CALL(glMultiTexBufferEXT, _texunit, _target, _internalformat, _buffer); }
	#define glMultiTexBufferEXT glad_wrapped_glMultiTexBufferEXT
struct glad_tag_glMultiDrawElementsIndirectBindlessCountNV {};
inline void glad_wrapped_glMultiDrawElementsIndirectBindlessCountNV(GLenum _mode, GLenum _type, const void* _indirect, GLsizei _drawCount, GLsizei _maxDrawCount, GLsizei _stride, GLint _vertexBufferCount) { GLAD_INSTRUMENT_CALL(glMultiDrawElementsIndirectBindlessCountNV, _mode, _type, _indirect, _drawCount, _maxDrawCount, _stride, _vertexBufferCount); }
	#define glMultiDrawElementsIndirectBindlessCountNV glad_wrapped_glMultiDrawElementsIndirectBindlessCountNV
struct glad_tag_glMultiTexParameterIuivEXT {};
inline void glad_wrapped_glMultiTexParameterIuivEXT(GLenum _texunit, GLenum _target, GLenum _pname, const GLuint* _params) { GLAD_INSTRUMENT_CALL(glMultiTexParameterIuivEXT, _texunit, _target, _pname, _params); }
	#define glMultiTexParameterIuivEXT glad_wrapped_glMultiTexParameterIuivEXT
struct glad_tag_glProgramUniform2dEXT {};
inline void glad_wrapped_glProgramUniform2dEXT(GLuint _program, GLint _location, GLdouble _x, GLdouble _y) { GLAD_INSTRUMENT_CALL(glProgramUniform2dEXT, _program, _location, _x, _y); }
	#define glProgramUniform2dEXT glad_wrapped_glProgramUniform2dEXT
struct glad_tag_glGetCompressedTextureImageEXT {};
inline void glad_wrapped_glGetCompressedTextureImageEXT(GLuint _texture, GLenum _target, GLint _lod, void* _img) { GLAD_INSTRUMENT_CALL(glGetCompressedTextureImageEXT, _texture, _target, _lod, _img); }
	#define glGetCompressedTextureImageEXT glad_wrapped_glGetCompressedTextureImageEXT
struct glad_tag_glGetIntegerui64i_vNV {};
inline void glad_wrapped_glGetIntegerui64i_vNV(GLenum _value, GLuint _index, GLuint64EXT* _result) { GLAD_INSTRUMENT_CALL(glGetIntegerui64i_vNV, _value, _index, _result); }
	#define glGetIntegerui64i_vNV glad_wrapped_glGetIntegerui64i_vNV
struct glad_tag_glUniformHandleui64NV {};
inline void glad_wrapped_glUniformHandleui64NV(GLint _location, GLuint64 _value) { GLAD_INSTRUMENT_CALL(glUniformHandleui64NV, _location, _value); }
	#define glUniformHandleui64NV glad_wrapped_glUniformHandleui64NV
struct glad_tag_glGetUniformui64vNV {};
inline void glad_wrapped_glGetUniformui64vNV(GLuint _program, GLint _location, GLuint64EXT* _params) { GLAD_INSTRUMENT_CALL(glGetUniformui64vNV, _program, _location, _params); }
	#define glGetUniformui64vNV glad_wrapped_glGetUniformui64vNV
struct glad_tag_glNamedRenderbufferStorageEXT {};
inline void glad_wrapped_glNamedRenderbufferStorageEXT(GLuint _renderbuffer, GLenum _internalformat, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glNamedRenderbufferStorageEXT, _renderbuffer, _internalformat, _width, _height); }
	#define glNamedRenderbufferStorageEXT glad_wrapped_glNamedRenderbufferStorageEXT
struct glad_tag_glTextureStorage1DEXT {};
inline void glad_wrapped_glTextureStorage1DEXT(GLuint _texture, GLenum _target, GLsizei _levels, GLenum _internalformat, GLsizei _width) { GLAD_INSTRUMENT_CALL(glTextureStorage1DEXT, _texture, _target, _levels, _internalformat, _width); }
	#define glTextureStorage1DEXT glad_wrapped_glTextureStorage1DEXT
struct glad_tag_glNamedBufferSubDataEXT {};
inline void glad_wrapped_glNamedBufferSubDataEXT(GLuint _buffer, GLintptr _offset, GLsizeiptr _size, const void* _data) { GLAD_INSTRUMENT_CALL(glNamedBufferSubDataEXT, _buffer, _offset, _size, _data); }
	#define glNamedBufferSubDataEXT glad_wrapped_glNamedBufferSubDataEXT
struct glad_tag_glNormalFormatNV {};
inline void glad_wrapped_glNormalFormatNV(GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glNormalFormatNV, _type, _stride); }
	#define glNormalFormatNV glad_wrapped_glNormalFormatNV
struct glad_tag_glIsBufferResidentNV {};
inline GLboolean glad_wrapped_glIsBufferResidentNV(GLenum _target) { return GLAD_INSTRUMENT_CALL(glIsBufferResidentNV, _target); }
	#define glIsBufferResidentNV glad_wrapped_glIsBufferResidentNV
struct glad_tag_glHistogram {};
inline void glad_wrapped_glHistogram(GLenum _target, GLsizei _width, GLenum _internalformat, GLboolean _sink) { GLAD_INSTRUMENT_CALL(glHistogram, _target, _width, _internalformat, _sink); }
	#define glHistogram glad_wrapped_glHistogram
struct glad_tag_glGetNamedRenderbufferParameterivEXT {};
inline void glad_wrapped_glGetNamedRenderbufferParameterivEXT(GLuint _renderbuffer, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedRenderbufferParameterivEXT, _renderbuffer, _pname, _params); }
	#define glGetNamedRenderbufferParameterivEXT glad_wrapped_glGetNamedRenderbufferParameterivEXT
struct glad_tag_glCompressedTextureImage1DEXT {};
inline void glad_wrapped_glCompressedTextureImage1DEXT(GLuint _texture, GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLint _border, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedTextureImage1DEXT, _texture, _target, _level, _internalformat, _width, _border, _imageSize, _bits); }
	#define glCompressedTextureImage1DEXT glad_wrapped_glCompressedTextureImage1DEXT
struct glad_tag_glProgramUniformMatrix4x3fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix4x3fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x3fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x3fvEXT glad_wrapped_glProgramUniformMatrix4x3fvEXT
struct glad_tag_glGetObjectLabelKHR {};
inline void glad_wrapped_glGetObjectLabelKHR(GLenum _identifier, GLuint _name, GLsizei _bufSize, GLsizei* _length, GLchar* _label) { GLAD_INSTRUMENT_CALL(glGetObjectLabelKHR, _identifier, _name, _bufSize, _length, _label); }
	#define glGetObjectLabelKHR glad_wrapped_glGetObjectLabelKHR
struct glad_tag_glProgramUniformMatrix4fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix4fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4fvEXT glad_wrapped_glProgramUniformMatrix4fvEXT
struct glad_tag_glMultiTexGendvEXT {};
inline void glad_wrapped_glMultiTexGendvEXT(GLenum _texunit, GLenum _coord, GLenum _pname, const GLdouble* _params) { GLAD_INSTRUMENT_CALL(glMultiTexGendvEXT, _texunit, _coord, _pname, _params); }
	#define glMultiTexGendvEXT glad_wrapped_glMultiTexGendvEXT
struct glad_tag_glIsTextureHandleResidentNV {};
inline GLboolean glad_wrapped_glIsTextureHandleResidentNV(GLuint64 _handle) { return GLAD_INSTRUMENT_CALL(glIsTextureHandleResidentNV, _handle); }
	#define glIsTextureHandleResidentNV glad_wrapped_glIsTextureHandleResidentNV
struct glad_tag_glNamedFramebufferTextureEXT {};
inline void glad_wrapped_glNamedFramebufferTextureEXT(GLuint _framebuffer, GLenum _attachment, GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTextureEXT, _framebuffer, _attachment, _texture, _level); }
	#define glNamedFramebufferTextureEXT glad_wrapped_glNamedFramebufferTextureEXT
struct glad_tag_glNamedProgramLocalParameter4fEXT {};
inline void glad_wrapped_glNamedProgramLocalParameter4fEXT(GLuint _program, GLenum _target, GLuint _index, GLfloat _x, GLfloat _y, GLfloat _z, GLfloat _w) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameter4fEXT, _program, _target, _index, _x, _y, _z, _w); }
	#define glNamedProgramLocalParameter4fEXT glad_wrapped_glNamedProgramLocalParameter4fEXT
struct glad_tag_glSecondaryColorFormatNV {};
inline void glad_wrapped_glSecondaryColorFormatNV(GLint _size, GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glSecondaryColorFormatNV, _size, _type, _stride); }
	#define glSecondaryColorFormatNV glad_wrapped_glSecondaryColorFormatNV
struct glad_tag_glMatrixPopEXT {};
inline void glad_wrapped_glMatrixPopEXT(GLenum _mode) { GLAD_INSTRUMENT_CALL(glMatrixPopEXT, _mode); }
	#define glMatrixPopEXT glad_wrapped_glMatrixPopEXT
struct glad_tag_glMultiTexEnviEXT {};
inline void glad_wrapped_glMultiTexEnviEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLint _param) { GLAD_INSTRUMENT_CALL(glMultiTexEnviEXT, _texunit, _target, _pname, _param); }
	#define glMultiTexEnviEXT glad_wrapped_glMultiTexEnviEXT
struct glad_tag_glProgramUniform1uivEXT {};
inline void glad_wrapped_glProgramUniform1uivEXT(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1uivEXT, _program, _location, _count, _value); }
	#define glProgramUniform1uivEXT glad_wrapped_glProgramUniform1uivEXT
struct glad_tag_glVertexArrayVertexAttribLFormatEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribLFormatEXT(GLuint _vaobj, GLuint _attribindex, GLint _size, GLenum _type, GLuint _relativeoffset) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribLFormatEXT, _vaobj, _attribindex, _size, _type, _relativeoffset); }
	#define glVertexArrayVertexAttribLFormatEXT glad_wrapped_glVertexArrayVertexAttribLFormatEXT
struct glad_tag_glCopyTextureSubImage3DEXT {};
inline void glad_wrapped_glCopyTextureSubImage3DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLint _x, GLint _y, GLsizei _width, GLsizei _height) { GLAD_INSTRUMENT_CALL(glCopyTextureSubImage3DEXT, _texture, _target, _level, _xoffset, _yoffset, _zoffset, _x, _y, _width, _height); }
	#define glCopyTextureSubImage3DEXT glad_wrapped_glCopyTextureSubImage3DEXT
struct glad_tag_glGetNamedFramebufferParameterivEXT {};
inline void glad_wrapped_glGetNamedFramebufferParameterivEXT(GLuint _framebuffer, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedFramebufferParameterivEXT, _framebuffer, _pname, _params); }
	#define glGetNamedFramebufferParameterivEXT glad_wrapped_glGetNamedFramebufferParameterivEXT
struct glad_tag_glProgramUniform4uivEXT {};
inline void glad_wrapped_glProgramUniform4uivEXT(GLuint _program, GLint _location, GLsizei _count, const GLuint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4uivEXT, _program, _location, _count, _value); }
	#define glProgramUniform4uivEXT glad_wrapped_glProgramUniform4uivEXT
struct glad_tag_glMakeBufferNonResidentNV {};
inline void glad_wrapped_glMakeBufferNonResidentNV(GLenum _target) { GLAD_INSTRUMENT_CALL(glMakeBufferNonResidentNV, _target); }
	#define glMakeBufferNonResidentNV glad_wrapped_glMakeBufferNonResidentNV
struct glad_tag_glGetNamedProgramLocalParameterdvEXT {};
inline void glad_wrapped_glGetNamedProgramLocalParameterdvEXT(GLuint _program, GLenum _target, GLuint _index, GLdouble* _params) { GLAD_INSTRUMENT_CALL(glGetNamedProgramLocalParameterdvEXT, _program, _target, _index, _params); }
	#define glGetNamedProgramLocalParameterdvEXT glad_wrapped_glGetNamedProgramLocalParameterdvEXT
struct glad_tag_glUniformHandleui64vNV {};
inline void glad_wrapped_glUniformHandleui64vNV(GLint _location, GLsizei _count, const GLuint64* _value) { GLAD_INSTRUMENT_CALL(glUniformHandleui64vNV, _location, _count, _value); }
	#define glUniformHandleui64vNV glad_wrapped_glUniformHandleui64vNV
struct glad_tag_glGetNamedProgramStringEXT {};
inline void glad_wrapped_glGetNamedProgramStringEXT(GLuint _program, GLenum _target, GLenum _pname, void* _string) { GLAD_INSTRUMENT_CALL(glGetNamedProgramStringEXT, _program, _target, _pname, _string); }
	#define glGetNamedProgramStringEXT glad_wrapped_glGetNamedProgramStringEXT
struct glad_tag_glFogCoordFormatNV {};
inline void glad_wrapped_glFogCoordFormatNV(GLenum _type, GLsizei _stride) { GLAD_INSTRUMENT_CALL(glFogCoordFormatNV, _type, _stride); }
	#define glFogCoordFormatNV glad_wrapped_glFogCoordFormatNV
struct glad_tag_glGetMultiTexGenfvEXT {};
inline void glad_wrapped_glGetMultiTexGenfvEXT(GLenum _texunit, GLenum _coord, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexGenfvEXT, _texunit, _coord, _pname, _params); }
	#define glGetMultiTexGenfvEXT glad_wrapped_glGetMultiTexGenfvEXT
struct glad_tag_glGetDebugMessageLogKHR {};
inline GLuint glad_wrapped_glGetDebugMessageLogKHR(GLuint _count, GLsizei _bufSize, GLenum* _sources, GLenum* _types, GLuint* _ids, GLenum* _severities, GLsizei* _lengths, GLchar* _messageLog) { return GLAD_INSTRUMENT_CALL(glGetDebugMessageLogKHR, _count, _bufSize, _sources, _types, _ids, _severities, _lengths, _messageLog); }
	#define glGetDebugMessageLogKHR glad_wrapped_glGetDebugMessageLogKHR
struct glad_tag_glConvolutionParameterfv {};
inline void glad_wrapped_glConvolutionParameterfv(GLenum _target, GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glConvolutionParameterfv, _target, _pname, _params); }
	#define glConvolutionParameterfv glad_wrapped_glConvolutionParameterfv
struct glad_tag_glMatrixFrustumEXT {};
inline void glad_wrapped_glMatrixFrustumEXT(GLenum _mode, GLdouble _left, GLdouble _right, GLdouble _bottom, GLdouble _top, GLdouble _zNear, GLdouble _zFar) { GLAD_INSTRUMENT_CALL(glMatrixFrustumEXT, _mode, _left, _right, _bottom, _top, _zNear, _zFar); }
	#define glMatrixFrustumEXT glad_wrapped_glMatrixFrustumEXT
struct glad_tag_glProgramUniform2uiEXT {};
inline void glad_wrapped_glProgramUniform2uiEXT(GLuint _program, GLint _location, GLuint _v0, GLuint _v1) { GLAD_INSTRUMENT_CALL(glProgramUniform2uiEXT, _program, _location, _v0, _v1); }
	#define glProgramUniform2uiEXT glad_wrapped_glProgramUniform2uiEXT
struct glad_tag_glCompressedMultiTexImage3DEXT {};
inline void glad_wrapped_glCompressedMultiTexImage3DEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedMultiTexImage3DEXT, _texunit, _target, _level, _internalformat, _width, _height, _depth, _border, _imageSize, _bits); }
	#define glCompressedMultiTexImage3DEXT glad_wrapped_glCompressedMultiTexImage3DEXT
struct glad_tag_glProgramUniformMatrix2x3fvEXT {};
inline void glad_wrapped_glProgramUniformMatrix2x3fvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix2x3fvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix2x3fvEXT glad_wrapped_glProgramUniformMatrix2x3fvEXT
struct glad_tag_glProgramUniform1fEXT {};
inline void glad_wrapped_glProgramUniform1fEXT(GLuint _program, GLint _location, GLfloat _v0) { GLAD_INSTRUMENT_CALL(glProgramUniform1fEXT, _program, _location, _v0); }
	#define glProgramUniform1fEXT glad_wrapped_glProgramUniform1fEXT
struct glad_tag_glProgramUniform3dEXT {};
inline void glad_wrapped_glProgramUniform3dEXT(GLuint _program, GLint _location, GLdouble _x, GLdouble _y, GLdouble _z) { GLAD_INSTRUMENT_CALL(glProgramUniform3dEXT, _program, _location, _x, _y, _z); }
	#define glProgramUniform3dEXT glad_wrapped_glProgramUniform3dEXT
struct glad_tag_glTextureImage3DEXT {};
inline void glad_wrapped_glTextureImage3DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureImage3DEXT, _texture, _target, _level, _internalformat, _width, _height, _depth, _border, _format, _type, _pixels); }
	#define glTextureImage3DEXT glad_wrapped_glTextureImage3DEXT
struct glad_tag_glGetMultiTexLevelParameterfvEXT {};
inline void glad_wrapped_glGetMultiTexLevelParameterfvEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexLevelParameterfvEXT, _texunit, _target, _level, _pname, _params); }
	#define glGetMultiTexLevelParameterfvEXT glad_wrapped_glGetMultiTexLevelParameterfvEXT
struct glad_tag_glIsEnabledIndexedEXT {};
inline GLboolean glad_wrapped_glIsEnabledIndexedEXT(GLenum _target, GLuint _index) { return GLAD_INSTRUMENT_CALL(glIsEnabledIndexedEXT, _target, _index); }
	#define glIsEnabledIndexedEXT glad_wrapped_glIsEnabledIndexedEXT
struct glad_tag_glGetNamedProgramivEXT {};
inline void glad_wrapped_glGetNamedProgramivEXT(GLuint _program, GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedProgramivEXT, _program, _target, _pname, _params); }
	#define glGetNamedProgramivEXT glad_wrapped_glGetNamedProgramivEXT
struct glad_tag_glProgramUniform3fvEXT {};
inline void glad_wrapped_glProgramUniform3fvEXT(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3fvEXT, _program, _location, _count, _value); }
	#define glProgramUniform3fvEXT glad_wrapped_glProgramUniform3fvEXT
struct glad_tag_glVertexArraySecondaryColorOffsetEXT {};
inline void glad_wrapped_glVertexArraySecondaryColorOffsetEXT(GLuint _vaobj, GLuint _buffer, GLint _size, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArraySecondaryColorOffsetEXT, _vaobj, _buffer, _size, _type, _stride, _offset); }
	#define glVertexArraySecondaryColorOffsetEXT glad_wrapped_glVertexArraySecondaryColorOffsetEXT
struct glad_tag_glGetNamedProgramLocalParameterIuivEXT {};
inline void glad_wrapped_glGetNamedProgramLocalParameterIuivEXT(GLuint _program, GLenum _target, GLuint _index, GLuint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedProgramLocalParameterIuivEXT, _program, _target, _index, _params); }
	#define glGetNamedProgramLocalParameterIuivEXT glad_wrapped_glGetNamedProgramLocalParameterIuivEXT
struct glad_tag_glGetVertexArrayPointervEXT {};
inline void glad_wrapped_glGetVertexArrayPointervEXT(GLuint _vaobj, GLenum _pname, void** _param) { GLAD_INSTRUMENT_CALL(glGetVertexArrayPointervEXT, _vaobj, _pname, _param); }
	#define glGetVertexArrayPointervEXT glad_wrapped_glGetVertexArrayPointervEXT
struct glad_tag_glEnableClientStateIndexedEXT {};
inline void glad_wrapped_glEnableClientStateIndexedEXT(GLenum _array, GLuint _index) { GLAD_INSTRUMENT_CALL(glEnableClientStateIndexedEXT, _array, _index); }
	#define glEnableClientStateIndexedEXT glad_wrapped_glEnableClientStateIndexedEXT
struct glad_tag_glMatrixLoadIdentityEXT {};
inline void glad_wrapped_glMatrixLoadIdentityEXT(GLenum _mode) { GLAD_INSTRUMENT_CALL(glMatrixLoadIdentityEXT, _mode); }
	#define glMatrixLoadIdentityEXT glad_wrapped_glMatrixLoadIdentityEXT
struct glad_tag_glGetObjectPtrLabelKHR {};
inline void glad_wrapped_glGetObjectPtrLabelKHR(const void* _ptr, GLsizei _bufSize, GLsizei* _length, GLchar* _label) { GLAD_INSTRUMENT_CALL(glGetObjectPtrLabelKHR, _ptr, _bufSize, _length, _label); }
	#define glGetObjectPtrLabelKHR glad_wrapped_glGetObjectPtrLabelKHR
struct glad_tag_glNamedProgramLocalParameterI4iEXT {};
inline void glad_wrapped_glNamedProgramLocalParameterI4iEXT(GLuint _program, GLenum _target, GLuint _index, GLint _x, GLint _y, GLint _z, GLint _w) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameterI4iEXT, _program, _target, _index, _x, _y, _z, _w); }
	#define glNamedProgramLocalParameterI4iEXT glad_wrapped_glNamedProgramLocalParameterI4iEXT
struct glad_tag_glMultiTexGenivEXT {};
inline void glad_wrapped_glMultiTexGenivEXT(GLenum _texunit, GLenum _coord, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glMultiTexGenivEXT, _texunit, _coord, _pname, _params); }
	#define glMultiTexGenivEXT glad_wrapped_glMultiTexGenivEXT
struct glad_tag_glNamedFramebufferTexture2DEXT {};
inline void glad_wrapped_glNamedFramebufferTexture2DEXT(GLuint _framebuffer, GLenum _attachment, GLenum _textarget, GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTexture2DEXT, _framebuffer, _attachment, _textarget, _texture, _level); }
	#define glNamedFramebufferTexture2DEXT glad_wrapped_glNamedFramebufferTexture2DEXT
struct glad_tag_glTextureImage2DEXT {};
inline void glad_wrapped_glTextureImage2DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLsizei _height, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureImage2DEXT, _texture, _target, _level, _internalformat, _width, _height, _border, _format, _type, _pixels); }
	#define glTextureImage2DEXT glad_wrapped_glTextureImage2DEXT
struct glad_tag_glTextureStorage3DMultisampleEXT {};
inline void glad_wrapped_glTextureStorage3DMultisampleEXT(GLuint _texture, GLenum _target, GLsizei _samples, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLboolean _fixedsamplelocations) { GLAD_INSTRUMENT_CALL(glTextureStorage3DMultisampleEXT, _texture, _target, _samples, _internalformat, _width, _height, _depth, _fixedsamplelocations); }
	#define glTextureStorage3DMultisampleEXT glad_wrapped_glTextureStorage3DMultisampleEXT
struct glad_tag_glResetHistogram {};
inline void glad_wrapped_glResetHistogram(GLenum _target) { GLAD_INSTRUMENT_CALL(glResetHistogram, _target); }
	#define glResetHistogram glad_wrapped_glResetHistogram
struct glad_tag_glDepthRangedNV {};
inline void glad_wrapped_glDepthRangedNV(GLdouble _zNear, GLdouble _zFar) { GLAD_INSTRUMENT_CALL(glDepthRangedNV, _zNear, _zFar); }
	#define glDepthRangedNV glad_wrapped_glDepthRangedNV
struct glad_tag_glCompressedMultiTexImage1DEXT {};
inline void glad_wrapped_glCompressedMultiTexImage1DEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLint _border, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedMultiTexImage1DEXT, _texunit, _target, _level, _internalformat, _width, _border, _imageSize, _bits); }
	#define glCompressedMultiTexImage1DEXT glad_wrapped_glCompressedMultiTexImage1DEXT
struct glad_tag_glProgramUniform2ivEXT {};
inline void glad_wrapped_glProgramUniform2ivEXT(GLuint _program, GLint _location, GLsizei _count, const GLint* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform2ivEXT, _program, _location, _count, _value); }
	#define glProgramUniform2ivEXT glad_wrapped_glProgramUniform2ivEXT
struct glad_tag_glProgramUniform4fvEXT {};
inline void glad_wrapped_glProgramUniform4fvEXT(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform4fvEXT, _program, _location, _count, _value); }
	#define glProgramUniform4fvEXT glad_wrapped_glProgramUniform4fvEXT
struct glad_tag_glMultiTexCoordPointerEXT {};
inline void glad_wrapped_glMultiTexCoordPointerEXT(GLenum _texunit, GLint _size, GLenum _type, GLsizei _stride, const void* _pointer) { GLAD_INSTRUMENT_CALL(glMultiTexCoordPointerEXT, _texunit, _size, _type, _stride, _pointer); }
	#define glMultiTexCoordPointerEXT glad_wrapped_glMultiTexCoordPointerEXT
struct glad_tag_glGenerateMultiTexMipmapEXT {};
inline void glad_wrapped_glGenerateMultiTexMipmapEXT(GLenum _texunit, GLenum _target) { GLAD_INSTRUMENT_CALL(glGenerateMultiTexMipmapEXT, _texunit, _target); }
	#define glGenerateMultiTexMipmapEXT glad_wrapped_glGenerateMultiTexMipmapEXT
struct glad_tag_glNamedFramebufferTexture1DEXT {};
inline void glad_wrapped_glNamedFramebufferTexture1DEXT(GLuint _framebuffer, GLenum _attachment, GLenum _textarget, GLuint _texture, GLint _level) { GLAD_INSTRUMENT_CALL(glNamedFramebufferTexture1DEXT, _framebuffer, _attachment, _textarget, _texture, _level); }
	#define glNamedFramebufferTexture1DEXT glad_wrapped_glNamedFramebufferTexture1DEXT
struct glad_tag_glMultiTexEnvfEXT {};
inline void glad_wrapped_glMultiTexEnvfEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glMultiTexEnvfEXT, _texunit, _target, _pname, _param); }
	#define glMultiTexEnvfEXT glad_wrapped_glMultiTexEnvfEXT
struct glad_tag_glGetTextureParameterIivEXT {};
inline void glad_wrapped_glGetTextureParameterIivEXT(GLuint _texture, GLenum _target, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameterIivEXT, _texture, _target, _pname, _params); }
	#define glGetTextureParameterIivEXT glad_wrapped_glGetTextureParameterIivEXT
struct glad_tag_glNamedProgramLocalParametersI4uivEXT {};
inline void glad_wrapped_glNamedProgramLocalParametersI4uivEXT(GLuint _program, GLenum _target, GLuint _index, GLsizei _count, const GLuint* _params) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParametersI4uivEXT, _program, _target, _index, _count, _params); }
	#define glNamedProgramLocalParametersI4uivEXT glad_wrapped_glNamedProgramLocalParametersI4uivEXT
struct glad_tag_glGetBooleanIndexedvEXT {};
inline void glad_wrapped_glGetBooleanIndexedvEXT(GLenum _target, GLuint _index, GLboolean* _data) { GLAD_INSTRUMENT_CALL(glGetBooleanIndexedvEXT, _target, _index, _data); }
	#define glGetBooleanIndexedvEXT glad_wrapped_glGetBooleanIndexedvEXT
struct glad_tag_glTextureSubImage1DEXT {};
inline void glad_wrapped_glTextureSubImage1DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureSubImage1DEXT, _texture, _target, _level, _xoffset, _width, _format, _type, _pixels); }
	#define glTextureSubImage1DEXT glad_wrapped_glTextureSubImage1DEXT
struct glad_tag_glCopyMultiTexImage2DEXT {};
inline void glad_wrapped_glCopyMultiTexImage2DEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _internalformat, GLint _x, GLint _y, GLsizei _width, GLsizei _height, GLint _border) { GLAD_INSTRUMENT_CALL(glCopyMultiTexImage2DEXT, _texunit, _target, _level, _internalformat, _x, _y, _width, _height, _border); }
	#define glCopyMultiTexImage2DEXT glad_wrapped_glCopyMultiTexImage2DEXT
struct glad_tag_glNamedProgramLocalParameter4fvEXT {};
inline void glad_wrapped_glNamedProgramLocalParameter4fvEXT(GLuint _program, GLenum _target, GLuint _index, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameter4fvEXT, _program, _target, _index, _params); }
	#define glNamedProgramLocalParameter4fvEXT glad_wrapped_glNamedProgramLocalParameter4fvEXT
struct glad_tag_glGetCompressedMultiTexImageEXT {};
inline void glad_wrapped_glGetCompressedMultiTexImageEXT(GLenum _texunit, GLenum _target, GLint _lod, void* _img) { GLAD_INSTRUMENT_CALL(glGetCompressedMultiTexImageEXT, _texunit, _target, _lod, _img); }
	#define glGetCompressedMultiTexImageEXT glad_wrapped_glGetCompressedMultiTexImageEXT
struct glad_tag_glColorTable {};
inline void glad_wrapped_glColorTable(GLenum _target, GLenum _internalformat, GLsizei _width, GLenum _format, GLenum _type, const void* _table) { GLAD_INSTRUMENT_CALL(glColorTable, _target, _internalformat, _width, _format, _type, _table); }
	#define glColorTable glad_wrapped_glColorTable
struct glad_tag_glMultiTexSubImage1DEXT {};
inline void glad_wrapped_glMultiTexSubImage1DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _xoffset, GLsizei _width, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glMultiTexSubImage1DEXT, _texunit, _target, _level, _xoffset, _width, _format, _type, _pixels); }
	#define glMultiTexSubImage1DEXT glad_wrapped_glMultiTexSubImage1DEXT
struct glad_tag_glGetImageHandleNV {};
inline GLuint64 glad_wrapped_glGetImageHandleNV(GLuint _texture, GLint _level, GLboolean _layered, GLint _layer, GLenum _format) { return GLAD_INSTRUMENT_CALL(glGetImageHandleNV, _texture, _level, _layered, _layer, _format); }
	#define glGetImageHandleNV glad_wrapped_glGetImageHandleNV
struct glad_tag_glProgramUniform4dEXT {};
inline void glad_wrapped_glProgramUniform4dEXT(GLuint _program, GLint _location, GLdouble _x, GLdouble _y, GLdouble _z, GLdouble _w) { GLAD_INSTRUMENT_CALL(glProgramUniform4dEXT, _program, _location, _x, _y, _z, _w); }
	#define glProgramUniform4dEXT glad_wrapped_glProgramUniform4dEXT
struct glad_tag_glTextureParameterIivEXT {};
inline void glad_wrapped_glTextureParameterIivEXT(GLuint _texture, GLenum _target, GLenum _pname, const GLint* _params) { GLAD_INSTRUMENT_CALL(glTextureParameterIivEXT, _texture, _target, _pname, _params); }
	#define glTextureParameterIivEXT glad_wrapped_glTextureParameterIivEXT
struct glad_tag_glMultiTexParameterfvEXT {};
inline void glad_wrapped_glMultiTexParameterfvEXT(GLenum _texunit, GLenum _target, GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glMultiTexParameterfvEXT, _texunit, _target, _pname, _params); }
	#define glMultiTexParameterfvEXT glad_wrapped_glMultiTexParameterfvEXT
struct glad_tag_glMakeImageHandleResidentNV {};
inline void glad_wrapped_glMakeImageHandleResidentNV(GLuint64 _handle, GLenum _access) { GLAD_INSTRUMENT_CALL(glMakeImageHandleResidentNV, _handle, _access); }
	#define glMakeImageHandleResidentNV glad_wrapped_glMakeImageHandleResidentNV
struct glad_tag_glNamedProgramLocalParameter4dvEXT {};
inline void glad_wrapped_glNamedProgramLocalParameter4dvEXT(GLuint _program, GLenum _target, GLuint _index, const GLdouble* _params) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameter4dvEXT, _program, _target, _index, _params); }
	#define glNamedProgramLocalParameter4dvEXT glad_wrapped_glNamedProgramLocalParameter4dvEXT
struct glad_tag_glTextureImage1DEXT {};
inline void glad_wrapped_glTextureImage1DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureImage1DEXT, _texture, _target, _level, _internalformat, _width, _border, _format, _type, _pixels); }
	#define glTextureImage1DEXT glad_wrapped_glTextureImage1DEXT
struct glad_tag_glMatrixScalefEXT {};
inline void glad_wrapped_glMatrixScalefEXT(GLenum _mode, GLfloat _x, GLfloat _y, GLfloat _z) { GLAD_INSTRUMENT_CALL(glMatrixScalefEXT, _mode, _x, _y, _z); }
	#define glMatrixScalefEXT glad_wrapped_glMatrixScalefEXT
struct glad_tag_glGetTextureParameterfvEXT {};
inline void glad_wrapped_glGetTextureParameterfvEXT(GLuint _texture, GLenum _target, GLenum _pname, GLfloat* _params) { GLAD_INSTRUMENT_CALL(glGetTextureParameterfvEXT, _texture, _target, _pname, _params); }
	#define glGetTextureParameterfvEXT glad_wrapped_glGetTextureParameterfvEXT
struct glad_tag_glTexturePageCommitmentEXT {};
inline void glad_wrapped_glTexturePageCommitmentEXT(GLuint _texture, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLboolean _commit) { GLAD_INSTRUMENT_CALL(glTexturePageCommitmentEXT, _texture, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _commit); }
	#define glTexturePageCommitmentEXT glad_wrapped_glTexturePageCommitmentEXT
struct glad_tag_glMultiTexEnvfvEXT {};
inline void glad_wrapped_glMultiTexEnvfvEXT(GLenum _texunit, GLenum _target, GLenum _pname, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glMultiTexEnvfvEXT, _texunit, _target, _pname, _params); }
	#define glMultiTexEnvfvEXT glad_wrapped_glMultiTexEnvfvEXT
struct glad_tag_glNamedCopyBufferSubDataEXT {};
inline void glad_wrapped_glNamedCopyBufferSubDataEXT(GLuint _readBuffer, GLuint _writeBuffer, GLintptr _readOffset, GLintptr _writeOffset, GLsizeiptr _size) { GLAD_INSTRUMENT_CALL(glNamedCopyBufferSubDataEXT, _readBuffer, _writeBuffer, _readOffset, _writeOffset, _size); }
	#define glNamedCopyBufferSubDataEXT glad_wrapped_glNamedCopyBufferSubDataEXT
struct glad_tag_glNamedProgramLocalParameters4fvEXT {};
inline void glad_wrapped_glNamedProgramLocalParameters4fvEXT(GLuint _program, GLenum _target, GLuint _index, GLsizei _count, const GLfloat* _params) { GLAD_INSTRUMENT_CALL(glNamedProgramLocalParameters4fvEXT, _program, _target, _index, _count, _params); }
	#define glNamedProgramLocalParameters4fvEXT glad_wrapped_glNamedProgramLocalParameters4fvEXT
struct glad_tag_glGetMultiTexLevelParameterivEXT {};
inline void glad_wrapped_glGetMultiTexLevelParameterivEXT(GLenum _texunit, GLenum _target, GLint _level, GLenum _pname, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetMultiTexLevelParameterivEXT, _texunit, _target, _level, _pname, _params); }
	#define glGetMultiTexLevelParameterivEXT glad_wrapped_glGetMultiTexLevelParameterivEXT
struct glad_tag_glCompressedTextureSubImage2DEXT {};
inline void glad_wrapped_glCompressedTextureSubImage2DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLsizei _width, GLsizei _height, GLenum _format, GLsizei _imageSize, const void* _bits) { GLAD_INSTRUMENT_CALL(glCompressedTextureSubImage2DEXT, _texture, _target, _level, _xoffset, _yoffset, _width, _height, _format, _imageSize, _bits); }
	#define glCompressedTextureSubImage2DEXT glad_wrapped_glCompressedTextureSubImage2DEXT
struct glad_tag_glEnableIndexedEXT {};
inline void glad_wrapped_glEnableIndexedEXT(GLenum _target, GLuint _index) { GLAD_INSTRUMENT_CALL(glEnableIndexedEXT, _target, _index); }
	#define glEnableIndexedEXT glad_wrapped_glEnableIndexedEXT
struct glad_tag_glGetNamedProgramLocalParameterIivEXT {};
inline void glad_wrapped_glGetNamedProgramLocalParameterIivEXT(GLuint _program, GLenum _target, GLuint _index, GLint* _params) { GLAD_INSTRUMENT_CALL(glGetNamedProgramLocalParameterIivEXT, _program, _target, _index, _params); }
	#define glGetNamedProgramLocalParameterIivEXT glad_wrapped_glGetNamedProgramLocalParameterIivEXT
struct glad_tag_glProgramUniform3uiEXT {};
inline void glad_wrapped_glProgramUniform3uiEXT(GLuint _program, GLint _location, GLuint _v0, GLuint _v1, GLuint _v2) { GLAD_INSTRUMENT_CALL(glProgramUniform3uiEXT, _program, _location, _v0, _v1, _v2); }
	#define glProgramUniform3uiEXT glad_wrapped_glProgramUniform3uiEXT
struct glad_tag_glProgramUniformMatrix4x2dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix4x2dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix4x2dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix4x2dvEXT glad_wrapped_glProgramUniformMatrix4x2dvEXT
struct glad_tag_glVertexArrayVertexAttribIOffsetEXT {};
inline void glad_wrapped_glVertexArrayVertexAttribIOffsetEXT(GLuint _vaobj, GLuint _buffer, GLuint _index, GLint _size, GLenum _type, GLsizei _stride, GLintptr _offset) { GLAD_INSTRUMENT_CALL(glVertexArrayVertexAttribIOffsetEXT, _vaobj, _buffer, _index, _size, _type, _stride, _offset); }
	#define glVertexArrayVertexAttribIOffsetEXT glad_wrapped_glVertexArrayVertexAttribIOffsetEXT
struct glad_tag_glProgramUniform3fEXT {};
inline void glad_wrapped_glProgramUniform3fEXT(GLuint _program, GLint _location, GLfloat _v0, GLfloat _v1, GLfloat _v2) { GLAD_INSTRUMENT_CALL(glProgramUniform3fEXT, _program, _location, _v0, _v1, _v2); }
	#define glProgramUniform3fEXT glad_wrapped_glProgramUniform3fEXT
struct glad_tag_glProgramUniform3dvEXT {};
inline void glad_wrapped_glProgramUniform3dvEXT(GLuint _program, GLint _location, GLsizei _count, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform3dvEXT, _program, _location, _count, _value); }
	#define glProgramUniform3dvEXT glad_wrapped_glProgramUniform3dvEXT
struct glad_tag_glMultiTexImage2DEXT {};
inline void glad_wrapped_glMultiTexImage2DEXT(GLenum _texunit, GLenum _target, GLint _level, GLint _internalformat, GLsizei _width, GLsizei _height, GLint _border, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glMultiTexImage2DEXT, _texunit, _target, _level, _internalformat, _width, _height, _border, _format, _type, _pixels); }
	#define glMultiTexImage2DEXT glad_wrapped_glMultiTexImage2DEXT
struct glad_tag_glTextureRenderbufferEXT {};
inline void glad_wrapped_glTextureRenderbufferEXT(GLuint _texture, GLenum _target, GLuint _renderbuffer) { GLAD_INSTRUMENT_CALL(glTextureRenderbufferEXT, _texture, _target, _renderbuffer); }
	#define glTextureRenderbufferEXT glad_wrapped_glTextureRenderbufferEXT
struct glad_tag_glGetConvolutionFilter {};
inline void glad_wrapped_glGetConvolutionFilter(GLenum _target, GLenum _format, GLenum _type, void* _image) { GLAD_INSTRUMENT_CALL(glGetConvolutionFilter, _target, _format, _type, _image); }
	#define glGetConvolutionFilter glad_wrapped_glGetConvolutionFilter
struct glad_tag_glProgramUniform1fvEXT {};
inline void glad_wrapped_glProgramUniform1fvEXT(GLuint _program, GLint _location, GLsizei _count, const GLfloat* _value) { GLAD_INSTRUMENT_CALL(glProgramUniform1fvEXT, _program, _location, _count, _value); }
	#define glProgramUniform1fvEXT glad_wrapped_glProgramUniform1fvEXT
struct glad_tag_glProgramUniformMatrix3dvEXT {};
inline void glad_wrapped_glProgramUniformMatrix3dvEXT(GLuint _program, GLint _location, GLsizei _count, GLboolean _transpose, const GLdouble* _value) { GLAD_INSTRUMENT_CALL(glProgramUniformMatrix3dvEXT, _program, _location, _count, _transpose, _value); }
	#define glProgramUniformMatrix3dvEXT glad_wrapped_glProgramUniformMatrix3dvEXT
struct glad_tag_glSeparableFilter2D {};
inline void glad_wrapped_glSeparableFilter2D(GLenum _target, GLenum _internalformat, GLsizei _width, GLsizei _height, GLenum _format, GLenum _type, const void* _row, const void* _column) { GLAD_INSTRUMENT_CALL(glSeparableFilter2D, _target, _internalformat, _width, _height, _format, _type, _row, _column); }
	#define glSeparableFilter2D glad_wrapped_glSeparableFilter2D
struct glad_tag_glPushDebugGroupKHR {};
inline void glad_wrapped_glPushDebugGroupKHR(GLenum _source, GLuint _id, GLsizei _length, const GLchar* _message) { GLAD_INSTRUMENT_CALL(glPushDebugGroupKHR, _source, _id, _length, _message); }
	#define glPushDebugGroupKHR glad_wrapped_glPushDebugGroupKHR
struct glad_tag_glMakeNamedBufferResidentNV {};
inline void glad_wrapped_glMakeNamedBufferResidentNV(GLuint _buffer, GLenum _access) { GLAD_INSTRUMENT_CALL(glMakeNamedBufferResidentNV, _buffer, _access); }
	#define glMakeNamedBufferResidentNV glad_wrapped_glMakeNamedBufferResidentNV
struct glad_tag_glMultiTexParameterfEXT {};
inline void glad_wrapped_glMultiTexParameterfEXT(GLenum _texunit, GLenum _target, GLenum _pname, GLfloat _param) { GLAD_INSTRUMENT_CALL(glMultiTexParameterfEXT, _texunit, _target, _pname, _param); }
	#define glMultiTexParameterfEXT glad_wrapped_glMultiTexParameterfEXT
struct glad_tag_glGetVertexArrayIntegervEXT {};
inline void glad_wrapped_glGetVertexArrayIntegervEXT(GLuint _vaobj, GLenum _pname, GLint* _param) { GLAD_INSTRUMENT_CALL(glGetVertexArrayIntegervEXT, _vaobj, _pname, _param); }
	#define glGetVertexArrayIntegervEXT glad_wrapped_glGetVertexArrayIntegervEXT
struct glad_tag_glTextureSubImage3DEXT {};
inline void glad_wrapped_glTextureSubImage3DEXT(GLuint _texture, GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLenum _type, const void* _pixels) { GLAD_INSTRUMENT_CALL(glTextureSubImage3DEXT, _texture, _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _type, _pixels); }
	#define glTextureSubImage3DEXT glad_wrapped_glTextureSubImage3DEXT
struct glad_tag_glProgramUniform1uiEXT {};
inline void glad_wrapped_glProgramUniform1uiEXT(GLuint _program, GLint _location, GLuint _v0) { GLAD_INSTRUMENT_CALL(glProgramUniform1uiEXT, _program, _location, _v0); }
	#define glProgramUniform1uiEXT                           glad_wrapped_glProgramUniform1uiEXT
#else
	#define glMultiDrawArraysIndirectBindlessCountNV         glad_glMultiDrawArraysIndirectBindlessCountNV
	#define glVertexArrayEdgeFlagOffsetEXT                   glad_glVertexArrayEdgeFlagOffsetEXT
	#define glGetIntegerIndexedvEXT                          glad_glGetIntegerIndexedvEXT
	#define glCopyTextureImage1DEXT                          glad_glCopyTextureImage1DEXT
	#define glCompressedMultiTexSubImage1DEXT                glad_glCompressedMultiTexSubImage1DEXT
	#define glMultiTexGendEXT                                glad_glMultiTexGendEXT
	#define glTextureStorage3DEXT                            glad_glTextureStorage3DEXT
	#define glVertexArrayBindVertexBufferEXT                 glad_glVertexArrayBindVertexBufferEXT
	#define glDebugMessageCallbackKHR                        glad_glDebugMessageCallbackKHR
	#define glGetIntegerui64vNV                              glad_glGetIntegerui64vNV
	#define glMultiTexParameteriEXT                          glad_glMultiTexParameteriEXT
	#define glGetTextureHandleNV                             glad_glGetTextureHandleNV
	#define glProgramUniformMatrix3x2dvEXT                   glad_glProgramUniformMatrix3x2dvEXT
	#define glGetNamedProgramLocalParameterfvEXT             glad_glGetNamedProgramLocalParameterfvEXT
	#define glEnableVertexArrayAttribEXT                     glad_glEnableVertexArrayAttribEXT
	#define glProgramUniform2uivEXT                          glad_glProgramUniform2uivEXT
	#define glGetTextureParameterivEXT                       glad_glGetTextureParameterivEXT
	#define glCompressedMultiTexSubImage3DEXT                glad_glCompressedMultiTexSubImage3DEXT
	#define glVertexArrayVertexAttribIFormatEXT              glad_glVertexArrayVertexAttribIFormatEXT
	#define glProgramUniform4fEXT                            glad_glProgramUniform4fEXT
	#define glMakeImageHandleNonResidentNV                   glad_glMakeImageHandleNonResidentNV
	#define glProgramUniform3ivEXT                           glad_glProgramUniform3ivEXT
	#define glGetFramebufferParameterivEXT                   glad_glGetFramebufferParameterivEXT
	#define glNamedBufferStorageEXT                          glad_glNamedBufferStorageEXT
	#define glNamedFramebufferTexture3DEXT                   glad_glNamedFramebufferTexture3DEXT
	#define glGetSeparableFilter                             glad_glGetSeparableFilter
	#define glProgramUniform4iEXT                            glad_glProgramUniform4iEXT
	#define glGenerateTextureMipmapEXT                       glad_glGenerateTextureMipmapEXT
	#define glProgramUniformMatrix3fvEXT                     glad_glProgramUniformMatrix3fvEXT
	#define glEnableVertexArrayEXT                           glad_glEnableVertexArrayEXT
	#define glTextureParameterivEXT                          glad_glTextureParameterivEXT
	#define glMultiTexRenderbufferEXT                        glad_glMultiTexRenderbufferEXT
	#define glMultiTexParameterIivEXT                        glad_glMultiTexParameterIivEXT
	#define glMatrixPushEXT                                  glad_glMatrixPushEXT
	#define glDebugMessageInsertKHR                          glad_glDebugMessageInsertKHR
	#define glProgramUniform2fEXT                            glad_glProgramUniform2fEXT
	#define glTextureParameterfEXT                           glad_glTextureParameterfEXT
	#define glMultiDrawElementsIndirectBindlessNV            glad_glMultiDrawElementsIndirectBindlessNV
	#define glNamedFramebufferTextureLayerEXT                glad_glNamedFramebufferTextureLayerEXT
	#define glObjectPtrLabelKHR                              glad_glObjectPtrLabelKHR
	#define glVertexAttribFormatNV                           glad_glVertexAttribFormatNV
	#define glNamedBufferDataEXT                             glad_glNamedBufferDataEXT
	#define glGetFloatIndexedvEXT                            glad_glGetFloatIndexedvEXT
	#define glMatrixMultfEXT                                 glad_glMatrixMultfEXT
	#define glCompressedTextureSubImage3DEXT                 glad_glCompressedTextureSubImage3DEXT
	#define glGetTextureLevelParameterivEXT                  glad_glGetTextureLevelParameterivEXT
	#define glGetVertexArrayIntegeri_vEXT                    glad_glGetVertexArrayIntegeri_vEXT
	#define glGetMultiTexParameterivEXT                      glad_glGetMultiTexParameterivEXT
	#define glProgramUniformMatrix3x4fvEXT                   glad_glProgramUniformMatrix3x4fvEXT
	#define glProgramUniform3iEXT                            glad_glProgramUniform3iEXT
	#define glBindImageTextureEXT                            glad_glBindImageTextureEXT
	#define glNamedProgramLocalParameterI4ivEXT              glad_glNamedProgramLocalParameterI4ivEXT
	#define glGetMultiTexParameterIuivEXT                    glad_glGetMultiTexParameterIuivEXT
	#define glTextureStorage2DMultisampleEXT                 glad_glTextureStorage2DMultisampleEXT
	#define glMatrixOrthoEXT                                 glad_glMatrixOrthoEXT
	#define glMatrixMultTransposedEXT                        glad_glMatrixMultTransposedEXT
	#define glGetDoubleIndexedvEXT                           glad_glGetDoubleIndexedvEXT
	#define glEnableClientStateiEXT                          glad_glEnableClientStateiEXT
	#define glCompressedTextureImage3DEXT                    glad_glCompressedTextureImage3DEXT
	#define glColorSubTable                                  glad_glColorSubTable
	#define glMultiTexGenfvEXT                               glad_glMultiTexGenfvEXT
	#define glTexCoordFormatNV                               glad_glTexCoordFormatNV
	#define glMultiTexGenfEXT                                glad_glMultiTexGenfEXT
	#define glColorTableParameteriv                          glad_glColorTableParameteriv
	#define glDebugMessageControlKHR                         glad_glDebugMessageControlKHR
	#define glCheckNamedFramebufferStatusEXT                 glad_glCheckNamedFramebufferStatusEXT
	#define glConvolutionParameteriv                         glad_glConvolutionParameteriv
	#define glVertexArrayVertexAttribFormatEXT               glad_glVertexArrayVertexAttribFormatEXT
	#define glCopyMultiTexSubImage2DEXT                      glad_glCopyMultiTexSubImage2DEXT
	#define glProgramUniformMatrix3x4dvEXT                   glad_glProgramUniformMatrix3x4dvEXT
	#define glGetNamedBufferSubDataEXT                       glad_glGetNamedBufferSubDataEXT
	#define glConvolutionFilter2D                            glad_glConvolutionFilter2D
	#define glProgramUniformMatrix4x3dvEXT                   glad_glProgramUniformMatrix4x3dvEXT
	#define glMatrixRotatedEXT                               glad_glMatrixRotatedEXT
	#define glMultiTexGeniEXT                                glad_glMultiTexGeniEXT
	#define glMatrixTranslatefEXT                            glad_glMatrixTranslatefEXT
	#define glNamedProgramStringEXT                          glad_glNamedProgramStringEXT
	#define glGetMinmaxParameteriv                           glad_glGetMinmaxParameteriv
	#define glGetPointerv                                    glad_glGetPointerv
	#define glGetNamedBufferParameterivEXT                   glad_glGetNamedBufferParameterivEXT
	#define glMatrixScaledEXT                                glad_glMatrixScaledEXT
	#define glFlushMappedNamedBufferRangeEXT                 glad_glFlushMappedNamedBufferRangeEXT
	#define glGetColorTableParameterfv                       glad_glGetColorTableParameterfv
	#define glDisableClientStateIndexedEXT                   glad_glDisableClientStateIndexedEXT
	#define glMultiDrawArraysIndirectBindlessNV              glad_glMultiDrawArraysIndirectBindlessNV
	#define glUniformui64NV                                  glad_glUniformui64NV
	#define glProgramUniform1dvEXT                           glad_glProgramUniform1dvEXT
	#define glProgramUniformMatrix2x4dvEXT                   glad_glProgramUniformMatrix2x4dvEXT
	#define glProgramUniform2iEXT                            glad_glProgramUniform2iEXT
	#define glMatrixLoadfEXT                                 glad_glMatrixLoadfEXT
	#define glCompressedMultiTexImage2DEXT                   glad_glCompressedMultiTexImage2DEXT
	#define glGetConvolutionParameteriv                      glad_glGetConvolutionParameteriv
	#define glFramebufferReadBufferEXT                       glad_glFramebufferReadBufferEXT
	#define glMinmax                                         glad_glMinmax
	#define glTextureStorage2DEXT                            glad_glTextureStorage2DEXT
	#define glVertexArrayFogCoordOffsetEXT                   glad_glVertexArrayFogCoordOffsetEXT
	#define glProgramUniform4uiEXT                           glad_glProgramUniform4uiEXT
	#define glClientAttribDefaultEXT                         glad_glClientAttribDefaultEXT
	#define glVertexAttribIFormatNV                          glad_glVertexAttribIFormatNV
	#define glProgramUniform3uivEXT                          glad_glProgramUniform3uivEXT
	#define glMakeTextureHandleResidentNV                    glad_glMakeTextureHandleResidentNV
	#define glGetNamedBufferParameterui64vNV                 glad_glGetNamedBufferParameterui64vNV
	#define glGetBufferParameterui64vNV                      glad_glGetBufferParameterui64vNV
	#define glMakeNamedBufferNonResidentNV                   glad_glMakeNamedBufferNonResidentNV
	#define glUnmapNamedBufferEXT                            glad_glUnmapNamedBufferEXT
	#define glMakeBufferResidentNV                           glad_glMakeBufferResidentNV
	#define glGetFloati_vEXT                                 glad_glGetFloati_vEXT
	#define glBufferAddressRangeNV                           glad_glBufferAddressRangeNV
	#define glUniformui64vNV                                 glad_glUniformui64vNV
	#define glMultiTexEnvivEXT                               glad_glMultiTexEnvivEXT
	#define glCopyTextureImage2DEXT                          glad_glCopyTextureImage2DEXT
	#define glCompressedTextureImage2DEXT                    glad_glCompressedTextureImage2DEXT
	#define glProgramUniformui64vNV                          glad_glProgramUniformui64vNV
	#define glDepthBoundsdNV                                 glad_glDepthBoundsdNV
	#define glGetVertexArrayPointeri_vEXT                    glad_glGetVertexArrayPointeri_vEXT
	#define glNamedProgramLocalParameter4dEXT                glad_glNamedProgramLocalParameter4dEXT
	#define glGetMinmax                                      glad_glGetMinmax
	#define glFramebufferDrawBufferEXT                       glad_glFramebufferDrawBufferEXT
	#define glCopyColorSubTable                              glad_glCopyColorSubTable
	#define glCopyMultiTexSubImage3DEXT                      glad_glCopyMultiTexSubImage3DEXT
	#define glProgramUniformMatrix2fvEXT                     glad_glProgramUniformMatrix2fvEXT
	#define glGetTextureParameterIuivEXT                     glad_glGetTextureParameterIuivEXT
	#define glGetMultiTexEnvivEXT                            glad_glGetMultiTexEnvivEXT
	#define glProgramUniform1ivEXT                           glad_glProgramUniform1ivEXT
	#define glCompressedTextureSubImage1DEXT                 glad_glCompressedTextureSubImage1DEXT
	#define glResetMinmax                                    glad_glResetMinmax
	#define glCopyTextureSubImage2DEXT                       glad_glCopyTextureSubImage2DEXT
	#define glProgramUniform4dvEXT                           glad_glProgramUniform4dvEXT
	#define glGetHistogramParameterfv                        glad_glGetHistogramParameterfv
	#define glVertexFormatNV                                 glad_glVertexFormatNV
	#define glVertexArrayNormalOffsetEXT                     glad_glVertexArrayNormalOffsetEXT
	#define glProgramUniformMatrix2x4fvEXT                   glad_glProgramUniformMatrix2x4fvEXT
	#define glMatrixTranslatedEXT                            glad_glMatrixTranslatedEXT
	#define glNamedFramebufferRenderbufferEXT                glad_glNamedFramebufferRenderbufferEXT
	#define glVertexArrayTexCoordOffsetEXT                   glad_glVertexArrayTexCoordOffsetEXT
	#define glGetHistogram                                   glad_glGetHistogram
	#define glColorFormatNV                                  glad_glColorFormatNV
	#define glProgramUniformui64NV                           glad_glProgramUniformui64NV
	#define glProgramUniformMatrix4x2fvEXT                   glad_glProgramUniformMatrix4x2fvEXT
	#define glMatrixLoadTransposedEXT                        glad_glMatrixLoadTransposedEXT
	#define glGetPointeri_vEXT                               glad_glGetPointeri_vEXT
	#define glMatrixRotatefEXT                               glad_glMatrixRotatefEXT
	#define glMakeTextureHandleNonResidentNV                 glad_glMakeTextureHandleNonResidentNV
	#define glGetMultiTexEnvfvEXT                            glad_glGetMultiTexEnvfvEXT
	#define glProgramUniformMatrix4dvEXT                     glad_glProgramUniformMatrix4dvEXT
	#define glMultiTexSubImage2DEXT                          glad_glMultiTexSubImage2DEXT
	#define glProgramUniform1dEXT                            glad_glProgramUniform1dEXT
	#define glVertexArrayVertexAttribBindingEXT              glad_glVertexArrayVertexAttribBindingEXT
	#define glGetPointervKHR                                 glad_glGetPointervKHR
	#define glTextureParameteriEXT                           glad_glTextureParameteriEXT
	#define glClearNamedBufferDataEXT                        glad_glClearNamedBufferDataEXT
	#define glDisableIndexedEXT                              glad_glDisableIndexedEXT
	#define glGetTextureLevelParameterfvEXT                  glad_glGetTextureLevelParameterfvEXT
	#define glVertexArrayVertexAttribLOffsetEXT              glad_glVertexArrayVertexAttribLOffsetEXT
	#define glGetNamedBufferPointervEXT                      glad_glGetNamedBufferPointervEXT
	#define glPopDebugGroupKHR                               glad_glPopDebugGroupKHR
	#define glConvolutionFilter1D                            glad_glConvolutionFilter1D
	#define glMatrixMultTransposefEXT                        glad_glMatrixMultTransposefEXT
	#define glCopyTextureSubImage1DEXT                       glad_glCopyTextureSubImage1DEXT
	#define glTextureSubImage2DEXT                           glad_glTextureSubImage2DEXT
	#define glProgramUniformMatrix2x3dvEXT                   glad_glProgramUniformMatrix2x3dvEXT
	#define glTextureParameterfvEXT                          glad_glTextureParameterfvEXT
	#define glIsNamedBufferResidentNV                        glad_glIsNamedBufferResidentNV
	#define glCopyMultiTexSubImage1DEXT                      glad_glCopyMultiTexSubImage1DEXT
	#define glMatrixMultdEXT                                 glad_glMatrixMultdEXT
	#define glProgramUniformMatrix3x2fvEXT                   glad_glProgramUniformMatrix3x2fvEXT
	#define glObjectLabelKHR                                 glad_glObjectLabelKHR
	#define glMultiTexImage1DEXT                             glad_glMultiTexImage1DEXT
	#define glEdgeFlagFormatNV                               glad_glEdgeFlagFormatNV
	#define glNamedProgramLocalParameterI4uivEXT             glad_glNamedProgramLocalParameterI4uivEXT
	#define glGetMultiTexParameterIivEXT                     glad_glGetMultiTexParameterIivEXT
	#define glMemoryBarrierEXT                               glad_glMemoryBarrierEXT
	#define glGetMultiTexParameterfvEXT                      glad_glGetMultiTexParameterfvEXT
	#define glCopyConvolutionFilter1D                        glad_glCopyConvolutionFilter1D
	#define glProgramUniform4ivEXT                           glad_glProgramUniform4ivEXT
	#define glColorTableParameterfv                          glad_glColorTableParameterfv
	#define glCopyMultiTexImage1DEXT                         glad_glCopyMultiTexImage1DEXT
	#define glVertexArrayVertexOffsetEXT                     glad_glVertexArrayVertexOffsetEXT
	#define glNamedFramebufferParameteriEXT                  glad_glNamedFramebufferParameteriEXT
	#define glTextureBufferRangeEXT                          glad_glTextureBufferRangeEXT
	#define glGetConvolutionParameterfv                      glad_glGetConvolutionParameterfv
	#define glCompressedMultiTexSubImage2DEXT                glad_glCompressedMultiTexSubImage2DEXT
	#define glGetHistogramParameteriv                        glad_glGetHistogramParameteriv
	#define glTextureBufferEXT                               glad_glTextureBufferEXT
	#define glGetMultiTexImageEXT                            glad_glGetMultiTexImageEXT
	#define glGetPointerIndexedvEXT                          glad_glGetPointerIndexedvEXT
	#define glVertexArrayVertexBindingDivisorEXT             glad_glVertexArrayVertexBindingDivisorEXT
	#define glIsImageHandleResidentNV                        glad_glIsImageHandleResidentNV
	#define glProgramUniform2dvEXT                           glad_glProgramUniform2dvEXT
	#define glGetNamedFramebufferAttachmentParameterivEXT    glad_glGetNamedFramebufferAttachmentParameterivEXT
	#define glGetMultiTexGendvEXT                            glad_glGetMultiTexGendvEXT
	#define glClearNamedBufferSubDataEXT                     glad_glClearNamedBufferSubDataEXT
	#define glGetTextureImageEXT                             glad_glGetTextureImageEXT
	#define glPushClientAttribDefaultEXT                     glad_glPushClientAttribDefaultEXT
	#define glCopyColorTable                                 glad_glCopyColorTable
	#define glMultiTexParameterivEXT                         glad_glMultiTexParameterivEXT
	#define glMatrixLoadTransposefEXT                        glad_glMatrixLoadTransposefEXT
	#define glVertexArrayVertexAttribOffsetEXT               glad_glVertexArrayVertexAttribOffsetEXT
	#define glIndexFormatNV                                  glad_glIndexFormatNV
	#define glVertexArrayVertexAttribDivisorEXT              glad_glVertexArrayVertexAttribDivisorEXT
	#define glProgramUniformMatrix2dvEXT                     glad_glProgramUniformMatrix2dvEXT
	#define glMapNamedBufferEXT                              glad_glMapNamedBufferEXT
	#define glMatrixLoaddEXT                                 glad_glMatrixLoaddEXT
	#define glDisableVertexArrayAttribEXT                    glad_glDisableVertexArrayAttribEXT
	#define glNamedProgramLocalParametersI4ivEXT             glad_glNamedProgramLocalParametersI4ivEXT
	#define glGetTextureSamplerHandleNV                      glad_glGetTextureSamplerHandleNV
	#define glGetColorTableParameteriv                       glad_glGetColorTableParameteriv
	#define glGetColorTable                                  glad_glGetColorTable
	#define glVertexArrayColorOffsetEXT                      glad_glVertexArrayColorOffsetEXT
	#define glVertexArrayMultiTexCoordOffsetEXT              glad_glVertexArrayMultiTexCoordOffsetEXT
	#define glGetMultiTexGenivEXT                            glad_glGetMultiTexGenivEXT
	#define glClearDepthdNV                                  glad_glClearDepthdNV
	#define glBindMultiTextureEXT                            glad_glBindMultiTextureEXT
	#define glDisableVertexArrayEXT                          glad_glDisableVertexArrayEXT
	#define glFramebufferDrawBuffersEXT                      glad_glFramebufferDrawBuffersEXT
	#define glMapNamedBufferRangeEXT                         glad_glMapNamedBufferRangeEXT
	#define glNamedRenderbufferStorageMultisampleEXT         glad_glNamedRenderbufferStorageMultisampleEXT
	#define glConvolutionParameteri                          glad_glConvolutionParameteri
	#define glConvolutionParameterf                          glad_glConvolutionParameterf
	#define glProgramUniform2fvEXT                           glad_glProgramUniform2fvEXT
	#define glNamedProgramLocalParameterI4uiEXT              glad_glNamedProgramLocalParameterI4uiEXT
	#define glTextureParameterIuivEXT                        glad_glTextureParameterIuivEXT
	#define glProgramUniform1iEXT                            glad_glProgramUniform1iEXT
	#define glMultiTexSubImage3DEXT                          glad_glMultiTexSubImage3DEXT
	#define glProgramUniformHandleui64vNV                    glad_glProgramUniformHandleui64vNV
	#define glNamedFramebufferTextureFaceEXT                 glad_glNamedFramebufferTextureFaceEXT
	#define glStringMarkerGREMEDY                            glad_glStringMarkerGREMEDY
	#define glGetMinmaxParameterfv                           glad_glGetMinmaxParameterfv
	#define glProgramUniformHandleui64NV                     glad_glProgramUniformHandleui64NV
	#define glCopyConvolutionFilter2D                        glad_glCopyConvolutionFilter2D
	#define glGetDoublei_vEXT                                glad_glGetDoublei_vEXT
	#define glVertexArrayIndexOffsetEXT                      glad_glVertexArrayIndexOffsetEXT
	#define glDisableClientStateiEXT                         glad_glDisableClientStateiEXT
	#define glMultiTexImage3DEXT                             glad_glMultiTexImage3DEXT
	#define glNamedRenderbufferStorageMultisampleCoverageEXT glad_glNamedRenderbufferStorageMultisampleCoverageEXT
	#define glMultiTexBufferEXT                              glad_glMultiTexBufferEXT
	#define glMultiDrawElementsIndirectBindlessCountNV       glad_glMultiDrawElementsIndirectBindlessCountNV
	#define glMultiTexParameterIuivEXT                       glad_glMultiTexParameterIuivEXT
	#define glProgramUniform2dEXT                            glad_glProgramUniform2dEXT
	#define glGetCompressedTextureImageEXT                   glad_glGetCompressedTextureImageEXT
	#define glGetIntegerui64i_vNV                            glad_glGetIntegerui64i_vNV
	#define glUniformHandleui64NV                            glad_glUniformHandleui64NV
	#define glGetUniformui64vNV                              glad_glGetUniformui64vNV
	#define glNamedRenderbufferStorageEXT                    glad_glNamedRenderbufferStorageEXT
	#define glTextureStorage1DEXT                            glad_glTextureStorage1DEXT
	#define glNamedBufferSubDataEXT                          glad_glNamedBufferSubDataEXT
	#define glNormalFormatNV                                 glad_glNormalFormatNV
	#define glIsBufferResidentNV                             glad_glIsBufferResidentNV
	#define glHistogram                                      glad_glHistogram
	#define glGetNamedRenderbufferParameterivEXT             glad_glGetNamedRenderbufferParameterivEXT
	#define glCompressedTextureImage1DEXT                    glad_glCompressedTextureImage1DEXT
	#define glProgramUniformMatrix4x3fvEXT                   glad_glProgramUniformMatrix4x3fvEXT
	#define glGetObjectLabelKHR                              glad_glGetObjectLabelKHR
	#define glProgramUniformMatrix4fvEXT                     glad_glProgramUniformMatrix4fvEXT
	#define glMultiTexGendvEXT                               glad_glMultiTexGendvEXT
	#define glIsTextureHandleResidentNV                      glad_glIsTextureHandleResidentNV
	#define glNamedFramebufferTextureEXT                     glad_glNamedFramebufferTextureEXT
	#define glNamedProgramLocalParameter4fEXT                glad_glNamedProgramLocalParameter4fEXT
	#define glSecondaryColorFormatNV                         glad_glSecondaryColorFormatNV
	#define glMatrixPopEXT                                   glad_glMatrixPopEXT
	#define glMultiTexEnviEXT                                glad_glMultiTexEnviEXT
	#define glProgramUniform1uivEXT                          glad_glProgramUniform1uivEXT
	#define glVertexArrayVertexAttribLFormatEXT              glad_glVertexArrayVertexAttribLFormatEXT
	#define glCopyTextureSubImage3DEXT                       glad_glCopyTextureSubImage3DEXT
	#define glGetNamedFramebufferParameterivEXT              glad_glGetNamedFramebufferParameterivEXT
	#define glProgramUniform4uivEXT                          glad_glProgramUniform4uivEXT
	#define glMakeBufferNonResidentNV                        glad_glMakeBufferNonResidentNV
	#define glGetNamedProgramLocalParameterdvEXT             glad_glGetNamedProgramLocalParameterdvEXT
	#define glUniformHandleui64vNV                           glad_glUniformHandleui64vNV
	#define glGetNamedProgramStringEXT                       glad_glGetNamedProgramStringEXT
	#define glFogCoordFormatNV                               glad_glFogCoordFormatNV
	#define glGetMultiTexGenfvEXT                            glad_glGetMultiTexGenfvEXT
	#define glGetDebugMessageLogKHR                          glad_glGetDebugMessageLogKHR
	#define glConvolutionParameterfv                         glad_glConvolutionParameterfv
	#define glMatrixFrustumEXT                               glad_glMatrixFrustumEXT
	#define glProgramUniform2uiEXT                           glad_glProgramUniform2uiEXT
	#define glCompressedMultiTexImage3DEXT                   glad_glCompressedMultiTexImage3DEXT
	#define glProgramUniformMatrix2x3fvEXT                   glad_glProgramUniformMatrix2x3fvEXT
	#define glProgramUniform1fEXT                            glad_glProgramUniform1fEXT
	#define glProgramUniform3dEXT                            glad_glProgramUniform3dEXT
	#define glTextureImage3DEXT                              glad_glTextureImage3DEXT
	#define glGetMultiTexLevelParameterfvEXT                 glad_glGetMultiTexLevelParameterfvEXT
	#define glIsEnabledIndexedEXT                            glad_glIsEnabledIndexedEXT
	#define glGetNamedProgramivEXT                           glad_glGetNamedProgramivEXT
	#define glProgramUniform3fvEXT                           glad_glProgramUniform3fvEXT
	#define glVertexArraySecondaryColorOffsetEXT             glad_glVertexArraySecondaryColorOffsetEXT
	#define glGetNamedProgramLocalParameterIuivEXT           glad_glGetNamedProgramLocalParameterIuivEXT
	#define glGetVertexArrayPointervEXT                      glad_glGetVertexArrayPointervEXT
	#define glEnableClientStateIndexedEXT                    glad_glEnableClientStateIndexedEXT
	#define glMatrixLoadIdentityEXT                          glad_glMatrixLoadIdentityEXT
	#define glGetObjectPtrLabelKHR                           glad_glGetObjectPtrLabelKHR
	#define glNamedProgramLocalParameterI4iEXT               glad_glNamedProgramLocalParameterI4iEXT
	#define glMultiTexGenivEXT                               glad_glMultiTexGenivEXT
	#define glNamedFramebufferTexture2DEXT                   glad_glNamedFramebufferTexture2DEXT
	#define glTextureImage2DEXT                              glad_glTextureImage2DEXT
	#define glTextureStorage3DMultisampleEXT                 glad_glTextureStorage3DMultisampleEXT
	#define glResetHistogram                                 glad_glResetHistogram
	#define glDepthRangedNV                                  glad_glDepthRangedNV
	#define glCompressedMultiTexImage1DEXT                   glad_glCompressedMultiTexImage1DEXT
	#define glProgramUniform2ivEXT                           glad_glProgramUniform2ivEXT
	#define glProgramUniform4fvEXT                           glad_glProgramUniform4fvEXT
	#define glMultiTexCoordPointerEXT                        glad_glMultiTexCoordPointerEXT
	#define glGenerateMultiTexMipmapEXT                      glad_glGenerateMultiTexMipmapEXT
	#define glNamedFramebufferTexture1DEXT                   glad_glNamedFramebufferTexture1DEXT
	#define glMultiTexEnvfEXT                                glad_glMultiTexEnvfEXT
	#define glGetTextureParameterIivEXT                      glad_glGetTextureParameterIivEXT
	#define glNamedProgramLocalParametersI4uivEXT            glad_glNamedProgramLocalParametersI4uivEXT
	#define glGetBooleanIndexedvEXT                          glad_glGetBooleanIndexedvEXT
	#define glTextureSubImage1DEXT                           glad_glTextureSubImage1DEXT
	#define glCopyMultiTexImage2DEXT                         glad_glCopyMultiTexImage2DEXT
	#define glNamedProgramLocalParameter4fvEXT               glad_glNamedProgramLocalParameter4fvEXT
	#define glGetCompressedMultiTexImageEXT                  glad_glGetCompressedMultiTexImageEXT
	#define glColorTable                                     glad_glColorTable
	#define glMultiTexSubImage1DEXT                          glad_glMultiTexSubImage1DEXT
	#define glGetImageHandleNV                               glad_glGetImageHandleNV
	#define glProgramUniform4dEXT                            glad_glProgramUniform4dEXT
	#define glTextureParameterIivEXT                         glad_glTextureParameterIivEXT
	#define glMultiTexParameterfvEXT                         glad_glMultiTexParameterfvEXT
	#define glMakeImageHandleResidentNV                      glad_glMakeImageHandleResidentNV
	#define glNamedProgramLocalParameter4dvEXT               glad_glNamedProgramLocalParameter4dvEXT
	#define glTextureImage1DEXT                              glad_glTextureImage1DEXT
	#define glMatrixScalefEXT                                glad_glMatrixScalefEXT
	#define glGetTextureParameterfvEXT                       glad_glGetTextureParameterfvEXT
	#define glTexturePageCommitmentEXT                       glad_glTexturePageCommitmentEXT
	#define glMultiTexEnvfvEXT                               glad_glMultiTexEnvfvEXT
	#define glNamedCopyBufferSubDataEXT                      glad_glNamedCopyBufferSubDataEXT
	#define glNamedProgramLocalParameters4fvEXT              glad_glNamedProgramLocalParameters4fvEXT
	#define glGetMultiTexLevelParameterivEXT                 glad_glGetMultiTexLevelParameterivEXT
	#define glCompressedTextureSubImage2DEXT                 glad_glCompressedTextureSubImage2DEXT
	#define glEnableIndexedEXT                               glad_glEnableIndexedEXT
	#define glGetNamedProgramLocalParameterIivEXT            glad_glGetNamedProgramLocalParameterIivEXT
	#define glProgramUniform3uiEXT                           glad_glProgramUniform3uiEXT
	#define glProgramUniformMatrix4x2dvEXT                   glad_glProgramUniformMatrix4x2dvEXT
	#define glVertexArrayVertexAttribIOffsetEXT              glad_glVertexArrayVertexAttribIOffsetEXT
	#define glProgramUniform3fEXT                            glad_glProgramUniform3fEXT
	#define glProgramUniform3dvEXT                           glad_glProgramUniform3dvEXT
	#define glMultiTexImage2DEXT                             glad_glMultiTexImage2DEXT
	#define glTextureRenderbufferEXT                         glad_glTextureRenderbufferEXT
	#define glGetConvolutionFilter                           glad_glGetConvolutionFilter
	#define glProgramUniform1fvEXT                           glad_glProgramUniform1fvEXT
	#define glProgramUniformMatrix3dvEXT                     glad_glProgramUniformMatrix3dvEXT
	#define glSeparableFilter2D                              glad_glSeparableFilter2D
	#define glPushDebugGroupKHR                              glad_glPushDebugGroupKHR
	#define glMakeNamedBufferResidentNV                      glad_glMakeNamedBufferResidentNV
	#define glMultiTexParameterfEXT                          glad_glMultiTexParameterfEXT
	#define glGetVertexArrayIntegervEXT                      glad_glGetVertexArrayIntegervEXT
	#define glTextureSubImage3DEXT                           glad_glTextureSubImage3DEXT
	#define glProgramUniform1uiEXT                           glad_glProgramUniform1uiEXT
#endif

#ifdef __cplusplus
}
#endif

#endif
