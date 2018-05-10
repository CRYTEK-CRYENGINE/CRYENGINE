// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BreakReplicator.h"
#include "GameContext.h"
#include "ObjectSelector.h"
#include "DebugBreakage.h"
#include "BreakReplicator_Simple.h"
#include "SerializeDirHelper.h"
#include "BreakReplicatorGameObject.h"
#include "NetworkCVars.h"

/*
   ---------------------------------------------------------------------------------
   TODO:

    - * Host Migration *

    - Send pos in part space for all break types
      - But mainly glass

    - Better memory allocation instead of DynArray

   ---------------------------------------------------------------------------------
 */

#if DEBUG_NET_BREAKAGE
	#define DEBUG_ASSERT(x) { if (!(x)) { int* p = 0; *p = 42; } } // crash on purpose!
#else
	#define DEBUG_ASSERT(x)
#endif

// Tree Syncing
#define NET_SYNC_TREES 0

#if NET_USE_SIMPLE_BREAKAGE

	#define NET_MAX_BREAKS 4096

class BreakStream;

	#define NET_MAX_NUMBER_OF_PARTS                     63

	#define WORLD_POS_SAMPLE_DISTANCE_APPART_INACCURATE 2.0f
	#define WORLD_POS_SAMPLE_DISTANCE_APPART_ACCURATE   0.005f

struct EmittedProduct
{
	IPhysicalEntity* pent;      // a physics entity that came off
	int              partidSrc; // The part id. Note that a gib can contain more than one part. At the moment we just store the part that has the smallest index
	Vec3             initialVel;
	Vec3             initialAngVel;
};

struct JointBreakInfo
{
	int idJoint;
	int partidEpicenter;
};

/*
   =======================================================================
   BreakStream
   =======================================================================

   A BreakStream holds all the info regarding a particular break.
   On the server it is in k_recording mode and on a client it is
   in k_playing mode

   Only one BreakStream can be applied to an IPhysicalEntity at any one time
   Each stream has an index associated with it, which is ascending in time.
   The server decides the break index.

 */

	#define NET_MAX_NUM_FRAMES_TO_FIND_ENTITY                 200
	#define NET_MAX_NUM_FRAMES_TO_WAIT_FOR_BREAK_DEPENDENCY   200
	#define NET_LEVEL_LOAD_TIME_BEFORE_USING_INAVLID_COUNTERS 15.f

static BreakStream s_dummyStream;

static const char* BreakStreamGetModeName(uint32 mode)
{
	enum EMode {k_invalid = 0, k_recording, k_playing, k_finished, k_numModes};
	#if DEBUG_NET_BREAKAGE
	static const char* names[BreakStream::k_numModes + 1] = {
		"invalid",
		"k_recording",
		"k_playing",
		"k_finished",
		"MODE ERROR",
	};
	return names[min((uint32)mode, (uint32)BreakStream::k_numModes)];
	#endif
	return "";
}

static const char* BreakStreamGetTypeName(uint32 type)
{
	#if DEBUG_NET_BREAKAGE
	static const char* names[BreakStream::k_numTypes + 1] = {
		"unknown",
		"PartBreak",
		"PlaneBreak",
		"DeformBreak",
		"TYPE ERROR",
	};
	return names[min((uint32)type, (uint32)BreakStream::k_numTypes)];
	#endif
	return "";
}

/*
   =======================================================================
   Part BreakStream
   =======================================================================
 */

class PartBreak : public BreakStream
{
public:
	PartBreak();
	virtual ~PartBreak();

	virtual void    Init(EMode mode);

	virtual void    SerializeWith(CBitArray& array);

	virtual bool    OnCreatePhysEntityPart(const EventPhysCreateEntityPart* pEvent);
	virtual bool    OnRemovePhysEntityParts(const EventPhysRemoveEntityParts* pEvent);
	virtual bool    OnJointBroken(const EventPhysJointBroken* pEvent);
	virtual void    OnEndFrame();
	virtual void    Playback();

	EmittedProduct* FindEmittedProduct(const EventPhysCreateEntityPart* pEvent, bool add);
	virtual void    OnSend(SNetBreakDescription& desc);

public:
	EmittedProduct* m_products;
	JointBreakInfo* m_jointsBroken;

	uint8           m_numProducts;
	uint8           m_numJointsBroken;
	uint8           m_numProductsCollected;
};

/*
   =======================================================================
   Plane BreakStream
   =======================================================================
 */

// class PlaneBreak : public BreakStream
// {
// public:
//  PlaneBreak();
//  virtual ~PlaneBreak();

//  virtual void SerializeWith(CBitArray& array);

//  virtual void OnSend( SNetBreakDescription& desc);

//  virtual void Playback();

//  virtual bool IsWaitinOnRenderMesh();

//  virtual bool IsRenderMeshReady();

// public:
//  SBreakEvent m_be;
//  IStatObj* m_pStatObj;
// };

/*
   =======================================================================
   Deforming BreakStream
   =======================================================================
 */

class DeformBreak : public BreakStream
{
public:
	DeformBreak();
	~DeformBreak();

	virtual void SerializeWith(CBitArray& array);

	virtual void OnSpawnEntity(IEntity* pEntity);
	virtual void OnSend(SNetBreakDescription& desc);

	virtual void Playback();

public:
	SBreakEvent        m_be;
	DynArray<EntityId> m_spawnedEntities;
};

/*
   =====================================================================
   Part Breakage
   =====================================================================

   For something that should be fairly simple, this is a nightmare of events that have to be collated and reconstructed


   If an item can break as such p1 (A,B,C,D, E,F)   -> p1 (A,B,C)   and new p2 (D,E,F)

   We would get these events:

   One,

   EventPhysRemoveEntityParts
          pEntity = p1
          partIds = Bit(D)| Bit(E) | Bit(F);

   And three,

   EventPhysCreateEntityPart
          pEntity = p1
          pEntNew = p2
          partidSrc = D
          nTotParts = 3

   EventPhysCreateEntityPart
          pEntity = p1
          pEntNew = p2
          partidSrc = E
          nTotParts = 3

   EventPhysCreateEntityPart
          pEntity = p1
          pEntNew = p2
          partidSrc = F
          nTotParts = 3

   In breakable manager, the first breaks creates a new temp IEntity to hold the remainding piece (A,B,C)
   This entity is identified through rep->OnSpawn()
   A second entity/or particle is created for the piece (D,E,F)
   The initial velocity is serialised.

 */

/*
   =====================================================================
   SClientGlassBreak
   =====================================================================
   used to inform the server that the client
   broke the glass
 */

// struct SClientGlassBreak
// {
//  void SerializeWith( TSerialize ser )
//  {
//    CBitArray array(&ser);
//    m_planeBreak.SerializeWith(array);
//    if (!array.IsReading())
//      array.WriteToSerializer();
//  }

//  PlaneBreak m_planeBreak;
// };

/*
   =====================================================================
   Object Identification
   =====================================================================

   This is a pain! Entitys have entIds and can be serialised over the network.
   But static things like render nodes, brushes, and foliage have no way to be identified.
   Also they may not exist on the client until they are "activated" by the physics on demand system.
   Making identification difficult!
   Current method is to serialise a pos+hash
 */

void CObjectIdentifier::Reset()
{
	m_objType = k_unknown;
	m_holdingEntity = 0;
	m_objCenter.zero();
	m_objHash = 0;
}

void CObjectIdentifier::SerializeWith(CBitArray& array, bool includeObjCenter)
{
	// This is such a horrible way to identify stuff over
	// the network, we seriously need to do better!
	if (array.IsReading())
	{
		Reset();
	}
	int bStatic = iszero((int)m_objType - (int)k_static_entity);
	array.Serialize(bStatic, 0, 1);
	if (bStatic)
	{
		if (includeObjCenter)
		{
			CBreakReplicator::SerialisePosition(array, m_objCenter, CNetworkCVars::Get().BreakMaxWorldSize, CBreakReplicator::m_inaccurateWorldPosNumBits); // If you change this, then you might need to adjust CObjectSelector_Eps
		}
		array.Serialize(m_objHash, 0, 0xffffffff);
		if (m_objHash)
		{
			m_objType = k_static_entity;
		}
	}
	else
	{
		m_objType = k_entity;
		array.SerializeEntity(m_holdingEntity);
	}
}

