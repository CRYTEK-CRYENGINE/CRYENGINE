// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLShaderReflection.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrappers for D3D11 shader
//               reflection interfaces
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLShaderReflection.hpp"
#include "../Implementation/GLShader.hpp"

////////////////////////////////////////////////////////////////////////////////
// CCryDXGLShaderReflectionVariable
////////////////////////////////////////////////////////////////////////////////

struct CCryDXGLShaderReflectionVariable::Impl
{
	NCryOpenGL::SShaderReflectionVariable* m_pVariable;
};

CCryDXGLShaderReflectionVariable::CCryDXGLShaderReflectionVariable()
	: m_pImpl(new Impl())
{
	DXGL_INITIALIZE_INTERFACE(D3D11ShaderReflectionVariable)
	DXGL_INITIALIZE_INTERFACE(D3D11ShaderReflectionType)
}

CCryDXGLShaderReflectionVariable::~CCryDXGLShaderReflectionVariable()
{
	delete m_pImpl;
}

bool CCryDXGLShaderReflectionVariable::Initialize(void* pvData)
{
	m_pImpl->m_pVariable = static_cast<NCryOpenGL::SShaderReflectionVariable*>(pvData);
	return true;
}

HRESULT CCryDXGLShaderReflectionVariable::GetDesc(D3D11_SHADER_VARIABLE_DESC* pDesc)
{
	(*pDesc) = m_pImpl->m_pVariable->m_kDesc;
	return S_OK;
}

ID3D11ShaderReflectionType* CCryDXGLShaderReflectionVariable::GetType()
{
	ID3D11ShaderReflectionType* pType;
	ToInterface(&pType, this);
	return pType;
}

ID3D11ShaderReflectionConstantBuffer* CCryDXGLShaderReflectionVariable::GetBuffer()
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

UINT CCryDXGLShaderReflectionVariable::GetInterfaceSlot(UINT uArrayIndex)
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}

HRESULT CCryDXGLShaderReflectionVariable::GetDesc(D3D11_SHADER_TYPE_DESC* pDesc)
{
	(*pDesc) = m_pImpl->m_pVariable->m_kType;
	return S_OK;
}

ID3D11ShaderReflectionType* CCryDXGLShaderReflectionVariable::GetMemberTypeByIndex(UINT Index)
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

ID3D11ShaderReflectionType* CCryDXGLShaderReflectionVariable::GetMemberTypeByName(LPCSTR Name)
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

LPCSTR CCryDXGLShaderReflectionVariable::GetMemberTypeName(UINT Index)
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

HRESULT CCryDXGLShaderReflectionVariable::IsEqual(ID3D11ShaderReflectionType* pType)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

ID3D11ShaderReflectionType* CCryDXGLShaderReflectionVariable::GetSubType()
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

ID3D11ShaderReflectionType* CCryDXGLShaderReflectionVariable::GetBaseClass()
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

UINT CCryDXGLShaderReflectionVariable::GetNumInterfaces()
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}

ID3D11ShaderReflectionType* CCryDXGLShaderReflectionVariable::GetInterfaceByIndex(UINT uIndex)
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

HRESULT CCryDXGLShaderReflectionVariable::IsOfType(ID3D11ShaderReflectionType* pType)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT CCryDXGLShaderReflectionVariable::ImplementsInterface(ID3D11ShaderReflectionType* pBase)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////
// CCryDXGLShaderReflectionConstBuffer
////////////////////////////////////////////////////////////////////////////////

struct CCryDXGLShaderReflectionConstBuffer::Impl
{
	typedef std::vector<_smart_ptr<CCryDXGLShaderReflectionVariable>> TVariables;
	TVariables m_kVariables;
	NCryOpenGL::SShaderReflectionConstBuffer* m_pConstBuffer;
};

CCryDXGLShaderReflectionConstBuffer::CCryDXGLShaderReflectionConstBuffer()
	: m_pImpl(new Impl())
{
	DXGL_INITIALIZE_INTERFACE(D3D11ShaderReflectionConstantBuffer)
}

CCryDXGLShaderReflectionConstBuffer::~CCryDXGLShaderReflectionConstBuffer()
{
	delete m_pImpl;
}

