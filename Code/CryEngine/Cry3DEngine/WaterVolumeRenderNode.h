// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/VertexFormats.h>
#include <CryRenderer/RenderElements/CREWaterVolume.h>

struct SWaterVolumeSerialize
{
	// volume type and id
	int32  m_volumeType;
	uint64 m_volumeID;

	// material
	IMaterial* m_pMaterial;

	// fog properties
	f32   m_fogDensity;
	Vec3  m_fogColor;
	bool  m_fogColorAffectedBySun;
	Plane m_fogPlane;
	f32   m_fogShadowing;

	f32   m_volumeDepth;
	f32   m_streamSpeed;
	bool  m_capFogAtVolumeDepth;

	// caustic properties
	bool m_caustics;
	f32  m_causticIntensity;
	f32  m_causticTiling;
	f32  m_causticHeight;

	// render geometry
	f32                  m_uTexCoordBegin;
	f32                  m_uTexCoordEnd;
	f32                  m_surfUScale;
	f32                  m_surfVScale;
	typedef std::vector<Vec3> VertexArraySerialize;
	VertexArraySerialize m_vertices;

	// physics properties
	typedef std::vector<Vec3> PhysicsAreaContourSerialize;
	PhysicsAreaContourSerialize m_physicsAreaContour;

	void                        GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_vertices);
		pSizer->AddObject(m_physicsAreaContour);
	}
};

struct SWaterVolumePhysAreaInput
{
	typedef std::vector<Vec3> PhysicsVertices;
	typedef std::vector<int>  PhysicsIndices;

	PhysicsVertices m_contour;
	PhysicsVertices m_flowContour;
	PhysicsIndices  m_indices;

	void            GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_contour);
		pSizer->AddObject(m_flowContour);
		pSizer->AddObject(m_indices);
	}
};

class CWaterVolumeRenderNode final : public IWaterVolumeRenderNode, public Cry3DEngineBase
{
public:
	// implements IWaterVolumeRenderNode
	virtual void             SetAreaAttachedToEntity() override;

	virtual void             SetFogDensity(float fogDensity) override;
	virtual float            GetFogDensity() const override;
	virtual void             SetFogColor(const Vec3& fogColor) override;
	virtual void             SetFogColorAffectedBySun(bool enable) override;
	virtual void             SetFogShadowing(float fogShadowing) override;

	virtual void             SetCapFogAtVolumeDepth(bool capFog) override;
	virtual void             SetVolumeDepth(float volumeDepth) override;
	virtual void             SetStreamSpeed(float streamSpeed) override;

	virtual void             SetCaustics(bool caustics) override;
	virtual void             SetCausticIntensity(float causticIntensity) override;
	virtual void             SetCausticTiling(float causticTiling) override;
	virtual void             SetCausticHeight(float causticHeight) override;
	virtual void             SetPhysParams(float density, float resistance) override { m_density = density; m_resistance = resistance; }
	virtual void             SetAuxPhysParams(pe_params_area* pa) override { m_auxPhysParams = *pa; if (m_pPhysArea) m_pPhysArea->SetParams(pa); }

	virtual void             CreateOcean(uint64 volumeID, /* TBD */ bool keepSerializationParams = false) override;
	virtual void             CreateArea(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false) override;
	virtual void             CreateRiver(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false) override;

	virtual void             SetAreaPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false) override;
	virtual void             SetRiverPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false) override;

	virtual IPhysicalEntity* SetAndCreatePhysicsArea(const Vec3* pVertices, unsigned int numVertices) override;
	void                     SyncToPhysMesh(const QuatT& qtSurface, IGeometry* pSurface, float depth);

	// implements IRenderNode
	virtual EERType          GetRenderNodeType() const override { return eERType_WaterVolume; }
	virtual const char*      GetEntityClassName() const override;
	virtual const char*      GetName() const override;
	virtual void             SetMatrix(const Matrix34& mat) override;
	virtual Vec3             GetPos(bool bWorldOnly = true) const override;
	virtual void             Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo) override;
	virtual void             SetMaterial(IMaterial* pMat) override;
	virtual IMaterial*       GetMaterial(Vec3* pHitPos) const override;
	virtual IMaterial*       GetMaterialOverride() const override { return m_pMaterial; }
	virtual float            GetMaxViewDist() const override;
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual void             Precache() override;

	virtual IPhysicalEntity* GetPhysics() const override;
	virtual void             SetPhysics(IPhysicalEntity*) override;

	virtual void             CheckPhysicalized() override;
	virtual void             Physicalize(bool bInstant = false) override;
	virtual void             Dephysicalize(bool bKeepIfReferenced = false) override;

	virtual const AABB       GetBBox() const override
	{
		AABB WSBBox = m_WSBBox;
		// Expand bounding box upwards while using caustics to avoid clipping the caustics when the volume goes out of view.
		if (m_caustics)
			WSBBox.max.z += m_causticHeight;
		return WSBBox;
	}

	virtual void         SetBBox(const AABB& WSBBox) override { m_WSBBox = WSBBox; }
	virtual void         FillBBox(AABB& aabb) const override { aabb = GetBBox(); }
	virtual void         OffsetPosition(const Vec3& delta) override;

	virtual void         SetLayerId(uint16 nLayerId) override { m_nLayerId = nLayerId; Get3DEngine()->C3DEngine::UpdateObjectsLayerAABB(this); }
	virtual uint16       GetLayerId() const          override { return m_nLayerId; }

	void                 Render_JobEntry(SRendParams rParam, SRenderingPassInfo passInfo);
	virtual bool         CanExecuteRenderAsJob() const override;

	void                 Transform(const Vec3& localOrigin, const Matrix34& l2w);
	virtual IRenderNode* Clone() const override;