void CObjectIdentifier::CreateFromPhysicalEntity(const EventPhysMono* pMono)
{
	IEntity* pEntity;
	IRenderNode* pRenderNode;
	Reset();
	switch (pMono->iForeignData)
	{
	case PHYS_FOREIGN_ID_ENTITY:
		m_objType = k_unknown;    // We are not sure whether this entity started life as a static, or an normal entity, yet
		m_holdingEntity = (pEntity = (IEntity*)pMono->pForeignData)->GetId();
		break;
	case PHYS_FOREIGN_ID_STATIC:
		m_holdingEntity = 0;
		m_objType = k_static_entity;
		pRenderNode = (IRenderNode*)pMono->pForeignData;
		m_objCenter = pRenderNode->GetBBox().GetCenter();
		m_objHash = CObjectSelector::CalculateHash(pRenderNode, true);
		break;
	}
}

IPhysicalEntity* CObjectIdentifier::GetPhysicsFromHoldingEntity()
{
	IEntity* pEntity;
	if (pEntity = gEnv->pEntitySystem->GetEntity(m_holdingEntity))
	{
		IPhysicalEntity* pent = pEntity->GetPhysics();
		LOGBREAK("# FindPhysicalEntity: got pent: %p, from entid: %x", pent, m_holdingEntity);
		return pent;
	}
	GameWarning("brk: # Couldn't identify an object to break");
	return NULL;
}

IPhysicalEntity* CObjectIdentifier::FindPhysicalEntity()
{
	LOGBREAK("# FindPhysicalEntity:");

	if (m_holdingEntity)
	{
		return GetPhysicsFromHoldingEntity();
	}
	if (m_objType == k_static_entity)
	{
		if (m_objHash)
		{
			// First look through the already registered identifiers
			int idx = CBreakReplicator::Get()->GetIdentifier(this, NULL, true);
			if (idx >= 0)
			{
				CObjectIdentifier* pIdentifier = &CBreakReplicator::Get()->m_identifiers[idx];
				if (pIdentifier->m_holdingEntity)
				{
					return pIdentifier->GetPhysicsFromHoldingEntity();
				}
			}
			/* Resort to a physics overlap test */
			IPhysicalEntity* pent;
			if (pent = CObjectSelector::FindPhysicalEntity(m_objCenter, m_objHash, 30.f))
			{
				return pent;
			}
		}
	}
	GameWarning("brk: # Couldn't identify an object to break");
	return NULL;
}

/*
   ====================
   BreakStream
   ====================
 */

	#define BS_NUM_RECORDING_FRAMES 2
	#define BS_NUM_PLAYBACK_FRAMES  5
	#define BS_MAX_NUM_TO_PLAYBACK  10

BreakStream::BreakStream()
	: m_pent(nullptr)
	, m_breakIdx(0xffff)
	, m_subBreakIdx(~0)
	, m_mode(k_invalid)
	, m_type(0)
	, m_numFramesLeft(0)
	, m_invalidFindCount(0)
	, m_waitForDependent(0)
	, m_logHostMigrationInfo(1)
{
	m_identifier.Reset();
}

BreakStream::~BreakStream()
{
	if (m_mode == k_playing)
	{
		if (m_pent)
		{
			m_pent->Release();
		}
	}
}

void BreakStream::Init(EMode mode)
{
	m_mode = mode;
	if (mode == k_playing)
		m_numFramesLeft = BS_NUM_PLAYBACK_FRAMES;
	if (mode == k_recording)
		m_numFramesLeft = BS_NUM_RECORDING_FRAMES;
}

bool BreakStream::CanSendPayload()
{
	DEBUG_ASSERT(m_mode == k_recording);
	return m_numFramesLeft == 0;
}

void BreakStream::OnEndFrame()
{
	const int n = m_numFramesLeft ? m_numFramesLeft - 1 : 0;
	if (m_mode == k_playing && n == 0)
	{
		if (m_pent)
		{
			m_pent->Release();
			m_pent = NULL;
		}
		LogHostMigrationInfo();
		m_mode = k_finished;
		m_collector->FinishedBreakage();
		m_collector = NULL; // release smart ptr
	}
	m_numFramesLeft = n;
	LOGBREAK("# stream: %d, setting m_numFramesLeft: %d, mode=%s", m_breakIdx, m_numFramesLeft, BreakStreamGetModeName(m_mode));
}

void BreakStream::LogHostMigrationInfo()
{
	if (m_logHostMigrationInfo == 0)
		return;

	DEBUG_ASSERT(m_mode != k_finished);
	LOGBREAK("HOSTMIG for stream: %d", m_breakIdx);
	{
		// - Host Migration -
		// The client also needs to log a copy of a break stream
		// in case it becomes the server and has to deal with client
		// joins. Log the break here, but dont send it (eNBF_SendOnlyOnClientJoin flag)
		SNetBreakDescription desc;
		desc.flags = (ENetBreakDescFlags)(eNBF_UseSimpleSend | eNBF_SendOnlyOnClientJoin);
		desc.nEntities = 0;
		desc.pEntities = NULL;
		desc.pMessagePayload = this;
		OnSend(desc);
		INetContext* pNetContext = CCryAction::GetCryAction()->GetNetContext();
		if (pNetContext)
		{
			pNetContext->LogBreak(desc);
		}
	}
}

void BreakStream::GetAffectedRegion(AABB& aabb)
{
	aabb.min.zero();
	aabb.max.zero();
}

void BreakStream::SerialiseSimpleBreakage(TSerialize ser)
{
	CBreakReplicator::Get()->SerialiseBreakage(ser, this);
}

void BreakStream::SerializeWith_Begin(CBitArray& array, bool includeObjCenter)
{
	m_identifier.SerializeWith(array, includeObjCenter);
	LOGBREAK("#   identifier(entId:%x, hash:%x, objType=%d)", m_identifier.m_holdingEntity, m_identifier.m_objHash, m_identifier.m_objType);
}

bool BreakStream::IsIdentifierEqual(const CObjectIdentifier& identifier)
{
	return (m_identifier.m_holdingEntity && m_identifier.m_holdingEntity == identifier.m_holdingEntity) || (m_identifier.m_objHash && m_identifier.m_objHash == identifier.m_objHash);
}

/*
   ===========================
   Part BreakStream
   ===========================
 */

PartBreak::PartBreak()
{
	m_type = k_partBreak;

	m_numProducts = 0;
	m_numJointsBroken = 0;
	m_numProductsCollected = 0;
	m_products = NULL;
	m_jointsBroken = NULL;
}

PartBreak::~PartBreak()
{
	if (m_mode == k_recording)
	{
		for (int i = 0; i < m_numProducts; i++)
		{
			if (m_products[i].pent)
				m_products[i].pent->Release();
		}
	}
	if (m_products)
	{
		free(m_products);
	}
	if (m_jointsBroken)
	{
		free(m_jointsBroken);
	}
}

void PartBreak::Init(EMode mode)
{
	m_mode = mode;
	if (mode == k_playing)
	{
		m_numFramesLeft = BS_NUM_PLAYBACK_FRAMES;
	}
	if (mode == k_recording)
	{
		m_numFramesLeft = BS_NUM_RECORDING_FRAMES;

		// Initially allocate these arrays to a large size
		// They are then freed and reallocated to a smaller amount once recording has finished
		m_products = (EmittedProduct*)malloc(sizeof(EmittedProduct) * NET_MAX_NUMBER_OF_PARTS);
		m_jointsBroken = (JointBreakInfo*)malloc(sizeof(JointBreakInfo) * NET_MAX_NUMBER_OF_PARTS);
	}
}

EmittedProduct* PartBreak::FindEmittedProduct(const EventPhysCreateEntityPart* pEvent, bool add)
{
	DEBUG_ASSERT(m_mode != k_finished);

	for (int i = 0; i < m_numProducts; i++)
	{
		if (m_products[i].pent == pEvent->pEntNew)
		{
			return &m_products[i];
		}
	}

	if (add)
	{
		if (m_numProducts < NET_MAX_NUMBER_OF_PARTS)
		{
			// This is only done server side, whilst recording
			CRY_ASSERT(m_mode == k_recording);
			EmittedProduct* pProduct = m_products + m_numProducts;
			m_numProducts++;
			pProduct->pent = pEvent->pEntNew;
			if (m_mode == k_recording)
			{
				// Don't let the physics delete this until observing has finished
				pProduct->pent->AddRef();
			}
			// memset(&pProduct->parts, 0, sizeof(pProduct->parts));
			pProduct->partidSrc = 0xffff;
			LOGBREAK("#   Added emitted product: %p", pEvent->pEntNew);
			return pProduct;
		}
		else
		{
			GameWarning("#brk: TOO MANY EMITTED PRODUCTS CAN'T ADD TO STREAM !!!");
		}
	}
	return NULL;
}

