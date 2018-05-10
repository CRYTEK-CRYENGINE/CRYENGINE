// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryScriptSystem/IScriptSystem.h>

struct IEntity;
struct ISystem;

#include <CryPhysics/IPhysics.h>
#include <CryParticleSystem/ParticleParams.h>

#define ENTITYPROP_CASTSHADOWS   0x00000001
#define ENTITYPROP_DONOTCHECKVIS 0x00000002

struct GoalParams;

/*!
 *	<description>In this class are all entity-related script-functions implemented in order to expose all functionalities provided by an entity.</description>
 *	<remarks>These function will never be called from C-Code. They're script-exclusive.</remarks>
 */
class CScriptBind_Entity : public CScriptableBase
{
public:
	CScriptBind_Entity(IScriptSystem* pSS, ISystem* pSystem);

	void         DelegateCalls(IScriptTable* pInstanceTable);

	void         GoalParamsHelper(IFunctionHandler* pH, uint32 index, GoalParams& node);

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddContainer(m_areaPoints);

	}
protected:
	//! <code>Entity.DeleteThis()</code>
	//! <description>Deletes this entity.</description>
	int DeleteThis(IFunctionHandler* pH);

	//! <code>Entity.CreateRenderProxy()</code>
	//! <description>Create a proxy render object for the entity, allows entity to be rendered without loading any assets in immediately.</description>
	int CreateRenderProxy(IFunctionHandler* pH);

	//! <code>Entity.UpdateShaderParamCallback()</code>
	//! <description>Check all the currently set shader param callbacks on the renderproxy with the current state of the entity.</description>
	int CheckShaderParamCallbacks(IFunctionHandler* pH);

	//! <code>Entity.LoadObject( nSlot, sFilename )</code>
	//! <description>Load CGF geometry into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="sFilename">CGF geometry file name.</param>
	int LoadObject(IFunctionHandler* pH, int nSlot, const char* sFilename);

	//! <code>Entity.LoadObject( nSlot, sFilename, nFlags )</code>
	//! <description>Load CGF geometry into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="sFilename">CGF geometry file name.</param>
	//!		<param name="nFlags">entity load flags</param>
	int LoadObjectWithFlags(IFunctionHandler* pH, int nSlot, const char* sFilename, const int nFlags);

	//! <code>Entity.LoadObjectLattice( nSlot )</code>
	//! <description>Load lattice into the entity slot.</description>
	//    <param name="nSlot">Slot identifier.</param>
	int LoadObjectLattice(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.LoadSubObject( nSlot, sFilename, sGeomName )</code>
	//! <description>Load geometry of one CGF node into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="sFilename">CGF geometry file name.</param>
	//!		<param name="sGeomName">Name of the node inside CGF geometry.</param>
	int LoadSubObject(IFunctionHandler* pH, int nSlot, const char* sFilename, const char* sGeomName);

	//! <code>Entity.LoadCharacter( nSlot, sFilename )</code>
	//! <description>Load CGF geometry into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="sFilename">CGF geometry file name.</param>
	int LoadCharacter(IFunctionHandler* pH, int nSlot, const char* sFilename);

	//! <code>Entity.LoadGeomCache( int nSlot,const char *sFilename )</code>
	//! <description>Load geom cache into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="sFilename">CAX file name.</param>
	int LoadGeomCache(IFunctionHandler* pH, int nSlot, const char* sFilename);

	//! <code>Entity.LoadLight( nSlot, lightTable )</code>
	//! <description>Load CGF geometry into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="lightTable">Table with all the light information.</param>
	int LoadLight(IFunctionHandler* pH, int nSlot, SmartScriptTable table);

	//! <code>Entity.SetLightColorParams( nSlot, color, specular_multiplier)</code>
	//! <description>changes the color related params of an existing light.</description>
	int SetLightColorParams(IFunctionHandler* pH, int nSlot, Vec3 color, float specular_multiplier);

	//! <code>Entity.UpdateLightClipBounds( nSlot )</code>
	//! <description>Update the clip bounds of the light from the linked entities.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	int UpdateLightClipBounds(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.SetLightCasterException( nLightSlot, nGeometrySlot )</code>
	//! <description>Entity render node will be a caster exception for light loaded in nLightSlot.
	//!		<param name="nLightSlot">Slot where our light is loaded.</param>
	int SetSelfAsLightCasterException(IFunctionHandler* pH, int nLightSlot);

	//! <code>Entity.LoadCloudBlocker( nSlot, table )</code>
	//! <description>Loads the properties of cloud blocker into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="table">Table with cloud blocker properties.</param>
	int LoadCloudBlocker(IFunctionHandler* pH, int nSlot, SmartScriptTable table);

	//! <code>Entity.LoadFogVolume( nSlot, table )</code>
	//! <description>Loads the fog volume XML file into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="table">Table with fog volume properties.</param>
	int LoadFogVolume(IFunctionHandler* pH, int nSlot, SmartScriptTable table);

	//! <code>Entity.FadeGlobalDensity( nSlot, fadeTime, newGlobalDensity )</code>
	//! <description>Sets the fade global density.</description>
	//!		<param name="nSlot">nSlot identifier.</param></param>
	//!		<param name="fadeTime">.</param>
	//!		<param name="newGlobalDensity">.</param>
	int FadeGlobalDensity(IFunctionHandler* pH, int nSlot, float fadeTime, float newGlobalDensity);

	//! <code>Entity.LoadParticleEffect( nSlot, sEffectName, fPulsePeriod, bPrime, fScale )</code>
	//! <description>Loads CGF geometry into the entity slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="sEffectName">Name of the particle effect (Ex: "explosions/rocket").</param>
	//!		<param name="(optional) bPrime">Whether effect starts fully primed to equilibrium state.</param>
	//!		<param name="(optional) fPulsePeriod">Time period between particle effect restarts.</param>
	//!		<param name="(optional) fScale">Size scale to apply to particles</param>
	//!		<param name="(optional) fCountScale">Count multiplier to apply to particles</param>
	//!		<param name="(optional) bScalePerUnit">Scale size by attachment extent</param>
	//!		<param name="(optional) bCountPerUnit">Scale count by attachment extent</param>
	//!		<param name="(optional) sAttachType">string for EGeomType</param>
	//!		<param name="(optional) sAttachForm">string for EGeomForm</param>
	int LoadParticleEffect(IFunctionHandler* pH, int nSlot, const char* sEffectName, SmartScriptTable table);

	//! <code>Entity.PreLoadParticleEffect( sEffectName )</code>
	//! <description>Pre-loads a particle effect.</description>
	//!		<param name="sEffectName">Name of the particle effect (Ex: "explosions/rocket").</param>
	int PreLoadParticleEffect(IFunctionHandler* pH, const char* sEffectName);

	//! <code>Entity.IsSlotParticleEmitter( slot )</code>
	//! <description>Checks if the slot is a particle emitter.</description>
	//!		<param name="slot">Slot identifier.</param>
	int IsSlotParticleEmitter(IFunctionHandler* pH, int slot);

	//! <code>Entity.IsSlotLight( slot )</code>
	//! <description>Checks if the slot is a light.</description>
	//!		<param name="slot">Slot identifier.</param>
	int IsSlotLight(IFunctionHandler* pH, int slot);

	//! <code>Entity.IsSlotGeometry( slot )</code>
	//! <description>Checks if the slot is a geometry.</description>
	//!		<param name="slot">Slot identifier.</param>
	int IsSlotGeometry(IFunctionHandler* pH, int slot);

	//! <code>Entity.IsSlotCharacter( slot )</code>
	//! <description>Checks if the slot is a character.</description>
	//!		<param name="slot">Slot identifier.</param>
	int IsSlotCharacter(IFunctionHandler* pH, int slot);

	//////////////////////////////////////////////////////////////////////////
	// Slots.
	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.GetSlotCount()</code>
	//! <description>Gets the count of the slots.</description>
	int GetSlotCount(IFunctionHandler* pH);

	//! <code>Entity.GetSlotPos( nSlot )</code>
	//! <description>Gets the slot position.</description>
	//!		<param name="nSlot">nSlot identifier.</param>
	int GetSlotPos(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.SetSlotPos( nSlot, v )</code>
	//! <description>Sets the slot position.</description>
	//!		<param name="nSlot">nSlot identifier.</param>
	//!		<param name="v">Position to be set.</param>
	int SetSlotPos(IFunctionHandler* pH, int slot, Vec3 v);

	//! <code>Entity.SetSlotPosAndDir( nSlot, pos, dir )</code>
	//! <description>Sets the slot position and direction.</description>
	//!		<param name="nSlot">nSlot identifier.</param>
	//!		<param name="pos">Position to be set.</param>
	//!		<param name="dir">Direction to be set.</param>
	int SetSlotPosAndDir(IFunctionHandler* pH, int nSlot, Vec3 pos, Vec3 dir);

	//! <code>Entity.GetSlotAngles( nSlot )</code>
	//! <description>Gets the slot angles.</description>
	//!		<param name="nSlot">nSlot identifier.</param>
	int GetSlotAngles(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.GetSlotAngles( nSlot, v )</code>
	//! <description>Sets the slot angles.</description>
	//!		<param name="nSlot">nSlot identifier.</param>
	//!		<param name="v">Angle to be set.</param>
	int SetSlotAngles(IFunctionHandler* pH, int nSlot, Ang3 v);

	//! <code>Entity.GetSlotScale( nSlot )</code>
	//! <description>Gets the slot scale amount.</description>
	//!		<param name="nSlot">nSlot identifier.</param>
	int GetSlotScale(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.SetSlotScale( nSlot, fScale )</code>
	//! <description>Sets the slot scale amount.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="fScale">Scale amount for the slot.</param>
	int SetSlotScale(IFunctionHandler* pH, int nSlot, float fScale);

	//! <code>Entity.IsSlotValid( nSlot )</code>
	//! <description>Checks if the slot is valid.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	int IsSlotValid(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.CopySlotTM( destSlot, srcSlot, includeTranslation )</code>
	//! <description>Copies the TM (Transformation Matrix) of the slot.</description>
	//!		<param name="destSlot">Destination slot identifier.</param>
	//!		<param name="srcSlot">Source slot identifier.</param>
	//!		<param name="includeTranslation">True to include the translation, false otherwise.</param>
	int CopySlotTM(IFunctionHandler* pH, int destSlot, int srcSlot, bool includeTranslation);

	//! <code>Entity.MultiplyWithSlotTM( slot, pos )</code>
	//! <description>Multiplies with the TM (Transformation Matrix) of the slot.</description>
	//!		<param name="slot">Slot identifier.</param>
	//!		<param name="pos">Position vector.</param>
	int MultiplyWithSlotTM(IFunctionHandler* pH, int slot, Vec3 pos);

	//! <code>Entity.SetSlotWorldTM( nSlot, pos, dir )</code>
	//! <description>Sets the World TM (Transformation Matrix) of the slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="pos">Position vector.</param>
	//!		<param name="dir">Direction vector.</param>
	int SetSlotWorldTM(IFunctionHandler* pH, int nSlot, Vec3 pos, Vec3 dir);

	//! <code>Entity.GetSlotWorldPos( nSlot )</code>
	//! <description>Gets the World position of the slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	int GetSlotWorldPos(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.GetSlotWorldDir( nSlot )</code>
	//! <description>Gets the World direction of the slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	int GetSlotWorldDir(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.SetSlotHud3D( nSlot )</code>
	//! <description>Setup flags for use as 3D HUD entity.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	int SetSlotHud3D(IFunctionHandler* pH, int nSlot);

	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.SetPos( vPos )</code>
	//! <description>Sets the position of the entity.</description>
	//!		<param name="vPos">Position vector.</param>
	int SetPos(IFunctionHandler* pH, Vec3 vPos);

	//! <code>Entity.GetPos()</code>
	//! <description>Gets the position of the entity.</description>
	int GetPos(IFunctionHandler* pH);

	//! <code>Entity.SetAngles( vAngles )</code>
	//!		<param name="vAngles">Angle vector.</param>
	//! <description>Sets the angle of the entity.</description>
	int SetAngles(IFunctionHandler* pH, Ang3 vAngles);

	//! <code>Entity.GetAngles()</code>
	//! <description>Gets the angle of the entity.</description>
	int GetAngles(IFunctionHandler* pH);

	//! <code>Entity.SetScale( fScale )</code>
	//!		<param name="fScale">Scale amount.</param>
	//! <description>Sets the scaling value for the entity.</description>
	int SetScale(IFunctionHandler* pH, float fScale);

	//! <code>Entity.GetScale()</code>
	//! <description>Gets the scaling value for the entity.</description>
	int GetScale(IFunctionHandler* pH);

	//! <code>Entity.GetCenterOfMassPos()</code>
	//! <description>Gets the position of the entity center of mass.</description>
	int GetCenterOfMassPos(IFunctionHandler* pH);

	//! <code>Entity.GetWorldBoundsCenter()</code>
	//! <description>Gets the world bbox center for the entity (defaults to entity position if no bbox present).</description>
	int GetWorldBoundsCenter(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////
	//! <code>Entity.SetLocalPos( vPos )</code>
	int SetLocalPos(IFunctionHandler* pH, Vec3 vPos);

	//! <code>Vec3 Entity.GetLocalPos()</code>
	int GetLocalPos(IFunctionHandler* pH);

	//! <code>Entity.SetLocalAngles( vAngles )</code>
	int SetLocalAngles(IFunctionHandler* pH, Ang3 vAngles);

	//! <code>Vec3 Entity.GetLocalAngles( vAngles )</code>
	int GetLocalAngles(IFunctionHandler* pH);

	//! <code>Entity.SetLocalScale( fScale )</code>
	int SetLocalScale(IFunctionHandler* pH, float fScale);

	//! <code>float Entity.GetLocalScale()</code>
	int GetLocalScale(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////
	//! <code>Entity.SetWorldPos( vPos )</code>
	int SetWorldPos(IFunctionHandler* pH, Vec3 vPos);

	//! <code>Vec3 Entity.GetWorldPos()</code>
	int GetWorldPos(IFunctionHandler* pH);

	//! <code>Vec3 Entity.GetWorldDir()</code>
	int GetWorldDir(IFunctionHandler* pH);

	//! <code>Entity.SetWorldAngles( vAngles )</code>
	int SetWorldAngles(IFunctionHandler* pH, Ang3 vAngles);

	//! <code>Vec3 Entity.GetWorldAngles( vAngles )</code>
	int GetWorldAngles(IFunctionHandler* pH);

	//! <code>Entity.SetWorldScale( fScale )</code>
	int SetWorldScale(IFunctionHandler* pH, float fScale);

	//! <code>float Entity.GetWorldScale()</code>
	int GetWorldScale(IFunctionHandler* pH);

	//! <code>float Entity.GetBoneLocal( boneName, trgDir )</code>
	int GetBoneLocal(IFunctionHandler* pH, const char* boneName, Vec3 trgDir);

	//! <code>Ang3 Entity.CalcWorldAnglesFromRelativeDir( dir )</code>
	int CalcWorldAnglesFromRelativeDir(IFunctionHandler* pH, Vec3 dir);

	//! <code>float Entity.IsEntityInside(entityId)</code>
	int IsEntityInside(IFunctionHandler* pH, ScriptHandle entityId);

	//////////////////////////////////////////////////////////////////////////

	//! <code>float Entity.GetDistance( entityId )</code>
	//! <returns>The distance from  entity specified with entityId/</returns>
	int GetDistance(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////
	//! <code>Entity.DrawSlot( nSlot )</code>
	//! <description>Enables/Disables drawing of object or character at specified slot of the entity.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	//!		<param name="nEnable">1-Enable drawing, 0-Disable drawing.</param>
	int DrawSlot(IFunctionHandler* pH, int nSlot, int nEnable);

	//////////////////////////////////////////////////////////////////////////
	//! <code>Entity.IgnorePhysicsUpdatesOnSlot( nSlot )</code>
	//! <description>Ignore physics when it try to update the position of a slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	int IgnorePhysicsUpdatesOnSlot(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.FreeSlot( nSlot )</code>
	//! <description>Delete all objects from specified slot.</description>
	//!		<param name="nSlot">Slot identifier.</param>
	int FreeSlot(IFunctionHandler* pH, int nSlot);

	//! <code>Entity.FreeAllSlots()</code>
	//! <description>Delete all objects on every slot part of the entity.</description>
	int FreeAllSlots(IFunctionHandler* pH);

	//! <code>Entity.GetCharacter( nSlot )</code>
	//! <description>Gets the character for the specified slot if there is any.</description>
	int GetCharacter(IFunctionHandler* pH, int nSlot);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Physics.
	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.DestroyPhysics()</code>
	int DestroyPhysics(IFunctionHandler* pH);

	//! <code>Entity.EnablePhysics( bEnable )</code>
	int EnablePhysics(IFunctionHandler* pH, bool bEnable);

	//! <code>Entity.ResetPhysics()</code>
	int ResetPhysics(IFunctionHandler* pH);

	//! <code>Entity.AwakePhysics( nAwake )</code>
	int AwakePhysics(IFunctionHandler* pH, int nAwake);

	//! <code>Entity.AwakeCharacterPhysics( nSlot, sRootBoneName, nAwake )</code>
	int AwakeCharacterPhysics(IFunctionHandler* pH, int nSlot, const char* sRootBoneName, int nAwake);

	//! <code>Entity.Physicalize( int nSlot,int nPhysicsType,table physicsParams )</code>
	//! <description>
	//!    Create physical entity from the specified entity slot.
	//!      <para>
	//!          Physics Type        Meaning
	//!          -------------       -----------
	//!          PE_NONE             No physics.
	//!          PE_STATIC           Static physical entity.
	//!          PE_LIVING           Live physical entity (Players,Monsters).
	//!          PE_RIGID            Rigid body physical entity.
	//!          PE_WHEELEDVEHICLE   Physical vechicle with wheels.
	//!          PE_PARTICLE         Particle physical entity, it only have mass and radius.
	//!          PE_ARTICULATED      Ragdolls or other articulated physical enttiies.
	//!          PE_ROPE             Physical representation of the rope.
	//!          PE_SOFT             Soft body physics, cloth simulation.
	//!          PE_AREA             Physical Area (Sphere,Box,Geometry or Shape).
	//!       </para>
	//!
	//!      <para>
	//!          Params table keys   Meaning
	//!          -----------------   -----------
	//!          mass                Object mass, only used if density is not specified or -1.
	//!          density             Object density, only used if mass is not specified or -1.
	//!          flags               Physical entity flags.
	//!          partid              Index of the articulated body part, that this new physical entity will be attached to.
	//!          stiffness_scale     Scale of character joints stiffness (Multiplied with stiffness values specified from exported model)
	//!          Particle            This table must be set when Physics Type is PE_PARTICLE.
	//!          Living              This table must be set when Physics Type is PE_LIVING.
	//!          Area                This table must be set when Physics Type is PE_AREA.
	//!       </para>
	//!
	//!      <para>
	//!          Particle table      Meaning
	//!          -----------------   -----------
	//!          mass                Particle mass.
	//!          radius              Particle pseudo radius.
	//!          thickness           Thickness when lying on a surface (if 0, radius will be used).
	//!          velocity            Velocity direction and magnitude vector.
	//!          air_resistance      Air resistance coefficient, F = kv.
	//!          water_resistance    Water resistance coefficient, F = kv.
	//!          gravity             Gravity force vector to the air.
	//!          water_gravity       Gravity force vector when in the water.
	//!          min_bounce_vel      Minimal velocity at which particle bounces of the surface.
	//!          accel_thrust        Acceleration along direction of movement.
	//!          accel_lift          Acceleration that lifts particle with the current speed.
	//!          constant_orientation (0,1) Keep constance orientation.
	//!          single_contact      (0,1) Calculate only one first contact.
	//!          no_roll             (0,1) Do not roll particle on terrain.
	//!          no_spin             (0,1) Do not spin particle in air.
	//!          no_path_alignment   (0,1) Do not align particle orientation to the movement path.
	//!       </para>
	//!      <para>
	//!          Living table        Meaning
	//!          -----------------   -----------
	//!          height              Vertical offset of collision geometry center.
	//!          size                Collision cylinder dimensions vector (x,y,z).
	//!          height_eye          Vertical offset of the camera.
	//!          height_pivot        Offset from central ground position that is considered entity center.
	//!          head_radius         Radius of the head.
	//!          height_head         Vertical offset of the head.
	//!          inertia             Inertia coefficient, the more it is, the less inertia is; 0 means no inertia
	//!          air_resistance      Air control coefficient 0..1, 1 - special value (total control of movement)
	//!          gravity             Vertical gravity magnitude.
	//!          mass                Mass of the player (in kg).
	//!          min_slide_angle     If surface slope is more than this angle, player starts sliding (In radians)
	//!          max_climb_angle     Player cannot climb surface which slope is steeper than this angle (In radians)
	//!          max_jump_angle      Player is not allowed to jump towards ground if this angle is exceeded (In radians)
	//!          min_fall_angle      Player starts falling when slope is steeper than this (In radians)
	//!          max_vel_ground      Player cannot stand on surfaces that are moving faster than this (In radians)
	//!       </para>
	//!
	//!      <para>
	//!          Area table keys     Meaning
	//!          -----------------   -----------
	//!          type                Type of the area, valid values are: AREA_SPHERE,AREA_BOX,AREA_GEOMETRY,AREA_SHAPE
	//!          radius              Radius of the area sphere, must be specified if type is AREA_SPHERE.
	//!          box_min             Min vector of bounding box, must be specified if type is AREA_BOX.
	//!          box_max             Max vector of bounding box, must be specified if type is AREA_BOX..
	//!          points              Table, indexed collection of vectors in local entity space defining 2D shape of the area, (AREA_SHAPE)
	//!          height              Height of the 2D area (AREA_SHAPE), relative to the minimal Z in the points table.
	//!          uniform             Same direction in every point or always point to the center.
	//!          falloff             ellipsoidal falloff dimensions; 0,0,0 if no falloff.
	//!          gravity             Gravity vector inside the physical area.
	//!       </para>
	//!</description>
	//!		<param name="nSlot - Slot identifier, if -1 use geometries from all slots.</param>
	//!		<param name="nPhysicsType - Type of physical entity to create.</param>
	//!		<param name="physicsParams - Table with physicalization parameters.</param>
	int Physicalize(IFunctionHandler* pH, int nSlot, int nPhysicsType, SmartScriptTable physicsParams);

	//! <code>Entity.SetPhysicParams()</code>
	int SetPhysicParams(IFunctionHandler* pH);

	//! <code>Entity.SetCharacterPhysicParams()</code>
	int SetCharacterPhysicParams(IFunctionHandler* pH);

	//! <code>Entity.ActivatePlayerPhysics( bEnable )</code>
	int ActivatePlayerPhysics(IFunctionHandler* pH, bool bEnable);

	//! <code>Entity.ReattachSoftEntityVtx( partId )</code>
	int ReattachSoftEntityVtx(IFunctionHandler* pH, ScriptHandle entityId, int partId);

	//! <code>Entity.PhysicalizeSlot( slot, physicsParams )</code>
	int PhysicalizeSlot(IFunctionHandler* pH, int slot, SmartScriptTable physicsParams);

	//! <code>Entity.UpdateSlotPhysics( slot )</code>
	int UpdateSlotPhysics(IFunctionHandler* pH, int slot);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.SetColliderMode( mode )</code>
	int SetColliderMode(IFunctionHandler* pH, int mode);

	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.SelectPipe()</code>
	int SelectPipe(IFunctionHandler* pH);

	//! <code>Entity.IsUsingPipe( pipename )</code>
	//! <description>Returns true if entity is running the given goalpipe or has it inserted.</description>
	//!		<param name="pipename - goalpipe name</param>
	//! <returns>
	//!   true - if entity is running the given goalpipe or has it inserted
	//!		false - otherwise
	//! </returns>
	int IsUsingPipe(IFunctionHandler* pH);

	//! <code>Entity.Activate( bActivate )</code>
	//! <description>
	//!		Activates or deactivates entity.
	//!		This calls ignores update policy and forces entity to activate or deactivate
	//!		All active entities will be updated every frame, having too many active entities can affect performance.
	//! </description>
	//!		<param name="bActivate - if true entity will become active, is false will deactivate and stop being updated every frame.</param>
	int Activate(IFunctionHandler* pH, int bActive);

	//! <code>Entity.IsActive( bActivate )</code>
	//! <description>Retrieve active status of entity.</description>
	//!	<returns>
	//!		true - Entity is active.
	//!		false - Entity is not active.
	//!	</returns>
	int IsActive(IFunctionHandler* pH);

	//! <code>Entity.SetUpdatePolicy( nUpdatePolicy )</code>
	//! <description>
	//!    Changes update policy for the entity.
	//!    Update policy controls when entity becomes active or inactive (ex. when visible, when in close proximity, etc...).
	//!    All active entities will be updated every frame, having too many active entities can affect performance.
	//!      <para>
	//!          Update Policy                    Meaning
	//!          -------------                    -----------
	//!          ENTITY_UPDATE_NEVER              Never update this entity.
	//!          ENTITY_UPDATE_IN_RANGE           Activate entity when in specified radius.
	//!          ENTITY_UPDATE_POT_VISIBLE        Activate entity when potentially visible.
	//!          ENTITY_UPDATE_VISIBLE            Activate entity when visible in frustum.
	//!          ENTITY_UPDATE_PHYSICS            Activate entity when physics awakes, deactivate when physics go to sleep.
	//!          ENTITY_UPDATE_PHYSICS_VISIBLE    Same as ENTITY_UPDATE_PHYSICS, but also activates when visible.
	//!          ENTITY_UPDATE_ALWAYS             Entity is always active and updated every frame.
	//!       </para>
	//! </description>
	//!		<param name="nUpdatePolicy">Update policy type.</param>
	//! <remarks>Use SetUpdateRadius for update policy that require a radius.</remarks>
	int SetUpdatePolicy(IFunctionHandler* pH, int nUpdatePolicy);

	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.SetLocalBBox( vMin, vMax )</code>
	int SetLocalBBox(IFunctionHandler* pH, Vec3 vMin, Vec3 vMax);

	//! <code>Entity.GetLocalBBox()</code>
	int GetLocalBBox(IFunctionHandler* pH);

	//! <code>Entity.GetWorldBBox()</code>
	int GetWorldBBox(IFunctionHandler* pH);

	//! <code>Entity.GetProjectedWorldBBox()</code>
	int GetProjectedWorldBBox(IFunctionHandler* pH);

	//! <code>Entity.SetTriggerBBox( vMin, vMax )</code>
	int SetTriggerBBox(IFunctionHandler* pH, Vec3 vMin, Vec3 vMax);

	//! <code>Entity.GetTriggerBBox()</code>
	int GetTriggerBBox(IFunctionHandler* pH);

	//! <code>Entity.InvalidateTrigger()</code>
	int InvalidateTrigger(IFunctionHandler* pH);

	//! <code>Entity.ForwardTriggerEventsTo( entityId )</code>
	int ForwardTriggerEventsTo(IFunctionHandler* pH, ScriptHandle entityId);

	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.SetUpdateRadius()</code>
	int SetUpdateRadius(IFunctionHandler* pH);

	//! <code>Entity.GetUpdateRadius()</code>
	int GetUpdateRadius(IFunctionHandler* pH);

	//! <code>Entity.TriggerEvent()</code>
	int TriggerEvent(IFunctionHandler* pH);

	//! <code>Entity.GetHelperPos()</code>
	int GetHelperPos(IFunctionHandler* pH);

	//! <code>Entity.GetHelperDir()</code>
	int GetHelperDir(IFunctionHandler* pH);

	//! <code>Entity.GetHelperAngles()</code>
	int GetHelperAngles(IFunctionHandler* pH);

	//! <code>Entity.GetSlotHelperPos( slot, helperName, objectSpace )</code>
	int GetSlotHelperPos(IFunctionHandler* pH, int slot, const char* helperName, bool objectSpace);

	//! <code>Entity.GetBonePos()</code>
	int GetBonePos(IFunctionHandler* pH);

	//! <code>Entity.GetBoneDir()</code>
	int GetBoneDir(IFunctionHandler* pH);

	//! <code>Entity.GetBoneVelocity( characterSlot, boneName )</code>
	int GetBoneVelocity(IFunctionHandler* pH, int characterSlot, const char* boneName);

	//! <code>Entity.GetBoneAngularVelocity( characterSlot, oneName )</code>
	int GetBoneAngularVelocity(IFunctionHandler* pH, int characterSlot, const char* boneName);

	//! <code>Entity.GetBoneNameFromTable()</code>
	int GetBoneNameFromTable(IFunctionHandler* pH);

	//! <code>Entity.SetName()</code>
	int SetName(IFunctionHandler* pH);

	//! <code>Entity.GetName()</code>
	int GetName(IFunctionHandler* pH);

	//! <code>Entity.GetRawId()</code>
	//! <description>Returns entityId in raw numeric format.</description>
	int GetRawId(IFunctionHandler* pH);

	//! <code>Entity.SetAIName()</code>
	int SetAIName(IFunctionHandler* pH);

	//! <code>Entity.GetAIName()</code>
	int GetAIName(IFunctionHandler* pH);

	//! <code>Entity.SetFlags( flags, mode )</code>
	//! <description>Mode: 0: or  1: and  2: xor</description>
	int SetFlags(IFunctionHandler* pH, int flags, int mode);

	//! <code>Entity.GetFlags()</code>
	int GetFlags(IFunctionHandler* pH);

	//! <code>Entity.HasFlags( flags )</code>
	int HasFlags(IFunctionHandler* pH, int flags);

	//! <code>Entity.SetFlagsExtended( flags, mode )</code>
	//! <description>Mode: 0: or  1: and  2: xor</description>
	int SetFlagsExtended(IFunctionHandler* pH, int flags, int mode);

	//! <code>Entity.GetFlagsExtended()</code>
	int GetFlagsExtended(IFunctionHandler* pH);

	//! <code>Entity.HasFlags( flags )</code>
	int HasFlagsExtended(IFunctionHandler* pH, int flags);

	//! <code>Entity.GetArchetype()</code>
	//! <description>Retrieve the archetype of the entity.</description>
	//! <returns>name of entity archetype, nil if no archetype.</returns>
	int GetArchetype(IFunctionHandler* pH);

	//! <code>Entity.IntersectRay( slot, rayOrigin, rayDir, maxDistance )</code>
	int IntersectRay(IFunctionHandler* pH, int slot, Vec3 rayOrigin, Vec3 rayDir, float maxDistance);

	//////////////////////////////////////////////////////////////////////////
	// Attachments
	//////////////////////////////////////////////////////////////////////////

	//! <code>Entity.AttachChild( childEntityId, flags )</code>
	int AttachChild(IFunctionHandler* pH, ScriptHandle childEntityId, int flags);

	//! <code>Entity.DetachThis()</code>
	int DetachThis(IFunctionHandler* pH);

	//! <code>Entity.DetachAll()</code>
	int DetachAll(IFunctionHandler* pH);

	//! <code>Entity.GetParent()</code>
	int GetParent(IFunctionHandler* pH);

	//! <code>Entity.GetChildCount()</code>
	int GetChildCount(IFunctionHandler* pH);

	//! <code>Entity.GetChild( int nIndex )</code>
	int GetChild(IFunctionHandler* pH, int nIndex);

	//! <code>Entity.EnableInheritXForm()</code>
	//! <description>Enables/Disable entity from inheriting transformation from the parent.</description>
	int EnableInheritXForm(IFunctionHandler* pH, bool bEnable);

	//! <code>Entity.NetPresent()</code>
	int NetPresent(IFunctionHandler* pH);

	//! <code>Entity.RenderShadow()</code>
	int RenderShadow(IFunctionHandler* pH);

	//! <code>Entity.SetRegisterInSectors()</code>
	int SetRegisterInSectors(IFunctionHandler* pH);

	//! <code>Entity.IsColliding()</code>
	int IsColliding(IFunctionHandler* pH);

	//! <code>Entity.GetDirectionVector()</code>
	int GetDirectionVector(IFunctionHandler* pH);

	//! <code>Entity.SetDirectionVector( direction )</code>
	int SetDirectionVector(IFunctionHandler* pH, Vec3 dir);

	//! <code>Entity.IsAnimationRunning( characterSlot, layer )</code>
	//! <param name="characterSlot">Index of the character slot.</param>
	//! <param name="layer">Index of the animation layer.</param>
	//! <returns>nil or not nil</returns>
	int IsAnimationRunning(IFunctionHandler* pH, int characterSlot, int layer);

	//! <code>Entity.AddImpulse( ipart, position, direction, linearImpulse, linearImpulseScale, angularAxis, angularImpulse, massScale )</code>
	//! <description>
	//!    Apply an impulse to the entity.
	//!    At least four parameters need to be provided for a linear impulse.
	//!    For an additional angular impulse, at least seven parameters need to be provided.
	//! </description>
	//!	<param name="ipart">The index of the part that receives the impulse.</param>
	//!	<param name="position">The point (in world coordinates) where the impulse is applied. Set this to (0, 0, 0) to ignore it.</param>
	//!	<param name="direction">The direction in which the impulse is applied.</param>
	//!	<param name="linearImpulse">The force of the linear impulse.</param>
	//!	<param name="linearImpulseScale">Scaling of the linear impulse. (Default: 1.0)</param>
	//!	<param name="angularAxis">The axis on which the angular impulse is applied.</param>
	//!	<param name="angularImpulse">The force of the the angular impulse.</param>
	//!	<param name="massScale">Mass scaling of the angular impulse. (Default: 1.0)</param>
	int AddImpulse(IFunctionHandler* pH);

	//! <code>Entity.AddConstraint()</code>
	int AddConstraint(IFunctionHandler* pH);

	//! <code>Entity.SetPublicParam()</code>
	//! <description>Set a shader parameter</description>
	//!	<param name="paramName">The name of the shader parameter.</param>
	//!	<param name="value">The new value of the parameter.</param>
	int SetPublicParam(IFunctionHandler* pH);

	// Audio

	//! <code>Entity.GetAllAuxAudioProxiesID()</code>
	//! <description>
	//!    Returns the ID used to address all AuxAudioProxy of the parent EntityAudioProxy.
	//! </description>
	//! <returns>Returns the ID used to address all AuxAudioProxy of the parent EntityAudioProxy.</returns>
	int GetAllAuxAudioProxiesID(IFunctionHandler* pH);

	//! <code>Entity.GetDefaultAuxAudioProxyID()</code>
	//! <description>
	//!    Returns the ID of the default AudioProxy of the parent EntityAudioProxy.
	//! </description>
	//! <returns>Returns the ID of the default AudioProxy of the parent EntityAudioProxy.</returns>
	int GetDefaultAuxAudioProxyID(IFunctionHandler* pH);

	//! <code>Entity.CreateAuxAudioProxy()</code>
	//! <description>
	//!    Creates an additional AudioProxy managed by the EntityAudioProxy.
	//!    The created AuxAudioProxy will move and rotate with the parent EntityAudioProxy.
	//! </description>
	//! <returns>Returns the ID of the additionally created AudioProxy.</returns>
	int CreateAuxAudioProxy(IFunctionHandler* pH);

	//! <code>Entity.RemoveAuxAudioProxy( hAudioProxyLocalID )</code>
	//! <description>
	//!    Removes the AuxAudioProxy corresponding to the passed ID from the parent EntityAudioProxy.
	//! </description>
	//!		<param name="hAudioProxyLocalID">hAudioProxyLocalID - ID of the AuxAudioProxy to be removed from the parent EntityAudioProxy.</param>
	//! <returns>nil</returns>
	int RemoveAuxAudioProxy(IFunctionHandler* pH, ScriptHandle const hAudioProxyLocalID);

	//! <code>Entity.ExecuteAudioTrigger( hTriggerID, hAudioProxyLocalID )</code>
	//! <description>
	//!    Execute the specified audio trigger and attach it to the entity.
	//!    The created audio object will move and rotate with the entity.
	//! </description>
	//!		<param name="hTriggerID">the audio trigger ID handle</param>
	//!		<param name="hAudioProxyLocalID">ID of the AuxAudioProxy local to the EntityAudioProxy (to address the default AuxAudioProxy pass 1 to address all AuxAudioProxies pass 0)</param>
	//! <returns>nil</returns>
	int ExecuteAudioTrigger(IFunctionHandler* pH, ScriptHandle const hTriggerID, ScriptHandle const hAudioProxyLocalID);

	//! <code>Entity.StopAudioTrigger( hTriggerID, hAudioProxyLocalID )</code>
	//! <description>Stop the audio event generated by the trigger with the specified ID on this entity.</description>
	//!		<param name="hTriggerID">the audio trigger ID handle</param>
	//!		<param name="hAudioProxyLocalID">ID of the AuxAudioProxy local to the EntityAudioProxy (to address the default AuxAudioProxy pass 1 to address all AuxAudioProxies pass 0)</param>
	//! <returns>nil</returns>
	int StopAudioTrigger(IFunctionHandler* pH, ScriptHandle const hTriggerID, ScriptHandle const hAudioProxyLocalID);

	//! <code>Entity.SetAudioSwitchState( hSwitchID, hSwitchStateID, hAudioProxyLocalID )</code>
	//! <description>Set the specified audio switch to the specified state on the current Entity.</description>
	//!		<param name="hSwitchID">the audio switch ID handle</param>
	//!		<param name="nSwitchStateID">the switch state ID handle</param>
	//!		<param name="hAudioProxyLocalID">ID of the AuxAudioProxy local to the EntityAudioProxy (to address the default AuxAudioProxy pass 1 to address all AuxAudioProxies pass 0)</param>
	//! <returns>nil</returns>
	int SetAudioSwitchState(IFunctionHandler* pH, ScriptHandle const hSwitchID, ScriptHandle const hSwitchStateID, ScriptHandle const hAudioProxyLocalID);

	//! <code>Entity.SetAudioObstructionCalcType( nObstructionCalcType, hAudioProxyLocalID )</code>
	//! <description>Set the Audio Obstruction/Occlusion calculation type on the underlying GameAudioObject.</description>
	//!		<param name="nObstructionCalcType">Obstruction/Occlusion calculation type;
	//!				Possible values:
	//!				0 - ignore Obstruction/Occlusion
	//!				1 - use single physics ray
	//!				2 - use multiple physics rays (currently 5 per object)</param>
	//!		<param name="hAudioProxyLocalID">ID of the AuxAudioProxy local to the EntityAudioProxy (to address the default AuxAudioProxy pass 1 to address all AuxAudioProxies pass 0)</param>
	//! <returns>nil</returns>
	int SetAudioObstructionCalcType(IFunctionHandler* pH, int const nObstructionCalcType, ScriptHandle const hAudioProxyLocalID);

	//! <code>Entity.SetFadeDistance( fFadeDistance )</code>
	//! <description>Sets the distance in which this entity will execute fade calculations.</description>
	//!		<param name="fFadeDistance">fade distance in meters</param>
	//! <returns>nil</returns>
	int SetFadeDistance(IFunctionHandler* pH, float const fFadeDistance);

	//! <code>Entity.SetAudioProxyOffset( vOffset, hAudioProxyLocalID )</code>
	//! <description>Set offset on the AudioProxy attached to the Entity.</description>
	//!		<param name="vOffset">offset vector</param>
	//!		<param name="hAudioProxyLocalID">ID of the AuxAudioProxy local to the EntityAudioProxy (to address the default AuxAudioProxy pass 1 to address all AuxAudioProxies pass 0)</param>
	//! <returns>nil</returns>
	int SetAudioProxyOffset(IFunctionHandler* pH, Vec3 const vOffset, ScriptHandle const hAudioProxyLocalID);

	//! <code>Entity.SetEnvironmentFadeDistance( fEnvironmentFadeDistance )</code>
	//! <description>Sets the distance over which this entity will fade the audio environment amount for all approaching entities.</description>
	//!		<param name="fEnvironmentFadeDistance">fade distance in meters</param>
	//! <returns>nil</returns>
	int SetEnvironmentFadeDistance(IFunctionHandler* pH, float const fEnvironmentFadeDistance);

	//! <code>Entity.SetAudioEnvironmentID( nAudioEnvironmentID )</code>
	//! <description>Sets the ID of the audio environment this entity will set for entities inside it.</description>
	//!		<param name="nAudioEnvironmentID">audio environment ID</param>
	//! <returns>nil</returns>
	int SetAudioEnvironmentID(IFunctionHandler* pH, ScriptHandle const hAudioEnvironmentID);

	//! <code>Entity.SetCurrentAudioEnvironments()</code>
	//! <description>Sets the correct audio environment amounts based on the entity's position in the world.</description>
	//! <returns>nil</returns>
	int SetCurrentAudioEnvironments(IFunctionHandler* pH);

	//! <code>Entity.SetAudioRtpcValue( hRtpcID, fValue, hAudioProxyLocalID )</code>
	//! <description>Set the specified audio RTPC to the specified value on the current Entity.</description>
	//!		<param name="hRtpcID">the audio RTPC ID handle</param>
	//!		<param name="fValue">the RTPC value</param>
	//!		<param name="hAudioProxyLocalID">ID of the AuxAudioProxy local to the EntityAudioProxy (to address the default AuxAudioProxy pass 1 to address all AuxAudioProxies pass 0)</param>
	//! <returns>nil</returns>
	int SetAudioRtpcValue(IFunctionHandler* pH, ScriptHandle const hRtpcID, float const fValue, ScriptHandle const hAudioProxyLocalID);

	//! <code>Entity.AuxAudioProxiesMoveWithEntity( bCanMoveWithEntity )</code>
	//! <description>Set whether AuxAudioProxies should move with the entity or not.</description>
	//!		<param name="bCanMoveWithEntity">boolean parameter to enable or disable</param>
	//! <returns>nil</returns>
	int AuxAudioProxiesMoveWithEntity(IFunctionHandler* pH, bool const bCanMoveWithEntity);

	// ~Audio

	//! <code> Entity.SetGeomCachePlaybackTime()</code>
	//! <description>Sets the playback time.</description>
	int SetGeomCachePlaybackTime(IFunctionHandler* pH, float time);

	//! <code> Entity.SetGeomCacheParams()</code>
	//! <description>Sets geometry cache parameters.</description>
	int SetGeomCacheParams(IFunctionHandler* pH, bool looping, const char* standIn, const char* standInMaterial, const char* firstFrameStandIn,
	                       const char* firstFrameStandInMaterial, const char* lastFrameStandIn, const char* lastFrameStandInMaterial, float standInDistance, float streamInDistance);
	//! <code> Entity.SetGeomCacheStreaming()</code>
	//! <description>Activates/deactivates geom cache streaming.</description>
	int SetGeomCacheStreaming(IFunctionHandler* pH, bool active, float time);

	//! <code> Entity.IsGeomCacheStreaming()</code>
	//! <returns>true if geom cache is streaming.</returns>
	int IsGeomCacheStreaming(IFunctionHandler* pH);

	//! <code> Entity.GetGeomCachePrecachedTime()</code>
	//! <description>Gets time delta from current playback position to last ready to play frame.</description>
	int GetGeomCachePrecachedTime(IFunctionHandler* pH);

	//! <code> Entity.SetGeomCacheDrawing()</code>
	//! <description>Activates/deactivates geom cache drawing.</description>
	int SetGeomCacheDrawing(IFunctionHandler* pH, bool active);

	//! <code>Entity.StartAnimation()</code>
	int StartAnimation(IFunctionHandler* pH);

	//! <code>Entity.StopAnimation( characterSlot, layer )</code>
	int StopAnimation(IFunctionHandler* pH, int characterSlot, int layer);

	//! <code>Entity.ResetAnimation( characterSlot, layer )</code>
	int ResetAnimation(IFunctionHandler* pH, int characterSlot, int layer);

	//! <code>Entity.RedirectAnimationToLayer0( characterSlot, redirect )</code>
	int RedirectAnimationToLayer0(IFunctionHandler* pH, int characterSlot, bool redirect);

	//! <code>Entity.SetAnimationBlendOut( characterSlot, layer, blendOut )</code>
	int SetAnimationBlendOut(IFunctionHandler* pH, int characterSlot, int layer, float blendOut);

	//! <code>Entity.EnableBoneAnimation( characterSlot, layer, boneName, status )</code>
	int EnableBoneAnimation(IFunctionHandler* pH, int characterSlot, int layer, const char* boneName, bool status);

	//! <code>Entity.EnableBoneAnimationAll( characterSlot, layer, status )</code>
	int EnableBoneAnimationAll(IFunctionHandler* pH, int characterSlot, int layer, bool status);

	//! <code>Entity.EnableProceduralFacialAnimation( enable )</code>
	int EnableProceduralFacialAnimation(IFunctionHandler* pH, bool enable);

	//! <code>Entity.PlayFacialAnimation( name, looping )</code>
	int PlayFacialAnimation(IFunctionHandler* pH, char* name, bool looping);

	//! <code>Entity.SetAnimationEvent( nSlot, sAnimation )</code>
	int SetAnimationEvent(IFunctionHandler* pH, int nSlot, const char* sAnimation);

	//! <code>Entity.SetAnimationKeyEvent( nSlot, sAnimation, nFrameID, sEvent)</code>
	int SetAnimationKeyEvent(IFunctionHandler* pH);

	//! <code>Entity.DisableAnimationEvent( nSlot, sAnimation )</code>
	int DisableAnimationEvent(IFunctionHandler* pH, int nSlot, const char* sAnimation);

	//! <code> Entity.SetAnimationSpeed( characterSlot, layer, speed )</code>
	int SetAnimationSpeed(IFunctionHandler* pH, int characterSlot, int layer, float speed);

	//! <code> Entity.SetAnimationTime( nSlot, nLayer, fTime )</code>
	int SetAnimationTime(IFunctionHandler* pH, int nSlot, int nLayer, float fTime);

	//! <code> Entity.GetAnimationTime( nSlot, nLayer )</code>
	int GetAnimationTime(IFunctionHandler* pH, int nSlot, int nLayer);

	//! <code> Entity.GetCurAnimation()</code>
	int GetCurAnimation(IFunctionHandler* pH);

	//! <code> Entity.GetAnimationLength( characterSlot, animation )</code>
	int GetAnimationLength(IFunctionHandler* pH, int characterSlot, const char* animation);

	//! <code> Entity.SetAnimationFlip( characterSlot, flip )</code>
	int SetAnimationFlip(IFunctionHandler* pH, int characterSlot, Vec3 flip);

	//! <code> Entity.SetTimer()</code>
	int SetTimer(IFunctionHandler* pH);

	//! <code> Entity.KillTimer()</code>
	int KillTimer(IFunctionHandler* pH);

	//! <code> Entity.SetScriptUpdateRate( nMillis )</code>
	int SetScriptUpdateRate(IFunctionHandler* pH, int nMillis);

	//////////////////////////////////////////////////////////////////////////
	// State management.
	//////////////////////////////////////////////////////////////////////////

	//! <code> Entity.GotoState( sState )</code>
	int GotoState(IFunctionHandler* pH, const char* sState);

	//! <code> Entity.IsInState( sState )</code>
	int IsInState(IFunctionHandler* pH, const char* sState);

	//! <code> Entity.GetState()</code>
	int GetState(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////

	//! <code> Entity.IsHidden()</code>
	int IsHidden(IFunctionHandler* pH);

	//! <code> Entity.GetTouchedSurfaceID()</code>
	int GetTouchedSurfaceID(IFunctionHandler* pH);

	//! <code> Entity.GetTouchedPoint()</code>
	//! <description>Retrieves point of collision for rigid body.</description>
	int GetTouchedPoint(IFunctionHandler* pH);

	//! <code> Entity.CreateBoneAttachment( characterSlot, boneName, attachmentName )</code>
	int CreateBoneAttachment(IFunctionHandler* pH, int characterSlot, const char* boneName, const char* attachmentName);

	//! <code> Entity.CreateSkinAttachment( characterSlot, attachmentName )</code>
	int CreateSkinAttachment(IFunctionHandler* pH, int characterSlot, const char* attachmentName);

	//! <code> Entity.DestroyAttachment( characterSlot, attachmentName )</code>
	int DestroyAttachment(IFunctionHandler* pH, int characterSlot, const char* attachmentName);

	//! <code> Entity.GetAttachmentBone( characterSlot, attachmentName )</code>
	int GetAttachmentBone(IFunctionHandler* pH, int characterSlot, const char* attachmentName);

	//! <code> Entity.GetAttachmentCGF( characterSlot, attachmentName )</code>
	int GetAttachmentCGF(IFunctionHandler* pH, int characterSlot, const char* attachmentName);

	//! <code> Entity.ResetAttachment( characterSlot, attachmentName )</code>
	int ResetAttachment(IFunctionHandler* pH, int characterSlot, const char* attachmentName);

	//! <code> Entity.SetAttachmentEffect( characterSlot, attachmentName, effectName, offset, dir, scale, flags )</code>
	int SetAttachmentEffect(IFunctionHandler* pH, int characterSlot, const char* attachmentName, const char* effectName, Vec3 offset, Vec3 dir, float scale, int flags);

	//! <code> Entity.SetAttachmentObject( characterSlot, attachmentName, entityId, slot, flags )</code>
	int SetAttachmentObject(IFunctionHandler* pH, int characterSlot, const char* attachmentName, ScriptHandle entityId, int slot, int flags);

	//! <code> Entity.SetAttachmentCGF( characterSlot, attachmentName, filePath )</code>
	int SetAttachmentCGF(IFunctionHandler* pH, int characterSlot, const char* attachmentName, const char* filePath);

	//! <code> Entity.SetAttachmentLight( characterSlot, attachmentName, lightTable, flags )</code>
	int SetAttachmentLight(IFunctionHandler* pH, int characterSlot, const char* attachmentName, SmartScriptTable lightTable, int flags);

	//! <code> Entity.SetAttachmentPos( characterSlot, attachmentName, pos )</code>
	int SetAttachmentPos(IFunctionHandler* pH, int characterSlot, const char* attachmentName, Vec3 pos);

	//! <code> Entity.SetAttachmentAngles( characterSlot, attachmentName, angles )</code>
	int SetAttachmentAngles(IFunctionHandler* pH, int characterSlot, const char* attachmentName, Vec3 angles);

	//! <code> Entity.SetAttachmentDir()</code>
	int SetAttachmentDir(IFunctionHandler* pH, int characterSlot, const char* attachmentName, Vec3 dir, bool worldSpace);

	//! <code> Entity.HideAttachment( characterSlot, attachmentName, hide, hideShadow )</code>
	int HideAttachment(IFunctionHandler* pH, int characterSlot, const char* attachmentName, bool hide, bool hideShadow);

	//! <code> Entity.HideAllAttachments( characterSlot, hide, hideShadow )</code>
	int HideAllAttachments(IFunctionHandler* pH, int characterSlot, bool hide, bool hideShadow);

	//! <code> Entity.HideAttachmentMaster( characterSlot, hide )</code>
	int HideAttachmentMaster(IFunctionHandler* pH, int characterSlot, bool hide);

	//! <code> Entity.PhysicalizeAttachment( characterSlot, attachmentName, physicalize )</code>
	int PhysicalizeAttachment(IFunctionHandler* pH, int characterSlot, const char* attachmentName, bool physicalize);

	//! <code> Entity.Damage()</code>
	int Damage(IFunctionHandler* pH);

	//! <code> Entity.GetEntitiesInContact()</code>
	int GetEntitiesInContact(IFunctionHandler* pH);

	//! <code> Entity.GetExplosionObstruction()</code>
	int GetExplosionObstruction(IFunctionHandler* pH);

	//! <code> Entity.GetExplosionImpulse()</code>
	int GetExplosionImpulse(IFunctionHandler* pH);

	//! <code> Entity.SetMaterial()</code>
	int SetMaterial(IFunctionHandler* pH);

	//! <code> Entity.GetMaterial()</code>
	int GetMaterial(IFunctionHandler* pH);

	//! <code> Entity.GetEntityMaterial()</code>
	int GetEntityMaterial(IFunctionHandler* pH);

	//! <code> Entity.ChangeAttachmentMaterial(attachmentName, materialName)</code>
	int ChangeAttachmentMaterial(IFunctionHandler* pH, const char* attachmentName, const char* materialName);

	//! <code> Entity.ReplaceMaterial( slot, name, replacement )</code>
	int ReplaceMaterial(IFunctionHandler* pH, int slot, const char* name, const char* replacement);

	//! <code> Entity.ResetMaterial( slot )</code>
	int ResetMaterial(IFunctionHandler* pH, int slot);

	/*	int AddMaterialLayer(IFunctionHandler *pH, int slotId, const char *shader);
	   int RemoveMaterialLayer(IFunctionHandler *pH, int slotId, int id);
	   int RemoveAllMaterialLayers(IFunctionHandler *pH, int slotId);
	   int SetMaterialLayerParamF(IFunctionHandler *pH, int slotId, int layerId, const char *name, float value);
	   int SetMaterialLayerParamV(IFunctionHandler *pH, int slotId, int layerId, const char *name, Vec3 vec);
	   int SetMaterialLayerParamC(IFunctionHandler *pH, int slotId, int layerId, const char *name,
	    float r, float g, float b, float a);
	 */
	//! <code> Entity.EnableMaterialLayer( enable, layer )</code>
	int EnableMaterialLayer(IFunctionHandler* pH, bool enable, int layer);

	//! <code>Entity.CloneMaterial( nSlotId, sSubMaterialName )</code>
	//! <description>
	//!		Replace material on the slot with a cloned version of the material.
	//!		Cloned material can be freely changed uniquely for this entity.
	//! </description>
	//!		<param name="nSlotId">On which slot to clone material.</param>
	//!		<param name="sSubMaterialName">if non empty string only this specific sub-material is cloned.</param>
	int CloneMaterial(IFunctionHandler* pH, int slot);

	//! <code>Entity.SetMaterialFloat( nSlotId, nSubMtlId, sParamName, fValue )</code>
	//! <description>Change material parameter.</description>
	//!		<param name="nSlot">On which slot to change material.</param>
	//!		<param name="nSubMtlId">Specify sub-material by Id.</param>
	//!		<param name="sParamName">Name of the material parameter.</param>
	//!		<param name="fValue">New material parameter value.</param>
	int SetMaterialFloat(IFunctionHandler* pH, int slot, int nSubMtlId, const char* sParamName, float fValue);

	//! <code>Entity.GetMaterialFloat( nSlotId, nSubMtlId, sParamName )</code>
	//! <description>Change material parameter.</description>
	//!		<param name="nSlot">On which slot to change material.</param>
	//!		<param name="nSubMtlId">Specify sub-material by Id.</param>
	//!		<param name="sParamName">Name of the material parameter.</param>
	//! <returns>Material parameter value.</returns>
	int GetMaterialFloat(IFunctionHandler* pH, int slot, int nSubMtlId, const char* sParamName);

	//! <code>Entity.SetMaterialVec3( nSlotId, nSubMtlId, sParamName, vVec3 )</code>
	//! <seealso cref="SetMaterialFloat">
	int SetMaterialVec3(IFunctionHandler* pH, int slot, int nSubMtlId, const char* sParamName, Vec3 fValue);

	//! <code>Entity.GetMaterialVec3( nSlotId, nSubMtlId, sParamName )</code>
	//! <seealso cref="GetMaterialFloat">
	int GetMaterialVec3(IFunctionHandler* pH, int slot, int nSubMtlId, const char* sParamName);

	//! <code>Entity.MaterialFlashInvoke()</code>
	int MaterialFlashInvoke(IFunctionHandler* pH);

	//! <code>Entity.ToLocal( slotId, point )</code>
	int ToLocal(IFunctionHandler* pH, int slotId, Vec3 point);

	//! <code>Entity.ToGlobal( slotId, point )</code>
	int ToGlobal(IFunctionHandler* pH, int slotId, Vec3 point);

	//! <code>Entity.VectorToLocal( slotId, dir )</code>
	int VectorToLocal(IFunctionHandler* pH, int slotId, Vec3 dir);

	//! <code>Entity.VectorToGlobal( slotId, dir )</code>
	int VectorToGlobal(IFunctionHandler* pH, int slotId, Vec3 dir);

	//! <code>Entity.CheckCollisions()</code>
	int CheckCollisions(IFunctionHandler* pH);

	//! <code>Entity.AwakeEnvironment()</code>
	int AwakeEnvironment(IFunctionHandler* pH);

	//! <code>Entity.GetTimeSinceLastSeen()</code>
	int GetTimeSinceLastSeen(IFunctionHandler* pH);

	//! <code>Entity.GetViewDistRatio()</code>
	int GetViewDistRatio(IFunctionHandler* pH);

	//! <code>Entity.SetViewDistRatio()</code>
	int SetViewDistRatio(IFunctionHandler* pH);

	//! <code>Entity.SetViewDistUnlimited()</code>
	int SetViewDistUnlimited(IFunctionHandler* pH);

	//! <code>Entity.SetLodRatio()</code>
	int SetLodRatio(IFunctionHandler* pH);

	//! <code>Entity.GetLodRatio()</code>
	int GetLodRatio(IFunctionHandler* pH);

	//! <code>Entity.SetStateClientside()</code>
	int SetStateClientside(IFunctionHandler* pH);

	//! <code>Entity.InsertSubpipe()</code>
	int InsertSubpipe(IFunctionHandler* pH);

	//! <code>Entity.CancelSubpipe()</code>
	int CancelSubpipe(IFunctionHandler* pH);

	//! <code>Entity.PassParamsToPipe()</code>
	int PassParamsToPipe(IFunctionHandler* pH);

	//! <code>Entity.SetDefaultIdleAnimations()</code>
	int SetDefaultIdleAnimations(IFunctionHandler* pH);

	//! <code>Entity.GetVelocity()</code>
	int GetVelocity(IFunctionHandler* pH);

	//! <code>Entity.GetVelocityEx()</code>
	int GetVelocityEx(IFunctionHandler* pH);

	//! <code>Entity.SetVelocity(velocity)</code>
	int SetVelocity(IFunctionHandler* pH, Vec3 velocity);

	//! <code>Entity.GetVelocityEx(velocity, angularVelocity)</code>
	int SetVelocityEx(IFunctionHandler* pH, Vec3 velocity, Vec3 angularVelocity);

	//! <code>Entity.GetSpeed()</code>
	int GetSpeed(IFunctionHandler* pH);

	//! <code>Entity.GetMass()</code>
	int GetMass(IFunctionHandler* pH);

	//! <code>Entity.GetVolume(slot)</code>
	int GetVolume(IFunctionHandler* pH, int slot);

	//! <code>Entity.GetGravity()</code>
	int GetGravity(IFunctionHandler* pH);

	//! <code>Entity.GetSubmergedVolume( slot, planeNormal, planeOrigin )</code>
	int GetSubmergedVolume(IFunctionHandler* pH, int slot, Vec3 planeNormal, Vec3 planeOrigin);

	//! <code>Entity.CreateLink( name, targetId )</code>
	//! <description>
	//!		Creates a new outgoing link for this entity.
	//! </description>
	//!	<param name="name">Name of the link. Does not have to be unique among all the links of this entity. Multiple links with the same name can perfectly co-exist.</param>
	//!	<param name="(optional) targetId">If specified, the ID of the entity the link shall target. If not specified or 0 then the link will not target anything. Default value: 0</param>
	//! <returns>nothing</returns>
	//! <seealso cref="SetLinkTarget"/>
	//! <seealso cref="GetLinkName"/>
	//! <seealso cref="GetLinkTarget"/>
	//! <seealso cref="RemoveLink"/>
	//! <seealso cref="RemoveAllLinks"/>
	//! <seealso cref="GetLink"/>
	//! <seealso cref="CountLinks"/>
	int CreateLink(IFunctionHandler* pH, const char* name);

	//! <code>Entity.GetLinkName( targetId, ith )</code>
	//! <description>
	//!		Returns the name of the link that is targeting the entity with given ID.
	//! </description>
	//!	<param name="targetId">ID of the entity for which the link name shall be looked up.</param>
	//!	<param name="(optional) ith">If specified, the i'th link that targets given entity. Default value: 0 (first entity)</param>
	//! <returns>The name of the i'th link targeting given entity or nil if no such link exists.</returns>
	//! <seealso cref="CreateLinkTarget"/>
	//! <seealso cref="SetLinkTarget"/>
	//! <seealso cref="GetLinkTarget"/>
	//! <seealso cref="RemoveLink"/>
	//! <seealso cref="RemoveAllLinks"/>
	//! <seealso cref="GetLink"/>
	//! <seealso cref="CountLinks"/>
	int GetLinkName(IFunctionHandler* pH, ScriptHandle targetId);

	//! <code>Entity.SetLinkTarget(name, targetId, ith)</code>
	//! <description>
	//!		Specifies the entity that an existing link shall target. Use this function to change the target of an existing link.
	//! </description>
	//!	<param name="name">Name of the link that shall target given entity.</param>
	//!	<param name="targetId">The ID of the entity the link shall target. Pass in NULL_ENTITY to make the link no longer target an entity.</param>
	//!	<param name="(optional) ith">If specified, the i'th link with given name that shall target given entity. Default value: 0 (first link with given name)</param>
	//! <returns>nothing</returns>
	//! <seealso cref="CreateLink"/>
	//! <seealso cref="GetLinkName"/>
	//! <seealso cref="GetLinkTarget"/>
	//! <seealso cref="RemoveLink"/>
	//! <seealso cref="RemoveAllLinks"/>
	//! <seealso cref="GetLink"/>
	//! <seealso cref="CountLinks"/>
	int SetLinkTarget(IFunctionHandler* pH, const char* name, ScriptHandle targetId);

	//! <code>Entity.GetLinkTarget( name, ith )</code>
	//! <description>
	//!		Returns the ID of the entity that given link is targeting.
	//! </description>
	//!	<param name="name">Name of the link.</param>
	//!	<param name="(optional) ith">If specified, the i'th link with given name for which to look up the targeted entity. Default value: 0 (first link with given name)</param>
	//! <returns>The ID of the entity that the link is targeting or nil if no such link exists.</returns>
	//! <seealso cref="CreateLink"/>
	//! <seealso cref="SetLinkTarget"/>
	//! <seealso cref="GetLinkName"/>
	//! <seealso cref="RemoveLink"/>
	//! <seealso cref="RemoveAllLinks"/>
	//! <seealso cref="GetLink"/>
	//! <seealso cref="CountLinks"/>
	int GetLinkTarget(IFunctionHandler* pH, const char* name);

	//! <code>Entity.RemoveLink( name, ith )</code>
	//! <description>
	//!		Removes an outgoing link from the entity.
	//! </description>
	//!	<param name="name">Name of the link to remove.</param>
	//!	<param name="(optional) ith">If specified, the i'th link with given name that shall be removed. Default value: 0 (first link with given name)</param>
	//! <returns>nothing</returns>
	//! <seealso cref="CreateLink"/>
	//! <seealso cref="SetLinkTarget"/>
	//! <seealso cref="GetLinkName"/>
	//! <seealso cref="GetLinkTarget"/>
	//! <seealso cref="RemoveAllLinks"/>
	//! <seealso cref="GetLink"/>
	//! <seealso cref="CountLinks"/>
	int RemoveLink(IFunctionHandler* pH, const char* name);

	//! <code>Entity.RemoveAllLinks()</code>
	//! <description>
	//!		Removes all links of an entity.
	//! </description>
	//! <returns>nothing</returns>
	//! <seealso cref="CreateLink"/>
	//! <seealso cref="SetLinkTarget"/>
	//! <seealso cref="GetLinkName"/>
	//! <seealso cref="GetLinkTarget"/>
	//! <seealso cref="RemoveLink"/>
	//! <seealso cref="GetLink"/>
	//! <seealso cref="CountLinks"/>
	int RemoveAllLinks(IFunctionHandler* pH);

	//! <code>Entity.GetLink()</code>
	//! <description>
	//!		Returns the link at given index.
	//! </description>
	//!	<param name="ith">The index of the link that shall be returned.</param>
	//! <returns>The script table of the entity that the i'th link is targeting or nil if the specified index is out of bounds.</returns>
	//! <seealso cref="CreateLink"/>
	//! <seealso cref="SetLinkTarget"/>
	//! <seealso cref="GetLinkName"/>
	//! <seealso cref="GetLinkTarget"/>
	//! <seealso cref="RemoveLink"/>
	//! <seealso cref="RemoveAllLinks"/>
	//! <seealso cref="CountLinks"/>
	int GetLink(IFunctionHandler* pH, int ith);

	//! <code>Entity.CountLinks()</code>
	//! <description>
	//!		Counts all outgoing links of the entity.
	//! </description>
	//! <returns>Number of outgoing links.</returns>
	//! <seealso cref="CreateLink"/>
	//! <seealso cref="SetLinkTarget"/>
	//! <seealso cref="GetLinkName"/>
	//! <seealso cref="GetLinkTarget"/>
	//! <seealso cref="RemoveLink"/>
	//! <seealso cref="RemoveAllLinks"/>
	//! <seealso cref="GetLink"/>
	int CountLinks(IFunctionHandler* pH);

	//! <code>Entity.RemoveDecals()</code>
	int RemoveDecals(IFunctionHandler* pH);

	//! <code>Entity.ForceCharacterUpdate( characterSlot, updateAlways )</code>
	int ForceCharacterUpdate(IFunctionHandler* pH, int characterSlot, bool updateAlways);

	//! <code>Entity.CharacterUpdateAlways( characterSlot, updateAlways )</code>
	int CharacterUpdateAlways(IFunctionHandler* pH, int characterSlot, bool updateAlways);

	//! <code>Entity.CharacterUpdateOnRender( characterSlot, bUpdateOnRender )</code>
	int CharacterUpdateOnRender(IFunctionHandler* pH, int characterSlot, bool bUpdateOnRender);

	//! <code>Entity.SetAnimateOffScreenShadow( bAnimateOffScreenShadow )</code>
	int SetAnimateOffScreenShadow(IFunctionHandler* pH, bool bAnimateOffScreenShadow);

	//! <code>Entity.RagDollize(slot)</code>
	int RagDollize(IFunctionHandler* pH, int slot);

	//! <code>Entity.Hide()</code>
	int Hide(IFunctionHandler* pH);

	//! <code>Entity.NoExplosionCollision()</code>
	int NoExplosionCollision(IFunctionHandler* pH);

	//! <code>Entity.NoBulletForce( state )</code>
	int NoBulletForce(IFunctionHandler* pH, bool state);

	//! <code>Entity.UpdateAreas()</code>
	int UpdateAreas(IFunctionHandler* pH);

	//! <code>Entity.IsPointInsideArea( areaId, point )</code>
	int IsPointInsideArea(IFunctionHandler* pH, int areaId, Vec3 point);

	//! <code>Entity.IsEntityInsideArea( areaId, entityId )</code>
	int IsEntityInsideArea(IFunctionHandler* pH, int areaId, ScriptHandle entityId);

	//! <code>Entity.GetPhysicalStats()</code>
	//! <description>Some more physics related.</description>
	int GetPhysicalStats(IFunctionHandler* pH);

	//! <code>Entity.SetParentSlot( child, parent )</code>
	int SetParentSlot(IFunctionHandler* pH, int child, int parent);

	//! <code>Entity.GetParentSlot( child )</code>
	int GetParentSlot(IFunctionHandler* pH, int child);

	//! <code>Entity.BreakToPieces()</code>
	//! <description>Breaks static geometry in slot 0 into sub objects and spawn them as particles or entities.</description>
	int BreakToPieces(IFunctionHandler* pH, int nSlot, int nPiecesSlot, float fExplodeImp, Vec3 vHitPt, Vec3 vHitImp, float fLifeTime, bool bSurfaceEffects);

	//! <code>Entity.AttachSurfaceEffect( nSlot, effect, countPerUnit, form, typ, countScale, sizeScale )</code>
	int AttachSurfaceEffect(IFunctionHandler* pH, int nSlot, const char* effect, bool countPerUnit, const char* form, const char* typ, float countScale, float sizeScale);

	//////////////////////////////////////////////////////////////////////////
	// This method is for engine internal usage.

	//! <code>Entity.ProcessBroadcastEvent()</code>
	int ProcessBroadcastEvent(IFunctionHandler* pH);

	//! <code>Entity.ActivateOutput()</code>
	int ActivateOutput(IFunctionHandler* pH);

	//! <code>Entity.CreateCameraProxy()</code>
	//! <description>Create a proxy camera object for the entity, allows entity to serve as camera source for material assigned on the entity.</description>
	int CreateCameraProxy(IFunctionHandler* pH);

	//! <code>Entity.UnSeenFrames()</code>
	int UnSeenFrames(IFunctionHandler* pH);

	//! <code>Entity.DeleteParticleEmitter(slot)</code>
	//!		<param name="slot">slot number</param>
	//! <description>Deletes particles emitter from 3dengine.</description>
	int DeleteParticleEmitter(IFunctionHandler* pH, int slot);

	//! <code>Entity.RegisterForAreaEvents(enable)</code>
	//!		<param name="enable">0, for disable, any other value for enable.</param>
	//! <description>Registers the script proxy so that it receives area events for this entity.</description>
	int RegisterForAreaEvents(IFunctionHandler* pH, int enable);

	//! <code>Entity.RenderAlways(enable)</code>
	//!		<param name="enable">0, for disable, any other value for enable.</param>
	//! <description>Enables 'always render' on the render node, skipping any kind of culling.</description>
	int RenderAlways(IFunctionHandler* pH, int enable);

	//! <code>Entity.GetTimeOfDayHour()</code>
	//! <returns>current time of day as a float value.</returns>
	int GetTimeOfDayHour(IFunctionHandler* pH);

	//! <code>Entity.CreateDRSProxy()</code>
	//! <description>
	//!    Creates a Dynamic Response System Proxy
	//! </description>
	//! <returns>Returns the ID of the created proxy.</returns>
	int CreateDRSProxy(IFunctionHandler* pH);

private: // -------------------------------------------------------------------------------
	friend class CEntityComponentLuaScript;

	// Helper function to get IEntity pointer from IFunctionHandler
	CEntity* GetEntity(IFunctionHandler* pH);
	Vec3     GetGlobalGravity() const { return Vec3(0, 0, -9.81f); }
	int      SetEntityPhysicParams(IFunctionHandler* pH, IPhysicalEntity* pe, int type, IScriptTable* pTable, ICharacterInstance* pIChar = nullptr);
	EntityId GetEntityID(IScriptTable* pEntityTable);

	ISystem* m_pISystem;

	bool ParseLightParams(IScriptTable* pLightTable, SRenderLight& light);
	bool ParseFogVolumesParams(IScriptTable* pTable, CEntity* pEntity, SFogVolumeProperties& properties);

	// Parse script table to the entity physical params table.
	bool ParsePhysicsParams(IScriptTable* pTable, SEntityPhysicalizeParams& params);

	typedef struct
	{
		Vec3  position;
		Vec3  v0, v1, v2;
		Vec2  uv0, uv1, uv2;
		Vec3  baricentric;
		float distance;
		bool  backface;
		char  material[256];

	} SIntersectionResult;

	static int __cdecl cmpIntersectionResult(const void* v1, const void* v2);
	int                IStatObjRayIntersect(IStatObj* pStatObj, const Vec3& rayOrigin, const Vec3& rayDir, float maxDistance, SIntersectionResult* pOutResult, unsigned int maxResults);

	//////////////////////////////////////////////////////////////////////////
	// Structures used in Physicalize call.
	//////////////////////////////////////////////////////////////////////////
	pe_params_particle                       m_particleParams;
	pe_params_buoyancy                       m_buoyancyParams;
	pe_player_dimensions                     m_playerDimensions;
	pe_player_dynamics                       m_playerDynamics;
	pe_params_area                           m_areaGravityParams;
	SEntityPhysicalizeParams::AreaDefinition m_areaDefinition;
	std::vector<Vec3>                        m_areaPoints;
	pe_params_car                            m_carParams;

	//temp table used by GetPhysicalStats
	SmartScriptTable m_pStats;
};
