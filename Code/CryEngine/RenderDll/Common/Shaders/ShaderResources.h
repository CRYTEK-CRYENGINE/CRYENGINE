// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <atomic>

class CConstantBuffer;

//! This class provide all necessary resources to the shader extracted from material definition.
class CShaderResources : public IRenderShaderResources, public SBaseShaderResources
{
public:
	enum EFlags
	{
		eFlagInvalid             = BIT(0),
		eFlagRecreateResourceSet = BIT(1),
		EFlags_DynamicUpdates    = BIT(2),
		EFlags_AnimatedSequence  = BIT(3),
	};

public:
	stl::aligned_vector<Vec4, 256> m_Constants;
	SEfResTexture*                 m_Textures[EFTT_MAX];
	SDeformInfo*                   m_pDeformInfo;
	TArray<struct SHRenderTarget*> m_RTargets;
	CCamera*                       m_pCamera;
	SSkyInfo*                      m_pSky;
	SDetailDecalInfo*              m_pDetailDecalInfo;
	CConstantBufferPtr             m_pConstantBuffer;
	uint16                         m_Id;
	uint16                         m_IdGroup;

	// @see EFlags
	uint32            m_flags;

	/////////////////////////////////////////////////////

	float        m_fMinMipFactorLoad;
	int          m_nLastTexture;
	int          m_nFrameLoad;
	uint32       m_nUpdateFrameID;

	// Compiled resource set.
	CDeviceResourceSetDesc                                  m_resources;
	std::shared_ptr<class CDeviceResourceSet>               m_pCompiledResourceSet;
	std::shared_ptr<class CGraphicsPipelineStateLocalCache> m_pipelineStateCache;

	uint8 m_nMtlLayerNoDrawFlags;

public:
	//////////////////////////////////////////////////////////////////////////
	void AddTextureMap(int Id)
	{
		assert(Id >= 0 && Id < EFTT_MAX);
		m_Textures[Id] = new SEfResTexture;
	}
	int Size() const
	{
		int nSize = sizeof(CShaderResources);
		for (int i = 0; i < EFTT_MAX; i++)
		{
			if (m_Textures[i])
				nSize += m_Textures[i]->Size();
		}
		nSize += sizeofVector(m_Constants);
		nSize += m_RTargets.GetMemoryUsage();
		if (m_pDeformInfo)
			nSize += m_pDeformInfo->Size();
		if (m_pDetailDecalInfo)
			nSize += m_pDetailDecalInfo->Size();
		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const final
	{
		pSizer->AddObject(this, sizeof(*this));

		for (int i = 0; i < EFTT_MAX; i++)
		{
			pSizer->AddObject(m_Textures[i]);
		}
		pSizer->AddObject(m_Constants);
		pSizer->AddObject(m_RTargets);
		pSizer->AddObject(m_pDeformInfo);
		pSizer->AddObject(m_pDetailDecalInfo);
		SBaseShaderResources::GetMemoryUsage(pSizer);

	}

	CShaderResources();
	CShaderResources(struct SInputShaderResources* pSrc);

	void                               PostLoad(CShader* pSH);
	void                               AdjustForSpec();
	void                               CreateModifiers(SInputShaderResources* pInRes);
	virtual void                       UpdateConstants(IShader* pSH) final;
	virtual void                       CloneConstants(const IRenderShaderResources* pSrc) final;
	virtual int                        GetResFlags() final                       { return m_ResFlags; }
	virtual void                       SetMaterialName(const char* szName) final { m_szMaterialName = szName; }
	virtual CCamera*                   GetCamera() final                         { return m_pCamera; }
	virtual void                       SetCamera(CCamera* pCam) final            { m_pCamera = pCam; }
	virtual SSkyInfo*                  GetSkyInfo() final                        { return m_pSky; }
	virtual const float&               GetAlphaRef() const final                 { return m_AlphaRef; }
	virtual void                       SetAlphaRef(float alphaRef) final         { m_AlphaRef = alphaRef; }
	virtual SEfResTexture*             GetTexture(int nSlot) const final         { return m_Textures[nSlot]; }
	virtual DynArrayRef<SShaderParam>& GetParameters() final                     { return m_ShaderParams; }
	virtual ColorF                     GetFinalEmittance() final
	{
		const float kKiloScale = 1000.0f;
		return GetColorValue(EFTT_EMITTANCE) * GetStrengthValue(EFTT_EMITTANCE) * (kKiloScale / RENDERER_LIGHT_UNIT_SCALE);
	}
	virtual float GetVoxelCoverage() final                   { return ((float)m_VoxelCoverage) * (1.0f / 255.0f); }

	virtual void  SetMtlLayerNoDrawFlags(uint8 nFlags) final { m_nMtlLayerNoDrawFlags = nFlags; }
	virtual uint8 GetMtlLayerNoDrawFlags() const final       { return m_nMtlLayerNoDrawFlags; }

	void          RT_UpdateConstants(IShader* pSH);
	void          RT_UpdateResourceSet();
	void          ReleaseConstants();
	inline float  FurAmount()
	{
		return ((float)m_FurAmount) * (1.0f / 255.0f);
	}

	inline uint8 HeatAmount_unnormalized() const
	{
		return m_HeatAmount;
	}

	inline float HeatAmount() const
	{
		const float fHeatRange = MAX_HEATSCALE;
		return ((float)m_HeatAmount) * (1.0f / 255.0f) * fHeatRange; // rescale range
	}

	inline float CloakAmount()
	{
		return ((float)m_CloakAmount) * (1.0f / 255.0f);
	}

	void Reset()
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			m_Textures[i] = NULL;
		}

