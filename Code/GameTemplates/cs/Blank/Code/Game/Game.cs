// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.UI;
using CryEngine.UI.Components;

namespace CryEngine.Game
{
	/// <summary>
	/// Basic sample of running a Mono Application.
	/// </summary>
	public class Game : IGameUpdateReceiver, IDisposable
	{
		private static Game _instance;

		private DateTime _updateFPSTime = DateTime.MinValue;
		private int _frameCount = 0;
		private Text _fpsText;

		private Game()
		{
			// The server doesn't support rendering UI and receiving input, so initializing those system is not required.
			if(Engine.IsDedicatedServer)
			{
				return;
			}

			Engine.EngineUnloading += DestroyUI;
			Engine.EngineReloaded += CreateUI;
			CreateUI();

			GameFramework.RegisterForUpdate(this);

			Input.OnKey += OnKey;
		}

		public static void Initialize()
		{
			if(_instance == null)
			{
				_instance = new Game();
			}
		}

		public static void Shutdown()
		{
			_instance?.Dispose();
			_instance = null;
		}

		private void CreateUI()
		{
			// Canvas to contain the FPS label.
			var canvas = SceneObject.Instantiate<Canvas>(SceneManager.RootObject);

			// Create FPS Label.
			_fpsText = canvas.AddComponent<Text>();
			_fpsText.Alignment = Alignment.TopLeft;
			_fpsText.Height = 28;
			_fpsText.Offset = new Point(10, 10);
			_fpsText.Color = Color.Yellow.WithAlpha(0.5f);
		}

		private void DestroyUI()
		{
			_fpsText.Owner.Destroy();
			_fpsText = null;
		}

		public void Dispose()
		{
			if(Engine.IsDedicatedServer)
			{
				return;
			}

			Input.OnKey -= OnKey;
			GameFramework.UnregisterFromUpdate(this);
		}

		public virtual void OnUpdate()
		{
			// Update FPS Label.
			if(DateTime.Now > _updateFPSTime)
			{
				_fpsText.Content = _frameCount + " fps";
				_frameCount = 0;
				_updateFPSTime = DateTime.Now.AddSeconds(1);
			}
			_frameCount++;
		}

		private void OnKey(InputEvent e)
		{
			if(e.KeyPressed(KeyId.Escape))
			{
				Engine.Shutdown();
			}

			// Show/Hide FPS Label on F4.
			if(e.KeyPressed(KeyId.F4))
			{
				_fpsText.Active = !_fpsText.Active;
			}
		}
	}
}