bool PartBreak::OnCreatePhysEntityPart(const EventPhysCreateEntityPart* pEvent)
{
	DEBUG_ASSERT(m_mode != k_finished);

	if (pEvent->pEntity == m_pent)
	{
		LOGBREAK("# OnCreatePhysEntityPart(%s), pent: %p, partidSrc: %d", BreakStreamGetModeName(m_mode), m_pent, pEvent->partidSrc);
		if (m_mode == k_recording)
		{
			if (m_numFramesLeft == 0)
				m_numFramesLeft = 1;  // Keep active for one more frame!

			if (EmittedProduct* pProduct = FindEmittedProduct(pEvent, true))
			{
				if (pEvent->partidSrc <= 255)
				{
					// pProduct->parts.Add(pEvent->partidSrc);
					pProduct->partidSrc = min(pProduct->partidSrc, pEvent->partidSrc);
					pProduct->initialVel = pEvent->v;
					pProduct->initialAngVel = pEvent->w;
				}
				else
				{
					GameWarning("#brk: CAN'T ADD EMITTED PRODUCT: partidSrc>255 !!!");
				}
			}
		}
		if (m_mode == k_playing)
		{
			for (int i = 0; i < m_numProducts; i++)
			{
				if ((m_products[i].partidSrc == pEvent->partidSrc) && (m_products[i].pent == NULL))
				{
					// Fix up the pent pointer
					m_products[i].pent = pEvent->pEntNew;
					m_products[i].pent->AddRef();  // Don't let the physics delete this until observing has finished
					m_numProductsCollected++;
				}
			}
		}
		return true;
	}
	return false;
}

bool PartBreak::OnRemovePhysEntityParts(const EventPhysRemoveEntityParts* pEvent)
{

	DEBUG_ASSERT(m_mode != k_finished);
	if (pEvent->pEntity == m_pent)
	{
		if (m_mode == k_recording)
		{
			if (m_numFramesLeft == 0)
				m_numFramesLeft = 1;  // Keep active for one more frame!
		}
		if (m_mode == k_playing)
		{
		}
		return true;
	}
	return false;
}

bool PartBreak::OnJointBroken(const EventPhysJointBroken* pEvent)
{
	DEBUG_ASSERT(m_mode != k_finished);

	if (pEvent->pEntity[0] == m_pent)
	{
		LOGBREAK("# OnJointBroken(%s), pent: %p, idJoint: %d, partidEpicenter: %d", BreakStreamGetModeName(m_mode), m_pent, pEvent->idJoint, pEvent->partidEpicenter);
		if (m_mode == k_recording)
		{
			if (m_numJointsBroken < NET_MAX_NUMBER_OF_PARTS && pEvent->idJoint <= 255)
			{
				JointBreakInfo* jb = &m_jointsBroken[m_numJointsBroken++];
				jb->idJoint = pEvent->idJoint;
				jb->partidEpicenter = pEvent->partidEpicenter;
			}
			else
			{
				GameWarning("#brk: TOO MANY BROKEN JOINTS CAN'T ADD TO STREAM / OR JOINT_ID > 255 !!!");
			}
		}
		if (m_mode == k_playing)
		{
			LOGBREAK("#    Was listening with numFramesLeft: %d", m_numFramesLeft);
		}
		return true;
	}
	return false;
}

void PartBreak::OnEndFrame()
{
	DEBUG_ASSERT(m_mode != k_finished);

	m_numFramesLeft = m_numFramesLeft ? m_numFramesLeft - 1 : 0;

	if (m_mode == k_recording)
	{
		for (int i = 0; i < m_numProducts; i++)
		{
			IPhysicalEntity* pent = m_products[i].pent;
			if (pent)
			{
				pent->Release();
				m_products[i].pent = NULL;
			}
		}
	}
	if (m_mode == k_playing)
	{
		if (m_numFramesLeft == 0 || (m_numProductsCollected == m_numProducts))
		{
			// Finished Playback!
			LOGBREAK("# Playback Stream (%d) is finished (pent: %p)", m_breakIdx, m_pent);
			if (m_pent)
			{
				m_pent->Release();
				m_pent = NULL;
			}
			for (int i = 0; i < m_numProducts; i++)
			{
				IPhysicalEntity* pent = m_products[i].pent;
				if (pent)
				{
					m_products[i].pent->Release();
					m_products[i].pent = NULL;
				}
			}
			LogHostMigrationInfo();
			m_mode = k_finished;
			m_collector = NULL; // release smart ptr
		}
	}
}

#pragma warning(push)
#pragma warning(disable : 6262)// 32k of stack space of CBitArray
void PartBreak::SerializeWith(CBitArray& array)
{
	SerializeWith_Begin(array, true);

	array.Serialize(m_numProducts, 0, NET_MAX_NUMBER_OF_PARTS);
	array.Serialize(m_numJointsBroken, 0, NET_MAX_NUMBER_OF_PARTS);

	if (array.IsReading())
	{
		// Allocate space
		if (m_numProducts)
		{
			m_products = (EmittedProduct*) malloc(sizeof(EmittedProduct) * m_numProducts);
			memset(m_products, 0, sizeof(EmittedProduct) * m_numProducts);
		}
		if (m_numJointsBroken)
		{
			m_jointsBroken = (JointBreakInfo*) malloc(sizeof(JointBreakInfo) * m_numJointsBroken);
		}
	}

	for (int i = 0; i < m_numJointsBroken; i++)
	{
		array.Serialize(m_jointsBroken[i].idJoint, 0, 255);
		array.Serialize(m_jointsBroken[i].partidEpicenter, 0, 255);
	}

	for (int i = 0; i < m_numProducts; i++)
	{
		PREFAST_ASSUME(i > 0 && i < m_numProducts);
		array.Serialize(m_products[i].partidSrc, 0, 255);
		SerializeDirVector(array, m_products[i].initialVel, 20.f, 12, 6, 6);
		SerializeDirVector(array, m_products[i].initialAngVel, 20.f, 12, 6, 6);
	}

	#if DEBUG_NET_BREAKAGE
	if (array.IsWriting())
	{
		CTimeValue time = gEnv->pTimer->GetAsyncTime();
		LOGBREAK("# .-----------------------------------.");
		LOGBREAK("# |    type: %s", "JointBreak");
		LOGBREAK("# .-----------------------------------");
	}

	LOGBREAK("#   numProducts: %d", m_numProducts);
	for (int i = 0; i < m_numProducts; i++)
	{
		EmittedProduct* pProduct = m_products + i;
		LOGBREAK("#    Product(%d), pent=%p, partidSrc=%d, initialVel=(%f,%f,%f)", i, pProduct->pent, pProduct->partidSrc, XYZ(pProduct->initialVel));
	}
	LOGBREAK("#   numJointBreaks: %d", m_numJointsBroken);
	#endif
}
#pragma warning(pop)

void PartBreak::Playback()
{
	DEBUG_ASSERT(m_mode == k_playing);

	LOGBREAK("# .-----------------------------------.");
	LOGBREAK("# |  Playback (%3d)                   |", m_breakIdx);
	LOGBREAK("# .-----------------------------------.");
	LOGBREAK("#   identifier(entId:%x, hash:%x)", m_identifier.m_holdingEntity, m_identifier.m_objHash);
	LOGBREAK("#");
	LOGBREAK("#     Need to break off %d pieces", m_numProducts);
	LOGBREAK("#     Apply %d joint breaks", m_numJointsBroken);
	LOGBREAK("#     Apply breakage to pent: %p", m_pent);
	LOGBREAK("");

	// Stay with the joint-break system (for now)
	// I don't like this, but at least we haven't got to
	// replicate all of the physics events, and particle generation
	// which would be painful. It would also involve cut-n-pasted lots
	// of breakablemanager code or a massive refactor.

	pe_params_structural_initial_velocity initialVel;
	for (int i = 0; i < m_numProducts; i++)
	{
		// Pre-Load the initial velocities
		initialVel.partid = m_products[i].partidSrc;
		initialVel.v = m_products[i].initialVel;
		initialVel.w = m_products[i].initialAngVel;
		m_pent->SetParams(&initialVel);
	}

	pe_params_structural_joint pj;
	for (int i = 0; i < m_numJointsBroken; i++)
	{
		LOGBREAK("#     BREAKING JOINT %d", m_jointsBroken[i].idJoint);
		pj.id = m_jointsBroken[i].idJoint;
		pj.partidEpicenter = m_jointsBroken[i].partidEpicenter;
		pj.bBroken = 1;
		pj.bReplaceExisting = 1;
		m_pent->SetParams(&pj);
	}
}

