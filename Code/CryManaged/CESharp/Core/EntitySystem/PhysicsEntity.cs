// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine.EntitySystem
{
	/// <summary>
	/// Contains the data a collision event. If the data is an array the first element will always be the <see cref="EntityComponent"/> that received the <see cref="EntityComponent.OnCollision(CollisionEvent)"/> call.
	/// </summary>
	public struct CollisionEvent
	{
		/// <summary>
		/// The objects involved in the collision. The first element will always be the <see cref="EntityComponent"/> that received the <see cref="EntityComponent.OnCollision(CollisionEvent)"/> call.
		/// </summary>
		public PhysicsObject[] PhysicsObjects { get; internal set; }

		/// <summary>
		/// Contact point in world space.
		/// </summary>
		public Vector3 Point { get; internal set; }

		/// <summary>
		/// Contact point normal.
		/// </summary>
		public Vector3 Normal { get; internal set; }
		
		/// <summary>
		/// Velocities at the contact point.
		/// </summary>
		public Vector3[] Velocities { get; internal set; }

		/// <summary>
		/// The mass of each <see cref="PhysicsObject"/>.
		/// </summary>
		public float[] Masses { get; internal set; }

		/// <summary>
		/// Contact's penetration depth.
		/// </summary>
		public float PenetrationDepth { get; internal set; }

		/// <summary>
		/// Impulse applied by the solver to resolve the collision.
		/// </summary>
		public float ResolvingImpulse { get; internal set; }

		/// <summary>
		/// Characteristic size of the contact area.
		/// </summary>
		public float Radius { get; internal set; }

		/// <summary>
		/// Maximum allowed size of decals caused by this collision.
		/// </summary>
		public float MaxDecalSize { get; internal set; }
	}

	/// <summary>
	/// Representation of an object in the physics engine
	/// </summary>
	public class PhysicsObject
	{
		/// <summary>
		/// Handle to the entity of this PhysicsObject. Use <see cref="OwnerEntity"/> to get the handle safely.
		/// </summary>
		protected Entity _entity;

		[SerializeValue]
		internal virtual IPhysicalEntity NativeHandle { get; private set; }

		/// <summary>
		/// The physicalized type of this <see cref="PhysicsObject"/>. If the <see cref="PhysicsObject"/> is not physicalized this will be <c>PhysicalizationType.None</c>.
		/// </summary>
		/// <value>The physicalized type of the <see cref="PhysicsObject"/>.</value>
		[SerializeValue]
		public PhysicalizationType PhysicsType{ get; protected set;} = PhysicalizationType.None;

		/// <summary>
		/// Gets the Entity that this <see cref="PhysicsObject"/> belongs to. 
		/// It's possible for this to be null if the owner is not an Entity but a Brush or Terrain.
		/// </summary>
		public virtual Entity OwnerEntity
		{
			get
			{
				if (_entity == null)
				{
					var entityHandle = Global.gEnv.pEntitySystem.GetEntityFromPhysics(NativeHandle);
					if (entityHandle != null)
					{
						_entity = new Entity(entityHandle, entityHandle.GetId());
					}
				}

				return _entity;
			}
		}

		internal PhysicsObject() { }

		internal PhysicsObject(IPhysicalEntity handle)
		{
			NativeHandle = handle;
		}

		/// <summary>
		/// Get or set the velocity of this <see cref="PhysicsObject"/>.
		/// </summary>
		public Vector3 Velocity
		{
			get
			{
				var status = new pe_status_dynamics();
				NativeHandle.GetStatus(status);
				return status.v;
			}
			set
			{
				var action = new pe_action_set_velocity();
				action.v = value;
				NativeHandle.Action(action);
			}
		}

		/// <summary>
		/// Get or set the angular velocity of this <see cref="PhysicsObject"/>.
		/// </summary>
		/// <value>The angular velocity.</value>
		public Vector3 AngularVelocity
		{
			get
			{
				var status = new pe_status_dynamics();
				NativeHandle.GetStatus(status);
				return status.w;
			}
			set
			{
				var action = new pe_action_set_velocity();
				action.w = value;
				NativeHandle.Action(action);
			}
		}

		/// <summary>
		/// The acceleration that is currently being applied to this <see cref="PhysicsObject"/>.
		/// </summary>
		/// <value>The linear acceleration.</value>
		public Vector3 LinearAcceleration
		{
			get
			{
				var status = new pe_status_dynamics();
				NativeHandle.GetStatus(status);
				return status.a;
			}
		}

		/// <summary>
		/// The angular acceleration that is currently being applied to this <see cref="PhysicsObject"/>.
		/// </summary>
		/// <value>The angular acceleration.</value>
		public Vector3 AngularAcceleration
		{
			get
			{
				var status = new pe_status_dynamics();
				NativeHandle.GetStatus(status);
				return status.wa;
			}
		}

		/// <summary>
		/// Adds an impulse to this <see cref="PhysicsObject"/>.
		/// </summary>
		[Obsolete("Impulse is obsolete, use AddImpulse instead.")]
		public Vector3 Impulse
		{
			set
			{
				var action = new pe_action_impulse();
				action.impulse = value;
				NativeHandle.Action(action);
			}
		}

		/// <summary>
		/// Set the velocity of this <see cref="PhysicsObject"/>.
		/// </summary>
		/// <param name="velocity"></param>
		public void SetVelocity(Vector3 velocity)
		{
			var action = new pe_action_set_velocity();
			action.v = velocity;
			NativeHandle.Action(action);
		}

		/// <summary>
		/// Adds an impulse to this <see cref="PhysicsObject"/>.
		/// </summary>
		/// <param name="impulse">Direction and length of the impulse.</param>
		public void AddImpulse(Vector3 impulse)
		{
			var action = new pe_action_impulse();
			action.impulse = impulse;
			NativeHandle.Action(action);
		}

		/// <summary>
		/// Adds an impulse to this <see cref="PhysicsObject"/>. The impulse will be applied to the point in world-space.
		/// </summary>
		/// <param name="impulse">Direction and length of the impulse.</param>
		/// <param name="point">Point of application in world-space.</param>
		public void AddImpulse(Vector3 impulse, Vector3 point)
		{
			var action = new pe_action_impulse();
			action.impulse = impulse;
			action.point = point;
			NativeHandle.Action(action);
		}

		/// <summary>
		/// Adds an angled impulse to this <see cref="PhysicsObject"/>.
		/// </summary>
		/// <param name="impulse">Angle and length of the impulse.</param>
		public void AddAngImpulse(Vector3 impulse)
		{
			var action = new pe_action_impulse
			{
				angImpulse = impulse
			};
			NativeHandle.Action(action);
		}

		/// <summary>
		/// Adds an angled impulse to this <see cref="PhysicsObject"/>. The impulse will be applied to the point in world-space.
		/// </summary>
		/// <param name="impulse">Direction and length of the impulse.</param>
		/// <param name="point">Point of application in world-space.</param>
		public void AddAngImpulse(Vector3 impulse, Vector3 point)
		{
			var action = new pe_action_impulse
			{
				angImpulse = impulse,
				point = point
			};
			NativeHandle.Action(action);
		}


		/// <summary>
		/// Move this <see cref="PhysicsObject"/> in a direction.
		/// </summary>
		/// <param name="direction"></param>
		public void Move(Vector3 direction)
		{
			var action = new pe_action_move();

			//Jump mode 2 - just adds to current velocity
			action.iJump = 2;
			action.dir = direction;

			NativeHandle.Action(action);
		}

		/// <summary>
		/// Move this <see cref="PhysicsObject"/> in a direction, but apply the velocity instantly.
		/// </summary>
		/// <param name="direction"></param>
		public void Jump(Vector3 direction)
		{
			var action = new pe_action_move();

			//Jump mode 1 - instant velocity change
			action.iJump = 1;
			action.dir = direction;
			
			NativeHandle.Action(action);
		}

		/// <summary>
		/// Adds and executes a physics action of type T.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="setParams">Sets the parameters of the action to be performed.</param>
		public void Action<T>(Action<T> setParams) where T : pe_action
		{
			var actionParams = Activator.CreateInstance<T>();
			setParams(actionParams);
			NativeHandle.Action(actionParams);
		}

		/// <summary>
		/// Get the status of the <see cref="PhysicsEntity"/>. 
		/// The type of status can be specified by choosing from for example <see cref="DynamicsStatus"/>, <see cref="LivingStatus"/>, or <see cref="VehicleStatus"/>.
		/// </summary>
		/// <typeparam name="T">The type of the <see cref="BasePhysicsStatus"/> to be retrieved, for example <see cref="DynamicsStatus"/></typeparam>
		/// <returns></returns>
		public T GetStatus<T>() where T : BasePhysicsStatus, new()
		{
			var status = new T();
			var nativeStatus = status.ToBaseNativeStatus();
			NativeHandle.GetStatus(nativeStatus);
			status.NativeToManaged(nativeStatus);
			return status;
		}

		/// <summary>
		/// Set the physics-type of this <see cref="PhysicsEntity"/>.
		/// </summary>
		/// <param name="nativeType"></param>
		protected void SetType(pe_type nativeType)
		{
			switch(nativeType)
			{
			case pe_type.PE_NONE:
				PhysicsType = PhysicalizationType.None;
				break;
			case pe_type.PE_STATIC:
				PhysicsType = PhysicalizationType.Static;
				break;
			case pe_type.PE_RIGID:
				PhysicsType = PhysicalizationType.Rigid;
				break;
			case pe_type.PE_WHEELEDVEHICLE:
				PhysicsType = PhysicalizationType.WheeledVehicle;
				break;
			case pe_type.PE_LIVING:
				PhysicsType = PhysicalizationType.Living;
				break;
			case pe_type.PE_PARTICLE:
				PhysicsType = PhysicalizationType.Particle;
				break;
			case pe_type.PE_ARTICULATED:
				PhysicsType = PhysicalizationType.Articulated;
				break;
			case pe_type.PE_ROPE:
				PhysicsType = PhysicalizationType.Rope;
				break;
			case pe_type.PE_SOFT:
				PhysicsType = PhysicalizationType.Soft;
				break;
			case pe_type.PE_AREA:
				PhysicsType = PhysicalizationType.Area;
				break;
			case pe_type.PE_GRID:
				throw new NotImplementedException();
			default:
				throw new ArgumentOutOfRangeException();
			}
		}
	}

	/// <summary>
	/// Representation of an Entity in the physics engine
	/// </summary>
	public sealed class PhysicsEntity : PhysicsObject
	{
		/// <summary>
		/// Gets the Entity that this <see cref="PhysicsEntity"/> belongs to.
		/// </summary>
		public override Entity OwnerEntity { get { return _entity; } }

		internal override IPhysicalEntity NativeHandle
		{
			get
			{
				return OwnerEntity.NativeHandle.GetPhysicalEntity();
			}
		}

		internal PhysicsEntity(Entity entity)
		{
			_entity = entity;
		}

		/// <summary>
		/// Physicalize the <see cref="PhysicsEntity"/> with the specified mass and of the specified type.
		/// </summary>
		/// <param name="mass"></param>
		/// <param name="type"></param>
		public void Physicalize(float mass, PhysicalizationType type)
		{
			if(type == PhysicalizationType.None)
			{
				Log.Error<PhysicsEntity>("Can't Physicalize an entity with PhysicalizationType.None!");
				return;
			}

			OwnerEntity.NativeHandle.Physicalize(new SEntityPhysicalizeParams
			{
				mass = mass,
				type = (int)type
			});
			PhysicsType = type;
		}

		/// <summary>
		/// Physicalize the <see cref="PhysicsEntity"/> with the specified Physicalize parameters.
		/// </summary>
		/// <param name="mass">Mass of the entity in Kilograms.</param>
		/// <param name="type"></param>
		[Obsolete("Using EPhysicalizizationType is obsolete. Use PhysicalizationType instead.")]
		public void Physicalize(float mass, EPhysicalizationType type)
		{
			Physicalize(mass, -1, type);
			SetType((pe_type)type);
		}

		/// <summary>
		/// Physicalize the <see cref="PhysicsEntity"/> with the specified Physicalize parameters.
		/// </summary>
		/// <param name="mass">Mass of the entity in Kilograms.</param>
		/// <param name="density"></param>
		/// <param name="type"></param>
		[Obsolete("Using EPhysicalizizationType is obsolete. Use PhysicalizationType instead.")]
		public void Physicalize(float mass, int density, EPhysicalizationType type)
		{
			OwnerEntity.NativeHandle.Physicalize(new SEntityPhysicalizeParams
			{
				mass = mass,
				density = density,
				type = (int)type
			});
			SetType((pe_type)type);
		}

		/// <summary>
		/// Physicalize the <see cref="PhysicsEntity"/> with the specified Physicalize parameters.
		/// </summary>
		/// <param name="phys"></param>
		[Obsolete("Using SEntityPhysicalizeParams is obsolete. Use EntityPhysicalizeParams instead.")]
		public void Physicalize(SEntityPhysicalizeParams phys)
		{
			OwnerEntity.NativeHandle.Physicalize(phys);
			SetType((pe_type)phys.type);
		}

		/// <summary>
		/// Physicalize the <see cref="PhysicsEntity"/> as a rigid-entity using default values.
		/// </summary>
		public void Physicalize()
		{
			Physicalize(-1, PhysicalizationType.Rigid);
			PhysicsType = PhysicalizationType.Rigid;
		}

		/// <summary>
		/// Physicalize the <see cref="PhysicsEntity"/> with the specified Physicalize parameters.
		/// </summary>
		/// <param name="parameters"></param>
		public void Physicalize(EntityPhysicalizeParams parameters)
		{
			if(parameters == null)
			{
				throw new ArgumentNullException(nameof(parameters));
			}

			//We could do a using here, but this could result in strange behaviour if these parameters are re-used to physicalize another entity.
			var nativeParameters = parameters.ToNativeParams();
			OwnerEntity.NativeHandle.Physicalize(nativeParameters);
			SetType(parameters.Type);
		}
	}
}
