// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CREMeshImpl : public CREMesh
{
public:

	uint       m_nPatchIDOffset;

	CREMeshImpl()
		: m_nPatchIDOffset(0)
	{}

	virtual ~CREMeshImpl()
	{}

public:
	virtual struct CRenderChunk* mfGetMatInfo() override;
	virtual TRenderChunkArray*   mfGetMatInfoList() override;
	virtual int                  mfGetMatId() override;
	virtual bool                 mfPreDraw(SShaderPass* sl) override;
	virtual bool                 mfIsHWSkinned() override
	{
		return (m_Flags & FCEF_SKINNED) != 0;
	}
	virtual void  mfGetPlane(Plane& pl) override;
	virtual void  mfPrepare(bool bCheckOverflow) override;
	virtual void  mfReset() override;
	virtual void  mfCenter(Vec3& Pos, CRenderObject* pObj) override;
	virtual bool  mfDraw(CShader* ef, SShaderPass* sfm) override;
	virtual void* mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags) override;
	virtual bool  mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation = false) override;
	virtual void  mfGetBBox(Vec3& vMins, Vec3& vMaxs) override;
	virtual void  mfPrecache(const SShaderItem& SH) override;
	virtual int   Size() override
	{
		int nSize = sizeof(*this);
		return nSize;
	}
	virtual void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	bool        BindRemappedSkinningData(uint32 guid);
#if !defined(_RELEASE)
	inline bool ValidateDraw(EShaderType shaderType);
#endif

	virtual bool          GetGeometryInfo(SGeometryInfo& geomInfo, bool bSupportTessellation = false) final;
	virtual InputLayoutHandle GetVertexFormat() const final;
	virtual bool          Compile(CRenderObject* pObj) final;
	virtual void          DrawToCommandList(CRenderObject* pObj, const SGraphicsPipelinePassContext& ctx) final;

	//protected:
	//	CREMeshImpl(CREMeshImpl&);
	//	CREMeshImpl& operator=(CREMeshImpl&);
};