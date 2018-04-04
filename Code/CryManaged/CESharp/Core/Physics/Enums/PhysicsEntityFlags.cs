using System;

namespace CryEngine
{
	/// In here all the enums from physinterface.h are wrapped and commented so they can be used in C#.
	/// For easier usage the enums are split up in multiple enums so the wrong enum can't be used for the wrong parameter.

	/// <summary>
	/// Physicalization flags specifically for Particle-entities.
	/// </summary>
	[Flags]
	public enum PhysicsParticleFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// Full stop after first contact.
		/// </summary>
		SingleContact = 0x01,
		/// <summary>
		/// Forces constant orientation.
		/// </summary>
		ConstantOrientation = 0x02,
		/// <summary>
		/// 'sliding' mode; entity's 'normal' vector axis will be alinged with the ground normal.
		/// </summary>
		NoRoll = 0x04,
		/// <summary>
		/// Unless set, entity's y axis will be aligned along the movement trajectory.
		/// </summary>
		NoPathAlignment = 0x08,
		/// <summary>
		/// Disables spinning while flying.
		/// </summary>
		ParticleNoSpin = 0x10,
		/// <summary>
		/// Disables collisions with other particles.
		/// </summary>
		NoSelfCollisions = 0x100,
		/// <summary>
		/// Particle will not add hit impulse (expecting that some other system will).
		/// </summary>
		NoImpulse = 0x200
	}

	/// <summary>
	/// Physicalization flags specifically for Living-entities.
	/// </summary>
	[Flags]
	public enum PhysicsLivingFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// Push objects during contacts.
		/// </summary>
		PushObjects = 0x01,
		/// <summary>
		/// Push players during contacts.
		/// </summary>
		PushPlayers = 0x02,
		/// <summary>
		/// Quantizes velocities after each step (was used in MP for precise deterministic sync).
		/// </summary>
		SnapVelocities = 0x04,
		/// <summary>
		/// Don't do additional intersection checks after each step (recommended for NPCs to improve performance).
		/// </summary>
		LoosenStuckChecks = 0x08,
		/// <summary>
		/// Unless set, 'grazing' contacts are not reported.
		/// </summary>
		ReportSlidingContacts = 0x10
	}

	/// <summary>
	/// Physicalization flags specifically for Rope-entities
	/// </summary>
	[Flags]
	public enum PhysicsRopeFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// Approximate velocity of the parent object as v = (pos1-pos0)/time_interval.
		/// </summary>
		FindiffAttachedVel = 0x01,
		/// <summary>
		/// No velocity solver; will rely on stiffness (if set) and positional length enforcement.
		/// </summary>
		NoSolver = 0x02,
		/// <summary>
		/// No collisions with objects the rope is attached to.
		/// </summary>
		IgnoreAttachments = 0x4,
		/// <summary>
		/// Whether target vertices are set in the parent entity's frame.
		/// </summary>
		TargetVtxRel0 = 0x08,
		/// <summary>
		/// Whether target vertices are set in the parent entity's frame.
		/// </summary>
		TargetVtxRel1 = 0x10,
		/// <summary>
		/// Turns on 'dynamic subdivision' mode (only in this mode contacts in a strained state are handled correctly).
		/// </summary>
		SubdivideSegs = 0x100,
		/// <summary>
		/// Rope will not tear when it reaches its force limit, but stretch.
		/// </summary>
		NoTears = 0x200,
		/// <summary>
		/// Rope will collide with objects other than the terrain.
		/// </summary>
		Collides = 0x200000,
		/// <summary>
		/// Rope will collide with the terrain.
		/// </summary>
		CollidesWithTerrain = 0x400000,
		/// <summary>
		/// Rope will collide with the objects it's attached to even if the other collision flags are not set.
		/// </summary>
		CollidesWithAttachment = 0x80,
		/// <summary>
		/// Rope will use stiffness 0 if it has contacts.
		/// </summary>
		NoStiffnessWhenColliding = 0x10000000
	}

	/// <summary>
	/// Physicalization flags specifically for Soft-entities
	/// </summary>
	[Flags]
	public enum PhysicsSoftFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// The longest edge in each triangle with not participate in the solver
		/// </summary>
		SkipLongestEdges = 0x01,
		/// <summary>
		/// Soft body will have an additional rigid body core
		/// </summary>
		RigidCore = 0x02, 
	}

	/// <summary>
	/// Physicalization flags specifically for Rigid-entities. Note that Articulated-entities and WheeledVehicle-entities are derived from it.
	/// </summary>
	[Flags]
	public enum PhysicsRigidFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// Will not generate EventPhysCollisions when contacting water
		/// </summary>
		NoSplashes = 0x04,
		/// <summary>
		/// Entity will trace rays against alive characters; set internally unless overriden
		/// </summary>
		SmallAndFast = 0x100
	}

	/// <summary>
	/// Physicalization flags specifically for Articulated-entities.
	/// </summary>
	[Flags]
	public enum PhysicsArticulatedFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// Specifies an entity that contains a pre-baked physics simulation
		/// </summary>
		RecordedPhysics = 0x02,
	}

	/// <summary>
	/// Physicalization flags specifically for WheeledVehicle-entities.
	/// </summary>
	[Flags]
	public enum PhysicsWheeledVehicleFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// Exclude wheels between the first and the last one from the solver
		/// (only wheels with non-0 suspension are considered)
		/// </summary>
		FakeInnerWheels = 0x08,
	}

	/// <summary>
	/// General flags for PhysicsEntity-parameters
	/// </summary>
	[Flags]
	public enum PhysicsEntityFlags
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x0,
		/// <summary>
		/// Each entity part will be registered separately in the entity grid
		/// </summary>
		TraceableParts = 0x10,
		/// <summary>
		/// Entity will not be simulated
		/// </summary>
		Disabled = 0x20,
		/// <summary>
		/// Entity will not break or deform other objects
		/// </summary>
		NeverBreak = 0x40,
		/// <summary>
		/// Entity undergoes dynamic breaking/deforming
		/// </summary>
		Deforming = 0x80,
		/// <summary>
		/// Entity can be pushed by players
		/// </summary>
		PushableByPlayers = 0x200,
		/// <summary>
		/// Entity is registered in the entity grid
		/// </summary>
		Traceable = 0x400,
		/// <summary>
		/// Entity is registered in the entity grid
		/// </summary>
		ParticleTraceable = 0x400,
		/// <summary>
		/// Entity is registered in the entity grid
		/// </summary>
		RopeTraceable = 0x400,
		/// <summary>
		/// Only entities with this flag are updated if ent_flagged_only is used in TimeStep()
		/// </summary>
		Update = 0x800,
		/// <summary>
		/// Generate immediate events for simulation class changed (typically rigid bodies falling asleep)
		/// </summary>
		MonitorStateChanges = 0x1000,
		/// <summary>
		/// Generate immediate events for collisions
		/// </summary>
		MonitorCollisions = 0x2000,
		/// <summary>
		/// Generate immediate events when something breaks nearby
		/// </summary>
		MonitorEnvChanges = 0x4000,
		/// <summary>
		/// Don't generate events when moving through triggers
		/// </summary>
		NeverAffectTriggers = 0x8000,
		/// <summary>
		/// Will apply certain optimizations for invisible entities
		/// </summary>
		Invisible = 0x10000,
		/// <summary>
		/// Entity will ignore global water area
		/// </summary>
		IgnoreOcean = 0x20000,
		/// <summary>
		/// Entity will force its damping onto the entire group
		/// </summary>
		FixedDamping = 0x40000,
		/// <summary>
		/// Entity will generate immediate post step events
		/// </summary>
		MonitorPoststep = 0x80000,
		/// <summary>
		/// When deleted, entity will awake objects around it even if it's not referenced (has refcount = 0)
		/// </summary>
		AlwaysNotifyOnDeletion = 0x100000,
		/// <summary>
		/// Entity will ignore breakImpulseScale in PhysVars
		/// </summary>
		OverrideImpulseScale = 0x200000,
		/// <summary>
		/// Players can break the Entiy by bumping into it
		/// </summary>
		PlayersCanBreak = 0x400000,
		/// <summary>
		/// Entity will never trigger 'squashed' state when colliding with players
		/// </summary>
		CannotSquashPlayers = 0x10000000,
		/// <summary>
		/// Entity will ignore phys areas (gravity and water)
		/// </summary>
		IgnoreAreas = 0x800000,
		/// <summary>
		/// Entity will log simulation class change events
		/// </summary>
		LogStateChanges = 0x1000000,
		/// <summary>
		/// Entity will log collision events
		/// </summary>
		LogCollisions = 0x2000000,
		/// <summary>
		/// Entity will log EventPhysEnvChange when something breaks nearby
		/// </summary>
		LogEnvChanges = 0x4000000,
		/// <summary>
		/// Entity will log EventPhysPostStep events
		/// </summary>
		LogPoststep = 0x8000000,
	}
}
