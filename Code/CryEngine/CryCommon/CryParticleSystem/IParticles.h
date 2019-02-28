// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IParticles.h
//  Version:     v1.00
//  Created:     2008-02-26 by Scott Peter
//  Compilers:   Visual Studio.NET
//  Description: Interfaces to particle types
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IRenderNode.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Color.h>
#include <CryMemory/IMemory.h>
#include <CrySystem/TimeValue.h>
#include <CryAudio/IAudioSystem.h>
#include <Cry3DEngine/GeomRef.h>
#include <CryCore/CryVariant.h>

class SmartScriptTable;

//! Real-time params to control particle emitters.

SERIALIZATION_DECLARE_ENUM(EParticleSpec,
	Default  = 0,
	Low      = CONFIG_LOW_SPEC,
	Medium   = CONFIG_MEDIUM_SPEC,
	High     = CONFIG_HIGH_SPEC,
	VeryHigh = CONFIG_VERYHIGH_SPEC,
	XBoxOne  = CONFIG_DURANGO,
	PS4      = CONFIG_ORBIS
);

//! Real-time params to control particle emitters.
//! Some parameters override emitter params.
struct SpawnParams
{
	bool                     bPrime          = false;                            //!< Advance emitter age to its equilibrium state.

	// Placement options
	bool                     bIgnoreVisAreas = false;                            //!< Renders in all VisAreas.
	bool                     bRegisterByBBox = false;                            //!< Registers in any overlapping portal VisArea.
	bool                     bNowhere        = false;                            //!< Exists outside of level.
	bool                     bPlaced         = false;                            //!< Loaded from placed entity.

	float                    fCountScale     = 1;                                //!< Multiple for particle count (on top of bCountPerUnit if set).
	float                    fSizeScale      = 1;                                //!< Multiple for all effect sizes.
	float                    fSpeedScale     = 1;                                //!< Multiple for particle emission speed.
	float                    fTimeScale      = 1;                                //!< Multiple for emitter time evolution.
	float                    fPulsePeriod    = 0;                                //!< How often to restart emitter.
	float                    fStrength       = -1;                               //!< Controls parameter strength curves.
	int                      nSeed           = -1;                               //!< Initial seed. Default is -1 which means random seed.

	EParticleSpec            eSpec           = EParticleSpec::Default;           //!< Overrides particle spec for this emitter
	EGeomType                eAttachType     = GeomType_None;                    //!< What type of object particles emitted from.
	EGeomForm                eAttachForm     = GeomForm_Surface;                 //!< What aspect of shape emitted from.
	bool                     bCountPerUnit   = false;                            //!< Multiply particle count also by geometry extent (length/area/volume).

	bool                     bEnableAudio    = true;                             //!< Used by particle effect instances to indicate whether audio should be updated or not.
	CryAudio::EOcclusionType occlusionType   = CryAudio::EOcclusionType::Ignore; //!< Audio obstruction/occlusion calculation type.
	string                   audioRtpc;                                          //!< Indicates what audio RTPC this particle effect instance drives.

	void Serialize(Serialization::IArchive& ar)
	{
		ar(bPrime, "prime", "Prime");                                  ar.doc("Advance emitter age to its equilibrium state");
		ar(fSizeScale, "scale", "Uniform Scale");                      ar.doc("Emitter uniform scale");
		ar(fCountScale, "countScale", "Count Scale");                  ar.doc("Particle count multiplier");
		ar(fSpeedScale, "speedScale", "Speed Scale");                  ar.doc("Particle emission speed multiplier");
		ar(fTimeScale, "timeScale", "Time Scale");                     ar.doc("Emitter time evolution multiplier");
		ar(eSpec, "spec", "Particle Spec");                            ar.doc("Overrides System Spec for this emitter");
		ar(bIgnoreVisAreas, "ignoreVvisAreas", "Ignore Vis Areas");    ar.doc("Renders in all VisAreas");

		bool bOverrideSeed = true;
		if (ar.isEdit())
		{
			bOverrideSeed = nSeed != -1;
			ar(bOverrideSeed, "customseed", "Custom Random Seed");
			ar.doc("Override the seed used to generate randomness within the particle. If false we will pick a random value on each run.");
			if (ar.isInput())
			{
				if (bOverrideSeed && nSeed == -1)
				{
					nSeed = 0;
				}
				else if (!bOverrideSeed && nSeed != -1)
				{
					nSeed = -1;
				}
			}
		}

		if (bOverrideSeed)
		{
			if (ar.isEdit())
			{
				int seed = nSeed;
				ar(seed, "seed", "Random Seed");
				if (!isneg(seed))
				{
					nSeed = seed;
				}
			}
			else
			{
				ar(nSeed, "seed", "Random Seed");
			}
		}
	}
};

