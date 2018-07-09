// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AttachmentBase.h"
#include "Vertex/VertexData.h"
#include "Vertex/VertexAnimation.h"
#include "ModelSkin.h"

class CModelMesh;
struct SVertexAnimationJob;

//////////////////////////////////////////////////////////////////////
// Special value to indicate a Skinning Transformation job is still running
#define SKINNING_TRANSFORMATION_RUNNING_INDICATOR (reinterpret_cast<SSkinningData*>(static_cast<intptr_t>(-1)))

class CAttachmentSKIN : public IAttachmentSkin, public SAttachmentBase
{
public:

	CAttachmentSKIN()
	{
		for (uint32 j = 0; j < 2; ++j) m_pRenderMeshsSW[j] = NULL;
		memset(m_arrSkinningRendererData, 0, sizeof(m_arrSkinningRendererData));
		m_pModelSkin = 0;
	};

	~CAttachmentSKIN();

	virtual void AddRef() override
	{
		++m_nRefCounter;
	}

	virtual void Release() override
	{
		if (--m_nRefCounter == 0)
			delete this;
	}

	virtual uint32             GetType() const override                               { return CA_SKIN; }
	virtual uint32             SetJointName(const char* szJointName) override         { return 0; }

	virtual const char*        GetName() const override                               { return m_strSocketName; };
	virtual uint32             GetNameCRC() const override                            { return m_nSocketCRC32; }
	virtual uint32             ReName(const char* strSocketName, uint32 crc) override { m_strSocketName.clear();  m_strSocketName = strSocketName; m_nSocketCRC32 = crc;  return 1; };

	virtual uint32             GetFlags() const override                              { return m_AttFlags; }
	virtual void               SetFlags(uint32 flags) override                        { m_AttFlags = flags; }

	void                       ReleaseRemapTablePair();
	void                       ReleaseSoftwareRenderMeshes();

	virtual uint32             Immediate_AddBinding(IAttachmentObject* pModel, ISkin* pISkin = 0, uint32 nLoadingFlags = 0) override;
	virtual void               Immediate_ClearBinding(uint32 nLoadingFlags = 0) override;
	virtual uint32             Immediate_SwapBinding(IAttachment* pNewAttachment) override;

	virtual IAttachmentObject* GetIAttachmentObject() const override { return m_pIAttachmentObject; }
	virtual IAttachmentSkin*   GetIAttachmentSkin() override         { return this; }

	virtual void               HideAttachment(uint32 x) override;
	virtual uint32             IsAttachmentHidden() const override             { return m_AttFlags & FLAGS_ATTACH_HIDE_MAIN_PASS; }
	virtual void               HideInRecursion(uint32 x) override;
	virtual uint32             IsAttachmentHiddenInRecursion() const override  { return m_AttFlags & FLAGS_ATTACH_HIDE_RECURSION; }
	virtual void               HideInShadow(uint32 x) override;
	virtual uint32             IsAttachmentHiddenInShadow() const override     { return m_AttFlags & FLAGS_ATTACH_HIDE_SHADOW_PASS; }

	virtual void               SetAttAbsoluteDefault(const QuatT& qt) override {};
	virtual void               SetAttRelativeDefault(const QuatT& qt) override {};
	virtual const QuatT& GetAttAbsoluteDefault() const override          { return g_IdentityQuatT; };
	virtual const QuatT& GetAttRelativeDefault() const override          { return g_IdentityQuatT; };

	virtual const QuatT& GetAttModelRelative() const override            { return g_IdentityQuatT;  };//this is relative to the animated bone
	virtual const QuatTS GetAttWorldAbsolute() const override;
	virtual const QuatT& GetAdditionalTransformation() const override    { return g_IdentityQuatT; }
	virtual void         UpdateAttModelRelative() override;

	virtual uint32       GetJointID() const override     { return -1; };
	virtual void         AlignJointAttachment() override {};

	virtual void         Serialize(TSerialize ser) override;
	virtual size_t       SizeOfThis() const override;
	virtual void         GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual void         TriggerMeshStreaming(uint32 nDesiredRenderLOD, const SRenderingPassInfo& passInfo);

	void                 DrawAttachment(SRendParams& rParams, const SRenderingPassInfo& passInfo, const Matrix34& rWorldMat34, f32 fZoomFactor = 1);
	void                 RecreateDefaultSkeleton(CCharInstance* pInstanceSkel, uint32 nLoadingFlags);
	void                 UpdateRemapTable();

	// Vertex Transformation
public:
	SSkinningData*          GetVertexTransformationData(bool useSwSkinningCpu, uint8 nRenderLOD, const SRenderingPassInfo& passInfo);
	bool                    ShouldSwSkin() const     { return (m_AttFlags & FLAGS_ATTACH_SW_SKINNING) != 0; }
	bool                    ShouldSkinLinear() const { return (m_AttFlags & FLAGS_ATTACH_LINEAR_SKINNING) != 0; }
	_smart_ptr<IRenderMesh> CreateVertexAnimationRenderMesh(uint lod, uint id);
	void                    CullVertexFrames(const SRenderingPassInfo& passInfo, float fDistance);

#ifdef EDITOR_PCDEBUGCODE
	void DrawWireframeStatic(const Matrix34& m34, int nLOD, uint32 color);
	void SoftwareSkinningDQ_VS_Emulator(CModelMesh* pModelMesh, Matrix34 rRenderMat34, uint8 tang, uint8 binorm, uint8 norm, uint8 wire, const DualQuat* const pSkinningTransformations);
#endif

	//////////////////////////////////////////////////////////////////////////
	//IAttachmentSkin implementation
	//////////////////////////////////////////////////////////////////////////
	virtual IVertexAnimation* GetIVertexAnimation() override { return &m_vertexAnimation; }
	virtual ISkin*            GetISkin() override            { return m_pModelSkin; };
	virtual float             GetExtent(EGeomForm eForm) override;
	virtual void              GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const override;
	virtual SMeshLodInfo      ComputeGeometricMean() const override;

	int                       GetGuid() const;

	//---------------------------------------------------------

	//just for skin-attachments
	DynArray<JointIdType>   m_arrRemapTable; // maps skin's bone indices to skeleton's bone indices
	_smart_ptr<CSkin>       m_pModelSkin;
	_smart_ptr<IRenderMesh> m_pRenderMeshsSW[2];
	string                  m_sSoftwareMeshName;
	CVertexData             m_vertexData;
	CVertexAnimation        m_vertexAnimation;
	// history for skinning data, needed for motion blur
	struct { SSkinningData* pSkinningData; int nFrameID; } m_arrSkinningRendererData[3]; // triple buffered for motion blur

private:
	// function to keep in sync ref counts on skins and cleanup of remap tables
	void ReleaseModelSkin();

};