void PartBreak::OnSend(SNetBreakDescription& desc)
{
	// Reallocate the arrays to avoid memory wastage
	EmittedProduct* products = NULL;
	JointBreakInfo* jointsBroken = NULL;

	if (m_numProducts)
	{
		products = (EmittedProduct*) malloc(sizeof(EmittedProduct) * m_numProducts);
		memcpy(products, m_products, sizeof(EmittedProduct) * m_numProducts);
	}
	if (m_numJointsBroken)
	{
		jointsBroken = (JointBreakInfo*) malloc(sizeof(JointBreakInfo) * m_numJointsBroken);
		memcpy(jointsBroken, m_jointsBroken, sizeof(JointBreakInfo) * m_numJointsBroken);
	}

	free(m_products);
	free(m_jointsBroken);

	m_products = products;
	m_jointsBroken = jointsBroken;
}

/*
   ===========================
   Plane BreakStream
   ===========================
 */

PlaneBreak::PlaneBreak()
{
	m_type = k_planeBreak;
	m_pStatObj = NULL;
}

PlaneBreak::~PlaneBreak()
{
	if (m_pStatObj)
		m_pStatObj->Release();
	m_pStatObj = NULL;
}

void PlaneBreak::SerializeWith(CBitArray& array)
{
	LOGBREAK("PlaneBreak::SerializeWith: %s", array.IsReading() ? "Reading:" : "Writing");

	SerializeWith_Begin(array, false);

	SBreakEvent& breakEvent = m_be;

	// TODO: entity glass breakage, in entity space

	CBreakReplicator::SerialisePosition(array, m_be.pt, CNetworkCVars::Get().BreakMaxWorldSize, CBreakReplicator::m_accurateWorldPosNumBits);

	m_be.partid[1] = EntityPhysicsUtils::GetSlotIdx(m_be.partid[1], 0) << 8 | EntityPhysicsUtils::GetSlotIdx(m_be.partid[1], 1);

	SerializeDirHelper(array, m_be.n, 8, 8);
	array.Serialize(m_be.idmat[0], -1, 254);
	array.Serialize(m_be.idmat[1], -1, 254);
	array.Serialize(m_be.partid[1], 0, 0xffff);
	array.Serialize(m_be.iPrim[1], -1, 0x7ffe);
	array.Serialize(m_be.mass[0], 1.f, 1000.f, 8);
	SerializeDirVector(array, m_be.vloc[0], 20.f, 8, 8, 8);

	m_be.partid[1] = (m_be.partid[1] & 0xff) + (m_be.partid[1] >> 8) * EntityPhysicsUtils::PARTID_MAX_SLOTS;

	// Looks like we dont need these
	m_be.partid[0] = 0;
	m_be.scale.Set(1.f, 1.f, 1.f);
	m_be.radius = 0.f;
	m_be.vloc[1].zero();   // Just assume the hitee is at zero velocity
	m_be.energy = 0.f;
	m_be.penetration = 0.f;
	m_be.mass[1] = 0.f;
	m_be.seed = 0;

	m_identifier.m_objCenter = m_be.pt;
}

void PlaneBreak::OnSend(SNetBreakDescription& desc)
{
	// If this is the first break, then send immediately
	// If this is a secondary break, then send only on a client join
	desc.flags = (ENetBreakDescFlags)(desc.flags | (eNBF_SendOnlyOnClientJoin & (m_be.bFirstBreak - 1)));
}

void PlaneBreak::Playback()
{
	LOGBREAK("# .-----------------------------------.");
	LOGBREAK("# |  PlaneBreak::Playback (%3d)      |", m_breakIdx);
	LOGBREAK("# .-----------------------------------.");

	EventPhysCollision epc;

	epc.pEntity[0] = 0;
	epc.iForeignData[0] = -1;
	epc.pt = m_be.pt;
	epc.n = m_be.n;
	epc.vloc[0] = m_be.vloc[0];
	epc.vloc[1] = m_be.vloc[1];
	epc.mass[0] = m_be.mass[0];
	epc.mass[1] = m_be.mass[1];
	epc.idmat[0] = m_be.idmat[0];
	epc.idmat[1] = m_be.idmat[1];
	epc.partid[0] = m_be.partid[0];
	epc.partid[1] = m_be.partid[1];
	epc.iPrim[1] = m_be.iPrim[1];
	epc.penetration = m_be.penetration;
	epc.radius = m_be.radius;

	if (m_pent)
	{
		epc.pEntity[1] = m_pent;
		epc.iForeignData[1] = m_pent->GetiForeignData();
		epc.pForeignData[1] = m_pent->GetForeignData(epc.iForeignData[1]);
	}
	else
	{
		epc.pEntity[1] = 0;
		epc.iForeignData[1] = -1;
		epc.pForeignData[1] = 0;
	}

	CActionGame::PerformPlaneBreak(epc, NULL, CActionGame::ePPB_EnableParticleEffects | CActionGame::ePPB_PlaybackEvent, 0);

	LogHostMigrationInfo();
	m_mode = k_finished;
	m_collector = NULL; // release smart ptr
}

bool PlaneBreak::IsWaitinOnRenderMesh()
{
	return m_pStatObj != NULL;
}