struct ParticleTarget
{
	Vec3  vTarget;      //!< Target location for particles.
	Vec3  vVelocity;    //!< Velocity of moving target.
	float fRadius;      //!< Size of target, for orbiting.

	// Options.
	bool bTarget;       //!< Target is enabled.
	bool bPriority;     //!< This target takes priority over others (attractor entities).

	ParticleTarget()
	{
		memset(this, 0, sizeof(*this));
	}
};

struct EmitParticleData
{
	IStatObj*        pStatObj;      //!< The displayable geometry object for the entity. If NULL, uses emitter settings for sprite or geometry.
	IPhysicalEntity* pPhysEnt;      //!< A physical entity which controls the particle. If NULL, uses emitter settings to physicalize or move particle.
	QuatTS           Location;      //!< Specified location for particle.
	Velocity3        Velocity;      //!< Specified linear and rotational velocity for particle.
	bool             bHasLocation;  //!< Location is specified.
	bool             bHasVel;       //!< Velocities are specified.

	EmitParticleData()
		: pStatObj(0), pPhysEnt(0), Location(IDENTITY), Velocity(ZERO), bHasLocation(false), bHasVel(false)
	{
	}
};

struct ParticleParams;

struct ParticleLoc : QuatTS
{
	ParticleLoc(const QuatTS& qts)
		: QuatTS(qts) {}
	ParticleLoc(const Matrix34& mx)
		: QuatTS(mx) {}

	ParticleLoc(const Vec3& pos, const Vec3& dir = Vec3(0, 0, 1), float scale = 1.f)
		: QuatTS(IDENTITY, pos, scale)
	{
		// ParticleLoc currently uses pfx1 behavior
		SetYToDir(q, dir);
	}

	static void SetZToDir(Quat& q, const Vec3& dir)
	{
		// pfx2 orientation system: Z is focus.
		if (dir.IsZero())
			q.SetIdentity();
		else
			q.SetRotationV0V1(Vec3(0, 0, 1), dir);
	}

	static void SetYToDir(Quat& q, const Vec3& dir)
	{
		// pfx1 orientation system: Y is focus.
		q.SetRotationVDir(dir);
	}

	static void RotateZtoY(Quat& q)
	{
		SetYToDir(q, q.GetColumn2());
	}

	static void RotateYtoZ(Quat& q)
	{
		SetZToDir(q, q.GetColumn1());
	}
};

//! Interface to control particle attributes

// Execute common code for all attribute types.
// Similiar in function to std::visit, but allows a separate type variable to be used,
// and doesn't require the construction of an external functor class.
// This could be done without a macro, but only if auto parameters are allowed in lambdas,
// which is not yet supported on all compilers.
#define DO_FOR_ATTRIBUTE_TYPE(type, T, code) \
	switch (type) \
	{ \
	case IParticleAttributes::ET_Boolean: { using T = bool;   code; break; } \
	case IParticleAttributes::ET_Integer: { using T = int;    code; break; } \
	case IParticleAttributes::ET_Float:   { using T = float;  code; break; } \
	case IParticleAttributes::ET_Color:   { using T = ColorB; code; break; } \
	} \

struct IParticleAttributes
{
	typedef uint TAttributeId;

	enum EType
	{
		ET_Boolean,
		ET_Integer,
		ET_Float,
		ET_Color,

		ET_Count,
	};

	typedef CryVariant<bool, int, float, ColorB> TVariant;

	struct TValue: TVariant
	{
		using TVariant::TVariant;
		using TVariant::operator=;

