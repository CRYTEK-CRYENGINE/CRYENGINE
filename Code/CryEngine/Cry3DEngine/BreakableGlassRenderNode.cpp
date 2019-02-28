// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Includes
#include "StdAfx.h"
#include "BreakableGlassRenderNode.h"

#include "3dEngine.h"
#include <CryPhysics/IPhysics.h>
#include <CryParticleSystem/IParticles.h>
#include <CryParticleSystem/ParticleParams.h>
#include <CryAction/IMaterialEffects.h>
#include <CryRenderer/IShader.h>

// Constants
#define DEFAULT_MAX_VIEW_DIST       1000.0f
#define GLASS_IMPACT_BOUNDARY       0.01f
#define LOOSE_FRAG_LIFETIME         4.5f
#define PHYS_FRAG_SHATTER_SIZE      0.65f

#define PHYSEVENT_COLLIDER          0

#define GLASS_TINT_COLOUR_PARAM     "TintColor"
#define GLASS_TINT_CLOUDINESS_PARAM "TintCloudiness"

// State flags
enum EGlassRNState
{
	EGlassRNState_Initial     = 0,
	EGlassRNState_Weakened    = 1 << 0,
	EGlassRNState_Shattering  = 1 << 1,
	EGlassRNState_Shattered   = 1 << 2,
	EGlassRNState_ActiveFrags = 1 << 3
};

// Statics
const SBreakableGlassCVars* CBreakableGlassRenderNode::s_pCVars = NULL;

// Error logging
#ifndef RELEASE
	#define LOG_GLASS_ERROR(...) CryLogAlways("[BreakGlassSystem Error]: " __VA_ARGS__)
#else
	#define LOG_GLASS_ERROR(...)
#endif

