// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BreakableGlassSystem.h"
#include <CryRenderer/RenderElements/CREBreakableGlass.h>
#include <CryRenderer/RenderElements/CREBreakableGlassHelpers.h>

#include "CryAction.h"
#include "ActionGame.h"
#include <Cry3DEngine/IIndexedMesh.h>

// Defines
#define PHYSEVENT_COLLIDER 0
#define PHYSEVENT_COLLIDEE 1

// Error logging
#ifndef RELEASE
	#define LOG_GLASS_ERROR(...)   CryLogAlways("[BreakGlassSystem Error]: " __VA_ARGS__)
	#define LOG_GLASS_WARNING(...) CryLogAlways("[BreakGlassSystem Warning]: " __VA_ARGS__)
#else
	#define LOG_GLASS_ERROR(...)
	#define LOG_GLASS_WARNING(...)
#endif

//--------------------------------------------------------------------------------------------------
// Name: CBreakableGlassSystem
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CBreakableGlassSystem::CBreakableGlassSystem()
	: m_enabled(false)
{
	// Add physics callback
	if (IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld)
	{
		const float listenerPriority = 2001.0f;
		pPhysWorld->AddEventClient(EventPhysCollision::id, &CBreakableGlassSystem::HandleImpact, 1, listenerPriority);
	}

	// Hook up debug cvars
	if (m_pGlassCVars = new SBreakableGlassCVars())
	{
#ifdef GLASS_DEBUG_MODE
		// Rendering
		REGISTER_CVAR2("g_glassSystem_draw", &m_pGlassCVars->m_draw, 1, VF_CHEAT, "Toggles drawing of glass nodes");
		REGISTER_CVAR2("g_glassSystem_drawWireframe", &m_pGlassCVars->m_drawWireframe, 0, VF_CHEAT, "Toggles drawing of glass node wireframe");
		REGISTER_CVAR2("g_glassSystem_drawDebugData", &m_pGlassCVars->m_drawDebugData, 0, VF_CHEAT, "Toggles drawing of glass node break/impact data");
		REGISTER_CVAR2("g_glassSystem_drawFragData", &m_pGlassCVars->m_drawFragData, 0, VF_CHEAT, "Toggles drawing of glass physicalized fragment data");

		// Impact decals
		REGISTER_CVAR2("g_glassSystem_decalAlwaysRebuild", &m_pGlassCVars->m_decalAlwaysRebuild, 0, VF_CHEAT, "Forces decals to rebuild every frame, allowing real-time tweaking.");
		REGISTER_CVAR2("g_glassSystem_decalScale", &m_pGlassCVars->m_decalScale, 2.5f, VF_CHEAT, "Scale of procedural impact decals on glass");
		REGISTER_CVAR2("g_glassSystem_decalMinRadius", &m_pGlassCVars->m_decalMinRadius, 0.25f, VF_CHEAT, "Minimum size of decal around impact");
		REGISTER_CVAR2("g_glassSystem_decalMaxRadius", &m_pGlassCVars->m_decalMaxRadius, 1.25f, VF_CHEAT, "Maximum size of decal around impact");
		REGISTER_CVAR2("g_glassSystem_decalRandChance", &m_pGlassCVars->m_decalRandChance, 0.67f, VF_CHEAT, "Chance for a decal to randomly be scaled up");
		REGISTER_CVAR2("g_glassSystem_decalRandScale", &m_pGlassCVars->m_decalRandScale, 1.6f, VF_CHEAT, "Scale of decal if selected randomly");
		REGISTER_CVAR2("g_glassSystem_decalMinImpactSpeed", &m_pGlassCVars->m_minImpactSpeed, 400.0f, VF_CHEAT, "Minimum speed for an impact to use the bullet decal");

		// Physicalized fragments
		REGISTER_CVAR2("g_glassSystem_fragImpulseScale", &m_pGlassCVars->m_fragImpulseScale, 5.0f, VF_CHEAT, "Scales impulse applied to fragments when physicalized");
		REGISTER_CVAR2("g_glassSystem_fragAngImpulseScale", &m_pGlassCVars->m_fragAngImpulseScale, 0.1f, VF_CHEAT, "Scales *angular* impulse applied to fragments when physicalized");
		REGISTER_CVAR2("g_glassSystem_fragImpulseDampen", &m_pGlassCVars->m_fragImpulseDampen, 0.3f, VF_CHEAT, "Dampens impulse applied to non-directly impacted fragments");
		REGISTER_CVAR2("g_glassSystem_fragAngImpulseDampen", &m_pGlassCVars->m_fragAngImpulseDampen, 0.4f, VF_CHEAT, "Dampens *angular* impulse applied to non-directly impacted fragments");
		REGISTER_CVAR2("g_glassSystem_fragSpread", &m_pGlassCVars->m_fragSpread, 1.5f, VF_CHEAT, "Spread velocity to apply to impacted fragments");
		REGISTER_CVAR2("g_glassSystem_fragMaxSpeed", &m_pGlassCVars->m_fragMaxSpeed, 4.0f, VF_CHEAT, "Maximum speed to apply to physicalized fragments");

		// Impact breaks
		REGISTER_CVAR2("g_glassSystem_impactSplitMinRadius", &m_pGlassCVars->m_impactSplitMinRadius, 0.05f, VF_CHEAT, "Minimum radius for split around impact");
		REGISTER_CVAR2("g_glassSystem_impactSplitRandMin", &m_pGlassCVars->m_impactSplitRandMin, 0.5f, VF_CHEAT, "Minimum radius variation for split around impact");
		REGISTER_CVAR2("g_glassSystem_impactSplitRandMax", &m_pGlassCVars->m_impactSplitRandMax, 1.5f, VF_CHEAT, "Maximum radius variation for split around impact");
		REGISTER_CVAR2("g_glassSystem_impactEdgeFadeScale", &m_pGlassCVars->m_impactEdgeFadeScale, 2.0f, VF_CHEAT, "Scales radial crack fade distance");

		// Particle effects
		REGISTER_CVAR2("g_glassSystem_particleFXEnable", &m_pGlassCVars->m_particleFXEnable, 1, VF_CHEAT, "Toggles particle effects being played when fragments are broken");
		REGISTER_CVAR2("g_glassSystem_particleFXUseColours", &m_pGlassCVars->m_particleFXUseColours, 0, VF_CHEAT, "Toggles particle effects being coloured to match the glass");
		REGISTER_CVAR2("g_glassSystem_particleFXScale", &m_pGlassCVars->m_particleFXScale, 0.25f, VF_CHEAT, "Scales the glass particle effects spawned");
#endif // GLASS_DEBUG_MODE
	}

	// Add callback for system enabled cvar
	IConsole* pConsole = gEnv->pConsole;
	ICVar* pSysEnabledCvar = pConsole ? pConsole->GetCVar("g_glassSystemEnable") : NULL;

	if (pSysEnabledCvar)
	{
		m_enabled = (pSysEnabledCvar->GetIVal() != 0);
		pSysEnabledCvar->SetOnChangeCallback(OnEnabledCVarChange);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CBreakableGlassSystem
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CBreakableGlassSystem::~CBreakableGlassSystem()
{
	// Release all nodes and data
	ResetAll();

#ifdef GLASS_DEBUG_MODE
	// Remove debug cvars after all nodes that could be using them
	if (IConsole* pConsole = gEnv->pConsole)
	{
		pConsole->UnregisterVariable("g_glassSystem_draw");
		pConsole->UnregisterVariable("g_glassSystem_drawWireframe");
		pConsole->UnregisterVariable("g_glassSystem_drawDebugData");
		pConsole->UnregisterVariable("g_glassSystem_drawFragData");

		pConsole->UnregisterVariable("g_glassSystem_decalAlwaysRebuild");
		pConsole->UnregisterVariable("g_glassSystem_decalScale");
		pConsole->UnregisterVariable("g_glassSystem_decalMinRadius");
		pConsole->UnregisterVariable("g_glassSystem_decalMaxRadius");
		pConsole->UnregisterVariable("g_glassSystem_decalRandChance");
		pConsole->UnregisterVariable("g_glassSystem_decalRandScale");
		pConsole->UnregisterVariable("g_glassSystem_decalMinImpactSpeed");

		pConsole->UnregisterVariable("g_glassSystem_fragImpulseScale");
		pConsole->UnregisterVariable("g_glassSystem_fragAngImpulseScale");
		pConsole->UnregisterVariable("g_glassSystem_fragImpulseDampen");
		pConsole->UnregisterVariable("g_glassSystem_fragAngImpulseDampen");
		pConsole->UnregisterVariable("g_glassSystem_fragSpread");
		pConsole->UnregisterVariable("g_glassSystem_fragMaxSpeed");

		pConsole->UnregisterVariable("g_glassSystem_particleFXEnable");
		pConsole->UnregisterVariable("g_glassSystem_particleFXUseColours");
		pConsole->UnregisterVariable("g_glassSystem_particleFXScale");
	}
#endif // GLASS_DEBUG_MODE

	SAFE_DELETE(m_pGlassCVars);

	// Remove system enabled cvar callback
	IConsole* pConsole = gEnv->pConsole;
	ICVar* pSysEnabledCVar = pConsole ? pConsole->GetCVar("g_glassSystemEnable") : NULL;

	if (pSysEnabledCVar)
	{
		pSysEnabledCVar->SetOnChangeCallback(NULL);
	}

	// Remove physics callback
	if (IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld)
	{
		pPhysWorld->RemoveEventClient(EventPhysCollision::id, &CBreakableGlassSystem::HandleImpact, 1);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Allows render nodes to updates
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::Update(const float frameTime)
{
	AssertUnusedIfDisabled();

	// Allow render nodes to update
	SBreakableGlassUpdateParams updateParams;
	updateParams.m_frametime = frameTime;

	const int numPlanes = m_glassPlanes.Count();
	for (int i = 0; i < numPlanes; ++i)
	{
		if (IBreakableGlassRenderNode* pRenderNode = m_glassPlanes[i])
		{
			pRenderNode->Update(updateParams);

			// May be able to remove glass entirely
			if (pRenderNode->HasGlassShattered() && !pRenderNode->HasActiveFragments())
			{
				pRenderNode->ReleaseNode();
				SAFE_DELETE(m_glassPlanes[i]);
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: OnEnabledCVarChange
// Desc: Handles glass system cvar changes
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::OnEnabledCVarChange(ICVar* pCVar)
{
	if (pCVar)
	{
		if (CCryAction* pCryAction = CCryAction::GetCryAction())
		{
			if (CBreakableGlassSystem* pGlassSystem = static_cast<CBreakableGlassSystem*>(pCryAction->GetIBreakableGlassSystem()))
			{
				pGlassSystem->m_enabled = (pCVar->GetIVal() != 0);
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
// Name: HandleImpact
// Desc: Passes an impact on to the simulation
//--------------------------------------------------------------------------------------------------
int CBreakableGlassSystem::HandleImpact(const EventPhys* pPhysEvent)
{
	if (CCryAction* pCryAction = CCryAction::GetCryAction())
	{
		if (CBreakableGlassSystem* pGlassSystem = static_cast<CBreakableGlassSystem*>(pCryAction->GetIBreakableGlassSystem()))
		{
			pGlassSystem->AssertUnusedIfDisabled();

			if (pGlassSystem->m_enabled)
			{
				if (const EventPhysCollision* pCollEvent = static_cast<const EventPhysCollision*>(pPhysEvent))
				{
					// Glass fragments always get destroyed on their first collision
					const uint physFrag = (pCollEvent->iForeignData[0] == PHYS_FOREIGN_ID_BREAKABLE_GLASS_FRAGMENT) ? 0 : 1;
					IPhysicalEntity* pPhysEnt = pCollEvent->pEntity[physFrag];

					if (pPhysEnt && pCollEvent->iForeignData[physFrag] == PHYS_FOREIGN_ID_BREAKABLE_GLASS_FRAGMENT)
					{
						// Fragments only collide with non-glass geometry
						const int nonFragType = pCollEvent->iForeignData[1 - physFrag];

						if (nonFragType != PHYS_FOREIGN_ID_BREAKABLE_GLASS
						    && nonFragType != PHYS_FOREIGN_ID_BREAKABLE_GLASS_FRAGMENT
						    && nonFragType != PHYS_FOREIGN_ID_ENTITY) // Only break on floors, walls, etc.
						{
							// Verify parent glass node, then allow it to handle impact
							if (SGlassPhysFragment* pPhysFrag = (SGlassPhysFragment*)pCollEvent->pForeignData[physFrag])
							{
								if (IBreakableGlassRenderNode* pRenderNode = (IBreakableGlassRenderNode*)pPhysFrag->m_pRenderNode)
								{
									pRenderNode->DestroyPhysFragment(pPhysFrag);
								}
							}
						}
					}
					else if (pCollEvent->iForeignData[PHYSEVENT_COLLIDEE] == PHYS_FOREIGN_ID_BREAKABLE_GLASS)
					{
						// Get breakable glass data
						PodArray<IBreakableGlassRenderNode*>& glassPlanes = pGlassSystem->m_glassPlanes;
						const int numPlanes = glassPlanes.Count();

						// Duplicate event so we can freely manipulate it
						EventPhysCollision dupeEvent = *pCollEvent;
						const EventPhysCollision* pDupeEvent = (const EventPhysCollision*)&dupeEvent;

						// Some actors can force breaks for gameplay reasons
						IPhysicalEntity* pCollider = dupeEvent.pEntity[PHYSEVENT_COLLIDER];

						if (pCollider && pCollider->GetType() == PE_LIVING)
						{
							IEntity* pColliderEntity = (IEntity*)pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
							IActor* pActor = pColliderEntity ? gEnv->pGameFramework->GetIActorSystem()->GetActor(pColliderEntity->GetId()) : NULL;

							if (pActor && pActor->MustBreakGlass())
							{
								pGlassSystem->ModifyEventToForceBreak(&dupeEvent);
							}
						}

						// Verify glass node and pass impact through
						IBreakableGlassRenderNode* pRenderNode = (IBreakableGlassRenderNode*)pCollEvent->pForeignData[PHYSEVENT_COLLIDEE];
						const int targetId = pRenderNode ? pRenderNode->GetId() : numPlanes;

						if (targetId < numPlanes && pRenderNode == glassPlanes[targetId])
						{
							pGlassSystem->PassImpactToNode(pRenderNode, pDupeEvent);
						}
					}
				}
			}
		}
	}

	return 1; // Pass event to other handlers even if we processed it
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: BreakGlassObject
// Desc: Replaces a physical StatObj with a glass node where supported
// Note: We're currently duplicating some work done in CryAction, so should be reading that in.
//			 Similarly, we should also be accepting the SProcessImpactIn struct instead of the event
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassSystem::BreakGlassObject(const EventPhysCollision& physEvent, const bool forceGlassBreak)
{
	bool bSuccess = false;
	SBreakableGlassInitParams initParams;

	// Get surface type's breaking effect
	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	initParams.surfaceTypeId = physEvent.idmat[PHYSEVENT_COLLIDEE];

	if (ISurfaceType* pMat = pSurfaceTypeManager->GetSurfaceType(initParams.surfaceTypeId))
	{
		if (ISurfaceType::SBreakable2DParams* pBreakableParams = pMat->GetBreakable2DParams())
		{
			initParams.pShatterEffect = pBreakableParams->full_fracture_fx ? gEnv->pParticleManager->FindEffect(pBreakableParams->full_fracture_fx) : NULL;
		}
	}

	// Calculate glass data
	SBreakableGlassPhysData physData;

	if (ExtractPhysDataFromEvent(physEvent, physData, initParams) &&
	    ValidateExtractedOutline(physData, initParams))
	{
		Vec3 size;
		Matrix34 transMat;

		CalculateGlassBounds(physData.pPhysGeom, size, transMat);

		// Update initialisation parameters
		if (!physData.defaultFrag.empty())
		{
			initParams.pInitialFrag = physData.defaultFrag.begin();
			initParams.numInitialFragPts = physData.defaultFrag.size();
		}
		else
		{
			initParams.pInitialFrag = NULL;
			initParams.numInitialFragPts = 0;
		}

		initParams.size.set(size.x, size.y);
		initParams.thickness = size.z;

		// Fix-up mesh and calculate anchors
		CalculateGlassAnchors(physData, initParams);

		// Calculate UV coords at axis-aligned points
		Vec2 origin = InterpolateUVCoords(physData.uvBasis[0], physData.uvBasis[1], physData.uvBasis[2], Vec2Constants<f32>::fVec2_Zero);
		Vec2 xAxisPt = InterpolateUVCoords(physData.uvBasis[0], physData.uvBasis[1], physData.uvBasis[2], Vec2Constants<f32>::fVec2_OneX);
		Vec2 yAxisPt = InterpolateUVCoords(physData.uvBasis[0], physData.uvBasis[1], physData.uvBasis[2], Vec2Constants<f32>::fVec2_OneY);

		// Convert the UV basis to X and Y axes w/ an offset
		initParams.uvOrigin = origin;
		initParams.uvXAxis = xAxisPt - origin;
		initParams.uvYAxis = yAxisPt - origin;

		// Create a replacement glass node
		IBreakableGlassRenderNode* pRenderNode = InitGlassNode(physData, initParams, transMat);

		// Duplicate the collision event and patch it up for the new node
		EventPhysCollision dupeEvent = physEvent;

		dupeEvent.pEntity[PHYSEVENT_COLLIDEE] = pRenderNode->GetPhysics();
		dupeEvent.pForeignData[PHYSEVENT_COLLIDEE] = (void*)pRenderNode;
		dupeEvent.iForeignData[PHYSEVENT_COLLIDEE] = PHYS_FOREIGN_ID_BREAKABLE_GLASS;

		pe_params_part partParams;
		if (dupeEvent.pEntity[PHYSEVENT_COLLIDEE]->GetParams(&partParams) && partParams.nMats > 0)
		{
			dupeEvent.idmat[PHYSEVENT_COLLIDEE] = partParams.pMatMapping[0];
		}

		if (forceGlassBreak)
		{
			ModifyEventToForceBreak(&dupeEvent);
		}

		// Send it directly to the new node
		PassImpactToNode(pRenderNode, &dupeEvent);

		bSuccess = true;
	}

	return bSuccess;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ResetAll
// Desc: Resets all glass to initial state
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::ResetAll()
{
	// Glass nodes are only created *after* a collision,
	// so need to actually remove all nodes here
	ReleaseGlassNodes();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetMemoryUsage
// Desc: Adds the size of the system
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::GetMemoryUsage(ICrySizer* pSizer) const
{
	// System usage
	SIZER_COMPONENT_NAME(pSizer, "BreakableGlassSystem");
	pSizer->AddObject(this, sizeof(*this));
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ExtractPhysDataFromEvent
// Desc: Extracts collider's physical data from an event
// Note 1: Ideally *ALL* of this should be calculated offline and the minimal data loaded
// Note 2: We're currently duplicating some work done in CryAction, so should be reading that in
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassSystem::ExtractPhysDataFromEvent(const EventPhysCollision& physEvent, SBreakableGlassPhysData& data, SBreakableGlassInitParams& initParams)
{
	if (IPhysicalEntity* pPhysEntity = physEvent.pEntity[PHYSEVENT_COLLIDEE])
	{
		// Get collider entity data
		const int entType = pPhysEntity->GetiForeignData();
		const int entPart = physEvent.partid[PHYSEVENT_COLLIDEE];

		// Local output data
		IStatObj* pStatObj = NULL;
		IMaterial* pRenderMat = NULL;
		phys_geometry* pPhysGeom = NULL;
		IRenderNode::RenderFlagsType renderFlags = 0;

		Matrix34A entityMat;
		entityMat.SetIdentity();

		// Only handling simple objects at the moment
		const pe_type physType = pPhysEntity->GetType();

		if (physType == PE_STATIC || physType == PE_RIGID)
		{
			// Entity or static object?
			if (entType == PHYS_FOREIGN_ID_ENTITY)
			{
				IEntity* pEntity = (IEntity*)pPhysEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

				pStatObj = pEntity->GetStatObj(entPart);
				entityMat = pEntity->GetSlotWorldTM(entPart);

				if (IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
				{
					pRenderMat = pIEntityRender->GetRenderMaterial(entPart);

					IRenderNode* pRenderNode = pIEntityRender->GetRenderNode();
					renderFlags = pRenderNode ? pRenderNode->GetRndFlags() : 0;

					// Fall back to top level material if sub-object fails to find it
					if (!pRenderMat)
					{
						pRenderMat = pIEntityRender->GetRenderMaterial();

						if (!pRenderMat && pStatObj)
						{
							pRenderMat = pStatObj->GetMaterial();
						}
					}
				}
			}
			else if (entType == PHYS_FOREIGN_ID_STATIC)
			{
				if (IRenderNode* pBrush = (IRenderNode*)physEvent.pForeignData[PHYSEVENT_COLLIDEE])
				{
					pStatObj = pBrush->GetEntityStatObj(0, &entityMat);
					pRenderMat = pBrush->GetMaterial();
					renderFlags = pBrush->GetRndFlags();

					// May need to get sub-object and it's material
					if (pStatObj && pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
					{
						if (IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(entPart))
						{
							pStatObj = pSubObj->pStatObj;

							if (!pSubObj->bIdentityMatrix)
							{
								entityMat = entityMat * pSubObj->tm;
							}

							// Find the correct sub-material
							// Note: We loop as the slots don't always line up
							const int subMtlCount = pRenderMat->GetSubMtlCount();
							for (int i = 0; i < subMtlCount; ++i)
							{
								if (IMaterial* pSubMat = pRenderMat->GetSubMtl(i))
								{
									if (pSubMat->GetSurfaceTypeId() == initParams.surfaceTypeId)
									{
										pRenderMat = pSubMat;
										break;
									}
								}
							}
						}
					}
				}
			}
		}

		// Validate geometry of collided object
		pPhysGeom = pStatObj ? pStatObj->GetPhysGeom() : NULL;
		IGeometry* pGeom = pPhysGeom ? pPhysGeom->pGeom : NULL;
		bool validGeom = false;

		primitives::box bbox;
		int thinAxis;

		if (pGeom)
		{
			// Determine thin geometry axis for glass alignment
			pGeom->GetBBox(&bbox);
			thinAxis = idxmin3((float*)&bbox.size);

			// Handle geometry mesh type
			switch (pGeom->GetType())
			{
			case GEOM_TRIMESH:
				// Perform full mesh analysis and extraction
				if (mesh_data* pPhysMeshData = (mesh_data*)pGeom->GetData())
				{
					if (ValidatePhysMesh(pPhysMeshData, thinAxis) && ExtractPhysMesh(pPhysMeshData, thinAxis, bbox, data.defaultFrag))
					{
						validGeom = true;
					}
				}
				break;

			case GEOM_BOX:
				// Simple box, so assume valid
				validGeom = true;
				break;

			default:
				// Only support boxes and tri-meshes
				break;
			}
		}

		// Invalid geometry, so can't continue
		if (!validGeom)
		{
			pPhysGeom = NULL;
		}
		// Attempt UV coord extraction from render mesh
		else
		{
			ExtractUVCoords(pStatObj, bbox, thinAxis, data);
		}

		// Copy final data
		data.pStatObj = pStatObj;
		data.pPhysGeom = pPhysGeom;
		data.renderFlags = renderFlags;
		data.entityMat = entityMat;
		initParams.pGlassMaterial = pRenderMat;
	}

	return data.pStatObj && data.pPhysGeom && initParams.pGlassMaterial;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ValidatePhysMesh
// Desc: Determines if a mesh is breakable/supported by the system
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassSystem::ValidatePhysMesh(mesh_data* pPhysMesh, const int thinAxis)
{
	bool isBreakable = true;

	// Only support a singular mesh
	if (pPhysMesh->pIslands && pPhysMesh->nIslands > 1)
	{
		LOG_GLASS_WARNING("Physics mesh has too many islands, glass elements split incorrectly?");
		isBreakable = false;
	}
	// Only support meshes with an actual surface (sanity check)
	else if (pPhysMesh->nVertices < 3)
	{
		LOG_GLASS_WARNING("Physics mesh doesn't contain enough vertices.");
		isBreakable = false;
	}

	return isBreakable;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ExtractPhysMesh
// Desc: Obtains a glass-fragment list from a breakable mesh
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassSystem::ExtractPhysMesh(mesh_data* pPhysMesh, const int thinAxis, const primitives::box& bbox, TGlassDefFragmentArray& fragOutline)
{
	// Calculate thin axis
	const Vec3& thinRow = bbox.Basis.GetRow(thinAxis);
	const Matrix33& vtxTrans = bbox.Basis;

	// Traverse connected triangles and build merged list
	const int numTris = pPhysMesh->nTris;
	TGlassFragmentIndexArray touchedTris, fragTris;

	for (int i = 0; i < numTris; ++i)
	{
		MergeFragmentTriangles(pPhysMesh, i, thinRow, touchedTris, fragTris);
	}

	// Thinnest axis determines mesh orientation
	int xAxis = thinAxis == 0 ? 2 : (thinAxis == 1 ? 0 : 0);
	int yAxis = thinAxis == 0 ? 1 : (thinAxis == 1 ? 2 : 1);

	const Vec3& vx = vtxTrans.GetRow(xAxis);
	const Vec3& vy = vtxTrans.GetRow(yAxis);

	// Create single outline for each set of triangles, discarding thin axis
	strided_pointer<Vec3> pVerts = pPhysMesh->pVertices;
	index_t* pInds = pPhysMesh->pIndices;

	if (fragTris.empty())
	{
		LOG_GLASS_ERROR("Valid phys mesh found, but no triangles extractable.");
	}
	else
	{
		// Pre-size outline buffer
		const uint fragTriCount = min(fragTris.size(), fragOutline.max_size() - 2);
		const uint vertCount = min(fragTriCount + 2, fragOutline.max_size());

		for (int i = 0; i < fragTriCount + 2; ++i)
		{
			fragOutline.push_back();
		}

		// Add initial triangle
		int triIndex = fragTris[0] * 3;

		Vec3 tmpVtx = pVerts[pInds[triIndex]];
		fragOutline[0].x = vx.Dot(tmpVtx);
		fragOutline[0].y = vy.Dot(tmpVtx);

		tmpVtx = pVerts[pInds[triIndex + 1]];
		fragOutline[1].x = vx.Dot(tmpVtx);
		fragOutline[1].y = vy.Dot(tmpVtx);

		tmpVtx = pVerts[pInds[triIndex + 2]];
		fragOutline[2].x = vx.Dot(tmpVtx);
		fragOutline[2].y = vy.Dot(tmpVtx);

		int ptCount = 3;

		// Calculate area of triangle to determine winding order
		float area = fragOutline[2].x * fragOutline[0].y - fragOutline[0].x * fragOutline[2].y;
		area += fragOutline[0].x * fragOutline[1].y - fragOutline[1].x * fragOutline[0].y;
		area += fragOutline[1].x * fragOutline[2].y - fragOutline[2].x * fragOutline[1].y;

		// Reverse triangle order if not clockwise
		if (area < 0.0f)
		{
			Vec2 temp = fragOutline[2];
			fragOutline[2] = fragOutline[0];
			fragOutline[0] = temp;
		}

		// Merge each additional triangle into the shape
		for (int j = 1; j < fragTriCount; ++j)
		{
			Vec2 triVerts[3];
			triIndex = fragTris[j] * 3;

			tmpVtx = pVerts[pInds[triIndex]];
			triVerts[0].x = vx.Dot(tmpVtx);
			triVerts[0].y = vy.Dot(tmpVtx);

			tmpVtx = pVerts[pInds[triIndex + 1]];
			triVerts[1].x = vx.Dot(tmpVtx);
			triVerts[1].y = vy.Dot(tmpVtx);

			tmpVtx = pVerts[pInds[triIndex + 2]];
			triVerts[2].x = vx.Dot(tmpVtx);
			triVerts[2].y = vy.Dot(tmpVtx);

			// Find triangle's shared indices
			uint insertInd = 0;
			int indMin = -1, indMax = -1;

			for (int k = 0; k < ptCount; ++k)
			{
				for (int l = 0; l < 3; ++l)
				{
					// Matching triangle point
					if (fragOutline[k] == triVerts[l])
					{
						// Record which indices were matched
						insertInd += l;

						// Store matching edge
						if (indMin == -1)
						{
							indMin = indMax = k;
						}
						else
						{
							indMin = min(indMin, k);
							indMax = max(indMax, k);
						}

						break;
					}
				}

				// Can break if two indices found
				if (indMin != indMax)
				{
					break;
				}
			}

			// Calculate triangle index to add (assuming results are 0+1, 0+2 or 1+2)
			insertInd = min(3 - insertInd, (uint)2);

			// If at boundaries, can simply push
			if (indMin == 0 && indMax == ptCount - 1)
			{
				fragOutline[ptCount] = triVerts[insertInd];
			}
			// May need to insert new vertex between shared vertices
			else
			{
				for (int k = ptCount; k > indMax; --k)
				{
					fragOutline[k] = fragOutline[k - 1];
				}

				fragOutline[indMax] = triVerts[insertInd];
			}

			++ptCount;
		}
	}

	// Remove center offset from extracted mesh
	const Vec3 center = vtxTrans.TransformVector(bbox.center);
	const Vec2 centerOffset(center[xAxis], center[yAxis]);
	const int fragOutlineSize = fragOutline.size();

	for (int i = 0; i < fragOutlineSize; ++i)
	{
		fragOutline[i] -= centerOffset;
	}

	return !fragOutline.empty();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: MergeFragmentTriangles
// Desc: Merges connected triangles into a single fragment list
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::MergeFragmentTriangles(mesh_data* pPhysMesh, const int tri, const Vec3& thinRow, TGlassFragmentIndexArray& touchedTris, TGlassFragmentIndexArray& fragTris)
{
	// Check for limits
	if (touchedTris.size() < touchedTris.max_size())
	{
		// Search for existing term
		const uint size = touchedTris.size();
		int index = -1;

		for (uint i = 0; i < size; ++i)
		{
			if (touchedTris[i] == tri)
			{
				index = i;
				break;
			}
		}

		if (index == -1)
		{
			touchedTris.push_back(tri);

			// Only support (fairly) flat surfaces
			const float flatThresh = 0.95f;

			if (thinRow.Dot(pPhysMesh->pNormals[tri]) >= flatThresh)
			{
				// Merge fragment
				fragTris.push_back(tri);

				// Recursively merge all connected triangles
				for (int i = 0; i < 3; ++i)
				{
					const int buddy = pPhysMesh->pTopology[tri].ibuddy[i];
					if (buddy >= 0)
					{
						MergeFragmentTriangles(pPhysMesh, buddy, thinRow, touchedTris, fragTris);
					}
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ExtractUVCoords
// Desc: Extracts the uv coord basis from the render mesh
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::ExtractUVCoords(IStatObj* const pStatObj, const primitives::box& bbox, const int thinAxis, SBreakableGlassPhysData& data)
{
	IIndexedMesh* pIndexedMesh = pStatObj ? pStatObj->GetIndexedMesh(true) : NULL;
	CMesh* pMesh = pIndexedMesh ? pIndexedMesh->GetMesh() : NULL;

	if (pMesh)
	{
		CRY_ASSERT(pMesh->m_pPositionsF16 == 0);
		const SMeshTexCoord* pUV = pMesh->m_pTexCoord;
		const SMeshTangents* pTangents = pMesh->m_pTangents;
		const Vec3* pPositions = pMesh->m_pPositions;

		if (pTangents && pUV)
		{
			// Determine thinnest axis
			Vec3 thinRow = bbox.Basis.GetRow(idxmin3((float*)&bbox.size));

			// Attempt UV coord extraction
			const uint numVerts = (uint)pMesh->GetVertexCount();
			CRY_ASSERT(numVerts == pMesh->GetTexCoordCount());

			// Initialise our surface points with invalid data
			uint pts[6] =
			{
				numVerts, numVerts, numVerts, // Front faces
				numVerts, numVerts, numVerts  // Back faces
			};

			for (int i = 0; i < numVerts; ++i)
			{
				// Unpack surface data
				const Vec3 normal = pTangents[i].GetN();
				const Vec2 uv = pUV[i].GetUV();

				// Validate vertex is part of our surface
				const float flatThreshold = 0.95f;
				const float dot = thinRow.Dot(normal);

				if (dot >= flatThreshold)
				{
					const Vec2 uv0 = pUV[pts[0]].GetUV();

					// Store unique points to determine UV axes
					if (pts[0] == numVerts)
					{
						pts[0] = i;
					}
					else if (pts[1] == numVerts && uv0.x != uv.x)
					{
						pts[1] = i;
					}
					else if (pts[2] == numVerts && uv0.y != uv.y)
					{
						pts[2] = i;
					}

					if (pts[1] < numVerts && pts[2] < numVerts)
					{
						break;
					}
				}
				else if (dot <= -flatThreshold)
				{
					const Vec2 uv3 = pUV[pts[3]].GetUV();

					// Same as above, but for back-faces
					if (pts[3] == numVerts)
					{
						pts[3] = i;
					}
					else if (pts[4] == numVerts && uv3.x != uv.x)
					{
						pts[4] = i;
					}
					else if (pts[5] == numVerts && uv3.y != uv.y)
					{
						pts[5] = i;
					}

					if (pts[4] < numVerts && pts[5] < numVerts)
					{
						break;
					}
				}
			}

			// If we failed to get data from the front faces, swap with back
			if (pts[1] == numVerts || pts[2] == numVerts)
			{
				pts[0] = pts[3];
				pts[1] = pts[4];
				pts[2] = pts[5];
			}

			if (pts[1] < numVerts && pts[2] < numVerts)
			{
				// Calculate vertex transformation constants
				int xAxis = thinAxis == 0 ? 2 : (thinAxis == 1 ? 0 : 0);
				int yAxis = thinAxis == 0 ? 1 : (thinAxis == 1 ? 2 : 1);

				const Vec3& vx = bbox.Basis.GetRow(xAxis);
				const Vec3& vy = bbox.Basis.GetRow(yAxis);

				const Vec3 centerAxis = bbox.Basis.TransformVector(bbox.center);
				Vec2 centerOffset(centerAxis[xAxis], centerAxis[yAxis]);

				// Transform and store best 3 points
				data.uvBasis[0].x = vx.Dot(pPositions[pts[0]]) - centerOffset.x;
				data.uvBasis[0].y = vy.Dot(pPositions[pts[0]]) - centerOffset.y;

				data.uvBasis[1].x = vx.Dot(pPositions[pts[1]]) - centerOffset.x;
				data.uvBasis[1].y = vy.Dot(pPositions[pts[1]]) - centerOffset.y;

				data.uvBasis[2].x = vx.Dot(pPositions[pts[2]]) - centerOffset.x;
				data.uvBasis[2].y = vy.Dot(pPositions[pts[2]]) - centerOffset.y;

				// Store associated UV coords
				pUV[pts[0]].ExportTo(data.uvBasis[0].z, data.uvBasis[0].w);
				pUV[pts[1]].ExportTo(data.uvBasis[1].z, data.uvBasis[1].w);
				pUV[pts[2]].ExportTo(data.uvBasis[2].z, data.uvBasis[2].w);
			}
			else
			{
				LOG_GLASS_ERROR("Failed to extract UV coordinates from mesh.");
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: InterpolateUVCoords
// Desc: Calculates the barycentric coordinates of a point and interpolates its UV coords
//--------------------------------------------------------------------------------------------------
Vec2 CBreakableGlassSystem::InterpolateUVCoords(const Vec4& uvPt0, const Vec4& uvPt1, const Vec4& uvPt2, const Vec2& pt)
{
	// Calculate barycentric coordinates of position
	Vec2 A(uvPt0.x, uvPt0.y);
	Vec2 B(uvPt1.x, uvPt1.y);
	Vec2 C(uvPt2.x, uvPt2.y);

	Vec2 v0(C - A);
	Vec2 v1(B - A);
	Vec2 v2(pt - A);

	float dot00 = v0.Dot(v0);
	float dot01 = v0.Dot(v1);
	float dot02 = v0.Dot(v2);
	float dot11 = v1.Dot(v1);
	float dot12 = v1.Dot(v2);

	float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	float uLerp = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float vLerp = (dot00 * dot12 - dot01 * dot02) * invDenom;

	// Interpolate from known uv coordinates
	Vec2 uvAB(uvPt1.z - uvPt0.z, uvPt1.w - uvPt0.w);
	Vec2 uvAC(uvPt2.z - uvPt0.z, uvPt2.w - uvPt0.w);

	Vec2 uv;
	uv.x = uvPt0.z + uvAC.x * uLerp + uvAB.x * vLerp;
	uv.y = uvPt0.w + uvAC.y * uLerp + uvAB.y * vLerp;

	return uv;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ValidateExtractedOutline
// Desc: Performs a final validation pass on the extracted outline
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassSystem::ValidateExtractedOutline(SBreakableGlassPhysData& data, SBreakableGlassInitParams& initParams)
{
	bool valid = true;

	// Check for overlapping points (leads to FPE during triangulation)
	if (initParams.pInitialFrag && initParams.numInitialFragPts > 0)
	{
		const Vec2* pPts = initParams.pInitialFrag;
		const uint numEdges = initParams.numInitialFragPts - 1;
		const float minEdgeLen = 0.0001f;

		for (uint i = 0, j = 1; i < numEdges; ++i, ++j)
		{
			const Vec2 edge(pPts[i] - pPts[j]);

			if (edge.GetLength2() < minEdgeLen)
			{
				LOG_GLASS_ERROR("Extracted mesh has invalid edges.");

				valid = false;
				break;
			}
		}
	}

	// Check for overlapping UVs (leads to FPE during uv basis calculation)
	if (valid)
	{
		const Vec2 uvPtA(data.uvBasis[0].x, data.uvBasis[0].y);
		const Vec2 uvPtB(data.uvBasis[1].x, data.uvBasis[1].y);
		const Vec2 uvPtC(data.uvBasis[2].x, data.uvBasis[2].y);

		const Vec2 uvEdge0(uvPtC - uvPtA);
		const Vec2 uvEdge1(uvPtB - uvPtA);

		const float dot00 = uvEdge0.Dot(uvEdge0);
		const float dot01 = uvEdge0.Dot(uvEdge1);
		const float dot11 = uvEdge1.Dot(uvEdge1);
		const float epsilon = 0.001f;

		if (fabs_tpl(dot00 * dot11 - dot01 * dot01) < epsilon)
		{
			LOG_GLASS_ERROR("Extracted mesh has invalid uv layout.");
			valid = false;
		}
	}

	return valid;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculateGlassBounds
// Desc: Calculates glass bounds from physics geometry
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::CalculateGlassBounds(const phys_geometry* const pPhysGeom, Vec3& size, Matrix34& matrix)
{
	// Find thinnest axis of physics geometry
	primitives::box bbox;
	pPhysGeom->pGeom->GetBBox(&bbox);

	Matrix33 basis = bbox.Basis.T();
	Vec3 halfSize = bbox.size;
	Vec3 center = bbox.center;

	const uint thinAxis = idxmin3(&halfSize.x);

	// Need to rotate so Z is our thin axis
	if (thinAxis < 2)
	{
		float tempSize;
		Matrix33 tempMat;
		tempMat.SetIdentity();

		// Calculate the rotation based on current facing dir
		const Vec3 axes[2] =
		{
			Vec3Constants<float>::fVec3_OneX,
			Vec3Constants<float>::fVec3_OneY
		};

		const Vec3& thinRow = bbox.Basis.GetRow(thinAxis);
		const Vec3 localAxis = bbox.Basis.TransformVector(axes[thinAxis]);
		float rot = (thinRow.Dot(localAxis) >= 0.0f) ? -gf_PI * 0.5f : gf_PI * 0.5f;

		if (thinAxis == 0)
		{
			tempSize = halfSize.x;
			halfSize.x = halfSize.z;

			tempMat.SetRotationY(rot);
		}
		else
		{
			tempSize = halfSize.y;
			halfSize.y = halfSize.z;

			tempMat.SetRotationX(rot);
		}

		// Apply rotation to matrix and vectors
		basis = basis * tempMat;
		halfSize.z = tempSize;
	}

	// Assert minimum thickness
	const float halfMinThickness = 0.004f;
	halfSize.z = max(halfSize.z, halfMinThickness);

	size = halfSize * 2.0f;

	// Calculate locally offset bounds
	matrix.SetIdentity();
	matrix.SetTranslation(-halfSize);

	matrix = basis * matrix;

	matrix.AddTranslation(center);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculateGlassAnchors
// Desc: Ensures mesh is within bounds and calculates anchor points
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::CalculateGlassAnchors(SBreakableGlassPhysData& data, SBreakableGlassInitParams& initParams)
{
	// Apply half-size offset to uv basis
	const Vec2& size = initParams.size;
	const Vec2 halfSize = size * 0.5f;

	for (uint i = 0; i < 3; ++i)
	{
		data.uvBasis[i].x += halfSize.x;
		data.uvBasis[i].y += halfSize.y;
	}

	// Only continue if non-default mesh used
	Vec2* pPts = initParams.pInitialFrag;
	const uint numPts = initParams.numInitialFragPts;

	if (pPts && numPts >= 3)
	{
		Vec2 minPt = size;
		Vec2 maxPt = Vec2Constants<float>::fVec2_Zero;

		// Offset fragment data to glass working space
		for (uint i = 0; i < numPts; ++i)
		{
			pPts[i] += halfSize;

			// Update bounds
			minPt.x = min(minPt.x, pPts[i].x);
			minPt.y = min(minPt.y, pPts[i].y);

			maxPt.x = max(maxPt.x, pPts[i].x);
			maxPt.y = max(maxPt.y, pPts[i].y);
		}

		// Calculate any final offset towards working bounds
		minPt.x = min(minPt.x, 0.0f);
		minPt.y = min(minPt.y, 0.0f);

		maxPt.x = max(maxPt.x - size.x, 0.0f);
		maxPt.y = max(maxPt.y - size.y, 0.0f);

		Vec2 offset;
		offset.x = (maxPt.x > 0.0f) ? -maxPt.x : -minPt.x;
		offset.y = (maxPt.y > 0.0f) ? -maxPt.y : -minPt.y;

		// Adjust bounds if off
		if (offset.GetLength2() > 0.0f)
		{
			for (uint i = 0; i < numPts; ++i)
			{
				pPts[i] += offset;
			}

			for (uint i = 0; i < 3; ++i)
			{
				data.uvBasis[i].x += offset.x;
				data.uvBasis[i].y += offset.y;
			}
		}

		// Determine best anchor points
		const uint maxNumAnchors = SBreakableGlassInitParams::MaxNumAnchors;
		const uint numAnchors = min(numPts, maxNumAnchors);
		initParams.numAnchors = numAnchors;

		if (numPts <= maxNumAnchors)
		{
			// If few enough points, use them directly
			for (uint i = 0; i < numAnchors; ++i)
			{
				initParams.anchors[i] = pPts[i];
			}
		}
		else
		{
			// Else, determine mesh points closest to corners
			const Vec2 anchorPos[maxNumAnchors] =
			{
				Vec2(0.0f,              0.0f),
				Vec2(0.0f,              initParams.size.y),
				Vec2(initParams.size.x, 0.0f),
				Vec2(initParams.size.x, initParams.size.y)
			};

			uint anchorPts[maxNumAnchors] = { 0, 0, 0, 0 };

			for (uint i = 0; i < maxNumAnchors; ++i)
			{
				float anchorDistSq = FLT_MAX;

				for (uint j = 0; j < numPts; ++j)
				{
					// Store point if new closest found
					const float distSq = (anchorPos[i] - pPts[j]).GetLength2();

					if (distSq < anchorDistSq)
					{
						// Avoid duplicates
						bool duplicate = false;
						for (uint k = 0; k < i; ++k)
						{
							if (anchorPts[k] == j)
							{
								duplicate = true;
								break;
							}
						}

						if (!duplicate)
						{
							anchorDistSq = distSq;
							anchorPts[i] = j;
						}
					}
				}

				// Save closest pt to this corner
				initParams.anchors[i] = pPts[anchorPts[i]];
			}
		}

		// Calculate approx. polygon center
		Vec2 center(Vec2Constants<float>::fVec2_Zero);
		const float fNumPts = (float)numPts;

		for (uint i = 0; i < numPts; ++i)
		{
			center += pPts[i];
		}

		center *= __fres(fNumPts);

		// Finally, shift anchors slightly towards center
		const float anchorPtShift = 0.015f;

		for (uint i = 0; i < numAnchors; ++i)
		{
			Vec2 toCenter = (center - initParams.anchors[i]).GetNormalized();
			initParams.anchors[i] += toCenter * anchorPtShift;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PassImpactToNode
// Desc: Passes an impact through to the node
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::PassImpactToNode(IBreakableGlassRenderNode* pRenderNode, const EventPhysCollision* pPhysEvent)
{
	if (pRenderNode && pPhysEvent)
	{
		// Special case for explosion
		const int explosionMatId = -1;
		const bool isExplosion = pPhysEvent->idmat[0] == explosionMatId;

		if (isExplosion)
		{
			pRenderNode->ApplyExplosionToGlass(pPhysEvent);
		}
		// Standard impact with glass
		else
		{
			pRenderNode->ApplyImpactToGlass(pPhysEvent);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: InitGlassNode
// Desc: Creates and initialises a new glass render node
//--------------------------------------------------------------------------------------------------
IBreakableGlassRenderNode* CBreakableGlassSystem::InitGlassNode(const SBreakableGlassPhysData& physData, SBreakableGlassInitParams& initParams, const Matrix34& transMat)
{
	IBreakableGlassRenderNode* pRenderNode = (IBreakableGlassRenderNode*)gEnv->p3DEngine->CreateRenderNode(eERType_BreakableGlass);

	if (pRenderNode)
	{
		// Determine node ID and add to list
		const uint16 id = m_glassPlanes.Count();

		IBreakableGlassRenderNode*& pGlassPlane = m_glassPlanes.AddNew();
		pGlassPlane = pRenderNode;

		// Basic initialisation
		pRenderNode->SetId(id);
		pRenderNode->SetRndFlags(physData.renderFlags);
		pRenderNode->SetRndFlags(ERF_HIDDEN, false);

		// By default, anchor to four corner points
		// Note: This should read artist-placed data
		if (initParams.numAnchors == 0)
		{
			const Vec2 anchorMin(0.01f, 0.01f);
			const Vec2 anchorMax(initParams.size - anchorMin);

			initParams.numAnchors = SBreakableGlassInitParams::MaxNumAnchors;
			initParams.anchors[0].set(anchorMin.x, anchorMin.y);
			initParams.anchors[1].set(anchorMin.x, anchorMax.y);
			initParams.anchors[2].set(anchorMax.x, anchorMin.y);
			initParams.anchors[3].set(anchorMax.x, anchorMax.y);
		}

		// Set params (which will force a state reset)
		pRenderNode->InitialiseNode(initParams, physData.entityMat * transMat);

		// Set cvar struct
		pRenderNode->SetCVars(m_pGlassCVars);
	}

	return pRenderNode;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReleaseGlassNodes
// Desc: Releases and removes all glass nodes
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::ReleaseGlassNodes()
{
	AssertUnusedIfDisabled();

	const int numPlanes = m_glassPlanes.Count();
	for (int i = 0; i < numPlanes; ++i)
	{
		if (IBreakableGlassRenderNode* pRenderNode = m_glassPlanes[i])
		{
			pRenderNode->ReleaseNode();
			SAFE_DELETE(m_glassPlanes[i]);
		}
	}

	m_glassPlanes.Free();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ModifyEventToForceBreak
// Desc: If forced, then clamp event impulse to a value that will cause a reasonable break
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::ModifyEventToForceBreak(EventPhysCollision* pPhysEvent)
{
	if (pPhysEvent)
	{
		const float forceBreakSpeed = 20.0f;
		if (pPhysEvent->vloc[PHYSEVENT_COLLIDER].GetLengthSquared() < forceBreakSpeed * forceBreakSpeed)
		{
			pPhysEvent->vloc[PHYSEVENT_COLLIDER].NormalizeSafe();
			pPhysEvent->vloc[PHYSEVENT_COLLIDER] *= forceBreakSpeed;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: AssertUnusedIfDisabled
// Desc: Asserts that if the system is currently disabled, it should have no used nodes
//--------------------------------------------------------------------------------------------------
void CBreakableGlassSystem::AssertUnusedIfDisabled()
{
#ifndef RELEASE
	CRY_ASSERT_MESSAGE(m_enabled || m_glassPlanes.Count() == 0, "Breakable glass system is disabled, should not have any active planes.");
#endif
}//-------------------------------------------------------------------------------------------------