		// Redeclaring default constructors required, because gcc is confused by the ColorF constructors below
		TValue()                                       {}
		TValue(const TValue& in)                       : TVariant(static_cast<const TVariant&>(in)) {}
		TValue& operator=(const TValue& in)            { TVariant::operator=(static_cast<const TVariant&>(in)); return *this; }

		TValue(const ColorF& in)                       : TVariant(ColorB(in)) {}
		TValue& operator=(const ColorF& in)            { emplace<ColorB>(ColorB(in)); return *this; }

		explicit TValue(EType type)                    { SetType(type); }

		template<typename T> T        get() const      { return stl::get<T>(*this); }
		template<typename T> T&       get()            { return stl::get<T>(*this); }
		template<typename T> const T* get_if() const   { return stl::get_if<T>(this); }
		template<typename T> T*       get_if()         { return stl::get_if<T>(this); }

		EType                         Type() const     { return (EType)index(); }
		void                          SetType(EType type)
		{
			DO_FOR_ATTRIBUTE_TYPE(type, T, *this = T());
		}

		template<typename T> void Serialize(Serialization::IArchive& ar, cstr name, cstr label, T defVal = T())
		{
			const T* pVal = get_if<T>();
			T val = pVal ? *pVal : defVal;

			ar(val, name, label);

			if (ar.isInput())
				*this = val;
		}

		void Serialize(Serialization::IArchive& ar, EType type, cstr name, cstr label)
		{
			DO_FOR_ATTRIBUTE_TYPE(type, T, Serialize<T>(ar, name, label));
		}

		void Serialize(Serialization::IArchive& ar)
		{
			EType type = Type();
			ar(type, "Type", "^>85>");
			Serialize(ar, type, "Value", "^");
		}
	};

	virtual void          Reset(const IParticleAttributes* pCopySource = nullptr) = 0;
	virtual void          Serialize(Serialization::IArchive& ar) = 0;
	virtual void          TransferInto(IParticleAttributes* pReceiver) const = 0;
	virtual TAttributeId  FindAttributeIdByName(cstr name) const = 0;
	virtual uint          GetNumAttributes() const = 0;
	virtual cstr          GetAttributeName(TAttributeId idx) const = 0;
	virtual EType         GetAttributeType(TAttributeId idx) const = 0;
	virtual const TValue& GetValue(TAttributeId idx) const = 0;
	virtual TValue        GetValue(TAttributeId idx, const TValue& defaultValue) const = 0;
	virtual bool          SetValue(TAttributeId idx, const TValue& value) = 0;

	template<typename T> T GetValueAs(TAttributeId idx, T defaultValue) const
	{
		return GetValue(idx, defaultValue).template get<T>();
	}
	
	const TValue& GetValue(cstr name) const
	{
		return GetValue(FindAttributeIdByName(name));
	}
	bool SetValue(cstr name, const TValue& value)
	{
		return SetValue(FindAttributeIdByName(name), value);
	}

	// Deprecated
	bool         GetAsBoolean(TAttributeId id, bool defaultValue = false) const         { return GetValueAs(id, defaultValue); }
	int          GetAsInteger(TAttributeId id, int defaultValue = 0) const              { return GetValueAs(id, defaultValue); }
	float        GetAsFloat(TAttributeId id, float defaultValue = 0) const              { return GetValueAs(id, defaultValue); }
	ColorB       GetAsColorB(TAttributeId id, ColorB defaultValue = ColorB(0U)) const   { return GetValueAs(id, defaultValue); }
	ColorF       GetAsColorF(TAttributeId id, ColorF defaultValue = ColorF(0.0f)) const { return GetValueAs(id, ColorB(defaultValue)); }

	void         SetAsBoolean(TAttributeId id, bool value) { SetValue(id, value); }
	void         SetAsInteger(TAttributeId id, int value)  { SetValue(id, value); }
	void         SetAsFloat(TAttributeId id, float value)  { SetValue(id, value); }
	void         SetAsColor(TAttributeId id, ColorB value) { SetValue(id, value); }
	void         SetAsColor(TAttributeId id, ColorF value) { SetValue(id, ColorB(value)); }
};

SERIALIZATION_ENUM_IMPLEMENT(IParticleAttributes::EType,
	Boolean,
	Integer,
	Float,
	Color
);
typedef std::shared_ptr<IParticleAttributes> TParticleAttributesPtr;

