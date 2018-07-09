// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLShader.hpp
//  Version:     v1.00
//  Created:     15/05/2013 by Valerio Guagliumi.
//  Description: Declares the shader types and related functions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLSHADER__
#define __GLSHADER__

#include "GLCommon.hpp"
#if !DXGL_INPUT_GLSL && DXGL_GLSL_FROM_HLSLCROSSCOMPILER
	#if defined(USE_SDL2_VIDEO)
DXGL_TODO("Find a better way to prevent typedef conflicts between the stdint replacement in sdl and the one in hlslcc")
		#include <stdint.h>
		#define _PSTDINT_H_INCLUDED 1
	#endif //defined(USE_SDL2_VIDEO)
	#include "hlslcc.hpp"
#endif //!DXGL_INPUT_GLSL && DXGL_GLSL_FROM_HLSLCROSSCOMPILER

#if DXGL_GLSL_FROM_HLSLCROSSCOMPILER && !DXGL_INPUT_GLSL && DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	#define DXGL_ENABLE_SHADER_TRACING 1
#else
	#define DXGL_ENABLE_SHADER_TRACING 0
#endif

#if DXGL_GLSL_FROM_HLSLCROSSCOMPILER && !DXGL_INPUT_GLSL
	#define DXGL_HLSL_TO_GLSL_DEBUG 1
#else
	#define DXGL_HLSL_TO_GLSL_DEBUG 0
#endif

#define DXGL_DUMP_GLSL_SOURCES 0

#define DXGL_SHADER_TYPE_MAP(_Predicate, _Vertex, _Fragment, _Geometry, _TessControl, _TessEvaluation, _Compute) \
  _Predicate(_Vertex) _Predicate(_Fragment)                                                                      \
  DXGL_IF(DXGL_ENABLE_GEOMETRY_SHADERS)(_Predicate(_Geometry))                                                   \
  DXGL_IF(DXGL_ENABLE_TESSELLATION_SHADERS)(_Predicate(_TessControl) _Predicate(_TessEvaluation))                \
  DXGL_IF(DXGL_ENABLE_COMPUTE_SHADERS)(_Predicate(_Compute))                                                     \

namespace NCryOpenGL
{

enum EShaderType
{
#define _ENUM(_Name) _Name,
	DXGL_SHADER_TYPE_MAP(_ENUM, eST_Vertex, eST_Fragment, eST_Geometry, eST_TessControl, eST_TessEvaluation, eST_Compute)
#undef _ENUM
	eST_NUM
};

enum EPipelineMode
{
	ePM_Graphics,
	ePM_Compute
};

enum EResourceUnitType
{
	eRUT_Texture,
	eRUT_UniformBuffer,
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	eRUT_StorageBuffer,
#endif
#if DXGL_SUPPORT_SHADER_IMAGES
	eRUT_Image,
#endif
	eRUT_NUM
};

struct SPipeline;
class CContext;

struct SSource
{
	const char* m_pData;
	uint32      m_uDataSize;

	SSource(const char* pData = NULL, uint32 uDataSize = 0);
	~SSource();
};

#if DXGL_ENABLE_SHADER_TRACING

struct SShaderTraceIndex
{
	std::vector<uint32>            m_kTraceStepSizes;
	std::vector<uint32>            m_kTraceStepOffsets;
	std::vector<VariableTraceInfo> m_kTraceVariables;
};

#endif

struct SShaderSource : SSource
{
	SShaderSource();
	~SShaderSource();

	void SetData(const char* pData = NULL, uint32 uDataSize = 0);
};

enum {DXGL_MAX_REFLECT_STRING_LENGTH = 128};

struct SShaderReflectionVariable
{
	D3D11_SHADER_VARIABLE_DESC m_kDesc;
	D3D11_SHADER_TYPE_DESC     m_kType;
	char                       m_acName[DXGL_MAX_REFLECT_STRING_LENGTH];
	std::vector<uint8>         m_kDefaultValue;
};

struct SShaderReflectionConstBuffer
{
	typedef std::vector<SShaderReflectionVariable> TVariables;

