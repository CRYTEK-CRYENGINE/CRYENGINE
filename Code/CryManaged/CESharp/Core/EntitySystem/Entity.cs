// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;
using System.Linq;
using CryEngine.Common;
using System;

namespace CryEngine.EntitySystem
{
	/// <summary>
	/// Checks collisions between registered entities. Will raise collision event on them if a collision occurs.
	/// </summary>
	public class CollisionManager : IEntityEventListener
	{
		private static CollisionManager _instance;

		public static CollisionManager Instance
		{
			get 
			{
				return _instance ?? (_instance = new CollisionManager ());
			}
		}

		/// <summary>
		/// Global event on entity collision. Forwards event to C# Entities.
		/// </summary>
		/// <param name="pEntity">CRYENGINE entity.</param>
		/// <param name="arg1">Collision information.</param>
		public override void OnEntityEvent(IEntity pEntity, SEntityEvent arg1)
		{
			// As the only attached Entity event is the collision event, get physics of the collision.
			EventPhysCollision collision = arg1.GetEventPhysCollision();

			// Retrieve both entities which are part of the collision.
			IEntity entOne = Env.EntitySystem.GetEntityFromPhysics(collision.GetFirstEntity());
			IEntity entTwo = Env.EntitySystem.GetEntityFromPhysics(collision.GetSecondEntity());

			if (entOne == null || entTwo == null)
				return;

			var e1 = Entity.ById (entOne.GetId ());
			var e2 = Entity.ById (entTwo.GetId ());
			e1.RaiseCollision (e2);
			e2.RaiseCollision (e1);
		}
	}

	/// <summary>
	/// Wrapper for the IEntity class of CRYENGINE. Provides properties and wraps functions for easier access of IEntity functions.
	/// Entity must not inherit from IEntity but keep an ID reference to it, for reasons of CRYENGINE internal entity representation.
	/// </summary>
	public class Entity
	{
		public event EventHandler<Entity> OnEntityCollision; ///< Raised if an entity collision occurred.

		private static Dictionary<uint, Entity> _entityById = new Dictionary<uint, Entity> ();
		private static Object _createLock = new Object();
		private static IEntity _newEntity;

		public uint ID { get; private set; } ///< Unique CRYENGINE IEntity ID.
		public Vec3 Position { get { return BaseEntity.GetPos (); } set { BaseEntity.SetPos (value); } } ///< Position of the wrapped IEntity.
		public Quat Rotation { get { return BaseEntity.GetRotation(); } set{ BaseEntity.SetRotation (value);} } ///< Rotation of the wrapped IEntity.
		public Vec3 Scale { get { return BaseEntity.GetScale(); } set{ BaseEntity.SetScale (value);} } ///< Scale of the wrapped IEntity.
		public string Name { get { return BaseEntity.GetName (); } } ///< Name of the wrapped IEntity.
		public IMaterial Material { get { return BaseEntity.GetMaterial(); } set { BaseEntity.SetMaterial (value); } } ///< Material of the wrapped IEntity.
		public bool Exists { get { return BaseEntity != null; } } ///< Determines whether the IEntity object still exists.
		public Matrix34 Transform {	get { return BaseEntity.GetLocalTM (); } set { BaseEntity.SetLocalTM (value); } } ///< Entities local transform matrix. 

		public IEntity BaseEntity
		{
			get 
			{
				if (ID == 0)
					return null;
				return Env.EntitySystem.GetEntity(ID);
			}
		} ///< Returns CRYENGINE entity object.
		public IPhysicalEntity PhysicalEntity
		{
			get 
			{
				return BaseEntity.GetPhysics ();
			}
		} ///< Returns physics object.

		/// <summary>
		/// Creates an entity and registers it as unique for the specific IEntity.
		/// </summary>
		/// <param name="baseEnt">Base ent.</param>
		public Entity(IEntity baseEnt)
		{
			ID = baseEnt.GetId ();
			_entityById [ID] = this;
		}

		/// <summary>
		/// Grabs the ID of the previously instantiated IEntity object. Internally used by Instantiate method.
		/// </summary>
		public Entity()
		{
			ID = _newEntity.GetId ();
			_entityById [ID] = this;
		}

		/// <summary>
		/// Finds an IEntity object on a basis of its ID. Creates or retrieves a unique Entity wrapper class.
		/// </summary>
		/// <returns>The identifier.</returns>
		/// <param name="id">Unique Entity wrapper class.</param>
		public static Entity ById(uint id)
		{
			if (id == 0)
				return null;
			Entity ent;
			if (_entityById.TryGetValue (id, out ent))
				return ent;

			var baseEnt = Env.EntitySystem.GetEntity (id);
			if (baseEnt == null)
				return null;

			return ent = new Entity (baseEnt);
		}

		/// <summary>
		/// Finds an IEntity object on a basis of its Name. Creates or retrieves a unique Entity wrapper class.
		/// </summary>
		/// <returns>The entity name.</returns>
		/// <param name="id">Unique Entity wrapper class.</param>
		public static Entity ByName(string name)
		{
			var ent = _entityById.Values.FirstOrDefault (x => x.Exists && x.Name == name);
			if (ent != null)
				return ent;

			var baseEnt = Env.EntitySystem.FindEntityByName (name);
			if (baseEnt == null)
				return null;
			
			return ent = new Entity (baseEnt);
		}

