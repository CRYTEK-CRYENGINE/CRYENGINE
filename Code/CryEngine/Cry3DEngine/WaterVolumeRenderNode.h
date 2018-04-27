// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _WATERVOLUME_RENDERNODE_
#define _WATERVOLUME_RENDERNODE_

#pragma once

#include <CryRenderer/VertexFormats.h>

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

class CWaterVolumeRenderNode : public IWaterVolumeRenderNode, public Cry3DEngineBase
{
public:
	// implements IWaterVolumeRenderNode
	virtual void             SetAreaAttachedToEntity();

	virtual void             SetFogDensity(float fogDensity);
	virtual float            GetFogDensity() const;
	virtual void             SetFogColor(const Vec3& fogColor);
	virtual void             SetFogColorAffectedBySun(bool enable);
	virtual void             SetFogShadowing(float fogShadowing);

	virtual void             SetCapFogAtVolumeDepth(bool capFog);
	virtual void             SetVolumeDepth(float volumeDepth);
	virtual void             SetStreamSpeed(float streamSpeed);

	virtual void             SetCaustics(bool caustics);
	virtual void             SetCausticIntensity(float causticIntensity);
	virtual void             SetCausticTiling(float causticTiling);
	virtual void             SetCausticHeight(float causticHeight);
	virtual void             SetAuxPhysParams(pe_params_area* pa) { m_auxPhysParams = *pa; if (m_pPhysArea) m_pPhysArea->SetParams(pa); }

	virtual void             CreateOcean(uint64 volumeID, /* TBD */ bool keepSerializationParams = false);
	virtual void             CreateArea(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false);
	virtual void             CreateRiver(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false);

	virtual void             SetAreaPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false);
	virtual void             SetRiverPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false);

	virtual IPhysicalEntity* SetAndCreatePhysicsArea(const Vec3* pVertices, unsigned int numVertices);
	void                     SyncToPhysMesh(const QuatT& qtSurface, IGeometry* pSurface, float depth);

	// implements IRenderNode
	virtual EERType          GetRenderNodeType();
	virtual const char*      GetEntityClassName() const;
	virtual const char*      GetName() const;
	virtual void             SetMatrix(const Matrix34& mat);
	virtual Vec3             GetPos(bool bWorldOnly = true) const;
	virtual void             Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo);
	virtual void             SetMaterial(IMaterial* pMat);
	virtual IMaterial*       GetMaterial(Vec3* pHitPos) const;
	virtual IMaterial*       GetMaterialOverride() { return m_pMaterial; }
	virtual float            GetMaxViewDist();
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const;
	virtual void             Precache();

	virtual IPhysicalEntity* GetPhysics() const;
	virtual void             SetPhysics(IPhysicalEntity*);

	virtual void             CheckPhysicalized();
	virtual void             Physicalize(bool bInstant = false);
	virtual void             Dephysicalize(bool bKeepIfReferenced = false);

	virtual const AABB       GetBBox() const
	{
		AABB WSBBox = m_WSBBox;
		// Expand bounding box upwards while using caustics to avoid clipping the caustics when the volume goes out of view.
		if (m_caustics)
			WSBBox.max.z += m_causticHeight;
		return WSBBox;
	}

	virtual void         SetBBox(const AABB& WSBBox) { m_WSBBox = WSBBox; }
	virtual void         FillBBox(AABB& aabb);
	virtual void         OffsetPosition(const Vec3& delta);

	virtual void         SetLayerId(uint16 nLayerId) { m_nLayerId = nLayerId; Get3DEngine()->C3DEngine::UpdateObjectsLayerAABB(this); }
	virtual uint16       GetLayerId()                { return m_nLayerId; }

	void                 Render_JobEntry(SRendParams rParam, SRenderingPassInfo passInfo);
	virtual bool         CanExecuteRenderAsJob() final;

	void                 Transform(const Vec3& localOrigin, const Matrix34& l2w);
	virtual IRenderNode* Clone() const;

public:
	CWaterVolumeRenderNode();
	const SWaterVolumeSerialize* GetSerializationParams();
	float*                       GetAuxSerializationDataPtr(int& count)
	{
		// TODO: 'pe_params_area' members between 'volume' and 'growthReserve' are not float only - a member (bConvexBorder) has int type, this should be investigated.
		count = (offsetof(pe_params_area, growthReserve) - offsetof(pe_params_area, volume)) / sizeof(float) + 1;
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
};

#endif // #ifndef _DECAL_RENDERNODE_
