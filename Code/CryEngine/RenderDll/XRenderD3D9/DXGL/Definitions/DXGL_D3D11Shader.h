// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DXGL_D3D11Shader.h
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains portable definition of structs and enums to match
//               those in D3D11Shader.h in the DirectX SDK
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DXGL_D3D11Shader_h__
#define __DXGL_D3D11Shader_h__

#include "DXGL_D3DCommon.h"

////////////////////////////////////////////////////////////////////////////
//  Enums
////////////////////////////////////////////////////////////////////////////

typedef enum D3D11_SHADER_VERSION_TYPE
{
	D3D11_SHVER_PIXEL_SHADER    = 0,
	D3D11_SHVER_VERTEX_SHADER   = 1,
	D3D11_SHVER_GEOMETRY_SHADER = 2,

	// D3D11 Shaders
	D3D11_SHVER_HULL_SHADER    = 3,
	D3D11_SHVER_DOMAIN_SHADER  = 4,
	D3D11_SHVER_COMPUTE_SHADER = 5,

	D3D11_SHVER_RESERVED0      = 0xFFF0,
} D3D11_SHADER_VERSION_TYPE;

////////////////////////////////////////////////////////////////////////////
//  Structs
////////////////////////////////////////////////////////////////////////////

typedef struct _D3D11_SIGNATURE_PARAMETER_DESC
{
	LPCSTR                      SemanticName;    // Name of the semantic
	UINT                        SemanticIndex;   // Index of the semantic
	UINT                        Register;        // Number of member variables
	D3D_NAME                    SystemValueType; // A predefined system value, or D3D_NAME_UNDEFINED if not applicable
	D3D_REGISTER_COMPONENT_TYPE ComponentType;   // Scalar type (e.g. uint, float, etc.)
	BYTE                        Mask;            // Mask to indicate which components of the register
	                                             // are used (combination of D3D10_COMPONENT_MASK values)
	BYTE                        ReadWriteMask;   // Mask to indicate whether a given component is
	                                             // never written (if this is an output signature) or
	                                             // always read (if this is an input signature).
	                                             // (combination of D3D10_COMPONENT_MASK values)
	UINT              Stream;                    // Stream index
	D3D_MIN_PRECISION MinPrecision;              // Minimum desired interpolation precision
} D3D11_SIGNATURE_PARAMETER_DESC;

typedef struct _D3D11_SHADER_BUFFER_DESC
{
	LPCSTR           Name;                    // Name of the constant buffer
	D3D_CBUFFER_TYPE Type;                    // Indicates type of buffer content
	UINT             Variables;               // Number of member variables
	UINT             Size;                    // Size of CB (in bytes)
	UINT             uFlags;                  // Buffer description flags
} D3D11_SHADER_BUFFER_DESC;

typedef struct _D3D11_SHADER_VARIABLE_DESC
{
	LPCSTR Name;                              // Name of the variable
	UINT   StartOffset;                       // Offset in constant buffer's backing store
	UINT   Size;                              // Size of variable (in bytes)
	UINT   uFlags;                            // Variable flags
	LPVOID DefaultValue;                      // Raw pointer to default value
	UINT   StartTexture;                      // First texture index (or -1 if no textures used)
	UINT   TextureSize;                       // Number of texture slots possibly used.
	UINT   StartSampler;                      // First sampler index (or -1 if no textures used)
	UINT   SamplerSize;                       // Number of sampler slots possibly used.
} D3D11_SHADER_VARIABLE_DESC;

typedef struct _D3D11_SHADER_TYPE_DESC
{
	D3D_SHADER_VARIABLE_CLASS Class;              // Variable class (e.g. object, matrix, etc.)
	D3D_SHADER_VARIABLE_TYPE  Type;               // Variable type (e.g. float, sampler, etc.)
	UINT                      Rows;               // Number of rows (for matrices, 1 for other numeric, 0 if not applicable)
	UINT                      Columns;            // Number of columns (for vectors & matrices, 1 for other numeric, 0 if not applicable)
	UINT                      Elements;           // Number of elements (0 if not an array)
	UINT                      Members;            // Number of members (0 if not a structure)
	UINT                      Offset;             // Offset from the start of structure (0 if not a structure member)
	LPCSTR                    Name;               // Name of type, can be NULL
} D3D11_SHADER_TYPE_DESC;

