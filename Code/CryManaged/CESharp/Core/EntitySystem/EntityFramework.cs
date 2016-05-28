// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Reflection;
using CryEngine.Common;
using CryEngine.Resources;

namespace CryEngine.EntitySystem
{
	public enum PauseMode : int
	{
		Normal = 0,
		Menu = 1,
		Cutscene = 2
	}

	/// <summary>
	/// Manages CESharp entities.
	/// </summary>
	public static class EntityFramework
	{
		#region Fields
		private static EntitySystemSink s_entitySystemSink;
		private static EntityClassRegistry s_entityClassRegistry;
		private static EntityUpdateListener s_entityUpdateListener;
		private static Dictionary<uint, BaseEntity> s_managedEntities;
		#endregion

		#region Properties
		/// <summary>
		/// References to the active manager for CESharp entity classes.
		/// </summary>
		public static EntityClassRegistry ClassRegistry { get { return s_entityClassRegistry; } }
		#endregion

		#region Methods
		/// <summary>
		/// Setup the CESharp EntityFramework, including registering of discovered CESharp entity classes and assumption of already spawned CESharp entites.
		/// </summary>
		public static void Instantiate(InterDomainHandler handler)
		{
			EntityProperty.RegisterConverters ();
			s_managedEntities = new Dictionary<uint, BaseEntity> ();
			s_entityClassRegistry = new EntityClassRegistry ();
			s_entityClassRegistry.RegisterAll ();
			s_entitySystemSink = new EntitySystemSink ();
			s_entitySystemSink.Spawn += OnSpawn;
			s_entitySystemSink.Event += OnEvent;
			s_entitySystemSink.Remove += OnRemove;
			s_entityUpdateListener = new EntityUpdateListener ();
			s_entityUpdateListener.Update += OnUpdate;

			// Assume already spawned entities
			IEntityIt it = Global.gEnv.pEntitySystem.GetEntityIterator();
			if (it != null) {
				it.MoveFirst ();

				IEntity pEntity = null;
				while (!it.IsEnd ()) {
					pEntity = it.Next ();

					EntityClass managedClass = s_entityClassRegistry.GetEntityClass (pEntity);
					if (managedClass == null)
						continue;
	
					Log.Info ("[EntityFramework] Assume entity: {0} ({1})", pEntity.GetName(), managedClass.ProtoType.Name);
					BaseEntity instance = managedClass.CreateInstance (pEntity);
					handler.RetrieveEntity (instance);
					s_managedEntities.Add (pEntity.GetId(), instance);
				}
			}
		}

		/// <summary>
		/// Clean up the Entity Framework, including shutting down of all handled CESharp entities and disabling of CESharp entity classes.
		/// </summary>
		public static void Destroy(InterDomainHandler handler)
		{
			foreach (BaseEntity ent in s_managedEntities.Values) {
				handler.StoreEntity (ent);
				ent.OnShutdown ();
			}
			
			s_managedEntities.Clear ();
			//s_entityClassRegistry.UnregisterAll ();
			s_entityClassRegistry.DisableAll ();
			s_entityClassRegistry = null;
			s_entityUpdateListener.Update -= OnUpdate;
			s_entityUpdateListener.Dispose ();
			s_entityUpdateListener = null;
			s_entitySystemSink.Spawn -= OnSpawn;
			s_entitySystemSink.Event -= OnEvent;
			s_entitySystemSink.Remove -= OnRemove;
			s_entitySystemSink.Dispose ();
			s_entitySystemSink = null;
		}

		/// <summary>
		/// Read entity properties from the InterDomainHandler (usually after reloading the assemblies).
		/// </summary>
		private static void RetrieveEntity(this InterDomainHandler handler, BaseEntity entity)
		{
			if (!handler.EntityCache.ContainsKey (entity.Id))
				return;

			if (!String.Equals (handler.EntityCache [entity.Id].Item1, entity.EntityClass.Description.sName, StringComparison.InvariantCultureIgnoreCase))
				return;

			entity.EntityClass.PropertyHandler.Retrieve (entity, handler.EntityCache [entity.Id].Item2);
		}