namespace pfx2
{
	struct TParticleFeatures;
};

//! Interface to control a particle effect.
//! This interface is used by I3DEngine::CreateParticleEffect to control a particle effect.
//! It is created by CreateParticleEffect method of 3d engine.
struct IParticleEffect : public _i_reference_target_t
{
	typedef ::ParticleLoc ParticleLoc;

	// <interfuscator:shuffle>

	virtual int  GetVersion() const = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	//! Spawns this effect.
	//! \param qLoc World location to place emitter.
	//! \param SpawnParams Emitter flags and options.
	//! \return Spawned emitter, or 0 if unable.
	virtual struct IParticleEmitter* Spawn(const ParticleLoc& loc, const SpawnParams* pSpawnParams = NULL) = 0;

	//! Compatibility versions.
	IParticleEmitter* Spawn(const ParticleLoc& loc, const SpawnParams& sp)
	{
		return Spawn(loc, &sp);
	}

	//! Spawn a particle at the specified location.
	//! \param bIndependent Deprecated: auto-serialization now occurs when external reference count = 0.
	IParticleEmitter* Spawn(bool bIndependent, const ParticleLoc& loc)
	{
		return Spawn(loc);
	}

	//! Sets a new name to this particle effect.
	//! \param sFullName Full name of this effect, including library and group qualifiers.
	virtual void SetName(cstr sFullName) = 0;

	//! Gets the name of this particle effect.
	//! \return A C-string which holds the minimally qualified name of this effect.
	//! \note For top level effects, the return value includes library.group qualifier.
	//! \note For child effects, the return value includes only the base name.
	virtual cstr GetName() const = 0;

	//! Gets the name of this particle effect.
	//! \return A stack_string which holds the fully qualified name of this effect, with all parents and library.
	virtual stack_string GetFullName() const = 0;

	//! Enables or disables the effect.
	//! \param bEnabled Set to true to enable the effect or to false to disable it.
	virtual void SetEnabled(bool bEnabled) = 0;

	//! Determines if the effect is enabled.
	enum ECheckOptions { eCheckChildren = 1, eCheckConfig = 2, eCheckFeatures = 4 };
	virtual bool IsEnabled(uint options = 0) const = 0;

	//! Returns true if this is a run-time only unsaved effect.
	virtual bool IsTemporary() const = 0;

	//! Sets the particle parameters.
	virtual void SetParticleParams(const ParticleParams& params) = 0;

	//! Gets the particle parameters.
	//! \return An object of the type ParticleParams which contains several parameters.
	virtual const ParticleParams& GetParticleParams() const = 0;

	//! Get the set of ParticleParams used as the default for this effect, for serialization, display, etc
	virtual const ParticleParams& GetDefaultParams() const = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Child particle systems.
	//////////////////////////////////////////////////////////////////////////

	//! Gets the number of sub particles children.
	//! \return An integer representing the amount of sub particles children
	virtual int GetChildCount() const = 0;

	//! Get sub Particle's child by index.
	//! \param index The index of a particle child.
	//! \return A pointer to a IParticleEffect derived object.
	virtual IParticleEffect* GetChild(int index) const = 0;

	//! Removes all child particles.
	virtual void ClearChilds() = 0;

	//! Inserts a child particle effect at a precise slot.
	//! \param slotInteger value which specify the desired slot.
	//! \param pEffect Pointer to the particle effect to insert.
	virtual void InsertChild(int slot, IParticleEffect* pEffect) = 0;

	//! Finds in which slot a child particle effect is stored.
	//! \param pEffect Pointer to the child particle effect.
	//! \return Integer representing the slot number or -1 if the slot is not found.
	virtual int FindChild(IParticleEffect* pEffect) const = 0;

	//! Remove effect from current parent, and set new parent.
	//! \param pParent New parent, may be 0.
	virtual void SetParent(IParticleEffect* pParent) = 0;

	//! Gets the particles effect parent, if any.
	//! \return Pointer representing the particles effect parent.
	virtual IParticleEffect* GetParent() const = 0;

	//! Loads all resources needed for a particle effects.
	//! \return true if any resources loaded.
	virtual bool LoadResources() = 0;

