#ifndef HLSLCC_H_
#define HLSLCC_H_

#if defined (_WIN32) && defined(HLSLCC_DYNLIB)
    #define HLSLCC_APIENTRY __stdcall
    #if defined(libHLSLcc_EXPORTS)
        #define HLSLCC_API __declspec(dllexport)
    #else
        #define HLSLCC_API __declspec(dllimport)
    #endif
#else
    #define HLSLCC_APIENTRY
    #define HLSLCC_API
#endif

#define EXCLUDE_PSTDINT
#ifndef EXCLUDE_PSTDINT
#include "pstdint.h"
#else
#include <stdint.h>
#endif

typedef enum
{
	LANG_DEFAULT,// Depends on the HLSL shader model.
	LANG_ES_100,
	LANG_ES_300,
	LANG_ES_310,
	LANG_120,
	LANG_130,
	LANG_140,
	LANG_150,
	LANG_200,
	LANG_320,
	LANG_330,
	LANG_400,
	LANG_410,
	LANG_420,
	LANG_430,
	LANG_440,
	LANG_450,
} GLLang;

typedef struct {
	uint32_t ARB_explicit_attrib_location : 1;
	uint32_t ARB_explicit_uniform_location : 1;
	uint32_t ARB_shading_language_420pack : 1;
}GlExtensions;

enum {MAX_SHADER_VEC4_OUTPUT = 512};
enum {MAX_SHADER_VEC4_INPUT = 512};
enum {MAX_TEXTURES = 128};
enum {MAX_FORK_PHASES = 2};
enum {MAX_FUNCTION_BODIES = 1024};
enum {MAX_CLASS_TYPES = 1024};
enum {MAX_FUNCTION_POINTERS = 128};

//Reflection
#define MAX_REFLECT_STRING_LENGTH 512
#define MAX_SHADER_VARS 256
#define MAX_CBUFFERS 256
#define MAX_UAV 256
#define MAX_FUNCTION_TABLES 256
#define MAX_RESOURCE_BINDINGS 256

typedef enum SPECIAL_NAME
{
	NAME_UNDEFINED = 0,
	NAME_POSITION = 1,
	NAME_CLIP_DISTANCE = 2,
	NAME_CULL_DISTANCE = 3,
	NAME_RENDER_TARGET_ARRAY_INDEX = 4,
	NAME_VIEWPORT_ARRAY_INDEX = 5,
	NAME_VERTEX_ID = 6,
	NAME_PRIMITIVE_ID = 7,
	NAME_INSTANCE_ID = 8,
	NAME_IS_FRONT_FACE = 9,
	NAME_SAMPLE_INDEX = 10,
	// The following are added for D3D11
	NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR = 11, 
	NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR = 12, 
	NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR = 13, 
	NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR = 14, 
	NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR = 15, 
	NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR = 16, 
	NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR = 17, 
	NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR = 18, 
	NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR = 19, 
	NAME_FINAL_TRI_INSIDE_TESSFACTOR = 20, 
	NAME_FINAL_LINE_DETAIL_TESSFACTOR = 21,
	NAME_FINAL_LINE_DENSITY_TESSFACTOR = 22,
} SPECIAL_NAME;


typedef enum { 
	INOUT_COMPONENT_UNKNOWN  = 0,
	INOUT_COMPONENT_UINT32   = 1,
	INOUT_COMPONENT_SINT32   = 2,
	INOUT_COMPONENT_FLOAT32  = 3
} INOUT_COMPONENT_TYPE;

typedef enum MIN_PRECISION { 
	MIN_PRECISION_DEFAULT    = 0,
	MIN_PRECISION_FLOAT_16   = 1,
	MIN_PRECISION_FLOAT_2_8  = 2,
	MIN_PRECISION_RESERVED   = 3,
	MIN_PRECISION_SINT_16    = 4,
	MIN_PRECISION_UINT_16    = 5,
	MIN_PRECISION_ANY_16     = 0xf0,
	MIN_PRECISION_ANY_10     = 0xf1
} MIN_PRECISION;

