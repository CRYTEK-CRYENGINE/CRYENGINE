// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef physinterface_h
#define physinterface_h

#include <CryCore/Platform/platform.h>
#include <CryNetwork/SerializeFwd.h>
#include <CryMemory/CrySizer.h>
#include <CryMath/Cry_Geo.h>
#include <CryCore/stridedptr.h>
#include "primitives.h"
#ifdef NEED_ENDIAN_SWAP
	#include <CryCore/CryEndian.h>
#endif

//////////////////////////////////////////////////////////////////////////
// Physics defines.
//////////////////////////////////////////////////////////////////////////

enum EPE_Params
{
	ePE_params_pos                         = 0,
	ePE_player_dimensions                  = 1,
	ePE_params_car                         = 2,
	ePE_params_particle                    = 3,
	ePE_player_dynamics                    = 4,
	ePE_params_joint                       = 5,
	ePE_params_part                        = 6,
	ePE_params_sensors                     = 7,
	ePE_params_articulated_body            = 8,
	ePE_params_outer_entity                = 9,
	ePE_simulation_params                  = 10,
	ePE_params_foreign_data                = 11,
	ePE_params_buoyancy                    = 12,
	ePE_params_rope                        = 13,
	ePE_params_bbox                        = 14,
	ePE_params_flags                       = 15,
	ePE_params_wheel                       = 16,
	ePE_params_softbody                    = 17,
	ePE_params_area                        = 18,
	ePE_tetrlattice_params                 = 19,
	ePE_params_ground_plane                = 20,
	ePE_params_structural_joint            = 21,
	ePE_params_waterman                    = 22,
	ePE_params_timeout                     = 23,
	ePE_params_skeleton                    = 24,
	ePE_params_structural_initial_velocity = 25,
	ePE_params_collision_class             = 26,
	ePE_params_walking_rigid               = 27,

	ePE_Params_Count
};

enum EPE_Action
{
	ePE_action_move                 = 1,
	ePE_action_impulse              = 2,
	ePE_action_drive                = 3,
	ePE_action_reset                = 4,
	ePE_action_add_constraint       = 5,
	ePE_action_update_constraint    = 6,
	ePE_action_register_coll_event  = 7,
	ePE_action_awake                = 8,
	ePE_action_remove_all_parts     = 9,
	ePE_action_set_velocity         = 10,
	ePE_action_attach_points        = 11,
	ePE_action_target_vtx           = 12,
	ePE_action_reset_part_mtx       = 13,
	ePE_action_notify               = 14,
	ePE_action_auto_part_detachment = 15,
	ePE_action_move_parts           = 16,
	ePE_action_batch_parts_update   = 17,
	ePE_action_slice                = 18,

	ePE_Action_Count
};

enum EPE_GeomParams
{
	ePE_geomparams      = 0,
	ePE_cargeomparams   = 1,
	ePE_articgeomparams = 2,

	ePE_GeomParams_Count
};

enum EPE_Status
{
	ePE_status_pos                 = 1,
	ePE_status_living              = 2,
	ePE_status_vehicle             = 4,
	ePE_status_wheel               = 5,
	ePE_status_joint               = 6,
	ePE_status_awake               = 7,
	ePE_status_dynamics            = 8,
	ePE_status_collisions          = 9,
	ePE_status_id                  = 10,
	ePE_status_timeslices          = 11,
	ePE_status_nparts              = 12,
	ePE_status_contains_point      = 13,
	ePE_status_rope                = 14,
	ePE_status_vehicle_abilities   = 15,
	ePE_status_placeholder         = 16,
	ePE_status_softvtx             = 17,
	ePE_status_sensors             = 18,
	ePE_status_sample_contact_area = 19,
	ePE_status_caps                = 20,
	ePE_status_check_stance        = 21,
	ePE_status_waterman            = 22,
	ePE_status_area                = 23,
	ePE_status_extent              = 24,
	ePE_status_random              = 25,
	ePE_status_constraint          = 26,
	ePE_status_netpos              = 27,

	ePE_Status_Count
};

//! CStatoscope::AddPhysEntity must be updated when changing this enum.
enum pe_type { PE_NONE = 0, PE_STATIC = 1, PE_RIGID = 2, PE_WHEELEDVEHICLE = 3, PE_LIVING = 4, PE_PARTICLE = 5, PE_ARTICULATED = 6, PE_ROPE = 7, PE_SOFT = 8, PE_AREA = 9, PE_GRID = 10, PE_WALKINGRIGID = 11 };
enum sim_class { SC_STATIC = 0, SC_SLEEPING_RIGID = 1, SC_ACTIVE_RIGID = 2, SC_LIVING = 3, SC_INDEPENDENT = 4, SC_TRIGGER = 6, SC_DELETED = 7 };
struct IGeometry;
struct IPhysicalEntity;
struct IGeomManager;
struct IPhysicalWorld;
struct IPhysRenderer;
class ICrySizer;
struct IDeferredPhysicsEvent;
struct ILog;
IPhysicalEntity* const WORLD_ENTITY = (IPhysicalEntity*)-10;

#if CRY_PLATFORM_ARM
//FIXME: There's a threading issue in CryPhysics with ARM's weak memory ordering.
	#define MAX_PHYS_THREADS 1
#else
	#if defined(DEDICATED_SERVER)
		#define MAX_PHYS_THREADS 1
	#else
		#define MAX_PHYS_THREADS 4
	#endif
#endif

#if !defined(USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION) && !defined(DONT_USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION)
	#define USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION 1
#endif

/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// IPhysicsStreamer Interface /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

//! \cond INTERNAL
//! This is a callback interface for on-demand physicalization, physics gets a pointer to an implementation.
struct IPhysicsStreamer
{
	// <interfuscator:shuffle>
	virtual ~IPhysicsStreamer(){}

	//! Called whenever a placeholder (created through CreatePhysicalPlaceholder) requests a full entity.
	virtual int CreatePhysicalEntity(void* pForeignData, int iForeignData, int iForeignFlags) = 0;

	//! Called whenever a placeholder-owned entity expires.
	virtual int DestroyPhysicalEntity(IPhysicalEntity* pent) = 0;

	//! Called when on-demand entities in a box need to be physicalized (the grid is activated once RegisterBBoxInPODGrid is called).
	virtual int CreatePhysicalEntitiesInBox(const Vec3& boxMin, const Vec3& boxMax) = 0;

	//! Called when on-demand physicalized box expires.
	//! The streamer is expected to delete those that have a 0 refcounter, and keep the rest
	virtual int DestroyPhysicalEntitiesInBox(const Vec3& boxMin, const Vec3& boxMax) = 0;
	// </interfuscator:shuffle>
};

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// IPhysRenderer Interface /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

//! This is a callback interface for debug rendering, physics gets a pointer to an implementation.
struct IPhysRenderer
{
	// <interfuscator:shuffle>
	virtual ~IPhysRenderer(){}

	//! Draws helpers for the specified geometry (idxColor is in 0..7 range).
	//! \param color Used to specify an explicit color, only used if idxColor is negative.
	virtual void DrawGeometry(IGeometry* pGeom, struct geom_world_data* pgwd, int idxColor = 0, int bSlowFadein = 0, const Vec3& sweepDir = Vec3(0), const ColorF& color = ColorF(1, 1, 1, 1)) = 0;

	//! Draws a line for wireframe helpers.
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor = 0, int bSlowFadein = 0) = 0;

	//! Gets a descriptive name of the phys entity's owner (used solely for debug output).
	virtual const char* GetForeignName(void* pForeignData, int iForeignData, int iForeignFlags) = 0;

	//! Draws a text line (stauration is 0..1 and is currently used to represent stress level on a breakable joint).
	virtual void DrawText(const Vec3& pt, const char* txt, int idxColor, float saturation = 0) = 0;

	//! Sets an offset that is to be added to all subsequent draw requests (ZERO quat to keep the current one). Returns the previous offset
	virtual QuatT SetOffset(const Vec3& offs = Vec3(ZERO), const Quat& qrot = Quat(ZERO)) = 0;

	//! Draw a frame or a partial frame using a scale for the axes.
	//! \param pnt The world space position.
	//! \param axes The 3 axes normalized.
	//! \param scale A scale applied on the axes.
	//! \param limits The x, y, z radians for the Y, Z, X plane. If the pointer is not null, limits will be drawn in form of arcs.
	//! bitfield for what axes are locked.
	virtual void DrawFrame(const Vec3& pnt, const Vec3* axes, const float scale, const Vec3* limits, const int axes_locked) = 0;
	// </interfuscator:shuffle>
};

//! For "fastload" serialization.
class CMemStream
{
public:

	ILINE CMemStream(bool swap)
	{
		Prealloc();
		m_iPos = 0;
		bDeleteBuf = true;
		bSwapEndian = swap;
		bMeasureOnly = 0;
	}

	ILINE CMemStream(void* pbuf, int sz, bool swap)
	{
		m_pBuf = (char*)pbuf;
		m_nSize = sz;
		m_iPos = 0;
		bDeleteBuf = false;
		bSwapEndian = swap;
		bMeasureOnly = 0;
	}
	ILINE CMemStream()
	{
		m_pBuf = (char*)m_dummyBuf;
		m_iPos = 0;
		m_nSize = 0;
		bDeleteBuf = false;
		bSwapEndian = false;
		bMeasureOnly = -1;
	}

	virtual ~CMemStream()
	{
		if (bDeleteBuf)
			free(m_pBuf);
	}
	virtual void Prealloc()
	{
		m_pBuf = (char*)malloc(m_nSize = 0x1000);
	}

	ILINE void*                      GetBuf()               { return m_pBuf; }
	ILINE int                        GetUsedSize()          { return m_iPos; }
	ILINE int                        GetAllocatedSize()     { return m_nSize; }

	template<class ftype> ILINE void Write(const ftype& op) { Write(&op, sizeof(op)); }
	ILINE void                       Write(const void* pbuf, int sz)
	{
#if defined(MEMSTREAM_DEBUG)
		if (bMeasureOnly <= 0 && m_nSize && m_iPos + sz > m_nSize)
		{
			printf("overflow: %d + %d >= %d\n", m_iPos, sz, m_nSize);
		}
#endif
		if (!bMeasureOnly)
		{
			if (m_iPos + sz > m_nSize)
				GrowBuf(sz);
			memcpy(m_pBuf + m_iPos, pbuf, (unsigned int)sz);
		}
		m_iPos += sz;
	}

	virtual void GrowBuf(int sz)
	{
		int prevsz = m_nSize;
		char* prevbuf = m_pBuf;
		m_pBuf = (char*)malloc(m_nSize = (m_iPos + sz - 1 & ~0xFFF) + 0x1000);
		memcpy(m_pBuf, prevbuf, (unsigned int)prevsz);
		free(prevbuf);
	}

	template<class ftype> ILINE void Read(ftype& op)
	{
		ReadRaw(&op, sizeof(op));
#if defined (NEED_ENDIAN_SWAP)
		if (bSwapEndian)
			SwapEndian(op);
#endif
	}

	template<class ftype> ILINE ftype Read()
	{
		ftype val;
		Read(val);
		return val;
	}
	template<class ftype> ILINE void ReadType(ftype* op, int count = 1)
	{
		ReadRaw(op, sizeof(*op) * count);
#if defined (NEED_ENDIAN_SWAP)
		if (bSwapEndian)
			while (count-- > 0)
				SwapEndian(*op++);
#endif
	}
	ILINE void ReadRaw(void* pbuf, int sz)
	{
#if defined(MEMSTREAM_DEBUG)
		if (bMeasureOnly <= 0 && m_nSize && m_iPos + sz > m_nSize)
		{
			printf("overflow: %d + %d >= %d\n", m_iPos, sz, m_nSize);
		}
#endif
		memcpy(pbuf, (m_pBuf + m_iPos), (unsigned int)sz);
		m_iPos += sz;
	}

	char* m_pBuf, m_dummyBuf[4];
	int   m_iPos, m_nSize;
	bool  bDeleteBuf;
	bool  bSwapEndian;
	int   bMeasureOnly;
};
//! \endcond

// Workaround for bug in GCC 4.8. The kind of access patterns here leads to an internal
// compiler error in GCC 4.8 when optimizing with debug symbols. Two possible solutions
// are available, compile in Profile mode without debug symbols or remove optimizations
// in the code where the bug occurs
// see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=59776
#if defined(_PROFILE) && !defined(__clang__) && (__GNUC__ == 4) && (__GNUC_MINOR__ == 8)
// Cannot use #pragma GCC optimize("O0") because it causes a system crash when using
// the gcc compiler for another platform
	#define CRY_GCC48_AVOID_OPTIMIZE __attribute__((optimize("-O0")))
#else
	#define CRY_GCC48_AVOID_OPTIMIZE
#endif

//! \cond INTERNAL
//! Unused_marker deliberately fills a variable with invalid data.
//! This is so that later is_unused() can check whether it was initialized (this is used in all physics params/status/action structures)
class unused_marker
{
public:
	union f2i
	{
		float  f;
		uint32 i;
	};
	union d2i
	{
		double d;
		uint32 i[2];
	};
	unused_marker() {}
	unused_marker&                     operator,(float& x) CRY_GCC48_AVOID_OPTIMIZE;
	unused_marker&                     operator,(double& x) CRY_GCC48_AVOID_OPTIMIZE;
	unused_marker&                     operator,(int& x) CRY_GCC48_AVOID_OPTIMIZE;
	unused_marker&                     operator,(unsigned int& x) CRY_GCC48_AVOID_OPTIMIZE;
	template<class ref> unused_marker& operator,(ref*& x)                        { x = (ref*)-1; return *this; }
	template<class F> unused_marker&   operator,(Vec3_tpl<F>& x)                 { return *this, x.x; }
	template<class F> unused_marker&   operator,(Quat_tpl<F>& x)                 { return *this, x.w; }
	template<class F> unused_marker&   operator,(strided_pointer<F>& x)          { return *this, x.data; }
};
//! \endcond

inline unused_marker& unused_marker::operator,(float& x)        { *alias_cast<int*>(&x) = 0xFFBFFFFF; return *this; }
inline unused_marker& unused_marker::operator,(double& x)       { (alias_cast<int*>(&x))[false ? 1 : 0] = 0xFFF7FFFF; return *this; }
inline unused_marker& unused_marker::operator,(int& x)          { x = 1 << 31; return *this; }
inline unused_marker& unused_marker::operator,(unsigned int& x) { x = 1u << 31; return *this; }

#undef CRY_GCC48_AVOID_OPTIMIZE

inline bool              is_unused(const float& x)         { unused_marker::f2i u; u.f = x; return (u.i & 0xFFA00000) == 0xFFA00000; }

inline bool              is_unused(int x)                  { return x == 1 << 31; }
inline bool              is_unused(unsigned int x)         { return x == 1u << 31; }
template<class ref> bool is_unused(ref* x)                 { return x == (ref*)-1; }
template<class ref> bool is_unused(strided_pointer<ref> x) { return is_unused(x.data); }
template<class F> bool   is_unused(const Ang3_tpl<F>& x)   { return is_unused(x.x); }
template<class F> bool   is_unused(const Vec3_tpl<F>& x)   { return is_unused(x.x); }
template<class F> bool   is_unused(const Quat_tpl<F>& x)   { return is_unused(x.w); }
inline bool              is_unused(const double& x)        { unused_marker::d2i u; u.d = x; return (u.i[eLittleEndian ? 1 : 0] & 0xFFF40000) == 0xFFF40000; }
#define MARK_UNUSED unused_marker(),

// validators do nothing in the interface, but inside the physics they are redefined
// so that they check the input for consistency and report errors
#if !defined(VALIDATOR_LOG)
	#define VALIDATOR_LOG(pLog, str)
	#define VALIDATORS_START
	#define VALIDATOR(member)
	#define VALIDATOR_NORM(member)
	#define VALIDATOR_NORM_MSG(member, msg, member1)
	#define VALIDATOR_RANGE(member, minval, maxval)
	#define VALIDATOR_RANGE2(member, minval, maxval)
	#define VALIDATORS_END
#endif

////////// physics entity collision filtering class enums /////////////////

enum pe_collision_class
{
	/// reserved basic collision classes
	collision_class_terrain     = 1 << 0,
	collision_class_wheeled     = 1 << 1,
	collision_class_living      = 1 << 2,
	collision_class_articulated = 1 << 3,
	collision_class_soft        = 1 << 4,
	collision_class_rope        = 1 << 5,
	collision_class_particle    = 1 << 6,
	// begin game specific ones from this enum
	collision_class_game        = 1 << 10,
};

struct SCollisionClass
{
	uint32 type;     //!< Collision_class flags to identify the entity.
	uint32 ignore;   //!< Another entity will be ignored if *any* of these bits are set in its type.

	SCollisionClass() {}

	SCollisionClass(uint32 t, uint32 i)
	{
		type = t;
		ignore = i;
	}
};

ILINE int IgnoreCollision(const SCollisionClass& a, const SCollisionClass& b)
{
	return (a.type & b.ignore) | (b.type & a.ignore);
}

// in physics interface [almost] all parameters are passed via structures
// this allows having stable interface methods and flexible default arguments system

////////////////////////// Params structures /////////////////////

////////// common params
struct pe_params
{
	int type;
};

//! Sets position and orientation of entity.
//! \par Example
//! \include CryPhysics/Examples/SetPhysicalEntityPosition.cpp
struct pe_params_pos : pe_params
{
	enum entype { type_id = ePE_params_pos };
	pe_params_pos()
	{
		type = type_id;
		MARK_UNUSED pos, scale, q, iSimClass, pGridRefEnt;
		pMtx3x4 = 0;
		pMtx3x3 = 0;
		bRecalcBounds = 1;
		bEntGridUseOBB = 0;
	}

	Vec3        pos;
	quaternionf q;
	float       scale;          //!< Note that since there's no per-entity scale, it gets 'baked' into individual parts' scales.
	Matrix34*   pMtx3x4;        //!< Optional position+orientation.
	Matrix33*   pMtx3x3;        //!< Optional orientation via 3x3 matrix.
	int         iSimClass;      //!< See the sim_class enum.
	int         bRecalcBounds;  //!< Tells to recompute the bounding boxes.
	bool        bEntGridUseOBB; //!< Whether or not to use part OBBs rather than object AABB when registering in the entity grid.

	const IPhysicalEntity* pGridRefEnt;	//!< New grid (set via a reference entity)

	VALIDATORS_START
	  VALIDATOR(pos)
	VALIDATOR_NORM_MSG(q, "(perhaps non-uniform scaling was used?)", pos)
	VALIDATOR(scale)
	VALIDATORS_END
};

//! Sets or gets the bounding box of a physical entity
struct pe_params_bbox : pe_params
{
	enum entype { type_id = ePE_params_bbox };
	pe_params_bbox() { type = type_id; MARK_UNUSED BBox[0], BBox[1]; }
	Vec3 BBox[2]; //!< Force this bounding box (note that if the entity recomputes it later, it will override this).

	VALIDATORS_START
	  VALIDATOR(BBox[0])
	VALIDATOR(BBox[1])
	VALIDATORS_END
};

struct pe_params_outer_entity : pe_params
{
	enum entype { type_id = ePE_params_outer_entity };
	pe_params_outer_entity() { type = type_id; pOuterEntity = 0; }

	IPhysicalEntity* pOuterEntity;      //!< Outer entity is used to group together SC_INDEPENDENT entities (example: ropes on a tree trunk) and order their updates.
};

struct ITetrLattice;

//! Sets geometrical parameters of entity part.
struct pe_params_part : pe_params
{
	enum entype { type_id = ePE_params_part };
	pe_params_part()
	{
		type = type_id;
		MARK_UNUSED pos, q, scale, partid, ipart, mass, density, pPhysGeom, pPhysGeomProxy, idmatBreakable, pLattice, pMatMapping, minContactDist, flagsCond, idSkeleton, invTimeStep, idParent;
		pMtx3x4 = 0;
		pMtx3x3 = 0;
		bRecalcBBox = 1;
		bAddrefGeoms = 0;
		flagsOR = flagsColliderOR = 0;
		flagsAND = flagsColliderAND = (unsigned)-1;
	}

	int                   partid;      //!< Identifier of part.
	int                   ipart;       //!< Optionally, internal part slot number.
	int                   bRecalcBBox; //!< Whether entity's bounding box should be recalculated.
	Vec3                  pos;
	quaternionf           q;
	float                 scale;
	Matrix34*             pMtx3x4;           //!< Optional position+orientation.
	Matrix33*             pMtx3x3;           //!< Optional orientation via 3x3 matrix.
	unsigned int          flagsCond;         //!< If partid and ipart are not specified, check for parts with flagsCond set.
	unsigned int          flagsOR, flagsAND; //!< New flags = (flags & flagsAND) | flagsOR.
	unsigned int          flagsColliderOR, flagsColliderAND;
	float                 mass;    //!< Either mass of density should be set; mass = density*volume.
	float                 density;
	float                 minContactDist;              //!< Threshold for contact points generation.
	struct phys_geometry* pPhysGeom, * pPhysGeomProxy; //!< If present and different from pPhysGeomProxy, pPhysGeom is used for raytracing.
	int                   idmatBreakable;              //!< If >=0, the part is procedurally breakable with this mat_id (see AddExplosionShape).
	ITetrLattice*         pLattice;                    //!< Lattice is used for soft bodies and procedural structural breaking.
	int                   idSkeleton;                  //!< Part with this id becomes this part's deformation skeleton.
	int*                  pMatMapping;                 //!< Material mapping table for this part.
	int                   nMats;                       //!< Number of pMatMapping entries.
	float                 invTimeStep;                 //!< 1.0f/time_step, ragdolls will compute joint's velocity if this and position is set.
	int                   bAddrefGeoms;                //!< AddRef returned geometries if used in GetParams.
	int                   idParent;                    //!< Parent for hierarchical breaking; it hides all children until at least one of them breaks off.

	VALIDATORS_START
	  VALIDATOR(pos)
	VALIDATOR_NORM_MSG(q, "(perhaps non-uniform scaling was used in the asset?)", pt)
	VALIDATOR(scale)
	VALIDATORS_END
};

//!< Attaches optional ray sensors to an entity; only living entities support it.
struct pe_params_sensors : pe_params
{
	enum entype { type_id = ePE_params_sensors };
	pe_params_sensors() { type = type_id; nSensors = 0; pOrigins = 0; pDirections = 0; }

	int         nSensors;       //!< Number of sensors.
	const Vec3* pOrigins;       //!< Sensor's origins in entity CS.
	const Vec3* pDirections;    //!< Sensor's directions (dir*ray length) in entity CS.
};

//! Used to set or get simulation parameters such as gravity and mass
//! \par Example
//! \include CryPhysics/Examples/GetSimulationParams.cpp
struct pe_simulation_params : pe_params
{
	enum entype { type_id = ePE_simulation_params };
	pe_simulation_params()
	{
		type = type_id;
		MARK_UNUSED maxTimeStep, gravity, minEnergy, damping, iSimClass,
		            dampingFreefall, gravityFreefall, mass, density, maxLoggedCollisions, maxRotVel, disablePreCG, maxFriction, collTypes;
	}