bool CCryDXGLShaderReflectionConstBuffer::Initialize(void* pvData)
{
	m_pImpl->m_pConstBuffer = static_cast<NCryOpenGL::SShaderReflectionConstBuffer*>(pvData);

	NCryOpenGL::SShaderReflectionConstBuffer::TVariables::iterator kVarIter(m_pImpl->m_pConstBuffer->m_kVariables.begin());
	const NCryOpenGL::SShaderReflectionConstBuffer::TVariables::iterator kVarEnd(m_pImpl->m_pConstBuffer->m_kVariables.end());
	while (kVarIter != kVarEnd)
	{
		_smart_ptr<CCryDXGLShaderReflectionVariable> spVariable(new CCryDXGLShaderReflectionVariable());
		m_pImpl->m_kVariables.push_back(spVariable);

		if (!spVariable->Initialize(static_cast<void*>(&*kVarIter)))
			return false;
		++kVarIter;
	}
	return true;
}

HRESULT CCryDXGLShaderReflectionConstBuffer::GetDesc(D3D11_SHADER_BUFFER_DESC* pDesc)
{
	(*pDesc) = m_pImpl->m_pConstBuffer->m_kDesc;
	return S_OK;
}

ID3D11ShaderReflectionVariable* CCryDXGLShaderReflectionConstBuffer::GetVariableByIndex(UINT Index)
{
	if (Index >= m_pImpl->m_kVariables.size())
		return NULL;
	ID3D11ShaderReflectionVariable* pVariable;
	CCryDXGLShaderReflectionVariable::ToInterface(&pVariable, m_pImpl->m_kVariables.at(Index));
	return pVariable;
}