public:
	CWaterVolumeRenderNode();
	const SWaterVolumeSerialize* GetSerializationParams();
	float*                       GetAuxSerializationDataPtr(int& count)
	{
		// TODO: 'pe_params_area' members between 'volume' and 'growthReserve' are not float only - a member (bConvexBorder) has int type, this should be investigated.
		count = (int)((&m_resistance + 1) - &m_auxPhysParams.volume);
		return &m_auxPhysParams.volume;
	}

private:
	typedef std::vector<SVF_P3F_C4B_T2F> WaterSurfaceVertices;
	typedef std::vector<uint16>          WaterSurfaceIndices;

private:
	~CWaterVolumeRenderNode();

	float            GetCameraDistToWaterVolumeSurface(const SRenderingPassInfo& passInfo) const;
	float            GetCameraDistSqToWaterVolumeAABB(const SRenderingPassInfo& passInfo) const;
	bool             IsCameraInsideWaterVolumeSurface2D(const SRenderingPassInfo& passInfo) const;

	void             UpdateBoundingBox();

	void             CopyVolatilePhysicsAreaContourSerParams(const Vec3* pVertices, unsigned int numVertices);
	void             CopyVolatileRiverSerParams(const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale);

	void             CopyVolatileAreaSerParams(const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale);

	bool             IsAttachedToEntity() const { return m_attachedToEntity; }

	IPhysicalEntity* CreatePhysicsAreaFromSettings();

private:
	IWaterVolumeRenderNode::EWaterVolumeType m_volumeType;
	uint64                     m_volumeID;

	float                      m_volumeDepth;
	float                      m_streamSpeed;

	CREWaterVolume::SParams    m_wvParams[RT_COMMAND_BUF_COUNT];

	_smart_ptr<IMaterial>      m_pMaterial;
	_smart_ptr<IMaterial>      m_pWaterBodyIntoMat;
	_smart_ptr<IMaterial>      m_pWaterBodyOutofMat;

	CREWaterVolume*            m_pVolumeRE[RT_COMMAND_BUF_COUNT];
	CREWaterVolume*            m_pSurfaceRE[RT_COMMAND_BUF_COUNT];
	SWaterVolumeSerialize*     m_pSerParams;

	SWaterVolumePhysAreaInput* m_pPhysAreaInput;
	IPhysicalEntity*           m_pPhysArea;

	WaterSurfaceVertices       m_waterSurfaceVertices;
	WaterSurfaceIndices        m_waterSurfaceIndices;

	Matrix34                   m_parentEntityWorldTM;
	uint16                     m_nLayerId;

	float                      m_fogDensity;
	Vec3                       m_fogColor;
	bool                       m_fogColorAffectedBySun;
	float                      m_fogShadowing;

	Plane                      m_fogPlane;
	Plane                      m_fogPlaneBase;

	Vec3                       m_vOffset;
	Vec3                       m_center;
	AABB                       m_WSBBox;

	bool                       m_capFogAtVolumeDepth;
	bool                       m_attachedToEntity;
	bool                       m_caustics;

	float                      m_causticIntensity;
	float                      m_causticTiling;
	float                      m_causticShadow;
	float                      m_causticHeight;
	pe_params_area             m_auxPhysParams;
	float                      m_density = 1000;
	float                      m_resistance = 1000;
};