	//! Unloads all resources previously loaded.
	virtual void UnloadResources() = 0;

	//! Serializes particle effect to/from XML.
	//! \param bLoading true when loading, false for saving.
	//! \param bChildren When true, also recursively serializes effect children.
	virtual void Serialize(XmlNodeRef node, bool bLoading, bool bChildren) = 0;

	virtual void Serialize(Serialization::IArchive& ar) = 0;

	//! Reloads the effect from the particle database.
	//! \param bChildren When true, also recursively reloads effect children.
	virtual void Reload(bool bChildren) = 0;

	// </interfuscator:shuffle>
};

//! Interface to a particle effect emitter.
//! An IParticleEmitter should usually be created by I3DEngine::CreateParticleEmitter.
//! Deleting the emitter should be done using I3DEngine::DeleteParticleEmitter.
struct IParticleEmitter : public IRenderNode, public CMultiThreadRefCount
{
	// <interfuscator:shuffle>

	virtual int GetVersion() const = 0;

	//! Returns whether emitter still alive in engine.
	virtual bool IsAlive() const = 0;

	//! Sets emitter state to active or inactive. Emitters are initially active.
	//! if bActive is true, Emitter updates and emits as normal, and deletes itself if limited lifetime.
	//! if bActive is false, stops all emitter updating and particle emission.
	//! Existing particles continue to update and render. Emitter is not deleted.
	virtual void Activate(bool bActive) = 0;

	//! Removes emitter and all particles instantly.
	virtual void Kill() = 0;

	//!Restarts the emitter from scratch (if active).
	//! Any existing particles are re-used oldest first.
	virtual void Restart() = 0;

	//! Set the effects used by the particle emitter.
	//! Will define the effect used to spawn the particles from the emitter.
	//! \param pEffect A pointer to an IParticleEffect object.
	//! \note Never call this function if you already used SetParams.
	virtual void SetEffect(const IParticleEffect* pEffect) = 0;

	//! Returns particle effect assigned on this emitter.
	virtual const IParticleEffect* GetEffect() const = 0;

	//! Sets attachment geometry for spawning.
	virtual void SetEmitGeom(const GeomRef& geom) = 0;

	//! Specifies how particles are emitted from source.
	virtual void SetSpawnParams(const SpawnParams& spawnParams) = 0;

	void         SetSpawnParams(const SpawnParams& spawnParams, const GeomRef& geom)
	{
		SetSpawnParams(spawnParams);
		SetEmitGeom(geom);
	}

	//! Retrieves current SpawnParams.
	virtual void GetSpawnParams(SpawnParams& sp) const = 0;
	SpawnParams  GetSpawnParams() const
	{
		SpawnParams sp;
		GetSpawnParams(sp);
		return sp;
	}

	//! Associates emitter with entity, for dynamic updating of positions etc.
	//! \note Must be done when entity created or serialized, entity association is not serialized.
	virtual void SetEntity(IEntity* pEntity, int nSlot) = 0;

	virtual void InvalidateCachedEntityData() = 0;

	//! Sets location with quat-based orientation.
	//! \note IRenderNode.SetMatrix() is equivalent, but performs conversion.
	virtual QuatTS GetLocation() const = 0;
	virtual void   SetLocation(const QuatTS& loc) = 0;

	// Attractors.
	virtual void SetTarget(const ParticleTarget& target) = 0;

	//! Updates emitter's state to current time.
	virtual void Update() = 0;

	//! Programmatically adds particle to emitter for rendering.
	//! With no arguments, spawns particles according to emitter settings.
	//! Specific objects can be passed for programmatic control.
	//! \param pData Specific data for particle, or NULL for defaults.
	virtual void EmitParticle(const EmitParticleData* pData = NULL) = 0;

	void         EmitParticle(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt = NULL, QuatTS* pLocation = NULL, Velocity3* pVel = NULL)
	{
		EmitParticleData data;
		data.pStatObj = pStatObj;
		data.pPhysEnt = pPhysEnt;
		if (pLocation)
		{
			data.Location = *pLocation;
			data.bHasLocation = true;
		}
		if (pVel)
		{
			data.Velocity = *pVel;
			data.bHasVel = true;
		}
		EmitParticle(&data);
	}

