// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.Serialization;
using CryEngine.Attributes;
using CryEngine.Common;

namespace CryEngine.EntitySystem
{
	public sealed class EntityClass
	{
		#region Fields
		private IEntityClassRegistry.SEntityClassDesc _description;
		private Type _protoType;
		private IEntityClass _nativeClass;
		private EntityPropertyHandler _propertyHandler;
		#endregion

		#region Properties
		public IEntityClassRegistry.SEntityClassDesc Description { get { return _description; } set{ _description = value; } }
		public Type ProtoType { get { return _protoType; } set{ _protoType = value; } }
		public IEntityClass NativeClass { get { return _nativeClass; } set{ _nativeClass = value; } }
		internal EntityPropertyHandler PropertyHandler { get { return _propertyHandler; } set { _propertyHandler = value; } }
		#endregion

		#region Constructors
		internal EntityClass(Type t)
		{
			_nativeClass = null;
			_protoType = t;
			_description = new IEntityClassRegistry.SEntityClassDesc ();
			_description.editorClassInfo = new SEditorClassInfo ();
			_description.sScriptFile = "";

			_propertyHandler = EntityPropertyHandler.CreateHandler (this);
			if (_propertyHandler != null)
				_description.pPropertyHandler = _propertyHandler;
			
			EntityClassAttribute attrib = (EntityClassAttribute)t.GetCustomAttributes (typeof(EntityClassAttribute), true).FirstOrDefault ();
			if (attrib == null) {
				_description.sName = t.Name;
				_description.editorClassInfo.sCategory = "Game";
			} else {
				_description.sName = attrib.Name;
				_description.editorClassInfo.sCategory = attrib.EditorPath;
				_description.editorClassInfo.sHelper = attrib.Helper;
				_description.editorClassInfo.sIcon = attrib.Icon;

				if (attrib.Hide)
					_description.flags |= (int)EEntityClassFlags.ECLF_INVISIBLE;
			}
		}
		#endregion

		#region Methods
		internal BaseEntity CreateInstance(IEntity pEntity)
		{
			object instance = FormatterServices.GetUninitializedObject (_protoType);
			FieldInfo fEntity = _protoType.GetField ("_entity", BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);
			if (fEntity != null)
				fEntity.SetValue (instance, pEntity);
			FieldInfo fEntityClass = _protoType.GetField ("_entityClass", BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);
			if (fEntityClass != null)
				fEntityClass.SetValue (instance, this);

			BaseEntity managedInstance = (BaseEntity)instance;
			if (_propertyHandler != null)
				_propertyHandler.SetDefaults (managedInstance);
			managedInstance.OnInitialize ();
			return managedInstance;
		}
		#endregion
	}
	
	public sealed class EntityClassRegistry
	{
		#region Fields
		private Dictionary<string, EntityClass> _registry = new Dictionary<string, EntityClass>();
		#endregion

		#region Constructors
		internal EntityClassRegistry()
		{
		}
		#endregion

		#region Methods
		public void RegisterAll()
		{
			Type tProto = typeof(BaseEntity);

			// Collect all types inheriting from BaseEntity
			foreach (Assembly asm in AppDomain.CurrentDomain.GetAssemblies()) {
				foreach (Type t in asm.GetTypes()) {
					if (t == tProto || !tProto.IsAssignableFrom (t))
						continue;

					Register (t);
				}
			}
		}

		public bool Register(Type entityType)
		{
			if (!typeof(BaseEntity).IsAssignableFrom (entityType))
				return false;

			EntityClass entry = new EntityClass (entityType);
			if (_registry.ContainsKey (entry.Description.sName)) {
				Log.Warning<EntityClassRegistry>("Managed entity class with name '{0}' already registered!", entry.Description.sName);
				return false;
			}

			IEntityClass native = Global.gEnv.pEntitySystem.GetClassRegistry ().FindClass (entry.Description.sName);
			if(native != null)
				entry.Description.flags |= (int)EEntityClassFlags.ECLF_MODIFY_EXISTING;

			entry.NativeClass = Global.gEnv.pEntitySystem.GetClassRegistry ().RegisterStdClass (entry.Description);
			_registry [entry.Description.sName] = entry;
			Log.Info<EntityClassRegistry>("Registered managed entity class: {0}", entry.ProtoType.Name);
			return true;
		}

		public bool Register<T>()
		{
			return Register (typeof(T));
		}

		public bool Disable(string className)
		{
			if (String.IsNullOrEmpty (className) || !_registry.ContainsKey (className))
				return false;

			EntityClass managedClass = _registry [className];

			managedClass.Description.flags |= (int)EEntityClassFlags.ECLF_MODIFY_EXISTING | (int)EEntityClassFlags.ECLF_INVISIBLE;
			managedClass.Description.pPropertyHandler = null;
			managedClass.NativeClass = Global.gEnv.pEntitySystem.GetClassRegistry ().RegisterStdClass (managedClass.Description);

			//TODO: Disable all entity instance handlers

			return true;
		}

		public bool Disable(Type entityType)
		{
			return Disable (FindClassName (entityType));
		}

		public bool Disable<T>()
		{
			return Disable (typeof(T));
		}

		public void DisableAll()
		{
			foreach (string className in _registry.Keys.ToList())
				Disable (className);
		}

		public bool Unregister(string className)
		{
			if (String.IsNullOrEmpty (className))
				return false;
			
			//TODO: Handle all entity instances using that entity class!!!!

			bool result = Global.gEnv.pEntitySystem.GetClassRegistry ().UnregisterEntityClass (_registry [className].NativeClass);
			Log.Info<EntityClassRegistry> ("Unregister entity class: {0} = {1}", className, result);
			_registry.Remove (className);
			return true;
		}

		public bool Unregister(Type entityType)
		{
			return Unregister (FindClassName (entityType));
		}

		public bool Unregister<T>()
		{
			return Unregister(typeof(T));
		}

		public void UnregisterAll()
		{
			foreach (string className in _registry.Keys.ToList())
				Unregister (className);
		}

		public string FindClassName(Type entityType)
		{
			foreach (string className in _registry.Keys)
				if (_registry [className].ProtoType == entityType)
					return className;

			return String.Empty;
		}

		public string FindClassName<T>()
		{
			return FindClassName (typeof(T));
		}

		public EntityClass GetEntityClass(IEntity pEntity)
		{
			if (pEntity == null || pEntity.IsGarbage())
				return null;

			IEntityClass pClass = pEntity.GetClass ();
			if (pClass == null)
				return null;

			string className = pClass.GetName ();
			if (!_registry.ContainsKey (className))
				return null;

			return _registry [className];
		}

		public EntityClass GetEntityClass(SEntitySpawnParams spawnParams)
		{
			if (spawnParams == null || spawnParams.pClass == null)
				return null;

			string className = spawnParams.pClass.GetName ();
			if (!_registry.ContainsKey (className))
				return null;

			return _registry [className];
		}

		public EntityClass GetEntityClass<T>() where T : BaseEntity
		{
			return GetEntityClass (typeof(T));
		}

		public EntityClass GetEntityClass(Type t)
		{
			foreach (EntityClass cls in _registry.Values)
				if (cls.ProtoType == t)
					return cls;

			return null;
		}
		#endregion
	}
}

