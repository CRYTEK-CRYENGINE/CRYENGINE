// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IBreakableManager_h__
#define __IBreakableManager_h__
#pragma once

// Forward declaration from physics interface.
struct EventPhys;
struct EventPhysRemoveEntityParts;

//////////////////////////////////////////////////////////////////////////
enum EPlaneBreakFlags
{
	ePlaneBreak_Static           = 0x1,
	ePlaneBreak_AutoSmash        = 0x2,
	ePlaneBreak_NoFractureEffect = 0x4,
	ePlaneBreak_PlaybackEvent    = 0x8
};

struct SExtractMeshIslandIn
{
	IStatObj*  pStatObj;
	IMaterial* pRenderMat;
	int        idMat;
	int        itriSeed;
	int        bCreateIsle;
	int        processFlags;

	SExtractMeshIslandIn()
		: pStatObj(0)
		, pRenderMat(0)
		, idMat(0)
		, itriSeed(0)
		, bCreateIsle(0)
		, processFlags(0)
	{
	}
};

struct SExtractMeshIslandOut
{
	IStatObj* pIsleStatObj;
	IStatObj* pStatObj;
	// For auto-smash, ExtractMeshIsland returns a centre and a size, instead of a pIsleStatObj
	Vec3      islandCenter;
	Vec3      islandSize;

	SExtractMeshIslandOut()
		: pIsleStatObj(0)
		, pStatObj(0)
	{
	}
};

struct ISurfaceType;
struct IMaterial;
struct SProcessImpactIn
{
	Vec3                         pthit;
	Vec3                         hitvel;
	Vec3                         hitnorm;
	float                        hitmass;
	float                        hitradius;
	int                          itriHit;
	IStatObj*                    pStatObj;
	IStatObj*                    pStatObjAux;
	Matrix34                     mtx;
	ISurfaceType*                pMat;
	IMaterial*                   pRenderMat;
	uint32                       processFlags;
	int                          eventSeed;
	float                        glassAutoShatterMinArea;

	const SExtractMeshIslandOut* pIslandOut;

	typedef void (* AddChunkFunc)(int);
	AddChunkFunc addChunkFunc;

	bool         bDelay;
	bool         bLoading;
	bool         bVerify;
};

struct SProcessImpactOut
{
	float                hitradius;
	int                  eventSeed;

	SExtractMeshIslandIn islandIn;

	IStatObj*            pStatObjNew;
	IStatObj*            pStatObjAux;
};

enum EProcessImpactResult
{
	eProcessImpact_Done,
	eProcessImpact_Delayed,
	eProcessImpact_DelayedMeshOnly,
	eProcessImpact_BadGeometry
};

//////////////////////////////////////////////////////////////////////////
struct IBreakableManager
{
	virtual ~IBreakableManager(){}
	enum EBReakageType
	{
		BREAKAGE_TYPE_DESTROY = 0,
		BREAKAGE_TYPE_FREEZE_SHATTER,
	};
	struct SBrokenObjRec
	{
		EntityId             idEnt;
		IRenderNode*         pSrcRenderNode;
		_smart_ptr<IStatObj> pStatObjOrg;
	};
	struct BreakageParams
	{
		EBReakageType type;              //!< Type of the breakage.
		float         fParticleLifeTime; //!< Average lifetime of particle pieces.
		int           nGenericCount;     //!< If not 0, force particle pieces to spawn generically, this many times.
		bool          bForceEntity;      //!< Force pieces to spawn as entities.
		bool          bMaterialEffects;  //!< Automatically create "destroy" and "breakage" material effects on pieces.
		bool          bOnlyHelperPieces; //!< Only spawn helper pieces.

		//! Impulse params.
		float fExplodeImpulse;        //!< Outward impulse to apply.
		Vec3  vHitImpulse;            //!< Hit impulse and center to apply.
		Vec3  vHitPoint;

		BreakageParams()
		{
			memset(this, 0, sizeof(*this));
		}
	};
	struct SCreateParams
	{
		int           nSlotIndex;
		Matrix34      slotTM;
		Matrix34      worldTM;
		float         fScale;
		IMaterial*    pCustomMtl;
		int           nMatLayers;
		int           nEntityFlagsAdd;
		int           nEntitySlotFlagsAdd;
		uint64        nRenderNodeFlags;
		IRenderNode*  pSrcStaticRenderNode;
		const char*   pName;
		IEntityClass* overrideEntityClass;

