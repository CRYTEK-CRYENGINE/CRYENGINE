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

		public static event Action<EventArgs<string>> LevelNotFound;
		public static event Action<EventArgs<ILevelInfo>> LoadingStart;
		public static event Action<EventArgs<ILevelInfo>> LoadingLevelEntitiesStart;
		public static event Action<EventArgs<ILevelInfo, string>> LoadingError;
		public static event Action<EventArgs<ILevelInfo, int>> LoadingProgress;
		public static event Action<EventArgs<ILevelInfo>> LoadingComplete;
		public static event Action<EventArgs<ILevelInfo>> UnloadingComplete;

		public override void OnLevelNotFound(string levelName)
		{
			LevelNotFound?.Invoke(new EventArgs<string>(levelName));
		}

		public override void OnLoadingStart(ILevelInfo pLevel)
		{
			LoadingStart?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		public override void OnLoadingLevelEntitiesStart(ILevelInfo pLevel)
		{
			LoadingLevelEntitiesStart?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		public override void OnLoadingComplete(ILevelInfo pLevel)
		{
			LoadingComplete?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		public override void OnLoadingError(ILevelInfo pLevel, string error)
		{
			LoadingError?.Invoke(new EventArgs<ILevelInfo, string>(pLevel, error));
		}

		public override void OnLoadingProgress(ILevelInfo pLevel, int progressAmount)
		{
			LoadingProgress?.Invoke(new EventArgs<ILevelInfo, int>(pLevel, progressAmount));
		}

		public override void OnUnloadComplete(ILevelInfo pLevel)
		{
			UnloadingComplete?.Invoke(new EventArgs<ILevelInfo>(pLevel));
		}

		internal LevelSystem()
		{
			AddListener();

			Engine.EndReload += AddListener;
		}

		void AddListener()
		{
			Engine.GameFramework?.GetILevelSystem()?.AddListener(this);
		}

		public override void Dispose()
		{
			var levelSystem = Engine.GameFramework?.GetILevelSystem();
			if(levelSystem != null)
			{
				levelSystem.RemoveListener(this);
			}

			base.Dispose();
		}
	}
}
