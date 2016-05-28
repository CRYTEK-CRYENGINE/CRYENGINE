// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using CryEngine.Common;
using CryEngine.UI;
using CryEngine.FlowSystem;
using CryEngine.Components;
using CryEngine.Resources;
using CryEngine.EntitySystem;
using CryEngine;

namespace CryEngine
{
	public delegate void EventHandler();
	public delegate void EventHandler<T>(T arg);
	public delegate void EventHandler<T1,T2>(T1 arg1, T2 arg2);

	/// <summary>
	/// Helper class for Singleton implementation.
	/// </summary>
	public class Singleton<T> where T : class
	{
		static T _instance = null;

		/// <summary>
		/// Called to instantiate the Singleton class or to get the current instance.
		/// </summary>
		public static T Instance { get { return _instance == null ? _instance = (T)Activator.CreateInstance(typeof(T)) : _instance; } }

		/// <summary>
		/// Called to destroy the current single instance.
		/// </summary>
		public static void Destroy() { _instance = null; }
	}

	/// <summary>
	/// Provides CryEngine's output resolution and sends event if resolution changed.
	/// </summary>
	public class Screen : IGameUpdateReceiver
	{
		public static event EventHandler<int,int> ResolutionChanged; ///< Event fired if resolution changed

		public static int Width { get { return Env.Renderer.GetWidth(); } } ///< Render Output Width
		public static int Height { get { return Env.Renderer.GetHeight(); } } ///< Render Output Height

		static Screen _instance = null;

