// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLResource.hpp
//  Version:     v1.00
//  Created:     27/04/2015 by Valerio Guagliumi.
//  Description: OpenGL feature detection and emulation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLFEATURES__
#define __GLFEATURES__

#if DXGL_TRACE_CALLS || DXGL_CHECK_ERRORS
	#include "GLInstrument.hpp"
#else
	#define DXGL_UNWRAPPED_FUNCTION(_Name) _Name
#endif

#if defined(DXGL_USE_GLEW)
	#include <GL/glew.h>
	#define DXGL_EXTENSION_LOADER 1
	#define DXGL_GL_EXTENSION_SUPPORTED(_Extension) (GLEW_ ## _Extension == GL_TRUE)
	#if CRY_PLATFORM_WINDOWS
		#include <GL/wglew.h>
		#define DXGL_WGL_EXTENSION_SUPPORTED(_Extension) (WGLEW_ ## _Extension == GL_TRUE)
	#endif // CRY_PLATFORM_WINDOWS
#elif defined(DXGL_USE_GLAD)
	#define DXGL_EXTENSION_LOADER 1
	#if DXGLES && !defined(DXGL_ES_SUBSET)
		#include "../Specification/glad_gles2.h"
	#else
		#include "../Specification/glad_gl.h"
	#endif
	#define DXGL_GL_EXTENSION_SUPPORTED(_Extension) (GLAD_GL_ ## _Extension == 1)
	#if CRY_PLATFORM_WINDOWS
		#include "../Specification/glad_wgl.h"
		#define DXGL_WGL_EXTENSION_SUPPORTED(_Extension) (GLAD_WGL_ ## _Extension == 1)
	#elif CRY_PLATFORM_ANDROID
		#include "../Specification/glad_egl.h"
	#elif CRY_PLATFORM_LINUX
		#include "../Specification/glad_glx.h"
	#endif
#else
	#define DXGL_EXTENSION_LOADER 0
	#define DXGL_GL_EXTENSION_SUPPORTED(_Extension) (false)
	#if CRY_PLATFORM_MAC
		#define GL3_PROTOTYPES
		#include <OpenGL/gl3.h>
		#include <OpenGL/gl3ext.h>
	#elif CRY_PLATFORM_IOS
		#include <OpenGLES/ES3/gl.h>
		#include <OpenGLES/ES3/glext.h>
	#endif
#endif

#define DXGL_SUPPORT_INDEXED_BOOL_STATE            (DXGL_REQUIRED_VERSION >= DXGL_VERSION_32 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_INDEXED_ENABLE_STATE          !DXGLES
#define DXGL_SUPPORT_INDEXED_FLOAT_STATE           !DXGLES
#define DXGL_SUPPORT_VIEWPORT_ARRAY                !DXGLES
#define DXGL_SUPPORT_SCISSOR_RECT_ARRAY            !DXGLES
#define DXGL_SUPPORT_INDEPENDENT_BLEND_STATES      !DXGLES
#define DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX         (DXGL_REQUIRED_VERSION >= DXGL_VERSION_42 && !DXGLES)
#define DXGL_SUPPORT_GETTEXIMAGE                   !DXGLES
#define DXGL_SUPPORT_DEPTH_CLAMP                   !DXGLES
#define DXGL_SUPPORT_TIMER_QUERIES                 !DXGLES
#define DXGL_SUPPORT_S3TC_TEXTURES                 (!DXGLES || DXGL_SUPPORT_TEGRA)
#define DXGL_SUPPORT_BGRA_TEXTURES                 !DXGLES
#define DXGL_SUPPORT_STENCIL_ONLY_FORMAT           (DXGL_REQUIRED_VERSION >= DXGL_VERSION_32 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_MULTISAMPLING                 (DXGL_REQUIRED_VERSION >= DXGL_VERSION_41 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_MULTISAMPLED_TEXTURES         (DXGL_REQUIRED_VERSION >= DXGL_VERSION_41 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_TESSELLATION                  (DXGL_REQUIRED_VERSION >= DXGL_VERSION_41)
#define DXGL_SUPPORT_GEOMETRY_SHADERS              (DXGL_REQUIRED_VERSION >= DXGL_VERSION_41)
#define DXGL_SUPPORT_TEXTURE_BUFFERS               (DXGL_REQUIRED_VERSION >= DXGL_VERSION_41)
#define DXGL_SUPPORT_CUBEMAP_ARRAYS                 (DXGL_REQUIRED_VERSION >= DXGL_VERSION_41 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_DRAW_INDIRECT                 (DXGL_REQUIRED_VERSION >= DXGL_VERSION_41 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_BPTC                          (DXGL_REQUIRED_VERSION >= DXGL_VERSION_42)
#define DXGL_SUPPORT_STENCIL_TEXTURES              ((DXGL_REQUIRED_VERSION >= DXGL_VERSION_42 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31) && !DXGL_SUPPORT_NSIGHT_SINCE(4_1))
#define DXGL_SUPPORT_ATOMIC_COUNTERS               (DXGL_REQUIRED_VERSION >= DXGL_VERSION_42 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_COMPUTE                       (!CRY_PLATFORM_MAC)
#define DXGL_SUPPORT_DISPATCH_INDIRECT             (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_SHADER_STORAGE_BLOCKS         (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43)
#define DXGL_SUPPORT_SHADER_IMAGES                 (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_VERTEX_ATTRIB_BINDING         (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43)
#define DXGL_SUPPORT_TEXTURE_VIEWS                 (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
#define DXGL_SUPPORT_QUERY_INTERNAL_FORMAT_SUPPORT (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43 && !DXGL_SUPPORT_NSIGHT_SINCE(3_0) && !DXGL_SUPPORT_VOGL)
#define DXGL_SUPPORT_DEBUG_OUTPUT                  (DXGLES_REQUIRED_VERSION >= DXGL_VERSION_32) && (!DXGL_SUPPORT_NSIGHT_SINCE(4_1) && !CRY_PLATFORM_MAC)
#define DXGL_SUPPORT_COPY_IMAGE                    (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43 && !DXGL_SUPPORT_NSIGHT_SINCE(4_1))
#define DXGL_SUPPORT_BUFFER_STORAGE                (DXGL_REQUIRED_VERSION >= DXGL_VERSION_44 && !DXGL_SUPPORT_NSIGHT_SINCE(4_5))
#define DXGL_SUPPORT_ETC2_EAC_COMPRESSION          (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_30)

