using System;
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
    /// Systems reference entity components by GUID, for example when serializing to file to detect which type a component belongs to.
    /// By default we generate a GUID automatically based on the EntityComponent implementation type, however this will result in serialization breaking if you rename it.
    /// To circumvent this, use System.Runtime.Interopservices.GuidAttribute to explicitly specify your desired GUID:
    /// 
    /// [Guid("C47DF64B-E1F9-40D1-8063-2C533A1CE7D5")]
    /// public class MyComponent : public EntityComponent {}
    /// </summary>
	public abstract class EntityComponent
	{
        internal struct GUID
        {
            public ulong lopart;
            public ulong hipart;
        }

        internal class TypeInfo
        {
            public GUID guid;
        }
        
        internal static Dictionary<Type, TypeInfo> _componentClassMap = new Dictionary<Type, TypeInfo>();

		public Entity Entity { get; private set; }

        #region Functions
        internal void SetEntity(IntPtr entityHandle, uint id)
        {
            Entity = new Entity(new IEntity(entityHandle, false), id); 
        }
        #endregion

        #region Entity Event Methods
		protected virtual void OnTransformChanged() { }

		protected virtual void OnInitialize() { }

		protected virtual void OnUpdate(float frameTime) { }

		protected virtual void OnEditorUpdate(float frameTime) { }

		protected virtual void OnEditorGameModeChange(bool enterGame) { }

		protected virtual void OnRemove() { }

		protected virtual void OnHide() { }

		protected virtual void OnUnhide() { }

		protected virtual void OnGameplayStart() { }

		protected virtual void OnCollision(CollisionEvent collisionEvent) { }

		private void OnCollisionInternal(IntPtr sourceEntityPhysics, IntPtr targetEntityPhysics)
		{
			var collisionEvent = new CollisionEvent();
            collisionEvent.Source = new PhysicsObject(new IPhysicalEntity(sourceEntityPhysics, false));
            collisionEvent.Target = new PhysicsObject(new IPhysicalEntity(targetEntityPhysics, false));
            OnCollision(collisionEvent);
        }

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
            if (!typeof(EntityComponent).IsAssignableFrom(entityComponentType) || entityComponentType.IsAbstract)
                return;

            var typeInfo = new TypeInfo();
            _componentClassMap[entityComponentType] = typeInfo;
            
            var guidAttribute = (GuidAttribute)entityComponentType.GetCustomAttributes(typeof(GuidAttribute), false).FirstOrDefault();
            if (guidAttribute != null)
            {
                var guid = new Guid(guidAttribute.Value);

                var guidArray = guid.ToByteArray();
                typeInfo.guid.hipart = BitConverter.ToUInt64(guidArray, 0);
                typeInfo.guid.lopart = BitConverter.ToUInt64(guidArray, 8);
            }
            else
            {
                // Fall back to generating GUID based on type
                var guidString = Engine.TypeToHash(entityComponentType);
                var half = (int)(guidString.Length / 2.0f);
                if (!ulong.TryParse(guidString.Substring(0, half), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out typeInfo.guid.hipart))
                {
                    Log.Error("Failed to parse {0} to UInt64", guidString.Substring(0, half));
                }
                if (!ulong.TryParse(guidString.Substring(half, guidString.Length - half), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out typeInfo.guid.lopart))
                {
                    Log.Error("Failed to parse {0} to UInt64", guidString.Substring(half, guidString.Length - half));
                }
            }

            var componentAttribute = (EntityComponentAttribute)entityComponentType.GetCustomAttributes(typeof(EntityComponentAttribute), false).FirstOrDefault();
            if (componentAttribute == null)
            {
                componentAttribute = new EntityComponentAttribute();
            }

            if(componentAttribute.Name.Length == 0)
            {
                componentAttribute.Name = entityComponentType.Name;
            }

			NativeInternals.Entity.RegisterComponent(entityComponentType, 
			                                         typeInfo.guid.hipart, 
			                                         typeInfo.guid.lopart, 
			                                         componentAttribute.Name, 
			                                         componentAttribute.Category, 
			                                         componentAttribute.Description, 
			                                         componentAttribute.Icon);

            // Register all properties
            var properties = entityComponentType.GetProperties(BindingFlags.Public | BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.SetProperty);
            foreach (PropertyInfo propertyInfo in properties)
            {
                var attribute = (EntityPropertyAttribute)propertyInfo.GetCustomAttributes(typeof(EntityPropertyAttribute), false).FirstOrDefault();
                if (attribute == null)
                    continue;

                NativeInternals.Entity.RegisterComponentProperty(entityComponentType, propertyInfo, propertyInfo.Name, propertyInfo.Name, attribute.Description, attribute.Type);
            }
        }
        #endregion
    }
}