	int   iSimClass;
	float maxTimeStep;       //!< Maximum time step that entity can accept (larger steps will be split).
	float minEnergy;         //!< Minimun of kinetic energy below which entity falls asleep (divided by mass).
	float damping;           //!< Damped velocity = oridinal velocity * (1 - damping*time interval).
	Vec3  gravity;           //!< Per-entity gravity (note that if there are any phys areas with gravity, they will override it unless pef_ignore_areas is set.
	float dampingFreefall;   //!< Damping and gravity used when there are no collisions.
	Vec3  gravityFreefall;   //!< NOTE: if left unused, gravity value will be substituted (if provided).
	float maxRotVel;         //!< Rotational velocity is clamped to this value.
	float mass;              //!< Either mass of density should be set; mass = density*volume.
	float density;
	int   maxLoggedCollisions; //!< Maximum EventPhysCollisions reported per frame (only supported by rigid bodies/ragdolls/vehicles).
	int   disablePreCG;        //!< Disables Pre-CG solver for the group this body is in (recommended for balls).
	float maxFriction;         //!< Sets upper friction limit for this object and all objects it's currently in contact with.
	int   collTypes;           //!< Collision types (a combination of ent_xxx flags).
};

struct pe_params_foreign_data : pe_params
{
	enum entype { type_id = ePE_params_foreign_data };
	pe_params_foreign_data() { type = type_id; MARK_UNUSED pForeignData, iForeignData, iForeignFlags; iForeignFlagsAND = -1; iForeignFlagsOR = 0; }

	void* pForeignData;                      //!< Foreign data is an arbitrary pointer used to associate physical entity with its owner object.
	int   iForeignData;                      //!< Foreign data types (defined in IPhysics.h).
	int   iForeignFlags;                     //!< Any flags the owner wants to store.
	int   iForeignFlagsAND, iForeignFlagsOR; //!< When setting, flagsNew = flags & flagsAND | flagsOR.
};

struct pe_params_buoyancy : pe_params
{
	enum entype { type_id = ePE_params_buoyancy };
	pe_params_buoyancy()
	{
		type = type_id;
		iMedium = 0;
		MARK_UNUSED waterDensity, kwaterDensity, waterDamping,
		            waterPlane.n, waterPlane.origin, waterEmin, waterResistance, kwaterResistance, waterFlow, flowVariance;
	};
	enum EMediumType
	{
		eWater = 0,
		eAir   = 1
	};

	float             waterDensity;  //!< Overrides water density from the current water volume for an entity; sets for water areas.
	float             kwaterDensity; //!< Scales water density from the current water volume (used for entities only).
	// NOTE: for entities , waterDensity override is stored as kwaterDensity relative to the global area's density.
	float             waterDamping;                      //!< Uniform damping while submerged, will be scaled with submerged fraction.
	float             waterResistance, kwaterResistance; //!< Water's medium resistance; same comments on water and kwater apply.
	Vec3              waterFlow;                         //!< Flow's movement vector; can only be set for a water area.
	float             flowVariance;                      //!< Not yet supported.
	primitives::plane waterPlane;                        //!< Positive normal = above the water surface.
	float             waterEmin;                         //!< Sleep energy while floating with no contacts (see minEnergy in pe_simulation_params).
	int               iMedium;                           //!< 0 for water, 1 for air.
};

enum phentity_flags
{
	// PE_PARTICLE-specific flags
	particle_single_contact       = 0x01,  //!< Full stop after first contact.
	particle_constant_orientation = 0x02,  //!< Forces constant orientation.
	particle_no_roll              = 0x04,  //!< 'sliding' mode; entity's 'normal' vector axis will be alinged with the ground normal.
	particle_no_path_alignment    = 0x08,  //!< Unless set, entity's y axis will be aligned along the movement trajectory.
	particle_no_spin              = 0x10,  //!< Disables spinning while flying.
	particle_no_self_collisions   = 0x100, //!< Disables collisions with other particles.
	particle_no_impulse           = 0x200, //!< Particle will not add hit impulse (expecting that some other system will).

	// PE_LIVING-specific flags
	lef_push_objects            = 0x01, lef_push_players = 0x02, //!< Push objects and players during contacts.
	lef_snap_velocities         = 0x04,                          //!< Quantizes velocities after each step (was ised in MP for precise deterministic sync).
	lef_loosen_stuck_checks     = 0x08,                          //!< Don't do additional intersection checks after each step (recommended for NPCs to improve performance).
	lef_report_sliding_contacts = 0x10,                          //!< Unless set, 'grazing' contacts are not reported.

	// PE_ROPE-specific flags
	rope_findiff_attached_vel        = 0x01,                              //!< Approximate velocity of the parent object as v = (pos1-pos0)/time_interval.
	rope_no_solver                   = 0x02,                              //!< No velocity solver; will rely on stiffness (if set) and positional length enforcement.
	rope_ignore_attachments          = 0x4,                               //!< No collisions with objects the rope is attached to.
	rope_target_vtx_rel0             = 0x08, rope_target_vtx_rel1 = 0x10, //!< Whether target vertices are set in the parent entity's frame.
	rope_subdivide_segs              = 0x100,                             //!< Turns on 'dynamic subdivision' mode (only in this mode contacts in a strained state are handled correctly).
	rope_no_tears                    = 0x200,                             //!< Rope will not tear when it reaches its force limit, but stretch.
	rope_collides                    = 0x200000,                          //!< Rope will collide with objects other than the terrain.
	rope_collides_with_terrain       = 0x400000,                          //!< Rope will collide with the terrain.
	rope_collides_with_attachment    = 0x80,                              //!< Rope will collide with the objects it's attached to even if the other collision flags are not set.
	rope_no_stiffness_when_colliding = 0x10000000,                        //!< Rope will use stiffness 0 if it has contacts.

	//! PE_SOFT-specific flags
	se_skip_longest_edges = 0x01, //!< the longest edge in each triangle with not participate in the solver
	se_rigid_core         = 0x02, //!< soft body will have an additional rigid body core

	//! PE_RIGID-specific flags (note that PE_ARTICULATED and PE_WHEELEDVEHICLE are derived from it)
	ref_use_simple_solver = 0x01,                                //!< use penalty-based solver (obsolete)
	ref_no_splashes       = 0x04,                                //!< will not generate EventPhysCollisions when contacting water
	ref_checksum_received = 0x04, ref_checksum_outofsync = 0x08, //!< obsolete
	ref_small_and_fast    = 0x100,                               //!< entity will trace rays against alive characters; set internally unless overriden

	//! PE_ARTICULATED-specific flags
	aef_recorded_physics = 0x02, //!< specifies a an entity that contains pre-baked physics simulation

	//! PE_WHEELEDVEHICLE-specific flags
	wwef_fake_inner_wheels = 0x08, //!< exclude wheels between the first and the last one from the solver
	                               //! (only wheels with non-0 suspension are considered)

	//! general flags
	pef_parts_traceable           = 0x10,                                                      //!< each entity part will be registered separately in the entity grid
	pef_disabled                  = 0x20,                                                      //!< entity will not be simulated
	pef_never_break               = 0x40,                                                      //!< entity will not break or deform other objects
	pef_deforming                 = 0x80,                                                      //!< entity undergoes a dynamic breaking/deforming
	pef_pushable_by_players       = 0x200,                                                     //!< entity can be pushed by playerd
	pef_traceable                 = 0x400, particle_traceable = 0x400, rope_traceable = 0x400, //!< entity is registered in the entity grid
	pef_update                    = 0x800,                                                     //!< only entities with this flag are updated if ent_flagged_only is used in TimeStep()
	pef_monitor_state_changes     = 0x1000,                                                    //!< generate immediate events for simulation class changed (typically rigid bodies falling asleep)
	pef_monitor_collisions        = 0x2000,                                                    //!< generate immediate events for collisions
	pef_monitor_env_changes       = 0x4000,                                                    //!< generate immediate events when something breaks nearby
	pef_never_affect_triggers     = 0x8000,                                                    //!< don't generate events when moving through triggers
	pef_invisible                 = 0x10000,                                                   //!< will apply certain optimizations for invisible entities
	pef_ignore_ocean              = 0x20000,                                                   //!< entity will ignore global water area
	pef_fixed_damping             = 0x40000,                                                   //!< entity will force its damping onto the entire group
	pef_monitor_poststep          = 0x80000,                                                   //!< entity will generate immediate post step events
	pef_always_notify_on_deletion = 0x100000,                                                  //!< when deleted, entity will awake objects around it even if it's not referenced (has refcount 0)
	pef_override_impulse_scale    = 0x200000,                                                  //!< entity will ignore breakImpulseScale in PhysVars
	pef_players_can_break         = 0x400000,                                                  //!< playes can break the entiy by bumping into it
	pef_cannot_squash_players     = 0x10000000,                                                //!< entity will never trigger 'squashed' state when colliding with players
	pef_ignore_areas              = 0x800000,                                                  //!< entity will ignore phys areas (gravity and water)
	pef_log_state_changes         = 0x1000000,                                                 //!< entity will log simulation class change events
	pef_log_collisions            = 0x2000000,                                                 //!< entity will log collision events
	pef_log_env_changes           = 0x4000000,                                                 //!< entity will log EventPhysEnvChange when something breaks nearby
	pef_log_poststep              = 0x8000000,                                                 //!< entity will log EventPhysPostStep events
};

struct pe_params_flags : pe_params
{
	enum entype { type_id = ePE_params_flags };
	pe_params_flags() { type = type_id; MARK_UNUSED flags, flagsOR, flagsAND; }
	unsigned int flags;
	unsigned int flagsOR;  //!< when setting, flagsNew = (flags set ? flags:flagsOld) & flagsAND | flagsOR
	unsigned int flagsAND; //!< when getting, only flags is filled
};

struct pe_params_collision_class : pe_params
{
	enum entype { type_id = ePE_params_collision_class };
	pe_params_collision_class() { type = type_id; collisionClassOR.type = collisionClassOR.ignore = 0; collisionClassAND.type = collisionClassAND.ignore = (unsigned)-1; }
	SCollisionClass collisionClassOR;   //!< When getting both collisionClassOR and collisionClassAND are filled out
	SCollisionClass collisionClassAND;  //!< When setting first collisionClassAND is applied to mask bits, then collisionClassOR is applied to turn on collision bits
};

struct pe_params_ground_plane : pe_params
{
	//! used for breakable objects; pieces that are below ground (at least partially) stay in the entity
	enum entype { type_id = ePE_params_ground_plane };
	pe_params_ground_plane() { type = type_id; iPlane = 0; MARK_UNUSED ground.origin, ground.n; }
	int               iPlane; //!< index of the plane to be set (-1 removes existing planes)
	primitives::plane ground;
};

enum special_joint_ids { joint_impulse = 1000000 };
struct pe_params_structural_joint : pe_params
{
	enum entype { type_id = ePE_params_structural_joint };
	pe_params_structural_joint()
	{
		type = type_id;
		id = 0;
		bReplaceExisting = 0;
		MARK_UNUSED idx, partid[0], partid[1], pt, n, maxForcePush, maxForcePull, maxForceShift, maxTorqueBend, maxTorqueTwist, damageAccum, damageAccumThresh,
		            bBreakable, szSensor, bBroken, partidEpicenter, axisx, limitConstraint, bConstraintWillIgnoreCollisions, dampingConstraint, bDirectBreaksOnly;
	}

	int   id;                                        //!< joint's 'foreign' identifier
	int   idx;                                       //!< joint's internal index
	int   bReplaceExisting;                          //!< if not set, SetParams will add a new joint even if id is already used
	int   partid[2];                                 //!< ids of the parts this joint connects (-1 for ground)
	Vec3  pt;                                        //!< point in entity space
	Vec3  n;                                         //!< push/pull direction in entity space
	Vec3  axisx;                                     //!< x axis in entity frame; only used for joints that can become dynamic constraints
	float maxForcePush, maxForcePull, maxForceShift; //!< linear force limits
	float maxTorqueBend, maxTorqueTwist;             //!< angular force (torque) limits
	float damageAccum, damageAccumThresh;            //!< fraction of tension that gets accumulated, can be used to emulate an health system
	Vec3  limitConstraint;                           //!< x=min angle, y=max angle, z=force limit
	int   bBreakable;                                //!< joint is at all breakable
	int   bConstraintWillIgnoreCollisions;           //!< dynamic constraints will have constraint_ignore_buddy flag
	int   bDirectBreaksOnly;                         //!< joint can only be broken by direct impulses to one of the parts it connects
	float dampingConstraint;                         //!< dynamic constraint's damping
	float szSensor;                                  //!< sensor geometry size; used to re-attach the joint when parts break off
	int   bBroken;                                   //!< joint is broken
	int   partidEpicenter;                           //!< tension recomputation will start from this part (used for network playback, for instance)
};

struct pe_params_structural_initial_velocity : pe_params
{
	//! Setting of initial velocities of parts before breaking joints on clients through pe_params_structural_joint
	enum entype { type_id = ePE_params_structural_initial_velocity };
	pe_params_structural_initial_velocity() { type = type_id; }

	int  partid; //!< id of the part to prepare for breakage
	Vec3 v;      //!< Initial velocity
	Vec3 w;      //!< Initial ang velocity
};

struct pe_params_timeout : pe_params
{
	//! entities can be forced to go to sleep after some time without external impulses
	enum entype { type_id = ePE_params_timeout };
	pe_params_timeout() { type = type_id; MARK_UNUSED timeIdle, maxTimeIdle; }
	float timeIdle;    //!< current 'idle' time (time without any 'prods' from outside)
	float maxTimeIdle; //!< sleep when timeIdle>maxTimeIdle; 0 turns this feature off
};

struct pe_params_skeleton : pe_params
{
	//! skeleton is a hidden mesh that uses cloth simulation to skin the main physics geometry
	enum entype { type_id = ePE_params_skeleton };
	pe_params_skeleton() { type = type_id; MARK_UNUSED partid, ipart, stiffness, thickness, maxStretch, maxImpulse, timeStep, nSteps, hardness, explosionScale, bReset; }

	int   partid;         //!< id of the skinned part
	int   ipart;          //!< ..or its internal index
	float stiffness;      //!< skeleton's hardness against bending and shearing
	float thickness;      //!< skeleton's thickness for collisions
	float maxStretch;     //!< skeleton's maximal stretching
	float maxImpulse;     //!< skeleton impulse cap
	float timeStep;       //!< time step, used to simulate the skeleton (typically small)
	int   nSteps;         //!< number of skeleton sub-steps per each structure update
	float hardness;       //!< skeleton's hardness against stretching
	float explosionScale; //!< skeleton's explosion impulse scale
	int   bReset;         //!< resets the skeleton to its original pose
};

////////// articulated entity params
enum joint_flags
{
	angle0_locked                = 1, all_angles_locked = 7, angle0_limit_reached = 010, angle0_auto_kd = 0100, joint_no_gravity = 01000,
	joint_isolated_accelerations = 02000, joint_expand_hinge = 04000, angle0_gimbal_locked = 010000,
	joint_dashpot_reached        = 0100000, joint_ignore_impulses = 0200000
};

struct pe_params_joint : pe_params
{
	enum entype { type_id = ePE_params_joint };
	pe_params_joint()
	{
		type = type_id;
		for (int i = 0; i < 3; i++)
			MARK_UNUSED limits[0][i], limits[1][i], qdashpot[i], kdashpot[i], bounciness[i], q[i], qext[i], ks[i], kd[i], qtarget[i];
		bNoUpdate = 0;
		pMtx0 = 0;
		flagsPivot = 3;
		MARK_UNUSED flags, q0, pivot, ranimationTimeStep, nSelfCollidingParts, pSelfCollidingParts, animationTimeStep, op[0];
	}

	unsigned int flags;      //!< should be a combination of angle0,1,2_locked, angle0,1,2_auto_kd, joint_no_gravity
	int          flagsPivot; //!< if bit 0 is set, update pivot point in parent frame, if bit 1 - in child
	Vec3         pivot;      //!< joint pivot in entity CS
	quaternionf  q0;         //!< orientation of child in parent coordinates that corresponds to angles (0,0,0)
	Matrix33*    pMtx0;      //!< same as q0
	Vec3         limits[2];  //!< limits for each angle
	Vec3         bounciness; //!< bounciness for each angle (applied when limit is reached)
	Vec3         ks, kd;     //!< stiffness and damping koefficients for each angle angular spring
	Vec3         qdashpot;   //!< limit vicinity where joints starts resisting movement
	Vec3         kdashpot;   //!< when dashpot is activated, this is roughly the angular speed, stopped in 2 sec
	Ang3         q;          //!< angles values
	Ang3         qext;       //!< additional angles values (angle[i] = q[i]+qext[i]; only q[i] is taken into account
	//! while calculating spring torque
	Ang3         qtarget;
	int          op[2];                                      //!< body identifiers of parent (optional) and child respectively
	int          nSelfCollidingParts, * pSelfCollidingParts; //!< part ids of only parts that should be checked for self-collision
	int          bNoUpdate;                                  //!< omit recalculation of body parameters after changing this joint
	float        animationTimeStep;                          //!< used to calculate joint velocities of animation
	float        ranimationTimeStep;                         //!< 1/animation time step, can be not specified (specifying just saves extra division operation)

	VALIDATORS_START
	  VALIDATOR(pivot)
	VALIDATOR_NORM(q0)
	VALIDATOR(q)
	VALIDATOR(qext)
	VALIDATORS_END
};

struct pe_params_articulated_body : pe_params
{
	enum entype { type_id = ePE_params_articulated_body };
	pe_params_articulated_body()
	{
		type = type_id;
		MARK_UNUSED bGrounded, bInheritVel, bCheckCollisions, bCollisionResp, nJointsAlloc;
		MARK_UNUSED bGrounded, bInheritVel, bCheckCollisions, bCollisionResp, a, wa, w, v, pivot, scaleBounceResponse, posHostPivot, qHostPivot;
		MARK_UNUSED bAwake, pHost, nCollLyingMode, gravityLyingMode, dampingLyingMode, minEnergyLyingMode, iSimType, iSimTypeLyingMode, nRoots;
		bApply_dqext = 0;
		bRecalcJoints = 1;
	}

	int              bGrounded;           //!< whether body's pivot is firmly attached to something or free
	int              bCheckCollisions;    //!< only works with bCollisionResp set
	int              bCollisionResp;      //!< when on, uses 'ragdoll' simulation mode, when off - 'skeleton' (for hit simulation on alive actors)
	Vec3             pivot;               //!< attachment position for grounded entities
	Vec3             a;                   //!< acceleration of the ground for grounded entities
	Vec3             wa;                  //!< angular acceleration of the ground for grounded entities
	Vec3             w;                   //!< angular velocity of the ground for grounded entities
	Vec3             v;                   //!< linear velocity of the ground for grounded entities
	float            scaleBounceResponse; //!< scales impulsive torque that is applied at a joint that has just reached its limit
	int              bApply_dqext;        //!< adds current dqext to joints velocities. dqext is the speed of external animation and is calculated each time
	                                      //! qext is set for joint (as difference between new value and current value, multiplied by inverse of animation timestep)
	int              bAwake;              //!< current state

	IPhysicalEntity* pHost;        //!< 'ground' entity
	Vec3             posHostPivot; //!< attachment position inside pHost
	quaternionf      qHostPivot;
	int              bInheritVel; //!< take pHost velocity into account during the simulation

	int              nCollLyingMode;     //!< number of contacts that triggers 'lying mode'
	Vec3             gravityLyingMode;   //!< gravity override in lying mode
	float            dampingLyingMode;   //!< damping override
	float            minEnergyLyingMode; //!< sleep speed override
	int              iSimType;           //!< simulation type: 0-'joint-based', 1-'body-based'; fast motion forces joint-based mode automatically
	int              iSimTypeLyingMode;  //!< simulation type override
	int              nRoots;             //!< only used in GetParams
	int              nJointsAlloc;       //!< pre-allocates this amount of joints and parts

	int              bRecalcJoints; //!< re-build geometry positions from joint agnles
};

////////// living entity params

//! Determines the dimensions of the main capsule (or cylinder) of a living / walking entity
//! \par Example
//! \include CryEntitySystem/Examples/PhysicalizeLiving.cpp
struct pe_player_dimensions : pe_params
{
	enum entype { type_id = ePE_player_dimensions };
	pe_player_dimensions() : dirUnproj(0, 0, 1), maxUnproj(0)
	{
		type = type_id;
		MARK_UNUSED sizeCollider, heightPivot, heightCollider, heightEye, heightHead, headRadius, bUseCapsule, groundContactEps;
	}

	float heightPivot;      //!< offset from central ground position that is considered entity center
	float heightEye;        //!< vertical offset of camera
	Vec3  sizeCollider;     //!< collision cylinder dimensions
	float heightCollider;   //!< vertical offset of collision geometry center
	float headRadius;       //!< radius of the 'head' geometry (used for camera offset)
	float heightHead;       //!< center.z of the head geometry
	Vec3  dirUnproj;        //!< unprojection direction to test in case the new position overlaps with the environment (can be 0 for 'auto')
	float maxUnproj;        //!< maximum allowed unprojection
	int   bUseCapsule;      //!< switches between capsule and cylinder collider geometry
	float groundContactEps; //!< the amount that the living needs to move upwards before ground contact is lost. defaults to which ever is greater 0.004, or 0.01*geometryHeight

	VALIDATORS_START
	  VALIDATOR(heightPivot)
	VALIDATOR(heightEye)
	VALIDATOR_RANGE2(sizeCollider, 0, 100)
	VALIDATORS_END
};

//! Determines the dynamics of a living / walking entity
//! \par Example
//! \include CryEntitySystem/Examples/PhysicalizeLiving.cpp
struct pe_player_dynamics : pe_params
{
	enum entype { type_id = ePE_player_dynamics };
	pe_player_dynamics()
	{
		type = type_id;
		MARK_UNUSED kInertia, kInertiaAccel, kAirControl, gravity, gravity.z, nodSpeed, mass, bSwimming, surface_idx, bActive, collTypes, pLivingEntToIgnore;
		MARK_UNUSED minSlideAngle, maxClimbAngle, maxJumpAngle, minFallAngle, kAirResistance, bNetwork, maxVelGround, timeImpulseRecover, iRequestedTime, bReleaseGroundColliderWhenNotActive;
	}

	float            kInertia;                            //!< inertia koefficient, the more it is, the less inertia is; 0 means no inertia
	float            kInertiaAccel;                       //!< inertia on acceleration
	float            kAirControl;                         //!< air control koefficient 0..1, 1 - special value (total control of movement)
	float            kAirResistance;                      //!< standard air resistance
	Vec3             gravity;                             //!< gravity vector
	float            nodSpeed;                            //!< vertical camera shake speed after landings
	int              bSwimming;                           //!< whether entity is swimming (is not bound to ground plane)
	float            mass;                                //!< mass (in kg)
	int              surface_idx;                         //!< surface identifier for collisions
	float            minSlideAngle;                       //!< if surface slope is more than this angle, player starts sliding (angle is in radians)
	float            maxClimbAngle;                       //!< player cannot climb surface which slope is steeper than this angle
	float            maxJumpAngle;                        //!< player is not allowed to jump towards ground if this angle is exceeded
	float            minFallAngle;                        //!< player starts falling when slope is steeper than this
	float            maxVelGround;                        //!< player cannot stand of surfaces that are moving faster than this
	float            timeImpulseRecover;                  //!< forcefully turns on inertia for that duration after receiving an impulse
	int              collTypes;                           //!< entity types to check collisions against
	IPhysicalEntity* pLivingEntToIgnore;                  //!< ignore collisions with this *living entity* (doesn't work with other entity types)
	int              bNetwork;                            //!< uses extended history information (obsolete)
	int              bActive;                             //!< 0 disables all simulation for the character, apart from moving along the requested velocity
	int              iRequestedTime;                      //!< requests that the player rolls back to that time and re-executes pending actions during the next step
	int              bReleaseGroundColliderWhenNotActive; //!< when not 0, if the living entity is not active, the ground collider, if any, will be explicitly released during the simulation step.
};

