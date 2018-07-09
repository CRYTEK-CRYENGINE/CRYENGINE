// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Interface that can be registered in the <see cref="GameFramework"/> to receive update calls after the engine has updated.
	/// </summary>
	public interface IGameUpdateReceiver
	{
		/// <summary>
		/// Called every frame when registered in the <see cref="GameFramework"/>.
		/// </summary>
		void OnUpdate();
	}

	/// <summary>
	/// Interface that can be registered in the <see cref="GameFramework"/> to receive update calls right before the frame is rendered.
	/// </summary>
	public interface IGameRenderReceiver
	{
		/// <summary>
		/// Called right before the frame is rendered when registered in the <see cref="GameFramework"/>
		/// </summary>
		void OnRender();
	}

	/// <summary>
	/// Listens for engine sided updates and informs registered objects by calling their OnUpdate function. Objects may register here, in order to be invoked on global update calls. Controls target FPS.
	/// </summary>
	public class GameFramework : IGameFrameworkListener
	{
		private class DestroyAction
		{
			public bool UpdateDone { get; set; }
			public bool RenderDone { get; set; }
			public Action Action { get; set; }
		}

		internal static GameFramework Instance { get; set; }
		private static readonly List<DestroyAction> _destroyActions = new List<DestroyAction>();
		private static readonly List<IGameUpdateReceiver> _updateReceivers = new List<IGameUpdateReceiver>();
		private static readonly List<IGameRenderReceiver> _renderReceivers = new List<IGameRenderReceiver>();

		/// <summary>
		/// Called when the engine saves the game.
		/// </summary>
		/// <param name="pSaveGame"></param>
		public override void OnSaveGame(ISaveGame pSaveGame)
		{
			Log.Info<GameFramework>("OnSaveGame");
		}

		/// <summary>
		/// Called by the engine when a game is loaded.
		/// </summary>
		/// <param name="pLoadGame"></param>
		public override void OnLoadGame(ILoadGame pLoadGame)
		{
			// nada
		}

		/// <summary>
		/// Called by the engine when loading is forced with Flash.
		/// </summary>
		public override void OnForceLoadingWithFlash()
		{
			// nada
		}

		/// <summary>
		/// Called by the engine when the level has ended.
		/// </summary>
		/// <param name="nextLevel"></param>
		public override void OnLevelEnd(string nextLevel)
		{
			// nada
		}

		/// <summary>
		/// Called by the engine when the savegame file has loaded.
		/// </summary>
		/// <param name="pLevelName"></param>
		public override void OnSavegameFileLoadedInMemory(string pLevelName)
		{
			// nada
		}

		/// <summary>
		/// Registered object will be invoked on function OnUpdate if CryEngine's GameFramework raises OnPostUpdate.
		/// </summary>
		public static void RegisterForUpdate(IGameUpdateReceiver obj)
		{
			_updateReceivers.Add(obj);
		}

		/// <summary>
		/// Registered object will be invoked on function OnRender if CryEngine's GameFramework raises OnPreRender.
		/// </summary>
		public static void RegisterForRender(IGameRenderReceiver obj)
		{
			_renderReceivers.Add(obj);
		}

		/// <summary>
		/// Unregisters object from OnPostUpdate.
		/// </summary>
		public static void UnregisterFromUpdate(IGameUpdateReceiver obj)
		{
			_updateReceivers.Remove(obj);
		}

		/// <summary>
		/// Unregisters object from OnPreRender.
		/// </summary>
		public static void UnregisterFromRender(IGameRenderReceiver obj)
		{
			_renderReceivers.Remove(obj);
		}

		internal static void AddDestroyAction(Action destroyAction)
		{
			var action = new DestroyAction
			{
				Action = destroyAction
			};
			_destroyActions.Add(action);
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override void OnActionEvent(SActionEvent arg0)
		{
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override void OnPostUpdate(float fDeltaTime)
		{
			// Calculate time used to render last frame.
			FrameTime.Delta = fDeltaTime;
			
			if(Global.gEnv.pGameFramework.IsInLevelLoad())
			{
				return;
			}

			if(_destroyActions.Count > 0)
			{
				var count = _destroyActions.Count;
				for(int i = count - 1; i >= 0; i--)
				{
					var action = _destroyActions[i];
					if(action.RenderDone && action.UpdateDone)
					{
						action.Action?.Invoke();
						_destroyActions.RemoveAt(i);
					}

					action.UpdateDone = true;
				}
			}

			var updateReceivers = new List<IGameUpdateReceiver>(_updateReceivers);
			foreach(IGameUpdateReceiver obj in updateReceivers)
			{
				obj.OnUpdate();
			}
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override void OnPreRender()
		{
			if(Global.gEnv.pGameFramework.IsInLevelLoad())
			{
				return;
			}

			if(_destroyActions.Count > 0)
			{
				var count = _destroyActions.Count;
				for(int i = count - 1; i >= 0; i--)
				{
					var action = _destroyActions[i];
					action.RenderDone = true;
				}
			}

			var renderReceivers = new List<IGameRenderReceiver>(_renderReceivers);
			foreach(IGameRenderReceiver obj in renderReceivers)
			{
				obj.OnRender();
			}
		}

		internal GameFramework()
		{
			AddListener();

			Engine.EndReload += AddListener;
			Engine.StartReload += RemoveListener;
		}

		private void AddListener()
		{
			Engine.GameFramework.RegisterListener(this, "MonoGameFramework", EFRAMEWORKLISTENERPRIORITY.FRAMEWORKLISTENERPRIORITY_DEFAULT);
		}

		private void RemoveListener()
		{
			Engine.GameFramework?.UnregisterListener(this);
		}

		/// <summary>
		/// Disposes of this instance.
		/// </summary>
		public override void Dispose()
		{
			RemoveListener();
			_updateReceivers.Clear();
			_renderReceivers.Clear();

			base.Dispose();
		}
	}
}