typedef struct InOutSignature_TAG
{
	char SemanticName[MAX_REFLECT_STRING_LENGTH];
	uint32_t ui32SemanticIndex;
	SPECIAL_NAME eSystemValueType;
	INOUT_COMPONENT_TYPE eComponentType;
	uint32_t ui32Register;
	uint32_t ui32Mask;
	uint32_t ui32ReadWriteMask;

	uint32_t ui32Stream;
	MIN_PRECISION eMinPrec;

} InOutSignature;

typedef enum ResourceType_TAG
{
	RTYPE_CBUFFER,//0
	RTYPE_TBUFFER,//1
	RTYPE_TEXTURE,//2
	RTYPE_SAMPLER,//3
	RTYPE_UAV_RWTYPED,//4
	RTYPE_STRUCTURED,//5
	RTYPE_UAV_RWSTRUCTURED,//6
	RTYPE_BYTEADDRESS,//7
	RTYPE_UAV_RWBYTEADDRESS,//8
	RTYPE_UAV_APPEND_STRUCTURED,//9
	RTYPE_UAV_CONSUME_STRUCTURED,//10
	RTYPE_UAV_RWSTRUCTURED_WITH_COUNTER,//11
	RTYPE_COUNT,
} ResourceType;

typedef enum ResourceGroup_TAG {
	RGROUP_CBUFFER,
	RGROUP_TEXTURE,
	RGROUP_SAMPLER,
	RGROUP_UAV,
	RGROUP_BUFFER,
	RGROUP_COUNT,
} ResourceGroup;

typedef enum REFLECT_RESOURCE_DIMENSION
{
	REFLECT_RESOURCE_DIMENSION_UNKNOWN = 0,
	REFLECT_RESOURCE_DIMENSION_BUFFER = 1,
	REFLECT_RESOURCE_DIMENSION_TEXTURE1D = 2,
	REFLECT_RESOURCE_DIMENSION_TEXTURE1DARRAY = 3,
	REFLECT_RESOURCE_DIMENSION_TEXTURE2D = 4,
	REFLECT_RESOURCE_DIMENSION_TEXTURE2DARRAY = 5,
	REFLECT_RESOURCE_DIMENSION_TEXTURE2DMS = 6,
	REFLECT_RESOURCE_DIMENSION_TEXTURE2DMSARRAY = 7,
	REFLECT_RESOURCE_DIMENSION_TEXTURE3D = 8,
	REFLECT_RESOURCE_DIMENSION_TEXTURECUBE = 9,
	REFLECT_RESOURCE_DIMENSION_TEXTURECUBEARRAY = 10,
	REFLECT_RESOURCE_DIMENSION_BUFFEREX = 11,
} REFLECT_RESOURCE_DIMENSION;

typedef struct ResourceBinding_TAG
{
	char Name[MAX_REFLECT_STRING_LENGTH];
	ResourceType eType;
	uint32_t ui32BindPoint;
	uint32_t ui32BindCount;
	uint32_t ui32Flags;
	REFLECT_RESOURCE_DIMENSION eDimension;
	uint32_t ui32ReturnType;
	uint32_t ui32NumSamples;
} ResourceBinding;