////////// particle entity params

struct pe_params_particle : pe_params
{
	enum entype { type_id = ePE_params_particle };
	pe_params_particle()
	{
		type = type_id;
		MARK_UNUSED mass, size, thickness, wspin, accThrust, kAirResistance, kWaterResistance, velocity, heading, accLift, accThrust, gravity, waterGravity;
		MARK_UNUSED surface_idx, normal, q0, minBounceVel, rollAxis, flags, pColliderToIgnore, iPierceability, areaCheckPeriod, minVel, collTypes, dontPlayHitEffect;
	}

	unsigned int     flags; //!< see entity flags
	float            mass;
	float            size;             //!< pseudo-radius
	float            thickness;        //!< thickness when lying on a surface (if left unused, size will be used)
	Vec3             heading;          //!< direction of movement
	float            velocity;         //!< velocity along "heading"
	float            kAirResistance;   //!< air resistance koefficient, F = kv
	float            kWaterResistance; //!< same for water
	float            accThrust;        //!< acceleration along direction of movement
	float            accLift;          //!< acceleration that lifts particle with the current speed
	int              surface_idx;
	Vec3             wspin;             //!< angular velocity
	Vec3             gravity;           //!< stores this gravity and uses it if the current area's gravity is equal to the global gravity
	Vec3             waterGravity;      //!< gravity when underwater
	Vec3             normal;            //!< aligns this direction with the surface normal when sliding
	Vec3             rollAxis;          //!< aligns this directon with the roll axis when rolling (0,0,0 to disable alignment)
	quaternionf      q0;                //!< initial orientation (zero means x along direction of movement, z up)
	float            minBounceVel;      //!< velocity threshold for bouncing->sliding switch
	float            minVel;            //!< sleep speed threshold
	IPhysicalEntity* pColliderToIgnore; //!< physical entity to ignore during collisions
	int              iPierceability;    //!< pierceability for ray tests; pierceble hits slow the particle down, but don't stop it
	int              collTypes;         //!< 'objtype' passed to RayWorldntersection
	int              areaCheckPeriod;   //!< how often (in frames) world area checks are made
	int              dontPlayHitEffect; //!< prevent playing of material FX from now on

	VALIDATORS_START
	  VALIDATOR(mass)
	VALIDATOR(size)
	VALIDATOR(thickness)
	VALIDATOR_NORM(heading)
	VALIDATOR_NORM(q0)
	VALIDATORS_END
};

////////// vehicle entity params

struct pe_params_car : pe_params
{
	enum entype { type_id = ePE_params_car };
	pe_params_car()
	{
		type = type_id;
		MARK_UNUSED engineMaxRPM, iIntegrationType, axleFriction, enginePower, maxSteer, maxTimeStep, minEnergy, damping, brakeTorque;
		MARK_UNUSED engineMinRPM, engineShiftUpRPM, engineShiftDownRPM, engineIdleRPM, engineStartRPM, clutchSpeed, nGears, gearRatios, kStabilizer;
		MARK_UNUSED slipThreshold, gearDirSwitchRPM, kDynFriction, minBrakingFriction, maxBrakingFriction, steerTrackNeutralTurn, maxGear, minGear, pullTilt;
		MARK_UNUSED maxTilt, bKeepTractionWhenTilted, wheelMassScale;
	}

	float  axleFriction;       //!< friction torque at axes divided by mass of vehicle
	float  enginePower;        //!< power of engine (about 10,000 - 100,000)
	float  maxSteer;           //!< maximum steering angle
	float  engineMaxRPM;       //!< engine torque decreases to 0 after reaching this rotation speed
	float  brakeTorque;        //!< torque applied when breaking using the engine
	int    iIntegrationType;   //!< for suspensions; 0-explicit Euler, 1-implicit Euler
	float  maxTimeStep;        //!< maximum time step when vehicle has only wheel contacts
	float  minEnergy;          //!< minimum awake energy when vehicle has only wheel contacts
	float  damping;            //!< damping when vehicle has only wheel contacts
	float  minBrakingFriction; //!< limits the the tire friction when handbraked
	float  maxBrakingFriction; //!< limits the the tire friction when handbraked
	float  kStabilizer;        //!< stabilizer force, as a multiplier for kStiffness of respective suspensions
	int    nWheels;            //!< the number of wheels
	float  engineMinRPM;       //!< disengages the clutch when falling behind this limit, if braking with the engine
	float  engineShiftUpRPM;   //!< RPM threshold for for automatic gear switching
	float  engineShiftDownRPM;
	float  engineIdleRPM;  //!< RPM for idle state
	float  engineStartRPM; //!< sets this RPM when activating the engine
	float  clutchSpeed;    //!< clutch engagement/disengagement speed
	int    nGears;
	float* gearRatios;              //!< assumes 0-backward gear, 1-neutral, 2 and above - forward
	int    maxGear, minGear;        //!< additional gear index clamping
	float  slipThreshold;           //!< lateral speed threshold for switchig a wheel to a 'slipping' mode
	float  gearDirSwitchRPM;        //!< RPM threshold for switching back and forward gears
	float  kDynFriction;            //!< friction modifier for sliping wheels
	float  steerTrackNeutralTurn;   //!< for tracked vehicles, steering angle that causes equal but opposite forces on tracks
	float  pullTilt;                //!< for tracked vehicles, tilt angle of pulling force towards ground
	float  maxTilt;                 //!< maximum wheel contact normal tilt (left or right) after which it acts as a locked part of the hull; it's a cosine of the angle
	int    bKeepTractionWhenTilted; //!< keeps wheel traction in tilted mode
	float  wheelMassScale;          //!< scales wheels' masses for inertia computations (default 0)
};

struct pe_params_wheel : pe_params
{
	enum entype { type_id = ePE_params_wheel };
	pe_params_wheel()
	{
		type = type_id;
		iWheel = 0;
		MARK_UNUSED bDriving, iAxle, suspLenMax, suspLenInitial, minFriction, maxFriction, surface_idx, bCanBrake, bBlocked,
		            bRayCast, kStiffness, kDamping, kLatFriction, Tscale, w, bCanSteer, kStiffnessWeight;
	}

	int   iWheel;
	int   bDriving;
	int   iAxle;          //!< wheels on the same axle align their coordinates (if only slightly misaligned)
	                      //!< and apply stabilizer force (if set); axle<0 means the wheel does not affect the physics
	int   bCanBrake;      //!< handbrake applies
	int   bBlocked;       //!< locks the wheel (acts like a forced handbrake)
	int   bCanSteer;      //!< can this wheel steer, 0 or 1
	float suspLenMax;     //!< full suspension length (relaxed)
	float suspLenInitial; //!< length in the initial state (used for automatic computations)
	float minFriction;    //!< surface friction is cropped to this min-max range
	float maxFriction;
	int   surface_idx;
	int   bRayCast;         //!< uses raycasts instead of cylinders
	float kStiffness;       //!< if 0, will be computed based on mass distribution, lenMax, and lenInitial
	float kStiffnessWeight; //!< When autocalculating stiffness use this weight for this wheel. Note weights for wheels in front of the centre of mass do not influence the weights of wheels behind the centre of mass
	                        //!< By default all weights are 1.0 and the sum doesn't have to add up to 1.0!
	                        //!< Also a <=0 weight will leave the wheel out of the autocalculation. It will be get a stiffness of weight*mass*gravity/defaultLength/numWheels. weight=-1 is a good starting point for these wheels
	float kDamping;         //!< absolute value if >=0, otherwise -(fraction of 0-oscillation damping)
	float kLatFriction;     //!< lateral friction scale (doesn't apply when on handbrake)
	float Tscale;           //!< optional driving torque scale
	float w;                //!< rotational velocity; it's computed automatically, but can be overriden if needed
};

////////// walking rigid entity params

struct pe_params_walking_rigid : pe_params
{
	enum entype { type_id = ePE_params_walking_rigid };
	pe_params_walking_rigid()
	{
		type = type_id;
		MARK_UNUSED velLegStick, legFriction, legStiffness, legsColltype, minLegTestMass;
	}

	float velLegStick;    // keep leg contact if it's separated by less than velStick*dt per frame
	float legFriction;	  // friction of the leg contact
	float legStiffness;	  // how fast the legs will return to the default length
	int   legsColltype;   // geometry flags the legs look for
	float minLegTestMass; // only test legs collisions against objects with this or higher mass
};

////////// rope entity params

struct pe_params_rope : pe_params
{
	enum entype { type_id = ePE_params_rope };
	pe_params_rope()
	{
		type = type_id;
		MARK_UNUSED length, mass, collDist, surface_idx, friction, nSegments, pPoints.data, pVelocities.data;
		MARK_UNUSED pEntTiedTo[0], ptTiedTo[0], idPartTiedTo[0], pEntTiedTo[1], ptTiedTo[1], idPartTiedTo[1], stiffnessAnim, maxForce,
		            flagsCollider, nMaxSubVtx, stiffnessDecayAnim, dampingAnim, bTargetPoseActive, wind, windVariance, airResistance, waterResistance, density, collTypes,
		            jointLimit, jointLimitDecay, sensorRadius, frictionPull, stiffness, collisionBBox[0], penaltyScale, maxIters, attachmentZone, minSegLen, unprojLimit, noCollDist, hingeAxis;
		bLocalPtTied = 0;
	}

	float length; //!< 'target' length; 0 is allowed for ropes with dynamic subdivision
	float mass;
	float collDist;                         //!< thickness for collisions
	int   surface_idx;                      //!< for collision reports; friction is overriden
	float friction;                         //!< friction for free state and lateral friction in strained state
	float frictionPull;                     //!< friction in pull direction in strained state
	float stiffness;                        //!< stiffness against stretching (used in the solver; it's *not* a spring, though)
	float stiffnessAnim;                    //!< shape-preservation stiffness
	float stiffnessDecayAnim;               //!< the final shape stiffness will be interpolated from full to full*(1-decay) at the end
	float dampingAnim;                      //!< damping for shape preservation forces
	int   bTargetPoseActive;                //!< 0-no target pose (no shape-preservation stiffness),
	                                        //!< 1-simplified target pose (vertices are pulled directly to targets)
	                                        //!< 2-physically correct target pose (the rope applies penalty torques at joints)
	Vec3                  wind;             //!< local wind in addition to one from phys areas
	float                 windVariance;     //!< wariance (applied to local only)
	float                 airResistance;    //!< needs to be >0 in order to be affetcted by the wind
	float                 waterResistance;  //!< medium resistance when underwater
	float                 density;          //!< used only to compute buoyancy
	float                 jointLimit;       //!< joint rotation limit (doesn't work when both ends are tied)
	float                 jointLimitDecay;  //!< joint limit change (0..1) towards the unattached rope end; can be positive or negative
	float                 sensorRadius;     //!< size of the sensor used to re-attach the rope if the host entity breaks
	float                 maxForce;         //!< force limit; when breached, the rope will detach itself unless rope_no_tears is set
	float                 penaltyScale;     //!< for the solver in strained state with subdivision on
	float                 attachmentZone;   //!< don't register solver contacts within this distance around attachment points (subdivision mode)
	float                 minSegLen;        //!< delete segments below this length in subdivision mode
	float                 unprojLimit;      //!< rotational unprojection limit per frame (no-subdivision mode)
	float                 noCollDist;       //!< fraction of the segment near the attachment point that doesn't collide (no-subdivision mode)
	int                   maxIters;         //!< tweak for the internal vertex solver
	int                   nSegments;        //!< segment count, changin will reset vertex positions
	int                   flagsCollider;    //!< only collide with entity parts flagged this way
	int                   collTypes;        //!< a selection of ent_xxx flags to collide against
	int                   nMaxSubVtx;       //!< maximum internal vertices per segment in subdivision mode
	Vec3                  collisionBBox[2]; //!< bbox for entity proximity query in host's space
	                                        //! (used make all ropes belonging to one host share the same box, to automatically reuse the query results)
	Vec3                  hingeAxis;        //!< only allow rotation around this axis (in parent's frame if rope_target_vtx_rel is set)
	strided_pointer<Vec3> pPoints;
	strided_pointer<Vec3> pVelocities;

	IPhysicalEntity*      pEntTiedTo[2];
	int                   bLocalPtTied; //!< ptTiedTo is in tied part's local coordinates
	Vec3                  ptTiedTo[2];
	int                   idPartTiedTo[2];
};

////////// soft entity params

struct pe_params_softbody : pe_params
{
	enum entype { type_id = ePE_params_softbody };
	pe_params_softbody()
	{
		type = type_id;
		MARK_UNUSED thickness, maxSafeStep, ks, kdRatio, airResistance, wind, windVariance, nMaxIters,
		            accuracy, friction, impulseScale, explosionScale, collisionImpulseScale, maxCollisionImpulse, collTypes, waterResistance, massDecay,
		            shapeStiffnessNorm, shapeStiffnessTang, stiffnessAnim, stiffnessDecayAnim, dampingAnim, maxDistAnim, hostSpaceSim;
	}

	float thickness;   //!< thickness for collisions
	float maxSafeStep; //!< time step cap
	float ks;          //!< stiffness against stretching (for soft bodies, <0 means fraction of maximum stable)
	float kdRatio;     //!< damping in stretch direction, in fractions of 0-oscillation damping
	float friction;    //!< overrides material friction
	float waterResistance;
	float airResistance;
	Vec3  wind;                  //!< wind in addition to phys area wind
	float windVariance;          //!< wind variance, in fractions of 1 (currently changes 4 times/sec)
	int   nMaxIters;             //!< tweak for the solver (complexity = O(nMaxIters*numVertices))
	float accuracy;              //!< accuracy for the solver (velocity)
	float impulseScale;          //!< scale general incoming impulses
	float explosionScale;        //!< scale impulses from explosions
	float collisionImpulseScale; //!< not used
	float maxCollisionImpulse;   //!< not used
	int   collTypes;             //!< combination of ent_... flags
	float massDecay;             //!< decreases mass from attached points to free ends; mass_free = mass_attached/(1+decay) (can impove stability)
	float shapeStiffnessNorm;    //!< resistance to bending
	float shapeStiffnessTang;    //!< resistance to shearing
	float stiffnessAnim;         //!< strength of linear target pose pull
	float stiffnessDecayAnim;    //!< decay of stiffnessAnim
	float dampingAnim;           //!< damping for target pose pull
	float maxDistAnim;           //!< max deviation from the target pose at the rim; uses stiffnessDecalAnim to scale down closer to attached vtx
	float hostSpaceSim;          //!< 0 - world-space simulation, 1 - fully host-space simulation
};

/////////// area params

struct params_wavesim
{
	params_wavesim() { MARK_UNUSED timeStep, waveSpeed, dampingCenter, dampingRim, minhSpread, minVel, simDepth, heightLimit, resistance; }
	float timeStep;      //!< fixed timestep used for the simulation
	float waveSpeed;     //!< wave propagation speed
	float simDepth;      //!< assumed height of moving water layer	(relative to cell size)
	float heightLimit;   //!< hard limit on height changes (relative to cell size)
	float resistance;    //!< rate of velocity transfer from floating objects
	float dampingCenter; //!< damping in the central tile
	float dampingRim;    //!< damping in the outer tiles
	float minhSpread;    //!< minimum height perturbation that activates a neighbouring tile
	float minVel;        //!< sleep speed threshold
};

struct pe_params_area : pe_params
{
	enum entype { type_id = ePE_params_area };
	pe_params_area() { type = type_id; MARK_UNUSED gravity, bUniform, damping, falloff0, bUseCallback, pGeom, volume, borderPad, bConvexBorder, objectVolumeThreshold, cellSize, growthReserve, volumeAccuracy; }
	// water/air area params are set through pe_params_buoyancy

	Vec3           gravity;               //!< see also bUniform
	float          falloff0;              //!< parametric distance (0..1) where falloff starts
	int            bUniform;              //!< gravity has same direction in every point or always points to the center
	int            bUseCallback;          //!< will generate immediate EventPhysArea when needs to apply params to an entity
	float          damping;               //!< uniform damping
	IGeometry*     pGeom;                 //!< phys geometry used in the area
	float          volume;                //!< the area will try to maintain this volume by adjusting water level
	float          volumeAccuracy;        //!< accuracy of level computation based on volume (in fractions of the volume)
	float          borderPad;             //!< after adjusting level, expand the border by this distance
	int            bConvexBorder;         //!< forces convex border after water level adjustments
	float          objectVolumeThreshold; //!< only consider entities larger than this for level adjustment (set in fractions of area volume)
	float          cellSize;              //!< cell size for wave simulation
	params_wavesim waveSim;
	float          growthReserve; //!< assume this area increase during level adjustment (only used for wave simulation)
};

////////// water manager params

struct pe_params_waterman : pe_params, params_wavesim
{
	enum entype { type_id = ePE_params_waterman };
	pe_params_waterman()
	{
		type = type_id;
		MARK_UNUSED posViewer, nExtraTiles, nCells, tileSize, timeStep, waveSpeed,
		            dampingCenter, dampingRim, minhSpread, minVel, simDepth, heightLimit, resistance;
	}

	Vec3  posViewer;   //!< water will only be simulated around this point
	int   nExtraTiles; //!< number of additional tiles in each direction around the one below posViewer (so total = (nExtarTiles*2+1)^2)
	int   nCells;      //!< number of cells in each tile
	float tileSize;
};

////////////////////////// Action structures /////////////////////

////////// common actions
struct pe_action
{
	int type;
};

//! Used to apply an impulse on a physical entity
//! \par Example
//! \include CryPhysics/Examples/ActionImpulse.cpp
struct pe_action_impulse : pe_action
{
	enum entype { type_id = ePE_action_impulse };
	pe_action_impulse() { type = type_id; impulse.Set(0, 0, 0); MARK_UNUSED point, angImpulse, partid, ipart, pGridRefEnt; iApplyTime = 2; iSource = 0; }

	Vec3 impulse;
	Vec3 angImpulse; //!< optional
	Vec3 point;      //!< point of application, in world CS, optional
	int  partid;     //!< receiver part identifier
	int  ipart;      //!< alternatively, part index can be used
	int  iApplyTime; //!< 0-apply immediately, 1-apply before the next time step, 2-apply after the next time step
	int  iSource;    //!< reserved for internal use

	const IPhysicalEntity* pGridRefEnt; //!< source grid of the impulse (set via a reference entity)

	VALIDATORS_START
	  VALIDATOR_RANGE2(impulse, 0, 1E12f)
	VALIDATOR_RANGE2(angImpulse, 0, 1E8f)
	VALIDATOR_RANGE2(point, 0, 1E6f)
	VALIDATOR_RANGE(ipart, 0, 10000)
	VALIDATORS_END
};

//!< Resets dynamic state of an entity.
struct pe_action_reset : pe_action
{
	enum entype { type_id = ePE_action_reset };
	pe_action_reset() { type = type_id; bClearContacts = 1; }
	int bClearContacts;
};

//! \see pe_action_add_constraint.
enum constrflags
{
	local_frames              = 1,      //!< pt and qframe are in respective entities' coordinate frames
	world_frames              = 2,      //!< pt and qframe are in the world frame
	local_frames_part         = 4,      //!< pt and qframe are if respective entity parts' coordinate frames
	constraint_inactive       = 0x100,  //!< constraint does nothing, except applying _ignore_buddy if set
	constraint_ignore_buddy   = 0x200,  //!< disables collisions between the constrained entities
	constraint_line           = 0x400,  //!< position if constrained to a line
	constraint_plane          = 0x800,  //!< position is constrained to a plane
	constraint_free_position  = 0x1000, //!< position is unconstrained
	constraint_no_rotation    = 0x2000, //!< relative rotation is fully constrained
	constraint_no_enforcement = 0x4000, //!< disables positional enforcement during fast movement (currently disabled unconditionally)
	constraint_no_tears       = 0x8000  //!< constraint is not deleted when force limit is reached
};

//! Adds a physical constraint on an entity
//! \par Example
//! \include CryPhysics/Examples/AddConstraint.cpp
struct pe_action_add_constraint : pe_action
{
	enum entype { type_id = ePE_action_add_constraint };
	pe_action_add_constraint()
	{
		type = type_id;
		pBuddy = 0;
		flags = world_frames;
		MARK_UNUSED id, pt[0], pt[1], partid[0], partid[1], qframe[0], qframe[1], xlimits[0], yzlimits[0],
		            pConstraintEntity, damping, sensorRadius, maxPullForce, maxBendTorque, hardnessLin, hardnessAng;
	}
	int              id;                          //!< if not set, will be auto-assigned; return value of Action()
	IPhysicalEntity* pBuddy;                      //!< the second constrained entity; can be WORLD_ENTITY for static attachments
	Vec3             pt[2];                       //!< pt[0] must be set; if pt[1] is not set, assumed to be equal to pt[1]
	int              partid[2];                   //!< if not set, the first part is assumed
	quaternionf      qframe[2];                   //!< constraint frames for constraint angles computation; if not set, identity in the specified frame is assumed
	float            xlimits[2];                  //!< rotation limits around x axis ("twist"); if xlimits[0]>=[1], x axis is locked
	float            yzlimits[2];                 //!< combined yz-rotation - "bending" of x axis; yzlimits[0] is ignored and assumed to be 0 during simulation
	unsigned int     flags;                       //!< see enum constrflags
	float            damping;                     //!< internal constraint damping
	float            sensorRadius;                //!< used for sampling environment and re-attaching the constraint when something breaks
	float            maxPullForce, maxBendTorque; //!< positional and rotational force limits
	float            hardnessLin, hardnessAng;    //!< sets how fast the solver tries to resolve positional and rotational drift
	IPhysicalEntity* pConstraintEntity;           //!< used internally for creating dynamic rope constraints
};

struct pe_action_update_constraint : pe_action
{
	enum entype { type_id = ePE_action_update_constraint };
	pe_action_update_constraint()
	{
		type = type_id;
		MARK_UNUSED idConstraint, pt[0], pt[1], qframe[0], qframe[1], maxPullForce, maxBendTorque, hardnessLin, hardnessAng, damping;
		flagsOR = 0;
		flagsAND = (unsigned int)-1;
		bRemove = 0;
		flags = world_frames;
	}
	int          idConstraint; //!< doesn't have to be unique - can update several constraints with one id; if not set, updates all constraints
	unsigned int flagsOR;
	unsigned int flagsAND;
	int          bRemove;   //!< permanently delete the constraint
	Vec3         pt[2];     //!< local_frames_part is not supported currently
	quaternionf  qframe[2]; //!< update to constraint frames
	float        maxPullForce, maxBendTorque;
	float        hardnessLin, hardnessAng;
	float        damping;
	int          flags; //!< generally it's better to use flagsOR and/or flagsAND
};

struct pe_action_register_coll_event : pe_action
{
	//! this can be used to ask an entity to post a fake collision event to the log
	enum entype { type_id = ePE_action_register_coll_event };
	pe_action_register_coll_event() { type = type_id; MARK_UNUSED vSelf; }

	Vec3             pt;        //!< collision point
	Vec3             n;         //!< collision normal
	Vec3             v;         //!< collider's velocity at pt
	Vec3             vSelf;     //!< optional override for the current entity's velocity at pt
	float            collMass;  //!< collider's mass
	IPhysicalEntity* pCollider; //!< collider entity
	int              partid[2];
	int              idmat[2];
	short            iPrim[2];
};