bool PlaneBreak::IsRenderMeshReady()
{
	if (m_pStatObj == NULL)
	{
		int iForeign = m_pent->GetiForeignData();
		if (iForeign == PHYS_FOREIGN_ID_STATIC)
		{
			if (IRenderNode* pRenderNode = (IRenderNode*)m_pent->GetForeignData(PHYS_FOREIGN_ID_STATIC))
			{
				IStatObj::SSubObject* pSubObj;
				Matrix34A mtx;
				IStatObj* pStatObj = pRenderNode->GetEntityStatObj(0, &mtx);
				if (pStatObj && pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
					if (pSubObj = pStatObj->GetSubObject(m_be.partid[1]))
						pStatObj = pSubObj->pStatObj;
				m_pStatObj = pStatObj;
				if (m_pStatObj)
					m_pStatObj->AddRef();
			}
		}
	}
	if (m_pStatObj)
	{
		if (m_pStatObj->GetRenderMesh() == NULL)
		{
			// Waiting on rendermesh
			if (m_pent)
			{
				m_pent->Release();
				m_pent = NULL;
			}
			return false;
		}
		// No longer waiting on statobj, release it
		m_pStatObj->Release();
		m_pStatObj = NULL;
	}
	return true;
}

/*
   ===========================
   Deforming BreakStream
   ===========================
 */

DeformBreak::DeformBreak()
{
	m_type = k_deformBreak;
}

DeformBreak::~DeformBreak()
{
}

void DeformBreak::OnSpawnEntity(IEntity* pEntity)
{
	#if NET_SYNC_TREES
	// We need to make sure that this entity is synchnronised correctly
	// when being deleted by the memory limiter in action game -
	// attach a "BreakRepGameObject"
	pEntity->SetFlags(pEntity->GetFlags() & ~(ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY));
	IGameObject* pGameObject = CCryAction::GetCryAction()->GetIGameObjectSystem()->CreateGameObjectForEntity(pEntity->GetId());
	pGameObject->ActivateExtension("BreakRepGameObject");

	// For now assume that all the spawned entities happen in the same order
	if (m_mode == k_playing)
	{
		// Tell the low level that we have a new entity
		// and it needs binding
		m_collector->BindSpawnedEntity(pEntity->GetId(), -1);
	}
	// Make a note of entities that will need binding
	m_spawnedEntities.push_back(pEntity->GetId());
	#endif
}

void DeformBreak::OnSend(SNetBreakDescription& desc)
{
	// Inform the low level of the entities we expect to be spawned on the client
	desc.nEntities = m_spawnedEntities.size();
	desc.pEntities = m_spawnedEntities.begin();
	#if DEBUG_NET_BREAKAGE
	if (m_mode == k_playing)
	{
		LOGBREAK(" HOSTMIG: Stream: %d, DeformBreak: numEnts: %d", m_breakIdx, m_spawnedEntities.size());
		for (int i = 0; i < m_spawnedEntities.size(); i++)
		{
			LOGBREAK("   - EntId: %x", m_spawnedEntities[i]);
		}
	}
	#endif
}

static inline void SerialiseHalf(CBitArray& array, f32& f)
{
	union FloatUnion
	{
		f32    f;
		uint32 u;
	} tmp;

	if (array.IsReading())
	{
		array.Serialize(tmp.u, 0, 0xffff);
		tmp.u = tmp.u << 16;
		f = tmp.f;
	}
	else
	{
		tmp.f = f;
		tmp.u = tmp.u >> 16;
		array.Serialize(tmp.u, 0, 0xffff);
	}
}

void DeformBreak::SerializeWith(CBitArray& array)
{
	SerializeWith_Begin(array, false);

	LOGBREAK("DeformBreak::SerializeWith: %s", array.IsReading() ? "Reading:" : "Writing");

	CBreakReplicator::SerialisePosition(array, m_be.pt, CNetworkCVars::Get().BreakMaxWorldSize, CBreakReplicator::m_accurateWorldPosNumBits);

	SerializeDirHelper(array, m_be.n, 8, 8);
	SerialiseHalf(array, m_be.energy);

	m_identifier.m_objCenter = m_be.pt;
}

void DeformBreak::Playback()
{
	LOGBREAK("# .-----------------------------------.");
	LOGBREAK("# |  DeformBreak::Playback (%3d)      |", m_breakIdx);
	LOGBREAK("# .-----------------------------------.");

	if (m_pent)
	{
		// Tell the low level that breakage has started
		m_collector->BeginBreakage();

		if (!gEnv->pPhysicalWorld->DeformPhysicalEntity(m_pent, m_be.pt, -m_be.n, m_be.energy))
		{
			GameWarning("brk: # DeformPhysicalEntity failed");
		}
	}
}

/*
   ================================
   CBreakReplicator
   ================================
 */

CBreakReplicator* CBreakReplicator::m_pThis = NULL;
int CBreakReplicator::m_accurateWorldPosNumBits = 0;
int CBreakReplicator::m_inaccurateWorldPosNumBits = 0;

// Suppress warning for unintialized array CBreakReplicator::m_pendingStreams
// cppcheck-suppress uninitMemberVar
CBreakReplicator::CBreakReplicator(CGameContext* pGameCtx)
	: m_bDefineProtocolMode_server(false)
	, m_activeStartIdx(0)
	, m_numPendingStreams(0)
	, m_timeSinceLevelLoaded(-1.0f)
{
	(void)pGameCtx;

	// This seems to be created when entering a level for the first time
	// both on server and clients

	LOGBREAK("# -------------------------------------------------------------");
	LOGBREAK("# -   CREATE: CBreakReplicator                         -");
	LOGBREAK("# -------------------------------------------------------------");

	DEBUG_ASSERT(m_pThis == NULL);
	m_pThis = this;
	m_streams.reserve(100);

	// Initialise a dummy stream. This is used as a place holder for
	// stream indexes that have not yet been serialised over on a client
	// saves lots of null pointer tests
	s_dummyStream.m_mode = BreakStream::k_finished;
	s_dummyStream.m_pent = (IPhysicalEntity*)(intptr_t)0xCBADBEEF;
	s_dummyStream.m_breakIdx = 0xffff;

	m_removePartEvents.reserve(100);
	m_entitiesToRemove.reserve(16);
	m_identifiers.reserve(100);
	m_streams.reserve(100);

	if (IPhysicalWorld* pWorld = gEnv->pPhysicalWorld)
	{
		pWorld->AddEventClient(EventPhysRevealEntityPart::id, OnRevealEntityPart, true, 100000.f);
		pWorld->AddEventClient(EventPhysCreateEntityPart::id, OnCreatePhysEntityPart, true, 100000.f);
		pWorld->AddEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysEntityParts, true, 100000.f);
		pWorld->AddEventClient(EventPhysJointBroken::id, OnJointBroken, true, 100000.f);
	}

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CBreakReplicator");

	float max = CNetworkCVars::Get().BreakMaxWorldSize;
	m_inaccurateWorldPosNumBits = IntegerLog2_RoundUp(uint32(max / WORLD_POS_SAMPLE_DISTANCE_APPART_INACCURATE));
	m_accurateWorldPosNumBits = IntegerLog2_RoundUp(uint32(max / WORLD_POS_SAMPLE_DISTANCE_APPART_ACCURATE));

	CryLog("[CBreakReplicator] Inaccurate world pos using range 0-%f with %d bits", max, m_inaccurateWorldPosNumBits);
	CryLog("[CBreakReplicator] Accurate world pos using range 0-%f with %d bits", max, m_accurateWorldPosNumBits);
}

CBreakReplicator::~CBreakReplicator()
{
	// When the game quits to the front-end this should be called.

	LOGBREAK("# -------------------------------------------------------------");
	LOGBREAK("# -   DESTROY: CBreakReplicator                        -");
	LOGBREAK("# -------------------------------------------------------------");

	DEBUG_ASSERT(m_pThis = this);
	m_pThis = NULL;
	Reset();

	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	if (IPhysicalWorld* pWorld = gEnv->pPhysicalWorld)
	{
		pWorld->RemoveEventClient(EventPhysRevealEntityPart::id, OnRevealEntityPart, true);
		pWorld->RemoveEventClient(EventPhysCreateEntityPart::id, OnCreatePhysEntityPart, true);
		pWorld->RemoveEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysEntityParts, true);
		pWorld->RemoveEventClient(EventPhysJointBroken::id, OnJointBroken, true);
	}
}

void CBreakReplicator::Reset()
{
	for (int i = 0; i < m_numPendingStreams; i++)
	{
		m_pendingStreams[i]->Release();
	}

	for (int i = 0; i < m_streams.size(); i++)
	{
		if (m_streams[i] != &s_dummyStream)
		{
			m_streams[i]->Release();
		}
	}

	m_streams.clear();
	m_identifiers.clear();
	m_activeStartIdx = 0;
	m_numPendingStreams = 0;
}

void CBreakReplicator::DefineProtocol(IProtocolBuilder* pBuilder)
{
	static SNetProtocolDef nullDef = { 0, 0 };

	// Setup so that client sends to server, and server recieves
	// (opposite to the original break replicator)
	// The messages through here are separate to the stangard break messages!
	if (m_bDefineProtocolMode_server)
		pBuilder->AddMessageSink(this, nullDef, GetProtocolDef());
	else
		pBuilder->AddMessageSink(this, GetProtocolDef(), nullDef);
}

void CBreakReplicator::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
		m_timeSinceLevelLoaded = 0.f;
		break;
	}
}

int CBreakReplicator::PushBackNewStream(const EventPhysMono* pMono, BreakStream* pStream)
{
	// Insert into the active list
	const int breakIdx = m_streams.size();

	LOGBREAK("#   adding new stream: %d for pent: %p", breakIdx, pMono->pEntity);
	DEBUG_ASSERT(pMono->iForeignData == PHYS_FOREIGN_ID_STATIC || pMono->iForeignData == PHYS_FOREIGN_ID_ENTITY);

	pStream->AddRef();  // Make sure "dumb" pointers never delete the stream

	pStream->Init(BreakStream::k_recording);
	pStream->m_pent = pMono->pEntity;
	pStream->m_breakIdx = breakIdx;
	pStream->m_identifier.CreateFromPhysicalEntity(pMono);

	// Have we already identified this object before?
	int idx = GetIdentifier(&pStream->m_identifier, pMono->pEntity, true);
	DEBUG_ASSERT(idx >= 0);
	DEBUG_ASSERT(pStream->m_identifier.m_objType != CObjectIdentifier::k_unknown);

	// NB: This code is run only on the server
	// We need to establish the dependency index m_subBreakIdx
	int subIndex = 0;
	int numStreams = m_streams.size();
	for (int i = numStreams - 1; i >= 0; --i)
	{
		// Find streams that previously broke the same object
		if (m_streams[i]->IsIdentifierEqual(pStream->m_identifier))
		{
			subIndex = m_streams[i]->m_subBreakIdx + 1;
			break;
		}
	}
	pStream->m_subBreakIdx = subIndex;

	// Insert into the stream list
	m_streams.push_back(pStream);

	return breakIdx;
}