typedef enum _SHADER_VARIABLE_TYPE { 
	SVT_VOID                         = 0,
	SVT_BOOL                         = 1,
	SVT_INT                          = 2,
	SVT_FLOAT                        = 3,
	SVT_STRING                       = 4,
	SVT_TEXTURE                      = 5,
	SVT_TEXTURE1D                    = 6,
	SVT_TEXTURE2D                    = 7,
	SVT_TEXTURE3D                    = 8,
	SVT_TEXTURECUBE                  = 9,
	SVT_SAMPLER                      = 10,
	SVT_PIXELSHADER                  = 15,
	SVT_VERTEXSHADER                 = 16,
	SVT_UINT                         = 19,
	SVT_UINT8                        = 20,
	SVT_GEOMETRYSHADER               = 21,
	SVT_RASTERIZER                   = 22,
	SVT_DEPTHSTENCIL                 = 23,
	SVT_BLEND                        = 24,
	SVT_BUFFER                       = 25,
	SVT_CBUFFER                      = 26,
	SVT_TBUFFER                      = 27,
	SVT_TEXTURE1DARRAY               = 28,
	SVT_TEXTURE2DARRAY               = 29,
	SVT_RENDERTARGETVIEW             = 30,
	SVT_DEPTHSTENCILVIEW             = 31,
	SVT_TEXTURE2DMS                  = 32,
	SVT_TEXTURE2DMSARRAY             = 33,
	SVT_TEXTURECUBEARRAY             = 34,
	SVT_HULLSHADER                   = 35,
	SVT_DOMAINSHADER                 = 36,
	SVT_INTERFACE_POINTER            = 37,
	SVT_COMPUTESHADER                = 38,
	SVT_DOUBLE                       = 39,
	SVT_RWTEXTURE1D                  = 40,
	SVT_RWTEXTURE1DARRAY             = 41,
	SVT_RWTEXTURE2D                  = 42,
	SVT_RWTEXTURE2DARRAY             = 43,
	SVT_RWTEXTURE3D                  = 44,
	SVT_RWBUFFER                     = 45,
	SVT_BYTEADDRESS_BUFFER           = 46,
	SVT_RWBYTEADDRESS_BUFFER         = 47,
	SVT_STRUCTURED_BUFFER            = 48,
	SVT_RWSTRUCTURED_BUFFER          = 49,
	SVT_APPEND_STRUCTURED_BUFFER     = 50,
	SVT_CONSUME_STRUCTURED_BUFFER    = 51,
	SVT_FLOAT8                       = 52,
	SVT_FLOAT10                      = 53,
	SVT_FLOAT16                      = 54,
	SVT_INT12                        = 55,
	SVT_INT16                        = 56,
	SVT_UINT16                       = 57,
//	SVT_FLOATSNORM                   = 58, // syntactic sugar?
//	SVT_FLOATUNORM                   = 59,

	SVT_FORCE_DWORD                  = 0x7fffffff
} SHADER_VARIABLE_TYPE;

typedef enum _SHADER_VARIABLE_CLASS { 
	SVC_SCALAR               = 0,
	SVC_VECTOR               = ( SVC_SCALAR + 1 ),
	SVC_MATRIX_ROWS          = ( SVC_VECTOR + 1 ),
	SVC_MATRIX_COLUMNS       = ( SVC_MATRIX_ROWS + 1 ),
	SVC_OBJECT               = ( SVC_MATRIX_COLUMNS + 1 ),
	SVC_STRUCT               = ( SVC_OBJECT + 1 ),
	SVC_INTERFACE_CLASS      = ( SVC_STRUCT + 1 ),
	SVC_INTERFACE_POINTER    = ( SVC_INTERFACE_CLASS + 1 ),
	SVC_FORCE_DWORD          = 0x7fffffff
} SHADER_VARIABLE_CLASS;

typedef struct ShaderVarType_TAG {
	SHADER_VARIABLE_CLASS Class;
	SHADER_VARIABLE_TYPE  Type;
	uint32_t              Rows;
	uint32_t              Columns;
	uint32_t              Elements;
	uint32_t              MemberCount;
	uint32_t              Offset;
	uint32_t              Unaligned;
	char                  Name[MAX_REFLECT_STRING_LENGTH];
	char                  FullName[MAX_REFLECT_STRING_LENGTH]; // Includes all parent names.

	uint32_t              ParentCount;

	struct ShaderVarType_TAG * Parent;

	struct ShaderVarType_TAG * Members;
} ShaderVarType;

typedef struct ShaderVar_TAG
{
	char Name[MAX_REFLECT_STRING_LENGTH];
	int haveDefaultValue;
	uint32_t* pui32DefaultValues;
	//Offset/Size in bytes.
	uint32_t ui32StartOffset;
	uint32_t ui32Size;
	uint32_t ui32Flags;

	struct ShaderVarType_TAG sType;
} ShaderVar;

typedef struct ConstantBuffer_TAG
{
	char Name[MAX_REFLECT_STRING_LENGTH];

	uint32_t ui32NumVars;
	ShaderVar* asVars;

	uint32_t ui32TotalSizeInBytes;

	int iUnsized;
	int blob;
} ConstantBuffer;

typedef struct ClassType_TAG
{
	char Name[MAX_REFLECT_STRING_LENGTH];
	uint16_t ui16ID;
	uint16_t ui16ConstBufStride;
	uint16_t ui16Texture;
	uint16_t ui16Sampler;
} ClassType;