#if DXGL_SUPPORT_GEOMETRY_SHADERS
	#define DXGL_ENABLE_GEOMETRY_SHADERS 1
#else
	#define DXGL_ENABLE_GEOMETRY_SHADERS 0
#endif
#if DXGL_SUPPORT_TESSELLATION
	#define DXGL_ENABLE_TESSELLATION_SHADERS 1
#else
	#define DXGL_ENABLE_TESSELLATION_SHADERS 0
#endif
#if DXGL_SUPPORT_COMPUTE
	#define DXGL_ENABLE_COMPUTE_SHADERS 1
#else
	#define DXGL_ENABLE_COMPUTE_SHADERS 0
#endif

namespace NCryOpenGL
{

inline bool SupportTextureViews()
{
#if DXGL_SUPPORT_NSIGHT_SINCE(4_1) || DXGL_SUPPORT_VOGL
	return false;
#elif DXGL_REQUIRED_VERSION >= DXGL_VERSION_42
	return true;
#elif DXGLES
	return DXGL_GL_EXTENSION_SUPPORTED(EXT_texture_view);
#else
	return DXGL_GL_EXTENSION_SUPPORTED(ARB_texture_view);
#endif
}

inline bool SupportSeparablePrograms()
{
#if DXGL_GLSL_FROM_HLSLCROSSCOMPILER
	DXGL_TODO(
	  "At the moment HLSLcc does guarantee exact interface matching between programs. "
	  "This can lead to pipelines that fail validation or worse with undefined behavior because they have "
	  "in one stage user-defined output variables that are ignored by the following stage."
	  "Investigate if this can be fixed.")
	return false;
#elif DXGL_REQUIRED_VERSION >= DXGL_VERSION_41 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31
	return true;
#elif DXGLES
	return DXGL_GL_EXTENSION_SUPPORTED(EXT_separate_shader_objects);
#else
	return DXGL_GL_EXTENSION_SUPPORTED(ARB_separate_shader_objects);
#endif
}

inline bool SupportTextureBorderClamp()
{
#if DXGL_REQUIRED_VERSION > 0 || defined(DXGL_ES_SUBSET)
	return true;
#elif DXGLES
	return DXGL_GL_EXTENSION_SUPPORTED(EXT_texture_border_clamp);
#else
	return DXGL_GL_EXTENSION_SUPPORTED(ARB_texture_border_clamp);
#endif
}

inline bool SupportBufferStorage()
{
#if DXGL_REQUIRED_VERSION >= DXGL_VERSION_44
	return true;
#elif DXGLES
	return false;
#else
	return DXGL_GL_EXTENSION_SUPPORTED(GL_ARB_buffer_storage);
#endif
}

inline bool SupportVertexAttribBinding()
{
	DXGL_TODO("Workaround for the NVIDIA 331.113 x64 linux driver crash - investigate");
	return false;

#if DXGL_REQUIRED_VERSION >= DXGL_VERSION_43
	return true;
#elif DXGLES
	return false;
#else
	return DXGL_GL_EXTENSION_SUPPORTED(GL_ARB_vertex_attrib_binding);
#endif
}

inline bool SupportMultiBind()
{
	DXGL_TODO("Workaround for the multi-threaded GL driver crash - investigate");
	return false;

#if DXGL_REQUIRED_VERSION >= DXGL_VERSION_44
	return true;
#elif DXGLES
	return false;
#else
	return DXGL_GL_EXTENSION_SUPPORTED(GL_ARB_multi_bind);
#endif
}

inline bool SupportDebugOutput()
{
#if DXGL_REQUIRED_VERSION >= DXGL_VERSION_43
	return true;
#else
	return DXGL_GL_EXTENSION_SUPPORTED(KHR_debug);
#endif
}

inline bool SupportCompute()
{
#if (DXGL_REQUIRED_VERSION >= DXGL_VERSION_43 || DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31)
	return true;
#else
	return DXGL_GL_EXTENSION_SUPPORTED(ARB_compute_shader);
#endif
}

}

