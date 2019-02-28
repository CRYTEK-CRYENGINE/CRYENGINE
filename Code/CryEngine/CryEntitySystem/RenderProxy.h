// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntitySystem.h"
#include <CryRenderer/IRenderer.h>
#include <Cry3DEngine/IRenderNode.h>

// forward declarations.
class CEntitySlot;
class CEntity;
struct SRendParams;
struct IShaderPublicParams;
class CEntityRender;
struct AnimEventInstance;
struct IRenderNode;

//////////////////////////////////////////////////////////////////////////
// Description:
//    CEntityRender provide rendering capabilities implementation for the Entity.
//    It can contain multiple sub object slots that can have their own relative transformation, and
//    each slot can represent specific renderable node (IStatObj,ICharacterInstance,etc..)
///////////////////////////////////////////////////////////// /////////////
class CEntityRender final : public ISimpleEntityEventListener
{
public:
	CEntityRender();
	virtual ~CEntityRender();

	// ISimpleEntityEventListener
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~ISimpleEntityEventListener

	// Must be called after constructor.
	void PostInit();
	void RegisterEventListeners(IEntityComponent::ComponentEventPriority priority);

	void Serialize(TSerialize ser);
	bool NeedNetworkSerialize();

	//////////////////////////////////////////////////////////////////////////
	// IEntityRender interface.
	//////////////////////////////////////////////////////////////////////////
	void         SetLocalBounds(const AABB& bounds, bool bDoNotRecalculate);
	void         GetWorldBounds(AABB& bounds) const;
	void         GetLocalBounds(AABB& bounds) const;
	void         InvalidateLocalBounds();

	IMaterial*   GetRenderMaterial(int nSlot = -1) const;
	IRenderNode* GetRenderNode(int nSlot = -1) const;

	void         SetSlotMaterial(int nSlot, IMaterial* pMaterial);
	IMaterial*   GetSlotMaterial(int nSlot) const;

	float        GetLastSeenTime() const { return m_fLastSeenTime; }
	//////////////////////////////////////////////////////////////////////////

	bool                IsSlotValid(int nIndex) const { return nIndex >= 0 && nIndex < (int)m_slots.size() && m_slots[nIndex] != NULL; };
	int                 GetSlotCount() const;
	bool                SetParentSlot(int nParentIndex, int nChildIndex);
	bool                GetSlotInfo(int nIndex, SEntitySlotInfo& slotInfo) const;

	void                SetSlotFlags(int nSlot, uint32 nFlags);
	uint32              GetSlotFlags(int nSlot) const;

	ICharacterInstance* GetCharacter(int nSlot);
	IStatObj*           GetStatObj(int nSlot);
	IParticleEmitter*   GetParticleEmitter(int nSlot);

	int                 SetSlotRenderNode(int nSlot, IRenderNode* pRenderNode);
	int                 SetSlotGeometry(int nSlot, IStatObj* pStatObj);
	int                 SetSlotCharacter(int nSlot, ICharacterInstance* pCharacter);
	int                 LoadGeometry(int nSlot, const char* sFilename, const char* sGeomName = NULL, int nLoadFlags = 0);
	int                 LoadCharacter(int nSlot, const char* sFilename, int nLoadFlags = 0);
	int                 LoadParticleEmitter(int nSlot, IParticleEffect* pEffect, SpawnParams const* params = NULL, bool bPrime = false, bool bSerialize = false);
	int                 SetParticleEmitter(int nSlot, IParticleEmitter* pEmitter, bool bSerialize = false);
	int                 LoadLight(int nSlot, SRenderLight* pLight, uint16 layerId);
	int                 LoadCloudBlocker(int nSlot, const SCloudBlockerProperties& properties);
	int                 LoadFogVolume(int nSlot, const SFogVolumeProperties& properties);
	int                 FadeGlobalDensity(int nSlot, float fadeTime, float newGlobalDensity);
#if defined(USE_GEOM_CACHES)
	int                 LoadGeomCache(int nSlot, const char* sFilename);
#endif

	//////////////////////////////////////////////////////////////////////////
	// Slots.
	//////////////////////////////////////////////////////////////////////////
	ILINE CEntitySlot* GetSlot(int nIndex) const { return (nIndex >= 0 && nIndex < (int)m_slots.size()) ? m_slots[nIndex] : NULL; }
	CEntitySlot*       GetParentSlot(int nIndex) const;

	// Allocate a new object slot.
	CEntitySlot* AllocSlot(int nIndex);
	// Frees existing object slot, also optimizes size of slot array.
	void         FreeSlot(int nIndex);
	void         FreeAllSlots();

	// Returns world transformation matrix of the object slot.
	Matrix34                          GetSlotWorldTM(int nIndex) const;
	// Returns local relative to host entity transformation matrix of the object slot.
	Matrix34                          GetSlotLocalTM(int nIndex, bool bRelativeToParent) const;
	// Set local transformation matrix of the object slot.
	void                              SetSlotLocalTM(int nIndex, const Matrix34& localTM, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask());
	// Set camera space position of the object slot
	void                              SetSlotCameraSpacePos(int nIndex, const Vec3& cameraSpacePos);
	// Get camera space position of the object slot
	void                              GetSlotCameraSpacePos(int nSlot, Vec3& cameraSpacePos) const;

	void                              SetSubObjHideMask(int nSlot, hidemask nSubObjHideMask);
	hidemask                          GetSubObjHideMask(int nSlot) const;

	void                              SetRenderNodeParams(const IEntity::SRenderNodeParams& params);
	IEntity::SRenderNodeParams&       GetRenderNodeParams()       { return m_renderNodeParams; };
	const IEntity::SRenderNodeParams& GetRenderNodeParams() const { return m_renderNodeParams; };
	//////////////////////////////////////////////////////////////////////////

	// Internal slot access function.
	ILINE CEntitySlot* Slot(int nIndex) const { assert(nIndex >= 0 && nIndex < (int)m_slots.size()); return m_slots[nIndex]; }
	IStatObj*          GetCompoundObj() const;

	void               SetLastSeenTime(float fNewTime) { m_fLastSeenTime = fNewTime; }

	void               DebugDraw(const SGeometryDebugDrawInfo& info);
	// Calls UpdateRenderNode on every slot.
	void               UpdateRenderNodes();

	void               CheckLocalBoundsChanged();

public:
	static int AnimEventCallback(ICharacterInstance* pCharacter, void* userdata);

	// Check if any render nodes are rendered recently.
	bool IsRendered() const;
	void PreviewRender(SEntityPreviewContext& context);

private:
	void ComputeLocalBounds(bool bForce = false);
	void OnEntityXForm(int nWhyFlags);

	// Get existing slot or make a new slot if not exist.
	// Is nSlot is negative will allocate a new available slot and return it Index in nSlot parameter.
	CEntitySlot* GetOrMakeSlot(int& nSlot);

	I3DEngine*   GetI3DEngine() { return gEnv->p3DEngine; }
	void         AnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event);

	CEntity*     GetEntity() const;

private:
	friend class CEntitySlot;
	friend class CEntity;

	// Object Slots.
	std::vector<CEntitySlot*> m_slots;

	// Local bounding box of render proxy.
	AABB m_localBBox = AABB(ZERO, ZERO);

	// Time passed since this entity was seen last time  (wrong: this is an absolute time, todo: fix float absolute time values)
	float m_fLastSeenTime;

	// Rendering related member variables, Passed to 3d engine render nodes.
	IEntity::SRenderNodeParams m_renderNodeParams;
};