typedef struct _D3D11_SHADER_DESC
{
	UINT                             Version;              // Shader version
	LPCSTR                           Creator;              // Creator string
	UINT                             Flags;                // Shader compilation/parse flags

	UINT                             ConstantBuffers;      // Number of constant buffers
	UINT                             BoundResources;       // Number of bound resources
	UINT                             InputParameters;      // Number of parameters in the input signature
	UINT                             OutputParameters;     // Number of parameters in the output signature

	UINT                             InstructionCount;            // Number of emitted instructions
	UINT                             TempRegisterCount;           // Number of temporary registers used
	UINT                             TempArrayCount;              // Number of temporary arrays used
	UINT                             DefCount;                    // Number of constant defines
	UINT                             DclCount;                    // Number of declarations (input + output)
	UINT                             TextureNormalInstructions;   // Number of non-categorized texture instructions
	UINT                             TextureLoadInstructions;     // Number of texture load instructions
	UINT                             TextureCompInstructions;     // Number of texture comparison instructions
	UINT                             TextureBiasInstructions;     // Number of texture bias instructions
	UINT                             TextureGradientInstructions; // Number of texture gradient instructions
	UINT                             FloatInstructionCount;       // Number of floating point arithmetic instructions used
	UINT                             IntInstructionCount;         // Number of signed integer arithmetic instructions used
	UINT                             UintInstructionCount;        // Number of unsigned integer arithmetic instructions used
	UINT                             StaticFlowControlCount;      // Number of static flow control instructions used
	UINT                             DynamicFlowControlCount;     // Number of dynamic flow control instructions used
	UINT                             MacroInstructionCount;       // Number of macro instructions used
	UINT                             ArrayInstructionCount;       // Number of array instructions used
	UINT                             CutInstructionCount;         // Number of cut instructions used
	UINT                             EmitInstructionCount;        // Number of emit instructions used
	D3D_PRIMITIVE_TOPOLOGY           GSOutputTopology;            // Geometry shader output topology
	UINT                             GSMaxOutputVertexCount;      // Geometry shader maximum output vertex count
	D3D_PRIMITIVE                    InputPrimitive;              // GS/HS input primitive
	UINT                             PatchConstantParameters;     // Number of parameters in the patch constant signature
	UINT                             cGSInstanceCount;            // Number of Geometry shader instances
	UINT                             cControlPoints;              // Number of control points in the HS->DS stage
	D3D_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive;           // Primitive output by the tessellator
	D3D_TESSELLATOR_PARTITIONING     HSPartitioning;              // Partitioning mode of the tessellator
	D3D_TESSELLATOR_DOMAIN           TessellatorDomain;           // Domain of the tessellator (quad, tri, isoline)
	// instruction counts
	UINT                             cBarrierInstructions;      // Number of barrier instructions in a compute shader
	UINT                             cInterlockedInstructions;  // Number of interlocked instructions
	UINT                             cTextureStoreInstructions; // Number of texture writes
} D3D11_SHADER_DESC;

typedef struct _D3D11_SHADER_INPUT_BIND_DESC
{
	LPCSTR                   Name;                // Name of the resource
	D3D_SHADER_INPUT_TYPE    Type;                // Type of resource (e.g. texture, cbuffer, etc.)
	UINT                     BindPoint;           // Starting bind point
	UINT                     BindCount;           // Number of contiguous bind points (for arrays)

	UINT                     uFlags;              // Input binding flags
	D3D_RESOURCE_RETURN_TYPE ReturnType;          // Return type (if texture)
	D3D_SRV_DIMENSION        Dimension;           // Dimension (if texture)
	UINT                     NumSamples;          // Number of samples (0 if not MS texture)
} D3D11_SHADER_INPUT_BIND_DESC;

////////////////////////////////////////////////////////////////////////////
//  Interfaces for full DX emulation
////////////////////////////////////////////////////////////////////////////

