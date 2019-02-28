// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Pre-Processes CryEngine's Input callback and generates Input events. Classes may be registered here, in order to receive global Input events.
	/// </summary>
	public static class Input
	{
		internal class InputListener : IInputEventListener
		{
			internal event Action<SInputEvent> OnInputReceived;

			internal InputListener()
			{
				AddListener();
			}

			private void AddListener()
			{
				Global.gEnv.pInput.AddEventListener(this);
			}

			private void RemoveListener()
			{
				Global.gEnv.pInput.RemoveEventListener(this);
			}

			public override void Dispose()
			{
				RemoveListener();

				base.Dispose();
			}

			public override bool OnInputEvent(SInputEvent arg0)
			{
				OnInputReceived?.Invoke(arg0);
				return false;
			}

			public override bool OnInputEventUI(SUnicodeEvent arg0)
			{
				return false;
			}
		}

		internal struct KeyState
		{
			internal bool Down { get; set; }
			internal float Value { get; set; }
		}

		/// <summary>
		/// Fired when a key-state was modified
		/// </summary>
		public static event Action<InputEvent> OnKey;

		private static InputListener _listener;
		private static bool _lShiftDown = false;
		private static bool _rShiftDown = false;
		private static readonly Dictionary<string, Vector2> _axisByName = new Dictionary<string, Vector2>();
		private static readonly Dictionary<KeyId, KeyState> _keyStateCache = new Dictionary<KeyId, KeyState>();

		/// <summary>
		/// Set if Shift key is held Down
		/// </summary>
		/// <value><c>true</c> if shift down; otherwise, <c>false</c>.</value>
		public static bool ShiftDown { get { return _lShiftDown || _rShiftDown; } }

		private static readonly Dictionary<string, string> _charByDescription = new Dictionary<string, string> { { "comma", "," }, { "period", "." }, { "minus", "-" }, { "plus", "+" } };

		static Input()
		{
			Engine.StartReload += RemoveListener;
			Engine.EndReload += AddListener;
		}

		private static void AddListener()
		{
			if(_listener != null)
			{
				RemoveListener();
			}

			_listener = new InputListener();
			_listener.OnInputReceived += OnInput;
		}

		private static void RemoveListener()
		{
			_listener?.Dispose();
			_listener = null;
		}

		/// <summary>
		/// Called to make sure Input is initialized and receiving events at the start of the application.
		/// </summary>
		internal static void Initialize()
		{
			AddListener();

			if(_listener == null)
			{
				//If this is ever called it means there's something wrong with the static constructor.
				Log.Error("Input listener is null, which can never happen!");
				throw new NullReferenceException();
			}
		}

		private static void OnInput(SInputEvent nativeEvent)
		{
			var managedEvent = new InputEvent(nativeEvent);
			if(managedEvent.DeviceType == InputDeviceType.Keyboard)
			{
				switch(managedEvent.State)
				{
				case InputState.Down:
					SetKeyStateDown(managedEvent.KeyId, true);
					break;

				case InputState.Released:
					SetKeyStateDown(managedEvent.KeyId, false);
					break;
				}

				if(KeyDown(KeyId.LShift))
				{
					_lShiftDown = true;
				}
				if(KeyUp(KeyId.LShift))
				{
					_lShiftDown = false;
				}
				if(KeyDown(KeyId.RShift))
				{
					_rShiftDown = true;
				}
				if(KeyUp(KeyId.RShift))
				{
					_rShiftDown = false;
				}

				// Preprocess keyName to contain valid Chars
				if(managedEvent.KeyId == KeyId.Space)
				{
					managedEvent.KeyName = " ";
				}

				if(managedEvent.KeyName.Length == 1)
				{
					if(ShiftDown)
					{
						managedEvent.KeyName = managedEvent.KeyName.ToUpper();
					}

					if(ShiftDown && char.IsDigit(managedEvent.KeyName[0]))
					{
						managedEvent.KeyName = "=!\"§$%&/()".Substring(managedEvent.KeyName[0] - '0', 1);
					}
				}
				else
				{
					string res;
					managedEvent.KeyName = _charByDescription.TryGetValue(managedEvent.KeyName, out res) ? res : string.Empty;
				}

				OnKey?.Invoke(managedEvent);
			}
			else if(managedEvent.DeviceType == InputDeviceType.Gamepad || managedEvent.DeviceType == InputDeviceType.Mouse)
			{
				// Set keyName.key = string.Empty to avoid showing up gamepad
				// presses controller keys in text input forms
				managedEvent.KeyName = string.Empty;

				switch(managedEvent.State)
				{
				case InputState.Down:
					SetKeyStateDown(managedEvent.KeyId, true);
					break;

				case InputState.Released:
					SetKeyStateDown(managedEvent.KeyId, false);
					break;

				case InputState.Changed:
					SetKeyStateValue(managedEvent.KeyId, managedEvent.Value);
					break;
				}

				if(OnKey != null)
				{
					OnKey(managedEvent);
				}
			}
			else if(managedEvent.DeviceType == InputDeviceType.EyeTracker)
			{
				if(managedEvent.KeyId == KeyId.EyeTracker_X)
				{
					var axis = GetAxis("EyeTracker");
					_axisByName["EyeTracker"] = new Vector2(managedEvent.Value, axis != null ? axis.y : 0);
				}
				if(managedEvent.KeyId == KeyId.EyeTracker_Y)
				{
					var axis = GetAxis("EyeTracker");
					_axisByName["EyeTracker"] = new Vector2(axis != null ? axis.x : 0, managedEvent.Value);
				}
			}
		}

		private static void SetKeyStateDown(KeyId id, bool down)
		{
			KeyState keyState;
			if(!_keyStateCache.TryGetValue(id, out keyState))
			{
				keyState = new KeyState();
			}
			keyState.Down = down;
			_keyStateCache[id] = keyState;
		}

		private static void SetKeyStateValue(KeyId id, float value)
		{
			KeyState keyState;
			if(!_keyStateCache.TryGetValue(id, out keyState))
			{
				keyState = new KeyState();
			}
			keyState.Value = value;
			_keyStateCache[id] = keyState;
		}

		/// <summary>
		/// Returns the value of and axis by name. 
		/// Can be used to retrieve EyeTracker values.
		/// </summary>
		/// <returns>The axis.</returns>
		/// <param name="name">Name.</param>
		public static Vector2 GetAxis(string name)
		{
			Vector2 axis;
			_axisByName.TryGetValue(name, out axis);
			return axis;
		}

		/// <summary>
		/// Used to detect if a key is not being pressed in the current frame.
		/// </summary>
		/// <returns><c>true</c> if the key is up, <c>false</c> otherwise.</returns>
		/// <param name="key">Key.</param>
		public static bool KeyUp(KeyId key)
		{
			KeyState state;
			if(_keyStateCache.TryGetValue(key, out state))
			{
				return !state.Down;
			}
			return true;
		}

		/// <summary>
		/// Used to detect if a key is being hold down in the current frame.
		/// </summary>
		/// <returns><c>true</c> if the key is down, <c>false</c> otherwise.</returns>
		/// <param name="key">Key.</param>
		public static bool KeyDown(KeyId key)
		{
			KeyState state;
			if(_keyStateCache.TryGetValue(key, out state))
			{
				return state.Down;
			}
			return false;
		}

		/// <summary>
		/// Get the current input value for the specified key.
		/// </summary>
		/// <returns>The input value.</returns>
		/// <param name="key">Key.</param>
		public static float GetInputValue(KeyId key)
		{
			KeyState state;
			if(_keyStateCache.TryGetValue(key, out state))
			{
				return state.Value;
			}
			return 0.0f;
		}
	}
}