struct pe_action_awake : pe_action
{
	enum entype { type_id = ePE_action_awake };
	pe_action_awake() { type = type_id; bAwake = 1; MARK_UNUSED minAwakeTime; }
	int   bAwake;
	float minAwakeTime; //!< minimum time to stay awake after executing the action; supported only by some entity types
};

struct pe_action_remove_all_parts : pe_action
{
	enum entype { type_id = ePE_action_remove_all_parts };
	pe_action_remove_all_parts() { type = type_id; }
};

struct pe_action_reset_part_mtx : pe_action
{
	//! this will bake the part's matrix into the entity's matrix and clear the former
	enum entype { type_id = ePE_action_reset_part_mtx };
	pe_action_reset_part_mtx() { type = type_id; MARK_UNUSED ipart, partid; }
	int ipart;
	int partid;
};

struct pe_action_set_velocity : pe_action
{
	enum entype { type_id = ePE_action_set_velocity };
	pe_action_set_velocity() { type = type_id; MARK_UNUSED ipart, partid, v, w; bRotationAroundPivot = 0; }
	int  ipart; //!< if part is not set, vel is applied to the whole entity; the distinction makes sense only for ragdolls
	int  partid;
	Vec3 v, w;
	int  bRotationAroundPivot; //!< if set, w is rotation around the entity's pivot, otherwise around its center of mass

	VALIDATORS_START
	  VALIDATOR_RANGE2(v, 0, 1E5f)
	VALIDATOR_RANGE2(w, 0, 1E5f)
	VALIDATORS_END
};

struct pe_action_notify : pe_action
{
	enum entype { type_id = ePE_action_notify };
	enum encodes { ParentChange = 0 }; //!< the entity this one is attached to moved; only ropes handle it ATM, by immediately enforcing length
	pe_action_notify() { type = type_id; iCode = ParentChange; }
	int iCode;
};

struct pe_action_auto_part_detachment : pe_action
{
	//! this is used to PE_ARTICULATED entities with pre-baked physics simulation
	enum entype { type_id = ePE_action_auto_part_detachment };
	pe_action_auto_part_detachment() { type = type_id; MARK_UNUSED threshold, autoDetachmentDist; }
	float threshold;          //!< each part will receive a breakable joint with this force limit (which is assumed to be set in fractions of gravity)
	float autoDetachmentDist; //!< additionally, a part will auto-detach itself once it's farther than that from the entity's pivot
};

struct pe_action_move_parts : pe_action
{
	//! this will move parts from one entity to another
	enum entype { type_id = ePE_action_move_parts };
	int              idStart, idEnd;
	int              idOffset; //!< added once parts are in the new entity
	IPhysicalEntity* pTarget;
	Matrix34         mtxRel;                              //!< mtxInNewEntity = mtxRel*mtxCurrent
	int              (* MapPartId)(int id, int* auxData); //!< optional mapping callback; return <0 to skip the part
	int*             auxData;
	int              szAuxData;
	pe_action_move_parts() { type = type_id; idStart = 0; idEnd = ~(1 << 31); idOffset = 0; mtxRel.SetIdentity(); pTarget = 0; MapPartId = 0; auxData = 0; szAuxData = 0; }
};

struct pe_action_batch_parts_update : pe_action
{
	//! updates positions of all parts from arrays
	enum entype { type_id = ePE_action_batch_parts_update };
	pe_action_batch_parts_update() { type = type_id; qOffs.SetIdentity(); posOffs.zero(); numParts = 0; pnumParts = 0; pIds = 0; pValidator = 0; }
	int*                         pIds;
	strided_pointer<quaternionf> qParts;
	strided_pointer<Vec3>        posParts;
	int                          numParts;
	int*                         pnumParts;
	quaternionf                  qOffs;   //!< extra rotation
	Vec3                         posOffs; //!< extra offset
	struct Validator
	{
		virtual ~Validator() {}
		virtual bool Lock() = 0;
		virtual void Unlock() = 0;
	}* pValidator;
};

struct pe_action_slice : pe_action
{
	//! slices entity's geometry with a shape
	enum entype { type_id = ePE_action_slice };
	pe_action_slice() { type = type_id; MARK_UNUSED ipart, partid; npt = 3; }
	int   ipart;
	int   partid;
	Vec3* pt;
	int   npt; //!< only 3 is supported currently
};

////////// living entity actions

//! Movement request for living entities.
//! \par Example
//! \include CryEntitySystem/Examples/PhysicalizeLiving.cpp
struct pe_action_move : pe_action
{
	enum entype { type_id = ePE_action_move };
	pe_action_move() { type = type_id; iJump = 0; dt = 0; MARK_UNUSED dir; }

	Vec3  dir;   //!< requested velocity vector
	int   iJump; //!< jump mode - 1-instant velocity change, 2-just adds to current velocity
	float dt;    //!< time interval for this action (doesn't need to be set normally)

	VALIDATORS_START
	  VALIDATOR_RANGE2(dir, 0, 1000)
	VALIDATOR_RANGE(dt, 0, 2)
	VALIDATORS_END
};

////////// vehicle entity actions

struct pe_action_drive : pe_action
{
	enum entype { type_id = ePE_action_drive };
	pe_action_drive() { type = type_id; MARK_UNUSED pedal, dpedal, steer, dsteer, bHandBrake, clutch, iGear; ackermanOffset = 0.f; }

	float pedal;          //!< engine pedal absolute value; active pedal always awakes the entity
	float dpedal;         //!< engine pedal delta
	float steer;          //!< steering angle absolute value
	float ackermanOffset; //!< apply ackerman steering, 0.0 -> normal driving front wheels steer, back fixed. 1.0 -> front fixed, back steer. 0.5 -> both front and back steer
	float dsteer;         //!< steering angle delta
	float clutch;         //!< forces clutch; 0..1
	int   bHandBrake;     //!< removing handbrake will automatically awaken the vehicle if it's sleeping
	int   iGear;          //!< 0-back; 1-neutral; 2+-forward
};

////////// rope entity actions

struct pe_action_target_vtx : pe_action
{
	enum entype { type_id = ePE_action_target_vtx };
	pe_action_target_vtx() { type = type_id; MARK_UNUSED points, nPoints; posHost.zero(); qHost.SetIdentity(); }

	int   nPoints;
	Vec3* points;  //!< coordinate frame is world, unless the rope has one of rope_target_vtx_rel flags
	Vec3  posHost; //!< world position of the host the vertices are attached to
	Quat  qHost;
};

////////// soft entity actions

struct pe_action_attach_points : pe_action
{
	enum entype { type_id = ePE_action_attach_points };
	pe_action_attach_points() { type = type_id; MARK_UNUSED partid, points, piVtx; nPoints = 1; pEntity = WORLD_ENTITY; bLocal = 0; }

	IPhysicalEntity* pEntity;
	int              partid;
	int*             piVtx;  //!< vertex indices to be attached to pEntity.partid
	Vec3*            points; //!< can set the desired attachment points; if not set, current positions are fixed in the target's frame
	int              nPoints;
	int              bLocal; //!< if true, points are in the attached part's CS
};

////////////////////////// Status structures /////////////////////

////////// common statuses
struct pe_status
{
	int type;
};

enum status_pos_flags { status_local = 1, status_thread_safe = 2, status_addref_geoms = 4 };

//! Gets the world-space position of an entity
//! \par Example
//! \include CryPhysics/Examples/GetPhysicalEntityPosition.cpp
struct pe_status_pos : pe_status
{
	enum entype { type_id = ePE_status_pos };
	pe_status_pos() { type = type_id; ipart = partid = -1; flags = 0; pMtx3x4 = 0; pMtx3x3 = 0; iSimClass = 0; timeBack = 0; pGridRefEnt = nullptr; }

	int          partid;   //!< part identifier, -1 for entire entity
	int          ipart;    //!< optionally, part slot index
	unsigned int flags;    //!< status_local if part coordinates should be returned in entity CS rather than world CS
	unsigned int flagsOR;  //!< boolean OR for all parts flags of the object (or just flags for the selected part)
	unsigned int flagsAND; //!< boolean AND for all parts flags of the object (or just flags for the selected part)
	Vec3         pos;      //!< position of center
	Vec3         BBox[2];  //!< bounding box relative to pos (bbox[0]-min, bbox[1]-max)
	quaternionf  q;
	float        scale;
	int          iSimClass;
	Matrix34*    pMtx3x4; //!< optional 3x4 transformation matrix
	Matrix33*    pMtx3x3; //!< optional 3x3 rotation+scale matrix
	IGeometry*   pGeom, * pGeomProxy;
	float        timeBack; //!< can retrieve previous position; only supported by rigid entities; pos and q; one step back

	const IPhysicalEntity* pGridRefEnt;	//!< Target grid (set via a reference entity)
};

//! Only works when USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION is 1.
struct pe_status_netpos : pe_status
{
	enum entype { type_id = ePE_status_netpos };
	pe_status_netpos() { type = type_id; }

	Vec3        pos;
	quaternionf rot;
	Vec3        vel;
	Vec3        angvel;
	float       timeOffset;
};

//! Caches eForm extent of entity in pGeo.
struct pe_status_extent : pe_status
{
	enum entype { type_id = ePE_status_extent };
	pe_status_extent() { type = type_id; eForm = EGeomForm(-1); extent = 0; }

	EGeomForm eForm;
	float     extent;
};

//! Generates random pos on entity, also caches extent.
struct pe_status_random : pe_status_extent
{
	enum entype { type_id = ePE_status_random };
	pe_status_random() { type = type_id; }
	Array<PosNorm> points;
	CRndGen seed;
};

//! Requests status of attached to the entity sensors.
struct pe_status_sensors : pe_status
{
	enum entype { type_id = ePE_status_sensors };
	pe_status_sensors() { type = type_id; }

	Vec3*        pPoints;  //!< pointer to array of points where sensors touch environment (assigned by physical entity)
	Vec3*        pNormals; //!< pointer to array of surface normals at points where sensors touch environment
	unsigned int flags;    //!< bitmask of flags, bit==1 - sensor touched environment
};

//! Gets the dynamic state of an entity, such as its velocity, mass and angular velocity
//! \par Example
//! \include CryPhysics/Examples/GetDynamicsStatus.cpp
struct pe_status_dynamics : pe_status
{
	enum entype { type_id = ePE_status_dynamics };
	pe_status_dynamics() : v(ZERO), w(ZERO), a(ZERO), wa(ZERO), centerOfMass(ZERO)
	{
		MARK_UNUSED partid, ipart;
		type = type_id;
		mass = energy = 0;
		nContacts = 0;
		time_interval = 0;
		submergedFraction = 0;
	}

	int   partid;
	int   ipart;
	Vec3  v;  //!< velocity
	Vec3  w;  //!< angular velocity
	Vec3  a;  //!< linear acceleration
	Vec3  wa; //!< angular acceleration
	Vec3  centerOfMass;
	float submergedFraction; //!< percentage of the entity that is underwater; 0..1. not supported for individual parts
	float mass;              //!< entity's or part's mass
	float energy;            //!< kinetic energy; only supported by PE_ARTICULATED currently
	int   nContacts;
	float time_interval; //!< not used
};

struct coll_history_item
{
	Vec3  pt;         //!< collision area center
	Vec3  n;          //!< collision normal in entity CS
	Vec3  v[2];       //!< velocities of contacting bodies at the point of impact
	float mass[2];    //!< masses of contacting bodies
	float age;        //!< age of collision event
	int   idCollider; //!< id of collider (not a pointer, since collider can be destroyed before history item is queried)
	int   partid[2];
	int   idmat[2]; //!< 0-this body material, 1-collider material
};

struct pe_status_collisions : pe_status
{
	//! obsolete, replaced with EventPhysCollision events
	enum entype { type_id = ePE_status_collisions };
	pe_status_collisions() { type = type_id; age = 0; len = 1; pHistory = 0; bClearHistory = 0; }

	coll_history_item* pHistory; //!< pointer to a user-provided array of history items
	int                len;      //!< length of this array
	float              age;      //!< maximum age of collision events (older events are ignored)
	int                bClearHistory;
};

struct pe_status_id : pe_status
{
	//! retrives surface id from geometry
	enum entype { type_id = ePE_status_id };
	pe_status_id() { type = type_id; ipart = partid = -1; bUseProxy = 1; }

	int ipart;
	int partid;
	int iPrim;     //!< primitive index (only makes sense for meshes)
	int iFeature;  //!< feature id inside the primitive; doesn't affect the result currently
	int bUseProxy; //!< use pPhysGeomProxy or pPhysGeom
	int id;        //!< surface id
};

struct pe_status_timeslices : pe_status
{
	//! only implemented for PE_LIVING and is obsolete, although still supported
	enum entype { type_id = ePE_status_timeslices };
	pe_status_timeslices() { type = type_id; pTimeSlices = 0; sz = 1; precision = 0.0001f; MARK_UNUSED time_interval; }

	float* pTimeSlices;
	int    sz;
	float  precision;     //!< time surplus below this threshhold will be discarded
	float  time_interval; //!< if unused, time elapsed since the last action will be used
};

//! GetStuts will return the number of parts.
struct pe_status_nparts : pe_status
{
	enum entype { type_id = ePE_status_nparts };
	pe_status_nparts() { type = type_id; }
};

struct pe_status_awake : pe_status
{
	enum entype { type_id = ePE_status_awake };
	pe_status_awake() { type = type_id; lag = 0; }
	int lag; //!< GetStatus returns 1 ("awake") if the entity fell asleep later than this amount of frames before
};

struct pe_status_contains_point : pe_status
{
	enum entype { type_id = ePE_status_contains_point };
	pe_status_contains_point() { type = type_id; }
	Vec3 pt;
};

struct pe_status_placeholder : pe_status
{
	enum entype { type_id = ePE_status_placeholder };
	pe_status_placeholder() { type = type_id; }
	IPhysicalEntity* pFullEntity; //!< if called on a placeholder, returns the corresponding full entity
};

struct pe_status_sample_contact_area : pe_status
{
	enum entype { type_id = ePE_status_sample_contact_area };
	pe_status_sample_contact_area() { type = type_id; }
	Vec3 ptTest; //!< checks if ptTest, projected along dirTest falls inside the convex hull of this entity's contacts
	Vec3 dirTest;
};

struct pe_status_caps : pe_status
{
	enum entype { type_id = ePE_status_caps };
	pe_status_caps() { type = type_id; }
	unsigned int bCanAlterOrientation; //!< the entity can change orientation that is explicitly set from outside (by baking it into parts/geometries)
};

struct pe_status_constraint : pe_status
{
	enum entype { type_id = ePE_status_constraint };
	pe_status_constraint() { type = type_id; idx = -1; }
	int              id;
	int              idx;
	int              flags;
	Vec3             pt[2];
	Vec3             n;
	IPhysicalEntity* pBuddyEntity;
	IPhysicalEntity* pConstraintEntity;
};

////////// area status

struct pe_status_area : pe_status
{
	enum entype { type_id = ePE_status_area };
	pe_status_area() { type = type_id; bUniformOnly = false; ctr.zero(); size.zero(); vel.zero(); MARK_UNUSED gravity, ptClosest; pLockUpdate = 0; pSurface = 0; }

	// inputs.
	Vec3 ctr, size;             //!< query bounds
	Vec3 vel;
	bool bUniformOnly;

	// outputs.
	Vec3               gravity;
	pe_params_buoyancy pb;
	volatile int*      pLockUpdate;
	IGeometry*         pSurface;

	// alternative query: closest point
	Vec3               ptClosest;	 // input-output
	Vec3               dirClosest; // output
};

////////// living entity statuses

struct pe_status_living : pe_status
{
	enum entype { type_id = ePE_status_living };
	pe_status_living() { type = type_id; }

	int              bFlying;          //!< whether entity has no contact with ground
	float            timeFlying;       //!< for how long the entity was flying
	Vec3             camOffset;        //!< camera offset
	Vec3             vel;              //!< actual velocity (as rate of position change)
	Vec3             velUnconstrained; //!< 'physical' movement velocity
	Vec3             velRequested;     //!< velocity requested in the last action
	Vec3             velGround;        //!< velocity of the object entity is standing on
	float            groundHeight;     //!< position where the last contact with the ground occured
	Vec3             groundSlope;
	int              groundSurfaceIdx;
	int              groundSurfaceIdxAux; //!< contact with the ground that also has default collision flags
	IPhysicalEntity* pGroundCollider;     //!< only returns an actual entity if the ground collider is not static
	int              iGroundColliderPart;
	float            timeSinceStanceChange;
	//! int bOnStairs; //! tries to detect repeated abrupt ground height changes
	int              bStuck;    //!< tries to detect cases when the entity cannot move as before because of collisions
	volatile int*    pLockStep; //!< internal timestepping lock
	int              iCurTime;  //!< quantised time
	int              bSquashed; //!< entity is being pushed by heavy objects from opposite directions
};

struct pe_status_check_stance : pe_status
{
	//! checks whether new dimensions cause collisions; params have the same meaning as in pe_player_dimensions
	enum entype { type_id = ePE_status_check_stance };
	pe_status_check_stance() : dirUnproj(0, 0, 1), unproj(0) { type = type_id; MARK_UNUSED pos, q, sizeCollider, heightCollider, bUseCapsule; }

	Vec3        pos;
	quaternionf q;
	Vec3        sizeCollider;
	float       heightCollider;
	Vec3        dirUnproj;
	float       unproj;
	int         bUseCapsule;
};

////////// vehicle entity statuses

struct pe_status_vehicle : pe_status
{
	enum entype { type_id = ePE_status_vehicle };
	pe_status_vehicle() { type = type_id; }

	float steer;      //!< current steering angle
	float pedal;      //!< current engine pedal
	int   bHandBrake; //!< nonzero if handbrake is on
	float footbrake;  //!< nonzero if footbrake is pressed (range 0..1)
	Vec3  vel;
	int   bWheelContact; //!< nonzero if at least one wheel touches ground
	int   iCurGear;
	float engineRPM;
	float clutch;
	float drivingTorque;
	int   nActiveColliders; //!< number of non-static contacting entities
};

struct pe_status_wheel : pe_status
{
	enum entype { type_id = ePE_status_wheel };
	pe_status_wheel() { type = type_id; iWheel = 0; MARK_UNUSED partid; }
	int              iWheel;
	int              partid;

	int              bContact;    //!< nonzero if wheel touches ground
	Vec3             ptContact;   //!< point where wheel touches ground
	Vec3             normContact; //!< contact normal
	float            w;           //!< rotation speed
	int              bSlip;
	Vec3             velSlip; //!< slip velocity
	int              contactSurfaceIdx;
	float            friction;    //!< current friction applied
	float            suspLen;     //!< current suspension spring length
	float            suspLenFull; //!< relaxed suspension spring length
	float            suspLen0;    //!< initial suspension spring length
	float            r;           //!< wheel radius
	float            torque;      //!< driving torque
	float            steer;       //!< current streeing angle
	IPhysicalEntity* pCollider;
};

struct pe_status_vehicle_abilities : pe_status
{
	enum entype { type_id = ePE_status_vehicle_abilities };
	pe_status_vehicle_abilities() { type = type_id; MARK_UNUSED steer; }

	float steer;       //!< should be set to requested steering angle
	Vec3  rotPivot;    //!< returns turning circle center
	float maxVelocity; //!< calculates maximum velocity of forward movement along a plane (steer is ignored)
};

////////// articulated entity statuses

struct pe_status_joint : pe_status
{
	enum entype { type_id = ePE_status_joint };
	pe_status_joint() { type = type_id; MARK_UNUSED partid, idChildBody; }

	int          idChildBody; //!< requested joint is identified by child body id
	int          partid;      //!< ..or, alternatively, by any of parts that belong to it
	unsigned int flags;       //!< joint flags
	Ang3         q;           //!< current joint angles (controlled by physics)
	Ang3         qext;        //!< external angles (from animation)
	Ang3         dq;          //!< current joint angular velocities
	quaternionf  quat0;       //!< orientation of child inside parent that corresponds to 0 angles
};

////////// rope entity statuses

struct pe_status_rope : pe_status
{
	enum entype { type_id = ePE_status_rope };
	pe_status_rope() : pContactEnts(0)
	{
		type = type_id;
		pPoints = pVelocities = pVtx = pContactNorms = 0;
		nCollStat = nCollDyn = bTargetPoseActive = bStrained = 0;
		stiffnessAnim = timeLastActive = 0;
		nVtx = 0;
		lock = 0;
		MARK_UNUSED pGridRefEnt;
	}

	int                               nSegments;
	Vec3*                             pPoints;             //!< expects the caller to provide an array of nSegments+1, if 0, no points are returned
	Vec3*                             pVelocities;         //!< expects the caller to provide an array of nSegments+1, if 0, no velocities are returned
	int                               nCollStat, nCollDyn; //!< number of rope contacts with static and dynamic objects
	int                               bTargetPoseActive;   //!< current traget pose mode (0, 1, or 2)
	float                             stiffnessAnim;       //!< current target pose stiffness
	int                               bStrained;           //!< whether the rope is strained, either along a line or wrapped around objects
	strided_pointer<IPhysicalEntity*> pContactEnts;        //!< returns a pointer to internal data, the caller doesn't need to provide it
	int                               nVtx;                //!< current number of vertices, used for ropes with dynamic subdivision
	Vec3*                             pVtx;                //!< expects the caller to provide the array
	Vec3*                             pContactNorms;       //!< normals for points (not vertices), expects a pointer from the caller
	float                             timeLastActive;      //!< physics time when the rope was last active (not asleep)
	Vec3                              posHost;             //!< host (the entity part it's attached to) position that corresponds to the returned state
	quaternionf                       qHost;               //!< host's orientation
	int                               lock;                //!< for subdivided ropes: +1 to leave read-locked, -1 to release from a previously read-locked state; 0 - local locking only
	const IPhysicalEntity*            pGridRefEnt;	       //!< Target grid (set via a reference entity)
};

////////// soft entity statuses

enum ESSVFlags
{
	eSSV_LockPos   = 1, //!< locks vertices (soft entity won't be able to update them until released)
	eSSV_UnlockPos = 2  //!< release the vertices
};

struct pe_status_softvtx : pe_status
{
	enum entype { type_id = ePE_status_softvtx };
	pe_status_softvtx() : pVtx(0), pNormals(0) { type = type_id; pVtxMap = 0; flags = 0; }

	int                   nVtx;
	strided_pointer<Vec3> pVtx;     //!< a pointer to an internal data; doesn't need to be filled
	strided_pointer<Vec3> pNormals; //!< a pointer to an internal data; doesn't need to be filled
	int*                  pVtxMap;  //!< mapping table mesh vertex->simulated vertex (can be 0)
	IGeometry*            pMesh;    //!< phys mesh that reflects the simulated data
	int                   flags;    //!< see ESSVFlags
	quaternionf           qHost;    //!< host's orientation
	Vec3                  posHost;  //!< host (the entity part it's attached to) position that corresponds to the returned state
	Vec3                  pos;      //!< entity position that corresponds to the returned state
	quaternionf           q;        //!< entity's orientation
};

////////// waterman statuses

struct SWaterTileBase
{
	int    bActive;
	float* ph;   //!< heights
	Vec3*  pvel; //!< velocities
};

struct pe_status_waterman : pe_status
{
	enum entype { type_id = ePE_status_waterman };
	pe_status_waterman() { type = type_id; }