		m_Id = 0;
		m_IdGroup = 0;
		m_nLastTexture = 0;
		m_pDeformInfo = NULL;
		m_pDetailDecalInfo = NULL;
		m_pCamera = NULL;
		m_pSky = NULL;
		m_pConstantBuffer.reset();
		m_nMtlLayerNoDrawFlags = 0;
		m_flags = 0;
		m_nUpdateFrameID = 0;
		m_resources.ClearResources();
	}
	bool IsEmpty(int nTSlot) const
	{
		if (!m_Textures[nTSlot])
			return true;
		return false;
	}
	bool                  HasChanges() const { return !!(m_flags & (eFlagRecreateResourceSet | EFlags_AnimatedSequence | EFlags_DynamicUpdates)) || m_resources.HasChanged() || !m_pCompiledResourceSet; }
	bool                  HasDynamicTexModifiers() const;
	bool                  HasDynamicUpdates() const { return !!(m_flags & EFlags_DynamicUpdates); }
	bool                  HasAnimatedTextures() const { return !!(m_flags & EFlags_AnimatedSequence); }
	bool                  HasLMConstants() const { return (m_Constants.size() > 0); }
	virtual void          SetInputLM(const CInputLightMaterial& lm) final;
	virtual void          ToInputLM(CInputLightMaterial& lm) final;

	virtual const ColorF& GetColorValue(EEfResTextures slot) const final;
	virtual float         GetStrengthValue(EEfResTextures slot) const final;

	virtual void          SetColorValue(EEfResTextures slot, const ColorF& color) final;
	virtual void          SetStrengthValue(EEfResTextures slot, float value) final;

	virtual void          SetInvalid() final;
	// Check if shader resource is valid
	virtual bool          IsValid() { return 0 == (m_flags & eFlagInvalid); };

	~CShaderResources();
	void                      RT_Release();
	virtual void              Release() const final;
	virtual void              AddRef() const final { SBaseShaderResources::AddRef(); }
	virtual void              ConvertToInputResource(SInputShaderResources* pDst) final;
	virtual CShaderResources* Clone() const final;
	virtual void              SetShaderParams(SInputShaderResources* pDst, IShader* pSH) final;

	virtual size_t            GetResourceMemoryUsage(ICrySizer* pSizer) final;
	virtual SDetailDecalInfo* GetDetailDecalInfo() final { return m_pDetailDecalInfo; }

	void                      Cleanup();

	void                      ClearPipelineStateCache();

private:
	CShaderResources(const CShaderResources& src);
};
typedef _smart_ptr<CShaderResources> CShaderResourcesPtr;
