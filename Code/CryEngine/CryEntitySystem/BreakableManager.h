// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IBreakableManager.h>

class CEntitySystem;
class CEntity;
struct GeomRef;
//////////////////////////////////////////////////////////////////////////
//
// BreakableManager manager handles all the code for breaking/destroying entity geometry.
//
//////////////////////////////////////////////////////////////////////////
class CBreakableManager : public IBreakableManager
{
public:
	CBreakableManager() = default;
	virtual ~CBreakableManager() {}

	// actual breaking function.
	void BreakIntoPieces(GeomRef& geoOrig, const Matrix34& srcObjTM,
	                     IStatObj* pPiecesObj, const Matrix34& piecesObjTM,	IStatObj *pRemovedObj,
	                     BreakageParams const& Breakage, int nMatLayers);

	//////////////////////////////////////////////////////////////////////////
	// IBreakableManager implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void BreakIntoPieces(IEntity* pEntity, int nOrigSlot, int nPiecesSlot, BreakageParams const& Breakage);

	// Breakable Plane
	virtual EProcessImpactResult ProcessPlaneImpact(const SProcessImpactIn& in, SProcessImpactOut& out);
	virtual void                 ExtractPlaneMeshIsland(const SExtractMeshIslandIn& in, SExtractMeshIslandOut& out);
	virtual void                 GetPlaneMemoryStatistics(void* pPlaneRaw, ICrySizer* pSizer);
	virtual bool                 IsGeometryBreakable(IPhysicalEntity* pEntity, IStatObj* pStatObj, IMaterial* pMaterial);

	// Check if this stat object can shatter.
	// Check if its materials support shattering.
	virtual bool CanShatter(IStatObj* pStatObj);
	virtual bool CanShatterEntity(IEntity* pEntity, int nSlot = -1);

	// Attach the effect & params specified by material of object in slot.
	virtual void AttachSurfaceEffect(IEntity* pEntity, int nSlot, const char* sType, SpawnParams const& paramsIn);
	virtual void CreateSurfaceEffect(IStatObj* pStatObj, const Matrix34& tm, const char* sType);
	//////////////////////////////////////////////////////////////////////////

	bool                  CanShatterRenderMesh(IRenderMesh* pMesh, IMaterial* pMtl);

	virtual void          AddBreakEventListener(IBreakEventListener* pListener);
	virtual void          RemoveBreakEventListener(IBreakEventListener* pListener);

	virtual void          EntityDrawSlot(CEntity* pEntity, int32 slot, int32 flags);

	virtual ISurfaceType* GetFirstSurfaceType(IStatObj* pStatObj);
	virtual ISurfaceType* GetFirstSurfaceType(ICharacterInstance* pCharacter);

	virtual void          ReplayRemoveSubPartsEvent(const EventPhysRemoveEntityParts* pRemoveEvent);

	void                  HandlePhysicsCreateEntityPartEvent(const EventPhysCreateEntityPart* pCreateEvent);
	void                  HandlePhysicsRemoveSubPartsEvent(const EventPhysRemoveEntityParts* pRemoveEvent);
	void                  HandlePhysicsRevealSubPartEvent(const EventPhysRevealEntityPart* pRevealEvent);
	int                   HandlePhysics_UpdateMeshEvent(const EventPhysUpdateMesh* pEvent);

	void                  FakePhysicsEvent(EventPhys* pEvent);

	// Resets broken objects
	void ResetBrokenObjects();

	// Freeze/unfreeze render node.
	void                                            FreezeRenderNode(IRenderNode* pRenderNode, bool bEnable);

	void                                            CreateObjectAsParticles(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, IBreakableManager::SCreateParams& createParams);
	virtual IEntity*                                CreateObjectAsEntity(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, IPhysicalEntity* pSrcPhysEnt, IBreakableManager::SCreateParams& createParams, bool bCreateSubstProxy = false);
	bool                                            CheckForPieces(IStatObj* pStatObjSrc, IStatObj::SSubObject* pSubObj, const Matrix34& worldTM, int nMatLayers, IPhysicalEntity* pPhysEnt);
	IStatObj::SSubObject*                           CheckSubObjBreak(IStatObj* pStatObj, IStatObj::SSubObject* pSubObj, const EventPhysCreateEntityPart* epcep);

	virtual const IBreakableManager::SBrokenObjRec* GetPartBrokenObjects(int& brokenObjectCount) { brokenObjectCount = m_brokenObjs.size(); return &m_brokenObjs[0]; }

	virtual void                                    GetBrokenObjectIndicesForCloning(int32* pPartRemovalIndices, int32& iNumPartRemovalIndices,
	                                                                                 int32* pOutIndiciesForCloning, int32& iNumEntitiesForCloning,
	                                                                                 const EventPhysRemoveEntityParts* BreakEvents);
	virtual void ClonePartRemovedEntitiesByIndex(int32* pBrokenObjectIndices, int32 iNumBrokenObjectIndices,
	                                             EntityId* pOutClonedEntities, int32& iNumClonedBrokenEntities,
	                                             const EntityId* pRecordingEntities, int32 iNumRecordingEntities,
	                                             SRenderNodeCloneLookup& nodeLookup);

	virtual void HideBrokenObjectsByIndex(const int32* pBrokenObjectIndices, const int32 iNumBrokenObjectIndices);
	virtual void UnhidePartRemovedObjectsByIndex(const int32* pPartRemovalIndices, const int32 iNumPartRemovalIndices, const EventPhysRemoveEntityParts* pBreakEvents);
	virtual void ApplySinglePartRemovalFromEventIndex(int32 iPartRemovalEventIndex, const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts* pBreakEvents);
	virtual void ApplyPartRemovalsUntilIndexToObjectList(int32 iFirstEventIndex, const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts* pBreakEvents);

private:
	void CreateObjectCommon(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, IBreakableManager::SCreateParams& createParams);

	void ApplyPartBreakToClonedObjectFromEvent(const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts& OriginalEvent);

	// Remove Parts
	bool RemoveStatObjParts(IStatObj*& pStatObj);

	IBreakEventListener* m_pBreakEventListener = nullptr;
	//////////////////////////////////////////////////////////////////////////

	std::vector<IBreakableManager::SBrokenObjRec> m_brokenObjs;
	std::vector<IPhysicalEntity*>                 m_brokenObjsParticles;
};
