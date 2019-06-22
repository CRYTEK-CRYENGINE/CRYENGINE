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
	EStreamMasks CombineVertexStreamMasks(EStreamMasks fromShader, EStreamMasks fromObject) const;

public:
	_smart_ptr<CShader>        m_pShader;
	CDeviceResourceLayoutPtr   m_pResourceLayout;
	CDeviceRenderPassPtr       m_pRenderPass;
	CCryNameTSCRC              m_technique;
	uint64                     m_ShaderFlags_RT;
	uint32                     m_ShaderFlags_MD;
	EVertexModifier            m_ShaderFlags_MDV;
	ERenderPrimitiveType       m_PrimitiveType;

	uint32                     m_RenderState;
	uint32                     m_StencilState;
	uint8                      m_StencilReadMask;
	uint8                      m_StencilWriteMask;
	EStreamMasks               m_ObjectStreamMask;
	InputLayoutHandle          m_VertexFormat;
	EShaderQuality             m_ShaderQuality;
	ECull                      m_CullMode;
	bool                       m_bAllowTesselation : 1;
	bool                       m_bDepthClip : 1;
	bool                       m_bDepthBoundsTest : 1;
	bool                       m_bRelaxedRasterizationOrder : 1;
	bool                       m_bDynamicDepthBias : 1; // When clear, SetDepthBias() may be ignored by the PSO. This may be faster on PS4 and VK. It has no effect on DX11 (always on) and DX12 (always off).
};

class CDeviceComputePSODesc
{
public:
	CDeviceComputePSODesc(const CDeviceComputePSODesc& other);
	CDeviceComputePSODesc(CDeviceResourceLayoutPtr pResourceLayout, CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags);

	CDeviceComputePSODesc& operator=(const CDeviceComputePSODesc& other);
	bool                   operator==(const CDeviceComputePSODesc& other) const;

	uint64                 GetHash() const;

public:
	void InitWithDefaults();

public:
	_smart_ptr<CShader>      m_pShader;
	CDeviceResourceLayoutPtr m_pResourceLayout;
	CCryNameTSCRC            m_technique;
	uint64                   m_ShaderFlags_RT;
	uint32                   m_ShaderFlags_MD;
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

class CDevicePSO
{
public:
	bool   IsValid() const { return m_isValid; }
	uint32 GetUpdateCount() const { return m_updateCount; }
	int    GetLastUseFrame() const { return m_frameLastUsed; }
	void   SetLastUseFrame(int frameLastUsed) { m_frameLastUsed = frameLastUsed; }

protected:
	bool   m_isValid       = false;
	uint32 m_updateCount   =  0;
	int    m_frameLastUsed = -1;
};

class CDeviceGraphicsPSO : public CDevicePSO
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
	virtual ~CDeviceGraphicsPSO() {}

	virtual EInitResult Init(const CDeviceGraphicsPSODesc& psoDesc) = 0;

	std::array<void*, eHWSC_Num>          m_pHwShaderInstances;

#if defined(ENABLE_PROFILING_CODE)
	ERenderPrimitiveType m_PrimitiveTypeForProfiling;
#endif
};

class CDeviceComputePSO : public CDevicePSO
{
public:
	virtual ~CDeviceComputePSO() {}

	virtual bool Init(const CDeviceComputePSODesc& psoDesc) = 0;

	void*          m_pHwShaderInstance = nullptr;
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