typedef struct ClassInstance_TAG
{
	char Name[MAX_REFLECT_STRING_LENGTH];
	uint16_t ui16ID;
	uint16_t ui16ConstBuf;
	uint16_t ui16ConstBufOffset;
	uint16_t ui16Texture;
	uint16_t ui16Sampler;
} ClassInstance;

typedef enum TESSELLATOR_PARTITIONING
{
	TESSELLATOR_PARTITIONING_UNDEFINED       = 0,
	TESSELLATOR_PARTITIONING_INTEGER         = 1,
	TESSELLATOR_PARTITIONING_POW2            = 2,
	TESSELLATOR_PARTITIONING_FRACTIONAL_ODD  = 3,
	TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN = 4
} TESSELLATOR_PARTITIONING;

typedef enum TESSELLATOR_OUTPUT_PRIMITIVE
{
	TESSELLATOR_OUTPUT_UNDEFINED     = 0,
	TESSELLATOR_OUTPUT_POINT         = 1,
	TESSELLATOR_OUTPUT_LINE          = 2,
	TESSELLATOR_OUTPUT_TRIANGLE_CW   = 3,
	TESSELLATOR_OUTPUT_TRIANGLE_CCW  = 4
} TESSELLATOR_OUTPUT_PRIMITIVE;

typedef enum INTERPOLATION_MODE
{
	INTERPOLATION_UNDEFINED = 0,
	INTERPOLATION_CONSTANT = 1,
	INTERPOLATION_LINEAR = 2,
	INTERPOLATION_LINEAR_CENTROID = 3,
	INTERPOLATION_LINEAR_NOPERSPECTIVE = 4,
	INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID = 5,
	INTERPOLATION_LINEAR_SAMPLE = 6,
	INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE = 7,
} INTERPOLATION_MODE;

typedef enum TRACE_VARIABLE_GROUP
{
	TRACE_VARIABLE_INPUT  = 0,
	TRACE_VARIABLE_TEMP   = 1,
	TRACE_VARIABLE_OUTPUT = 2
} TRACE_VARIABLE_GROUP;

typedef enum TRACE_VARIABLE_TYPE
{
	TRACE_VARIABLE_FLOAT   = 0,
	TRACE_VARIABLE_SINT    = 1,
	TRACE_VARIABLE_UINT    = 2,
	TRACE_VARIABLE_DOUBLE  = 3,
	TRACE_VARIABLE_BOOL    = 4,
	TRACE_VARIABLE_UNKNOWN = 5
} TRACE_VARIABLE_TYPE;

typedef struct VariableTraceInfo_TAG
{
	TRACE_VARIABLE_GROUP eGroup;
	TRACE_VARIABLE_TYPE eType;
	uint8_t ui8Index;
	uint8_t ui8Component;
} VariableTraceInfo;

typedef struct StepTraceInfo_TAG
{
	uint32_t ui32NumVariables;
	VariableTraceInfo* psVariables;
} StepTraceInfo;

typedef enum SYMBOL_TYPE
{
	SYMBOL_TESSELLATOR_PARTITIONING = 0,
	SYMBOL_TESSELLATOR_OUTPUT_PRIMITIVE = 1,
	SYMBOL_INPUT_INTERPOLATION_MODE = 2,
	SYMBOL_EMULATE_DEPTH_CLAMP = 3,
	SYMBOL_TRACE_SHADER = 4
} SYMBOL_TYPE;

typedef struct Symbol_TAG
{
	SYMBOL_TYPE eType;
	uint32_t ui32ID;
	uint32_t ui32Value;
} Symbol;

typedef struct EmbeddedResourceName_TAG
{
	uint32_t ui20Offset           : 20;
	uint32_t ui12Size             : 12;
} EmbeddedResourceName;

typedef struct TextureSamplerPair_TAG
{
	char Name[MAX_REFLECT_STRING_LENGTH];
} TextureSamplerPair;

typedef struct TextureSamplerInfo_TAG
{
	uint32_t ui32NumTextureSamplerPairs;
	TextureSamplerPair aTextureSamplerPair[MAX_RESOURCE_BINDINGS];
} TextureSamplerInfo;

typedef struct SamplerMask_TAG
{
	uint32_t ui10TextureBindPoint : 10;
	uint32_t ui10SamplerBindPoint : 10;
	uint32_t ui10TextureUnit      : 10;
	uint32_t bNormalSample        :  1;
	uint32_t bCompareSample       :  1;
} SamplerMask;