int CBreakReplicator::GetBreakIndex(const EventPhysMono* pMono, bool add)
{
	// This is called both on the server and client and will find
	// a break stream for the physicalEntity being observed.
	// On the server if a an active recording stream can't be found
	// it will add a new one.

	LOGBREAK("# GetBreakIndex(pent: %p)", pMono->pEntity);

	int breakIdx = -1;

	const int lastBreak = m_streams.size();
	int i = m_activeStartIdx;
	while (i < lastBreak)
	{
		if (m_streams[i]->m_mode == BreakStream::k_finished || m_streams[i]->m_pent != pMono->pEntity)
		{
			i++;
		}
		else
		{
			breakIdx = i;
			break;
		}
	}
	if (add && i == lastBreak)
	{
		if ((pMono->iForeignData == PHYS_FOREIGN_ID_STATIC) || (pMono->iForeignData == PHYS_FOREIGN_ID_ENTITY))
		{
			// Start new stream (default is Part Break)
			BreakStream* pStream = new PartBreak;
			breakIdx = PushBackNewStream(pMono, pStream);
		}
		else
		{
			LOGBREAK("#   WARNING: PHYSICS SUPPLIED A BAD LOGGED EVENT!");
		}
	}

	LOGBREAK("#   returning: breakIdx: %d for pent: %p", breakIdx, pMono->pEntity);
	return breakIdx;
}

int CBreakReplicator::GetIdentifier(CObjectIdentifier* identifier, const IPhysicalEntity* pent, bool add)
{
	// Have we already identified this object before?
	// If so, return the cached version by copying it out
	// This is called by both the server and client

	LOGBREAK("# GetIdentifier(for: %p)", pent);

	int ret = -1;
	if (identifier->m_objType == CObjectIdentifier::k_static_entity)
	{
		for (int i = 0; i < m_identifiers.size(); i++)  // ToDo: make me faster!
		{
			if (m_identifiers[i].m_objHash == identifier->m_objHash)
			{
				// NOTE: We copy out the identifier to the requestor
				*identifier = m_identifiers[i];
				LOGBREAK("#   returning %d, from k_static_entity pool", i);
				return i;
			}
		}
	}
	if (identifier->m_objType == CObjectIdentifier::k_unknown)
	{
		for (int i = 0; i < m_identifiers.size(); i++)  // ToDo: make me faster!
		{
			if (m_identifiers[i].m_holdingEntity == identifier->m_holdingEntity)
			{
				// NOTE: We copy out the identifier to the requestor
				*identifier = m_identifiers[i];
				LOGBREAK("#   returning %d, from k_unknown", i);
				return i;
			}
		}
	}

	if (ret == -1 && add)
	{
		if (identifier->m_holdingEntity != 0 || identifier->m_objHash != 0)
		{
			if (identifier->m_objType == CObjectIdentifier::k_unknown)
			{
				// Looks like this entity started life as an entity
				identifier->m_objType = CObjectIdentifier::k_entity;
			}
			m_identifiers.push_back(*identifier);
			LOGBREAK("#   adding to %d (entid:%x, hash:%x)", m_identifiers.size() - 1, identifier->m_holdingEntity, identifier->m_objHash);
			return m_identifiers.size() - 1;
		}
	}
	return ret;
}

bool CBreakReplicator::OnCreatePhysEntityPart(const EventPhysCreateEntityPart* pEvent)
{
	LOGBREAK("# OnCreate: from: %p ----------------", pEvent->pEntity);
	int breakIdx = GetBreakIndex(pEvent, m_bDefineProtocolMode_server);
	if (breakIdx >= 0)
	{
		BreakStream* pStream = m_streams[breakIdx];
		pStream->OnCreatePhysEntityPart(pEvent);
		return true;
	}
	return false;
}

bool CBreakReplicator::OnRemovePhysEntityParts(const EventPhysRemoveEntityParts* pEvent)
{
	LOGBREAK("# OnRemove: from: %p ----------------", pEvent->pEntity);
	int breakIdx = GetBreakIndex(pEvent, m_bDefineProtocolMode_server);

	const int oldSize = m_removePartEvents.size();

	BreakLogAlways("PR: Storing RemovePart event index %d,  'foreign data': 0x%p   iForeignType: %d", oldSize, pEvent->pForeignData, pEvent->iForeignData);

	m_removePartEvents.push_back(*pEvent);

	CCryAction::GetCryAction()->OnPartRemoveEvent(oldSize);

	if (breakIdx >= 0)
	{
		BreakStream* pStream = m_streams[breakIdx];
		pStream->OnRemovePhysEntityParts(pEvent);
		return true;
	}
	return false;
}

bool CBreakReplicator::OnRevealEntityPart(const EventPhysRevealEntityPart* pEvent)
{
	// Just create an empty stream, ready for the subsequent creates, removes, and joint breaks
	LOGBREAK("# OnReveal: from: %p ----------------", pEvent->pEntity);
	int breakIdx = GetBreakIndex(pEvent, m_bDefineProtocolMode_server);
	if (breakIdx >= 0)
	{
		return true;
	}
	return false;
}

int CBreakReplicator::OnRevealEntityPart(const EventPhys* pEvent)
{
	CBreakReplicator::Get()->OnRevealEntityPart(static_cast<const EventPhysRevealEntityPart*>(pEvent));
	return 1;
}

bool CBreakReplicator::OnJointBroken(const EventPhysJointBroken* pEvent)
{
	LOGBREAK("# OnJointBreak: from: %p ----------------", pEvent->pEntity[0]);

	EventPhysMono mono;
	mono.pEntity = pEvent->pEntity[0];
	mono.pForeignData = pEvent->pForeignData[0];
	mono.iForeignData = pEvent->iForeignData[0];

	int breakIdx = GetBreakIndex(&mono, m_bDefineProtocolMode_server);
	if (breakIdx >= 0)
	{
		BreakStream* pStream = m_streams[breakIdx];
		pStream->OnJointBroken(pEvent);
		return true;
	}
	return false;
}

void CBreakReplicator::BeginRecordingPlaneBreak(const SBreakEvent& be, EventPhysMono* pMono)
{
	if (pMono->iForeignData == PHYS_FOREIGN_ID_STATIC || pMono->iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		PlaneBreak* pStream = new PlaneBreak;
		pStream->m_be = be;
		PushBackNewStream(pMono, pStream);
	}
	else
	{
		LOGBREAK("#   WARNING: PHYSICS SUPPLIED A BAD LOGGED PLANE-BREAK EVENT!");
	}
}

void CBreakReplicator::BeginRecordingDeforminBreak(const SBreakEvent& be, EventPhysMono* pMono)
{
	if (pMono->iForeignData == PHYS_FOREIGN_ID_STATIC || pMono->iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		DeformBreak* pStream = new DeformBreak;
		pStream->m_be = be;
		PushBackNewStream(pMono, pStream);
	}
	else
	{
		LOGBREAK("#   WARNING: PHYSICS SUPPLIED A BAD LOGGED DEFORM-BREAK EVENT!");
	}
}

void CBreakReplicator::OnEndFrame()
{
	// Update the active list
	int i = m_activeStartIdx;
	const int lastBreak = m_streams.size();
	while (i < lastBreak && m_streams[i]->m_mode == BreakStream::k_finished)
		i++;
	m_activeStartIdx = i;

	#if DEBUG_NET_BREAKAGE
	if (i < lastBreak)
		LOGBREAK("# Replicator::OnEndFrame, activeStartIdx: %d", i);
	#endif

	for (; i < lastBreak; i++)
	{
		if (m_streams[i]->m_mode != BreakStream::k_finished)
		{
			// Notify Recorders (on the server), and players (on the clients)
			m_streams[i]->OnEndFrame();
		}
	}

	if (m_bDefineProtocolMode_server)
	{
		// Server:
		if (m_activeStartIdx < m_streams.size())
		{
			// Try to flush active streams
			SendPayloads();
		}
	}
	else
	{
		// Client:
		if (m_numPendingStreams > 0)
		{
			PlaybackBreakage();
		}
	}

	#if NET_SYNC_TREES
	//================================
	// Process Entities To be Removed
	//================================
	// If the object is bound to the network then it must not be deleted. Instead the game object
	// serialises the state and frees the statobj slot on all machines. This is used when the
	// breakage system is consuming too much memory.
	// Generally this is just for trees
	for (i = 0; i < (const int)m_entitiesToRemove.size(); i++)
	{
		EntityId entId = m_entitiesToRemove[i];
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entId);
		if (pEntity)
		{
			if ((pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)) == 0)
			{
				if (CGameObject* pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
				{
					if (IGameObjectExtension* pExtension = pGameObject->QueryExtension("BreakRepGameObject"))
					{
						// Net serialise that this entity should free its breakage statobj slot
						static_cast<CBreakRepGameObject*>(pExtension)->m_removed = true;
					}
				}
			}
			else
			{
				// Entities that are not bound to the network can be deleted
				gEnv->pEntitySystem->RemoveEntity(entId);
			}
		}
	}
	m_entitiesToRemove.clear();
	#endif
}