//--------------------------------------------------------------------------------------------------
// Name: CBreakableGlassRenderNode
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CBreakableGlassRenderNode::CBreakableGlassRenderNode()
	: m_pBreakableGlassRE(NULL)
	, m_pPhysEnt(NULL)
	, m_glassTintColour(0.5f, 0.5f, 0.5f, 0.5f)
	, m_id(0)
	, m_state(EGlassRNState_Initial)
	, m_nextPhysFrag(0)
{
	GetInstCount(GetRenderNodeType())++;

	m_matrix.SetIdentity();
	m_planeBBox.Reset();
	m_fragBBox.Reset();
	m_maxViewDist = DEFAULT_MAX_VIEW_DIST;

	// Start with full array
	for (int i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
	{
		m_physFrags.push_back();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CBreakableGlassRenderNode
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CBreakableGlassRenderNode::~CBreakableGlassRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	gEnv->p3DEngine->FreeRenderNodeState(this);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: InitialiseNode
// Desc: Initialises render node
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassRenderNode::InitialiseNode(const SBreakableGlassInitParams& params, const Matrix34& matrix)
{
	// Store param data
	SetMaterial(params.pGlassMaterial);
	memcpy(&m_glassParams, &params, sizeof(SBreakableGlassInitParams));

	// Create glass element
	if (m_pBreakableGlassRE = static_cast<CREBreakableGlass*>(gEnv->pRenderer->EF_CreateRE(eDATA_BreakableGlass)))
	{
		m_pBreakableGlassRE->InitialiseRenderElement(params);
	}

	// Update matrix
	SetMatrix(matrix);

	// Finally, physicalize glass
	PhysicalizeGlass();

	// Flag for cubemap update if required
	IMaterial* pMaterial = m_glassParams.pGlassMaterial;
	if (pMaterial && (pMaterial->GetFlags() & MTL_FLAG_REQUIRE_NEAREST_CUBEMAP))
	{
		m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
	}
	else
	{
		m_nInternalFlags &= ~IRenderNode::REQUIRES_NEAREST_CUBEMAP;
	}

	return true;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReleaseNode
// Desc: Releases render node
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::ReleaseNode(bool bImmediate)
{
	CRY_ASSERT((m_dwRndFlags & ERF_PENDING_DELETE) == 0);

	// Remove all remaining glass physics
	DephysicalizeGlass();

	const int numPhysFrags = m_physFrags.size();
	SGlassPhysFragment* pPhysFrags = m_physFrags.begin();

	for (int i = 0; i < numPhysFrags; ++i)
	{
		DephysicalizeGlassFragment(pPhysFrags[i]);
	}

	// Release glass data
	SAFE_RELEASE(m_glassParams.pGlassMaterial);

	if (m_pBreakableGlassRE)
	{
		m_pBreakableGlassRE->Release(false);
		m_pBreakableGlassRE = NULL;
	}

	bImmediate ? gEnv->p3DEngine->UnRegisterEntityDirect(this) : gEnv->p3DEngine->UnRegisterEntityAsJob(this);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetId
// Desc: Sets the node's id
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::SetId(const uint16 id)
{
	m_id = id;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetId
// Desc: Returns the node's id
//--------------------------------------------------------------------------------------------------
uint16 CBreakableGlassRenderNode::GetId()
{
	return m_id;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PhysicalizeGlass
// Desc: Creates the physics representation of the plane
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::PhysicalizeGlass()
{
	// Generate aabb
	primitives::box box;
	box.Basis.SetIdentity();
	box.center = Vec3(m_glassParams.size.x, m_glassParams.size.y, m_glassParams.thickness) * 0.5f;
	box.size = box.center;

	// Create box geometry
	IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld;
	IGeomManager* pGeomMan = pPhysWorld->GetGeomManager();

	if (IGeometry* pBoxGeom = pGeomMan->CreatePrimitive(primitives::box::type, &box))
	{
		if (phys_geometry* pPhysGeom = pGeomMan->RegisterGeometry(pBoxGeom))
		{
			// Handle ref internally for auto-release w/ phys entity
			pPhysGeom->nRefCount = 0;

			// Physicalize box
			if (m_pPhysEnt = pPhysWorld->CreatePhysicalEntity(PE_RIGID, NULL, NULL, PHYS_FOREIGN_ID_BREAKABLE_GLASS))
			{
				// Set box surface params
				pe_geomparams geomParams;
				geomParams.flags = geom_collides;
				geomParams.mass = 0.0f; // Magic number for a static object
				geomParams.nMats = 1;
				geomParams.pMatMapping = &m_glassParams.surfaceTypeId;
				m_pPhysEnt->AddGeometry(pPhysGeom, &geomParams);

				// Set transformation
				pe_params_pos transParams;
				transParams.pMtx3x4 = &m_matrix;
				m_pPhysEnt->SetParams(&transParams);

				// Set foreign data to flag this as our glass
				pe_params_foreign_data foreignData;
				foreignData.pForeignData = this;
				foreignData.iForeignData = PHYS_FOREIGN_ID_BREAKABLE_GLASS;
				m_pPhysEnt->SetParams(&foreignData);

				// Limit number of logged collisions
				pe_simulation_params simParams;
				simParams.maxLoggedCollisions = 2;
				m_pPhysEnt->SetParams(&simParams);
			}
		}

		pBoxGeom->Release();
	}

	// Default state
	m_state = EGlassRNState_Initial;
	m_lastGlassState = SBreakableGlassState();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DephysicalizeGlass
// Desc: Removes the physics representation of the plane
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::DephysicalizeGlass()
{
	if (m_pPhysEnt)
	{
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pPhysEnt);
		m_pPhysEnt = NULL;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PhysicalizeGlassFragment
// Desc: Creates the physics representation of a glass fragment
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::PhysicalizeGlassFragment(SGlassPhysFragment& physFrag, const Vec3& centerOffset)
{
	if (!physFrag.m_pPhysEnt)
	{
		const float density = 900.0f; // From breakable2DParams

		// Calculate transformation
		pe_params_pos transParams;
		transParams.pos = physFrag.m_matrix.TransformPoint(centerOffset);
		transParams.q = Quat(physFrag.m_matrix);

		// Physicalize particle
		if (physFrag.m_pPhysEnt = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_PARTICLE, &transParams))
		{
			pe_params_particle particle;
			particle.size = physFrag.m_size;
			particle.thickness = m_glassParams.thickness;
			particle.surface_idx = m_glassParams.surfaceTypeId;
			particle.mass = particle.size * particle.thickness * density;
			particle.flags = particle_no_path_alignment | particle_no_roll | particle_traceable | particle_no_self_collisions;

			// Large fragments get logged so we can destroy them on a collision
			const float shatterSize = 0.65f;
			if (physFrag.m_size >= shatterSize)
			{
				particle.flags |= pef_log_collisions;
			}

			particle.normal = Vec3Constants<float>::fVec3_OneZ;
			particle.q0 = transParams.q;
			particle.pColliderToIgnore = m_pPhysEnt; // Ignore parent plane
			physFrag.m_pPhysEnt->SetParams(&particle);

			// Set foreign data to flag this as our glass
			pe_params_foreign_data foreignData;
			foreignData.pForeignData = &physFrag;
			foreignData.iForeignData = PHYS_FOREIGN_ID_BREAKABLE_GLASS_FRAGMENT;
			physFrag.m_pPhysEnt->SetParams(&foreignData);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DephysicalizeGlassFragment
// Desc: Removes the physics representation of a glass fragment
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::DephysicalizeGlassFragment(SGlassPhysFragment& physFrag)
{
	// Remove physical entity
	if (physFrag.m_pPhysEnt)
	{
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(physFrag.m_pPhysEnt);
		physFrag.m_pPhysEnt = NULL;

		// Push this into the dead list for later syncing
		uint16 physFragData = physFrag.m_bufferIndex;
		physFragData |= (uint16)physFrag.m_fragIndex << 8;
		m_deadPhysFrags.push_back(physFragData);
	}

	// Zero lifetime as now dead
	physFrag.m_lifetime = 0.0f;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates every frame
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::Update(SBreakableGlassUpdateParams& params)
{
	if (m_pBreakableGlassRE)
	{
		// Allow glass to update and isolate fragments to be physicalized
		TGlassPhysFragmentArray newPhysFrags;
		TGlassPhysFragmentInitArray newPhysFragsInitData;

		params.m_pPhysFrags = &newPhysFrags;
		params.m_pPhysFragsInitData = &newPhysFragsInitData;
		params.m_pDeadPhysFrags = &m_deadPhysFrags;

		m_pBreakableGlassRE->Update(params);

		// Dealt with dead fragments
		m_deadPhysFrags.clear();

		// Arrays should always be in sync
		if (newPhysFrags.size() != newPhysFragsInitData.size())
		{
			CRY_ASSERT_MESSAGE(0, "Glass physical fragment arrays are out of sync.");
			LOG_GLASS_ERROR("Physical fragment arrays are out of sync.");
		}

		// Sync with glass element if data has changed
		if (params.m_geomChanged)
		{
			UpdateGlassState(NULL);
		}

		// Reset fragment bounding box
		m_fragBBox.min = m_fragBBox.max = m_planeBBox.GetCenter();
		float maxFragSize = 0.0f;

		// Add any newly created fragments
		const int numNewPhysFrags = newPhysFrags.size();
		bool fragsActive = (numNewPhysFrags > 0);

		for (int i = 0; i < numNewPhysFrags; ++i)
		{
			// Recycle next fragment
			SGlassPhysFragment& physFrag = m_physFrags[m_nextPhysFrag];
			m_nextPhysFrag = m_nextPhysFrag + 1 < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS ? m_nextPhysFrag + 1 : 0;

			// Remove if slot still in use
			if (physFrag.m_pPhysEnt)
			{
				DephysicalizeGlassFragment(physFrag);
			}

			// Initialise
			const SGlassPhysFragmentInitData& initData = newPhysFragsInitData[i];
			physFrag = newPhysFrags[i];

			PhysicalizeGlassFragment(physFrag, initData.m_center);

			physFrag.m_pRenderNode = this;
			physFrag.m_lifetime = LOOSE_FRAG_LIFETIME;

			if (physFrag.m_pPhysEnt)
			{
				// Apply initial impulse
				pe_action_impulse applyImpulse;
				applyImpulse.impulse = initData.m_impulse;
				applyImpulse.point = initData.m_impulsePt;

				physFrag.m_pPhysEnt->Action(&applyImpulse);

				// Update bounding box
				m_fragBBox.Add(physFrag.m_matrix.GetTranslation());
				maxFragSize = max(maxFragSize, physFrag.m_size);
			}
		}

		// Update existing loose glass fragments
		const float frametime = params.m_frametime;

		for (int i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
		{
			SGlassPhysFragment& physFrag = m_physFrags[i];

			if (IPhysicalEntity* pPhysEnt = physFrag.m_pPhysEnt)
			{
				fragsActive = true;

				if (physFrag.m_lifetime > 0.0f)
				{
					// Age
					physFrag.m_lifetime -= frametime;

					// Sync matrix
					pe_status_pos state;
					state.pMtx3x4 = &physFrag.m_matrix;
					pPhysEnt->GetStatus(&state);

					// Update bounding box
					m_fragBBox.Add(physFrag.m_matrix.GetTranslation());
					maxFragSize = max(maxFragSize, physFrag.m_size);
				}
				else
				{
					DephysicalizeGlassFragment(physFrag);
				}
			}
		}

		// Update state flags
		if (fragsActive)
		{
			m_state |= EGlassRNState_ActiveFrags;
		}
		else
		{
			m_state &= ~EGlassRNState_ActiveFrags;
		}

		// Expand box to encompass largest fragment
		m_fragBBox.Expand(Vec3(maxFragSize, maxFragSize, maxFragSize));
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: HasGlassShattered
// Desc: Used to determine if the glass has fully shattered
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassRenderNode::HasGlassShattered()
{
	return (m_state & EGlassRNState_Shattered) ? true : false;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: HasActiveFragments
// Desc: Used to determine if there are any physicalized fragments still active
//--------------------------------------------------------------------------------------------------
bool CBreakableGlassRenderNode::HasActiveFragments()
{
	return (m_state & EGlassRNState_ActiveFrags) ? true : false;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ApplyImpactToGlass
// Desc: Top level impact handler, passes impact down and updates state
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::ApplyImpactToGlass(const EventPhysCollision* pPhysEvent)
{
	if (m_pBreakableGlassRE && pPhysEvent)
	{
		Vec2 impactPt;
		CalculateImpactPoint(pPhysEvent->pt, impactPt);

		// Break glass if point is within actual bounds
		SGlassImpactParams params;
		params.x = impactPt.x;
		params.y = impactPt.y;

		params.seed = cry_random_uint32();

		// Copy main params
		params.speed = pPhysEvent->vloc[PHYSEVENT_COLLIDER].len();
		params.velocity = pPhysEvent->vloc[PHYSEVENT_COLLIDER];
		params.impulse = params.speed;

		// Try to calculate impulse if valid entity set (Invalid on playback breaks)
		if (IPhysicalEntity* pCollider = pPhysEvent->pEntity[PHYSEVENT_COLLIDER])
		{
			pe_status_dynamics dynamics;
			if (pCollider->GetStatus(&dynamics))
			{
				params.impulse *= dynamics.mass;
			}
		}
		else if (pPhysEvent->mass[PHYSEVENT_COLLIDER] > 0.0f)
		{
			params.impulse *= pPhysEvent->mass[PHYSEVENT_COLLIDER];
		}

		// Set impact radius to size of entity
		if (IPhysicalEntity* pPhysEntity = pPhysEvent->pEntity[PHYSEVENT_COLLIDER])
		{
			// Test for particle or rigid entity bodies
			pe_params_particle particle;
			if (pPhysEntity->GetParams(&particle))
			{
				params.radius = particle.size;
			}
			else if (pPhysEntity->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY)
			{
				if (IEntity* pEntity = (IEntity*)pPhysEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
				{
					AABB aabb;
					pEntity->GetLocalBounds(aabb);

					const float aabbRadiusScale = 0.9f;
					params.radius = aabb.GetRadius() * aabbRadiusScale;
				}
			}
		}

		// Pass impact data into sim
		m_pBreakableGlassRE->ApplyImpactToGlass(params);

		// Process any changes in glass
		UpdateGlassState(pPhysEvent);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ApplyExplosionToGlass
// Desc: Passes on the data to the RE and updates glass state
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::ApplyExplosionToGlass(const EventPhysCollision* pPhysEvent)
{
	if (m_pBreakableGlassRE && pPhysEvent)
	{
		Vec2 impactPt;
		CalculateImpactPoint(pPhysEvent->pt, impactPt);

		// Break glass if point is within actual bounds
		SGlassImpactParams params;
		params.x = impactPt.x;
		params.y = impactPt.y;

		params.seed = cry_random_uint32();

		// Tweak input so values scale better with the system
		params.speed = GLASSCFG_MIN_BULLET_SPEED * 2.0f;
		params.velocity = pPhysEvent->vloc[PHYSEVENT_COLLIDER].GetNormalized() * params.speed;
		params.radius = max(pPhysEvent->radius, 1.0f);

		// Apply a fixed impulse we know will break the glass
		params.impulse = (GLASSCFG_PLANE_SPLIT_IMPULSE + GLASSCFG_PLANE_SHATTER_IMPULSE) * 0.5f;

		// Pass impact data into sim
		m_pBreakableGlassRE->ApplyExplosionToGlass(params);

		// Process any changes in glass
		UpdateGlassState(pPhysEvent);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DestroyPhysFragment
// Desc: Removes a physicalized fragment when it collides
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::DestroyPhysFragment(SGlassPhysFragment* pPhysFrag)
{
	if (pPhysFrag && pPhysFrag->m_pPhysEnt)
	{
		if (pPhysFrag->m_pRenderNode != this)
		{
			CRY_ASSERT_MESSAGE(0, "Glass physicalized fragment being destroyed by invalid render node.");
			LOG_GLASS_ERROR("Physicalized fragment being destroyed by invalid render node.");
		}

		// Remove glass
		DephysicalizeGlassFragment(*pPhysFrag);

		// Play effect
		if (!s_pCVars || s_pCVars->m_particleFXEnable > 0)
		{
			if (m_glassParams.pShatterEffect)
			{
				Vec3 center = pPhysFrag->m_matrix.GetTranslation();
				Vec3 dir = Vec3Constants<f32>::fVec3_OneZ;
				float size = pPhysFrag->m_size;

				float effectScale = s_pCVars ? s_pCVars->m_particleFXScale : 0.25f;

				// Update effect colour to match this glass
				if (!s_pCVars || s_pCVars->m_particleFXUseColours > 0)
				{
					// Blend colour so smaller shatters are whiter/cloudier
					Vec4 particleColour = m_glassTintColour;
					float sizeLerp = min(size, 1.0f);

					particleColour.x += sizeLerp * (1.0f - particleColour.x);
					particleColour.y += sizeLerp * (1.0f - particleColour.y);
					particleColour.z += sizeLerp * (1.0f - particleColour.z);

					SetParticleEffectColours(m_glassParams.pShatterEffect, m_glassTintColour);
				}

				// Spawn effect emitter
				SpawnParams spawnParams;
				spawnParams.fCountScale = effectScale * size;
				spawnParams.fSizeScale = size;

				QuatTS spawnLoc;
				spawnLoc.q = Quat::CreateRotationVDir(dir);
				spawnLoc.t = center;
				spawnLoc.s = 1.f;

				m_glassParams.pShatterEffect->Spawn(spawnLoc, &spawnParams);
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculateImpactPoint
// Desc: Calculates an impact point (if any) from the input position
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::CalculateImpactPoint(const Vec3& pt, Vec2& impactPt)
{
	// Create point in plane's local space
	Matrix34 invPlaneTransMat = m_matrix.GetInvertedFast();
	Vec3 localPt = invPlaneTransMat.TransformPoint(pt);

	// Can assume valid as a phys event led here
	impactPt.x = clamp_tpl<float>(localPt.x, GLASS_IMPACT_BOUNDARY, m_glassParams.size.x - GLASS_IMPACT_BOUNDARY);
	impactPt.y = clamp_tpl<float>(localPt.y, GLASS_IMPACT_BOUNDARY, m_glassParams.size.y - GLASS_IMPACT_BOUNDARY);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateGlassState
// Desc: Spawns/plays glass impact effects
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::UpdateGlassState(const EventPhysCollision* pPhysEvent)
{
	if (m_pBreakableGlassRE && m_pPhysEnt)
	{
		// Get updated glass state
		uint8 lastNumImpacts = m_lastGlassState.m_numImpacts;
		uint8 lastNumLooseFrags = m_lastGlassState.m_numLooseFrags;

		m_pBreakableGlassRE->GetGlassState(m_lastGlassState);
		bool physicsChanged = false;

		// If several loose fragments created, play breakage effect
		const uint8 minFragsForEffect = 2;
		if (m_lastGlassState.m_numLooseFrags > lastNumLooseFrags + minFragsForEffect)
		{
			PlayBreakageEffect(pPhysEvent);
		}

		// If glass shatters from impact, de-physicalize
		if (m_lastGlassState.m_hasShattered && m_pPhysEnt)
		{
			DephysicalizeGlass();
			physicsChanged = true;
		}
		else if (!(m_state & EGlassRNState_Weakened)
		         && !m_lastGlassState.m_hasShattered
		         && m_lastGlassState.m_numImpacts > lastNumImpacts)
		{
			// Flag all parts with no collision response, to allow objects to pass through
			// after first successful impact. We also remove the "man_break" flag as some
			// materials auto-add it during the physics entity initialisation
			pe_params_part colltype;
			colltype.flagsOR = geom_no_coll_response;
			colltype.flagsAND = ~geom_manually_breakable;
			m_pPhysEnt->SetParams(&colltype);

			// Update surface type with more pierceable material
			//      pe_params_part partParams;
			//      partParams.nMats = 1;
			//      partParams.pMatMapping = &m_weakGlassTypeId;
			//      m_pPhysEnt->SetParams(&partParams);

			// Update internal flags
			m_state |= EGlassRNState_Weakened;
			physicsChanged = true;
		}

		// On physical state change, wake all entities resting on this glass so they can fall through
		if (physicsChanged)
		{
			AABB glassArea = m_planeBBox;
			glassArea.Expand(Vec3Constants<f32>::fVec3_One);

			IPhysicalEntity** pEntities = NULL;
			int numEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(glassArea.min, glassArea.max, pEntities, ent_sleeping_rigid | ent_living | ent_ignore_noncolliding);

			for (int i = 0; i < numEntities; ++i)
			{
				if (IPhysicalEntity* pPhysEnt = pEntities[i])
				{
					// Only need to wake up sleeping entities
					pe_status_awake status;
					if (pPhysEnt->GetStatus(&status) == 0)
					{
						pe_action_awake wake;
						wake.bAwake = 1;
						pPhysEnt->Action(&wake);
					}
				}
			}
		}

		// If we no longer have a physical presence, no need to exist
		if (!m_pPhysEnt)
		{
			m_state |= EGlassRNState_Shattering;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetParticleEffectColours
// Desc: Recursively updates all of a particular particle effect's colour params
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::SetParticleEffectColours(IParticleEffect* pEffect, const Vec4& rgba)
{
	if (pEffect)
	{
		// Set this node
		ParticleParams partParams = pEffect->GetParticleParams();
		partParams.cColor.Set(Color3F(rgba.x, rgba.y, rgba.z));
		partParams.fAlpha.Set(rgba.w);
		pEffect->SetParticleParams(partParams);

		// Recurse through children
		const int numSubEffects = pEffect->GetChildCount();
		for (int i = 0; i < numSubEffects; ++i)
		{
			SetParticleEffectColours(pEffect->GetChild(i), rgba);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PlayBreakageEffect
// Desc: Spawns/plays glass impact breakage effects
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::PlayBreakageEffect(const EventPhysCollision* pPhysEvent)
{
	if (m_glassParams.pGlassMaterial && pPhysEvent)
	{
		if (IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects)
		{
			// Set effect params
			SMFXBreakageParams breakParams;
			breakParams.SetHitPos(pPhysEvent->pt);
			breakParams.SetVelocity(pPhysEvent->vloc[PHYSEVENT_COLLIDER]);
			breakParams.SetMass(0.0f);

			// Play breakage effect
			pMaterialEffects->PlayBreakageEffect(m_glassParams.pGlassMaterial->GetSurfaceType(), "Breakage", breakParams);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetName
// Desc: Gets the node's name
//--------------------------------------------------------------------------------------------------
const char* CBreakableGlassRenderNode::GetName() const
{
	return "BreakableGlass";
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetEntityClassName
// Desc: Gets the entity's class name
//--------------------------------------------------------------------------------------------------
const char* CBreakableGlassRenderNode::GetEntityClassName() const
{
	return "BreakableGlass";
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetMemoryUsage
// Desc: Gets the node's memory usage
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "BreakableGlassRenderNode");
	pSizer->AddObject(this, sizeof(*this));
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetMaterial
// Desc: Sets the node's material
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::SetMaterial(IMaterial* pMaterial)
{
	// Update material pointer if changed
	if (pMaterial == m_glassParams.pGlassMaterial)
		return;

	if (m_glassParams.pGlassMaterial)
	{
		m_glassParams.pGlassMaterial->Release();
	}

	m_glassParams.pGlassMaterial = pMaterial;

	if (m_glassParams.pGlassMaterial)
	{
		m_glassParams.pGlassMaterial->AddRef();

		// Extract glass tint colour parameters
		const SShaderItem& shaderItem = pMaterial->GetShaderItem();
		float tintCloudiness = 0.0f;

		if (shaderItem.m_pShaderResources && shaderItem.m_pShader)
		{
			DynArrayRef<SShaderParam>& shaderParams = shaderItem.m_pShaderResources->GetParameters();
			const int numParams = shaderParams.size();

			for (int i = 0; i < numParams; ++i)
			{
				if (SShaderParam* sp = &shaderParams[i])
				{
					if (!stricmp(sp->m_Name, GLASS_TINT_COLOUR_PARAM)
					    || !stricmp(sp->m_Name, GLASS_TINT_CLOUDINESS_PARAM))
					{
						switch (sp->m_Type)
						{
						case eType_FLOAT:
							tintCloudiness = sp->m_Value.m_Float;
							break;

						case eType_FCOLOR:
							m_glassTintColour[0] = sp->m_Value.m_Vector[0];
							m_glassTintColour[1] = sp->m_Value.m_Vector[1];
							m_glassTintColour[2] = sp->m_Value.m_Vector[2];
							break;
						}
					}
				}
			}

			// Blend diffuse colour with tint colour based on cloudiness
			const ColorF& diffuse = shaderItem.m_pShaderResources->GetColorValue(EFTT_DIFFUSE);

			m_glassTintColour.x = LERP(diffuse.r * m_glassTintColour.x, diffuse.r + m_glassTintColour.x, tintCloudiness);
			m_glassTintColour.y = LERP(diffuse.g * m_glassTintColour.y, diffuse.g + m_glassTintColour.y, tintCloudiness);
			m_glassTintColour.z = LERP(diffuse.b * m_glassTintColour.z, diffuse.b + m_glassTintColour.z, tintCloudiness);

			// Finally store alpha
			m_glassTintColour.w = shaderItem.m_pShaderResources->GetStrengthValue(EFTT_OPACITY);
		}
		else
		{
			// Default to mid-grey
			m_glassTintColour = Vec4(0.5f, 0.5f, 0.5f, 0.5f);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetMatrix
// Desc: Sets the node's matrix and updates the bbox
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::SetMatrix(const Matrix34& matrix)
{
	if (m_matrix == matrix)
		return;

	gEnv->p3DEngine->UnRegisterEntityAsJob(this);
	m_matrix = matrix;

	// Update world-space bounding box
	m_planeBBox.min.Set(0.0f, 0.0f, 0.0f);
	m_planeBBox.max.Set(m_glassParams.size.x, m_glassParams.size.y, m_glassParams.thickness);
	m_planeBBox.SetTransformedAABB(m_matrix, m_planeBBox);

	// Update render element
	if (SBreakableGlassREParams* pREParams = (SBreakableGlassREParams*)m_pBreakableGlassRE->GetParams())
	{
		pREParams->matrix = m_matrix;
	}

	// Update physics entity
	if (m_pPhysEnt)
	{
		pe_params_pos transParams;
		transParams.pMtx3x4 = &m_matrix;
		m_pPhysEnt->SetParams(&transParams);
	}

	gEnv->p3DEngine->RegisterEntity(this);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetBBox
// Desc: Sets the node's bbox
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::SetBBox(const AABB& worldSpaceBoundingBox)
{
	m_planeBBox = worldSpaceBoundingBox;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetPhysics
// Desc: Gets the node's physics
//--------------------------------------------------------------------------------------------------
IPhysicalEntity* CBreakableGlassRenderNode::GetPhysics() const
{
	return m_pPhysEnt;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetPhysics
// Desc: Sets the node's physics
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::SetPhysics(IPhysicalEntity* pPhysics)
{
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetMaterialOverride
// Desc: Returns the material override
//--------------------------------------------------------------------------------------------------
IMaterial* CBreakableGlassRenderNode::GetMaterialOverride()
{
	return m_glassParams.pGlassMaterial;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetCVars
// Desc: Updates the cvar pointer
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::SetCVars(const SBreakableGlassCVars* pCVars)
{
	s_pCVars = pCVars;

	if (m_pBreakableGlassRE)
	{
		m_pBreakableGlassRE->SetCVars(pCVars);
	}
}
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Render
// Desc: Renders the node
//--------------------------------------------------------------------------------------------------
void CBreakableGlassRenderNode::Render(const SRendParams& renderParams, const SRenderingPassInfo& passInfo)
{
	if (m_glassParams.pGlassMaterial && m_pBreakableGlassRE)
	{
		IMaterial* pMaterial = m_glassParams.pGlassMaterial;

		// Set render element parameters
		SBreakableGlassREParams* pREParams = (SBreakableGlassREParams*)m_pBreakableGlassRE->GetParams();

		const CCamera& camera = passInfo.GetCamera();
		const Vec3 cameraPos = camera.GetPosition();
		pREParams->centre = cameraPos;

		const int beforeWater = 0;
		const int afterWater = 1;
		const bool isShattering = (m_state & EGlassRNState_Shattering) ? true : false;

		if (!m_lastGlassState.m_hasShattered || isShattering)
		{
			// We allow one final draw to avoid popping during updates
			if (isShattering)
			{
				m_state &= ~EGlassRNState_Shattering;
				m_state |= EGlassRNState_Shattered;
			}

			// Submit render object for cracks
			if (CRenderObject* pRenderObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject())
			{
				// Set render object properties
				pRenderObject->SetMatrix(m_matrix, passInfo);
				pRenderObject->m_ObjFlags |= FOB_TRANS_MASK;
				pRenderObject->m_fSort = 0;
				pRenderObject->m_fDistance = renderParams.fDistance;
				pRenderObject->m_pCurrMaterial = NULL;  // Null flags as cracks
				pRenderObject->m_pRenderNode = this;
				pRenderObject->SetAmbientColor(renderParams.AmbientColor, passInfo);
				pRenderObject->m_breakableGlassSubFragIndex = GLASSCFG_GLASS_PLANE_FLAG_LOD;
				pRenderObject->m_nTextureID = renderParams.nTextureID;

				// Add render element and render object to render list
				const auto transparentList = !(pRenderObject->m_ObjFlags & FOB_AFTER_WATER) ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW;
				passInfo.GetIRenderView()->AddRenderObject(m_pBreakableGlassRE, pMaterial->GetShaderItem(), pRenderObject, passInfo, transparentList, beforeWater);
			}

			// Submit render object for plane
			if (CRenderObject* pRenderObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject())
			{
				// Offset glass surface towards edge nearest viewer
				const Vec3 normal = m_matrix.GetColumn2().GetNormalizedFast();
				const Vec3 camDir = (m_matrix.GetTranslation() - cameraPos).GetNormalizedFast();

				const float viewerSide = normal.Dot(camDir) > 0.0f ? -1.0f : 1.0f;
				const float halfThickness = m_glassParams.thickness * 0.5f;
				Vec3 offset = normal * (halfThickness * viewerSide);

				// Set render object properties
				Matrix34 mat = m_matrix;
				mat.AddTranslation(offset);
				pRenderObject->SetMatrix(mat, passInfo);
				pRenderObject->m_ObjFlags |= FOB_TRANS_MASK;
				pRenderObject->m_fSort = 0;
				pRenderObject->m_fDistance = renderParams.fDistance;
				pRenderObject->m_pCurrMaterial = pMaterial;  // Material flags as plane
				pRenderObject->m_pRenderNode = this;
				pRenderObject->SetAmbientColor(renderParams.AmbientColor, passInfo);
				pRenderObject->m_breakableGlassSubFragIndex = GLASSCFG_GLASS_PLANE_FLAG_LOD;
				pRenderObject->m_nTextureID = renderParams.nTextureID;

				// Add render element and render object to render list
				const auto transparentList = !(pRenderObject->m_ObjFlags & FOB_AFTER_WATER) ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW;
				passInfo.GetIRenderView()->AddRenderObject(m_pBreakableGlassRE, pMaterial->GetShaderItem(), pRenderObject, passInfo, transparentList, afterWater);
			}
		}

		// Draw any physicalized fragment cracks
		SGlassPhysFragment* pPhysFrags = m_physFrags.begin();
		CRY_ASSERT(GLASSCFG_MAX_NUM_PHYS_FRAGMENTS == m_physFrags.max_size());

		for (int i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
		{
			SGlassPhysFragment& physFrag = pPhysFrags[i];

			if (physFrag.m_pPhysEnt || !physFrag.m_initialised)
			{
				// Calculate distance and fragment alpha
				const float dist = (cameraPos - physFrag.m_matrix.GetTranslation()).GetLengthFast();
				const float alpha = min(physFrag.m_lifetime * physFrag.m_lifetime, 1.0f);

				// Submit render object for full fragment
				if (CRenderObject* pRenderObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject())
				{
					// All phys fragments have to be drawn at least once regardless of state
					// - This allows internal buffer states/syncs to remain balanced
					physFrag.m_initialised = true;

					// Set render object properties
					pRenderObject->SetMatrix(physFrag.m_matrix, passInfo);
					pRenderObject->m_ObjFlags |= FOB_TRANS_MASK;
					pRenderObject->m_fSort = 0;
					pRenderObject->m_fAlpha = alpha;
					pRenderObject->m_fDistance = dist;
					pRenderObject->m_pCurrMaterial = pMaterial;  // Material flags as plane
					pRenderObject->m_pRenderNode = this;
					pRenderObject->SetAmbientColor(renderParams.AmbientColor, passInfo);
					pRenderObject->m_breakableGlassSubFragIndex = physFrag.m_bufferIndex;
					pRenderObject->m_nTextureID = renderParams.nTextureID;

					// Add render element and render object to render list
					const auto transparentList = !(pRenderObject->m_ObjFlags & FOB_AFTER_WATER) ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW;
					passInfo.GetIRenderView()->AddRenderObject(m_pBreakableGlassRE, pMaterial->GetShaderItem(), pRenderObject, passInfo, transparentList, afterWater);
				}

#ifdef GLASS_DEBUG_MODE
				// Draw debug data
				if (s_pCVars && s_pCVars->m_drawFragData)
				{
					m_pBreakableGlassRE->DrawFragmentDebug(physFrag.m_fragIndex, physFrag.m_matrix, physFrag.m_bufferIndex, alpha);
				}
#endif
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

void CBreakableGlassRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_planeBBox.Move(delta);
	m_fragBBox.Move(delta);
	m_matrix.SetTranslation(m_matrix.GetTranslation() + delta);
	if (m_pPhysEnt)
	{
		pe_params_pos par_pos;
		par_pos.pos = m_matrix.GetTranslation();
		m_pPhysEnt->SetParams(&par_pos);
	}
}

void CBreakableGlassRenderNode::FillBBox(AABB& aabb)
{
	aabb = CBreakableGlassRenderNode::GetBBox();
}

const AABB CBreakableGlassRenderNode::GetBBox() const
{
	AABB bbox = m_planeBBox;
	bbox.Add(m_fragBBox);
	return bbox;
}

EERType CBreakableGlassRenderNode::GetRenderNodeType()
{
	return eERType_BreakableGlass;
}

float CBreakableGlassRenderNode::GetMaxViewDist()
{
	return m_maxViewDist;
}

Vec3 CBreakableGlassRenderNode::GetPos(bool worldOnly) const
{
	return m_matrix.GetTranslation();
}

IMaterial* CBreakableGlassRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_glassParams.pGlassMaterial;
}
