using System;

using CryEngine.Common;

namespace CryEngine.Rendering
{
	/// <summary>
	/// Provides CryEngine's output resolution and sends event if resolution changed.
	/// </summary>
	public class Renderer : IGameUpdateReceiver
	{
		/// <summary>
		/// Event fired if resolution changed
		/// </summary>
		public static event Action<int, int> ResolutionChanged;

		/// <summary>
		/// Render Output Width
		/// </summary>
		/// <value>The width of the screen.</value>
		public static int ScreenWidth { get { return Global.gEnv.pRenderer.GetWidth(); } }

		/// <summary>
		/// Render Output Height
		/// </summary>
		/// <value>The height of the screen.</value>
		public static int ScreenHeight { get { return Global.gEnv.pRenderer.GetHeight(); } }

		internal static Renderer Instance { get; set; }

		int _lastWidth;
		int _lastHeight;

		internal Renderer()
		{
			_lastWidth = ScreenWidth;
			_lastHeight = ScreenHeight;
			GameFramework.RegisterForUpdate(this);
		}

		~Renderer()
		{
			GameFramework.UnregisterFromUpdate(this);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public void OnUpdate()
		{
			if(_lastWidth != ScreenWidth || _lastHeight != ScreenHeight)
			{
				_lastWidth = ScreenWidth;
				_lastHeight = ScreenHeight;
				if(ResolutionChanged != null)
					ResolutionChanged(_lastWidth, _lastHeight);
			}
		}
	}
}