	TVariables               m_kVariables;
	D3D11_SHADER_BUFFER_DESC m_kDesc;
	char                     m_acName[DXGL_MAX_REFLECT_STRING_LENGTH];
};

struct SShaderReflectionResource
{
	D3D11_SHADER_INPUT_BIND_DESC m_kDesc;
	char                         m_acName[DXGL_MAX_REFLECT_STRING_LENGTH];
};

struct SShaderReflectionParameter
{
	D3D11_SIGNATURE_PARAMETER_DESC m_kDesc;
	char                           m_acSemanticName[DXGL_MAX_REFLECT_STRING_LENGTH];
};

struct SShaderReflection
{
	typedef std::vector<SShaderReflectionConstBuffer> TConstantBuffers;
	typedef std::vector<SShaderReflectionResource>    TResources;
	typedef std::vector<SShaderReflectionParameter>   TParameters;

	uint32            m_uGLSLSourceOffset;
	uint32            m_uSamplersOffset;
	uint32            m_uNumSamplers;
	uint32            m_uImagesOffset;
	uint32            m_uNumImages;
	uint32            m_uStorageBuffersOffset;
	uint32            m_uNumStorageBuffers;
	uint32            m_uUniformBuffersOffset;
	uint32            m_uNumUniformBuffers;
	uint32            m_uImportsOffset;
	uint32            m_uImportsSize;
	uint32            m_uExportsOffset;
	uint32            m_uExportsSize;
	uint32            m_uInputHash;
	uint32            m_uSymbolsOffset;

	TConstantBuffers  m_kConstantBuffers;
	TResources        m_kResources;
	TParameters       m_kInputs, m_kOutputs, m_kPatchConstants;
	D3D11_SHADER_DESC m_kDesc;
};

struct SGLShader
{
	GLuint m_uName; // Name of the shader program if SupportSeparablePrograms() returns true, name of the shader otherwise

	SGLShader();
	~SGLShader();
};

DXGL_DECLARE_REF_COUNTED(struct, SShader)
{
	EShaderType m_eType;

	SShaderSource m_kSource;
	SShaderReflection m_kReflection;
#if DXGL_ENABLE_SHADER_TRACING
	SShaderTraceIndex m_kTraceIndex;
#endif

	typedef std::vector<SPipeline*> TPipelines;
	TPipelines m_kBoundPipelines;

	void AttachPipeline(SPipeline * pPipeline);
	void DetachPipeline(SPipeline * pPipeline);

	SShader();
	~SShader();
};

struct SPipelineConfiguration
{
	SShader*      m_apShaders[eST_NUM];
#if DXGL_ENABLE_SHADER_TRACING
	EShaderType   m_eStageTracing;
#endif
	EPipelineMode m_eMode;
#if !DXGL_SUPPORT_DEPTH_CLAMP
	bool          m_bEmulateDepthClamp;
#endif

	SPipelineConfiguration();
};

struct SPipelineCompilationBuffer
{
	enum { VERSION_STRING_CAPACITY = 32 };

	GLchar* m_acImportsHeader;
	uint32  m_uImportsBeginOffset;
	uint32  m_uImportsEndOffset;
	uint32  m_uImportsHeaderCapacity;
	GLchar  m_acVersionString[VERSION_STRING_CAPACITY];
	GLint   m_iVersionStringLength;

	SPipelineCompilationBuffer();
	~SPipelineCompilationBuffer();
};

struct SIndexPartitionRange
{
	enum { INVALID_INDEX = 0xFFFFFFFFu };

	uint32 m_uFirstIn;
	uint32 m_uFirstOut;
	uint32 m_uCount;

	uint32 operator()(uint32 uSlot) const { return uSlot < m_uFirstIn || uSlot >= m_uFirstIn + m_uCount ? INVALID_INDEX : m_uFirstOut + uSlot - m_uFirstIn; }
};

struct SIndexPartition
{
	SIndexPartitionRange  m_akStages[eST_NUM];

