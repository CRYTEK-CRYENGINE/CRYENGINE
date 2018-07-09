// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

////////////////////////////////////////////////////////////////////////////
class CDeviceGraphicsPSODesc
{
public:
	CDeviceGraphicsPSODesc();
	CDeviceGraphicsPSODesc(const CDeviceGraphicsPSODesc& other);
	CDeviceGraphicsPSODesc(CDeviceResourceLayoutPtr pResourceLayout, const SGraphicsPipelineStateDescription& pipelineDesc);

	CDeviceGraphicsPSODesc& operator=(const CDeviceGraphicsPSODesc& other);
	bool                    operator==(const CDeviceGraphicsPSODesc& other) const;

	uint64                  GetHash() const;

public:
	void  InitWithDefaults();

	void  FillDescs(D3D11_RASTERIZER_DESC& rasterizerDesc, D3D11_BLEND_DESC& blendDesc, D3D11_DEPTH_STENCIL_DESC& depthStencilDesc) const;
	uint32 CombineVertexStreamMasks(uint32 fromShader, uint32 fromObject) const;

public:
	_smart_ptr<CShader>        m_pShader;
	CCryNameTSCRC              m_technique;
	uint64                     m_ShaderFlags_RT;
	uint32                     m_ShaderFlags_MD;
	uint32                     m_ShaderFlags_MDV;
	CDeviceResourceLayoutPtr   m_pResourceLayout;

	EShaderQuality             m_ShaderQuality;
	uint32                     m_RenderState;
	uint32                     m_StencilState;
	uint8                      m_StencilReadMask;
	uint8                      m_StencilWriteMask;
	InputLayoutHandle          m_VertexFormat;
	uint32                     m_ObjectStreamMask;
	ECull                      m_CullMode;
	ERenderPrimitiveType       m_PrimitiveType;
	CDeviceRenderPassPtr       m_pRenderPass;
	bool                       m_bAllowTesselation;
	bool                       m_bDepthClip;
	bool                       m_bDepthBoundsTest;
	bool                       m_bDynamicDepthBias; // When clear, SetDepthBias() may be ignored by the PSO. This may be faster on PS4 and VK. It has no effect on DX11 (always on) and DX12 (always off).
};

class CDeviceComputePSODesc
{
public:
	CDeviceComputePSODesc(const CDeviceComputePSODesc& other);
	CDeviceComputePSODesc(CDeviceResourceLayoutPtr pResourceLayout, CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags);

	CDeviceComputePSODesc& operator=(const CDeviceComputePSODesc& other);
	bool                   operator==(const CDeviceComputePSODesc& other) const;

	uint64                 GetHash() const;

public:
	void InitWithDefaults();

public:
	_smart_ptr<CShader>      m_pShader;
	CCryNameTSCRC            m_technique;
	uint64                   m_ShaderFlags_RT;
	uint32                   m_ShaderFlags_MD;
	uint32                   m_ShaderFlags_MDV;
	CDeviceResourceLayoutPtr m_pResourceLayout;
};

namespace std
{
	template<>
	struct hash<CDeviceGraphicsPSODesc>
	{
		uint64 operator()(const CDeviceGraphicsPSODesc& psoDesc) const
		{
			return psoDesc.GetHash();
		}
	};

	template<>
	struct equal_to<CDeviceGraphicsPSODesc>
	{
		bool operator()(const CDeviceGraphicsPSODesc& psoDesc1, const CDeviceGraphicsPSODesc& psoDesc2) const
		{
			return psoDesc1 == psoDesc2;
		}
	};

	template<>
	struct hash<CDeviceComputePSODesc>
	{
		uint64 operator()(const CDeviceComputePSODesc& psoDesc) const
		{
			return psoDesc.GetHash();
		}
	};

	template<>
	struct equal_to<CDeviceComputePSODesc>
	{
		bool operator()(const CDeviceComputePSODesc& psoDesc1, const CDeviceComputePSODesc& psoDesc2) const
		{
			return psoDesc1 == psoDesc2;
		}
	};

	template<>
	struct less<SDeviceResourceLayoutDesc>
	{
		bool operator()(const SDeviceResourceLayoutDesc& layoutDesc1, const SDeviceResourceLayoutDesc& layoutDesc2) const
		{
			return layoutDesc1 < layoutDesc2;
		}
	};
}

class CDeviceGraphicsPSO
{
public:
	enum class EInitResult : uint8
	{
		Success,
		Failure,
		ErrorShadersAndTopologyCombination,
	};

	static bool ValidateShadersAndTopologyCombination(const CDeviceGraphicsPSODesc& psoDesc, const std::array<void*, eHWSC_Num>& hwShaderInstances);

public:
	CDeviceGraphicsPSO()
		: m_bValid(false)
		, m_nUpdateCount(0)
	{}

	virtual ~CDeviceGraphicsPSO() {}

	virtual EInitResult Init(const CDeviceGraphicsPSODesc& psoDesc) = 0;
	bool                IsValid() const { return m_bValid; }
	uint32              GetUpdateCount() const { return m_nUpdateCount; }

	std::array<void*, eHWSC_Num>          m_pHwShaderInstances;

#if defined(ENABLE_PROFILING_CODE)
	ERenderPrimitiveType m_PrimitiveTypeForProfiling;
#endif

protected:
	bool   m_bValid;
	uint32 m_nUpdateCount;
};

class CDeviceComputePSO
{
public:
	CDeviceComputePSO()
		: m_bValid(false)
		, m_nUpdateCount(0)
		, m_pHwShaderInstance(nullptr)
	{}

	virtual ~CDeviceComputePSO() {}

	virtual bool Init(const CDeviceComputePSODesc& psoDesc) = 0;
	bool         IsValid() const { return m_bValid; }
	uint32       GetUpdateCount() const { return m_nUpdateCount; }

	void*          m_pHwShaderInstance;

protected:
	bool m_bValid;
	uint32 m_nUpdateCount;
};

typedef std::shared_ptr<const CDeviceGraphicsPSO> CDeviceGraphicsPSOConstPtr;
typedef std::weak_ptr<const CDeviceGraphicsPSO>   CDeviceGraphicsPSOConstWPtr;

typedef std::shared_ptr<const CDeviceComputePSO>  CDeviceComputePSOConstPtr;
typedef std::weak_ptr<const CDeviceComputePSO>    CDeviceComputePSOConstWPtr;

typedef std::shared_ptr<CDeviceGraphicsPSO>       CDeviceGraphicsPSOPtr;
typedef std::weak_ptr<CDeviceGraphicsPSO>         CDeviceGraphicsPSOWPtr;

typedef std::shared_ptr<CDeviceComputePSO>        CDeviceComputePSOPtr;
typedef std::weak_ptr<CDeviceComputePSO>          CDeviceComputePSOWPtr;

////////////////////////////////////////////////////////////////////////////
