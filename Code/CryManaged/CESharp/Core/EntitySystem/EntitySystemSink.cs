// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine.EntitySystem
{
	internal sealed class EntitySystemSink : IEntitySystemSink
	{
		#region Events
		public event EventHandler<SEntitySpawnParams> BeforeSpawn;
		public event EventHandler<IEntity, SEntitySpawnParams> Reused;
		public event EventHandler<IEntity, SEntityEvent> Event;
		public event EventHandler<IEntity, SEntitySpawnParams> Spawn;
		public event EventHandler<IEntity> Remove;
		#endregion
		
		#region Constructors
		internal EntitySystemSink()
		{
			Global.gEnv.pEntitySystem.AddSink (this, (uint)IEntitySystem.SinkEventSubscriptions.AllSinkEvents, 0);
		}
		#endregion

		#region Methods
		public override void Dispose ()
		{
			Global.gEnv.pEntitySystem.RemoveSink (this);
			base.Dispose ();
		}

		public override bool OnBeforeSpawn (SEntitySpawnParams arg0)
		{
			if (BeforeSpawn != null)
				BeforeSpawn (arg0);
			
			return true;
		}

		public override void OnReused (IEntity pEntity, SEntitySpawnParams arg1)
		{
			if (Reused != null)
				Reused (pEntity, arg1);
		}

		public override void OnEvent (IEntity pEntity, SEntityEvent arg1)
		{
			if (Event != null)
				Event (pEntity, arg1);
		}

		public override void OnSpawn (IEntity pEntity, SEntitySpawnParams arg1)
		{
			if (Spawn != null)
				Spawn (pEntity, arg1);
		}

		public override bool OnRemove (IEntity pEntity)
		{
			if (Remove != null)
				Remove (pEntity);
			return true;
		}
		#endregion
	}
}