	//! Get the Entity ID that this particle emitter is attached to.
	virtual unsigned int GetAttachedEntityId() = 0;

	//! Get the Entity Slot that this particle emitter is attached to.
	virtual int GetAttachedEntitySlot() = 0;

	// pfx2 base interface
	//! Get Particle Attributes
	virtual IParticleAttributes& GetAttributes() = 0;

	//! Set additional Features on the emitter
	virtual void SetEmitterFeatures(pfx2::TParticleFeatures& features) { }

	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////
//! \cond INTERNAL
//! A callback interface for a class that wants to be aware when particle emitters are being created/deleted.
struct IParticleEffectListener
{
	// <interfuscator:shuffle>
	virtual ~IParticleEffectListener() {}
	//! This callback is called when a new particle emitter is created.
	//! \param pEmitter Created Emitter.
	//! \param bIndependent
	//! \param mLoc Location of the emitter.
	//! \param pEffect Particle effect.
	virtual void OnCreateEmitter(IParticleEmitter* pEmitter) = 0;

	//! This callback is called when a particle emitter is deleted.
	//! \param pEmitter Emitter being deleted.
	virtual void OnDeleteEmitter(IParticleEmitter* pEmitter) = 0;
	// </interfuscator:shuffle>
};
//! \endcond

//////////////////////////////////////////////////////////////////////////

// General particle stats
template<typename F>
struct TElementCountsBase
{
	F alloc = 0, alive = 0, updated = 0, rendered = 0;
};
template<typename F>
struct TElementCounts
	: INumberVector<F, 4, TElementCounts<F>>
	, TElementCountsBase<F>
{
};

// pfx1 particle stats
template<typename F>
struct TContainerCountsBase
{
	TElementCounts<F> components;
	struct SubEmitterCounts
	{
		F updated = 0;
	} subemitters;
	struct ParticleCounts : TElementCounts<F>
	{
		F reiterate = 0, reject = 0, clip = 0, collideTest = 0, collideHit = 0;
	} particles;
	TElementCounts<F> pixels;
};

template<typename F>
struct TContainerCounts
	: INumberVector<float, 18, TContainerCounts<F>>
	, TContainerCountsBase<F>
{
};

template<typename F>
struct TParticleCounts
	: INumberVector<float, 25, TParticleCounts<F>>
	, TContainerCountsBase<F>
{
	TElementCounts<F> emitters;
	struct VolumeStats
	{
		F stat = 0, dyn = 0, error = 0;
	} volume;
};

typedef TContainerCounts<float> SContainerCounts;
typedef TParticleCounts<float> SParticleCounts;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct IParticleManager
{
	// <interfuscator:shuffle>
	virtual ~IParticleManager() {}
	//////////////////////////////////////////////////////////////////////////
	// ParticleEffects
	//////////////////////////////////////////////////////////////////////////

	//! Set the ParticleEffect used as defaults for new effects, and when reading effects from XML
	//! If pEffect has children, they will be used if they match the current sys_spec, or particle version number.
	//! \param pEffect The effect containing the default params.
	virtual void SetDefaultEffect(const IParticleEffect* pEffect) = 0;

	//! Get the ParticleEffect tree used as defaults for new effects.
	virtual const IParticleEffect* GetDefaultEffect() = 0;

	//! Get the ParticleParams tree used as defaults for new effects, for the current sys_spec.
	//! \param nVersion Particle library version to select for. Use 0 for the latest version.
	virtual const ParticleParams& GetDefaultParams(int nVersion = 0) const = 0;

	//! Creates a new empty particle effect.
	//! \return Pointer to a object derived from IParticleEffect.
	virtual IParticleEffect* CreateEffect() = 0;

	//! Deletes a specified particle effect.
	//! \param pEffect Pointer to the particle effect object to delete.
	virtual void DeleteEffect(IParticleEffect* pEffect) = 0;

	//! Searches by name for a particle effect.
	//! \param sEffectName The fully qualified name (with library prefix) of the particle effect to search.
	//! \param bLoad Whether to load the effect's assets if found.
	//! \param sSource Optional client context, for diagnostics.
	//! \return Pointer to a particle effect matching the specified name, or NULL if not found.
	virtual IParticleEffect* FindEffect(cstr sEffectName, cstr sSource = "", bool bLoadResources = true) = 0;

	//! Creates a particle effect from an XML node. Overwrites any existing effect of the same name.
	//! \param sEffectName  The name of the particle effect to be created.
	//! \param effectNode XML structure describing the particle effect properties.
	//! \param bLoadResources Indicates if the resources for this effect should be loaded.
	//! \return Pointer to the particle effect.
	virtual IParticleEffect* LoadEffect(cstr sEffectName, XmlNodeRef& effectNode, bool bLoadResources, const cstr sSource = NULL) = 0;

	//! Loads a library of effects from an XML description.
	virtual bool LoadLibrary(cstr sParticlesLibrary, XmlNodeRef& libNode, bool bLoadResources) = 0;
	virtual bool LoadLibrary(cstr sParticlesLibrary, cstr sParticlesLibraryFile = NULL, bool bLoadResources = false) = 0;

	//////////////////////////////////////////////////////////////////////////
	// ParticleEmitters
	//////////////////////////////////////////////////////////////////////////

	//! Creates a new particle emitter, with custom particle params instead of a library effect.
	//! \param bIndependent
	//! \param mLoc World location of emitter.
	//! \param Params Programmatic particle params.
	//! \return Pointer to an object derived from IParticleEmitter
	virtual IParticleEmitter* CreateEmitter(const ParticleLoc& loc, const ParticleParams& params, const SpawnParams* pSpawnParams = NULL) = 0;

	//! Deletes a specified particle emitter.
	//! \param pPartEmitter Specify the emitter to delete.
	virtual void DeleteEmitter(IParticleEmitter* pEmitter) = 0;

	//! Deletes all particle emitters which match a filter
	//! \param filter Boolean function determining whether to delete emitter.
	typedef std::function<bool (IParticleEmitter&)> FEmitterFilter;
	virtual void DeleteEmitters(FEmitterFilter filter) = 0;

	//! Reads or writes emitter state -- creates emitter if reading.
	//! \param ser Serialization context.
	//! \param pEmitter Emitter to save if writing, NULL if reading.
	//! \return pEmitter if writing, newly created emitter if reading.
	virtual IParticleEmitter* SerializeEmitter(TSerialize ser, IParticleEmitter* pEmitter = NULL) = 0;

	// Processing.
	virtual void Update() = 0;
	virtual void RenderDebugInfo() = 0;
	virtual void Reset() = 0;
	virtual void ClearRenderResources(bool bForceClear) = 0;
	virtual void ClearDeferredReleaseResources() = 0;
	virtual void OnFrameStart() = 0;
	virtual void Serialize(TSerialize ser) = 0;
	virtual void PostSerialize(bool bReading) = 0;

	//! Set the timer used to update the particle system.
	//! \param pTimer Specify the timer.
	virtual void SetTimer(ITimer* pTimer) = 0;

	//! Stats.
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
	virtual void GetCounts(SParticleCounts& counts) = 0;
	virtual void DisplayStats(Vec2& location, float lineHeight) = 0;

	//! PerfHUD.
	virtual void CreatePerfHUDWidget() = 0;

	//! Registers new particle events listener.
	virtual void AddEventListener(IParticleEffectListener* pListener) = 0;
	virtual void RemoveEventListener(IParticleEffectListener* pListener) = 0;

	//! Prepare all date for Job Particle*::Render Tasks.
	virtual void PrepareForRender(const SRenderingPassInfo& passInfo) = 0;

	//! Finish up all Particle*::Render Tasks.
	virtual void FinishParticleRenderTasks(const SRenderingPassInfo& passInfo) = 0;

	//! Add a container to collect which take the most memory in the vertex/index pools.
	virtual void AddVertexIndexPoolUsageEntry(uint32 nVertexMemory, uint32 nIndexMemory, const char* pContainerName) = 0;
	virtual void MarkAsOutOfMemory() = 0;

	//! Light profile counts.
	virtual struct SParticleLightProfileCounts& GetLightProfileCounts() = 0;

	// </interfuscator:shuffle>
};

struct SParticleLightProfileCounts
{
	SParticleLightProfileCounts() { ResetFrameTicks(); }

