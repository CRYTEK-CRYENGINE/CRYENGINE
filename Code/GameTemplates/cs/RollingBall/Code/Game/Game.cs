// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.Game
{
	/// <summary>
	/// Example of a rolling ball physics game.
	/// </summary>
	public class Game : IDisposable
	{
		private static Game _instance;

		private Game()
		{
			// The server doesn't support Input so return early.
			if(Engine.IsDedicatedServer)
			{
				return;
			}

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

		public void Dispose()
		{
			if(!Engine.IsDedicatedServer)
			{
				Input.OnKey -= OnKey;
			}
		}

		private void OnKey(InputEvent e)
		{
			if(e.KeyPressed(KeyId.Escape))
			{
				Engine.Shutdown();
			}
		}
	}
}
