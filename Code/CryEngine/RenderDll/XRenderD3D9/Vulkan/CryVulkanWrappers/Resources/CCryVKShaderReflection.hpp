// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Vulkan/API/VKBase.hpp"
#include <spirv_cross.hpp>

class CCryVKShaderReflection;
class CCryVKShaderReflectionVariable;
class CCryVKShaderReflectionConstantBuffer;
class CCryVKShaderReflectionType;
class CCryVKBlob;

typedef CCryVKBlob ID3DBlob;

//Global functions
HRESULT D3DReflect(const void* pShaderBytecode, size_t BytecodeLength, UINT pInterface, void** ppReflector);
HRESULT D3D10CreateBlob(size_t NumBytes, ID3DBlob** ppBuffer);
HRESULT D3DDisassemble(const void* pShader, size_t BytecodeLength, uint32 nFlags, ID3DBlob** ppComments, ID3DBlob** ppDisassembly);
HRESULT D3DCompile(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData, _In_ SIZE_T SrcDataSize, _In_opt_ LPCSTR pSourceName, CONST D3D_SHADER_MACRO* pDefines,
                   _In_opt_ ID3DInclude* pInclude, _In_opt_ LPCSTR pEntrypoint, _In_ LPCSTR pTarget, _In_ UINT Flags1, _In_ UINT Flags2, _Out_ ID3DBlob** ppCode,
                   _Always_(_Outptr_opt_result_maybenull_) ID3DBlob** ppErrorMsgs);

class CCryVKBlob : public NCryVulkan::CRefCounted
{
public:
	CCryVKBlob(size_t numBytes);
	virtual ~CCryVKBlob();

	virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() { return m_pData; }
	virtual SIZE_T STDMETHODCALLTYPE GetBufferSize()    { return m_size; }

private:
	uint8* m_pData = nullptr;
	size_t m_size = 0;
};

class CCryVKShaderReflectionType // Not ref-counted
{
public:
	CCryVKShaderReflectionType(CCryVKShaderReflection* pShaderReflection, uint32_t typeId);
	~CCryVKShaderReflectionType() {};

	HRESULT STDMETHODCALLTYPE GetDesc(D3D11_SHADER_TYPE_DESC* pDesc);

private:
	D3D11_SHADER_TYPE_DESC m_Desc;
};

class CCryVKShaderReflectionVariable // Not ref-counted
{
public:
	CCryVKShaderReflectionVariable(CCryVKShaderReflection* pShaderReflection, const spirv_cross::Resource& constantBuffer, uint32 memberIndex, bool bInUse);
	~CCryVKShaderReflectionVariable() {};

	HRESULT STDMETHODCALLTYPE                               GetDesc(D3D11_SHADER_VARIABLE_DESC* pDesc);
	ID3D11ShaderReflectionType* STDMETHODCALLTYPE           GetType();
	ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetBuffer();
	UINT STDMETHODCALLTYPE                                  GetInterfaceSlot(UINT uArrayIndex);

private:
	CCryVKShaderReflection* const               m_pShaderReflection;
	const spirv_cross::Resource&                m_constantBuffer;
	uint32                                      m_memberIndex;
	char                                        m_name[128];
	bool                                        m_bInUse;
	std::unique_ptr<CCryVKShaderReflectionType> m_pType;
};

class CCryVKShaderReflectionConstantBuffer // Not ref-counted
{
public:
	CCryVKShaderReflectionConstantBuffer(CCryVKShaderReflection* pShaderReflection, const spirv_cross::Resource& resource);
	~CCryVKShaderReflectionConstantBuffer() {};

	HRESULT STDMETHODCALLTYPE                         GetDesc(D3D11_SHADER_BUFFER_DESC* pDesc);
	ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByIndex(UINT Index);
	ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByName(LPCSTR Name);

private:
	char                                                         m_name[64];
	CCryVKShaderReflection* const                                m_pShaderReflection;
	const spirv_cross::Resource&                                 m_resource;
	std::vector<spirv_cross::BufferRange>                        m_usedVariables;
	std::vector<std::unique_ptr<CCryVKShaderReflectionVariable>> m_variables;
};

class CCryVKShaderReflection : public NCryVulkan::CRefCounted
{
	friend class CCryVKShaderReflectionVariable;
	friend class CCryVKShaderReflectionType;
	friend class CCryVKShaderReflectionConstantBuffer;

public:
	CCryVKShaderReflection(const void* pShaderBytecode, size_t BytecodeLength);
	virtual ~CCryVKShaderReflection() {}

	HRESULT STDMETHODCALLTYPE                               GetDesc(D3D11_SHADER_DESC* pDesc);
	ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByIndex(UINT Index);
	ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByName(LPCSTR Name);
	HRESULT STDMETHODCALLTYPE                               GetResourceBindingDesc(UINT ResourceIndex, D3D11_SHADER_INPUT_BIND_DESC* pDesc);
	HRESULT STDMETHODCALLTYPE                               GetInputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc);
	HRESULT STDMETHODCALLTYPE                               GetOutputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc);
	HRESULT STDMETHODCALLTYPE                               GetPatchConstantParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc);
	ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE       GetVariableByName(LPCSTR Name);
	HRESULT STDMETHODCALLTYPE                               GetResourceBindingDescByName(LPCSTR Name, D3D11_SHADER_INPUT_BIND_DESC* pDesc);
	UINT STDMETHODCALLTYPE                                  GetMovInstructionCount();
	UINT STDMETHODCALLTYPE                                  GetMovcInstructionCount();
	UINT STDMETHODCALLTYPE                                  GetConversionInstructionCount();
	UINT STDMETHODCALLTYPE                                  GetBitwiseInstructionCount();
	D3D_PRIMITIVE STDMETHODCALLTYPE                         GetGSInputPrimitive();
	BOOL STDMETHODCALLTYPE                                  IsSampleFrequencyShader();
	UINT STDMETHODCALLTYPE                                  GetNumInterfaceSlots();
	HRESULT STDMETHODCALLTYPE                               GetMinFeatureLevel(D3D_FEATURE_LEVEL* pLevel);

private:
	struct SInputParameter
	{
		char semanticName[64];
		int  semanticIndex;
		int  attributeLocation;
	};

	struct SResourceBinding
	{
		char semanticName[128];
		int  semanticType;
		int  bindPoint;
	};

	std::unique_ptr<spirv_cross::Compiler>                             m_pCompiler;
	spirv_cross::ShaderResources                                       m_shaderResources;
	std::vector<SInputParameter>                                       m_shaderInputs;
	std::vector<SResourceBinding>                                      m_shaderBindings;
	std::vector<std::unique_ptr<CCryVKShaderReflectionConstantBuffer>> m_constantBuffers;
};