ID3D11ShaderReflectionVariable* CCryDXGLShaderReflectionConstBuffer::GetVariableByName(LPCSTR Name)
{
	Impl::TVariables::const_iterator kVarIter(m_pImpl->m_kVariables.begin());
	const Impl::TVariables::const_iterator kVarEnd(m_pImpl->m_kVariables.end());
	for (; kVarIter != kVarEnd; ++kVarIter)
	{
		D3D11_SHADER_VARIABLE_DESC kDesc;
		if (FAILED((*kVarIter)->GetDesc(&kDesc)))
			return NULL;
		if (strcmp(kDesc.Name, Name) == 0)
		{
			ID3D11ShaderReflectionVariable* pVariable;
			CCryDXGLShaderReflectionVariable::ToInterface(&pVariable, kVarIter->get());
			return pVariable;
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// CCryDXGLShaderReflection
////////////////////////////////////////////////////////////////////////////////

struct CCryDXGLShaderReflection::Impl
{
	struct SResource
	{
		NCryOpenGL::SShaderReflectionResource* m_pResource;
	};

	struct SParameter
	{
		NCryOpenGL::SShaderReflectionParameter* m_pParameter;
	};

	typedef std::vector<_smart_ptr<CCryDXGLShaderReflectionConstBuffer>> TConstantBuffers;

	TConstantBuffers              m_kConstantBuffers;

	NCryOpenGL::SShaderReflection m_kReflection;
};

CCryDXGLShaderReflection::CCryDXGLShaderReflection()
	: m_pImpl(new Impl())
{
	DXGL_INITIALIZE_INTERFACE(D3D11ShaderReflection)
}

CCryDXGLShaderReflection::~CCryDXGLShaderReflection()
{
	delete m_pImpl;
}

bool CCryDXGLShaderReflection::Initialize(const void* pvData)
{
	if (!InitializeShaderReflectionFromInput(&m_pImpl->m_kReflection, pvData))
		return false;

	NCryOpenGL::SShaderReflection::TConstantBuffers::iterator kConstBufferIter(m_pImpl->m_kReflection.m_kConstantBuffers.begin());
	const NCryOpenGL::SShaderReflection::TConstantBuffers::iterator kConstBufferEnd(m_pImpl->m_kReflection.m_kConstantBuffers.end());
	while (kConstBufferIter != kConstBufferEnd)
	{
		_smart_ptr<CCryDXGLShaderReflectionConstBuffer> spConstBuffer(new CCryDXGLShaderReflectionConstBuffer());
		if (!spConstBuffer->Initialize(static_cast<void*>(&*kConstBufferIter)))
			return false;
		m_pImpl->m_kConstantBuffers.push_back(spConstBuffer);
		++kConstBufferIter;
	}
	return true;
}

HRESULT CCryDXGLShaderReflection::GetDesc(D3D11_SHADER_DESC* pDesc)
{
	(*pDesc) = m_pImpl->m_kReflection.m_kDesc;
	return S_OK;
}

ID3D11ShaderReflectionConstantBuffer* CCryDXGLShaderReflection::GetConstantBufferByIndex(UINT Index)
{
	if (Index >= m_pImpl->m_kConstantBuffers.size())
		return NULL;
	ID3D11ShaderReflectionConstantBuffer* pConstantBuffer;
	CCryDXGLShaderReflectionConstBuffer::ToInterface(&pConstantBuffer, m_pImpl->m_kConstantBuffers.at(Index));
	return pConstantBuffer;
}

ID3D11ShaderReflectionConstantBuffer* CCryDXGLShaderReflection::GetConstantBufferByName(LPCSTR Name)
{
	Impl::TConstantBuffers::const_iterator kCBIter(m_pImpl->m_kConstantBuffers.begin());
	const Impl::TConstantBuffers::const_iterator kCBEnd(m_pImpl->m_kConstantBuffers.end());
	for (; kCBIter != kCBEnd; ++kCBIter)
	{
		D3D11_SHADER_BUFFER_DESC kDesc;
		if (FAILED((*kCBIter)->GetDesc(&kDesc)))
			return NULL;
		if (strcmp(kDesc.Name, Name) == 0)
		{
			ID3D11ShaderReflectionConstantBuffer* pConstantBuffer;
			CCryDXGLShaderReflectionConstBuffer::ToInterface(&pConstantBuffer, kCBIter->get());
			return pConstantBuffer;
		}
	}
	return NULL;
}

HRESULT CCryDXGLShaderReflection::GetResourceBindingDesc(UINT ResourceIndex, D3D11_SHADER_INPUT_BIND_DESC* pDesc)
{
	if (ResourceIndex >= m_pImpl->m_kReflection.m_kResources.size())
		return E_FAIL;
	*pDesc = m_pImpl->m_kReflection.m_kResources[ResourceIndex].m_kDesc;
	return S_OK;
}

HRESULT CCryDXGLShaderReflection::GetInputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc)
{
	if (ParameterIndex >= m_pImpl->m_kReflection.m_kInputs.size())
		return E_FAIL;
	*pDesc = m_pImpl->m_kReflection.m_kInputs[ParameterIndex].m_kDesc;
	return S_OK;
}

HRESULT CCryDXGLShaderReflection::GetOutputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc)
{
	if (ParameterIndex >= m_pImpl->m_kReflection.m_kOutputs.size())
		return E_FAIL;
	*pDesc = m_pImpl->m_kReflection.m_kOutputs[ParameterIndex].m_kDesc;
	return S_OK;
}

HRESULT CCryDXGLShaderReflection::GetPatchConstantParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

ID3D11ShaderReflectionVariable* CCryDXGLShaderReflection::GetVariableByName(LPCSTR Name)
{
	DXGL_NOT_IMPLEMENTED
	return NULL;
}

HRESULT CCryDXGLShaderReflection::GetResourceBindingDescByName(LPCSTR Name, D3D11_SHADER_INPUT_BIND_DESC* pDesc)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

UINT CCryDXGLShaderReflection::GetMovInstructionCount()
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}

UINT CCryDXGLShaderReflection::GetMovcInstructionCount()
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}

UINT CCryDXGLShaderReflection::GetConversionInstructionCount()
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}

UINT CCryDXGLShaderReflection::GetBitwiseInstructionCount()
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}

D3D_PRIMITIVE CCryDXGLShaderReflection::GetGSInputPrimitive()
{
	DXGL_NOT_IMPLEMENTED
	return D3D_PRIMITIVE_TRIANGLE;
}

BOOL CCryDXGLShaderReflection::IsSampleFrequencyShader()
{
	DXGL_NOT_IMPLEMENTED
	return FALSE;
}

UINT CCryDXGLShaderReflection::GetNumInterfaceSlots()
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}

HRESULT CCryDXGLShaderReflection::GetMinFeatureLevel(enum D3D_FEATURE_LEVEL* pLevel)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

UINT CCryDXGLShaderReflection::GetThreadGroupSize(UINT* pSizeX, UINT* pSizeY, UINT* pSizeZ)
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}
