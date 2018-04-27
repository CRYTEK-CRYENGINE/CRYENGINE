using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Listens to CryEngine's LevelSystem and forwards events from it. Classes may use events from this class to be informed about Level System state changes. 
	/// </summary>
	public class LevelSystem : ILevelSystemListener
	{
		internal static LevelSystem Instance { get; set; }

		public static event EventHandler<EventArgs<string>> LevelNotFound;
		public static event EventHandler<EventArgs<ILevelInfo>> LoadingStart;
		public static event EventHandler<EventArgs<ILevelInfo>> LoadingLevelEntitiesStart;
		public static event EventHandler<EventArgs<ILevelInfo, string>> LoadingError;
		public static event EventHandler<EventArgs<ILevelInfo, int>> LoadingProgress;
		public static event EventHandler<EventArgs<ILevelInfo>> LoadingComplete;
		public static event EventHandler<EventArgs<ILevelInfo>> UnloadingComplete;

		public override void OnLevelNotFound(string levelName)
		{
			if (LevelNotFound != null)
				LevelNotFound(new EventArgs<string>(levelName));
		}

		public override void OnLoadingStart(ILevelInfo pLevel)
		{
			if (LoadingStart != null)
				LoadingStart(new EventArgs<ILevelInfo>(pLevel));
		}

		public override void OnLoadingLevelEntitiesStart(ILevelInfo pLevel)
		{
			if (LoadingLevelEntitiesStart != null)
				LoadingLevelEntitiesStart(new EventArgs<ILevelInfo>(pLevel));
		}

		public override void OnLoadingComplete(ILevelInfo pLevel)
		{
			if (LoadingComplete != null)
				LoadingComplete(new EventArgs<ILevelInfo>(pLevel));
		}

		public override void OnLoadingError(ILevelInfo pLevel, string error)
		{
			if (LoadingError != null)
				LoadingError(new EventArgs<ILevelInfo, string>(pLevel, error));
		}

		public override void OnLoadingProgress(ILevelInfo pLevel, int progressAmount)
		{
			if (LoadingProgress != null)
				LoadingProgress(new EventArgs<ILevelInfo, int>(pLevel, progressAmount));
		}

		public override void OnUnloadComplete(ILevelInfo pLevel)
		{
			if (UnloadingComplete != null)
				UnloadingComplete(new EventArgs<ILevelInfo>(pLevel));
		}

		internal LevelSystem()
		{
			AddListener();

			Engine.EndReload += AddListener;
		}

		void AddListener()
		{
			Engine.GameFramework.GetILevelSystem().AddListener(this);
		}

		public override void Dispose()
		{
			if(Engine.GameFramework.GetILevelSystem() != null)
			{
				Engine.GameFramework.GetILevelSystem().RemoveListener(this);
			}

			base.Dispose();
		}
	}
}
