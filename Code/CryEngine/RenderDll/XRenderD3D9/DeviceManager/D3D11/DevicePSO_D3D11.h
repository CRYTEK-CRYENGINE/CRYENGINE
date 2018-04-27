// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../../DeviceManager/DevicePSO.h"

class CDeviceGraphicsPSO_DX11 : public CDeviceGraphicsPSO
{
public:
	CDeviceGraphicsPSO_DX11();

	virtual EInitResult Init(const CDeviceGraphicsPSODesc& psoDesc) final;

	_smart_ptr<ID3D11RasterizerState>                 m_pRasterizerState;
	uint32                                            m_RasterizerStateIndex;
	_smart_ptr<ID3D11BlendState>                      m_pBlendState;
	_smart_ptr<ID3D11DepthStencilState>               m_pDepthStencilState;
	_smart_ptr<ID3D11InputLayout>                     m_pInputLayout;

	std::array<void*, eHWSC_Num>                      m_pDeviceShaders;

	std::array<std::array<uint8, MAX_TMU>, eHWSC_Num> m_Samplers;
	std::array<std::array<uint8, MAX_TMU>, eHWSC_Num> m_SRVs;

	std::array<uint8, eHWSC_Num>                      m_NumSamplers;
	std::array<uint8, eHWSC_Num>                      m_NumSRVs;

	// Do we still need these?
	uint64           m_ShaderFlags_RT;
	uint32           m_ShaderFlags_MD;
	uint32           m_ShaderFlags_MDV;

	D3DPrimitiveType m_PrimitiveTopology;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceComputePSO_DX11 : public CDeviceComputePSO
{
public:
	CDeviceComputePSO_DX11();

	virtual bool Init(const CDeviceComputePSODesc& psoDesc) final;

	std::array<void*, eHWSC_Num> m_pDeviceShaders;
};