	SIndexPartitionRange& operator[](EShaderType eType)       { return m_akStages[eType]; };
	SIndexPartitionRange  operator[](EShaderType eType) const { return m_akStages[eType]; };
};

DXGL_DECLARE_REF_COUNTED(struct, SUnitMap)
{
	enum
	{
		MAX_TEXTURE_SLOT_IN_MAP = 0x400,
		MAX_SAMPLER_SLOT_IN_MAP = 0x400,
		MAX_TEXTURE_UNIT_IN_MAP = 0x400,
	};

	struct SElement
	{
		uint32 m_uMask;

		// For non-texture unit maps
		uint32          GetResourceSlot() const { return m_uMask >> 16; }
		uint32          GetResourceUnit() const { return m_uMask & 0xFFFF; }

		static SElement Resource(uint32 uSlot, uint32 uUnit)
		{
			SElement kElement = { uSlot << 16 | uUnit };
			return kElement;
		}

		// For texture unit maps
		uint32          GetTextureSlot() const { return m_uMask >> 22; }
		uint32          GetSamplerSlot() const { return (m_uMask >> 12) & 0x3FF; }
		uint32          GetTextureUnit() const { return (m_uMask >> 2) & 0x3FF; }
		bool            HasSampler()     const { return ((m_uMask >> 1) & 0x1) == 1; }

		static SElement TextureWithoutSampler(uint32 uTextureSlot, uint32 uTextureUnit)
		{
			SElement kElement = { uTextureSlot << 22 | uTextureUnit << 2 };
			return kElement;
		}

		static SElement TextureWithSampler(uint32 uTextureSlot, uint32 uSamplerSlot, uint32 uTextureUnit)
		{
			SElement kElement = { uTextureSlot << 22 | uSamplerSlot << 12 | uTextureUnit << 2 | 1 << 1 };
			return kElement;
		}
	};

	SElement* m_akUnits;
	uint16 m_uNumUnits;

	SUnitMap(uint16 uNumUnits = 0);
	SUnitMap(const SUnitMap &kOther);
	~SUnitMap();
};

DXGL_DECLARE_REF_COUNTED(struct, SPipeline)
{
	GLuint m_uName; // Name of the pipeline if SupportSeparablePrograms() returns true, name of the program otherwise
	SGLShader m_akGLShaders[eST_NUM];

	CContext* m_pContext;
	SPipelineConfiguration m_kConfiguration;
	SUnitMapPtr m_aspResourceUnitMaps[eRUT_NUM];

#if DXGL_ENABLE_SHADER_TRACING
	uint32 m_uTraceBufferUnit;
#endif

	SPipeline(const SPipelineConfiguration &kConfiguration, CContext * pContext);
	~SPipeline();
};

DXGL_DECLARE_REF_COUNTED(struct, SInputLayout)
{
	// Parameters taken by glVertexAttribPointer (excluding stride and pointer base that are part of the vertex buffer data)
	struct SAttributeParameters
	{
		GLuint    m_uAttributeIndex;
		GLint     m_iDimension;
		GLenum    m_eType;
		GLboolean m_bNormalized;
		GLuint    m_uPointerOffset;
		GLuint    m_uVertexAttribDivisor;
		bool      m_bInteger;
	};

	// Interleaved vertex attributes for a specific input assembler slot
	typedef std::vector<SAttributeParameters> TAttributes;

	// Vertex attributes for each input assembler slot
	TAttributes m_akSlotAttributes[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

#if DXGL_SUPPORT_VERTEX_ATTRIB_BINDING
	// for vertex_attrib_binding we need the binding slot in addition to the other data
	struct SAttributeFormats : SAttributeParameters
	{
		GLuint m_uBindingIndex;
	};

	typedef std::vector<SAttributeFormats> TAttributeFormats;
	TAttributeFormats m_akVertexAttribFormats;
#endif //DXGL_SUPPORT_VERTEX_ATTRIB_BINDING
};

typedef SShaderReflection TShaderReflection;

bool            InitializeShaderReflectionFromInput(SShaderReflection* pReflection, const void* pvData);

bool            CompilePipeline(SPipeline* pPipeline, SPipelineCompilationBuffer* pBuffer, CDevice* pDevice);
bool            InitializeShader(SShader* pShader, const void* pvSourceData, size_t uSourceSize, const GLchar* szSourceOverride = NULL);
bool            InitializePipelineResources(SPipeline* pPipeline, CContext* pContext);
SInputLayoutPtr CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 uNumElements, const TShaderReflection& kReflection);

bool            IsPipelineStageUsed(EPipelineMode eMode, EShaderType eType);
bool            GetGLShaderType(GLenum& eGLShaderType, EShaderType eType);
bool            GetGLProgramStageBit(GLbitfield& uProgramStageBit, EShaderType eType);

bool            VerifyShaderStatus(GLuint uShader, GLenum eStatus);
bool            VerifyProgramStatus(GLuint uProgram, GLenum eStatus);
bool            VerifyProgramPipelineStatus(GLuint uPipeline, GLenum eStatus);

}

#endif //__GLSHADER__