void CBreakReplicator::OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
{
}

void CBreakReplicator::OnSpawn(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity)
{
	// Check to see if this is an physEnt that we are currently breaking.

	EventPhysMono mono;
	mono.pEntity = pSrcPhysEntity;
	mono.iForeignData = 0xFBAD;

	EntityId entId = pEntity->GetId();

	// Udpate the identifiers
	if (pPhysEntity == pSrcPhysEntity)
	{
		if (IRenderNode* pRenderNode = (IRenderNode*)pSrcPhysEntity->GetForeignData(PHYS_FOREIGN_ID_STATIC))
		{
			uint32 objHash = CObjectSelector::CalculateHash(pRenderNode, true);
			if (objHash)
			{
				for (int i = 0; i < m_identifiers.size(); i++)  // ToDo: make me faster!
				{
					if (m_identifiers[i].m_objHash == objHash)
					{
						m_identifiers[i].m_holdingEntity = entId;
						break;
					}
				}
			}
		}
	}

	int breakIdx = GetBreakIndex(&mono, false);
	if (breakIdx >= 0)
	{
		BreakStream* pStream = m_streams[breakIdx];
		pStream->OnSpawnEntity(pEntity);
	}
}

void CBreakReplicator::OnRemove(IEntity* pEntity)
{
	// ToDo: make me faster!
	const EntityId entId = pEntity->GetId();
	for (int i = 0; i < m_identifiers.size(); i++)
	{
		if (m_identifiers[i].m_holdingEntity == entId)
		{
			m_identifiers[i].m_holdingEntity = 0;
		}
	}
}

void CBreakReplicator::RemoveEntity(IEntity* pEntity)
{
	#if NET_SYNC_TREES
	// Delay entity removal, which needs to be done through CBreakRepGameObject
	m_entitiesToRemove.push_back(pEntity->GetId());
	#else
	// Can safely delete straight away
	CRY_ASSERT(pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY));
	gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
	#endif
}

void CBreakReplicator::SerialisePosition(CBitArray& array, Vec3& pos, float range, int numBits)
{
	if (array.IsReading())
	{
		array.Serialize(pos, 0.f, range, numBits);
		pos.x += CNetworkCVars::Get().BreakWorldOffsetX;
		pos.y += CNetworkCVars::Get().BreakWorldOffsetY;
	}
	else
	{
		Vec3 p = pos;
		p.x -= CNetworkCVars::Get().BreakWorldOffsetX;
		p.y -= CNetworkCVars::Get().BreakWorldOffsetY;
		array.Serialize(p, 0.f, range, numBits);
	}

	#if DEBUG_NET_BREAKAGE
	if (pos.x < CNetworkCVars::Get().BreakWorldOffsetX || pos.x > (CNetworkCVars::Get().BreakWorldOffsetX + range))
		GameWarning("#brk: ERROR!!!! POSITION OUT OF RANGE");
	if (pos.y < CNetworkCVars::Get().BreakWorldOffsetY || pos.y > (CNetworkCVars::Get().BreakWorldOffsetY + range))
		GameWarning("#brk: ERROR!!!! POSITION OUT OF RANGE");
	#endif
}

void CBreakReplicator::SendPayloads()
{
	// Create a simple network payload, without
	// dependencies on spawned entities
	SNetBreakDescription desc;
	desc.flags = eNBF_UseSimpleSend;
	desc.nEntities = 0;
	desc.pEntities = NULL;

	const int lastBreak = m_streams.size();
	int i = m_activeStartIdx;
	while (i < lastBreak)
	{
		BreakStream* pStream = m_streams[i];
		if (pStream->m_mode == BreakStream::k_recording && pStream->CanSendPayload())
		{
			if (i == m_activeStartIdx) m_activeStartIdx++;
			desc.pMessagePayload = pStream;   // NB: this is an assignment to a "dumb" pointer

			// Let the stream fill out the desc if there are entities that need net-binding
			pStream->OnSend(desc);

			pStream->m_mode = BreakStream::k_finished;

			CObjectIdentifier& identifier = pStream->m_identifier;
			if (identifier.m_objType == CObjectIdentifier::k_entity && identifier.m_holdingEntity != 0)
			{
				// This ensures that this entity is enabled
				// before sending any break messages, which
				// is important for client joins
				desc.breakageEntity = identifier.m_holdingEntity;
			}

			CCryAction* pCryAction = CCryAction::GetCryAction();
			if (pCryAction != NULL)
			{
				INetContext* pNetContext = pCryAction->GetNetContext();
				if (pNetContext != NULL)
				{
					pNetContext->LogBreak(desc);
				}
			}
		}
		i++;
	}
}

void CBreakReplicator::PushPlayback(BreakStream* pStream)
{
	// Client: push the stream on a todo list.
	LOGBREAK("# PushPlayback, breakIdx = %d", pStream->m_breakIdx);
	if (m_numPendingStreams < NET_MAX_PENDING_BREAKAGE_STREAMS)
	{
		// We need to keep the list of pending breaks sorted by breakIdx
		// Insert at end, and then push downwards
		int i = m_numPendingStreams;
		const int breakIdx = pStream->m_breakIdx;
		while (i > 0 && breakIdx < m_pendingStreams[i - 1]->m_breakIdx)
		{
			m_pendingStreams[i] = m_pendingStreams[i - 1];
			i--;
		}
		m_pendingStreams[i] = pStream;
		m_numPendingStreams++;
		return;
	}
	else
	{
		DEBUG_ASSERT("m_numPendingStreams < (NET_MAX_PENDING_BREAKAGE_STREAMS-1)");
		GameWarning("#brk: TOO MANY PENDING BREAK STREAMS!");
	}
	pStream->Release();
}

int CBreakReplicator::PlaybackStream(BreakStream* pStream)
{
	if (!pStream->IsRenderMeshReady())
		return 0;

	const int lastBreak = m_streams.size();

	int bStartStream = 1;

	LOGBREAK("# Trying playback for breakstream: %d", pStream->m_breakIdx);

	// Check that we are not already breaking this entity!
	// can only process one break at a time
	int i = m_activeStartIdx;
	while (i < lastBreak)
	{
		if (((m_streams[i]->m_mode - BreakStream::k_playing) | ((INT_PTR)m_streams[i]->m_pent - (INT_PTR)pStream->m_pent)) == 0)
		{
			// Entity is already being broken, skip for now!
			bStartStream = 0;
			LOGBREAK("#     ==== CAN'T START because waiting for stream: %d, pent: %p, mode: %s", m_streams[i]->m_breakIdx, m_streams[i]->m_pent, BreakStreamGetModeName(m_streams[i]->m_mode));
			break;
		}
		i++;
	}
	if (bStartStream)
	{
		// Make room for the stream
		const int initialSize = m_streams.size();
		if (m_streams.size() <= pStream->m_breakIdx)
		{
			m_streams.resize(pStream->m_breakIdx + 32);
			const int newSize = m_streams.size();
			for (int j = initialSize; j < newSize; j++)
			{
				m_streams[j] = &s_dummyStream;
			}
		}
		LOGBREAK("# => Starting Playback for BreakStream: %d", pStream->m_breakIdx);
		m_streams[pStream->m_breakIdx] = pStream;
		m_activeStartIdx = min(m_activeStartIdx, (int)pStream->m_breakIdx);
		pStream->Playback();
	}

	return bStartStream;
}