		/// <summary>
		/// Instantiates an Entity wrapper class, along with a CRYENGINE IEntity object. The IEntity is already setup by various properties as well as physicallized.
		/// Registers the created Entity class as a unique wrapper for the created IEntity object.
		/// </summary>
		/// <param name="pos">Position.</param>
		/// <param name="rot">Rotation.</param>
		/// <param name="scale">Scale.</param>
		/// <param name="model">Model.</param>
		/// <param name="material">Material.</param>
		/// <typeparam name="T">Optionally the Type of a class inheriting by Entity.</typeparam>
		public static T Instantiate<T>(Vec3 pos, Quat rot, float scale = 1.0f, string model = null, string material = null) 
			where T : Entity
		{
			lock(_createLock)
			{
				SEntitySpawnParams spawnParams = new SEntitySpawnParams() 
				{
					pClass = Env.EntitySystem.GetClassRegistry().GetDefaultClass(),
					vPosition = pos,
					vScale = new Vec3(scale)
				};

				_newEntity = Env.EntitySystem.SpawnEntity(spawnParams, true);
				if (model != null)
					_newEntity.LoadGeometry(0, model);
				_newEntity.SetRotation(rot);

				if (material != null)
				{
					IMaterial mat = Env.Engine.GetMaterialManager().LoadMaterial(material);
					_newEntity.SetMaterial(mat);
				}

				var physics = new SEntityPhysicalizeParams() 
				{
					density = 1,
					mass = 0f,
					type = (int)EPhysicalizationType.ePT_Rigid,
				};
				_newEntity.Physicalize(physics);

				return Activator.CreateInstance<T>();
			}
		}

		/// <summary>
		/// Same as Instantiate<T> but will automatically instantiate an Entity class.
		/// </summary>
		/// <param name="pos">Position.</param>
		/// <param name="rot">Rot.</param>
		/// <param name="scale">Scale.</param>
		/// <param name="model">Model.</param>
		/// <param name="material">Material.</param>
		public static Entity Instantiate(Vec3 pos, Quat rot, float scale = 1.0f, string model = null, string material = null)
		{
			return Instantiate<Entity> (pos, rot, scale, model, material);
		}

		/// <summary>
		/// Used by Collision Manager. Do not call directly.
		/// </summary>
		/// <param name="hitEntity">Hit entity.</param>
		public void RaiseCollision(Entity hitEntity)
		{
			if (OnEntityCollision != null)
				OnEntityCollision (hitEntity);
		}

		/// <summary>
		/// Removes the IEntity object from the scene.
		/// </summary>
		public void Remove()
		{
			Env.EntitySystem.RemoveEntity (ID);
			_entityById.Remove (ID);
			ID = 0;
		}

		/// <summary>
		/// Attaches a particle emitter object to a specific entity slot.
		/// </summary>
		/// <param name="slot">Slot.</param>
		/// <param name="emitter">Emitter.</param>
		/// <param name="scale">Scale.</param>
		public void LoadParticleEmitter(int slot, IParticleEffect emitter, float scale = 1.0f)
		{
			var sp = new SpawnParams { fSizeScale = scale };
			BaseEntity.LoadParticleEmitter(slot, emitter, sp, false, false);
		}

		/// <summary>
		/// Returns the world coordinates of a specific helper in a specific entity slot.
		/// </summary>
		/// <returns>The helper position.</returns>
		/// <param name="slot">Slot.</param>
		/// <param name="helperName">Helper name.</param>
		public Vec3 GetHelperPos(int slot, string helperName)
		{
			return BaseEntity.GetStatObj(slot).GetHelperPos(helperName);
		}

		/// <summary>
		/// Sets position, rotation and scale of an entity slot, ba a matrix.
		/// </summary>
		/// <param name="slot">Slot.</param>
		/// <param name="mx">Mx.</param>
		public void SetTM(int slot, Matrix34 mx)
		{
			BaseEntity.SetSlotLocalTM(slot, mx);
		}

		/// <summary>
		/// Removes any geometry that was previously attached to an entity slot.
		/// </summary>
		/// <param name="slot">Slot.</param>
		public void FreeSlot(int slot)
		{
			BaseEntity.FreeSlot (slot);
		}

		/// <summary>
		/// Loads a specific geometry into an entity slot.
		/// </summary>
		/// <param name="slot">Slot.</param>
		/// <param name="url">URL.</param>
		public void LoadGeometry(int slot, string url)
		{
			BaseEntity.LoadGeometry (slot, url);
		}

		public void SetSlotFlag(int slot, EEntitySlotFlags flags)
		{
			BaseEntity.SetSlotFlags (slot, (uint)flags);
		}

		/// <summary>
		/// Returns a particle emitter on the basis od a given slot.
		/// </summary>
		/// <returns>The particle emitter.</returns>
		/// <param name="slot">Slot.</param>
		public IParticleEmitter GetParticleEmitter(int slot)
		{
			return BaseEntity.GetParticleEmitter (slot);
		}

		/// <summary>
		/// Loads a light into a given slot.
		/// </summary>
		/// <param name="slot">Slot.</param>
		/// <param name="light">Light.</param>
		public void LoadLight(int slot, CDLight light)
		{
			BaseEntity.LoadLight (slot, light);
		}

		/// <summary>
		/// Subscribes the entity to collision events.
		/// </summary>
		public void SubscribeToCollision()
		{ 
			Env.EntitySystem.AddEntityEventListener(ID, EEntityEvent.ENTITY_EVENT_COLLISION, CollisionManager.Instance);
		}

		/// <summary>
		/// Unsubscribes the entity from collision events.
		/// </summary>
		public void UnsubscribeFromCollision()
		{ 
			Env.EntitySystem.RemoveEntityEventListener(ID, EEntityEvent.ENTITY_EVENT_COLLISION, CollisionManager.Instance);
		}
	}
}