#if DXGL_FULL_EMULATION

struct ID3D11ShaderReflectionType
{
	virtual HRESULT STDMETHODCALLTYPE                     GetDesc(D3D11_SHADER_TYPE_DESC* pDesc) = 0;
	virtual ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByIndex(UINT Index) = 0;
	virtual ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByName(LPCSTR Name) = 0;
	virtual LPCSTR STDMETHODCALLTYPE                      GetMemberTypeName(UINT Index) = 0;
	virtual HRESULT STDMETHODCALLTYPE                     IsEqual(ID3D11ShaderReflectionType* pType) = 0;
	virtual ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetSubType() = 0;
	virtual ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetBaseClass() = 0;
	virtual UINT STDMETHODCALLTYPE                        GetNumInterfaces() = 0;
	virtual ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetInterfaceByIndex(UINT uIndex) = 0;
	virtual HRESULT STDMETHODCALLTYPE                     IsOfType(ID3D11ShaderReflectionType* pType) = 0;
	virtual HRESULT STDMETHODCALLTYPE                     ImplementsInterface(ID3D11ShaderReflectionType* pBase) = 0;
};

struct ID3D11ShaderReflectionConstantBuffer;

struct ID3D11ShaderReflectionVariable
{
	virtual HRESULT STDMETHODCALLTYPE                               GetDesc(D3D11_SHADER_VARIABLE_DESC* pDesc) = 0;
	virtual ID3D11ShaderReflectionType* STDMETHODCALLTYPE           GetType() = 0;
	virtual ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetBuffer() = 0;
	virtual UINT STDMETHODCALLTYPE                                  GetInterfaceSlot(UINT uArrayIndex) = 0;
};

struct ID3D11ShaderReflectionConstantBuffer
{
	virtual HRESULT STDMETHODCALLTYPE                         GetDesc(D3D11_SHADER_BUFFER_DESC* pDesc) = 0;
	virtual ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByIndex(UINT Index) = 0;
	virtual ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByName(LPCSTR Name) = 0;
};

struct ID3D11ShaderReflection : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE                               QueryInterface(REFIID iid, LPVOID* ppv) = 0;
	virtual ULONG STDMETHODCALLTYPE                                 AddRef() = 0;
	virtual ULONG STDMETHODCALLTYPE                                 Release() = 0;
	virtual HRESULT STDMETHODCALLTYPE                               GetDesc(D3D11_SHADER_DESC* pDesc) = 0;
	virtual ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByIndex(UINT Index) = 0;
	virtual ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByName(LPCSTR Name) = 0;
	virtual HRESULT STDMETHODCALLTYPE                               GetResourceBindingDesc(UINT ResourceIndex, D3D11_SHADER_INPUT_BIND_DESC* pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE                               GetInputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE                               GetOutputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE                               GetPatchConstantParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc) = 0;
	virtual ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE       GetVariableByName(LPCSTR Name) = 0;
	virtual HRESULT STDMETHODCALLTYPE                               GetResourceBindingDescByName(LPCSTR Name, D3D11_SHADER_INPUT_BIND_DESC* pDesc) = 0;
	virtual UINT STDMETHODCALLTYPE                                  GetMovInstructionCount() = 0;
	virtual UINT STDMETHODCALLTYPE                                  GetMovcInstructionCount() = 0;
	virtual UINT STDMETHODCALLTYPE                                  GetConversionInstructionCount() = 0;
	virtual UINT STDMETHODCALLTYPE                                  GetBitwiseInstructionCount() = 0;
	virtual D3D_PRIMITIVE STDMETHODCALLTYPE                         GetGSInputPrimitive() = 0;
	virtual BOOL STDMETHODCALLTYPE                                  IsSampleFrequencyShader() = 0;
	virtual UINT STDMETHODCALLTYPE                                  GetNumInterfaceSlots() = 0;
	virtual HRESULT STDMETHODCALLTYPE                               GetMinFeatureLevel(D3D_FEATURE_LEVEL* pLevel) = 0;
};

#endif //DXGL_FULL_EMULATION

#endif //__DXGL_D3D11Shader_h__