enum
{
#if DXGL_SUPPORT_VIEWPORT_ARRAY
	DXGL_NUM_SUPPORTED_VIEWPORTS = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE,
#else
	DXGL_NUM_SUPPORTED_VIEWPORTS = 1,
#endif
#if DXGL_SUPPORT_SCISSOR_RECT_ARRAY
	DXGL_NUM_SUPPORTED_SCISSOR_RECTS = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE,
#else
	DXGL_NUM_SUPPORTED_SCISSOR_RECTS = 1,
#endif
#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
	DXGL_NUM_SUPPORTED_BLEND_STATES = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
#else
	DXGL_NUM_SUPPORTED_BLEND_STATES = 1,
#endif
};

////////////////////////////////////////////////////////////////////////////
// Conform extension definitions
////////////////////////////////////////////////////////////////////////////

#if DXGL_SUPPORT_S3TC_TEXTURES
	#if !defined(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT) || \
	  !defined(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT) ||   \
	  !defined(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
		#undef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
		#undef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
		#undef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
		#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV
		#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV
		#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV
	#endif
#endif
#if DXGL_SUPPORT_BGRA_TEXTURES
	#if !defined(GL_BGRA)
		#if defined(GL_BGRA_EXT)
			#define GL_BGRA GL_BGRA_EXT
		#else
			#define GL_BGRA GL_BGRA_IMG
		#endif
	#endif
#endif
#if DXGL_REQUIRED_VERSION > 0 || defined(DXGL_ES_SUBSET)
	#define GL_CLAMP_TO_BORDER_EXT      GL_CLAMP_TO_BORDER
	#define GL_TEXTURE_BORDER_COLOR_EXT GL_TEXTURE_BORDER_COLOR
	#define glTextureViewEXT            glTextureView
#endif

#if !defined(GL_COPY_READ_BUFFER_BINDING) || \
  !defined(GL_COPY_WRITE_BUFFER_BINDING)
	#undef GL_COPY_READ_BUFFER_BINDING
	#undef GL_COPY_WRITE_BUFFER_BINDING
	#define GL_COPY_READ_BUFFER_BINDING  GL_COPY_READ_BUFFER
	#define GL_COPY_WRITE_BUFFER_BINDING GL_COPY_WRITE_BUFFER
#endif

#if DXGLES
	#undef GL_TEXTURE_1D
	#undef GL_TEXTURE_1D_ARRAY
// Emulate 1D textures as 2D textures on OpenGL ES
	#define GL_TEXTURE_1D       GL_TEXTURE_2D
	#define GL_TEXTURE_1D_ARRAY GL_TEXTURE_2D_ARRAY
#endif //DXGLES

#if DXGL_SUPPORT_NSIGHT_SINCE(3_0) || DXGL_SUPPORT_VOGL || CRY_PLATFORM_MAC
	#undef glClearBufferData
	#undef glClearBufferSubData
	#undef glClearNamedBufferDataEXT
	#undef glClearNamedBufferSubDataEXT
	#undef glDispatchCompute
	#undef glDispatchComputeIndirect
	#undef glFramebufferParameteri
	#undef glGetFramebufferParameteriv
	#undef glGetNamedFramebufferParameterivEXT
	#undef glNamedFramebufferParameteriEXT
	#undef glInvalidateBufferData
	#undef glInvalidateBufferSubData
	#undef glInvalidateFramebuffer
	#undef glInvalidateSubFramebuffer
	#undef glInvalidateTexImage
	#undef glInvalidateTexSubImage
	#undef glMultiDrawArraysIndirect
	#undef glMultiDrawElementsIndirect
	#undef glTexStorage2DMultisample
	#undef glTexStorage3DMultisample
	#undef glBindVertexBuffer
	#undef glVertexAttribBinding
	#undef glVertexAttribFormat
	#undef glVertexAttribIFormat
	#undef glVertexAttribLFormat
	#undef glVertexBindingDivisor
	#undef glTextureImage2DEXT
	#undef glCompressedTextureImage2DEXT
	#undef glCompressedTextureSubImage2DEXT
	#undef glNamedFramebufferTextureEXT
	#undef glNamedFramebufferTextureLayerEXT
	#undef glCheckNamedFramebufferStatusEXT
	#undef glGetNamedFramebufferAttachmentParameterivEXT
	#undef glGetTextureLevelParameterivEXT
#endif //DXGL_SUPPORT_NSIGHT_SINCE(3_0) || DXGL_SUPPORT_VOGL || CRY_PLATFORM_MAC

#if DXGL_SUPPORT_NSIGHT_SINCE(3_0) || CRY_PLATFORM_MAC
	#undef glGetProgramInterfaceiv
	#undef glGetProgramResourceIndex
	#undef glGetProgramResourceLocation
	#undef glGetProgramResourceLocationIndex
	#undef glGetProgramResourceName
	#undef glGetProgramResourceiv
	#undef glShaderStorageBlockBinding
#endif //DXGL_SUPPORT_NSIGHT_SINCE(3_0) || CRY_PLATFORM_MAC

#if DXGL_SUPPORT_NSIGHT_SINCE(4_1) || CRY_PLATFORM_MAC
	#undef glDebugMessageCallback
	#undef glDebugMessageInsert
	#undef glGetDebugMessageLog
	#undef glGetObjectLabel
	#undef glGetObjectPtrLabel
	#undef glTextureSubImage2DEXT
#endif // DXGL_SUPPORT_NSIGHT_SINCE(4_1)

#if DXGL_SUPPORT_NSIGHT_SINCE(4_1) || DXGL_SUPPORT_VOGL || CRY_PLATFORM_MAC
	#undef glDebugMessageControl
	#undef glPushDebugGroup
	#undef glNamedBufferDataEXT
	#undef glNamedBufferSubDataEXT
	#undef glMapNamedBufferEXT
	#undef glMapNamedBufferRangeEXT
	#undef glUnmapNamedBufferEXT
	#undef glTexBufferRange
	#undef glTextureBufferRangeEXT
#endif // DXGL_SUPPORT_NSIGHT_SINCE(4_1) || DXGL_SUPPORT_VOGL || CRY_PLATFORM_MAC

#if DXGL_SUPPORT_NSIGHT_SINCE(4_5)
	#undef glNamedBufferStorageEXT
#endif

#if DXGL_SUPPORT_VOGL
	#undef glReleaseShaderCompiler
#endif //DXGL_SUPPORT_VOGL

////////////////////////////////////////////////////////////////////////////
// Direct state access framebuffer attachment emulation when not available
////////////////////////////////////////////////////////////////////////////

#if !defined(glNamedFramebufferTextureEXT) ||    \
  !defined(glNamedFramebufferTextureLayerEXT) || \
  !defined(glCheckNamedFramebufferStatusEXT) ||  \
  !defined(glFramebufferDrawBuffersEXT) ||       \
  !defined(glGetNamedFramebufferAttachmentParameterivEXT)
	#undef glNamedFramebufferTextureEXT
	#undef glNamedFramebufferTextureLayerEXT
	#undef glCheckNamedFramebufferStatusEXT
	#undef glFramebufferDrawBuffersEXT
	#undef glGetNamedFramebufferAttachmentParameterivEXT

struct SAutoBindFrameBuffer
{
	SAutoBindFrameBuffer(GLenum eTarget, GLuint uFrameBuffer)
		: m_eTarget(eTarget)
	{
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_iPrevFrameBuffer);
		glBindFramebuffer(m_eTarget, uFrameBuffer);
	}

	~SAutoBindFrameBuffer()
	{
		glBindFramebuffer(m_eTarget, m_iPrevFrameBuffer);
	}

	GLint m_eTarget;
	GLint m_iPrevFrameBuffer;
};

