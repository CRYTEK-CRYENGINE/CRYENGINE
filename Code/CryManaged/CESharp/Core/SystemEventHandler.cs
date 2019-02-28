// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{	
	/// <summary>
	/// Processes CryEngine's system callback and generates events from it.
	/// </summary>
	public class SystemEventHandler : ISystemEventListener
	{
		public static Action FocusChanged;
		public static Action PrecacheEnded;

		/// <summary>
		/// Raised if Game-Mode was initiated in Editor 
		/// </summary>
		public static Action EditorGameStart;

		/// <summary>
		/// Raised if Game-Mode was exited in Editor 
		/// </summary>
		public static Action EditorGameEnded;

		internal static SystemEventHandler Instance { get; set; }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		internal SystemEventHandler()
		{
			AddListener();

			Engine.EndReload += AddListener;
			Engine.StartReload += RemoveListener;
		}

		private void AddListener()
		{
			Engine.System.GetISystemEventDispatcher().RegisterListener(this, "SystemEventHandler.cs");
		}

		private void RemoveListener()
		{
			Engine.System.GetISystemEventDispatcher()?.RemoveListener(this);
		}

		/// <summary>
		/// Disposes the SystemEventHandler.
		/// </summary>
		public override void Dispose()
		{
			RemoveListener();

			base.Dispose();
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
#if WIN64 || WIN86
		public override void OnSystemEvent(ESystemEvent arg0, ulong wparam, ulong lparam)
#elif WIN32
		public override void OnSystemEvent (ESystemEvent arg0, uint wparam, uint lparam)
#endif
		{
			switch (arg0)
			{
				case ESystemEvent.ESYSTEM_EVENT_CHANGE_FOCUS:
					if (FocusChanged != null)
						FocusChanged();
					break;
				case ESystemEvent.ESYSTEM_EVENT_LEVEL_PRECACHE_END:
					if (PrecacheEnded != null)
						PrecacheEnded();
					break;

				case ESystemEvent.ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
					if (wparam == 1 && EditorGameStart != null)
						EditorGameStart();
					if (wparam == 0 && EditorGameEnded != null)
						EditorGameEnded();
					break;
			}
		}
	}
}