		int _lastWidth;
		int _lastHeight;

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Instanciate()
		{
			_instance = new Screen();
			_instance._lastWidth = Width;
			_instance._lastHeight = Height;
			GameFramework.RegisterForUpdate(_instance);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public void OnUpdate ()
		{
			if (_lastWidth != Width || _lastHeight != Height) 
			{
				_lastWidth = Width;
				_lastHeight = Height;
				if (ResolutionChanged != null)
					ResolutionChanged (_lastWidth, _lastHeight);
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Destroy()
		{
			if (_instance != null)
			{
				GameFramework.UnregisterFromUpdate(_instance);
				_instance = null;
			}
		}
	}

	/// <summary>
	/// Pre-Processes CryEngine's Input callback and generates Input events. Classes may be registered here, in order to receive global Input events.
	/// </summary>
	public class Input : IInputEventListener
	{
		public static event EventHandler<SInputEvent> OnKey; ///< Fired when a key-state was modified

		private static Input _instance = null;
		private static bool _lShiftDown = false;
		private static bool _rShiftDown = false;
		private static Dictionary<string, Vec2> _axisByName = new Dictionary<string, Vec2>();

		public static bool ShiftDown { get { return _lShiftDown || _rShiftDown; } } ///< Set if Shift key is held Down

		public static Vec2 GetAxis(string name)
		{
			Vec2 axis;
			_axisByName.TryGetValue (name, out axis);
			return axis;
		}

		private Dictionary<string, string> _charByDescription = new Dictionary<string, string>() {{"comma", ","}, {"period", "."}, {"minus","-"}, {"plus","+"}};

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override bool OnInputEvent(SInputEvent e)
		{
			if (e.deviceType == EInputDeviceType.eIDT_Keyboard) 
			{
				if (e.state == EInputState.eIS_Down)
					SInputEventExtentions.KeyDownLog [e.keyId] = true;
				if (e.state == EInputState.eIS_Released)
					SInputEventExtentions.KeyDownLog [e.keyId] = false;

				if (e.KeyDown (EKeyId.eKI_LShift))
					_lShiftDown = true;
				if (e.KeyUp (EKeyId.eKI_LShift))
					_lShiftDown = false;
				if (e.KeyDown (EKeyId.eKI_RShift))
					_rShiftDown = true;
				if (e.KeyUp (EKeyId.eKI_RShift))
					_rShiftDown = false;

				// Preprocess keyName to contain valid Chars
				if (e.keyId == EKeyId.eKI_Space)
					e.keyName.key = " ";
				if (e.keyName.key.Length == 1) 
				{
					if (ShiftDown)
						e.keyName.key = e.keyName.key.ToUpper ();
					if (ShiftDown && Char.IsDigit (e.keyName.key[0]))
						e.keyName.key = "=!\"§$%&/()".Substring (e.keyName.key[0] - '0', 1);
				} 
				else
				{
					string res;
					e.keyName.key = _charByDescription.TryGetValue (e.keyName.key, out res) ? res : string.Empty;
				} 
				
				if (OnKey != null)
					OnKey(e);
			}
			else if (e.deviceType == EInputDeviceType.eIDT_Gamepad) 
			{
				// Set keyName.key = string.Empty to avoid showing up gamepad
				// presses controller keys in text input forms
				e.keyName.key = string.Empty;

				if (e.state == EInputState.eIS_Down)
					SInputEventExtentions.KeyDownLog [e.keyId] = true;
				if (e.state == EInputState.eIS_Released)
					SInputEventExtentions.KeyDownLog [e.keyId] = false;
				if (e.state == EInputState.eIS_Changed)
					SInputEventExtentions.KeyInputValueLog [e.keyId] = e.value;

				if(OnKey != null)
					OnKey (e);
			}
			else if (e.deviceType == EInputDeviceType.eIDT_EyeTracker)
			{
				if (e.keyId == EKeyId.eKI_EyeTracker_X) 
				{
					var axis = GetAxis ("EyeTracker");
					_axisByName ["EyeTracker"] = new Vec2 (e.value, axis != null ? axis.y : 0);
				}
				if (e.keyId == EKeyId.eKI_EyeTracker_Y) 
				{
					var axis = GetAxis ("EyeTracker");
					_axisByName ["EyeTracker"] = new Vec2 (axis != null ? axis.x : 0, e.value);
				}
			}		
			return false;
		}

		public static bool KeyDown(EKeyId key)
		{
			bool isDown;
			SInputEventExtentions.KeyDownLog.TryGetValue (key, out isDown);
			return isDown;
		}

		public static float GetInputValue(EKeyId key)
		{
			float changedValue;
			SInputEventExtentions.KeyInputValueLog.TryGetValue (key, out changedValue);
			return changedValue;
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override bool OnInputEventUI(SUnicodeEvent e)
		{
			return false;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Instanciate()
		{
			_instance = new Input();
			Env.Input.AddEventListener(_instance);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Destroy()
		{
			if (_instance != null)
			{
				Env.Input.RemoveEventListener(_instance);
				_instance.Dispose();
				_instance = null;
			}
		}
	}

	/// <summary>
	/// Listens to CryEngine's LevelSystem and forwards events from it. Classes may use events from this class to be informed about Level System state changes. 
	/// </summary>
	public class LevelSystem : ILevelSystemListener
	{
		private static LevelSystem _instance;

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
				LevelNotFound (new EventArgs<string> (levelName));
		}

		public override void OnLoadingStart(ILevelInfo pLevel)
		{
			if (LoadingStart != null)
				LoadingStart (new EventArgs<ILevelInfo> (pLevel));
		}

		public override void OnLoadingLevelEntitiesStart(ILevelInfo pLevel)
		{
			if (LoadingLevelEntitiesStart != null)
				LoadingLevelEntitiesStart (new EventArgs<ILevelInfo> (pLevel));
		}

		public override void OnLoadingComplete (ILevelInfo pLevel)
		{
			if (LoadingComplete != null)
				LoadingComplete (new EventArgs<ILevelInfo> (pLevel));
		}

		public override void OnLoadingError(ILevelInfo pLevel, string error)
		{
			if (LoadingError != null)
				LoadingError (new EventArgs<ILevelInfo, string> (pLevel, error));
		}

		public override void OnLoadingProgress(ILevelInfo pLevel, int progressAmount)
		{
			if (LoadingProgress != null)
				LoadingProgress (new EventArgs<ILevelInfo, int> (pLevel, progressAmount));
		}

		public override void OnUnloadComplete(ILevelInfo pLevel)
		{
			if (UnloadingComplete != null)
				UnloadingComplete (new EventArgs<ILevelInfo> (pLevel));
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Instantiate()
		{
			_instance = new LevelSystem ();
			Env.Game.GetIGameFramework ().GetILevelSystem ().AddListener (_instance);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Destroy()
		{
			if (_instance != null) 
			{
				if(Global.gEnv != null && Env.Game != null && Env.Game.GetIGameFramework() != null)
					Env.Game.GetIGameFramework ().GetILevelSystem ().RemoveListener (_instance);
				_instance.Dispose ();
				_instance = null;
			}
		}
	}

	/// <summary>
	/// Used by SceneManager to receive hierarchically ordered update calls.
	/// </summary>
	public interface IUpdateReceiver
	{
		SceneObject Root { get; } ///< Contains the Objects scene root
		void Update(); ///< Called by SceneManager
	}

	/// <summary>
	/// Handles prioritized and ordered updating of SceneObjects and Components. Receives update call itself from GameFramework. Will update scenes by priority in ascending order. Will update receivers in ascending order.
	/// @see GameFramework
	/// @see SceneObject
	/// @see Components.Component
	/// </summary>
	public class SceneManager : IGameUpdateReceiver
	{
		class SceneDescription
		{
			public SceneObject Root;
			public bool IsValid = true;
			public int Priority = 0;
			public Dictionary<IUpdateReceiver, int> UpdateReceiverOrder = new Dictionary<IUpdateReceiver, int>();
		}

		static SceneManager _instance = null;
		static List<SceneDescription> _scenes = new List<SceneDescription>();

		/// <summary>
		/// Will trigger refreshing of update receiver order inside update loop.
		/// </summary>
		/// <param name="root">Root object of referenced scene.</param>
		public static void InvalidateSceneOrder(SceneObject root)
		{
			var scene = _scenes.SingleOrDefault (x => x.Root == root);
			if (scene != null)
				scene.IsValid = false;
			else
				_scenes.Add (new SceneDescription() { Root = root, IsValid = false } );
		}

		/// <summary>
		/// Defines scene update order.
		/// </summary>
		/// <param name="root">Root object of referenced scene.</param>
		/// <param name="priority">Priority to be assigned.</param>
		public static void SetScenePriority(SceneObject root, int priority)
		{
			var scene = _scenes.SingleOrDefault (x => x.Root == root);
			if (scene != null)
				scene.Priority = priority;
		}

		/// <summary>
		/// Registers an update receiver by sorting it into its scene root element.
		/// </summary>
		/// <param name="ur">Object to be updated.</param>
		/// <param name="order">Call order.</param>
		public static void RegisterUpdateReceiver(IUpdateReceiver ur, int order = 0)
		{
			var root = ur.Root;
			var scene = _scenes.SingleOrDefault (x => x.Root == root);
			if (scene == null)
			{
				scene = new SceneDescription () { Root = root };
				_scenes.Add (scene);
			}
			scene.UpdateReceiverOrder[ur] = order;
		}

		/// <summary>
		/// Removes an update receiver.
		/// </summary>
		/// <param name="ur">Object to be removed.</param>
		public static void RemoveUpdateReceiver(IUpdateReceiver ur)
		{
			var root = ur.Root;
			var scene = _scenes.SingleOrDefault (x => x.Root == root);
			if (scene != null)
				scene.UpdateReceiverOrder.Remove(ur);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Instantiate()
		{
			_instance = new SceneManager();
			GameFramework.RegisterForUpdate (_instance);
		}

		/// <summary>
		/// Refreshes update receiver order if scene was invalidated. Calls all update functions for update receivers in correct scene priority and receiver order.
		/// </summary>
		public void OnUpdate()
		{
			var scenes = new List<SceneDescription> (_scenes);
			foreach (var scene in scenes.Where(x => !x.IsValid)) 
			{
				scene.Root.RefreshUpdateOrder ();
				scene.IsValid = true;
			}
			foreach(var scene in scenes.OrderBy(x => x.Priority))
			{
				var sorted = scene.UpdateReceiverOrder.OrderBy (x => x.Value).ToList();
				sorted.ForEach (x => { if (scene.UpdateReceiverOrder.ContainsKey(x.Key)) x.Key.Update(); });
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Destroy()
		{
			GameFramework.UnregisterFromUpdate (_instance);
			_scenes.Clear ();
			_instance = null;
		}
	}

	public interface IGameUpdateReceiver
	{
		void OnUpdate();
	}

	public interface IGameRenderReceiver
	{
		void OnRender();
	}

	/// <summary>
	/// Listens for engine sided updates and informs registered objects by calling their OnUpdate function. Objects may register here, in order to be invoked on global update calls. Controls target FPS.
	/// </summary>
	public class GameFramework : IGameFrameworkListener
	{
		static GameFramework _instance = null;
		static List<IGameUpdateReceiver> _updateReceivers = new List<IGameUpdateReceiver>();
		static List<IGameRenderReceiver> _renderReceivers = new List<IGameRenderReceiver>();
		ICVar _monoTargetFps;
		DateTime _nextFrameTime = DateTime.MinValue;

		public override void OnSaveGame (ISaveGame pSaveGame)
		{
			Log.Info<GameFramework> ("OnSaveGame");
		}

		public override void OnLoadGame (ILoadGame pLoadGame)
		{
			// nada
		}

		public override void OnForceLoadingWithFlash ()
		{
			// nada
		}

		public override void OnLevelEnd (string nextLevel)
		{
			// nada
		}

		public override void OnSavegameFileLoadedInMemory (string pLevelName)
		{
			// nada
		}

		/// <summary>
		/// Registered object will be invoked on function OnUpdate if CryEngine's GameFramework raises OnPostUpdate.
		/// </summary>
		public static void RegisterForUpdate(IGameUpdateReceiver obj)
		{
			_updateReceivers.Add (obj);
		}

		/// <summary>
		/// Registered object will be invoked on function OnRender if CryEngine's GameFramework raises OnPreRender.
		/// </summary>
		public static void RegisterForRender(IGameRenderReceiver obj)
		{
			_renderReceivers.Add (obj);
		}

		/// <summary>
		/// Unregisters object from OnPostUpdate.
		/// </summary>
		public static void UnregisterFromUpdate(IGameUpdateReceiver obj)
		{
			_updateReceivers.Remove (obj);
		}

		/// <summary>
		/// Unregisters object from OnPreRender.
		/// </summary>
		public static void UnregisterFromRender(IGameRenderReceiver obj)
		{
			_renderReceivers.Remove (obj);
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override void OnActionEvent (SActionEvent arg0)
		{
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override void OnPostUpdate (float fDeltaTime)
		{
			// Calculate time used to render last frame.
			FrameTime.Update();

			var updateReceivers = new List<IGameUpdateReceiver> (_updateReceivers);
			foreach (IGameUpdateReceiver obj in updateReceivers)
				obj.OnUpdate ();
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override void OnPreRender()
		{
			// Sleep if necessary, to match target FPS.
			if (_nextFrameTime > DateTime.Now)
				System.Threading.Thread.Sleep (Math.Max(0, (int)(_nextFrameTime - DateTime.Now).TotalMilliseconds));
			if(_monoTargetFps != null && _monoTargetFps.GetIVal() > 0)
				_nextFrameTime = DateTime.Now.AddSeconds (1.0f / (float)_monoTargetFps.GetIVal());

			var renderReceivers = new List<IGameRenderReceiver> (_renderReceivers);
			foreach (IGameRenderReceiver obj in renderReceivers)
				obj.OnRender ();
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Instantiate()
		{
			_instance = new GameFramework();
			Env.Game.GetIGameFramework ().RegisterListener (_instance, "MonoGameFramework", EFRAMEWORKLISTENERPRIORITY.FRAMEWORKLISTENERPRIORITY_DEFAULT);
			_instance._monoTargetFps = Env.Console.GetCVar ("mono_targetfps");
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Destroy()
		{
			_updateReceivers.Clear ();
			if (_instance != null)
			{
				if(Global.gEnv != null && Env.Game != null && Env.Game.GetIGameFramework() != null)
					Env.Game.GetIGameFramework().UnregisterListener(_instance);
				_instance.Dispose();
				_instance = null;
			}
		}
	}

	/// <summary>
	/// Wraps CryEngine's gEnv variable for global access to main modules. Initializes all C# sided handler and wrapper classes.
	/// </summary>
	public class Env
	{
		static bool _isInitialized = false;

		public static IRenderer Renderer { get { return Global.gEnv.pRenderer; } }
		public static IConsole Console { get { return Global.gEnv.pConsole; } }
		public static IInput Input { get { return Global.gEnv.pInput; } }
		public static ICryFont Font { get { return Global.gEnv.pCryFont; } }
		public static ISystem System { get { return Global.gEnv.pSystem; } }
		public static IEntitySystem EntitySystem { get { return Global.gEnv.pEntitySystem; } }
		public static IHardwareMouse Mouse { get { return Global.gEnv.pHardwareMouse; } }
		public static IFlowSystem FlowSystem { get { return Global.gEnv.pFlowSystem; } }
		public static IParticleManager ParticleManager { get { return Global.gEnv.pParticleManager; } }
		public static I3DEngine Engine { get { return Global.gEnv.p3DEngine; } }
		public static IGame Game { get { return Global.gEnv.pGame; } }
		public static ITimer Timer { get { return Global.gEnv.pTimer; }}
		public static IAudioSystem AudioSystem { get { return Global.gEnv.pAudioSystem; } }
		public static bool IsSandbox { get { return Global.gEnv.IsEditor(); } } ///< Indicates whether CryEngine is run in Editor mode.

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Initialize(InterDomainHandler handler)
		{
			if (_isInitialized)
				return;
			_isInitialized = true;

			SystemHandler.Instantiate ();
			CryEngine.Input.Instanciate ();
			Screen.Instanciate ();
			CryEngine.Mouse.Instantiate ();
			SceneManager.Instantiate ();
			GameFramework.Instantiate ();
			LevelSystem.Instantiate ();
			EntityFramework.Instantiate (handler);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Shutdown(InterDomainHandler handler)
		{
			if (!_isInitialized)
				return;
			_isInitialized = false;

			EntityFramework.Destroy (handler);
			GameFramework.Destroy ();
			SceneManager.Destroy ();
			LevelSystem.Destroy ();
			CryEngine.Mouse.Destroy ();
			Screen.Destroy ();
			CryEngine.Input.Destroy ();
			SystemHandler.Destroy ();
		}
	}

	public class AddIn : ICryEngineAddIn
	{
		public static InterDomainHandler InterDomainHandler { get; private set; }

		public void Initialize(InterDomainHandler handler)
		{
			InterDomainHandler = handler;
			Env.Initialize (handler);
		}

		public void OnFlowNodeSignal(FlowNode node, PropertyInfo signal)
		{
		}

		public void Shutdown()
		{
			Env.Shutdown (InterDomainHandler);
		}

		public void StartGame()
		{
			
		}

		public void EndGame()
		{
			
		}
	}

	/// <summary>
	/// Processes CryEngine's system callback and generates events from it.
	/// </summary>
	public class SystemHandler : ISystemEventListener
	{
		public static EventHandler FocusChanged;
		public static EventHandler PrecacheEnded;
		public static EventHandler EditorGameStart; ///< Raised if Game-Mode was initiated in Editor 
		public static EventHandler EditorGameEnded; ///< Raised if Game-Mode was exited in Editor 

		static SystemHandler _instance;

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Instantiate()
		{
			_instance = new SystemHandler();
			Env.System.GetISystemEventDispatcher ().RegisterListener (_instance);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Destroy()
		{
			if (_instance != null)
			{
				Env.System.GetISystemEventDispatcher ().RemoveListener (_instance);
				_instance.Dispose();
				_instance = null;
			}
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
#if WIN64
		public override void OnSystemEvent (ESystemEvent evt, ulong wparam, ulong lparam)
#elif WIN86
		public override void OnSystemEvent (ESystemEvent evt, uint wparam, uint lparam)
#endif
		{
			switch(evt)
			{
			case ESystemEvent.ESYSTEM_EVENT_CHANGE_FOCUS:
				if (FocusChanged != null)
					FocusChanged ();
				break;
			case ESystemEvent.ESYSTEM_EVENT_LEVEL_PRECACHE_END:
				if (PrecacheEnded != null)
					PrecacheEnded ();
				break;
				
			case ESystemEvent.ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
				if (wparam == 1 && EditorGameStart != null)
					EditorGameStart ();
				if (wparam == 0 && EditorGameEnded != null)
					EditorGameEnded ();
				break;
			}
		}
	}

	/// <summary>
	/// Wraps System.Drawing.Color object for easier usage.
	/// </summary>
	public class Color
	{
		public float R;
		public float G;
		public float B;
		public float A;

		public static Color Red { get { return new Color (1, 0, 0); } }
		public static Color Yellow { get { return new Color (1, 1, 0); } }
		public static Color Green { get { return new Color (0, 1, 0); } }
		public static Color Cyan { get { return new Color (0, 1, 1); } }
		public static Color SkyBlue { get { return new Color (0.25f, 0.5f, 1); } }
		public static Color Blue { get { return new Color (0, 0, 1); } }
		public static Color Pink { get { return new Color (1, 0, 1); } }
		public static Color White { get { return new Color (1, 1, 1); } }
		public static Color LiteGray { get { return new Color (0.75f, 0.75f, 0.75f); } }
		public static Color Gray { get { return new Color (0.5f, 0.5f, 0.5f); } }
		public static Color DarkGray { get { return new Color (0.25f, 0.25f, 0.25f); } }
		public static Color Black { get { return new Color (0, 0, 0); } }
		public static Color Transparent { get { return White.WithAlpha(0); } }

		public Color(float r, float g, float b, float a = 1)
		{
			R = r;
			G = g;
			B = b;
			A = a;
		}

		/// <summary>
		/// Creates a Color from given components.
		/// </summary>
		/// <returns>The RG.</returns>
		/// <param name="r">The red component.</param>
		/// <param name="g">The green component.</param>
		/// <param name="b">The blue component.</param>
		public static Color FromRGB(float r, float g, float b)
		{
			return new Color (r,g,b);
		}

		/// <summary>
		/// Creates new Color from current Color, with alpha applied to it.
		/// </summary>
		/// <returns>The alpha.</returns>
		/// <param name="a">The alpha component.</param>
		public Color WithAlpha(float a)
		{
			return new Color(R,G,B,a);
		}

		/// <summary>
		/// Creates a <see cref="System.Drawing.Color"/> from this Color.
		/// </summary>
		/// <returns>The created color.</returns>
		public System.Drawing.Color ToSystemColor()
		{
			return System.Drawing.Color.FromArgb ((int)(A * 255), (int)(R * 255), (int)(G * 255), (int)(B * 255));
		}

		/// <summary>
		/// Returns a <see cref="System.String"/> that represents the current <see cref="CryEngine.Color"/>.
		/// </summary>
		/// <returns>A <see cref="System.String"/> that represents the current <see cref="CryEngine.Color"/>.</returns>
		public override string ToString()
		{
			return R.ToString("0.0") + "," + G.ToString("0.0") + "," + B.ToString("0.0");
		}
	}

	/// <summary>
	/// Central point for mouse access. Listens to CryEngine sided mouse callbacks and generates events from them.
	/// </summary>
	public class Mouse : IHardwareMouseEventListener, IGameUpdateReceiver
	{
		public interface IMouseOverride
		{
			event MouseEventHandler LeftButtonDown;
			event MouseEventHandler LeftButtonUp;
            event MouseEventHandler RightButtonDown;
			event MouseEventHandler RightButtonUp;
			event MouseEventHandler Move;
		}

		public delegate void MouseEventHandler (int x, int y); ///< Used by all Mouse events.
		public static event MouseEventHandler OnLeftButtonDown;
		public static event MouseEventHandler OnLeftButtonUp;
        public static event MouseEventHandler OnRightButtonDown;
        public static event MouseEventHandler OnRightButtonUp;
		public static event MouseEventHandler OnMove;
		public static event MouseEventHandler OnWindowLeave;
		public static event MouseEventHandler OnWindowEnter;

		private static Mouse _instance = null;
		private static IMouseOverride s_override = null;
		private static float _lmx = 0;
		private static float _lmy = 0;
		private static bool _updateLeftDown = false;
		private static bool _updateLeftUp = false;
        private static bool _updateRightDown = false;
        private static bool _updateRightUp = false;
		private static uint _hitEntityId = 0;
		private static Vec2 _hitEntityUV = new Vec2();
		private static bool _cursorVisible = false;

		public static Point CursorPosition { get { return new Point (_lmx, _lmy); } } ///< Current Mouse Cursor Position, refreshed before update loop.
		public static bool LeftDown { get; private set; } ///< Indicates whether left mouse button is Down during one update phase.
		public static bool LeftUp { get; private set; } ///< Indicates whether left mouse button is Released during one update phase.
        public static bool RightDown { get; private set; } ///< Indicates whether right mouse button is Down during one update phase.
        public static bool RightUp { get; private set; } ///< Indicates whether right mouse button is Released during one update phase.
		public static uint HitEntityId ///< ID of IEntity under cursor position.
		{
			get { return _hitEntityId; }
			set { _hitEntityId = value; }
		}
		public static Vec2 HitEntityUV ///< UV of IEntity under cursor position.
		{
			get { return _hitEntityUV; }
			set { _hitEntityUV = value; }
		}
		public static EntitySystem.Entity HitEntity
		{
			get 
			{
				return Entity.ById (HitEntityId);
			}
		}

		public static void ShowCursor()
		{
			if (!_cursorVisible)
				Env.Mouse.IncrementCounter();
			_cursorVisible = true;
		}

		public static void HideCursor()
		{
			if (_cursorVisible)
				Env.Mouse.DecrementCounter();
			_cursorVisible = false;
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		/// <param name="x">The x coordinate.</param>
		/// <param name="y">The y coordinate.</param>
		/// <param name="eHardwareMouseEvent">Event struct.</param>
		/// <param name="wheelDelta">Wheel delta.</param>
		public override void OnHardwareMouseEvent(int x, int y, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta)
		{
            switch (eHardwareMouseEvent)
            {
            case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONDOWN:
                {
                    _updateLeftDown = true;
                    HitScenes(x, y);
                    if (OnLeftButtonDown != null)
                        OnLeftButtonDown(x, y);
                    break;
                }
            case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONUP:
                {
                    _updateLeftUp = true;
                    HitScenes(x, y);
                    if (OnLeftButtonUp != null)
                        OnLeftButtonUp(x, y);
                    break;
                }
            case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONDOWN:
                {
                    _updateRightDown = true;
                    HitScenes(x, y);
                    if (OnRightButtonDown != null)
                        OnRightButtonDown(x, y);
                    break;
                }
            case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONUP:
                {
                    _updateRightUp = true;
                    HitScenes(x, y);
                    if (OnRightButtonUp != null)
                        OnRightButtonUp(x, y);
                    break;
                }
            }
		}

		private static void OnOverrideLeftButtonDown(int x, int y)
		{
			_updateLeftDown = true;
			if (OnLeftButtonDown != null)
				OnLeftButtonDown (x, y);			
		}

		private static void OnOverrideLeftButtonUp(int x, int y)
		{
			_updateLeftUp = true;
			if (OnLeftButtonUp != null)
				OnLeftButtonUp (x, y);			
		}

        private static void OnOverrideRightButtonDown(int x, int y)
        {
            _updateRightDown = true;
            if (OnRightButtonDown != null)
                OnRightButtonDown (x, y);            
        }

        private static void OnOverrideRightButtonUp(int x, int y)
        {
            _updateRightUp = true;
            if (OnRightButtonUp != null)
                OnRightButtonUp (x, y);          
        }

		private static void OnOverrideMove(int x, int y)
		{
			if (OnMove != null)
				OnMove (x, y);          
		}

		public static void SetOverride(IMouseOverride mouseOverride)
		{
			if (s_override != null) {
				s_override.LeftButtonDown -= OnOverrideLeftButtonDown;
				s_override.LeftButtonUp -= OnOverrideLeftButtonUp;
                s_override.RightButtonDown -= OnOverrideRightButtonDown;
                s_override.RightButtonUp -= OnOverrideRightButtonUp;
				s_override.Move -= OnOverrideMove;
			}
			s_override = mouseOverride;
			if (s_override != null) {
				s_override.LeftButtonDown += OnOverrideLeftButtonDown;
				s_override.LeftButtonUp += OnOverrideLeftButtonUp;
                s_override.RightButtonDown += OnOverrideRightButtonDown;
				s_override.RightButtonUp += OnOverrideRightButtonUp;
				s_override.Move += OnOverrideMove;
			}
				
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public void OnUpdate()
		{	
			LeftDown = _updateLeftDown;
			LeftUp = _updateLeftUp;
            RightDown = _updateRightDown;
            RightUp = _updateRightUp;

			_updateLeftDown = false;
			_updateLeftUp = false;
            _updateRightDown = false;
            _updateRightUp = false;

			float x = 0, y = 0;
			Env.Mouse.GetHardwareMouseClientPosition (ref x, ref y);

			var w = Screen.Width;
			var h = Screen.Height;
			bool wasInside = _lmx >= 0 && _lmy >= 0 && _lmx < w && _lmy < h;
			bool isInside = x >= 0 && y >= 0 && x < w && y < h;
			_lmx = x;_lmy = y;

			HitScenes ((int)x, (int)y);
			if (wasInside && isInside) 
			{
				if (OnMove != null) 
					OnMove ((int)x, (int)y);
			} 
			else if (wasInside != isInside && isInside) 
			{
				if (OnWindowEnter != null) 
					OnWindowEnter ((int)x, (int)y);
			} 
			else if (wasInside != isInside && !isInside) 
			{
				if (OnWindowLeave != null) 
					OnWindowLeave ((int)x, (int)y);
			}
		}

		public static void HitScenes(int x, int y)
		{
			HitEntityId = 0;
			if (Camera.Current == null || Camera.Current.HostEntity == null)
				return;
			float u = 0, v = 0;
			var mouseDir = Camera.Unproject(x, y);
			HitEntityId = (uint)Env.Renderer.RayToUV (Camera.Current.Position, mouseDir, ref u, ref v);
			HitEntityUV.x = u;
			HitEntityUV.y = v;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Instantiate()
		{
			_instance = new Mouse();
			Global.gEnv.pHardwareMouse.AddListener(_instance);
			GameFramework.RegisterForUpdate (_instance);
			HitEntityId = 0;
			HitEntityUV = new Vec2 ();
		}

		/// <summary>
		/// Checks if cursor is inside a UIElement, considering its Bounds.
		/// </summary>
		public static bool CursorInside(UIElement e)
		{
			return e.RectTransform.Bounds.Contains(CursorPosition);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public static void Destroy()
		{
			if (_instance != null)
			{
				GameFramework.UnregisterFromUpdate (_instance);
				Global.gEnv.pHardwareMouse.RemoveListener (_instance);				
				_instance.Dispose();
				_instance = null;
			}
		}
	}

	/// <summary>
	/// Maps trigger identifier names to id's of internal audio project. Audio projects can be setup via SDL, WWISE or FMOD. 
	/// </summary>
	public class AudioManager
	{
		private static Dictionary<string, uint> _triggerByName = new Dictionary<string, uint> ();

		/// <summary>
		/// Gets trigger ID by trigger name. Plays back trigger with respect to currently loaded audio lib.
		/// </summary>
		/// <returns><c>true</c>, if trigger loaded successfully.</returns>
		/// <param name="triggerName">Trigger name.</param>
		public static bool PlayTrigger(string triggerName)
		{
			uint triggerId;
			if (!_triggerByName.TryGetValue (triggerName, out triggerId))
				triggerId = _triggerByName [triggerName] = Env.AudioSystem.GetAudioTriggerId (triggerName);

			if (triggerId != 0)
				Env.AudioSystem.PlayAudioTriggerId (triggerId);
			return triggerId != 0;
		}
	}

	/// <summary>
	/// General purpose randomizer.
	/// </summary>
	public class Rand
	{
		private static Random _random = new Random((int)DateTime.Now.Ticks);

		/// <summary>
		/// Returns next double between 0 and 1.
		/// </summary>
		/// <value>The next double.</value>
		public static double NextDouble
		{
			get 
			{
				return _random.NextDouble ();					
			}
		}

		/// <summary>
		/// Returns next int between 0 and range - 1.
		/// </summary>
		/// <returns>The int.</returns>
		/// <param name="range">Range.</param>
		public static int NextInt(int range)
		{
			return _random.Next(range);
		}
	}
}