inline void glNamedFramebufferTextureEXT(GLuint uFramebuffer, GLenum eAttachment, GLuint uTexture, GLint iLevel)
{
	SAutoBindFrameBuffer kAutoBind(GL_DRAW_FRAMEBUFFER, uFramebuffer);
	#if DXGL_REQUIRED_VERSION >= DXGL_VERSION_41
	glFramebufferTexture(GL_DRAW_FRAMEBUFFER, eAttachment, uTexture, iLevel);
	#else
	// Note ES can only bind a cubemap face or a 2D texture
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, eAttachment, GL_TEXTURE_2D, uTexture, iLevel);
	#endif
}

inline void glNamedFramebufferTextureLayerEXT(GLuint uFramebuffer, GLenum eAttachment, GLuint uTexture, GLint iLevel, GLint iLayer)
{
	SAutoBindFrameBuffer kAutoBind(GL_DRAW_FRAMEBUFFER, uFramebuffer);
	glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, eAttachment, uTexture, iLevel, iLayer);
}

inline GLenum glCheckNamedFramebufferStatusEXT(GLuint uFramebuffer, GLenum eTarget)
{
	SAutoBindFrameBuffer kAutoBind(eTarget, uFramebuffer);
	return glCheckFramebufferStatus(eTarget);
}

inline void glFramebufferDrawBuffersEXT(GLuint uFramebuffer, GLsizei uNumber, const GLenum* aeBuffers)
{
	SAutoBindFrameBuffer kAutoBind(GL_DRAW_FRAMEBUFFER, uFramebuffer);
	glDrawBuffers(uNumber, aeBuffers);
}

inline void glGetNamedFramebufferAttachmentParameterivEXT(GLuint uFramebuffer, GLenum eAttachment, GLenum ePName, GLint* aiParams)
{
	SAutoBindFrameBuffer kAutoBind(GL_DRAW_FRAMEBUFFER, uFramebuffer);
	glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, eAttachment, ePName, aiParams);
}

#endif

////////////////////////////////////////////////////////////////////////////
// Direct state access texture functions emulation when not available
////////////////////////////////////////////////////////////////////////////

