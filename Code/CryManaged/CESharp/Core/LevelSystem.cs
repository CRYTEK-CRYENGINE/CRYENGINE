using System;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Listens to CryEngine's LevelSystem and forwards events from it. Classes may use events from this class to be informed about Level System state changes. 
	/// </summary>
	public class LevelSystem : ILevelSystemListener
	{
		internal static LevelSystem Instance { get; set; }

		/// <summary>
		/// Invked if a level is not found.
		/// </summary>
		public static event Action<EventArgs<string>> LevelNotFound;
		/// <summary>
		/// Invoked when loading a level has started.
		/// </summary>
		public static event Action<EventArgs<ILevelInfo>> LoadingStart;
		/// <summary>
		/// Invoked when loading entities in a level has started.
		/// </summary>
		public static event Action<EventArgs<ILevelInfo>> LoadingLevelEntitiesStart;
		/// <summary>
		/// Invoked when there is an error during loading.
		/// </summary>
		public static event Action<EventArgs<ILevelInfo, string>> LoadingError;
		/// <summary>
		/// Invoked when there is loading progress.
		/// </summary>
		public static event Action<EventArgs<ILevelInfo, int>> LoadingProgress;
		/// <summary>
		/// Invoked when loading has completed.
		/// </summary>
		public static event Action<EventArgs<ILevelInfo>> LoadingComplete;
		/// <summary>
		/// Invoked when unloading has completed.
		/// </summary>
		public static event Action<EventArgs<ILevelInfo>> UnloadingComplete;

		/// <summary>
		/// Called by the engine when a level was not found.
		/// </summary>
		/// <param name="levelName"></param>
		public override void OnLevelNotFound(string levelName)
		{
			LevelNotFound?.Invoke(new EventArgs<string>(levelName));
		}

		/// <summary>
		/// Called by the engine when loading a level has started.
		/// </summary>
		/// <param name="pLevel"></param>
		public override void OnLoadingStart(ILevelInfo pLevel)
		{
			LoadingStart?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		/// <summary>
		/// Called by the engine when loading the entities in a level has started.
		/// </summary>
		/// <param name="pLevel"></param>
		public override void OnLoadingLevelEntitiesStart(ILevelInfo pLevel)
		{
			LoadingLevelEntitiesStart?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		/// <summary>
		/// Called by the engine when loading a level has completed.
		/// </summary>
		/// <param name="pLevel"></param>
		public override void OnLoadingComplete(ILevelInfo pLevel)
		{
			LoadingComplete?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		/// <summary>
		/// Called by the engine when there is an error during loading.
		/// </summary>
		/// <param name="pLevel"></param>
		/// <param name="error"></param>
		public override void OnLoadingError(ILevelInfo pLevel, string error)
		{
			LoadingError?.Invoke(new EventArgs<ILevelInfo, string>(pLevel, error));
		}

		/// <summary>
		/// Called by the engine when there is loading progress.
		/// </summary>
		/// <param name="pLevel"></param>
		/// <param name="progressAmount"></param>
		public override void OnLoadingProgress(ILevelInfo pLevel, int progressAmount)
		{
			LoadingProgress?.Invoke(new EventArgs<ILevelInfo, int>(pLevel, progressAmount));
		}

		/// <summary>
		/// Called by the engine when unloading has completed.
		/// </summary>
		/// <param name="pLevel"></param>
		public override void OnUnloadComplete(ILevelInfo pLevel)
		{
			UnloadingComplete?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		internal LevelSystem()
		{
			AddListener();

			Engine.EndReload += AddListener;
			Engine.StartReload += RemoveListener;
		}

		private void AddListener()
		{
			Engine.GameFramework?.GetILevelSystem()?.AddListener(this);
		}

		private void RemoveListener()
		{
			var levelSystem = Engine.GameFramework?.GetILevelSystem();
			if(levelSystem != null)
			{
				levelSystem.RemoveListener(this);
			}
		}

		/// <summary>
		/// Disposes of this instance.
		/// </summary>
		public override void Dispose()
		{
			RemoveListener();

			base.Dispose();
		}
	}
}
