// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine.EntitySystem
{
	/// <summary>
	/// Base class for CESharp entities. Classes inheriting from this class will automatically be registered with CRYENGINE.
	/// </summary>
	public abstract class BaseEntity
	{
		#region Fields
		/// <summary>
		/// Reference to the native engine entity.
		/// </summary>
		protected IEntity _entity;
		/// <summary>
		/// Reference to the managed entity class.
		/// </summary>
		protected EntityClass _entityClass;
		#endregion

		#region Properties
		public uint Id { get { return _entity != null ? _entity.GetId () : 0; } }
		public IEntity NativeEntity { get { return _entity; } }
		public EntityClass EntityClass { get { return _entityClass; } }
		#endregion

		#region Methods
		/// <summary>
		/// Called when initializing (creating) the entity.
		/// </summary>
		public virtual void OnInitialize() { }
		/// <summary>
		/// Called once per frame.
		/// </summary>
		public virtual void OnUpdate(float frameTime, PauseMode pauseMode) { }
		/// <summary>
		/// Called when CRYENGINE's entity system issues an event.
		/// </summary>
		public virtual void OnEvent(SEntityEvent arg) { }
		/// <summary>
		/// Called before removing the entity from CRYENGINE's entity system.
		/// </summary>
		public virtual void OnRemove() { }
		/// <summary>
		/// Called when the entity's properties have been changed inside Sandbox.
		/// </summary>
		public virtual void OnPropertiesChanged() { }
		/// <summary>
		/// Called when the CESharp framework is shutting down (e.g. when reloading the code).
		/// </summary>
		public virtual void OnShutdown() { }
		#endregion
	}
}