#if !defined(glTextureStorage1DEXT) ||          \
  !defined(glTextureStorage2DEXT) ||            \
  !defined(glTextureStorage3DEXT) ||            \
  !defined(glTextureImage1DEXT) ||              \
  !defined(glTextureImage2DEXT) ||              \
  !defined(glTextureImage3DEXT) ||              \
  !defined(glCompressedTextureImage1DEXT) ||    \
  !defined(glCompressedTextureImage2DEXT) ||    \
  !defined(glCompressedTextureImage3DEXT) ||    \
  !defined(glTextureSubImage1DEXT) ||           \
  !defined(glTextureSubImage2DEXT) ||           \
  !defined(glTextureSubImage3DEXT) ||           \
  !defined(glCompressedTextureSubImage1DEXT) || \
  !defined(glCompressedTextureSubImage2DEXT) || \
  !defined(glCompressedTextureSubImage3DEXT) || \
  !defined(glGetTextureImageEXT) ||             \
  !defined(glGetCompressedTextureImageEXT) ||   \
  !defined(glGenerateTextureMipmapEXT) ||       \
  !defined(glTextureBufferEXT) ||               \
  !defined(glTextureBufferRangeEXT) ||          \
  !defined(glTextureParameterfEXT) ||           \
  !defined(glTextureParameteriEXT) ||           \
  !defined(glTextureParameterfvEXT) ||          \
  !defined(glTextureParameterivEXT) ||          \
  !defined(glGetTextureLevelParameterivEXT)
	#undef glTextureStorage1DEXT
	#undef glTextureStorage2DEXT
	#undef glTextureStorage3DEXT
	#undef glTextureImage1DEXT
	#undef glTextureImage2DEXT
	#undef glTextureImage3DEXT
	#undef glCompressedTextureImage1DEXT
	#undef glCompressedTextureImage2DEXT
	#undef glCompressedTextureImage3DEXT
	#undef glTextureSubImage1DEXT
	#undef glTextureSubImage2DEXT
	#undef glTextureSubImage3DEXT
	#undef glCompressedTextureSubImage1DEXT
	#undef glCompressedTextureSubImage2DEXT
	#undef glCompressedTextureSubImage3DEXT
	#undef glGetTextureImageEXT
	#undef glGetCompressedTextureImageEXT
	#undef glGenerateTextureMipmapEXT
	#undef glTextureBufferEXT
	#undef glTextureBufferRangeEXT
	#undef glTextureParameterfEXT
	#undef glTextureParameteriEXT
	#undef glTextureParameterfvEXT
	#undef glTextureParameterivEXT
	#undef glGetTextureLevelParameterivEXT

struct SAutoTexBind
{
	SAutoTexBind(GLenum eSubTarget, GLuint uTexture)
	{
		switch (eSubTarget)
		{
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			m_eTarget = GL_TEXTURE_CUBE_MAP;
			break;
		default:
			m_eTarget = eSubTarget;
			break;
		}

		GLenum eBindingName;
		switch (m_eTarget)
		{
		case GL_TEXTURE_CUBE_MAP:
			eBindingName = GL_TEXTURE_BINDING_CUBE_MAP;
			break;
		case GL_TEXTURE_2D:
			eBindingName = GL_TEXTURE_BINDING_2D;
			break;
		case GL_TEXTURE_2D_ARRAY:
			eBindingName = GL_TEXTURE_BINDING_2D_ARRAY;
			break;
		case GL_TEXTURE_3D:
			eBindingName = GL_TEXTURE_BINDING_3D;
			break;
	#if !DXGLES
		case GL_TEXTURE_1D:
			eBindingName = GL_TEXTURE_BINDING_1D;
			break;
		case GL_TEXTURE_1D_ARRAY:
			eBindingName = GL_TEXTURE_BINDING_1D_ARRAY;
			break;
		case GL_TEXTURE_RECTANGLE:
			eBindingName = GL_TEXTURE_BINDING_RECTANGLE;
			break;
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
			eBindingName = GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
			break;
	#endif //!DXGLES
	#if DXGL_SUPPORT_CUBEMAP_ARRAYS
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			eBindingName = GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
			break;
	#endif //DXGL_SUPPORT_CUBEMAP_ARRAYS
	#if DXGL_SUPPORT_MULTISAMPLED_TEXTURES
		case GL_TEXTURE_2D_MULTISAMPLE:
			eBindingName = GL_TEXTURE_BINDING_2D_MULTISAMPLE;
			break;
	#endif //DXGL_SUPPORT_MULTISAMPLED_TEXTURES
	#if DXGL_SUPPORT_TEXTURE_BUFFERS
		case GL_TEXTURE_BUFFER:
			eBindingName = GL_TEXTURE_BINDING_BUFFER;
			break;
	#endif //DXGL_SUPPORT_TEXTURE_BUFFERS
		default:
			assert(false);
			eBindingName = 0;
			break;
		}
		GLint iBinding;
		glGetIntegerv(eBindingName, &iBinding);
		m_uPreviousBinding = (GLuint)iBinding;
		glBindTexture(m_eTarget, uTexture);
	}

	~SAutoTexBind()
	{
		glBindTexture(m_eTarget, m_uPreviousBinding);
	}

	GLuint m_uPreviousBinding;
	GLenum m_eTarget;
};

inline void glTextureStorage1DEXT(GLuint uTexture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	SAutoTexBind kAutoBind(target, uTexture);
	#if DXGLES
	glTexStorage2D(target, levels, internalformat, width, 1);
	#else
	glTexStorage1D(target, levels, internalformat, width);
	#endif
}

inline void glTextureStorage2DEXT(GLuint uTexture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	SAutoTexBind kAutoBind(target, uTexture);
	glTexStorage2D(target, levels, internalformat, width, height);
}

inline void glTextureStorage3DEXT(GLuint uTexture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	SAutoTexBind kAutoBind(target, uTexture);
	glTexStorage3D(target, levels, internalformat, width, height, depth);
}

inline void glTextureImage1DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eInternalFormat, GLsizei iSizeX, GLint iBorder, GLenum eBaseFormat, GLenum eDataType, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	#if DXGLES
	glTexImage2D(eTarget, iLevel, eInternalFormat, iSizeX, 1, iBorder, eBaseFormat, eDataType, pData);
	#else
	glTexImage1D(eTarget, iLevel, eInternalFormat, iSizeX, iBorder, eBaseFormat, eDataType, pData);
	#endif
}

inline void glTextureImage2DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eInternalFormat, GLsizei iSizeX, GLsizei iSizeY, GLint iBorder, GLenum eBaseFormat, GLenum eDataType, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexImage2D(eTarget, iLevel, eInternalFormat, iSizeX, iSizeY, iBorder, eBaseFormat, eDataType, pData);
}

