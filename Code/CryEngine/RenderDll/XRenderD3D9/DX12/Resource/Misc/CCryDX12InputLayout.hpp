// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Device/CCryDX12DeviceChild.hpp"

class CCryDX12InputLayout : public CCryDX12DeviceChild<ID3D11InputLayoutToImplement>
{
public:
	DX12_OBJECT(CCryDX12InputLayout, CCryDX12DeviceChild<ID3D11InputLayoutToImplement> );

	static CCryDX12InputLayout* Create(CCryDX12Device* pDevice, const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs11, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength);

	typedef std::vector<D3D12_INPUT_ELEMENT_DESC> TDescriptors;

	const TDescriptors& GetDescriptors() const
	{
		return m_Descriptors;
	}

	size_t GetInputSize(UINT inputSlot) const;

protected:
	CCryDX12InputLayout();

private:
	TDescriptors             m_Descriptors;

	std::vector<std::string> m_SemanticNames;
};