	int              bActive;
	Matrix33         R;
	Vec3             origin;
	int              nTiles, nCells; //!< number of tiles and cells in one dimension
	SWaterTileBase** pTiles;         //!< nTiles^2 entries
};

////////////////////////// Geometry structures /////////////////////

////////// common geometries
enum geom_flags
{
	//! collisions between parts are checked if (part0->flagsCollider & part1->flags) !=0
	geom_colltype0              = 0x0001, geom_colltype1 = 0x0002, geom_colltype2 = 0x0004, geom_colltype3 = 0x0008, geom_colltype4 = 0x0010,
	geom_colltype5              = 0x0020, geom_colltype6 = 0x0040, geom_colltype7 = 0x0080, geom_colltype8 = 0x0100, geom_colltype9 = 0x0200,
	geom_colltype10             = 0x0400, geom_colltype11 = 0x0800, geom_colltype12 = 0x1000, geom_colltype13 = 0x2000, geom_colltype14 = 0x4000,
	geom_colltype_ray           = 0x8000,    //!< special colltype used by raytracing by default
	geom_floats                 = 0x10000,   //!< colltype required to apply buoyancy
	geom_proxy                  = 0x20000,   //!< only used in AddGeometry to specify that this geometry should go to pPhysGeomProxy
	geom_structure_changes      = 0x40000,   //!< part is breaking/deforming
	geom_can_modify             = 0x80000,   //!< geometry is cloned and is used in this entity only
	geom_squashy                = 0x100000,  //!< part has 'soft' collisions (used for tree foliage proxy)
	geom_log_interactions       = 0x200000,  //!< part will post EventPhysBBoxOverlap whenever something happens inside its bbox
	geom_monitor_contacts       = 0x400000,  //!< part needs collision callback from the solver (used internally)
	geom_manually_breakable     = 0x800000,  //!< part is breakable outside the physics
	geom_no_coll_response       = 0x1000000, //!< collisions are detected and reported, but not processed
	geom_mat_substitutor        = 0x2000000, //!< geometry is used to change other collision's material id if the collision point is inside it
	geom_break_approximation    = 0x4000000, //!< applies capsule approximation after breaking (used for tree trunks)
	geom_no_particle_impulse    = 0x8000000, //!< phys particles don't apply impulses to this part; should be used in flagsCollider
	geom_destroyed_on_break     = 0x2000000, //!< should be used in flagsCollider
	geom_allow_id_duplicates    = 0x1000000, //!< doesn't check if id is already used; should be set in flagsCollider
	geom_ignore_BBox            = 0x20000000,//!< doon't include the part in entity world BBox computation
	//! mnemonic group names
	geom_colltype_player        = geom_colltype1, geom_colltype_explosion = geom_colltype2,
	geom_colltype_vehicle       = geom_colltype3, geom_colltype_foliage = geom_colltype4, geom_colltype_debris = geom_colltype5,
	geom_colltype_foliage_proxy = geom_colltype13, geom_colltype_obstruct = geom_colltype14,
	geom_colltype_solid         = 0x0FFF & ~geom_colltype_explosion, geom_collides = 0xFFFF
};

struct pe_geomparams
{
	enum entype { type_id = ePE_geomparams };
	pe_geomparams()
	{
		type = type_id;
		density = mass = 0;
		pos.Set(0, 0, 0);
		q.SetIdentity();
		bRecalcBBox = 1;
		flags = geom_colltype_solid | geom_colltype_ray | geom_floats | geom_colltype_explosion;
		flagsCollider = geom_colltype0;
		pMtx3x4 = 0;
		pMtx3x3 = 0;
		scale = 1.0f;
		pLattice = 0;
		pMatMapping = 0;
		nMats = 0;
		MARK_UNUSED surface_idx, minContactDist, idmatBreakable;
	}

	int           type;
	float         density; //!< 0 if mass is used
	float         mass;    //!< 0 if density is used
	Vec3          pos;     //!< offset from object's geometrical pivot
	quaternionf   q;       //!< orientation relative to object
	float         scale;
	Matrix34*     pMtx3x4;     //!< optional full transform matrix
	Matrix33*     pMtx3x3;     //!< optional 3x3 orintation+scale matrix
	int           surface_idx; //!< surface identifier (used if corresponding CGeometry does not contain materials)
	unsigned int  flags, flagsCollider;
	float         minContactDist; //!< contacts closer then this threshold are merged
	int           idmatBreakable; //!< index for procedural (boolean) breakability
	ITetrLattice* pLattice;       //!< optional tetrahedral lattice (for lattice breaking and soft bodies)
	int*          pMatMapping;    //!< mapping of mat ids from geometry to final mat ids
	int           nMats;
	int           bRecalcBBox; //!< recalculate all bbox info after the part is added

	VALIDATORS_START
	  VALIDATOR_RANGE(density, -1E8, 1E8)
	VALIDATOR_RANGE(mass, -1E8, 1E8)
	VALIDATOR(pos)
	VALIDATOR_NORM_MSG(q, "(perhaps non-uniform scaling was used in the asset?)", pt)
	VALIDATORS_END
};

////////// articulated entity geometries

struct pe_articgeomparams : pe_geomparams
{
	enum entype { type_id = ePE_articgeomparams };
	pe_articgeomparams() { type = type_id; idbody = 0; }
	pe_articgeomparams(pe_geomparams& src)
	{
		type = type_id;
		density = src.density;
		mass = src.mass;
		pos = src.pos;
		q = src.q;
		scale = src.scale;
		surface_idx = src.surface_idx;
		pLattice = src.pLattice;
		pMatMapping = src.pMatMapping;
		nMats = src.nMats;
		pMtx3x4 = src.pMtx3x4;
		pMtx3x3 = src.pMtx3x3;
		flags = src.flags;
		flagsCollider = src.flagsCollider;
		idbody = 0;
		idmatBreakable = src.idmatBreakable;
		bRecalcBBox = src.bRecalcBBox;
		if (!is_unused(src.minContactDist)) minContactDist = src.minContactDist; else MARK_UNUSED minContactDist;
	}
	int idbody; //!< id of the subbody this geometry is attached to, the 1st add geometry specifies frame CS of this subbody
};

////////// vehicle entity geometries

const int NMAXWHEELS = 30;
struct pe_cargeomparams : pe_geomparams
{
	enum entype { type_id = ePE_cargeomparams };
	pe_cargeomparams() : pe_geomparams() { type = type_id; MARK_UNUSED bDriving, minFriction, maxFriction, bRayCast, kLatFriction; bCanBrake = 1; bCanSteer = 1; kStiffnessWeight = 1.f; }
	pe_cargeomparams(pe_geomparams& src)
	{
		type = type_id;
		density = src.density;
		mass = src.mass;
		pos = src.pos;
		q = src.q;
		surface_idx = src.surface_idx;
		idmatBreakable = src.idmatBreakable;
		pLattice = src.pLattice;
		pMatMapping = src.pMatMapping;
		nMats = src.nMats;
		pMtx3x4 = src.pMtx3x4;
		pMtx3x3 = src.pMtx3x3;
		flags = src.flags;
		flagsCollider = src.flagsCollider;
		MARK_UNUSED bDriving, minFriction, maxFriction, bRayCast;
		bCanBrake = 1, bCanSteer = 1;
		kStiffnessWeight = 1.f;
	}
	int   bDriving;                 //!< whether wheel is driving, -1 - geometry os not a wheel
	int   iAxle;                    //!< wheel axle, currently not used
	int   bCanBrake;                //!< whether the wheel is locked during handbrakes
	int   bRayCast;                 //!< whether the wheel use simple raycasting instead of geometry sweep check
	int   bCanSteer;                //!< wheel the wheel can steer
	Vec3  pivot;                    //!< upper suspension point in vehicle CS
	float lenMax;                   //!< relaxed suspension length
	float lenInitial;               //!< current suspension length (assumed to be length in rest state)
	float kStiffness;               //!< suspension stiffness, if 0 - calculate from lenMax, lenInitial, and vehicle mass and geometry
	float kStiffnessWeight;         //!< When autocalculating stiffness use this weight for this wheel. Note weights for wheels in front of the centre of mass do not influence the weights of wheels behind the centre of mass
	float kDamping;                 //!< suspension damping, if <0 - calculate as -kdamping*(approximate zero oscillations damping)
	float minFriction, maxFriction; //!< additional friction limits for tire friction
	float kLatFriction;             //!< coefficient for lateral friction
};

///////////////// tetrahedra lattice params ////////////////////////

struct pe_tetrlattice_params : pe_params
{
	enum entype { type_id = ePE_tetrlattice_params };
	pe_tetrlattice_params()
	{
		type = type_id;
		MARK_UNUSED nMaxCracks, maxForcePush, maxForcePull, maxForceShift, maxTorqueTwist, maxTorqueBend, crackWeaken, density;
	}

	int   nMaxCracks;                                //!< maximum cracks per update
	float maxForcePush, maxForcePull, maxForceShift; //!< linear force limits
	float maxTorqueTwist, maxTorqueBend;             //!< angular force limits
	float crackWeaken;                               //!< weakens faces neighbouring a newly generated crack (0..1)
	float density;                                   //!< also affects all force limits
};

/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// IGeometry Interface ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

//! \cond INTERNAL
struct geom_world_data   //!< geometry orientation for Intersect() requests
{
	geom_world_data()
	{
		v.Set(0, 0, 0);
		w.Set(0, 0, 0);
		offset.Set(0, 0, 0);
		R.SetIdentity();
		centerOfMass.Set(0, 0, 0);
		scale = 1.0f;
		iStartNode = 0;
	}
	Vec3     offset;
	Matrix33 R;
	float    scale;
	Vec3     v, w;         //!< used to give hints about unprojection direction
	Vec3     centerOfMass; //!< w is rotation around this point
	int      iStartNode;   //!< for warm-starting (checks collisions in this node first)
};
//! \endcond

struct intersection_params
{
	intersection_params()
	{
		iUnprojectionMode = 0;
		vrel_min = 1E-6f;
		time_interval = 100.0f;
		maxSurfaceGapAngle = 1.0f * float(g_PI / 180);
		pGlobalContacts = 0;
		minAxisDist = 0;
		bSweepTest = false;
		centerOfRotation.Set(0, 0, 0);
		axisContactNormal.Set(0, 0, 1);
		unprojectionPlaneNormal.Set(0, 0, 0);
		axisOfRotation.Set(0, 0, 0);
		bKeepPrevContacts = false;
		bStopAtFirstTri = false;
		ptOutsidePivot[0].Set(1E11f, 1E11f, 1E11f);
		ptOutsidePivot[1].Set(1E11f, 1E11f, 1E11f);
		maxUnproj = 1E10f;
		bNoAreaContacts = false;
		bNoBorder = false;
		bNoIntersection = 0;
		bExactBorder = 0;
		bThreadSafe = bThreadSafeMesh = 0;
	}
	int           iUnprojectionMode;       //!< 0-angular, 1-rotational
	Vec3          centerOfRotation;        //!< for mode 1 only
	Vec3          axisOfRotation;          //!< if left 0, will be set based on collision area normal
	float         time_interval;           //!< used to set unprojection limits
	float         vrel_min;                //!< if local relative velocity in contact area is above this, unprojects along its derection; otherwise along area normal
	float         maxSurfaceGapAngle;      //!< theshold for generating area contacts
	float         minAxisDist;             //!< disables rotational unprojection if contact point is closer than this to the axis
	Vec3          unprojectionPlaneNormal; //!< restrict linear unprojection to this plane
	Vec3          axisContactNormal;       //!< a mild hint about possible contact normal
	float         maxUnproj;               //!< unprojections longer than this are discarded
	Vec3          ptOutsidePivot[2];       //!< discard contacts that are not facing outward wrt this point
	bool          bSweepTest;              //!< requests a linear sweep test along v*time_interval (v from geom_world_data)
	bool          bKeepPrevContacts;       //!< append results to existing contact buffer
	bool          bStopAtFirstTri;         //!< stop after the first collision is detected
	bool          bNoAreaContacts;         //!< don't try to detect contact areas
	bool          bNoBorder;               //!< don't trace contact border
	int           bExactBorder;            //!< always tries to return a consequtive border (useful for boolean ops)
	int           bNoIntersection;         //!< don't find all intersection points (only applies to primitive-primitive collisions)
	int           bBothConvex;             //!< (output) both operands were convex
	int           bThreadSafe;             //!< set if it's known that no other thread will contend for the internal intersection data (only used in PrimitiveWorldIntersection now)
	int           bThreadSafeMesh;         //!< set if it's known that no other thread will try to modify the colliding geometry
	geom_contact* pGlobalContacts;         //!< pointer to thread's global contact buffer
};

struct phys_geometry
{
	IGeometry*  pGeom;
	Vec3        Ibody; //!< tensor of inertia in body frame
	quaternionf q;     //!< body frame
	Vec3        origin;
	float       V; //!< volume
	int         nRefCount;
	int         surface_idx; //!< used for primitives and meshes without per-face ids
	int*        pMatMapping; //!< mat mapping; can later be overridden inside entity part
	int         nMats;
	void*       pForeignData; //!< any external pointer to be associated with phys geometry
};

struct bop_newvtx
{
	int idx;       //!< vertex index in the resulting A phys mesh
	int iBvtx;     //!< -1 if intersection vertex, >=0 if B vertex
	int idxTri[2]; //!< intersecting triangles' foreign indices
};

struct bop_newtri
{
	int   idxNew;  //!< a newly generated foreign index (can be remapped later)
	int   iop;     //!< triangle source 0=A, 1=B
	int   idxOrg;  //!< original (triangulated) triangle's foreign index
	int   iVtx[3]; //!< for each vertex, existing vertex index if >=0, -(new vertex index+1) if <0
	float areaOrg; //!< original triangle's area
	Vec3  area[3]; //!< areas of compementary triangles for each vertex (divide by original tri area to get barycentric coords)
};

struct bop_vtxweld
{
	void set(int _ivtxDst, int _ivtxWelded) { ivtxDst = _ivtxDst; ivtxWelded = _ivtxWelded; }
	int  ivtxDst    : 16; //!< ivtxWelded is getting replaced with ivtxDst
	int  ivtxWelded : 16;
};

struct bop_TJfix
{
	void set(int _iACJ, int _iAC, int _iABC, int _iCA, int _iTJvtx) { iACJ = _iACJ; iAC = _iAC; iABC = _iABC; iCA = _iCA; iTJvtx = _iTJvtx; }
	// A _____J____ C	 (ACJ is a thin triangle on top of ABC; J is 'the junction vertex')
	// \		.	  /		 in ABC: set A->Jnew
	//     \	. /			 in ACJ: set J->Jnew, A -> A from original ABC, C -> B from original ABC
	//       \/
	//       B
	int iABC;   //!< big triangle's foreign idx
	int iACJ;   //!< small triangle's foreign idx
	int iCA;    //!< CA edge number in ABC
	int iAC;    //!< AC edge number in ACJ
	int iTJvtx; //!< J vertex index
};

struct bop_meshupdate_thunk
{
	bop_meshupdate_thunk() { prevRef = nextRef = this; }
	virtual ~bop_meshupdate_thunk() { prevRef->nextRef = nextRef; nextRef->prevRef = prevRef; prevRef = nextRef = this; }
	bop_meshupdate_thunk* prevRef, * nextRef;
};

struct bop_meshupdate : bop_meshupdate_thunk
{
	bop_meshupdate() { Reset(); }
	virtual ~bop_meshupdate();

	void Reset()
	{
		pRemovedVtx = 0;
		pRemovedTri = 0;
		pNewVtx = 0;
		pNewTri = 0;
		pWeldedVtx = 0;
		pTJFixes = 0;
		pMovedBoxes = 0;
		nRemovedVtx = nRemovedTri = nNewVtx = nNewTri = nWeldedVtx = nTJFixes = nMovedBoxes = 0;
		next = 0;
		pMesh[0] = pMesh[1] = 0;
		relScale = 1.0f;
		relRot.SetIdentity();
	}

	IGeometry*       pMesh[2]; //!< 0-dst (A), 1-src (B)
	int*             pRemovedVtx;
	int              nRemovedVtx;
	int*             pRemovedTri;
	int              nRemovedTri;
	bop_newvtx*      pNewVtx;
	int              nNewVtx;
	bop_newtri*      pNewTri;
	int              nNewTri;
	bop_vtxweld*     pWeldedVtx;
	int              nWeldedVtx;
	bop_TJfix*       pTJFixes;
	int              nTJFixes;
	bop_meshupdate*  next;
	primitives::box* pMovedBoxes;
	int              nMovedBoxes;
	float            relScale;
	Quat             relRot;
};

struct trinfo
{
	trinfo& operator=(trinfo& src) { ibuddy[0] = src.ibuddy[0]; ibuddy[1] = src.ibuddy[1]; ibuddy[2] = src.ibuddy[2]; return *this; }
	index_t ibuddy[3];
};

//! Mesh island is a connected group of trinagles.
struct mesh_island
{
	int   itri;       //!< first triangle
	int   nTris;      //!< trinagle count
	int   iParent;    //!< outer island is V<0
	int   iChild;     //!< first islands inside it with V<0
	int   iNext;      //!< maintains a linked list of iChild's inside iParent
	float V;          //!< can be negative (means it represents an inner surface)
	Vec3  center;     //!< geometrical center
	int   bProcessed; //!< for internal use
};

//! \cond INTERNAL
//! Maintains a linked triangle list inside an island.
struct tri2isle
{
	unsigned int inext : 16;
	unsigned int isle  : 15;
	unsigned int bFree : 1;
};
//! \endcond

struct mesh_data : primitives::primitive
{
	index_t*              pIndices;
	char*                 pMats;
	int*                  pForeignIdx; //!< an int associated with each face
	strided_pointer<Vec3> pVertices;
	Vec3*                 pNormals;
	int*                  pVtxMap;   //!< original vertex index->merged vertex index
	trinfo*               pTopology; //!< neighbours for each triangle's edge
	int                   nTris, nVertices;
	mesh_island*          pIslands;
	int                   nIslands;
	tri2isle*             pTri2Island;
	int                   flags;
};

const int BOP_NEWIDX0 = 0x8000000;

enum geomtypes
{
	GEOM_TRIMESH = primitives::triangle::type, GEOM_HEIGHTFIELD = primitives::heightfield::type, GEOM_CYLINDER = primitives::cylinder::type,
	GEOM_CAPSULE = primitives::capsule::type, GEOM_RAY = primitives::ray::type, GEOM_SPHERE = primitives::sphere::type,
	GEOM_BOX     = primitives::box::type, GEOM_VOXELGRID = primitives::voxelgrid::type
};
enum foreigntypes { DATA_OWNED_OBJECT = 1, DATA_MESHUPDATE = -1, DATA_UNUSED = -2 };

enum meshflags
{
	//! mesh_shared_... flags mean that the mesh will not attempt to free the corresponding array upon deletion
	mesh_shared_vtx             = 1, mesh_shared_idx = 2, mesh_shared_mats = 4, mesh_shared_foreign_idx = 8, mesh_shared_normals = 0x10,
	//! bounding volume flags (if several are specified in CreateMesh(), the best fitting one will be used)
	mesh_OBB                    = 0x20, mesh_AABB = 0x40, mesh_SingleBB = 0x80, mesh_AABB_rotated = 0x40000, mesh_VoxelGrid = 0x80000,
	//! mesh_multicontact is a hint on how many contacts per node to expect; 0-one contact, 2-no limit, 1-balanced
	mesh_multicontact0          = 0x100, mesh_multicontact1 = 0x200, mesh_multicontact2 = 0x400,
	//! mesh_approx flags are used in CreateMesh to specify which primitive approximations to try
	mesh_approx_cylinder        = 0x800, mesh_approx_box = 0x1000, mesh_approx_sphere = 0x2000, mesh_approx_capsule = 0x200000,
	mesh_keep_vtxmap            = 0x8000,    //!< keeps vertex map after adjacent vertices were merged
	mesh_keep_vtxmap_for_saving = 0x10000,   //!< deletes vertex map after loading
	mesh_no_vtx_merge           = 0x20000,   //!< does not attempt to merge adjacent vertices
	mesh_always_static          = 0x100000,  //!< simplifies phys mass properties calculation if by just setting them to 0
	mesh_full_serialization     = 0x400000,  //!< mesh will save all data to stream unconditionally
	mesh_transient              = 0x800000,  //!< all mesh allocations will go to a flushable pool
	mesh_no_booleans            = 0x1000000, //!< disables boolean operations on the mesh
	mesh_AABB_plane_optimise    = 0x4000,    //!< aabb generation is faster since it assumes the tri's are in a plane and distributed uniformly
	mesh_no_filter              = 0x2000000  //!< doesn't attempt to filter away degenerate triangles
};
enum meshAuxData { mesh_data_materials = 1, mesh_data_foreign_idx = 2, mesh_data_vtxmap = 4 }; //!< used in DestroyAuxiliaryMeshData

struct IOwnedObject
{
	// <interfuscator:shuffle>
	virtual ~IOwnedObject(){}
	virtual int Release() = 0;
	// </interfuscator:shuffle>
};

struct SOcclusionCubeMap;