inline void glTextureImage3DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eInternalFormat, GLsizei iSizeX, GLsizei iSizeY, GLsizei iSizeZ, GLint iBorder, GLenum eBaseFormat, GLenum eDataType, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexImage3D(eTarget, iLevel, eInternalFormat, iSizeX, iSizeY, iSizeZ, iBorder, eBaseFormat, eDataType, pData);
}

inline void glCompressedTextureImage1DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eInternalFormat, GLsizei iSizeX, GLint iBorder, GLsizei iImageSize, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	#if DXGLES
	glCompressedTexImage2D(eTarget, iLevel, eInternalFormat, iSizeX, 1, iBorder, iImageSize, pData);
	#else
	glCompressedTexImage1D(eTarget, iLevel, eInternalFormat, iSizeX, iBorder, iImageSize, pData);
	#endif
}

inline void glCompressedTextureImage2DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eInternalFormat, GLsizei iSizeX, GLsizei iSizeY, GLint iBorder, GLsizei iImageSize, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glCompressedTexImage2D(eTarget, iLevel, eInternalFormat, iSizeX, iSizeY, iBorder, iImageSize, pData);
}

inline void glCompressedTextureImage3DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eInternalFormat, GLsizei iSizeX, GLsizei iSizeY, GLsizei iSizeZ, GLint iBorder, GLsizei iImageSize, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glCompressedTexImage3D(eTarget, iLevel, eInternalFormat, iSizeX, iSizeY, iSizeZ, iBorder, iImageSize, pData);
}

inline void glTextureSubImage1DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLint iOffsetX, GLsizei iSizeX, GLenum eFormat, GLenum eDataType, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	#if DXGLES
	glTexSubImage2D(eTarget, iLevel, iOffsetX, 0, iSizeX, 1, eFormat, eDataType, pData);
	#else
	glTexSubImage1D(eTarget, iLevel, iOffsetX, iSizeX, eFormat, eDataType, pData);
	#endif
}

inline void glTextureSubImage2DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLint iOffsetX, GLint iOffsetY, GLsizei iSizeX, GLsizei iSizeY, GLenum eFormat, GLenum eDataType, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexSubImage2D(eTarget, iLevel, iOffsetX, iOffsetY, iSizeX, iSizeY, eFormat, eDataType, pData);
}

inline void glTextureSubImage3DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLint iOffsetX, GLint iOffsetY, GLint iOffsetZ, GLsizei iSizeX, GLsizei iSizeY, GLsizei iSizeZ, GLenum eFormat, GLenum eDataType, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexSubImage3D(eTarget, iLevel, iOffsetX, iOffsetY, iOffsetZ, iSizeX, iSizeY, iSizeZ, eFormat, eDataType, pData);
}

inline void glCompressedTextureSubImage1DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLint iOffsetX, GLsizei iSizeX, GLenum eFormat, GLsizei iImageSize, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	#if DXGLES
	glCompressedTexSubImage2D(eTarget, iLevel, iOffsetX, 0, iSizeX, 1, eFormat, iImageSize, pData);
	#else
	glCompressedTexSubImage1D(eTarget, iLevel, iOffsetX, iSizeX, eFormat, iImageSize, pData);
	#endif
}

inline void glCompressedTextureSubImage2DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLint iOffsetX, GLint iOffsetY, GLsizei iSizeX, GLsizei iSizeY, GLenum eFormat, GLsizei iImageSize, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glCompressedTexSubImage2D(eTarget, iLevel, iOffsetX, iOffsetY, iSizeX, iSizeY, eFormat, iImageSize, pData);
}

inline void glCompressedTextureSubImage3DEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLint iOffsetX, GLint iOffsetY, GLint iOffsetZ, GLsizei iSizeX, GLsizei iSizeY, GLsizei iSizeZ, GLenum eFormat, GLsizei iImageSize, const GLvoid* pData)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glCompressedTexSubImage3D(eTarget, iLevel, iOffsetX, iOffsetY, iOffsetZ, iSizeX, iSizeY, iSizeZ, eFormat, iImageSize, pData);
}

	#if !DXGLES

inline void glGetTextureImageEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eFormat, GLenum eType, void* pPixels)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glGetTexImage(eTarget, iLevel, eFormat, eType, pPixels);
}

inline void glGetCompressedTextureImageEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, void* pImg)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glGetCompressedTexImage(eTarget, iLevel, pImg);
}

inline void glTextureBufferEXT(GLuint uTexture, GLenum eTarget, GLenum eInternalFormat, GLuint uBuffer)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexBuffer(eTarget, eInternalFormat, uBuffer);
}

	#endif //!DXGLES

inline void glGenerateTextureMipmapEXT(GLuint uTexture, GLenum eTarget)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glGenerateMipmap(eTarget);
}

	#if defined(glTexBufferRange)
inline void glTextureBufferRangeEXT(GLuint uTexture, GLenum eTarget, GLenum eInternalformat, GLuint uBuffer, GLintptr iOffset, GLsizeiptr iSize)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexBufferRange(eTarget, eInternalformat, uBuffer, iOffset, iSize);
}
	#endif //defined(glTexBufferRange)

inline void glTextureParameterfEXT(GLuint uTexture, GLenum eTarget, GLenum eName, GLfloat fParam)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexParameterf(eTarget, eName, fParam);
}