	void   AddFrameTicks(uint64 nTicks)     { m_nFrameTicks += nTicks; }
	void   ResetFrameTicks()                { m_nFrameTicks = 0; m_nFrameSyncTicks = 0; }
	uint64 NumFrameTicks() const            { return m_nFrameTicks; }
	void   AddFrameSyncTicks(uint64 nTicks) { m_nFrameSyncTicks += nTicks; }
	uint64 NumFrameSyncTicks() const        { return m_nFrameSyncTicks; }

	uint64 m_nFrameTicks;
	uint64 m_nFrameSyncTicks;
};

#if defined(ENABLE_LW_PROFILERS)
class CParticleLightProfileSection
{
public:
	CParticleLightProfileSection()
		: m_nTicks(CryGetTicks())
	{
	}
	~CParticleLightProfileSection()
	{
		IParticleManager* pPartMan = gEnv->p3DEngine->GetParticleManager();
		IF (pPartMan != NULL, 1)
		{
			pPartMan->GetLightProfileCounts().AddFrameTicks(CryGetTicks() - m_nTicks);
		}
	}
private:
	uint64 m_nTicks;
};

class CParticleLightProfileSectionSyncTime
{
public:
	CParticleLightProfileSectionSyncTime()
		: m_nTicks(CryGetTicks())
	{}
	~CParticleLightProfileSectionSyncTime()
	{
		IParticleManager* pPartMan = gEnv->p3DEngine->GetParticleManager();
		IF (pPartMan != NULL, 1)
		{
			pPartMan->GetLightProfileCounts().AddFrameSyncTicks(CryGetTicks() - m_nTicks);
		}
	}
private:
	uint64 m_nTicks;
};