struct IGeometry
{
	struct SBoxificationParams
	{
		SBoxificationParams() { minFaceArea = sqr(0.4f); distFilter = 0.2f; voxResolution = 100; maxFaceTiltAngle = DEG2RAD(10); minLayerFilling = 0.5f; maxLayerReusage = 0.8f; maxVoxIslandConnections = 0.5f; }
		float minFaceArea;             //!< ignore patches smaller than this in the box growing process
		float distFilter;              //!< smooth away details smaller than this (in terms of linear size)
		int   voxResolution;           //!< resolution of the grid in the longest direction
		float maxFaceTiltAngle;        //!< tolerance for patch alignment with box faces
		float minLayerFilling;         //!< stop growing boxes once layer fill percentage falls below this
		float maxLayerReusage;         //!< stop growing a box if it goes through already used cells (> than this percentage)
		float maxVoxIslandConnections; //!< ignore isolated voxel islands that have more than this amount of connections to the used ones
	};
	struct SProxifyParams
	{
		SProxifyParams() : ncells(50), islandMap(-1l), maxLineDot(0.88f), maxLineDist(2.0f), minLineCells(8), minSurfCells(50), surfPrimIters(1.0f), surfMinNormLen(0.5f), surfMergeDist(4.0f),
			surfNormRefineThresh(0.94f), primVoxInflate(1.5f), primRefillThresh(0.6f), primVfillSurf(0.85f), primVfillLine(0.6f), primSurfOutside(0.4f), capsHRratio(4.0f), maxGeoms(128),
			surfMeshMinCells(80), surfMeshIters(7), lenVtxNorm(1.0f), surfRefineWithMesh(1), surfMaxAndMinNorms(0), inflatePrims(0), inflateMeshes(0), nVoxPatches(0),
			mergeIslands(1), convexHull(0), closeHoles(0), forceBBox(0), findPrimSurfaces(1), findPrimLines(1), findMeshes(1), storeVox(0), reuseVox(0), flipCurCell(0), skipPrimMask(0)
		{ MARK_UNUSED qForced, pVoxPatches; }
		int          ncells;
		uint64       islandMap;
		Quat         qForced;
		float        maxLineDot, maxLineDist;
		int          minLineCells;
		int          minSurfCells;
		float        surfPrimIters;
		float        surfMinNormLen;
		float        surfMergeDist;
		float        surfNormRefineThresh;
		float        primVoxInflate;
		float        primRefillThresh;
		float        primVfillSurf;
		float        primVfillLine;
		float        primSurfOutside;
		float        capsHRratio;
		int          maxGeoms;
		uint64       skipPrimMask;
		int          surfMeshMinCells;
		int          surfMeshIters;
		float        lenVtxNorm;
		float        inflatePrims;
		float        inflateMeshes;
		Vec3i*       pVoxPatches;
		int          nVoxPatches;
		unsigned int mergeIslands       : 1;
		unsigned int convexHull         : 1;
		unsigned int closeHoles         : 1;
		unsigned int forceBBox          : 1;
		unsigned int findPrimSurfaces   : 1;
		unsigned int findPrimLines      : 1;
		unsigned int findMeshes         : 1;
		unsigned int surfMaxAndMinNorms : 1;
		unsigned int surfRefineWithMesh : 1;
		unsigned int storeVox           : 1;
		unsigned int reuseVox           : 1;
		unsigned int flipCurCell        : 1;
	};
	// <interfuscator:shuffle>
	virtual ~IGeometry(){}
	virtual int  GetType() = 0; //!< see enum geomtypes
	virtual int  AddRef() = 0;
	virtual void Release() = 0;
	virtual void Lock(int bWrite = 1) = 0;                         //!< locks the geometry for reading or writing
	virtual void Unlock(int bWrite = 1) = 0;                       //!< bWrite should match the preceding Lock
	virtual void GetBBox(primitives::box* pbox) = 0;               //!< possibly oriented bbox (depends on BV tree type)
	virtual int  CalcPhysicalProperties(phys_geometry* pgeom) = 0; //!< O(num_triangles) for meshes, unless mesh_always_static is set
	virtual int  PointInsideStatus(const Vec3& pt) = 0;            //!< for meshes, will create an auxiliary hashgrid for acceleration
	//! IntersectLocked - the main function for geomtries. pdata1,pdata2,pparams can be 0 - defaults will be assumed.
	//! returns a pointer to an internal thread-specific contact buffer, locked with the lock argument
	virtual int IntersectLocked(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2, intersection_params* pparams, geom_contact*& pcontacts, WriteLockCond& lock, int iCaller = MAX_PHYS_THREADS) = 0;
	//! Intersect - same as Intersect, but doesn't lock pcontacts
	virtual int Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2, intersection_params* pparams, geom_contact*& pcontacts) = 0;
	//! FindClosestPoint - for non-convex meshes only does local search, doesn't guarantee global minimum
	//! iFeature's format: (feature type: 2-face, 1-edge, 0-vertex)<<9 | feature index
	//! if ptdst0 and ptdst1 are different, searches for a closest point on a line segment
	//! ptres[0] is the closest point on the geometry, ptres[1] - on the test line segment
	virtual int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1, Vec3* ptres, int nMaxIters = 10) = 0;
	//! CalcVolumetricPressure: a fairly correct computation of volumetric pressure with inverse-quadratic falloff (ex: explosions)
	//! for a surface fragment dS, impulse is: k*dS*cos(surface_normal,direction to epicenter) / max(rmin, distance to epicenter)^2
	//! returns integral impulse and angular impulse
	virtual void  CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin, const Vec3& centerOfMass, Vec3& P, Vec3& L) = 0;
	//! CalculateBuoyancy: computes the submerged volume (return value) and the mass center of the submerged part
	virtual float CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& submergedMassCenter) = 0;
	//! CalculateMediumResistance: computes medium resistance integral of the surface; self flow of the medium should be baked into pgwd
	//! for a surface fragment dS with normal n and velocity v impulse is: -n*max(0,n*v) (can be scaled by the medium resistance coeff. later)
	virtual void CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres, Vec3& dLres) = 0;
	//! DrawWireframe: draws physics helpers; iLevel>0 will draaw a level in the bounding volume tree
	virtual void DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor) = 0;
	virtual int  GetPrimitiveId(int iPrim, int iFeature) = 0; //!< get material id for a primitive (iFeature is ignored currently)
	//! GetPrimitive: expects a valid pprim pointer, type depends on GetType; meshes return primitives::triangle
	virtual int  GetPrimitive(int iPrim, primitives::primitive* pprim) = 0;
	virtual int  GetForeignIdx(int iPrim) = 0;                      //!< only works for meshes
	virtual Vec3 GetNormal(int iPrim, const Vec3& pt) = 0;          //!< only implemented for meshes currently; pt is ignored
	virtual int  GetFeature(int iPrim, int iFeature, Vec3* pt) = 0; //!< returns vertices of face, edge, or vertex; only for boxes and meshes currently
	virtual int  IsConvex(float tolerance) = 0;
	//! PrepareForRayTest: creates an auxiliary hash structure for short rays test acceleration
	virtual void PrepareForRayTest(float raylen) = 0; //!< raylen - 'expected' ray length to optimize the hash for
	//! BuildOcclusionCubemap: cubemap projection-based occlusion (used for explosions); pGrids are 6 [nRes^2] arrays
	//! iMode: 0 - update cubemap in pGrid0; 1 - build cubemap in pGrid1, grow edges by nGrow cells, and compare with pGrid0
	//! all geometry closer than rmin is ignored (to avoid large projection scale); same if farther than rmax
	virtual float                        BuildOcclusionCubemap(geom_world_data* pgwd, int iMode, SOcclusionCubeMap* cubemap0, SOcclusionCubeMap* cubemap1, int nGrow) = 0;
	virtual void                         GetMemoryStatistics(ICrySizer* pSizer) = 0;
	virtual void                         Save(CMemStream& stm) = 0;
	virtual void                         Load(CMemStream& stm) = 0;
	//! Load: meshes can avoid storing vertex, index, and mat id data, in this case same arrays should be provided during loading
	virtual void                         Load(CMemStream& stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char* pIds) = 0;
	virtual int                          GetPrimitiveCount() = 0;
	virtual const primitives::primitive* GetData() = 0;                             //!< returns an pointer to an internal structure; for meshes returns mesh_data
	virtual void                         SetData(const primitives::primitive*) = 0; //!< not supported by meshes
	virtual float                        GetVolume() = 0;
	virtual Vec3                         GetCenter() = 0;
	//! Subtract: performs boolean subtraction; if bLogUpdates==1, will create bop_meshupdate inside the mesh
	virtual int                          Subtract(IGeometry* pGeom, geom_world_data* pdata1, geom_world_data* pdata2, int bLogUpdates = 1) = 0;
	virtual int                          GetSubtractionsCount() = 0; //!< number of Subtract()s the mesh has survived so far
	//! GetForeignData: returns a pointer associated with the geometry
	//! special: GetForeignData(DATA_MESHUPDATE) returns the internal bop_meshupdate list (does not interfere with the main foreign pointer)
	virtual void* GetForeignData(int iForeignData = 0) = 0;
	virtual int   GetiForeignData() = 0; //!< foreign data type
	virtual void  SetForeignData(void* pForeignData, int iForeignData) = 0;
	virtual int   GetErrorCount() = 0;                                                      //!< for meshes, the number of edges that don't belong to exactly 2 triangles
	virtual void  DestroyAuxilaryMeshData(int idata) = 0;                                   //!< see meshAuxData enum
	virtual void  RemapForeignIdx(int* pCurForeignIdx, int* pNewForeignIdx, int nTris) = 0; //!< used in rendermesh-physics sync after boolean ops
	virtual void  AppendVertices(Vec3* pVtx, int* pVtxMap, int nVtx) = 0;                   //!< used in rendermesh-physics sync after boolean ops
	virtual float GetExtent(EGeomForm eForm) const = 0;
	virtual void  GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const = 0;
	virtual void  CompactMemory() = 0; //!< used only by non-breakable meshes to compact non-shared vertices into same contingous block of memory
	//! Boxify: attempts to build a set of boxes covering the geometry's volume (only supported by trimeshes)
	virtual int   Boxify(primitives::box* pboxes, int nMaxBoxes, const SBoxificationParams& params) = 0;
	//! Proxies: attempts to build a set of primitives and meshes to approximate the mesh
	virtual int   Proxify(IGeometry**& pOutGeoms, SProxifyParams* pparams = 0) = 0;
	//! Sanity check the geometry. i.e. its tree doesn't have an excessive depth. returns 0 if fails
	virtual int   SanityCheck() = 0;
	// </interfuscator:shuffle>
};

/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// IGeometryManager Interface /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

struct SMeshBVParams {};

struct SBVTreeParams : SMeshBVParams
{
	int   nMinTrisPerNode; //!< if a split creates a node with <nMinTris triangles, another split is attempted
	int   nMaxTrisPerNode; //!< nodes are split until they have more triangles than this
	float favorAABB;       //!< when several BV trees are requested in CreateMesh, it selects the one with the smallest volume; favorAABB scales AABB's volume down
};

//! \cond INTERNAL
//! Voxel grid is a regular 3d grid collision test acceleration structure.
struct SVoxGridParams : SMeshBVParams
{
	Vec3  origin;
	Vec3  step;
	Vec3i size;
};

struct ITetrLattice
{
	// <interfuscator:shuffle>
	virtual ~ITetrLattice(){}
	virtual int        SetParams(pe_params*) = 0; //!< only accepts pe_tetrlattice_params
	virtual int        GetParams(pe_params*) = 0;
	virtual void       DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int idxColor) = 0;
	virtual IGeometry* CreateSkinMesh(int nMaxTrisPerBVNode = 8) = 0;      //!< builds triangle mesh for exterior faces
	virtual int        CheckPoint(const Vec3& pt, int* idx, float* w) = 0; //!< check if a point is inside any tetrahedron, fills barycentric weights[4]
	virtual void       Release() = 0;
	// </interfuscator:shuffle>
};

struct IBreakableGrid2d
{
	// <interfuscator:shuffle>
	virtual ~IBreakableGrid2d(){}
	//! BreakIntoChunks: emulates fracure in the grid around pt with dimensions r x ry
	//! ptout receives a pointer to a vertex array
	//! return value is an allocated array of indices, 3 per triangle, -1 marks contour end, -2 array end
	//! maxPatchTris tells to unite broken trianges into patches of up to this size; 0 means broken triangles are discarded
	//! jointhresh (0..1) affects the way triangles unite into patches
	//! seed bootsraps the RNG if >=0
	//! hole edges are filtered to removes corners sharper than filterAng (except those that are formed by several joined triangles)
	virtual int*              BreakIntoChunks(const Vec2& pt, float r, Vec2*& ptout, int maxPatchTris, float jointhresh, int seed = -1, float filterAng = 0.0f, float ry = 0.0f) = 0;
	virtual primitives::grid* GetGridData() = 0;
	virtual bool              IsEmpty() = 0;
	virtual void              Release() = 0;
	virtual float             GetFracture() = 0; //!< destroyed percentage so far
	virtual void              GetMemoryStatistics(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};
//! \endcond

struct IGeomManager
{
	// <interfuscator:shuffle>
	virtual ~IGeomManager(){}
	virtual void InitGeoman() = 0;
	virtual void ShutDownGeoman() = 0;

	//! CreateMesh - depending on flags (see enum meshflags) can create either a mesh or a primitive that approximates
	//! approx_tolerance is the approximation tolerance in relative units
	//! pMats are per-face material ids (which can later be mapped via pMatMapping)
	//! pForeignIdx store any user data per face (internally indices might be sorted when building BV structure, pForegnIdx will reflect that)
	virtual IGeometry* CreateMesh(strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char* pMats, int* pForeignIdx, int nTris, int flags, float approx_tolerance = 0.05f, int nMinTrisPerNode = 2, int nMaxTrisPerNode = 4, float favorAABB = 1.0f) = 0;
	virtual IGeometry* CreateMesh(strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char* pMats, int* pForeignIdx, int nTris, int flags, float approx_tolerance, SMeshBVParams* pParams) = 0; //!< this version can take SVoxGridParams in pParams, if mesh_VoxelGrid is set
	virtual IGeometry* CreatePrimitive(int type, const primitives::primitive* pprim) = 0;                                                                                                                                    //!< used to create primitives explicitly
	virtual void       DestroyGeometry(IGeometry* pGeom) = 0;                                                                                                                                                                //!< just calls Release() on pGeom

	//! RegisterGeometry: creates a phys_geometry structure for IGeometry, computes mass properties
	//! phys_geometries are managed in pools internally; the new structure has nRefCount 1
	//! defSurfaceIdx will be used (until overwritten in entity part) if the geometry doesn't have per-face materials
	virtual phys_geometry* RegisterGeometry(IGeometry* pGeom, int defSurfaceIdx = 0, int* pMatMapping = 0, int nMats = 0) = 0;
	virtual int            AddRefGeometry(phys_geometry* pgeom) = 0;
	virtual int            UnregisterGeometry(phys_geometry* pgeom) = 0; //!< decreases nRefCount, frees the pool slot if <=0
	virtual void           SetGeomMatMapping(phys_geometry* pgeom, int* pMatMapping, int nMats) = 0;

	virtual void           SaveGeometry(CMemStream& stm, IGeometry* pGeom) = 0;
	virtual IGeometry*     LoadGeometry(CMemStream& stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char* pMats) = 0;
	virtual void           SavePhysGeometry(CMemStream& stm, phys_geometry* pgeom) = 0;
	virtual phys_geometry* LoadPhysGeometry(CMemStream& stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char* pIds) = 0;
	virtual IGeometry*     CloneGeometry(IGeometry* pGeom) = 0;

	virtual ITetrLattice*  CreateTetrLattice(const Vec3* pt, int npt, const int* pTets, int nTets) = 0;
	//! RegisterCrack: cracks are used for ITertLattice-induced breaking to subtract a shape whenever a tetrahedral face breaks
	//! pVtx specify 3 control vertices; when applying, they are affinely stretched to match the broken face's corners
	//! idmat is breakability index
	virtual int  RegisterCrack(IGeometry* pGeom, Vec3* pVtx, int idmat) = 0;
	virtual void UnregisterCrack(int id) = 0;
	virtual void UnregisterAllCracks(void (* OnRemoveGeom)(IGeometry* pGeom) = 0) = 0;
	//! GetCrackGeom - creates a stretched crack based on the three corner vertices (pt[3])
	//! pgwd receives it world transformation
	virtual IGeometry* GetCrackGeom(const Vec3* pt, int idmat, geom_world_data* pgwd) = 0;

	//! GenerateBreakebleGrid - creates a perturbed regular grid of points
	//! ptsrc's bbox is split into a nCells grid (with an additional border)
	//! vertices are randomly perturbed up to 0.4 cell size (seed can bootstrap the randomizer)
	//! ptsrc is "painted" into the grid, snapping the closest grid vertices to ptsrc (but no new ones are created at this stage)
	//! bStatic is ignored currently
	virtual IBreakableGrid2d* GenerateBreakableGrid(Vec2* ptsrc, int npt, const Vec2i& nCells, int bStatic = 1, int seed = -1) = 0;

	virtual void              ReleaseGeomsImmediately(bool bReleaseImmediately) = 0;
	// </interfuscator:shuffle>
};

/////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// IPhysUtils Interface /////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

typedef void* (* qhullmalloc)(size_t);

struct IPhysUtils
{
	// <interfuscator:shuffle>
	virtual ~IPhysUtils(){}
	//! CoverPolygonWithCircles - attempts to fits circles to roughly cover a polygon (can used to generate round spalshes over an area)
	//! bConsecutive is false, uses convex hull of the points
	//! center is a pre-calculated geometrical center of pt's
	//! outputs data into centers and radii arrays, which use global buffers; returns the number of circles
	virtual int  CoverPolygonWithCircles(strided_pointer<Vec2> pt, int npt, bool bConsecutive, const Vec2& center, Vec2*& centers, float*& radii, float minCircleRadius) = 0;
	//! qhull - computes convex hull of a set of 3d points. returns the number of triangles in pTris
	//! pts - point list
	//! npts - number of points
	//! pTris - will receive an internally allocated pointer to the triangles (recommended to be deleted via DeletePointer)
	//! qmalloc - optional custom allocator for pTris 
	virtual int  qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris, qhullmalloc qmalloc = 0) = 0;
	//! qhull2d - computes convex hull of a set of 2d points. returns the number of edges (linked via edgeitem.next)
	//! pts - source points (only pt needs to be set in ptitem; next/prev are used internally; iContact can be used to store some relevant index)
	//! nVtx - number of source points
	//! edges - pre-allocated destination array of edges (pvtx and next are the output fields)
	//! nMaxEdges - if >0, the function will chop vertices with the least associated "ear" area until the limit is satisfied
	//              in this case the first edge of the hull will not necessarily be in edges[0] - it'll be the first item with non-0 next
	virtual int  qhull2d(ptitem2d* pts, int nVtx, edgeitem* edges, int nMaxEdges = 0) = 0;
	//! DeletePointer - deletes any pointer allocated by the physics
	virtual void DeletePointer(void* pdata) = 0; //!< should be used to free data allocated in physics
	//! Triangulates a 2d polygon, returns the number of triangles
	//! pVtx - source points of the polygon. polygons can have several unconnected contours. counter-clockwise contours have polygon area inside them, 
	//         clockwise - outside. each contour must have a terminator point with MARK_UNUSED pt[i].x (even if there's only one contour)
	//! nVtx - the number of source points, including terminators
	//! pTris - pointer to a pre-allocated destination index buffer; (nVtx+nContours*2)*3 is a conservative upper limit
	//! szTriBuf - size of the index buffer (assuming 3 indices per tri)
	virtual int  TriangulatePoly(Vec2* pVtx, int nVtx, int* pTris, int szTriBuf) = 0;
	// </interfuscator:shuffle>
};

/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// IPhysicalEntity Interface //////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

enum snapshot_flags { ssf_compensate_time_diff = 1, ssf_checksum_only = 2, ssf_no_update = 4 };

//! Represents a physical entity instance in the world
//! Note that this is independent of entities (IEntity), but physical entities can be attached to entities via the entity system.
struct IPhysicalEntity
{
	// <interfuscator:shuffle>
	virtual ~IPhysicalEntity(){}
	virtual pe_type GetType() const = 0; //!< returns pe_type

	virtual int     AddRef() = 0;
	virtual int     Release() = 0;

	//! Sets / changes parameters on a physical entity; can be queued and executed later if the physics is busy (unless bThreadSafe is flagged)
	//! \return !0 if successful.
	//! \par Example
	//! \include CryPhysics/Examples/SetPhysicalEntityPosition.cpp
	virtual int SetParams(pe_params* params, int bThreadSafe = 0) = 0;
	//! Gets the parameters of a physical entity, uses the same structures as SetParams;
	//! \return  !0 if successful
	//! \par Example
	//! \include CryPhysics/Examples/GetSimulationParams.cpp
	virtual int GetParams(pe_params* params) const = 0;
	//! Gets the status of a physical entity, using the pe_status structure.
	//! \return In most cases, >0 if successful - with special exception for some pe_status implementations.
	//! \par Example
	//! \include CryPhysics/Examples/GetPhysicalEntityPosition.cpp
	virtual int GetStatus(pe_status* status) const = 0;
	//! Performs an action such as applying an impulse on an entity.
	//! \par Example
	//! \include CryPhysics/Examples/ActionImpulse.cpp
	virtual int Action(pe_action*, int bThreadSafe = 0) = 0;

	//! AddGeometry - add a new entity part, containing pgeom; request can get queued
	//! params can be specialized depending on the entity type
	//! id is a requested geometry id (expected to be unique), if -1 - assign automatically
	//! returns geometry id (0..some number), -1 means error
	virtual int   AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 0) = 0;
	virtual void  RemoveGeometry(int id, int bThreadSafe = 0) = 0; //!< returns !0 if successful; can get queued

	virtual void* GetForeignData(int itype = 0) const = 0; //!< returns entity's pForeignData if itype matches iForeignData, 0 otherwise
	virtual int   GetiForeignData() const = 0;             //!< returns entity's iForegnData

	virtual int          GetStateSnapshot(class CStream& stm, float time_back = 0, int flags = 0) = 0; //!< obsolete, was used in Far Cry
	virtual int          GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) = 0;
	virtual int          SetStateFromSnapshot(class CStream& stm, int flags = 0) = 0;    //!< obsolete
	virtual int          PostSetStateFromSnapshot() = 0;                                 //!< obsolete
	virtual unsigned int GetStateChecksum() = 0;                                         //!< obsolete
	virtual void         SetNetworkAuthority(int authoritive = -1, int paused = -1) = 0; //!< -1 dont change, 0 - set to false, 1 - set to true

	virtual int          SetStateFromSnapshot(TSerialize ser, int flags = 0) = 0;
	virtual int          SetStateFromTypedSnapshot(TSerialize ser, int type, int flags = 0) = 0;
	virtual int          GetStateSnapshotTxt(char* txtbuf, int szbuf, float time_back = 0) = 0; //!< packs state into ASCII text
	virtual void         SetStateFromSnapshotTxt(const char* txtbuf, int szbuf) = 0;

	//! DoStep: evolves entity in time. Normally this is called from PhysicalWorld::TimeStep
	virtual int             DoStep(float time_interval, int iCaller = MAX_PHYS_THREADS) = 0;
	virtual void            StartStep(float time_interval) = 0; //!< must be called before DoStep
	virtual void            StepBack(float time_interval) = 0;
	virtual IPhysicalWorld* GetWorld() const = 0; //!< returns physical world this entity belongs to

	virtual void            GetMemoryStatistics(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};

/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// IPhysicsEventClient Interface //////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

//! \cond INTERNAL
//! Obsolete, replaced with event system (EventPhys...).
struct IPhysicsEventClient
{
	// <interfuscator:shuffle>
	virtual ~IPhysicsEventClient(){}
	virtual void OnBBoxOverlap(IPhysicalEntity* pEntity, void* pForeignData, int iForeignData, IPhysicalEntity* pCollider, void* pColliderForeignData, int iColliderForeignData) = 0;
	virtual void OnStateChange(IPhysicalEntity* pEntity, void* pForeignData, int iForeignData, int iOldSimClass, int iNewSimClass) = 0;
	virtual void OnCollision(IPhysicalEntity* pEntity, void* pForeignData, int iForeignData, coll_history_item* pCollision) = 0;
	virtual int  OnImpulse(IPhysicalEntity* pEntity, void* pForeignData, int iForeignData, pe_action_impulse* impulse) = 0;
	virtual void OnPostStep(IPhysicalEntity* pEntity, void* pForeignData, int iForeignData, float dt) = 0;
	// </interfuscator:shuffle>
};
//! \endcond

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// IPhysicalWorld Interface //////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