inline void glTextureParameteriEXT(GLuint uTexture, GLenum eTarget, GLenum eName, GLint iParam)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexParameteri(eTarget, eName, iParam);
}

inline void glTextureParameterfvEXT(GLuint uTexture, GLenum eTarget, GLenum eName, const GLfloat* pfParam)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexParameterfv(eTarget, eName, pfParam);
}

inline void glTextureParameterivEXT(GLuint uTexture, GLenum eTarget, GLenum eName, const GLint* piParam)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	glTexParameteriv(eTarget, eName, piParam);
}

inline void glGetTextureLevelParameterivEXT(GLuint uTexture, GLenum eTarget, GLint iLevel, GLenum eName, GLint* piParam)
{
	SAutoTexBind kAutoBind(eTarget, uTexture);
	#if DXGLES && DXGLES_REQUIRED_VERSION < DXGLES_VERSION_31
	DXGL_NOT_IMPLEMENTED;
	#else
	glGetTexLevelParameteriv(eTarget, iLevel, eName, piParam);
	#endif
}

#endif

////////////////////////////////////////////////////////////////////////////
// Direct state access buffer functions emulation when not available
////////////////////////////////////////////////////////////////////////////

template<GLenum eTarget = GL_COPY_WRITE_BUFFER, GLenum eBinding = GL_COPY_WRITE_BUFFER_BINDING>
struct SAutoBufferBind
{
	enum {TARGET = eTarget, BINDING = eBinding};

	SAutoBufferBind(GLuint uBuffer)
	{
		GLint iBinding;
		glGetIntegerv(BINDING, &iBinding);
		m_uPreviousBinding = (GLuint)iBinding;
		glBindBuffer(TARGET, uBuffer);
	}

	~SAutoBufferBind()
	{
		glBindBuffer(TARGET, m_uPreviousBinding);
	}

	GLuint m_uPreviousBinding;
};

#if !defined(glNamedBufferDataEXT)
inline void glNamedBufferDataEXT(GLuint uBuffer, GLsizeiptr iSize, const void* pData, GLenum eUsage)
{
	SAutoBufferBind<> kAutoBind(uBuffer);
	glBufferData(kAutoBind.TARGET, iSize, pData, eUsage);
}
#endif

#if !defined(glNamedBufferSubDataEXT)
inline void glNamedBufferSubDataEXT(GLuint uBuffer, GLintptr iOffset, GLsizeiptr iSize, const void* pData)
{
	SAutoBufferBind<> kAutoBind(uBuffer);
	glBufferSubData(kAutoBind.TARGET, iOffset, iSize, pData);
}
#endif

#if !defined(glMapNamedBufferEXT)
inline GLvoid* glMapNamedBufferEXT(GLuint uBuffer, GLenum eAccess)
{
	SAutoBufferBind<> kAutoBind(uBuffer);
	#if DXGLES
	// Workaround, bind full buffer with range
	DXGL_NOT_IMPLEMENTED;
	return NULL;
	#else
	return glMapBuffer(kAutoBind.TARGET, eAccess);
	#endif
}
#endif

#if !defined(glMapNamedBufferRangeEXT)
inline GLvoid* glMapNamedBufferRangeEXT(GLuint uBuffer, GLintptr iOffset, GLsizeiptr iLength, GLbitfield uAccess)
{
	SAutoBufferBind<> kAutoBind(uBuffer);
	return glMapBufferRange(kAutoBind.TARGET, iOffset, iLength, uAccess);
}
#endif

#if !defined(glUnmapNamedBufferEXT)
inline GLboolean glUnmapNamedBufferEXT(GLuint uBuffer)
{
	SAutoBufferBind<> kAutoBind(uBuffer);
	return glUnmapBuffer(kAutoBind.TARGET);
}
#endif

#if !defined(glGetNamedBufferSubDataEXT) && !DXGLES
inline void glGetNamedBufferSubDataEXT(GLuint uBuffer, GLintptr iOffset, GLsizeiptr iSize, void* pData)
{
	SAutoBufferBind<> kAutoBind(uBuffer);
	glGetBufferSubData(kAutoBind.TARGET, iOffset, iSize, pData);
}
#endif

#if !defined(glNamedCopyBufferSubDataEXT)
inline void glNamedCopyBufferSubDataEXT(GLuint uReadBuffer, GLuint uWriteBuffer, GLintptr iReadOffset, GLintptr iWriteOffset, GLsizeiptr iSize)
{
	SAutoBufferBind<GL_COPY_READ_BUFFER, GL_COPY_READ_BUFFER_BINDING> kAutoBindRead(uReadBuffer);
	SAutoBufferBind<GL_COPY_WRITE_BUFFER, GL_COPY_WRITE_BUFFER_BINDING> kAutoBindWrite(uWriteBuffer);
	glCopyBufferSubData(kAutoBindRead.TARGET, kAutoBindWrite.TARGET, iReadOffset, iWriteOffset, iSize);
}
#endif

#if !defined(glNamedBufferStorageEXT) && DXGL_SUPPORT_BUFFER_STORAGE
inline void glNamedBufferStorageEXT(GLuint uBuffer, GLsizeiptr iSize, const GLvoid* pData, GLbitfield uFlags)
{
	SAutoBufferBind<> kAutoBind(uBuffer);
	glBufferStorage(kAutoBind.TARGET, iSize, pData, uFlags);
}
#endif