		SCreateParams() : fScale(1.0f), pCustomMtl(0), nSlotIndex(0), nRenderNodeFlags(0), pName(0),
			nMatLayers(0), nEntityFlagsAdd(0), nEntitySlotFlagsAdd(0), pSrcStaticRenderNode(0), overrideEntityClass(NULL) { slotTM.SetIdentity(); worldTM.SetIdentity(); };
	};
	virtual void BreakIntoPieces(IEntity* pEntity, int nSlot, int nPiecesSlot, BreakageParams const& Breakage) = 0;

	// Breakable Plane
	virtual EProcessImpactResult ProcessPlaneImpact(const SProcessImpactIn& in, SProcessImpactOut& out) = 0;
	virtual void                 ExtractPlaneMeshIsland(const SExtractMeshIslandIn& in, SExtractMeshIslandOut& out) = 0;
	virtual void                 GetPlaneMemoryStatistics(void* pPlane, ICrySizer* pSizer) = 0;
	virtual bool                 IsGeometryBreakable(IPhysicalEntity* pPhysEntity, IStatObj* pStatObj, IMaterial* pMaterial) = 0;

	//! Attaches the effect & params specified by material of object in slot.
	virtual void AttachSurfaceEffect(IEntity* pEntity, int nSlot, const char* sType, SpawnParams const& paramsIn) = 0;

	//! Checks if static object can be shattered, by checking it`s surface types.
	virtual bool CanShatter(IStatObj* pStatObj) = 0;

	//! Checks if entity can be shattered, by checking surface types of geometry or character.
	virtual bool     CanShatterEntity(IEntity* pEntity, int nSlot = -1) = 0;

	virtual void     FakePhysicsEvent(EventPhys* pEvent) = 0;

	virtual IEntity* CreateObjectAsEntity(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, IPhysicalEntity* pSrcPhysEnt, IBreakableManager::SCreateParams& createParams, bool bCreateSubstProxy = false) = 0;

	//! Adds a break event listener.
	virtual void AddBreakEventListener(IBreakEventListener* pListener) = 0;

	//! Removes a break event listener.
	virtual void RemoveBreakEventListener(IBreakEventListener* pListener) = 0;

	//! Replays a RemoveSubPartsEvent.
	virtual void ReplayRemoveSubPartsEvent(const EventPhysRemoveEntityParts* pRemoveEvent) = 0;

	//! Records that there has been a call to CEntity::DrawSlot() for later playback.
	virtual void EntityDrawSlot(CEntity* pEntity, int32 slot, int32 flags) = 0;

	//! Resets broken objects.
	virtual void ResetBrokenObjects() = 0;

	//! \return Vector of broken object records.
	virtual const IBreakableManager::SBrokenObjRec* GetPartBrokenObjects(int& brokenObjectCount) = 0;

	virtual void                                    GetBrokenObjectIndicesForCloning(int32* pPartRemovalIndices, int32& iNumPartRemovalIndices,
	                                                                                 int32* pOutIndiciesForCloning, int32& iNumEntitiesForCloning,
	                                                                                 const EventPhysRemoveEntityParts* BreakEvents) = 0;

	virtual void ClonePartRemovedEntitiesByIndex(int32* pBrokenObjectIndices, int32 iNumBrokenObjectIndices,
	                                             EntityId* pOutClonedEntities, int32& iNumClonedBrokenEntities,
	                                             const EntityId* pRecordingEntities, int32 iNumRecordingEntities,
	                                             SRenderNodeCloneLookup& nodeLookup) = 0;

	virtual void                 HideBrokenObjectsByIndex(const int32* pBrokenObjectIndices, const int32 iNumBrokenObjectIndices) = 0;
	virtual void                 UnhidePartRemovedObjectsByIndex(const int32* pPartRemovalIndices, const int32 iNumPartRemovalIndices, const EventPhysRemoveEntityParts* BreakEvents) = 0;
	virtual void                 ApplySinglePartRemovalFromEventIndex(int32 iPartRemovalEventIndex, const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts* pBreakEvents) = 0;
	virtual void                 ApplyPartRemovalsUntilIndexToObjectList(int32 iFirstEventIndex, const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts* pBreakEvents) = 0;

	virtual struct ISurfaceType* GetFirstSurfaceType(IStatObj* pStatObj) = 0;
	virtual struct ISurfaceType* GetFirstSurfaceType(ICharacterInstance* pCharacter) = 0;
};

#endif