enum draw_helper_flags { pe_helper_collisions = 1, pe_helper_geometry = 2, pe_helper_bbox = 4, pe_helper_lattice = 8 };
enum surface_flags { sf_pierceable_mask = 0x0F, sf_max_pierceable = 0x0F, sf_important = 0x200, sf_manually_breakable = 0x400, sf_matbreakable_bit = 16 };
#define sf_pierceability(i) (i)
#define sf_matbreakable(i)  (((i) + 1) << sf_matbreakable_bit)
//! \see RayWorldIntersection
enum rwi_flags
{
	rwi_ignore_terrain_holes     = 0x20, rwi_ignore_noncolliding = 0x40, rwi_ignore_back_faces = 0x80, rwi_ignore_solid_back_faces = 0x100,
	rwi_pierceability_mask       = 0x0F, rwi_pierceability0 = 0, rwi_stop_at_pierceable = 0x0F,
	rwi_max_piercing             = 0x10,         //!< the ray will pierce all surfaces (including those with pierceability 0)
	rwi_separate_important_hits  = sf_important, //!< among pierceble hits, materials with sf_important will have priority
	rwi_colltype_bit             = 16,           //!< used to manually specify collision geometry types (default is geom_colltype_ray)
	rwi_colltype_any             = 0x400,        //!< if several colltype flag are specified, switches between requiring all or any of them in a geometry
	rwi_queue                    = 0x800,        //!< queues the RWI request, when done it'll generate EventPhysRWIResult
	rwi_force_pierceable_noncoll = 0x1000,       //!< non-colliding geometries will be treated as pierceable regardless of the actual material
	rwi_update_last_hit          = 0x4000,       //!< update phitLast with the current hit results (should be set if the last hit should be reused for a "warm" start)
	rwi_any_hit                  = 0x8000        //!< returns the first found hit for meshes, not necessarily the closets
};
#define rwi_pierceability(pty)      (pty)
#define rwi_colltype_all(colltypes) ((colltypes) << rwi_colltype_bit)
#define rwi_colltype_any(colltypes) ((colltypes) << rwi_colltype_bit | rwi_colltype_any)
//! \see GetEntitiesInBox and RayWorldIntersection
enum entity_query_flags
{
	ent_static                 = 1, ent_sleeping_rigid = 2, ent_rigid = 4, ent_living = 8, ent_independent = 16, ent_deleted = 128, ent_terrain = 0x100,
	ent_all                    = ent_static | ent_sleeping_rigid | ent_rigid | ent_living | ent_independent | ent_terrain,
	ent_flagged_only           = pef_update, ent_skip_flagged = pef_update * 2, //!< "flagged" meas has pef_update set
	ent_areas                  = 32, ent_triggers = 64,
	ent_ignore_noncolliding    = 0x10000,
	ent_sort_by_mass           = 0x20000,  //!< sort by mass in ascending order
	ent_allocate_list          = 0x40000,  //!< if not set, the function will return an internal pointer
	ent_addref_results         = 0x100000, //!< will call AddRef on each entity in the list (expecting the caller call Release)
	ent_water                  = 0x200,    //!< can only be used in RayWorldIntersection
	ent_no_ondemand_activation = 0x80000,  //!< can only be used in RayWorldIntersection
	ent_delayed_deformations   = 0x80000   //!< queues procedural breakage requests; can only be used in SimulateExplosion
};
enum phys_locks { PLOCK_WORLD_STEP = 1, PLOCK_QUEUE, PLOCK_TRACE_PENDING_RAYS, PLOCK_AREAS, PLOCK_CALLER0, PLOCK_CALLER1 };

struct phys_profile_info
{
	IPhysicalEntity* pEntity;
	int              nTicks, nCalls;
	int              nTicksLast, nCallsLast;
	int              nTicksAvg;
	float            nCallsAvg;
	int              nTicksPeak, nCallsPeak, peakAge;
	int              nTicksStep;
	int              id;
	const char*      pName;
};

struct phys_job_info
{
	int   jobType;
	int   nInvocations;
	int   nFallbacks;
	int64 nTicks, nLatency, nLatencyAbs;
	int64 nTicksPeak, nLatencyPeak, nLatencyAbsPeak, peakAge;
	char* pName;
};

struct SolverSettings
{
	int   nMaxStackSizeMC;            //!< def 8
	float maxMassRatioMC;             //!< def 50
	int   nMaxMCiters;                //!< def 1400
	int   nMinMCiters;                //!< def 4
	int   nMaxMCitersHopeless;        //!< def 400
	float accuracyMC;                 //!< def 0.005
	float accuracyLCPCG;              //!< def 0.005
	int   nMaxContacts;               //!< def 150
	int   nMaxPlaneContacts;          //!< def 7
	int   nMaxPlaneContactsDistress;  //!< def 4
	int   nMaxLCPCGsubiters;          //!< def 120
	int   nMaxLCPCGsubitersFinal;     //!< def 250
	int   nMaxLCPCGmicroiters;
	int   nMaxLCPCGmicroitersFinal;
	int   nMaxLCPCGiters;             //!< def 5
	float minLCPCGimprovement;        //!< def 0.1
	int   nMaxLCPCGFruitlessIters;    //!< def 4
	float accuracyLCPCGnoimprovement; //!< def 0.05
	float minSeparationSpeed;         //!< def 0.02
	float maxvCG;
	float maxwCG;
	float maxvUnproj;
	int   bCGUnprojVel;
	float maxMCMassRatio;
	float maxMCVel;
	int   maxLCPCGContacts;
};

enum entity_out_of_bounds_flags
{
	raycast_out_of_bounds      = 1, //!< Affects ray casts. NB, ray casting out of bounds entities can cause performance issues
	get_entities_out_of_bounds = 2, //!< Affects GetEntitiesAround.
};

struct PhysicsVars : SolverSettings
{
	int   bFlyMode;
	int   iCollisionMode;
	int   bSingleStepMode;
	int   bDoStep;
	float fixedTimestep;
	float timeGranularity;
	float maxWorldStep;
	int   iDrawHelpers;
	float drawHelpersOpacity;
	int   iOutOfBounds;
	float maxContactGap;
	float maxContactGapPlayer;
	float minBounceSpeed;
	int   bProhibitUnprojection;
	int   bUseDistanceContacts;
	float unprojVelScale;
	float maxUnprojVel;
	float maxUnprojVelRope;
	int   bEnforceContacts;
	int   nMaxSubsteps;
	int   nMaxSurfaces;
	Vec3  gravity;
	int   nGroupDamping;
	float groupDamping;
	int   nMaxSubstepsLargeGroup;
	int   nBodiesLargeGroup;
	int   bBreakOnValidation;
	int   bLogActiveObjects;
	int   bMultiplayer;
	int   bProfileEntities;
	int   bProfileFunx;
	int   bProfileGroups;
	int   nGEBMaxCells;
	int   nMaxEntityCells;
	int   nMaxAreaCells;
	float maxVel;
	float maxVelPlayers;
	float maxVelBones;
	float maxContactGapSimple;
	float penaltyScale;
	int   bSkipRedundantColldet;
	int   bLimitSimpleSolverEnergy;
	int   nMaxEntityContacts;
	int   bLogLatticeTension;
	int   nMaxLatticeIters;
	int   bLogStructureChanges;
	float tickBreakable;
	float approxCapsLen;
	int   nMaxApproxCaps;
	int   bPlayersCanBreak;
	float lastTimeStep;
	int   bMultithreaded;
	float breakImpulseScale;
	float rtimeGranularity;
	float massLimitDebris;
	int   flagsColliderDebris;
	int   flagsANDDebris;
	int   maxRopeColliderSize;
	int   maxSplashesPerObj;
	float splashDist0, minSplashForce0, minSplashVel0;
	float splashDist1, minSplashForce1, minSplashVel1;
	int   bDebugExplosions;
	float jointGravityStep;
	float jointDmgAccum;
	float jointDmgAccumThresh;
	float timeScalePlayers;
	float threadLag;
	int   numThreads;
	int   physCPU;
	int   physWorkerCPU;
	Vec3  helperOffset;
	int64 ticksPerSecond;
	// net-synchronization related
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	float netInterpTime;
	float netExtrapMaxTime;
	int   netSequenceFrequency;
	int   netDebugDraw;
#else
	float netMinSnapDist;
	float netVelSnapMul;
	float netMinSnapDot;
	float netAngSnapMul;
	float netSmoothTime;
#endif

	int   bEntGridUseOBB;
	int   nStartupOverloadChecks;
	float breakageMinAxisInertia; //!< For procedural breaking, each axis must have a minium inertia compared to the axis with the largest inertia (0.01-1.00)

	int   bForceSyncPhysics;
	int   idEntBreakOnAwake;
};

struct ray_hit
{
	float            dist;
	IPhysicalEntity* pCollider;
	int              ipart;
	int              partid;
	short            surface_idx;
	short            idmatOrg; //!< original material index, not mapped with material mapping
	int              foreignIdx;
	int              iNode; //!< BV tree node that had the intersection; can be used for "warm start" next time
	Vec3             pt;
	Vec3             n;        //!< surface normal
	int              bTerrain; //!< global terrain hit
	int              iPrim;    //!< hit triangle index
	ray_hit*         next;     //!< reserved for internal use, do not change
};

//! \cond INTERNAL
struct ray_hit_cached   //!< used in conjunction with rwi_reuse_last_hit
{
	ray_hit_cached() { pCollider = 0; ipart = 0; }
	ray_hit_cached(const ray_hit& hit) { pCollider = hit.pCollider; ipart = hit.ipart; iNode = hit.iNode; }
	ray_hit_cached& operator=(const ray_hit& hit) { pCollider = hit.pCollider; ipart = hit.ipart; iNode = hit.iNode; return *this; }

	IPhysicalEntity* pCollider;
	int              ipart;
	int              iNode;
};
//! \endcond

#ifndef PWI_NAME_TAG
	#define PWI_NAME_TAG "PrimitiveWorldIntersection"
#endif
#ifndef RWI_NAME_TAG
	#define RWI_NAME_TAG "RayWorldIntersection"
#endif

//! \see SimulateExplosion
struct pe_explosion
{
	pe_explosion() { nOccRes = 0; nGrow = 0; rminOcc = 0.1f; holeSize = 0; explDir.Set(0, 0, 1); iholeType = 0; forceDeformEntities = false; }
	Vec3 epicenter;    //!< epicenter for the occlusion computation
	Vec3 epicenterImp; //!< epicenter for impulse computation
	//! the impulse a surface fragment with area dS and normal n gets is: dS*k*n*max(0,n*dir_to_epicenter)/max(rmin, dist_to_epicenter)^2
	//! k is selected in such way that at impulsivePressureAtR = k/r^2
	float             rmin, rmax, r;
	float             impulsivePressureAtR;
	int               nOccRes;             //!< resolution of the occlusion map (0 disables)
	int               nGrow;               //!< grow occlusion projections by this amount of cells to allow explosion to reach around corners a bit
	float             rminOcc;             //!< ignores geometry closer than this for occlusion computations
	float             holeSize;            //!< explosion shape for iholeType will be scaled by this holeSize / shape's declared size
	Vec3              explDir;             //!< hit direction, for aligning the explosion boolean shape
	int               iholeType;           //!< breakability index for the explosion (<0 disables)
	bool              forceDeformEntities; //!< force deformation even if breakImpulseScale is zero
	//! filled as results
	IPhysicalEntity** pAffectedEnts;
	float*            pAffectedEntsExposure; //!< 0..1 exposure, computed from the occlusion map
	int               nAffectedEnts;
};

//! Physics events can be logged or immediate. The former are posted to the event queue and the client handler is called
//! during PumpLoggedEvents function. The latter are call client handlers immediately when they happen, which is likely
//! to be inside the physics thread, so the handler must be thread-safe.
//! In most cases, in order to generate events the entity must have the corresponding flag set
//! Important Note: Please keep event ids contigous in respect to being stereo or mono events
struct EventPhys
{
	EventPhys* next;
	int        idval;
};

//! Base for one-entity events, ids 0-2.
struct EventPhysMono : EventPhys
{
	IPhysicalEntity* pEntity;
	void*            pForeignData;
	int              iForeignData;
};

//! Base for two-entity events, ids 3-14.
struct EventPhysStereo : EventPhys
{
	IPhysicalEntity* pEntity[2];
	void*            pForeignData[2];
	int              iForeignData[2];
};

//! Generated by triggers and parts with geom_log_interactions.
struct EventPhysBBoxOverlap : EventPhysStereo
{
	enum entype { id = 0, flagsCall = 0, flagsLog = 0 };
	EventPhysBBoxOverlap() { idval = id; }
};

enum EventPhysCollisionState {EPC_DEFERRED_INITIAL, EPC_DEFERRED_REQUEUE, EPC_DEFERRED_FINISHED };

struct EventPhysCollision : EventPhysStereo
{
	enum entype { id = 2, flagsCall = pef_monitor_collisions, flagsLog = pef_log_collisions };
	EventPhysCollision() { idval = id; pEntContact = 0; iPrim[0] = iPrim[1] = -1; deferredState = EPC_DEFERRED_INITIAL; fDecalPlacementTestMaxSize = 1000.f; }
	int   idCollider; //!< in addition to pEntity[1]
	Vec3  pt;         //!< contact point in world coordinates
	Vec3  n;          //!< contact normal
	Vec3  vloc[2];    //!< velocities at the contact point
	float mass[2];
	int   partid[2];
	short idmat[2];
	short iPrim[2];
	float penetration;                //!< contact's penetration depth
	float normImpulse;                //!< impulse applied by the solver to resolve the collision
	float radius;                     //!< some characteristic size of the contact area
	void* pEntContact;                //!< reserved for internal use
	char  deferredState;              //!< EventPhysCollisionState
	char  deferredResult;             //!< stores the result returned by the deferred event
	float fDecalPlacementTestMaxSize; //!< maximum allowed size of decals caused by this collision
};

//! Triggered by simclass changes, even those caused by SetParams.
struct EventPhysStateChange : EventPhysMono
{
	enum entype { id = 8, flagsCall = pef_monitor_state_changes, flagsLog = pef_log_state_changes };
	EventPhysStateChange() { idval = id; }
	int   iSimClass[2];
	float timeIdle; //!< how long the entity stayed without external activation (such as impulses)
	Vec3  BBoxOld[2];
	Vec3  BBoxNew[2];
};

//! Called when something around the entityy breaks.
struct EventPhysEnvChange : EventPhysMono
{
	enum entype { id = 3, flagsCall = pef_monitor_env_changes, flagsLog = pef_log_env_changes };
	enum encode { EntStructureChange = 0 };
	EventPhysEnvChange() { idval = id; }
	int              iCode;
	IPhysicalEntity* pentSrc; //!< entity that broke
	IPhysicalEntity* pentNew; //!< new entity that broke off the original one
};

//! Entity has just completed its step.
struct EventPhysPostStep : EventPhysMono
{
	enum entype { id = 4, flagsCall = pef_monitor_poststep, flagsLog = pef_log_poststep };
	EventPhysPostStep() { idval = id; pGrid = nullptr; iCaller = 0; }
	float            dt;
	Vec3             pos;
	quaternionf      q;
	int              idStep; //!< world's internal step count
	IPhysicalEntity* pGrid; //!< interface to the grid
	int              iCaller; //!< index of the physics thread
};

//! Physics mesh changed.
struct EventPhysUpdateMesh : EventPhysMono
{
	enum entype { id = 5, flagsCall = 1, flagsLog = 2 };
	enum reason { ReasonExplosion, ReasonFracture, ReasonRequest, ReasonDeform };
	EventPhysUpdateMesh() { idval = id; idx = -1; pMesh = 0; }
	int             partid;
	int             bInvalid;
	int             iReason;       //!< see enum reason
	IGeometry*      pMesh;         //!< ->GetForeignData(DATA_MESHUPDATE) returns a list of bop_meshupdates
	bop_meshupdate* pLastUpdate;   //!< the last mesh update for at moment when the event was generated
	Matrix34f       mtxSkelToMesh; //!< skeleton's frame -> mesh's frame transform
	IGeometry*      pMeshSkel;     //!< for deformable bodies
	int             idx;           //!< used for event deferring by listeners
};

//! A part broke off an existing entity.
struct EventPhysCreateEntityPart : EventPhysMono
{
	enum entype { id = 6, flagsCall = 1, flagsLog = 2 };
	enum reason { ReasonMeshSplit, ReasonJointsBroken };
	EventPhysCreateEntityPart() { idval = id; idx = -1; }
	IPhysicalEntity* pEntNew;   //!< new physical entity (has type PE_RIGID)
	int              partidSrc; //!< original part id
	int              partidNew; //!< part id assigned to it in the new entity
	int              nTotParts; //!< total number of parts that broke off during this update (each will have its own event)
	int              bInvalid;  //!< generated mesh was invalid (degenerate or flipped)
	int              iReason;
	Vec3             breakImpulse; //!< impulse that initiated the breaking
	Vec3             breakAngImpulse;
	Vec3             v;            //!< initial vel of ejected product
	Vec3             w;            //!< initial ang vel of ejected product
	float            breakSize;    //!< if caused by an explosion, the explosion's rmin
	float            cutRadius;    //!< if updated mesh was successfully approximated with capsules, this is their cross section at the point of breakage
	Vec3             cutPtLoc[2];  //!< the cut's center in both entities' frames
	Vec3             cutDirLoc[2]; //!< the cut area's normal
	IGeometry*       pMeshNew;     //!< new mesh if was caused by boolean breaking, 0 if by joint breaking (i.e. no new mesh was created)
	bop_meshupdate*  pLastUpdate;  //!< last meshupdate for the moment the event was reported
	int              idx;          //!< used for event deferring by listeners
};

struct EventPhysRemoveEntityParts : EventPhysMono
{
	enum entype { id = 7, flagsCall = 1, flagsLog = 2 };
	EventPhysRemoveEntityParts() { idval = id; idOffs = 0; }
	unsigned int partIds[4]; //!< remove parts with ids corresponding to the set bits in partIds[], +idOffs
	int          idOffs;
	float        massOrg; //!< entity's mass before the parts were removed
};

struct EventPhysRevealEntityPart : EventPhysMono
{
	enum entype { id = 13, flagsCall = 1, flagsLog = 2 };
	EventPhysRevealEntityPart() { idval = id; }
	int partId; //!< id of a part that was hidden due to hierarchical breakability, but should be revealed now
};

struct EventPhysJointBroken : EventPhysStereo
{
	enum entype { id = 1, flagsCall = 1, flagsLog = 2 };
	EventPhysJointBroken() { idval = id; }
	int              idJoint;
	int              bJoint;          //!< structural joint if 1, dynamics constraint if 0
	int              partidEpicenter; //!< the "seed" part during the update
	Vec3             pt;              //!< joint's position in the entity frame
	Vec3             n;               //!< joint's z axis
	int              partid[2];
	int              partmat[2];    //!< material id from the parts' first primitive
	IPhysicalEntity* pNewEntity[2]; //!< only set for broken constraints
};

struct EventPhysRWIResult : EventPhysMono
{
	enum entype { id = 9, flagsCall = 0, flagsLog = 0 };
	EventPhysRWIResult() { idval = id; }
	int      (* OnEvent)(const EventPhysRWIResult*);
	ray_hit* pHits;
	int      nHits, nMaxHits;
	int      bHitsFromPool; //!< 1 if hits reside in the internal physics hits pool
};

struct EventPhysPWIResult : EventPhysMono
{
	enum entype { id = 10, flagsCall = 0, flagsLog = 0 };
	EventPhysPWIResult() { idval = id; }
	int   (* OnEvent)(const EventPhysPWIResult*);
	float dist;
	Vec3  pt;
	Vec3  n;
	int   idxMat;
	int   partId;
};

//! For callback phys areas.
struct EventPhysArea : EventPhysMono
{
	enum entype { id = 11, flagsCall = 0, flagsLog = 0 };
	EventPhysArea() { idval = id; }

	Vec3               pt;      //!< entity's center
	Vec3               ptref;   //!< for splines - closest point on spline to pt; for normal areas - area's world position
	Vec3               dirref;  //!< for splines, calculated force direction
	pe_params_buoyancy pb;      //!< can be filled by the caller
	Vec3               gravity; //!< can be filled by the caller
	IPhysicalEntity*   pent;    //!< the entity that entered the area and caused this event
};

struct EventPhysAreaChange : EventPhysMono
{
	enum entype { id = 12, flagsCall = 0, flagsLog = 0 };
	EventPhysAreaChange() { idval = id; pContainer = 0; }

	Vec3             boxAffected[2];
	quaternion       q;
	Vec3             pos;
	float            depth;
	IPhysicalEntity* pContainer;
	quaternion       qContainer;
	Vec3             posContainer;
};

struct EventPhysEntityDeleted : EventPhysMono
{
	enum entype { id = 14, flagsCall = 0, flagsLog = 0 };
	EventPhysEntityDeleted() { idval = id; }
	int mode;
	int isFromPOD;
};

struct EventPhysPostPump : EventPhys
{
	enum entype { id = 15, flagsCall = 0, flagsLog = 0 };
	EventPhysPostPump() {}
};

const int EVENT_TYPES_NUM = 16;

//! Physical entity iterator interface. This interface is used to traverse trough all the physical entities in an physical world.
//! In a way, this iterator works a lot like a stl iterator.
struct IPhysicalEntityIt
{
	// <interfuscator:shuffle>
	virtual ~IPhysicalEntityIt(){}
	virtual void             AddRef() = 0;
	virtual void             Release() = 0; //!< Deletes this iterator and frees any memory it might have allocated.

	virtual bool             IsEnd() = 0;     //!< Check whether current iterator position is the end position.
	virtual IPhysicalEntity* Next() = 0;      //!< returns the entity that the iterator points to before it goes to the next
	virtual IPhysicalEntity* This() = 0;      //!< returns the entity that the iterator points to
	virtual void             MoveFirst() = 0; //!< positions the iterator at the begining of the entity list
	// </interfuscator:shuffle>
};

//! Main interface to the physics implementation
//! \see IPhysicalEntity
struct IPhysicalWorld
{
	//! RayWorldIntersection - steps through the entity grid and raytraces entities
	//! traces a finite ray from org along dir, obtypes has uses enum entity_query_flags
	//! flags - see enum rwi_flags
	//! results are returned in hits. hits[0] is the solid hit, rest are pierceable (negative hit distance means no hit)
	//! a hit is solid if ray's pierceability (set in flags) is >= material's piercebility (set in SetSurfaceParameters)
	//! OnEvent - optional personal callback to be called *instead* of the global one
	//! returns the total amount of hits detected (solid and pierceable)
	//! iCaller specifies which set of global (thread) variables to use; 0..MAX_PHYS_THREADS-1 are reserved for the physics own threads
	//! collclass is the collision filter for ignoring entities
	struct SRWIParams
	{
		SRWIParams() { memset(this, 0, sizeof(*this)); objtypes = ent_all; flags = rwi_stop_at_pierceable; }

		const SRWIParams& Init(const Vec3& _org, const Vec3& _dir, int _objtypes, unsigned int _flags, const SCollisionClass& _collclass, ray_hit* _hits, int _nMaxHits,
		                       IPhysicalEntity** _pSkipEnts = 0, int _nSkipEnts = 0, void* _pForeignData = 0, int _iForeignData = 0, ray_hit_cached* _phitLast = 0,
		                       IPhysicalEntity** _pPortals = nullptr, int _nMaxPortals = 0)
		{
			memset(this, 0, sizeof(*this));
			org = _org;
			dir = _dir;
			objtypes = _objtypes;
			flags = _flags;
			collclass = _collclass;
			hits = _hits;
			nMaxHits = _nMaxHits;
			pForeignData = _pForeignData;
			iForeignData = _iForeignData;
			phitLast = _phitLast;
			pSkipEnts = _pSkipEnts;
			nSkipEnts = _nSkipEnts;
			pPortals = _pPortals;
			nMaxPortals = _nMaxPortals;
			return *this;
		}

		void*             pForeignData;
		int               iForeignData;
		int               (* OnEvent)(const EventPhysRWIResult*);
		Vec3              org;
		Vec3              dir;
		int               objtypes;
		unsigned int      flags;
		ray_hit*          hits;
		int               nMaxHits;
		ray_hit_cached*   phitLast;
		int               nSkipEnts;
		IPhysicalEntity** pSkipEnts;
		SCollisionClass   collclass;
		IPhysicalEntity** pPortals;	   // returns null-terminated list of encountered portals
		int               nMaxPortals; // max amount of returned portals (= list size - 1)
	};

