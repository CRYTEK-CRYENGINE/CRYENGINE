// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Reflection;
using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	public partial class Entity
	{
		#region Fields
#pragma warning disable 414 // Assigned value is never used
		private static FieldInfo _ientitySwigPtrField;
#pragma warning restore 414 // Assigned value is never used
		#endregion

		#region Methods
		static Entity()
		{
			//TODO Is the _ientitySwigPtrField still needed? It is never used.
			// Special case, always get native pointers for SWIG director types
			_ientitySwigPtrField = typeof(IEntity).GetField("swigCPtr", BindingFlags.NonPublic | BindingFlags.DeclaredOnly | BindingFlags.Instance);
		}

		/// <summary>
		/// Finds an entity by its entity id
		/// First attempts to find the entity in the managed cache, otherwise queries native code
		/// </summary>
		/// <param name="id">EntityId</param>
		public static Entity Get(EntityId id)
		{
			var nativeEntity = Global.gEnv.pEntitySystem.GetEntity(id);
			if(nativeEntity != null && !nativeEntity.IsGarbage())
			{
				return new Entity(nativeEntity, id);
			}

			return null;
		}

		/// <summary>
		/// Queries an entity by name
		/// First attempts to find the entity in the managed cache, otherwise queries native code
		/// </summary>
		/// <param name="name"></param>
		/// <returns></returns>
		public static Entity Find(string name)
		{
			var nativeEntity = Global.gEnv.pEntitySystem.FindEntityByName(name);
			if(nativeEntity != null && !nativeEntity.IsGarbage())
			{
				return new Entity(nativeEntity, nativeEntity.GetId());
			}

			return null;
		}
        
        /// <summary>
        /// Spawns an empty without any components
        /// </summary>
        /// <param name="name">Name of the entity</param>
        /// <param name="position">Initial position of the entity</param>
        /// <param name="rotation">Initial rotation of the entity</param>
        /// <param name="scale">Initial scale of the entity</param>
        /// <returns></returns>
        public static Entity Spawn(string name, Vector3 position, Quaternion rotation, Vector3 scale)
        {
            var spawnParams = new SEntitySpawnParams();

            spawnParams.sName = name;
            spawnParams.pClass = Global.gEnv.pEntitySystem.GetClassRegistry().GetDefaultClass();

            spawnParams.vPosition = position;
            spawnParams.qRotation = rotation;
            spawnParams.vScale = scale;

            var nativeEntity = Global.gEnv.pEntitySystem.SpawnEntity(spawnParams);
            if (nativeEntity != null)
            {
                return new Entity(nativeEntity, nativeEntity.GetId());
            }

            return null;
        }

        /// <summary>
        /// Spawn a new Entity with the specified EntityComponent.
        /// </summary>
        /// <returns>The component that's on the new Entity.</returns>
        /// <param name="name">The name of the Entity in the level hierarchy.</param>
        /// <param name="position">Position of the Entity.</param>
        /// <param name="rotation">Rotation of the Entity.</param>
        /// <param name="scale">Scale of the Entity.</param>
        /// <typeparam name="T">The type of the EntityComponent.</typeparam>
        public static T SpawnWithComponent<T>(string name, Vector3 position, Quaternion rotation, Vector3 scale) where T : EntityComponent, new()
		{
            var entity = Spawn(name, position, rotation, scale);

            if (entity != null)
            {
                return entity.GetOrCreateComponent<T>();
            }

            return null;
        }

        /// <summary>
        /// Spawn a new Entity with the specified EntityComponent.
        /// </summary>
        /// <returns>The component that's on the new Entity.</returns>
        /// <param name="position">Position of the Entity.</param>
        /// <param name="rotation">Rotation of the Entity.</param>
        /// <param name="scale">Scale of the Entity.</param>
        /// <typeparam name="T">The type of the EntityComponent.</typeparam>
        public static T SpawnWithComponent<T>(Vector3 position, Quaternion rotation, float scale = 1f) where T : EntityComponent, new()
        {
            return SpawnWithComponent<T>(typeof(T).Name, position, rotation, Vector3.One * scale);
        }
        
        /// <summary>
        /// Spawn a new Entity with the specified EntityComponent.
        /// </summary>
        /// <returns>The component that's on the new Entity.</returns>
        /// <param name="name">The name of the Entity in the level hierarchy.</param>
        /// <param name="position">Position of the Entity.</param>
        /// <param name="rotation">Rotation of the Entity.</param>
        /// <param name="scale">Scale of the Entity.</param>
        /// <typeparam name="T">The type of the EntityComponent.</typeparam>
        public static T SpawnWithComponent<T>(string name, Vector3 position, Quaternion rotation, float scale = 1.0f) where T : EntityComponent, new()
        {
            return SpawnWithComponent<T>(name, position, rotation, Vector3.One * scale);
        }

        /// <summary>
        /// Remove the entity with the specified id.
        /// </summary>
        /// <param name="id"></param>
        public static void Remove(EntityId id)
		{
			Global.gEnv.pEntitySystem.RemoveEntity(id);
		}
		#endregion
	}
}