void CBreakReplicator::PlaybackBreakage()
{
	// Only use the invalid find counters once level has fully loaded
	if (m_timeSinceLevelLoaded < 0.f)
		return;

	float dt = gEnv->pTimer->GetFrameTime();
	m_timeSinceLevelLoaded += dt;
	bool bEnableInavlidCounters = (m_timeSinceLevelLoaded > NET_LEVEL_LOAD_TIME_BEFORE_USING_INAVLID_COUNTERS);

	// Pull streams out of the pending list,
	// try to find the physics entity,
	// making sure that only one stream can act upon
	// an entity at any one time

	int n = 0;
	const int numPending = m_numPendingStreams;
	int numPlayed = 0;

	int p = 0;
	for (; p < numPending && numPlayed < BS_MAX_NUM_TO_PLAYBACK; p++)
	{
		BreakStream* pStream = m_pendingStreams[p];
		IPhysicalEntity* pent = pStream->m_pent;

		if (pStream->IsWaitinOnRenderMesh() && !pStream->IsRenderMeshReady())
		{
			// Couldn't start, we need to keep the stream pending
			m_pendingStreams[n] = m_pendingStreams[p];
			n++;
			continue;
		}

		if (pStream->m_invalidFindCount < NET_MAX_NUM_FRAMES_TO_FIND_ENTITY)
		{
			bool bCanPlay = false;
			if (pStream->m_subBreakIdx == 0)
			{
				// Can play straight away, this stream has no dependencies
				bCanPlay = true;
			}
			else
			{
				// Play streams in order - need to wait for their dependents
				for (int i = m_streams.size() - 1; i >= 0; --i)
				{
					if (m_streams[i]->IsIdentifierEqual(pStream->m_identifier))
					{
						if (pStream->m_subBreakIdx == (m_streams[i]->m_subBreakIdx + 1))
						{
							bCanPlay = (m_streams[i]->m_mode == BreakStream::k_finished);
						}
						break;
					}
				}

				if (bCanPlay == false && pStream->m_waitForDependent >= NET_MAX_NUM_FRAMES_TO_WAIT_FOR_BREAK_DEPENDENCY)
				{
					// Force the playback of this stream
					bCanPlay = true;
				}
				if (bEnableInavlidCounters)
					pStream->m_waitForDependent++;
			}

			if (bCanPlay)
			{
				if (pent == NULL)
				{
					// Can we identify the physics ent that should be broken by the stream?
					pent = pStream->m_identifier.FindPhysicalEntity();
					if (pent)
					{
						pent->AddRef(); // Don't let the physics delete until observing has finished
						pStream->m_pent = pent;

					}
					else
					{
						if (bEnableInavlidCounters)
						{
							pStream->m_invalidFindCount++;
							LOGBREAK("# WARNING: stream (%d), could not be played, can't find pent to break", pStream->m_breakIdx);
						}
					}
				}
			}

			int bPlayed = (pent) ? PlaybackStream(pStream) : 0;
			numPlayed += bPlayed;
			if (bPlayed == 0)
			{
				// Couldn't start, we need to keep the stream pending
				m_pendingStreams[n] = m_pendingStreams[p];
				n++;
			}
		}
		else
		{
			// Failed to playback the stream
			pStream->Release();
		}
	}
	for (; p < numPending; ++p)
	{
		// Couldn't process (numPlayed<BS_MAX_NUM_TO_PLAYBACK condition), we need to keep the streams pending
		m_pendingStreams[n] = m_pendingStreams[p];
		n++;
	}
	m_numPendingStreams = n;

	#if DEBUG_NET_BREAKAGE
	LOGBREAK("# breaks still to play:");
	for (int i = 0; i < m_numPendingStreams; i++)
	{
		LOGBREAK("#    pending: %d", m_pendingStreams[i]->m_breakIdx);
	}
	#endif
}

void* CBreakReplicator::SerialiseBreakage(TSerialize ser, BreakStream* pStream)
{
	CBitArray array(&ser);

	if (array.IsReading())
	{
		// Create the correct stream
		int type;
		int breakIdx;
		int subBreakIdx;
		array.Serialize(breakIdx, 0, NET_MAX_BREAKS - 1);
		array.Serialize(subBreakIdx, 0, 255);
		array.Serialize(type, 1, BreakStream::k_numTypes - 1);
		assert(pStream == NULL);
		if (type == BreakStream::k_partBreak)
			pStream = new PartBreak;
		if (type == BreakStream::k_planeBreak)
			pStream = new PlaneBreak;
		if (type == BreakStream::k_deformBreak)
			pStream = new DeformBreak;
		assert(pStream);
		assert(pStream->m_type == type);
		pStream->m_breakIdx = breakIdx;
		pStream->m_subBreakIdx = subBreakIdx;
	}
	else
	{
		array.Serialize(pStream->m_breakIdx, 0, NET_MAX_BREAKS - 1);
		array.Serialize(pStream->m_subBreakIdx, 0, 255);
		array.Serialize(pStream->m_type, 1, BreakStream::k_numTypes - 1);
	}

	#if DEBUG_NET_BREAKAGE
	LOGBREAK("# .-----------------------------------.");
	LOGBREAK("# |  %s (type: %s) (idx: %3d)", array.IsReading() ? "RECIEVED" : "SENDING", BreakStreamGetTypeName(pStream->m_type), pStream->m_breakIdx);
	LOGBREAK("# .-----------------------------------.");
	#endif

	pStream->SerializeWith(array);

	// Flush the bit serialiser
	if (!array.IsReading())
	{
		array.WriteToSerializer();
	}

	return pStream;
}

// This is the reciever of the low level RMI
void* CBreakReplicator::ReceiveSimpleBreakage(TSerialize ser)
{
	return SerialiseBreakage(ser, NULL);
}

void CBreakReplicator::PlaybackSimpleBreakage(void* userData, INetBreakageSimplePlaybackPtr pBreakage)
{
	if (userData)
	{
		// Check we haven't already received this break
		// This is only an issue after host-migration
		BreakStream* pStream = (BreakStream*)userData;
		if (pStream->m_breakIdx >= m_streams.size() || m_streams[pStream->m_breakIdx] == &s_dummyStream)
		{
			// A break stream has been serialised, push it on the pending playlist
			// The low level class, pBreakage, is used to collect any spawned entities
			// which need to be net bound
			CRY_ASSERT(!CCryAction::GetCryAction()->IsGamePaused());
			LOGBREAK("# PlaybackSimpleBreakage: breakIdx: %d", pStream->m_breakIdx);
			pStream->AddRef(); // Make sure any "dumb" pointers never delete this stream!
			pStream->Init(BreakStream::k_playing);
			pStream->m_collector = pBreakage;
			// Need to a create a new identifier
			// or get a more upto date one from the existing list
			GetIdentifier(&pStream->m_identifier, NULL, true);
			PushPlayback(pStream);
		}
	}
}

const EventPhysRemoveEntityParts* CBreakReplicator::GetRemovePartEvents(int& iNumEvents)
{
	iNumEvents = m_removePartEvents.size();
	if (iNumEvents)
		return &(m_removePartEvents[0]);
	else
		return NULL;
}

void CBreakReplicator::OnBrokeSomething(const SBreakEvent& be, EventPhysMono* pMono, bool isPlane)
{
	if (m_bDefineProtocolMode_server)
	{
		if (isPlane)
			BeginRecordingPlaneBreak(be, pMono);
		else
			BeginRecordingDeforminBreak(be, pMono);
	}
	else
	{
		// Client needs to send this to the server
		SClientGlassBreak glassBreak;
		glassBreak.m_planeBreak.m_identifier.CreateFromPhysicalEntity(pMono);
		glassBreak.m_planeBreak.m_be = be;
		CBreakReplicator::SendSvRecvGlassBreakWith(glassBreak, CCryAction::GetCryAction()->GetClientChannel());
	}
}

// Static callbacks from the physics events
int CBreakReplicator::OnJointBroken(const EventPhys* pEvent)
{
	m_pThis->OnJointBroken(static_cast<const EventPhysJointBroken*>(pEvent));
	return 1;
}

int CBreakReplicator::OnCreatePhysEntityPart(const EventPhys* pEvent)
{
	m_pThis->OnCreatePhysEntityPart(static_cast<const EventPhysCreateEntityPart*>(pEvent));
	return 1;
}

int CBreakReplicator::OnRemovePhysEntityParts(const EventPhys* pEvent)
{
	m_pThis->OnRemovePhysEntityParts(static_cast<const EventPhysRemoveEntityParts*>(pEvent));
	return 1;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE_SEND(CBreakReplicator, SvRecvGlassBreak, SClientGlassBreak);

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, SvRecvGlassBreak, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	// The client broke the glass - this is the server recieving the message
	// Playback the breakage - but do *not* put it into the break stream list
	PlaneBreak* planeBreak = (PlaneBreak*)&param.m_planeBreak;
	IPhysicalEntity* pent = planeBreak->m_identifier.FindPhysicalEntity();
	if (pent)
	{
		pent->AddRef(); // Released when the planeBreak is destroyed
		planeBreak->m_pent = pent;
		planeBreak->m_mode = BreakStream::k_playing;
		planeBreak->m_logHostMigrationInfo = 0;
		planeBreak->Playback();
	}
	return true;
}

/*
   ====================================================================================================================================================
   End - NET_USE_SIMPLE_BREAKAGE
   ====================================================================================================================================================
 */
#endif