#if DXGL_USE_ES_EMULATOR
// ES extensions
	#define GL_DEBUG_OUTPUT                GL_DEBUG_OUTPUT_KHR
	#define GL_DEBUG_OUTPUT_SYNCHRONOUS    GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR
	#define GL_DEBUG_SEVERITY_HIGH         GL_DEBUG_SEVERITY_HIGH_KHR
	#define GL_DEBUG_SEVERITY_MEDIUM       GL_DEBUG_SEVERITY_MEDIUM_KHR
	#define GL_DEBUG_SEVERITY_LOW          GL_DEBUG_SEVERITY_LOW_KHR
	#define GL_DEBUG_SEVERITY_NOTIFICATION GL_DEBUG_SEVERITY_NOTIFICATION_KHR
	#define GLDEBUGPROC                    GLDEBUGPROCKHR
extern PFNGLDEBUGMESSAGECONTROLKHRPROC glDebugMessageControl;
extern PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallback;
#endif //DXGL_USE_ES_EMULATOR

#if CRY_PLATFORM_ANDROID && !defined(EGL_OPENGL_API)
	#define EGL_OPENGL_API 0x30A2
#endif

#if DXGLES && DXGLES_REQUIRED_VERSION == DXGLES_VERSION_30 && defined(GL_EXT_separate_shader_objects)
	#define GL_VERTEX_SHADER_BIT        GL_VERTEX_SHADER_BIT_EXT
	#define GL_FRAGMENT_SHADER_BIT      GL_FRAGMENT_SHADER_BIT_EXT
	#define GL_ALL_SHADER_BITS          GL_ALL_SHADER_BITS_EXT
	#define GL_PROGRAM_SEPARABLE        GL_PROGRAM_SEPARABLE_EXT
	#define GL_ACTIVE_PROGRAM           GL_ACTIVE_PROGRAM_EXT
	#define GL_PROGRAM_PIPELINE_BINDING GL_PROGRAM_PIPELINE_BINDING_EXT

	#define glUseProgramStages          glUseProgramStagesEXT
	#define glActiveShaderProgram       glActiveShaderProgramEXT
	#define glCreateShaderProgramv      glCreateShaderProgramvEXT
	#define glBindProgramPipeline       glBindProgramPipelineEXT
	#define glDeleteProgramPipelines    glDeleteProgramPipelinesEXT
	#define glGenProgramPipelines       glGenProgramPipelinesEXT
	#define glIsProgramPipeline         glIsProgramPipelineEXT
	#define glProgramParameteri         glProgramParameteriEXT
	#define glGetProgramPipelineiv      glGetProgramPipelineivEXT
	#define glValidateProgramPipeline   glValidateProgramPipelineEXT
	#define glGetProgramPipelineInfoLog glGetProgramPipelineInfoLogEXT

	#define glProgramUniform1i          glProgramUniform1iEXT
	#define glProgramUniform2i          glProgramUniform2iEXT
	#define glProgramUniform2i          glProgramUniform2iEXT
	#define glProgramUniform2i          glProgramUniform2iEXT

	#define glProgramUniform1f          glProgramUniform1fEXT
	#define glProgramUniform2f          glProgramUniform2fEXT
	#define glProgramUniform3f          glProgramUniform3fEXT
	#define glProgramUniform4f          glProgramUniform4fEXT

	#define glProgramUniform1ui         glProgramUniform1uiEXT
	#define glProgramUniform2ui         glProgramUniform2uiEXT
	#define glProgramUniform3ui         glProgramUniform3uiEXT
	#define glProgramUniform4ui         glProgramUniform4uiEXT

	#define glProgramUniform1iv         glProgramUniform1ivEXT
	#define glProgramUniform2iv         glProgramUniform2ivEXT
	#define glProgramUniform3iv         glProgramUniform3ivEXT
	#define glProgramUniform4iv         glProgramUniform4ivEXT

	#define glProgramUniform1fv         glProgramUniform1fvEXT
	#define glProgramUniform2fv         glProgramUniform2fvEXT
	#define glProgramUniform3fv         glProgramUniform3fvEXT
	#define glProgramUniform4fv         glProgramUniform4fvEXT

	#define glProgramUniform1uiv        glProgramUniform1uivEXT
	#define glProgramUniform2uiv        glProgramUniform2uivEXT
	#define glProgramUniform3uiv        glProgramUniform3uivEXT
	#define glProgramUniform4uiv        glProgramUniform4uivEXT

	#define glProgramUniformMatrix2fv   glProgramUniformMatrix2fvEXT
	#define glProgramUniformMatrix3fv   glProgramUniformMatrix3fvEXT
	#define glProgramUniformMatrix4fv   glProgramUniformMatrix4fvEXT
	#define glProgramUniformMatrix2x3fv glProgramUniformMatrix2x3fvEXT
	#define glProgramUniformMatrix3x2fv glProgramUniformMatrix3x2fvEXT
	#define glProgramUniformMatrix2x4fv glProgramUniformMatrix2x4fvEXT
	#define glProgramUniformMatrix4x2fv glProgramUniformMatrix4x2fvEXT
	#define glProgramUniformMatrix3x4fv glProgramUniformMatrix3x4fvEXT
	#define glProgramUniformMatrix4x3fv glProgramUniformMatrix4x3fvEXT
#endif

#endif //__GLFEATURES__