	//! PrimitiveWorldIntersection  - similar to RayWorldIntersection, but does a primitive sweep (or overlap) check
	//! unlike RWI, it doesn't trace enitity cells along the path, but checks all of them in the swept volume's bounding box,
	//! so long PWIs are not recommended
	//! ppcontact - pointer to the pointer to the resulting contacts array, since it uses shared data it gets locked with lockContacts
	//! geomFlagsAll - flags that must all be present in an entity part
	//! geomFlagsAny - flags at least one of which must be present in an entity part
	//! pip - custom intersection parameters, overrides the sweepDir setting if any.
	//!   if not specified, the function performs a simple true/false overlap check if sweepDir is 0, and a sweep check otherwise
	//!   if specified and pip->bThreadSafe==false, lockContacts will keep the lock while in scope
	//! OnEvent - optional personal callback to be called *instead* of the global one
	//! returns distance to the first hit for sweep checks and the number of hits for intersection checks (as float)
	//! special note: if specified, **ppcontact has the collider entity id in iPrim[0], partid in iPrim[1], matid in id[1]
	//! special note #2: if nSkipEnts<0, pSkipEnts is used as a fixed list of entities to only test collisions against (the world is ignored)
	//! special note #3: if applicable, contacts stored in ppcontact will have their iNode[0] filled with the index to the feature's primitive.
	//! collclass is the collision filter for ignoring entities
	struct SPWIParams
	{
		SPWIParams()
		{
			memset(this, 0, sizeof(*this));
			entTypes = ent_all;
			geomFlagsAny = geom_colltype0 | geom_colltype_player;
			lockContacts.prw = &lockContacts.iActive;
		}

		void*                        pForeignData;
		int                          iForeignData;
		int                          (* OnEvent)(const EventPhysPWIResult*);
		int                          itype;
		const primitives::primitive* pprim;
		Vec3                         sweepDir;
		int                          entTypes;
		geom_contact**               ppcontact;
		int                          geomFlagsAll;
		int                          geomFlagsAny;
		intersection_params*         pip;
		int                          nSkipEnts;
		IPhysicalEntity**            pSkipEnts;
		SCollisionClass              collclass;
		WriteLockCond                lockContacts;
	};

	// <interfuscator:shuffle>
	virtual ~IPhysicalWorld(){}
	virtual void          Init() = 0;
	virtual void          Shutdown(int bDeleteGeometries = 1) = 0;
	virtual void          Release() = 0; //!< destroys the world

	virtual IGeomManager* GetGeomManager() = 0;
	virtual IPhysUtils*   GetPhysUtils() = 0;

	//! SetupEntityGrid: initializes entity hash grid for broad-phase collision detection and raytracing
	//! entity grid partitions the space along x and axes; grid's axisz can be aligned with any world axis (0-x,1-y,2-
	//! log2PODscale sets how many grid cells comprise one on-demand physicalization sector (on_demand_sector = 1<<log2PODscale)
	//! bCyclic==0 : entities that are outside the grid are all registered in one "exterior" cell
	//! bCyclic==1 : the grid is infinitely repeated, projected world coordinates only use lower bits to access grid cells
	virtual IPhysicalEntity* SetupEntityGrid(int axisz, Vec3 org, int nx, int ny, float stepx, float stepy, int log2PODscale = 0, int bCyclic = 0, 
		IPhysicalEntity* pHost = nullptr, const QuatT& posInHost = QuatT(IDENTITY)) = 0;

	//! Clean up memory pools after level was unloaded.
	//! should only be called when all outside references to physics where deleted.
	virtual void Cleanup() = 0;

	//! RegisterBBoxInPODGrid marks affected sectors as needing on-demand physicalization via IPhysicsStreamer->CreatePhysicalEntitiesInBox
	virtual void RegisterBBoxInPODGrid(const Vec3* BBox) = 0;
	virtual void UnregisterBBoxInPODGrid(const Vec3* BBox) = 0;
	virtual void DeactivateOnDemandGrid() = 0; //!< deactivates sector-based on-demand physicalization
	//! AddRefEntInPODGrid: makes sure the entity gets an on-demand AddRef for each on-demand sector it touches
	//! (needed when manually creating entities outside on-deman callback)
	virtual int AddRefEntInPODGrid(IPhysicalEntity* pent, const Vec3* BBox = 0) = 0;

	//! SetHeightfieldData - sets the global heightfield data. Uses callbacks to get height
	virtual IPhysicalEntity* SetHeightfieldData(const primitives::heightfield* phf, int* pMatMapping = 0, int nMats = 0) = 0;
	virtual IPhysicalEntity* GetHeightfieldData(primitives::heightfield* phf) = 0;
	virtual void             SetHeightfieldMatMapping(int* pMatMapping, int nMats) = 0;
	virtual PhysicsVars*     GetPhysVars() = 0; //!< returns a writable structure

	//! CreatePhysicalEntity - creates an entity with specified type, sets foreign data and optionally initializes if some pe_params
	//! if id is left <0, the physics will assign one (note: internally there's a id->ptr array map, so keep ids wihin some sane range)
	//! returns a valid entity pointer even if the physics thread is busy during the call
	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, pe_params* params = 0, void* pforeigndata = 0, int iforeigndata = 0, int id = -1, IGeneralMemoryHeap* pHeap = NULL) = 0;
	//! CreatePhysicalEntity - this version is used when creating an a temporary entity (lifeTime) on demand for a placeholder
	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params = 0, void* pForeignData = 0, int iForeignData = 0, int id = -1, IPhysicalEntity* pHostPlaceholder = 0, IGeneralMemoryHeap* pHeap = NULL) = 0;
	//! CreatePhysicalPlaceholder - placeholder is a lightweight structure that requests creation of a full entity
	//! when needed via IPhysicsStreamer->CreatePhysicalEntity
	virtual IPhysicalEntity* CreatePhysicalPlaceholder(pe_type type, pe_params* params = 0, void* pForeignData = 0, int iForeignData = 0, int id = -1) = 0;
	//! DestroyPhysicalEntity - moves the entity to the to-be-destroyed list (mode 0), destroys areas and placeholders
	//! mode 1-suspend the entity (unregister from all physics structures, but don't delete), 2-restore from suspended state
	//! mode & 4 - doesn't delete the entity if it is still referenced and returns 0
	//! even if this flag is not set, referenced entities (nRefCount>0) will not be physically deleted until freed
	//! request can get queued if the physics thread is busy, unless bThreadSafe is set
	virtual int              DestroyPhysicalEntity(IPhysicalEntity* pent, int mode = 0, int bThreadSafe = 0) = 0;

	virtual int              SetPhysicalEntityId(IPhysicalEntity* pent, int id, int bReplace = 1, int bThreadSafe = 0) = 0;
	virtual int              GetPhysicalEntityId(IPhysicalEntity* pent) = 0; //!< special values: NULL : -1, WORLD_ENTITY : -2;
	virtual IPhysicalEntity* GetPhysicalEntityById(int id) = 0;              //!< special values: -1 : terrain, -2 : default (empty) static entity; if bit 30 is set - don't expand placeholders into entities

	//! SetSurfaceParameters: sets parameters for surface_idx (0..511 currently)
	//! bounciness - restitution coefficient (for pair of surfaces k = sum of their coefficients, clamped to [0..1]
	//! friction - friction coefficient (for pair of surfaces k = sum of their coefficients, clamped to [0..inf)
	//! flags - see surface_flags enum
	virtual int SetSurfaceParameters(int surface_idx, float bounciness, float friction, unsigned int flags = 0) = 0;
	virtual int GetSurfaceParameters(int surface_idx, float& bounciness, float& friction, unsigned int& flags) = 0;
	virtual int SetSurfaceParameters(int surface_idx, float bounciness, float friction, float damage_reduction, float ric_angle, float ric_dam_reduction, float ric_vel_reduction, unsigned int flags = 0) = 0;
	virtual int GetSurfaceParameters(int surface_idx, float& bounciness, float& friction, float& damage_reduction, float& ric_angle, float& ric_dam_reduction, float& ric_vel_reduction, unsigned int& flags) = 0;

	//! TimeStep - the main world's function
	//! flags - entity types to update (ent_..; ent_deleted to purge deletion physics-on-demand state monitoring)
	virtual void  TimeStep(float time_interval, int flags = ent_all | ent_deleted) = 0;

	virtual float GetPhysicsTime() = 0;
	virtual int   GetiPhysicsTime() = 0; //!< physics time, quantized with PhysVars->timeGranularity
	virtual void  SetPhysicsTime(float time) = 0;
	virtual void  SetiPhysicsTime(int itime) = 0;

	virtual void  SetSnapshotTime(float time_snapshot, int iType = 0) = 0; //!< time corresponding to the current snapshot
	virtual void  SetiSnapshotTime(int itime_snapshot, int iType = 0) = 0;

	//! GetEntitiesInBox - uses entity grid to query entites in bbox (objtypes - see enum entity_query_flags)
	//! unless ent_allocate_list is set, returns pre-alocated internal list for iCaller==MAX_PHYS_THREADS
	//! otherwise, tries to use supplied pList up to szListPrealloc, allocates a new one if the size's not enough
	//! returns the number of entities
	virtual int GetEntitiesInBox(Vec3 ptmin, Vec3 ptmax, IPhysicalEntity**& pList, int objtypes, int szListPrealloc = 0) = 0;

	//! Performs a raycast through the physical world, returning possible hits into the provided hits structure
	//! \par Example
	//! \include CryPhysics/Examples/RayWorldIntersection.cpp
	virtual int RayWorldIntersection(const SRWIParams& rp, const char* pNameTag = RWI_NAME_TAG, int iCaller = MAX_PHYS_THREADS) = 0;
	//! Traces ray requests (rwi calls with rwi_queue set); logs and calls EventPhysRWIResult for each
	//! returns the number of rays traced
	virtual int  TracePendingRays(int bDoActualTracing = 1) = 0;

	virtual void ResetDynamicEntities() = 0; //!< Freezes (resets velocities of) all physical, living, and detached entities
	//! DestroyDynamicEntities - immediately destroys all physical, living, and detached entities; flushes the deleted entities
	//! All subsequent calls to DestroyPhysicalEntity for non-static entities are ignored until the next
	//! non-static entity is created
	virtual void               DestroyDynamicEntities() = 0;

	virtual void               PurgeDeletedEntities() = 0;             //!< Forces immediate physical deletion of all entities marked as deleted
	virtual int                GetEntityCount(int iEntType) = 0;       //!< iEntType is of pe_type
	virtual int                ReserveEntityCount(int nExtraEnts) = 0; //!< can prevent excessive internal lists re-allocations

	virtual IPhysicalEntityIt* GetEntitiesIterator() = 0;

	//! Simulates an explosion in the world, affecting entities near the epicenter (and within the specified radius)
	//! \par Example
	//! \include CryPhysics/Examples/SimulateExplosion.cpp
	virtual void               SimulateExplosion(pe_explosion* pexpl, IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0, int iTypes = ent_rigid | ent_sleeping_rigid | ent_living | ent_independent, int iCaller = MAX_PHYS_THREADS) = 0;

	//! RasterizeEntities - builds a depth map from physical gometries in a box specified by grid; only updates area affected by offsBBox +/- sizeBBox
	//! depth is z values, scaled and clamped so that the grid box's -size.z..+size.z is mapped to 0..255
	virtual void RasterizeEntities(const primitives::grid3d& grid, uchar* rbuf, int objtypes, float massThreshold, const Vec3& offsBBox, const Vec3& sizeBBox, int flags) = 0;

	//! DeformPhysicalEntity - applies boolean breaking for entity parts that have >=0 breakability index
	//! r is used to scale the corresponding explosion (boolean) shape
	virtual int   DeformPhysicalEntity(IPhysicalEntity* pent, const Vec3& ptHit, const Vec3& dirHit, float r, int flags = 0) = 0;
	//! UpdateDeformingEntities - normally this happens automatically during TimeStep; can be called manually for ex. during loading
	virtual void  UpdateDeformingEntities(float time_interval = 0.01f) = 0; //!< normally this happens during TimeStep
	//! CalculateExplosionExposure - uses occlusion grid from the last SimulateExplosion to calculate exposure
	virtual float CalculateExplosionExposure(pe_explosion* pexpl, IPhysicalEntity* pient) = 0;

	//! IsAffectedByExplosion - returns fraction of pent (0..1) that was exposed to the last explosion
	virtual float IsAffectedByExplosion(IPhysicalEntity* pent, Vec3* impulse = 0) = 0;

	//! AddExplosionShape - registers a boolean carving shape for breakable meashes
	//! size is the shape's 'characteristic' size; during the actual carving the caller provides a desired size,
	//! and the shape is scaled by desired_size/this_size.
	//! idmat is the breakability index this shape gets assigned to. if there are several shapes for one index,
	//! the ones with the size closest to the requested are selected, and among those one is selected randomly based on relative probability
	//! returns shape id, which can be used to unregister the shape later
	virtual int  AddExplosionShape(IGeometry* pGeom, float size, int idmat, float probability = 1.0f) = 0;
	virtual void RemoveExplosionShape(int id) = 0;
	virtual void RemoveAllExplosionShapes(void (* OnRemoveGeom)(IGeometry* pGeom) = 0) = 0;

	virtual void DrawPhysicsHelperInformation(IPhysRenderer* pRenderer, int iCaller = MAX_PHYS_THREADS) = 0;
	virtual void DrawEntityHelperInformation(IPhysRenderer* pRenderer, int iEntityId, int iDrawHelpers) = 0;

	//! CollideEntityWithBeam - does a sphere sweep test against an entity
	virtual int   CollideEntityWithBeam(IPhysicalEntity* _pent, Vec3 org, Vec3 dir, float r, ray_hit* phit) = 0;
	//! CollideEntityWithPrimitive - collides entity with a single primitive; does a sweep check if dir is non-0
	virtual int   CollideEntityWithPrimitive(IPhysicalEntity* _pent, int itype, primitives::primitive* pprim, Vec3 dir, ray_hit* phit, intersection_params* pip = 0) = 0;
	virtual int   RayTraceEntity(IPhysicalEntity* pient, Vec3 origin, Vec3 dir, ray_hit* pHit, pe_params_pos* pp = 0, unsigned int geomFlagsAny = geom_colltype0 | geom_colltype_player) = 0; //!< like RWI, but for one entity

	virtual float PrimitiveWorldIntersection(const SPWIParams& pp, WriteLockCond* pLockContacts = 0, const char* pNameTag = PWI_NAME_TAG) = 0;
	virtual void  GetMemoryStatistics(ICrySizer* pSizer) = 0;

	virtual void  SetPhysicsStreamer(IPhysicsStreamer* pStreamer) = 0;          //!< sets the callbacks for on-demand creation
	virtual void  SetPhysicsEventClient(IPhysicsEventClient* pEventClient) = 0; //!< obsolete
	virtual float GetLastEntityUpdateTime(IPhysicalEntity* pent) = 0;           //!< simulation class-based, not actually per-entity
	virtual int   GetEntityProfileInfo(phys_profile_info*& pList) = 0;
	virtual int   GetFuncProfileInfo(phys_profile_info*& pList) = 0;
	virtual int   GetGroupProfileInfo(phys_profile_info*& pList) = 0;
	virtual int   GetJobProfileInfo(phys_job_info*& pList) = 0;

	//! AddEventClient - adds a phyisics event listener
	//! bLogged==1 for a logged event version, 0 for immediate
	//! listeners are called in the order of decreasing priorities
	//! if a listener returns 0, further event processing stops
	virtual void             AddEventClient(int type, int (* func)(const EventPhys*), int bLogged, float priority = 1.0f) = 0;
	virtual int              RemoveEventClient(int type, int (* func)(const EventPhys*), int bLogged) = 0;
	virtual void             PumpLoggedEvents() = 0; //!< calls event clients for logged events
	virtual uint32           GetPumpLoggedEventsTicks() = 0;
	virtual void             ClearLoggedEvents() = 0;
	virtual int              NotifyEventClients(EventPhys* pEvent, int bLogged) = 0; // immediately calls listeners for the event; returns the sum of their return results

	virtual IPhysicalEntity* AddGlobalArea() = 0; //!< adds a global phys area or returns an existing one
	//! AddArea - adds a 2d-contour area. Computes the best fitting plane for the points and projects them on it
	//! zmin and zmax are the dimensions along this plane's normal
	//! if set, pTessIdx is the surface's triangulation (3 idx per tri), used if the points are not close enough to the plane
	//! pFlows is per-vertex (0..npt-1) water flow vectors
	virtual IPhysicalEntity* AddArea(Vec3* pt, int npt, float zmin, float zmax, const Vec3& pos = Vec3(0, 0, 0), const quaternionf& q = quaternionf(IDENTITY), float scale = 1.0f, const Vec3& normal = Vec3(ZERO), int* pTessIdx = 0, int nTessTris = 0, Vec3* pFlows = 0) = 0;
	//! AddArea - creates a general geometry area. Typically, primitives work best, but can be a mesh or a heightfield
	//! In the latter case, it chages water plane in buoyancy params based on local height perturbation (used for waves)
	virtual IPhysicalEntity* AddArea(IGeometry* pGeom, const Vec3& pos, const quaternionf& q, float scale) = 0;
	//! AddArea - this adds a 3d-spline area
	virtual IPhysicalEntity* AddArea(Vec3* pt, int npt, float r, const Vec3& pos = Vec3(0, 0, 0), const quaternionf& q = quaternionf(IDENTITY), float scale = 1) = 0;
	//! GetNextArea: iterates through all registered areas, if prevarea==0 returns the global area
	virtual IPhysicalEntity* GetNextArea(IPhysicalEntity* pPrevArea = 0) = 0;
	virtual int              CheckAreas(const Vec3& ptc, Vec3& gravity, pe_params_buoyancy* pb, int nMaxBuoys = 1, int iMedium = -1, const Vec3& vec = Vec3(ZERO), IPhysicalEntity* pent = 0, int iCaller = MAX_PHYS_THREADS) = 0; //!< checks areas for a given point (ptc)

	virtual void             SetWaterMat(int imat) = 0; //!< material to use for water hits in RWI with ent_water
	virtual int              GetWaterMat() = 0;
	virtual int              SetWaterManagerParams(pe_params* params) = 0; //!< pe_params_waterman; creates water manager if none is present and nExtraTiles is set
	virtual int              GetWaterManagerParams(pe_params* params) = 0;
	virtual int              GetWatermanStatus(pe_status* status) = 0; //!< pe_status_waterman
	virtual void             DestroyWaterManager() = 0;

	virtual volatile int*    GetInternalLock(int idx) = 0; //!< returns one of phys_locks locks

	virtual int              SerializeWorld(const char* fname, int bSave) = 0; //!< saves/loads the world state (without geometries) in a text file
	virtual int              SerializeGeometries(const char* fname, int bSave) = 0;

	virtual void             SerializeGarbageTypedSnapshot(TSerialize ser, int iSnapshotType, int flags) = 0;

	virtual void             SavePhysicalEntityPtr(TSerialize ser, IPhysicalEntity* pent) = 0;
	virtual IPhysicalEntity* LoadPhysicalEntityPtr(TSerialize ser) = 0;
	virtual void             GetEntityMassAndCom(IPhysicalEntity* pIEnt, float& mass, Vec3& com) = 0;

	virtual EventPhys*       AddDeferredEvent(int type, EventPhys* event) = 0;

	virtual void*			 GetInternalImplementation(int type, void* object = nullptr) = 0; // get native data from actual physics implementation
	// </interfuscator:shuffle>

	//! Performs a raycast through the physical world, returning possible hits into the provided hits structure
	//! \par Example
	//! \include CryPhysics/Examples/RayWorldIntersection.cpp
	inline int RayWorldIntersection(const Vec3& org, const Vec3& dir, int objtypes, unsigned int flags, ray_hit* hits, int nMaxHits,
	                                IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0, void* pForeignData = 0, int iForeignData = 0,
	                                const char* pNameTag = RWI_NAME_TAG, ray_hit_cached* phitLast = 0, int iCaller = MAX_PHYS_THREADS)
	{
		SRWIParams rp;
		rp.org = org;
		rp.dir = dir;
		rp.objtypes = objtypes;
		rp.flags = flags;
		rp.hits = hits;
		rp.nMaxHits = nMaxHits;
		rp.pForeignData = pForeignData;
		rp.iForeignData = iForeignData;
		rp.phitLast = phitLast;
		rp.pSkipEnts = pSkipEnts;
		rp.nSkipEnts = nSkipEnts;
		return RayWorldIntersection(rp, pNameTag, iCaller);
	}
	//! Performs a raycast through the physical world, returning possible hits into the provided hits structure
	//! \par Example
	//! \include CryPhysics/Examples/RayWorldIntersection.cpp
	int RayWorldIntersection(const Vec3& org, const Vec3& dir, int objtypes, unsigned int flags, ray_hit* hits, int nMaxHits,
	                         IPhysicalEntity* pSkipEnt, IPhysicalEntity* pSkipEntAux = 0, void* pForeignData = 0, int iForeignData = 0)
	{
		IPhysicalEntity* pSkipEnts[2];
		int nSkipEnts = 0;
		if (pSkipEnt) pSkipEnts[nSkipEnts++] = pSkipEnt;
		if (pSkipEntAux) pSkipEnts[nSkipEnts++] = pSkipEntAux;
		return RayWorldIntersection(org, dir, objtypes, flags, hits, nMaxHits, pSkipEnts, nSkipEnts, pForeignData, iForeignData);
	}
	void SimulateExplosion(Vec3 epicenter, Vec3 epicenterImp, float rmin, float rmax, float r, float impulsivePressureAtR,
	                       int nOccRes = 0, int nGrow = 0, float rminOcc = 0.1f, IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0,
	                       int iTypes = ent_rigid | ent_sleeping_rigid | ent_living | ent_independent)
	{
		pe_explosion expl;
		expl.epicenter = epicenter;
		expl.epicenterImp = epicenterImp;
		expl.rmin = rmin;
		expl.rmax = rmax;
		expl.r = r;
		expl.impulsivePressureAtR = impulsivePressureAtR;
		expl.nOccRes = nOccRes;
		expl.nGrow = nGrow;
		expl.rminOcc = rminOcc;
		SimulateExplosion(&expl, pSkipEnts, nSkipEnts, iTypes);
	}
	float PrimitiveWorldIntersection(int itype, const primitives::primitive* pprim, const Vec3& sweepDir = Vec3(ZERO), int entTypes = ent_all,
	                                 geom_contact** ppcontact = 0, int geomFlagsAll = 0, int geomFlagsAny = geom_colltype0 | geom_colltype_player, intersection_params* pip = 0,
	                                 void* pForeignData = 0, int iForeignData = 0, IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0, WriteLockCond* pLockContacts = 0, const char* pNameTag = PWI_NAME_TAG)
	{
		SPWIParams pp;
		pp.itype = itype;
		pp.pprim = pprim;
		pp.sweepDir = sweepDir;
		pp.entTypes = entTypes;
		pp.ppcontact = ppcontact;
		pp.geomFlagsAll = geomFlagsAll;
		pp.geomFlagsAny = geomFlagsAny;
		pp.pip = pip;
		pp.pForeignData = pForeignData;
		pp.iForeignData = iForeignData;
		pp.pSkipEnts = pSkipEnts;
		pp.nSkipEnts = nSkipEnts;
		return PrimitiveWorldIntersection(pp, pLockContacts, pNameTag);
	}

};

#endif
