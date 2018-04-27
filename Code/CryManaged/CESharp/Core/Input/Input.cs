// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Pre-Processes CryEngine's Input callback and generates Input events. Classes may be registered here, in order to receive global Input events.
	/// </summary>
	public class Input : IInputEventListener
	{
		public static event EventHandler<SInputEvent> OnKey; ///< Fired when a key-state was modified

		internal static Input Instance { get; set; }

		private static bool _lShiftDown = false;
		private static bool _rShiftDown = false;
		private static Dictionary<string, Vector2> _axisByName = new Dictionary<string, Vector2>();

		public static bool ShiftDown { get { return _lShiftDown || _rShiftDown; } } ///< Set if Shift key is held Down

		public static Vector2 GetAxis(string name)
		{
			Vector2 axis;
			_axisByName.TryGetValue(name, out axis);
			return axis;
		}

		private Dictionary<string, string> _charByDescription = new Dictionary<string, string>() { { "comma", "," }, { "period", "." }, { "minus", "-" }, { "plus", "+" } };

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override bool OnInputEvent(SInputEvent e)
		{
			if(e.deviceType == EInputDeviceType.eIDT_Keyboard)
			{
				switch(e.state)
				{
					case EInputState.eIS_Down:
						SInputEventExtensions.KeyDownLog[e.keyId] = true;
						break;

					case EInputState.eIS_Released:
						SInputEventExtensions.KeyDownLog[e.keyId] = false;
						break;
				}

				if(e.KeyDown(EKeyId.eKI_LShift))
				{
					_lShiftDown = true;
				}
				if(e.KeyUp(EKeyId.eKI_LShift))
				{
					_lShiftDown = false;
				}
				if(e.KeyDown(EKeyId.eKI_RShift))
				{
					_rShiftDown = true;
				}
				if(e.KeyUp(EKeyId.eKI_RShift))
				{
					_rShiftDown = false;
				}

				// Preprocess keyName to contain valid Chars
				if(e.keyId == EKeyId.eKI_Space)
				{
					e.keyName.key = " ";
				}

				if(e.keyName.key.Length == 1)
				{
					if(ShiftDown)
					{
						e.keyName.key = e.keyName.key.ToUpper();
					}

					if(ShiftDown && Char.IsDigit(e.keyName.key[0]))
					{
						e.keyName.key = "=!\"§$%&/()".Substring(e.keyName.key[0] - '0', 1);
					}
				}
				else
				{
					string res;
					e.keyName.key = _charByDescription.TryGetValue(e.keyName.key, out res) ? res : string.Empty;
				}

				if(OnKey != null)
				{
					OnKey(e);
				}
			}
			else if(e.deviceType == EInputDeviceType.eIDT_Gamepad || e.deviceType == EInputDeviceType.eIDT_Mouse)
			{
				// Set keyName.key = string.Empty to avoid showing up gamepad
				// presses controller keys in text input forms
				e.keyName.key = string.Empty;

				switch(e.state)
				{
					case EInputState.eIS_Down:
						SInputEventExtensions.KeyDownLog[e.keyId] = true;
						break;

					case EInputState.eIS_Released:
						SInputEventExtensions.KeyDownLog[e.keyId] = false;
						break;

					case EInputState.eIS_Changed:
						SInputEventExtensions.KeyInputValueLog[e.keyId] = e.value;
						break;
				}

				if(OnKey != null)
				{
					OnKey(e);
				}
			}
			else if(e.deviceType == EInputDeviceType.eIDT_EyeTracker)
			{
				if(e.keyId == EKeyId.eKI_EyeTracker_X)
				{
					var axis = GetAxis("EyeTracker");
					_axisByName["EyeTracker"] = new Vector2(e.value, axis != null ? axis.y : 0);
				}
				if(e.keyId == EKeyId.eKI_EyeTracker_Y)
				{
					var axis = GetAxis("EyeTracker");
					_axisByName["EyeTracker"] = new Vector2(axis != null ? axis.x : 0, e.value);
				}
			}
			return false;
		}

		public static bool KeyDown(EKeyId key)
		{
			bool isDown;
			SInputEventExtensions.KeyDownLog.TryGetValue(key, out isDown);
			return isDown;
		}

		public static float GetInputValue(EKeyId key)
		{
			float changedValue;
			SInputEventExtensions.KeyInputValueLog.TryGetValue(key, out changedValue);
			return changedValue;
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		public override bool OnInputEventUI(SUnicodeEvent e)
		{
			return false;
		}

		internal Input()
		{
			AddListener();

			Engine.EndReload += AddListener;
		}

		void AddListener()
		{
			Global.gEnv.pInput.AddEventListener(this);
		}

		public override void Dispose()
		{
			Global.gEnv.pInput.RemoveEventListener(this);

			base.Dispose();
		}
	}
}
