// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine.EntitySystem
{	
	public class EntityUpdateListener : IMonoListener
	{
		#region Events
		public event EventHandler<int, int> Update;
		#endregion

		#region Constructors
		public EntityUpdateListener ()
		{
			Global.gEnv.pMonoRuntime.RegisterListener (this);
		}
		#endregion

		#region Methods
	 	public override void Dispose ()
		{
			Global.gEnv.pMonoRuntime.UnregisterListener (this);
			base.Dispose ();
		}

		public override void OnUpdate (int updateFlags, int nPauseMode)
		{
			if (Update != null)
				Update (updateFlags, nPauseMode);
		}
		#endregion
	}
}