		/// <summary>
		/// Write entity properties to the InterDomainHandler (to re-use the property values after reloading assemblies).
		/// </summary>
		private static void StoreEntity(this InterDomainHandler handler, BaseEntity entity)
		{
			handler.EntityCache.Add (entity.Id, 
				new Tuple<string, Dictionary<string, string>> (
					entity.EntityClass.Description.sName, 
					entity.EntityClass.PropertyHandler.Store (entity)
				)
			);
		}

		/// <summary>
		/// Get managed CESharp entity instance for a given EntityId. Returns null, if no managed handler exists.
		/// </summary>
		/// <param name="id">EntityId</param>
		public static BaseEntity GetEntity(uint id)
		{
			if(!s_managedEntities.ContainsKey(id))
				return null;

			return s_managedEntities [id];
		}

		/// <summary>
		/// Spawn a new instance of the given CESharp entity.
		/// </summary>
		/// <param name="name">Name.</param>
		public static T Spawn<T> (string name = null) where T : BaseEntity
		{
			EntityClass cls = s_entityClassRegistry.GetEntityClass<T> ();
			if (cls == null) {
				Log.Warning ("[EntityFramework] Could not find managed CESharp entity class for {0}", typeof(T).Name);
				return null;
			}
			
			int i = 1;
			while (String.IsNullOrEmpty (name)) {
				string test = cls.Description.sName + i.ToString ();
				if (Global.gEnv.pEntitySystem.FindEntityByName (test) == null)
					name = test;
			}

			SEntitySpawnParams sParams = new SEntitySpawnParams ();
			sParams.sName = name;
			sParams.pClass = cls.NativeClass;
			IEntity pEnt = Global.gEnv.pEntitySystem.SpawnEntity (sParams);

			Log.Info ("[EntityFramework] Spawn entity: {0} ({1})", name, cls.ProtoType.Name);
			T instance = (T)cls.CreateInstance (pEnt);
			cls.NativeClass.LoadScript (true);
			s_managedEntities.Add (pEnt.GetId(), instance);
			return instance;
		}

		/// <summary>
		/// Called when CRYENGINE's entity system is spawning an entity. Will check if the entity class is registered with the managed entity class registry and if yes, it will create a managed CESharp entity instance.
		/// </summary>
		private static void OnSpawn (IEntity pEntity, SEntitySpawnParams arg1)
		{
			if (s_managedEntities.ContainsKey (pEntity.GetId ()))
				return;

			EntityClass managedClass = s_entityClassRegistry.GetEntityClass (arg1);
			if (managedClass == null)
				return;

			Log.Info ("[EntityFramework] Spawn entity: {0} ({1})", arg1.sName, managedClass.ProtoType.Name);
			s_managedEntities.Add (pEntity.GetId(), managedClass.CreateInstance(pEntity));
		}

		/// <summary>
		/// Called when CRYENGINE's entity system issues an entity event. If a managed CESharp entity instance exists the event will be forwarded to it.
		/// </summary>
		private static void OnEvent (IEntity pEntity, SEntityEvent arg1)
		{
			uint id = pEntity.GetId ();
			if (!s_managedEntities.ContainsKey (id))
				return;

			s_managedEntities [id].OnEvent (arg1);
		}

		/// <summary>
		/// Called when CRYENGINE's entity system is removing an entity. Will remove the managed CESharp entity instance if it exists.
		/// </summary>
		private static void OnRemove (IEntity pEntity)
		{
			uint id = pEntity.GetId ();
			if (!s_managedEntities.ContainsKey (id))
				return;

			Log.Info ("[EntityFramework] Remove entity: {0} ({1})", pEntity.GetName (), pEntity.GetId ());
			s_managedEntities [id].OnRemove ();
			s_managedEntities.Remove (id);
		}

		/// <summary>
		/// Called once per frame. Will forward the update to all managed CESharp entity instances - unless the user is currently 'editing'.
		/// </summary>
		private static void OnUpdate(int updateFlags, int pauseMode)
		{
			if (Global.gEnv.IsEditor () && !Global.gEnv.IsEditorGameMode ())
				return;
			
			float frameTime = Global.gEnv.pTimer.GetFrameTime ();
			PauseMode ePauseMode = (PauseMode)pauseMode;

			foreach (BaseEntity entity in s_managedEntities.Values)
				entity.OnUpdate (frameTime, ePauseMode);
		}
		#endregion
	}
}