	#define PARTICLE_LIGHT_PROFILER()      CParticleLightProfileSection _particleLightProfileSection;
	#define PARTICLE_LIGHT_SYNC_PROFILER() CParticleLightProfileSectionSyncTime _particleLightSyncProfileSection;
#else
	#define PARTICLE_LIGHT_PROFILER()
	#define PARTICLE_LIGHT_SYNC_PROFILER()
#endif

enum EPerfHUD_ParticleDisplayMode
{
	PARTICLE_DISP_MODE_NONE = 0,
	PARTICLE_DISP_MODE_PARTICLE,
	PARTICLE_DISP_MODE_EMITTER,
	PARTICLE_DISP_MODE_FILL,
	PARTICLE_DISP_MODE_NUM,
};

struct SParticleShaderData
{
	SParticleShaderData()
	{
		m_expansion[0] = m_expansion[1] = 1;
		m_curvature = 0.0f;
		m_textureFrequency = 1.0f;

		m_tileSize[0] = m_tileSize[1] = 1;
		m_frameCount = 1;
		m_firstTile = 0;

		const float alphaTest[2][4] = { { 0, 0, 1, 0 }, { 1, 0, 1, 0 } };
		memcpy(m_alphaTest, alphaTest, sizeof(m_alphaTest));
		m_softnessMultiplier = 1;
		m_sphericalApproximation = 0;
		m_thickness = 0.0f;
		m_axisScale = 0.0f;

		m_diffuseLighting = 1.0f;
		m_emissiveLighting = 0.0f;
		m_backLighting = 0;
	}

	// This could possibly become a cbuffer at some point.
	union
	{
		float m_params[4];
		struct
		{
			float m_expansion[2];
			float m_curvature;
			float m_textureFrequency;
		};
	};

	union
	{
		float m_textureTileSize[4];
		struct
		{
			float m_tileSize[2];
			float m_frameCount;
			float m_firstTile;
		};
	};

	union
	{
		float m_alphaTest[2][4];
		struct
		{
			float m_alphaTestMin[4];
			float m_alphaTestMax[4];
		};
	};

	union
	{
		float m_softness[4];
		struct
		{
			float m_softnessMultiplier;
			float m_sphericalApproximation;
			float m_thickness;
			float m_axisScale;
		};
	};

	union
	{
		float m_lightParams[4];
		struct
		{
			float m_diffuseLighting;
			float m_emissiveLighting;
			float m_backLighting;
		};
	};
};
