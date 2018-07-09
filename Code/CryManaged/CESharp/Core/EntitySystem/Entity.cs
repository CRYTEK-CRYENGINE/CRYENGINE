// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using CryEngine.Animations;
using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	/// <summary>
	/// Wrapper for the IEntity class of CRYENGINE. Provides properties and wraps functions for easier access of IEntity functions.
	/// Entity must not inherit from IEntity but keep an ID reference to it, for reasons of CRYENGINE internal entity representation.
	/// </summary>
	public sealed partial class Entity
	{
		#region Fields
		private PhysicsEntity _physics;
		#endregion

		#region Properties
		/// <summary>
		/// Gets the unique identifier associated with this entity
		/// </summary>
		[SerializeValue]
		public EntityId Id { get; internal set; }

		/// <summary>
		/// Get or set the position of the Entity in local-space.
		/// </summary>
		public Vector3 Position
		{
			get
			{
				return NativeHandle.GetPos();
			}
			set
			{
				NativeHandle.SetPos(value);
			}
		}

		/// <summary>
		/// Get or set the position of the Entity in world-space.
		/// </summary>
		/// <value>The world position.</value>
		public Vector3 WorldPosition
		{
			get
			{
				return NativeHandle.GetWorldPos();
			}
			set
			{
				Matrix3x4 worldTM = NativeHandle.GetWorldTM();
				worldTM.SetTranslation(value);
				NativeHandle.SetWorldTM(worldTM);
			}
		}

		/// <summary>
		/// Get or set the rotation of the Entity in local-space.
		/// </summary>
		public Quaternion Rotation
		{
			get
			{
				return NativeHandle.GetRotation();
			}
			set
			{
				NativeHandle.SetRotation(value);
			}
		}

		/// <summary>
		/// Get or set the rotation of the Entity in world-space.
		/// </summary>
		/// <value>The world rotation.</value>
		public Quaternion WorldRotation
		{
			get
			{
				return NativeHandle.GetWorldRotation();
			}
			set
			{
				var worldMatrix = NativeHandle.GetWorldTM();
				var rotationMatrix = new Matrix3x3(value);
				worldMatrix.SetRotation33(rotationMatrix);
				NativeHandle.SetWorldTM(worldMatrix);
			}
		}

		/// <summary>
		/// Get or set the scale of the Entity in local-space.
		/// </summary>
		public Vector3 Scale
		{
			get
			{
				return NativeHandle.GetScale();
			}
			set
			{
				NativeHandle.SetScale(value);
			}
		}

		/// <summary>
		/// Get the forward direction of the Entity in world-space.
		/// </summary>
		/// <value>The forward.</value>
		public Vector3 Forward
		{
			get
			{
				return NativeHandle.GetForwardDir();
			}
		}

		/// <summary>
		/// Get or set the name of the Entity.
		/// </summary>
		public string Name
		{
			get
			{
				return NativeHandle.GetName();
			}
			set
			{
				NativeHandle.SetName(value ?? "");
			}
		}

		/// <summary>
		/// Get or set the Material of this Entity.
		/// </summary>
		public IMaterial Material
		{
			get
			{
				return NativeHandle.GetMaterial();
			}
			set
			{
				NativeHandle.SetMaterial(value);
			}
		}

		/// <summary>
		/// Hide or unhide this <see cref="T:CryEngine.Entity"/>.
		/// </summary>
		/// <value><c>true</c> if hidden; otherwise, <c>false</c>.</value>
		public bool Hidden
		{
			get
			{
				return NativeHandle.IsHidden();
			}
			set
			{
				NativeHandle.Hide(value);
			}
		}

		/// <summary>
		/// Get a list of all the child Entities of this Entity.
		/// </summary>
		public List<Entity> Children
		{
			get
			{
				var entity = NativeHandle;

				if(NativeHandle == null)
				{
					throw new NullReferenceException();
				}

				var count = entity.GetChildCount();
				var children = new List<Entity>(count);
				for(int i = 0; i < count; ++i)
				{
					var nativeChild = entity.GetChild(i);
					if(nativeChild != null)
					{
						var child = Get(nativeChild.GetId());
						if(child != null)
						{
							children.Add(child);
						}
					}
				}
				return children;
			}
		}

		/// <summary>
		/// Gets or sets the parent of this Entity. Setting a new parent will keep the local-transform of this object.
		/// Use SetParent() to manually decide the behaviour when setting the parent.
		/// </summary>
		/// <value>The parent.</value>
		public Entity Parent
		{
			get
			{
				var entity = NativeHandle;

				var parent = entity.GetParent();
				if(parent == null)
				{
					return null;
				}

				return Get(parent.GetId());
			}
			set
			{
				var entity = NativeHandle;

				entity.DetachThis();
				if(value != null)
				{
					value.AttachChild(this);
				}
			}
		}

		/// <summary>
		/// Determines whether this Entity still exists in the engine.
		/// </summary>
		public bool Exists
		{
			get
			{
				return NativeHandle != null && Global.gEnv.pEntitySystem.GetEntity(Id) != null;
			}
		}

		/// <summary>
		/// The local transform-matrix of this Entity.
		/// </summary>
		public Matrix3x4 Transform
		{
			get
			{
				return NativeHandle.GetLocalTM();
			}
			set
			{
				NativeHandle.SetLocalTM(value);
			}
		}

		/// <summary>
		/// The representation of this Entity in the physics engine.
		/// </summary>
		public PhysicsEntity Physics
		{
			get
			{
				if(_physics == null)
				{
					_physics = new PhysicsEntity(this);
				}

				return _physics;
			}
		}

		[SerializeValue]
		internal IEntity NativeHandle { get; set; }

		[SerializeValue]
		internal IntPtr NativeEntityPointer { get; set; }


		#endregion

		#region Constructors
		internal Entity(IEntity handle)
		{
			NativeHandle = handle;
			Id = NativeHandle.GetId();

			// Access the raw pointer
			NativeEntityPointer = IEntity.getCPtr(handle).Handle;
		}

		internal Entity(IEntity handle, EntityId id)
		{
			NativeHandle = handle;
			Id = id;

			// Access the raw pointer
			NativeEntityPointer = IEntity.getCPtr(handle).Handle;
		}
		#endregion

		/// <summary>
		/// Load a material from a path and set it as the current material.
		/// </summary>
		/// <param name="path"></param>
		public void LoadMaterial(string path)
		{
			var material = Global.gEnv.p3DEngine.GetMaterialManager().LoadMaterial(path);
			if(material != null)
			{
				NativeHandle.SetMaterial(material);
			}
			else
			{
				Log.Error("Unable to set material {0} to {1}!", path, Name);
			}
		}

		/// <summary>
		/// Called before removing the entity from CRYENGINE's entity system.
		/// </summary>
		public void OnRemove()
		{
			NativeHandle = null;
		}

		/// <summary>
		/// Removes the Entity object from the scene.
		/// </summary>
		public void Remove()
		{
			Global.gEnv.pEntitySystem.RemoveEntity(Id);
		}

		#region Components
		/// <summary>
		/// Adds a new instance of the component.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <returns></returns>
		public T AddComponent<T>() where T : EntityComponent, new()
		{
            var componentTypeGUID = EntityComponent.GetComponentTypeGUID<T>();

			return NativeInternals.Entity.AddComponent(NativeEntityPointer, componentTypeGUID.hipart, componentTypeGUID.lopart) as T;
		}

		/// <summary>
		/// Returns the first <see cref="EntityComponent"/> that matches the specified type. Returns null if nothing is found.
		/// </summary>
		public T GetComponent<T>() where T : EntityComponent
		{
            var componentTypeGUID = EntityComponent.GetComponentTypeGUID<T>();

            return NativeInternals.Entity.GetComponent(NativeEntityPointer, componentTypeGUID.hipart, componentTypeGUID.lopart) as T;
		}

		private EntityComponent[] GetComponents(Type type)
		{
            var componentTypeGUID = EntityComponent.GetComponentTypeGUID(type);
            
			EntityComponent[] baseComponents;

			NativeInternals.Entity.GetComponents(NativeEntityPointer, componentTypeGUID.hipart, componentTypeGUID.lopart, out baseComponents);

			return baseComponents;
		}

		/// <summary>
		/// Get all components that match the specified type. Returns null if nothing is found.
		/// </summary>
		/// <returns>Every <see cref="EntityComponent"/> that matches the type <typeparamref name="T"/>.</returns>
		/// <typeparam name="T">The type the components have to match.</typeparam>
		public List<T> GetComponents<T>() where T: EntityComponent
		{
			var baseComponents = GetComponents(typeof(T));

			if(baseComponents == null || baseComponents.Length == 0)
			{
				return null;
			}

			var components = new List<T>(baseComponents.Length);
			foreach(var baseComponent in baseComponents)
			{
				var component = baseComponent as T;
				if(component != null)
				{
					components.Add(component);
				}
			}

			return components;
		}

		/// <summary>
		/// Gets the first <see cref="EntityComponent"/> that implements the specified interface. Returns null if nothing was found.
		/// </summary>
		/// <returns>The component with interface.</returns>
		/// <typeparam name="T">The type of interface that has to be implemented.</typeparam>
		public T GetComponentWithInterface<T>() where T : class
		{
			// Check if the type is an interface here, since generic constraints don't have an option for interfaces.
			if(!typeof(T).IsInterface)
			{
				throw new ArgumentException(string.Format("Class {0} is not an interface! Use GetComponent() for normal classes instead.", typeof(T).Name));
			}

			var baseComponents = GetComponents(typeof(EntityComponent));

			if(baseComponents == null || baseComponents.Length == 0)
			{
				return null;
			}

			foreach(var baseComponent in baseComponents)
			{
				var component = baseComponent as T;
				if(component != null)
				{
					return component;
				}
			}

			return null;
		}

		/// <summary>
		/// Gets all components that implement the specified interface. Returns null if nothing was found.
		/// </summary>
		/// <returns>The components that implement the interface.</returns>
		/// <typeparam name="T">The type of interface that has to be implemented.</typeparam>
		public List<T> GetComponentsWithInterface<T>() where T : class
		{
			// Check if the type is an interface here, since generic constraints don't have an option for interfaces.
			if(!typeof(T).IsInterface)
			{
				throw new ArgumentException(string.Format("Class {0} is not an interface! Use GetComponents() for normal classes instead.", typeof(T).Name));
			}

			var baseComponents = GetComponents(typeof(EntityComponent));

			if(baseComponents == null || baseComponents.Length == 0)
			{
				return null;
			}

			var components = new List<T>(baseComponents.Length);
			foreach(var baseComponent in baseComponents)
			{
				var component = baseComponent as T;
				if(component != null)
				{
					components.Add(component);
				}
			}

			return components.Count > 0 ? components : null;
		}

		/// <summary>
		/// Returns the first EntityComponent that matches the specified type, if it exists. Otherwise, adds a new instance of the component.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <returns></returns>
		public T GetOrCreateComponent<T>() where T : EntityComponent, new()
		{
            var componentTypeGUID = EntityComponent.GetComponentTypeGUID<T>();

            return NativeInternals.Entity.GetOrCreateComponent(NativeEntityPointer, componentTypeGUID.hipart, componentTypeGUID.lopart) as T;
		}

		/// <summary>
		/// Returns true if this Entity has the specified EntityComponent, otherwise false.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <returns></returns>
		public bool HasComponent<T>() where T : EntityComponent
		{
			return GetComponent<T>() != null;
		}
		#endregion

		/// <summary>
		/// Set the parent Entity of this Entity. 
		/// </summary>
		/// <param name="parent"></param>
		/// <param name="keepWorldTransform"></param>
		public void SetParent(Entity parent, bool keepWorldTransform = false)
		{
			var entity = NativeHandle;

            uint flags = !keepWorldTransform ? 0 : (uint)IEntity.EAttachmentFlags.ATTACHMENT_KEEP_TRANSFORMATION;
			entity.DetachThis(flags);
			if(parent != null)
			{
				parent.AttachChild(this, new SChildAttachParams((int)flags));
			}
		}

		/// <summary>
		/// Attach an Entity to this Entity as a child.
		/// </summary>
		/// <param name="child"></param>
		public void AttachChild(Entity child)
		{
			var entity = NativeHandle;

			if(child == null)
			{
				throw new ArgumentNullException(nameof(child));
			}

			entity.AttachChild(child.NativeHandle);
		}

		/// <summary>
		/// Attach an Entity to this Entity as a child. The attach-behavior is defined in the attachParams.
		/// </summary>
		/// <param name="child"></param>
		/// <param name="attachParams"></param>
		public void AttachChild(Entity child, SChildAttachParams attachParams)
		{
			var entity = NativeHandle;

			if(child == null)
			{
				throw new ArgumentNullException(nameof(child));
			}

			entity.AttachChild(child.NativeHandle, attachParams);
		}

		/// <summary>
		/// Attaches a particle emitter object to a specific entity slot and loads it with <paramref name="particleEffect"/>.
		/// </summary>
		/// <param name="slot">The slot to load the particle in.</param>
		/// <param name="particleEffect">Particle effect to load.</param>
		/// <param name="scale">Scale of the emitter.</param>
		public void LoadParticleEmitter(int slot, ParticleEffect particleEffect, float scale = 1.0f)
		{
			var sp = new SpawnParams { fSizeScale = scale };
			NativeHandle.LoadParticleEmitter(slot, particleEffect.NativeHandle, sp, false, false);
		}

		/// <summary>
		/// Returns the world coordinates of a specific helper in a specific entity slot.
		/// </summary>
		/// <returns>The helper position.</returns>
		/// <param name="slot">Slot.</param>
		/// <param name="helperName">Helper name.</param>
		public Vector3 GetHelperPos(int slot, string helperName)
		{
			var statObj = NativeHandle.GetStatObj(slot);

			if(statObj != null)
			{
				return statObj.GetHelperPos(helperName);
			}

			return Vector3.Zero;
		}

		/// <summary>
		/// Sets position, rotation and scale of an entity slot, ba a matrix.
		/// </summary>
		/// <param name="slot">Slot.</param>
		/// <param name="mx">Mx.</param>
		public void SetGeometrySlotLocalTransform(int slot, Matrix3x4 mx)
		{
			NativeHandle.SetSlotLocalTM(slot, mx);
		}

		/// <summary>
		/// Removes any geometry that was previously attached to an entity slot.
		/// </summary>
		/// <param name="slot">Slot.</param>
		public void FreeGeometrySlot(int slot)
		{
			NativeHandle.FreeSlot(slot);
		}

		/// <summary>
		/// Loads a specific geometry into an entity slot.
		/// </summary>
		/// <param name="slot">Slot.</param>
		/// <param name="url">URL.</param>
		public void LoadGeometry(int slot, string url)
		{
			NativeHandle.LoadGeometry(slot, url);
		}

		/// <summary>
		/// Load a primitive geometry shape from the engine-assets.
		/// </summary>
		/// <param name="slot"></param>
		/// <param name="primitiveShape"></param>
		public void LoadGeometry(int slot, Primitives primitiveShape)
		{
			string[] urls = new string[]
			{
				"%ENGINE%/EngineAssets/Objects/primitive_box.cgf",
				"%ENGINE%/EngineAssets/Objects/primitive_cube.cgf",
				"%ENGINE%/EngineAssets/Objects/primitive_cylinder.cgf",
				"%ENGINE%/EngineAssets/Objects/primitive_plane.cgf",
				"%ENGINE%/EngineAssets/Objects/primitive_pyramid.cgf",
				"%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf"
			};

			NativeHandle.LoadGeometry(slot, urls[(int)primitiveShape]);
		}

		/// <summary>
		/// Loads a character to the specified slot, or to next available slot.
		/// If same character is already loaded in this slot, operation is ignored.
		/// If this slot number is occupied by a different kind of object it is overwritten.
		/// </summary>
		/// <param name="slot">The index of the character slot.</param>
		/// <param name="url">The path to the character file.</param>
		/// <returns>Slot id where the object was loaded, or -1 if loading failed.</returns>
		public int LoadCharacter(int slot, string url)
		{
			return NativeHandle.LoadCharacter(slot, url);
		}

		/// <summary>
		/// Method to get the character at the specified slot.
		/// </summary>
		/// <param name="slot">The index of the character slot.</param>
		/// <returns>The CharacterInstance or null if character with this slot does not exist.</returns>
		public Character GetCharacter(int slot)
		{
			var nativeCharacter = NativeHandle.GetCharacter(slot);
			if(nativeCharacter == null)
			{
				return null;
			}
			return new Character(nativeCharacter);
		}

		/// <summary>
		/// Set the flags for a specific slot.
		/// </summary>
		/// <param name="slot"></param>
		/// <param name="flags"></param>
		public void SetSlotFlag(int slot, EntitySlotFlags flags)
		{
			NativeHandle.SetSlotFlags(slot, (uint)flags);
		}

		/// <summary>
		/// Returns a particle emitter on the basis of a given slot.
		/// </summary>
		/// <returns>The particle emitter.</returns>
		/// <param name="slot">Slot.</param>
		public ParticleEmitter GetParticleEmitter(int slot)
		{
			var nativeEmitter = NativeHandle.GetParticleEmitter(slot);
			return nativeEmitter == null ? null : new ParticleEmitter(nativeEmitter);
		}

		/// <summary>
		/// Loads a light into a given slot.
		/// </summary>
		/// <param name="slot">Slot.</param>
		/// <param name="light">Light.</param>
		public void LoadLight(int slot, DynamicLight light)
		{
			NativeHandle.LoadLight(slot, light.NativeHandle);
		}

		/// <summary>
		/// Specify the ratio at which this entity is made invisible based on distance.
		/// Ratio is 0.0 - 1.0, with 1.0 being 100% visibility regardless of distance.
		/// </summary>
		/// <param name="viewDistanceRatio">View distance ratio.</param>
		public void SetViewDistanceRatio(float viewDistanceRatio)
		{
			viewDistanceRatio = MathHelpers.Clamp01(viewDistanceRatio);

			NativeHandle.SetViewDistRatio((int)(viewDistanceRatio * 255));
		}
	}
}
