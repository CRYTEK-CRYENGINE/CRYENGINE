// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GAME_VOLUME_WATER_H
#define _GAME_VOLUME_WATER_H

#pragma once

class CGameVolume_Water : public CGameObjectExtensionHelper<CGameVolume_Water, IGameObjectExtension>
{
	struct WaterProperties
	{
		WaterProperties(IEntity* pEntity)
		{
			CRY_ASSERT(pEntity != NULL);

			memset(this, 0, sizeof(WaterProperties));

			SmartScriptTable properties;
			IScriptTable* pScriptTable = pEntity->GetScriptTable();

			if ((pScriptTable != NULL) && pScriptTable->GetValue("Properties", properties))
			{
				properties->GetValue("StreamSpeed", streamSpeed);
				properties->GetValue("FogDensity", fogDensity);
				properties->GetValue("UScale", uScale);
				properties->GetValue("VScale", vScale);
				properties->GetValue("Depth", depth);
				properties->GetValue("ViewDistanceRatio", viewDistanceRatio);
				properties->GetValue("MinSpec", minSpec);
				properties->GetValue("MaterialLayerMask", materialLayerMask);
				properties->GetValue("FogColorMultiplier", fogColorMultiplier);
				properties->GetValue("color_FogColor", fogColor);
				properties->GetValue("bFogColorAffectedBySun", fogColorAffectedBySun);
				properties->GetValue("FogShadowing", fogShadowing);
				properties->GetValue("bCapFogAtVolumeDepth", capFogAtVolumeDepth);
				properties->GetValue("bAwakeAreaWhenMoving", awakeAreaWhenMoving);
				properties->GetValue("bIsRiver", isRiver);
				properties->GetValue("bCaustics", caustics);
				properties->GetValue("CausticIntensity", causticIntensity);
				properties->GetValue("CausticTiling", causticTiling);
				properties->GetValue("CausticHeight", causticHeight);
			}
		}

		float streamSpeed;
		float fogDensity;
		float uScale;
		float vScale;
		float depth;
		int   viewDistanceRatio;
		int   minSpec;
		int   materialLayerMask;
		float fogColorMultiplier;
		Vec3  fogColor;
		bool  fogColorAffectedBySun;
		float fogShadowing;
		bool  capFogAtVolumeDepth;
		bool  awakeAreaWhenMoving;
		bool  isRiver;
		bool  caustics;
		float causticIntensity;
		float causticTiling;
		float causticHeight;
	};

public:
	CGameVolume_Water();
	virtual ~CGameVolume_Water();

	// IGameObjectExtension
	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId)                                                       {};
	virtual void                 PostInit(IGameObject* pGameObject);
	virtual void                 PostInitClient(int channelId)                                                   {};
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual void                 Release();
	virtual void                 FullSerialize(TSerialize ser)                                                 {};
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) { return false; };
	virtual void                 PostSerialize()                                                               {};
	virtual void                 SerializeSpawnInfo(TSerialize ser)                                            {}
	virtual ISerializableInfoPtr GetSpawnInfo()                                                                { return 0; }
	virtual void                 Update(SEntityUpdateContext& ctx, int slot);
	virtual void                 HandleEvent(const SGameObjectEvent& gameObjectEvent);
	virtual void                 ProcessEvent(const SEntityEvent&);
	virtual uint64               GetEventMask() const final;;
	virtual void                 SetChannelId(uint16 id)     {};
	virtual void                 PostUpdate(float frameTime) { CRY_ASSERT(false); }
	virtual void                 PostRemoteSpawn()           {};
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const;
	// ~IGameObjectExtension

private:

	struct SWaterSegment
	{
		SWaterSegment() : m_pWaterRenderNode(NULL), m_pWaterArea(NULL), m_physicsLocalAreaCenter(ZERO) {}
		IWaterVolumeRenderNode* m_pWaterRenderNode;
		IPhysicalEntity*        m_pWaterArea;
		Vec3                    m_physicsLocalAreaCenter;
	};

	typedef std::vector<SWaterSegment> WaterSegments;

	void CreateWaterRenderNode(IWaterVolumeRenderNode*& pWaterRenderNode);
	void SetupVolume();
	void SetupVolumeSegment(const WaterProperties& waterProperties, const uint32 segmentIndex, const Vec3* pVertices, const uint32 vertexCount);

	void CreatePhysicsArea(const uint32 segmentIndex, const Matrix34& baseMatrix, const Vec3* pVertices, uint32 vertexCount, const bool isRiver, const float streamSpeed);
	void DestroyPhysicsAreas();

	void AwakeAreaIfRequired(bool forceAwake);
	void UpdateRenderNode(IWaterVolumeRenderNode* pWaterRenderNode, const Matrix34& newLocation);

	void FillOutRiverSegment(const uint32 segmentIndex, const Vec3* pVertices, const uint32 vertexCount, Vec3* pVerticesOut);

	void DebugDrawVolume();

	WaterSegments m_segments;

	Matrix34      m_baseMatrix;
	Matrix34      m_initialMatrix;
	Vec3          m_lastAwakeCheckPosition;
	float         m_volumeDepth;
	float         m_streamSpeed;
	bool          m_awakeAreaWhenMoving;
	bool          m_isRiver;
};

#endif