typedef struct Sampler_TAG
{
	SamplerMask sMask;
	EmbeddedResourceName sNormalName;
	EmbeddedResourceName sCompareName;
} Sampler;

typedef struct Resource_TAG
{
	uint32_t ui32BindPoint;
	ResourceGroup eGroup;
	EmbeddedResourceName sName;
} Resource;

typedef struct ShaderInfo_TAG
{
	uint32_t ui32MajorVersion;
	uint32_t ui32MinorVersion;

	uint32_t ui32NumInputSignatures;
	InOutSignature* psInputSignatures;

	uint32_t ui32NumOutputSignatures;
	InOutSignature* psOutputSignatures;

	uint32_t ui32NumPatchConstantSignatures;
	InOutSignature* psPatchConstantSignatures;

	uint32_t ui32NumResourceBindings;
	ResourceBinding* psResourceBindings;

	uint32_t ui32NumConstantBuffers;
	ConstantBuffer* psConstantBuffers;
	ConstantBuffer* psThisPointerConstBuffer;

	uint32_t ui32NumClassTypes;
	ClassType* psClassTypes;

	uint32_t ui32NumClassInstances;
	ClassInstance* psClassInstances;

	//Func table ID to class name ID.
	uint32_t aui32TableIDToTypeID[MAX_FUNCTION_TABLES];

	uint32_t aui32ResourceMap[RGROUP_COUNT][MAX_RESOURCE_BINDINGS];

	// GLSL resources
	Sampler asSamplers[MAX_RESOURCE_BINDINGS];
	Resource asImages[MAX_RESOURCE_BINDINGS];
	Resource asUniformBuffers[MAX_RESOURCE_BINDINGS];
	Resource asStorageBuffers[MAX_RESOURCE_BINDINGS];
	uint32_t ui32NumSamplers;
	uint32_t ui32NumImages;
	uint32_t ui32NumUniformBuffers;
	uint32_t ui32NumStorageBuffers;
	// Texture index to sampler slot
	uint32_t aui32SamplerMap[MAX_RESOURCE_BINDINGS];

	// Trace info if tracing is enabled
	uint32_t ui32NumTraceSteps;
	StepTraceInfo* psTraceSteps;

	// Symbols imported
	uint32_t ui32NumImports;
	Symbol* psImports;

	// Symbols exported
	uint32_t ui32NumExports;
	Symbol* psExports;

	// Hash of the input shader for debugging purposes
	uint32_t ui32InputHash;

	// Offset in the GLSL string where symbol definitions can be inserted
	uint32_t ui32SymbolsOffset;

	TESSELLATOR_PARTITIONING eTessPartitioning;
	TESSELLATOR_OUTPUT_PRIMITIVE eTessOutPrim;
} ShaderInfo;

//The shader stages (Vertex, Pixel et al) do not depend on each other
//in HLSL. GLSL is a different story. HLSLCrossCompiler requires
//that hull shaders must be compiled before domain shaders, and
//the pixel shader must be compiled before all of the others.
//Durring compiliation the GLSLCrossDependencyData struct will
//carry over any information needed about a different shader stage
//in order to construct valid GLSL shader combinations.

//Using GLSLCrossDependencyData is optional. However some shader
//combinations may show link failures, or runtime errors.
typedef struct
{
    //dcl_tessellator_partitioning and dcl_tessellator_output_primitive appear in hull shader for D3D,
    //but they appear on inputs inside domain shaders for GL.
    //Hull shader must be compiled before domain so the
    //ensure correct partitioning and primitive type information
    //can be saved when compiling hull and passed to domain compiliation.
    TESSELLATOR_PARTITIONING eTessPartitioning;
    TESSELLATOR_OUTPUT_PRIMITIVE eTessOutPrim;

    //Required if PixelInterpDependency is true
    INTERPOLATION_MODE aePixelInputInterpolation[MAX_SHADER_VEC4_INPUT];
} GLSLCrossDependencyData;

typedef struct
{
	int shaderType; //One of the GL enums.
	char* sourceCode;
	ShaderInfo reflection;
	GLLang GLSLLanguage;

	TextureSamplerInfo textureSamplerInfo;    // HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS fills this out
} GLSLShader;

