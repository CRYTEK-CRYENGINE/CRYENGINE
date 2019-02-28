// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CREMeshImpl final : public CREMesh
{
public:

	uint       m_nPatchIDOffset;

	CREMeshImpl()
		: m_nPatchIDOffset(0)
	{}

	virtual ~CREMeshImpl()
	{}

public:
	struct CRenderChunk* mfGetMatInfo();
	TRenderChunkArray*   mfGetMatInfoList();
	int                  mfGetMatId();

	bool                 mfIsHWSkinned()
	{
		return (m_Flags & FCEF_SKINNED) != 0;
	}
	void  mfGetPlane(Plane& pl);

	void  mfCenter(Vec3& Pos, CRenderObject* pObj, const SRenderingPassInfo& passInfo);

	void* mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags);
	bool  mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation = false);
	void  mfGetBBox(Vec3& vMins, Vec3& vMaxs) const;

	int   Size()
	{
		int nSize = sizeof(*this);
		return nSize;
	}
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	bool          BindRemappedSkinningData(uint32 guid);

	bool          GetGeometryInfo(SGeometryInfo& geomInfo, bool bSupportTessellation = false);
	InputLayoutHandle GetVertexFormat() const;
	bool          Compile(CRenderObject* pObj,CRenderView *pRenderView, bool updateInstanceDataOnly);
	void          DrawToCommandList(CRenderObject* pObj, const SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList);

	//protected:
	//	CREMeshImpl(CREMeshImpl&);
	//	CREMeshImpl& operator=(CREMeshImpl&);
};