// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/ISplines.h>
#include <CryPhysics/IDeferredCollisionEvent.h>
#include <Cry3DEngine/IIndexedMesh.h>

class CRopeRenderNode final : public IRopeRenderNode, public Cry3DEngineBase
{
public:
	static void StaticReset();
	static int  OnPhysStateChange(EventPhys const* pEvent);

public:
	//////////////////////////////////////////////////////////////////////////
	// implements IRenderNode
	virtual void             GetLocalBounds(AABB& bbox) const override;
	virtual void             SetMatrix(const Matrix34& mat) override;

	virtual EERType          GetRenderNodeType() const override { return eERType_Rope; }
	virtual const char*      GetEntityClassName() const override;
	virtual const char*      GetName() const override;
	virtual Vec3             GetPos(bool bWorldOnly = true) const override;
	virtual void             Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo) override;
	virtual IPhysicalEntity* GetPhysics() const override;
	virtual void             SetPhysics(IPhysicalEntity*) override;
	virtual void             Physicalize(bool bInstant = false) override;
	virtual void             Dephysicalize(bool bKeepIfReferenced = false) override;
	virtual void             SetMaterial(IMaterial* pMat) override;
	virtual IMaterial*       GetMaterial(Vec3* pHitPos = 0) const override;
	virtual IMaterial*       GetMaterialOverride() const override { return m_pMaterial; }
	virtual float            GetMaxViewDist() const override;
	virtual void             Precache() override;
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual void             LinkEndPoints() override;
	virtual const AABB       GetBBox() const override             { return m_WSBBox; }
	virtual void             SetBBox(const AABB& WSBBox) override { m_WSBBox = WSBBox; m_bNeedToReRegister = true; }
	virtual void             FillBBox(AABB& aabb) const override { aabb = GetBBox(); }
	virtual void             OffsetPosition(const Vec3& delta) override;

	// Set a new owner entity
	virtual void             SetOwnerEntity(IEntity* pEntity) override { m_pEntity = pEntity; }
	// Retrieve a pointer to the entity who owns this render node.
	virtual IEntity*         GetOwnerEntity() const override { return m_pEntity; }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IRopeRenderNode implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void                                SetName(const char* sNodeName) override;
	virtual void                                SetParams(const SRopeParams& params) override;
	virtual const IRopeRenderNode::SRopeParams& GetParams() const override;

	virtual void                                SetPoints(const Vec3* pPoints, int nCount) override;
	virtual int                                 GetPointsCount() const override;
	virtual const Vec3*                         GetPoints() const override;

	virtual uint32                              GetLinkedEndsMask() override { return m_nLinkedEndsMask; }
	virtual void                                OnPhysicsPostStep() override;

	virtual void                                ResetPoints() override;

	virtual void                                LinkEndEntities(IPhysicalEntity* pStartEntity, IPhysicalEntity* pEndEntity) override;
	virtual void                                GetEndPointLinks(SEndPointLink* links) override;

	virtual void                                DisableAudio() override;
	virtual void                                SetAudioParams(SRopeAudioParams const& audioParams) override;
	//////////////////////////////////////////////////////////////////////////

public:
	CRopeRenderNode();

	void CreateRenderMesh();
	void UpdateRenderMesh();
	void AnchorEndPoints(pe_params_rope& pr);
	void SyncWithPhysicalRope(bool bForce);
	bool RenderDebugInfo(const SRendParams& rParams, const SRenderingPassInfo& passInfo);

private:
	~CRopeRenderNode();

	void UpdateAudio();

private:
	string                  m_sName;
	IEntity*                m_pEntity = nullptr;
	Vec3                    m_pos;
	AABB                    m_localBounds;
	Matrix34                m_worldTM;
	Matrix34                m_InvWorldTM;
	_smart_ptr<IMaterial>   m_pMaterial;
	_smart_ptr<IRenderMesh> m_pRenderMesh;
	IPhysicalEntity*        m_pPhysicalEntity;

	uint32                  m_nLinkedEndsMask;

	// Flags
	uint32            m_bModified                 : 1;
	uint32            m_bRopeCreatedInsideVisArea : 1;
	uint32            m_bNeedToReRegister         : 1;
	uint32            m_bStaticPhysics            : 1;

	std::vector<Vec3> m_points;
	std::vector<Vec3> m_physicsPoints;
	std::vector<Vec3> m_filteredPoints;

	std::vector<DualQuat>                m_bones;
	float                                m_lenSkin = 0;
	std::vector<SMeshBoneMapping_uint16> m_tmpSkin;
	std::vector< Vec3_tpl<vtx_idx> >     m_tmpIdx;
	_smart_ptr<IStatObj>                 m_segObj;
	SSkinningData*                       m_skinDataHist[3] = {};
	uint                                 m_idSkinFrame = ~0u;

	SRopeParams       m_params;
	bool              m_paramsChanged = true;

	typedef spline::CatmullRomSpline<Vec3> SplineType;
	SplineType         m_spline;

	AABB               m_WSBBox;

	CryAudio::IObject* m_pIAudioObject;
	SRopeAudioParams   m_audioParams;
};
