// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Attributes;

namespace CryEngine
{
	/// <summary>
	/// Defines the icons that are defined in the ObjectIcons of the Editor assets. These can be used to give an <see cref="EntityComponent"/> an icon in the Sandbox.
	/// </summary>
	public enum IconType
	{
		/// <summary>
		/// This will show the default icon for an <see cref="EntityComponent"/> in the Sandbox.
		/// </summary>
		[IconPath(null)]
		None,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/animobject.bmp.
		/// </summary>
		[IconPath("animobject.bmp")]
		AnimObject,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/AreaTrigger.bmp.
		/// </summary>
		[IconPath("AreaTrigger.bmp")]
		AreaTrigger,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/AudioAreaAmbience.bmp.
		/// </summary>
		[IconPath("AudioAreaAmbience.bmp")]
		AudioAreaAmbience,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/AudioAreaEntity.bmp.
		/// </summary>
		[IconPath("AudioAreaEntity.bmp")]
		AudioAreaEntity,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/AudioAreaRandom.bmp.
		/// </summary>
		[IconPath("AudioAreaRandom.bmp")]
		AudioAreaRandom,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/bird.bmp.
		/// </summary>
		[IconPath("bird.bmp")]
		Bird,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/bug.bmp.
		/// </summary>
		[IconPath("bug.bmp")]
		Bug,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Camera.bmp.
		/// </summary>
		[IconPath("Camera.bmp")]
		Camera,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/character.bmp.
		/// </summary>
		[IconPath("character.bmp")]
		Character,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Checkpoint.bmp.
		/// </summary>
		[IconPath("Checkpoint.bmp")]
		Checkpoint,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/ClipVolume.bmp.
		/// </summary>
		[IconPath("ClipVolume.bmp")]
		ClipVolume,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Clock.bmp.
		/// </summary>
		[IconPath("Clock.bmp")]
		Clock,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Clouds.bmp.
		/// </summary>
		[IconPath("Clouds.bmp")]
		Clouds,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Comment.bmp.
		/// </summary>
		[IconPath("Comment.bmp")]
		Comment,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/DeadBody.bmp.
		/// </summary>
		[IconPath("DeadBody.bmp")]
		DeadBody,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/death.bmp.
		/// </summary>
		[IconPath("death.bmp")]
		Death,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Decal.bmp.
		/// </summary>
		[IconPath("Decal.bmp")]
		Decal,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Dialog.bmp.
		/// </summary>
		[IconPath("Dialog.bmp")]
		Dialog,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/door.bmp.
		/// </summary>
		[IconPath("door.bmp")]
		Door,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/elevator.bmp.
		/// </summary>
		[IconPath("elevator.bmp")]
		Elevator,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/entity_container.bmp.
		/// </summary>
		[IconPath("entity_container.bmp")]
		EntityContainer,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/environmentProbe.bmp.
		/// </summary>
		[IconPath("environmentProbe.bmp")]
		EnvironmentProbe,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/explosion.bmp.
		/// </summary>
		[IconPath("explosion.bmp")]
		Explosion,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/fish.bmp.
		/// </summary>
		[IconPath("fish.bmp")]
		Fish,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Flash.bmp.
		/// </summary>
		[IconPath("Flash.bmp")]
		Flash,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/FlowgraphEntity.bmp.
		/// </summary>
		[IconPath("FlowgraphEntity.bmp")]
		FlowGraphEntity,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Fog.bmp.
		/// </summary>
		[IconPath("Fog.bmp")]
		Fog,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/FogVolume.bmp.
		/// </summary>
		[IconPath("FogVolume.bmp")]
		FogVolume,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/forbiddenarea.bmp.
		/// </summary>
		[IconPath("forbiddenarea.bmp")]
		ForbiddenArea,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/GravitySphere.bmp.
		/// </summary>
		[IconPath("GravitySphere.bmp")]
		GravitySphere,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/hazard.bmp.
		/// </summary>
		[IconPath("hazard.bmp")]
		Hazard,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/health.bmp.
		/// </summary>
		[IconPath("health.bmp")]
		Health,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Item.bmp.
		/// </summary>
		[IconPath("Item.bmp")]
		Item,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Ladder.bmp.
		/// </summary>
		[IconPath("Ladder.bmp")]
		Ladder,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/ledge.bmp.
		/// </summary>
		[IconPath("ledge.bmp")]
		Ledge,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/T.bmp.
		/// </summary>
		[IconPath("T.bmp")]
		Letter_T,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/W.bmp.
		/// </summary>
		[IconPath("W.bmp")]
		Letter_W,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Light.bmp.
		/// </summary>
		[IconPath("Light.bmp")]
		Light,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Lightning.bmp.
		/// </summary>
		[IconPath("Lightning.bmp")]
		Lightning,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/LightPropagationVolume.bmp.
		/// </summary>
		[IconPath("LightPropagationVolume.bmp")]
		LightPropagationVolume,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Magnet.bmp.
		/// </summary>
		[IconPath("Magnet.bmp")]
		Magnet,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/mine.bmp.
		/// </summary>
		[IconPath("mine.bmp")]
		Mine,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/MultiTrigger.bmp.
		/// </summary>
		[IconPath("MultiTrigger.bmp")]
		MultiTrigger,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/ODD.bmp.
		/// </summary>
		[IconPath("ODD.bmp")]
		ODD,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Particles.bmp.
		/// </summary>
		[IconPath("Particles.bmp")]
		Particles,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/physicsobject.bmp.
		/// </summary>
		[IconPath("physicsobject.bmp")]
		PhysicsObject,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/PrecacheCamera.bmp.
		/// </summary>
		[IconPath("PrecacheCamera.bmp")]
		PrecacheCamera,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Prefab.bmp.
		/// </summary>
		[IconPath("Prefab.bmp")]
		Prefab,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/prefabbuilding.bmp.
		/// </summary>
		[IconPath("prefabbuilding.bmp")]
		PrefabBuilding,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/proceduralbuilding.bmp.
		/// </summary>
		[IconPath("proceduralbuilding.bmp")]
		ProceduralBuilding,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/proceduralobject.bmp.
		/// </summary>
		[IconPath("proceduralobject.bmp")]
		ProceduralObject,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Prompt.bmp.
		/// </summary>
		[IconPath("Prompt.bmp")]
		Prompt,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/proximitytrigger.bmp.
		/// </summary>
		[IconPath("proximitytrigger.bmp")]
		ProximityTrigger,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/river.bmp.
		/// </summary>
		[IconPath("river.bmp")]
		River,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/road.bmp.
		/// </summary>
		[IconPath("road.bmp")]
		Road,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/rope.bmp.
		/// </summary>
		[IconPath("rope.bmp")]
		Rope,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/SavePoint.bmp.
		/// </summary>
		[IconPath("SavePoint.bmp")]
		Savepoint,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Schematyc.bmp.
		/// </summary>
		[IconPath("Schematyc.bmp")]
		Schematyc,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Seed.bmp.
		/// </summary>
		[IconPath("Seed.bmp")]
		Seed,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/sequence.bmp.
		/// </summary>
		[IconPath("sequence.bmp")]
		Sequence,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/shake.bmp.
		/// </summary>
		[IconPath("shake.bmp")]
		Shake,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/smartobject.bmp.
		/// </summary>
		[IconPath("smartobject.bmp")]
		SmartObject,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Sound.bmp.
		/// </summary>
		[IconPath("Sound.bmp")]
		Sound,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/spawngroup.bmp.
		/// </summary>
		[IconPath("spawngroup.bmp")]
		SpawnGroup,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/SpawnPoint.bmp.
		/// </summary>
		[IconPath("SpawnPoint.bmp")]
		SpawnPoint,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/spectator.bmp.
		/// </summary>
		[IconPath("spectator.bmp")]
		Spectator,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/switch.bmp.
		/// </summary>
		[IconPath("switch.bmp")]
		Switch,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/TagPoint.bmp.
		/// </summary>
		[IconPath("TagPoint.bmp")]
		TagPoint,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/territory.bmp.
		/// </summary>
		[IconPath("territory.bmp")]
		Territory,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/tornado.bmp.
		/// </summary>
		[IconPath("tornado.bmp")]
		Tornado,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Trigger.bmp.
		/// </summary>
		[IconPath("Trigger.bmp")]
		Trigger,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/User.bmp.
		/// </summary>
		[IconPath("User.bmp")]
		User,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/vehicle.bmp.
		/// </summary>
		[IconPath("vehicle.bmp")]
		Vehicle,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/voxel.bmp.
		/// </summary>
		[IconPath("voxel.bmp")]
		Voxel,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/VVVArea.bmp.
		/// </summary>
		[IconPath("VVVArea.bmp")]
		VVV_Area,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/Water.bmp.
		/// </summary>
		[IconPath("Water.bmp")]
		Water,

		/// <summary>
		/// The icon that is defined at Editor/ObjectIcons/wave.bmp.
		/// </summary>
		[IconPath("wave.bmp")]
		Wave
	}
}