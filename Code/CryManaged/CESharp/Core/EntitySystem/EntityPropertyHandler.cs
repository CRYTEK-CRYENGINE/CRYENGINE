// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using CryEngine.Common;
using CryEngine.Attributes;

namespace CryEngine.EntitySystem
{	
	internal sealed class EntityProperty
	{
		#region Fields
		private static Dictionary<Type,IEntityPropertyConverter> s_converters = new Dictionary<Type, IEntityPropertyConverter>();
		private PropertyInfo _managedInfo;
		private IEntityPropertyHandler.SPropertyInfo _engineProperty;
		private string _default;
		#endregion

		#region Properties
		internal PropertyInfo ManagedInfo { get { return _managedInfo; } }
		internal IEntityPropertyHandler.SPropertyInfo EngineInfo { get { return _engineProperty; } }
		internal string Default { get { return _default; } }
		#endregion

		#region Constructors
		private EntityProperty(PropertyInfo managed, IEntityPropertyHandler.SPropertyInfo engine, string defaultValue)
		{
			_managedInfo = managed;
			_engineProperty = engine;
			_default = defaultValue;
		}
		#endregion

		#region Methods
		internal static EntityProperty Create(PropertyInfo managed)
		{
			EntityPropertyAttribute attrib = (EntityPropertyAttribute)managed.GetCustomAttributes (typeof(EntityPropertyAttribute), true).FirstOrDefault ();
			if (attrib == null)
				return null;
			
			if (!s_converters.ContainsKey (managed.PropertyType))
				return null;

			EEditTypes editType = attrib.EditType;
			if (editType == EEditTypes.Default)
				editType = s_converters [managed.PropertyType].DefaultEditType;
			string strEditType = StringValueAttribute.GetStringValue (editType);
			
			IEntityPropertyHandler.SPropertyInfo engine = new IEntityPropertyHandler.SPropertyInfo ();
			engine.name = managed.Name;
			engine.description = attrib.Description;
			engine.editType = strEditType;
			engine.limits = new IEntityPropertyHandler.SPropertyInfo.SLimits ();
			engine.limits.min = attrib.Min;
			engine.limits.max = attrib.Max;
			engine.type = s_converters [managed.PropertyType].EngineType;

			return new EntityProperty (managed, engine, attrib.Default);
		}

		internal string Get(BaseEntity entity)
		{
			return s_converters [_managedInfo.PropertyType].GetValue (_managedInfo.GetValue (entity, null));
		}

		internal void Set(BaseEntity entity, string value)
		{
			_managedInfo.SetValue (entity, s_converters [_managedInfo.PropertyType].SetValue (value), null);
		}

		internal static void RegisterConverters()
		{
			Type tProto = typeof(IEntityPropertyConverter);

			// Collect all types implementing IEntityPropertyConverter
			foreach (Assembly asm in AppDomain.CurrentDomain.GetAssemblies()) {
				foreach (Type t in asm.GetTypes()) {
					if (!t.IsAbstract && !t.IsInterface && t.GetInterfaces ().Contains (tProto)) {
						IEntityPropertyConverter instance = (IEntityPropertyConverter)Activator.CreateInstance (t);
						s_converters.Add (instance.PropertyType, instance);
					}
				}
			}

			Type[] keys = s_converters.Keys.ToArray ();
			StringBuilder sbTypes = new StringBuilder ();
			for (int i = 0; i < keys.Length; i++) {
				sbTypes.Append (keys [i].Name);

				if (i < keys.Length - 1)
					sbTypes.Append (", ");
			}
			Log.Info ("[EntityFramework] Available property converters: {0}", sbTypes.ToString ());
		}
		#endregion
	}

	internal sealed class EntityPropertyHandler : IMonoEntityPropertyHandler
	{
		#region Fields
		private EntityClass _parent;
		private List<EntityProperty> _properties;
		#endregion

		#region Constructors
		private EntityPropertyHandler (EntityClass parent)
		{
			_parent = parent;
		}
		#endregion

		#region Methods
		internal static EntityPropertyHandler CreateHandler(EntityClass parent)
		{
			EntityPropertyHandler instance = new EntityPropertyHandler (parent);
			if (!instance.Init ()) {
				instance.Dispose ();
				instance = null;
			}
			return instance;
		}

		private bool Init()
		{
			_properties = new List<EntityProperty> ();
			PropertyInfo[] properties = _parent.ProtoType.GetProperties (BindingFlags.Public | BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.SetProperty);
			foreach(PropertyInfo pi in properties)
			{
				EntityProperty prop = EntityProperty.Create (pi);
				if (prop == null)
					continue;

				_properties.Add (prop);
			}
			return _properties.Count > 0;
		}

		public void SetDefaults(BaseEntity entity)
		{
			foreach (EntityProperty prop in _properties)
				prop.Set (entity, prop.Default);
		}

		public override void Dispose ()
		{
			base.Dispose ();
		}

		public override string GetDefaultProperty (int index)
		{
			if (index >= _properties.Count)
				return null;

			return _properties [index].Default;
		}

		public override void GetMemoryUsage (ICrySizer pSizer)
		{
		}

		public override SPropertyInfo GetMonoPropertyInfo (int index)
		{
			if (index >= _properties.Count)
				return null;
			
			return _properties [index].EngineInfo;
		}

		public override string GetProperty (IEntity entity, int index)
		{
			if (index >= _properties.Count)
				return null;
			
			uint id = entity.GetId ();
			BaseEntity ent = EntityFramework.GetEntity (id);
			if (ent == null)
				return null;
			
			return _properties [index].Get (ent);
		}

		public override int GetPropertyCount ()
		{
			return _properties.Count;
		}

		public override uint GetScriptFlags ()
		{
			return 0;
		}

		public override void InitArchetypeEntity (IEntity entity, string archetypeName, SEntitySpawnParams spawnParams)
		{
			// TODO: Implement
		}

		public override void LoadArchetypeXMLProperties (string archetypeName, XmlNodeRef xml)
		{
			// TODO: Implement
		}

		public override void LoadEntityXMLProperties (IEntity entity, XmlNodeRef xml)
		{
			 // TODO: Implement
		}

		public override void PropertiesChanged (IEntity entity)
		{
			uint id = entity.GetId ();
			BaseEntity ent = EntityFramework.GetEntity (id);
			if (ent != null)
				ent.OnPropertiesChanged ();
		}

		public override void RefreshProperties ()
		{
			
			// TODO: Implement
		}

		public override void SetProperty (IEntity entity, int index, string value)
		{
			if (index >= _properties.Count)
				return;

			uint id = entity.GetId ();
			BaseEntity ent = EntityFramework.GetEntity (id);
			if (ent == null)
				return;
			
			_properties [index].Set (ent, value);
		}

		public void Retrieve(BaseEntity ent, Dictionary<string, string> cacheEntry)
		{
			foreach (KeyValuePair<string, string> kvp in cacheEntry) {
				foreach (EntityProperty prop in _properties) {
					if (!prop.EngineInfo.name.Equals (kvp.Key, StringComparison.InvariantCultureIgnoreCase))
						continue;

					prop.Set (ent, kvp.Value);
				}
			}
		}

		public Dictionary<string, string> Store(BaseEntity ent)
		{
			Dictionary<string, string> cacheEntry = new Dictionary<string, string> ();

			foreach (EntityProperty prop in _properties) {
				string key = prop.EngineInfo.name;
				string value = prop.Get (ent);
				cacheEntry.Add (key, value);
			}
			return cacheEntry;
		}
		#endregion
	}
}