/*HLSL constant buffers are treated as default-block unform arrays by default. This is done
  to support versions of GLSL which lack ARB_uniform_buffer_object functionality.
  Setting this flag causes each one to have its own uniform block.
  Note: Currently the nth const buffer will be named UnformBufferN. This is likey to change to the original HLSL name in the future.*/
static const unsigned int HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT = 0x1;

static const unsigned int HLSLCC_FLAG_ORIGIN_UPPER_LEFT = 0x2;

static const unsigned int HLSLCC_FLAG_PIXEL_CENTER_INTEGER = 0x4;

static const unsigned int HLSLCC_FLAG_GLOBAL_CONSTS_NEVER_IN_UBO = 0x8;

//GS enabled?
//Affects vertex shader (i.e. need to compile vertex shader again to use with/without GS).
//This flag is needed in order for the interfaces between stages to match when GS is in use.
//PS inputs VtxGeoOutput
//GS outputs VtxGeoOutput
//Vs outputs VtxOutput if GS enabled. VtxGeoOutput otherwise.
static const unsigned int HLSLCC_FLAG_GS_ENABLED = 0x10;

static const unsigned int HLSLCC_FLAG_TESS_ENABLED = 0x20;

//Either use this flag or glBindFragDataLocationIndexed.
//When set the first pixel shader output is the first input to blend
//equation, the others go to the second input.
static const unsigned int HLSLCC_FLAG_DUAL_SOURCE_BLENDING = 0x40;

//If set, shader inputs and outputs are declared with their semantic name.
static const unsigned int HLSLCC_FLAG_INOUT_SEMANTIC_NAMES = 0x80;
//If set, shader inputs and outputs are declared with their semantic name appended.
static const unsigned int HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES = 0x100;

//If set, combines texture/sampler pairs used together into samplers named "texturename_X_samplername".
static const unsigned int HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS = 0x200;

//If set, attribute and uniform explicit location qualifiers are disabled (even if the language version supports that)
static const unsigned int HLSLCC_FLAG_DISABLE_EXPLICIT_LOCATIONS = 0x400;

//If set, global uniforms are not stored in a struct.
static const unsigned int HLSLCC_FLAG_DISABLE_GLOBALS_STRUCT = 0x800;

static const unsigned int HLSLCC_FLAG_INVERT_CLIP_SPACE_Y = 0x1000;
static const unsigned int HLSLCC_FLAG_CONVERT_CLIP_SPACE_Z = 0x2000;
static const unsigned int HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS = 0x4000;
static const unsigned int HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING = 0x8000;
static const unsigned int HLSLCC_FLAG_TRACING_INSTRUMENTATION = 0x10000;
static const unsigned int HLSLCC_FLAG_HASH_INPUT = 0x20000;

static const unsigned int HLSLCC_FLAG_ADD_DEBUG_HEADER = 0x40000;
static const unsigned int HLSLCC_FLAG_NO_VERSION_STRING = 0x80000;
static const unsigned int HLSLCC_FLAG_MANGLE_IDENTIFIERS_PER_STAGE = 0x100000;
static const unsigned int HLSLCC_FLAG_FLOATS_AS_BITPATTERNS = 0x200000;
static const unsigned int HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS = 0x400000;

#ifdef __cplusplus
extern "C" {
#endif

HLSLCC_API void HLSLCC_APIENTRY HLSLcc_SetMemoryFunctions(void* (*malloc_override)(size_t),
                                                          void* (*calloc_override)(size_t,size_t),
                                                          void (*free_override)(void *),
                                                          void* (*realloc_override)(void*,size_t));

HLSLCC_API int HLSLCC_APIENTRY TranslateHLSLFromFile(const char* filename, unsigned int flags, GLLang language, const GlExtensions *extensions, GLSLCrossDependencyData* dependencies, GLSLShader* result);

HLSLCC_API int HLSLCC_APIENTRY TranslateHLSLFromMem(const char* shader, size_t size, unsigned int flags, GLLang language, const GlExtensions *extensions, GLSLCrossDependencyData* dependencies, GLSLShader* result);

HLSLCC_API void HLSLCC_APIENTRY FreeGLSLShader(GLSLShader*);

#ifdef __cplusplus
}
#endif

#endif

