// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Runtime.InteropServices;
using System.Text;
using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	/// <summary>
	/// Represents a component that can be attached to an entity at runtime
	/// Automatically exposes itself to Schematyc for usage by designers.
	/// 
	/// Systems reference entity components by GUID, for example when serializing 
	/// to file to detect which type a component belongs to.
	/// By default we generate a GUID automatically based on the EntityComponent 
	/// implementation type, however this will result in serialization breaking 
	/// if you rename it.
	/// To circumvent this, use either System.Runtime.Interopservices.GuidAttribute 
	/// or the Guid property of the CryEngine.EntityComponent to explicitly specify your desired GUID.
	/// Example:
	/// <c>[Guid("C47DF64B-E1F9-40D1-8063-2C533A1CE7D5")]
	/// public class MyComponent : public EntityComponent {}</c>
	/// or
	/// <c>[EntityComponent(Guid="C47DF64B-E1F9-40D1-8063-2C533A1CE7D5")]
	/// public class MyComponent : public EntityComponent {}</c>
	/// </summary>
	public abstract class EntityComponent
	{
		internal struct GUID
		{
			public ulong lopart;
			public ulong hipart;
		}

		/// <summary>
		/// Unique information for each registered component type
		/// </summary>
		internal class TypeInfo
		{
			/// <summary>
			/// Represents a Schematyc signal, and the information required to execute it
			/// </summary>
			public struct Signal
			{
				public int id;
				public Type delegateType;
			}

			public Type type;
			public GUID guid;
			// Signals that can be sent to Schematyc
			public List<Signal> signals;
		}

		[NonSerialized]
		internal static List<TypeInfo> _componentTypes = new List<TypeInfo>();

		internal static GUID GetComponentTypeGUID<T>()
		{
			return GetComponentTypeGUID(typeof(T));
		}

		internal static GUID GetComponentTypeGUID(Type type)
		{
			foreach(var typeInfo in _componentTypes)
			{
				if(typeInfo.type == type)
				{
					return typeInfo.guid;
				}
			}

			throw new KeyNotFoundException("Component was not registered!");
		}

		internal static Type GetComponentTypeByGUID(GUID guid)
		{
			foreach(var typeInfo in _componentTypes)
			{
				if(typeInfo.guid.hipart == guid.hipart && typeInfo.guid.lopart == guid.lopart)
				{
					return typeInfo.type;
				}
			}

			return null;
		}

		/// <summary>
		/// The <see cref="CryEngine.Entity"/> that owns this <see cref="EntityComponent"/>.
		/// </summary>
		[SerializeValue]
		public Entity Entity { get; private set; }

		#region Functions
		internal void SetEntity(IntPtr entityHandle, uint id)
		{
			Entity = new Entity(new IEntity(entityHandle, false), id);
		}

		/// <summary>
		/// Sends a signal attributed with [SchematycSignal] to Schematyc
		/// </summary>
		/// <typeparam name="T">A delegate contained in an EntityComponent decorated with [SchematycSignal]</typeparam>
		/// <param name="args">The exact arguments specified with the specified delegate</param>
		public void SendSignal<T>(params object[] args) where T : class
		{
			var thisType = GetType();

			foreach(var typeInfo in _componentTypes)
			{
				if(typeInfo.type == thisType)
				{
					if(typeInfo.signals != null)
					{
						foreach(var signal in typeInfo.signals)
						{
							if(signal.delegateType == typeof(T))
							{
								NativeInternals.Entity.SendComponentSignal(Entity.NativeEntityPointer, typeInfo.guid.hipart, typeInfo.guid.lopart, signal.id, args);
								return;
							}
						}
					}
				}
			}

			throw new ArgumentException("Tried to send invalid signal to component!");
		}
		#endregion

		#region Entity Event Methods
		/// <summary>
		/// Called whenever the transform of this <see cref="CryEngine.Entity"/> is changed.
		/// </summary>
		protected virtual void OnTransformChanged() { }

		/// <summary>
		/// Called when the component is initialized. This happens when a level is loaded that contains the component, or when the component is placed in the level.
		/// </summary>
		protected virtual void OnInitialize() { }

		/// <summary>
		/// Called every frame. In the Sandbox this is called every frame if game-mode is enabled.
		/// </summary>
		/// <param name="frameTime"></param>
		protected virtual void OnUpdate(float frameTime) { }

		/// <summary>
		/// Called every frame in the Sandbox.
		/// </summary>
		/// <param name="frameTime"></param>
		protected virtual void OnEditorUpdate(float frameTime) { }

		/// <summary>
		/// Called whenever the Sandbox goes in or out of game-mode.
		/// </summary>
		/// <param name="enterGame"></param>
		protected virtual void OnEditorGameModeChange(bool enterGame) { }

		/// <summary>
		/// Called when this <see cref="EntityComponent"/> is removed.
		/// </summary>
		protected virtual void OnRemove() { }

		/// <summary>
		/// Called when this <see cref="CryEngine.Entity"/> is hidden.
		/// </summary>
		protected virtual void OnHide() { }

		/// <summary>
		/// Called when this <see cref="CryEngine.Entity"/> is not hidden anymore.
		/// </summary>
		protected virtual void OnUnhide() { }

		/// <summary>
		/// Called at the start of the game when all entities have initialized. In the Sandbox this is called when game-mode is enabled.
		/// </summary>
		protected virtual void OnGameplayStart() { }

		/// <summary>
		/// Called when this <see cref="CryEngine.Entity"/> has a collision. The collision is not neceserilly with another <see cref="CryEngine.Entity"/>.
		/// </summary>
		/// <param name="collisionEvent"></param>
		protected virtual void OnCollision(CollisionEvent collisionEvent) { }

		private void OnCollisionInternal(IntPtr sourceEntityPhysics, IntPtr targetEntityPhysics, Vector3 point, Vector3 normal, Vector3 ownVelocity, Vector3 otherVelocity, float ownMass, float otherMass, float penetrationDepth, float normImpulse, float radius, float decalMaxSize)
		{
			var collisionEvent = new CollisionEvent()
			{
				MaxDecalSize = decalMaxSize,
				Normal = normal,
				ResolvingImpulse = normImpulse,
				PenetrationDepth = penetrationDepth,
				Point = point,
				Radius = radius,
				Masses = new float[] { ownMass, otherMass },
				Velocities = new Vector3[] { ownVelocity, otherVelocity },
				PhysicsObjects = new PhysicsObject[]
				{
					new PhysicsObject(new IPhysicalEntity(sourceEntityPhysics, false)),
					new PhysicsObject(new IPhysicalEntity(targetEntityPhysics, false))
				}
			};
			OnCollision(collisionEvent);
		}

		/// <summary>
		/// Called every frame before the physics world is updated.
		/// </summary>
		/// <param name="frameTime"></param>
		protected virtual void OnPrePhysicsUpdate(float frameTime) { }
		#endregion

		#region Statics
		private static string TypeToHash(Type type)
		{
			string result = string.Empty;
			string input = type.FullName;
			using(SHA384 hashGenerator = SHA384.Create())
			{
				var hash = hashGenerator.ComputeHash(Encoding.Default.GetBytes(input));
				var shortHash = new byte[16];
				for(int i = 0, j = 0; i < hash.Length; ++i, ++j)
				{
					if(j >= shortHash.Length)
					{
						j = 0;
					}
					unchecked
					{
						shortHash[j] += hash[i];
					}
				}
				result = BitConverter.ToString(shortHash);
				result = result.Replace("-", string.Empty);
			}
			return result;
		}

		/// <summary>
		/// Register the given entity prototype class. 
		/// </summary>
		/// <param name="entityComponentType">Entity class prototype.</param>
		internal static void TryRegister(Type entityComponentType)
		{
			if(!entityComponentType.IsAbstract && !entityComponentType.IsValueType && entityComponentType.GetConstructor(Type.EmptyTypes) == null)
			{
				string errorMessage = string.Format("Unable to register component of type {0}!{2}" +
													"The component doesn't have a default parameterless constructor defined which is not supported on classes inheriting from {1}." +
													"The {1} will overwrite the values of the constructor with the serialized values of the component." +
													"Use {3} instead to initialize your component.",
													entityComponentType.FullName, nameof(EntityComponent), Environment.NewLine, nameof(EntityComponent.OnInitialize));
				throw new NotSupportedException(errorMessage);
			}

			var typeInfo = new TypeInfo
			{
				type = entityComponentType
			};
			_componentTypes.Add(typeInfo);

			var guidAttribute = entityComponentType.GetCustomAttribute<GuidAttribute>(false);
			var componentAttribute = entityComponentType.GetCustomAttribute<EntityComponentAttribute>(false);
			bool hasGuid = false;
			if(guidAttribute != null)
			{
				Guid guid;
				if(ValidateGuid(entityComponentType.Name, guidAttribute.Value, out guid))
				{
					var guidArray = guid.ToByteArray();
					typeInfo.guid.hipart = BitConverter.ToUInt64(guidArray, 0);
					typeInfo.guid.lopart = BitConverter.ToUInt64(guidArray, 8);
					hasGuid = true;
				}
			}

			if(!hasGuid && componentAttribute != null)
			{
				if(!string.IsNullOrWhiteSpace(componentAttribute.Guid))
				{
					Guid guid;
					if(ValidateGuid(entityComponentType.Name, componentAttribute.Guid, out guid))
					{
						var guidArray = guid.ToByteArray();
						typeInfo.guid.hipart = BitConverter.ToUInt64(guidArray, 0);
						typeInfo.guid.lopart = BitConverter.ToUInt64(guidArray, 8);
						hasGuid = true;
					}
				}
			}

			if(!hasGuid)
			{
				// Fall back to generating GUID based on type
				var guidString = Engine.TypeToHash(entityComponentType);
				var half = (int)(guidString.Length / 2.0f);
				if(!ulong.TryParse(guidString.Substring(0, half), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out typeInfo.guid.hipart))
				{
					Log.Error("Failed to parse {0} to UInt64", guidString.Substring(0, half));
				}
				if(!ulong.TryParse(guidString.Substring(half, guidString.Length - half), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out typeInfo.guid.lopart))
				{
					Log.Error("Failed to parse {0} to UInt64", guidString.Substring(half, guidString.Length - half));
				}
			}

			if(componentAttribute == null)
			{
				componentAttribute = new EntityComponentAttribute();
			}

			if(componentAttribute.Name.Length == 0)
			{
				componentAttribute.Name = entityComponentType.Name;
			}

			if(entityComponentType.IsAbstract || entityComponentType.IsInterface)
			{
				// By passing an empty string as the name the component will still be registered, 
				// but the Sandbox will not show it in the AddComponent menu.
				componentAttribute.Name = string.Empty;
			}

			NativeInternals.Entity.RegisterComponent(entityComponentType,
													 typeInfo.guid.hipart,
													 typeInfo.guid.lopart,
													 componentAttribute.Name,
													 componentAttribute.Category,
													 componentAttribute.Description,
													 componentAttribute.Icon);

			// Register all bases, note that the base has to have been registered before the component we're registering right now!
			var baseType = entityComponentType.BaseType;
			while(baseType != typeof(object))
			{
				NativeInternals.Entity.AddComponentBase(entityComponentType, baseType);

				baseType = baseType.BaseType;
			}

			if(!entityComponentType.IsAbstract)
			{
				RegisterComponentProperties(typeInfo);
			}
		}

		private static bool ValidateGuid(string componentName, string guidString, out Guid guid)
		{
			try
			{
				guid = new Guid(guidString);
			}
			catch(FormatException)
			{
				Log.Error("Encountered a FormatException while parsing GUID with value \"{0}\" on {1}! " +
						  "Please provide GUIDs in the format: \"C47DF64B-E1F9-40D1-8063-2C533A1CE7D5\"", guidString, componentName);
				guid = new Guid();
				return false;
			}
			catch(OverflowException)
			{
				Log.Error("Encountered an OverFlowException while parsing GUID with value \"{0}\" on {1}! " +
						  "Please provide GUIDs in the format: \"C47DF64B-E1F9-40D1-8063-2C533A1CE7D5\"", guidString, componentName);
				guid = new Guid();
				return false;
			}
			catch(ArgumentNullException)
			{
				Log.Error("Encountered an ArgumentNullException while parsing GUID on {0}! " +
						  "Please provide GUIDs in the format: \"C47DF64B-E1F9-40D1-8063-2C533A1CE7D5\"", componentName);
				guid = new Guid();
				return false;
			}
			return true;
		}

		private static void RegisterComponentProperties(TypeInfo componentTypeInfo)
		{
			// Create a dummy from which we can get default values.
			var dummy = Activator.CreateInstance(componentTypeInfo.type);

			// Retrieve all properties
			var properties = componentTypeInfo.type.GetProperties(BindingFlags.Public | BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.SetProperty);
			foreach(PropertyInfo propertyInfo in properties)
			{
				// Only properties with the EntityProperty attribute are registered.
				var attribute = (EntityPropertyAttribute)propertyInfo.GetCustomAttributes(typeof(EntityPropertyAttribute), false).FirstOrDefault();
				if(attribute == null)
				{
					continue;
				}

				object defaultValue = null;
				var propertyType = propertyInfo.PropertyType;
				var defaultValueAttribute = (DefaultValueAttribute)propertyInfo.GetCustomAttributes(typeof(DefaultValueAttribute), false).FirstOrDefault();
				if(defaultValueAttribute != null)
				{
					defaultValue = defaultValueAttribute.Value;
				}
				else if(propertyType.IsValueType || propertyType.IsAssignableFrom(typeof(string)))
				{
					// It could happen that a property needs a reference to an Entity, which it doesn't have yet.
					try
					{
						defaultValue = propertyInfo.GetValue(dummy);
					}
					catch
					{
						// Value type (struct) always has an empty constructor, so it's safe to construct a default one.
						defaultValue = propertyType.IsAssignableFrom(typeof(string)) ? "" : Activator.CreateInstance(propertyType);
					}
				}

				// Register the property in native code.
				NativeInternals.Entity.RegisterComponentProperty(componentTypeInfo.type, propertyInfo, propertyInfo.Name, propertyInfo.Name, attribute.Description, attribute.Type, defaultValue);
			}

			// Register all Schematyc functions
			var methods = componentTypeInfo.type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
			foreach(MethodInfo methodInfo in methods)
			{
				var attribute = (SchematycMethodAttribute)methodInfo.GetCustomAttributes(typeof(SchematycMethodAttribute), false).FirstOrDefault();
				if(attribute == null)
					continue;

				NativeInternals.Entity.RegisterComponentFunction(componentTypeInfo.type, methodInfo);
			}

			// Register all Schematyc signals
			var nestedTypes = componentTypeInfo.type.GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic);
			foreach(Type nestedType in nestedTypes)
			{
				var attribute = (SchematycSignalAttribute)nestedType.GetCustomAttributes(typeof(SchematycSignalAttribute), false).FirstOrDefault();
				if(attribute == null)
					continue;

				int signalId = NativeInternals.Entity.RegisterComponentSignal(componentTypeInfo.type, nestedType.Name);

				MethodInfo invoke = nestedType.GetMethod("Invoke");
				ParameterInfo[] pars = invoke.GetParameters();

				List<ParameterInfo> parameters = new List<ParameterInfo>();

				foreach(ParameterInfo p in pars)
				{
					NativeInternals.Entity.AddComponentSignalParameter(componentTypeInfo.type, signalId, p.Name, p.ParameterType);

					parameters.Add(p);
				}
				if(componentTypeInfo.signals == null)
				{
					componentTypeInfo.signals = new List<TypeInfo.Signal>();
				}

				componentTypeInfo.signals.Add(new TypeInfo.Signal
				{
					id = signalId,
					delegateType = nestedType
				});
			}
		}
		#endregion
	}
}
