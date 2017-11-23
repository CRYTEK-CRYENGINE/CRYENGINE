// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

namespace CryEngine
{
	/// <summary>
	/// The IDs for the keys that can receive input.
	/// </summary>
	public enum KeyId : uint
	{
		#region Keyboard input
		/// <summary>
		/// The escape key.
		/// </summary>
		Escape,
		/// <summary>
		/// The numeric key 1 at the top of the keyboard.
		/// </summary>
		Alpha1,
		/// <summary>
		/// The numeric key 2 at the top of the keyboard.
		/// </summary>
		Alpha2,
		/// <summary>
		/// The numeric key 3 at the top of the keyboard.
		/// </summary>
		Alpha3,
		/// <summary>
		/// The numeric key 4 at the top of the keyboard.
		/// </summary>
		Alpha4,
		/// <summary>
		/// The numeric key 5 at the top of the keyboard.
		/// </summary>
		Alpha5,
		/// <summary>
		/// The numeric key 6 at the top of the keyboard.
		/// </summary>
		Alpha6,
		/// <summary>
		/// The numeric key 7 at the top of the keyboard.
		/// </summary>
		Alpha7,
		/// <summary>
		/// The numeric key 8 at the top of the keyboard.
		/// </summary>
		Alpha8,
		/// <summary>
		/// The numeric key 9 at the top of the keyboard.
		/// </summary>
		Alpha9,
		/// <summary>
		/// The numeric key 0 at the top of the keyboard.
		/// </summary>
		Alpha0,
		/// <summary>
		/// The minus key at the top of the keyboard.
		/// </summary>
		Minus,
		/// <summary>
		/// The equals sign key.
		/// </summary>
		Equals,
		/// <summary>
		/// The backspace key.
		/// </summary>
		Backspace,
		/// <summary>
		/// The tab key.
		/// </summary>
		Tab,
		/// <summary>
		/// The "q" key.
		/// </summary>
		Q,
		/// <summary>
		/// The "w" key.
		/// </summary>
		W,
		/// <summary>
		/// The "e" key.
		/// </summary>
		E,
		/// <summary>
		/// The "r" key.
		/// </summary>
		R,
		/// <summary>
		/// The "t" key.
		/// </summary>
		T,
		/// <summary>
		/// The "y" key.
		/// </summary>
		Y,
		/// <summary>
		/// The "u" key.
		/// </summary>
		U,
		/// <summary>
		/// The "i" key.
		/// </summary>
		I,
		/// <summary>
		/// The "o" key.
		/// </summary>
		O,
		/// <summary>
		/// The "p" key.
		/// </summary>
		P,
		/// <summary>
		/// The left bracket key.
		/// </summary>
		LBracket,
		/// <summary>
		/// The right bracket key.
		/// </summary>
		RBracket,
		/// <summary>
		/// The enter key.
		/// </summary>
		Enter,
		/// <summary>
		/// The left control key.
		/// </summary>
		LCtrl,
		/// <summary>
		/// The "a" key.
		/// </summary>
		A,
		/// <summary>
		/// The "s" key.
		/// </summary>
		S,
		/// <summary>
		/// The "d" key.
		/// </summary>
		D,
		/// <summary>
		/// The "f" key.
		/// </summary>
		F,
		/// <summary>
		/// The "g" key.
		/// </summary>
		G,
		/// <summary>
		/// The "h" key.
		/// </summary>
		H,
		/// <summary>
		/// The "j" key.
		/// </summary>
		J,
		/// <summary>
		/// The "k" key.
		/// </summary>
		K,
		/// <summary>
		/// The "l" key.
		/// </summary>
		L,
		/// <summary>
		/// The semicolon key.
		/// </summary>
		Semicolon,
		/// <summary>
		/// The apostrophe key.
		/// </summary>
		Apostrophe,
		/// <summary>
		/// The tilde key.
		/// </summary>
		Tilde,
		/// <summary>
		/// The left shift key.
		/// </summary>
		LShift,
		/// <summary>
		/// The backslash key.
		/// </summary>
		Backslash,
		/// <summary>
		/// The "z" key.
		/// </summary>
		Z,
		/// <summary>
		/// The "x" key.
		/// </summary>
		X,
		/// <summary>
		/// The "c" key.
		/// </summary>
		C,
		/// <summary>
		/// The "v" key.
		/// </summary>
		V,
		/// <summary>
		/// The "b" key.
		/// </summary>
		B,
		/// <summary>
		/// The "n" key.
		/// </summary>
		N,
		/// <summary>
		/// The "m" key.
		/// </summary>
		M,
		/// <summary>
		/// The comma key.
		/// </summary>
		Comma,
		/// <summary>
		/// The period key.
		/// </summary>
		Period,
		/// <summary>
		/// The forward slash key.
		/// </summary>
		Slash,
		/// <summary>
		/// The right shift key.
		/// </summary>
		RShift,
		/// <summary>
		/// The multiply key on the numeric keypad.
		/// </summary>
		NP_Multiply,
		/// <summary>
		/// The left alt key.
		/// </summary>
		LAlt,
		/// <summary>
		/// The space key.
		/// </summary>
		Space,
		/// <summary>
		/// The Caps Lock key.
		/// </summary>
		CapsLock,
		/// <summary>
		/// The F1 function key.
		/// </summary>
		F1,
		/// <summary>
		/// The F2 function key.
		/// </summary>
		F2,
		/// <summary>
		/// The F3 function key.
		/// </summary>
		F3,
		/// <summary>
		/// The F4 function key.
		/// </summary>
		F4,
		/// <summary>
		/// The F5 function key.
		/// </summary>
		F5,
		/// <summary>
		/// The F6 function key.
		/// </summary>
		F6,
		/// <summary>
		/// The F7 function key.
		/// </summary>
		F7,
		/// <summary>
		/// The F8 function key.
		/// </summary>
		F8,
		/// <summary>
		/// The F9 function key.
		/// </summary>
		F9,
		/// <summary>
		/// The F10 function key.
		/// </summary>
		F10,
		/// <summary>
		/// The Num Lock key.
		/// </summary>
		NumLock,
		/// <summary>
		/// The Scroll Lock key.
		/// </summary>
		ScrollLock,
		/// <summary>
		/// The 7 key on the numeric keypad.
		/// </summary>
		NP_7,
		/// <summary>
		/// The 8 key on the numeric keypad.
		/// </summary>
		NP_8,
		/// <summary>
		/// The 9 key on the numeric keypad.
		/// </summary>
		NP_9,
		/// <summary>
		/// The substract key on the numeric keypad.
		/// </summary>
		NP_Substract,
		/// <summary>
		/// The 4 key on the numeric keypad.
		/// </summary>
		NP_4,
		/// <summary>
		/// The 5 key on the numeric keypad.
		/// </summary>
		NP_5,
		/// <summary>
		/// The 6 key on the numeric keypad.
		/// </summary>
		NP_6,
		/// <summary>
		/// The add key on the numeric keypad.
		/// </summary>
		NP_Add,
		/// <summary>
		/// The 1 key on the numeric keypad.
		/// </summary>
		NP_1,
		/// <summary>
		/// The 2 key on the numeric keypad.
		/// </summary>
		NP_2,
		/// <summary>
		/// The 3 key on the numeric keypad.
		/// </summary>
		NP_3,
		/// <summary>
		/// The 0 key on the numeric keypad.
		/// </summary>
		NP_0,
		/// <summary>
		/// The F11 function key.
		/// </summary>
		F11,
		/// <summary>
		/// The F12 function key.
		/// </summary>
		F12,
		/// <summary>
		/// The F13 function key.
		/// </summary>
		F13,
		/// <summary>
		/// The F14 function key.
		/// </summary>
		F14,
		/// <summary>
		/// The F15 function key.
		/// </summary>
		F15,
		/// <summary>
		/// The colon key.
		/// </summary>
		Colon,
		/// <summary>
		/// The underline key.
		/// </summary>
		Underline,
		/// <summary>
		/// The enter key on the numeric keypad.
		/// </summary>
		NP_Enter,
		/// <summary>
		/// The right control key.
		/// </summary>
		RCtrl,
		/// <summary>
		/// The period key on the numeric keypad.
		/// </summary>
		NP_Period,
		/// <summary>
		/// The divide key on the numeric keypad.
		/// </summary>
		NP_Divide,
		/// <summary>
		/// The print scree key.
		/// </summary>
		Print,
		/// <summary>
		/// The right alt key.
		/// </summary>
		RAlt,
		/// <summary>
		/// The pause|break key.
		/// </summary>
		Pause,
		/// <summary>
		/// The home key.
		/// </summary>
		Home,
		/// <summary>
		/// The up direction key.
		/// </summary>
		Up,
		/// <summary>
		/// The page up key.
		/// </summary>
		PgUp,
		/// <summary>
		/// The left direction key.
		/// </summary>
		Left,
		/// <summary>
		/// The right direction key.
		/// </summary>
		Right,
		/// <summary>
		/// The end key.
		/// </summary>
		End,
		/// <summary>
		/// The down direction key.
		/// </summary>
		Down,
		/// <summary>
		/// The page down key.
		/// </summary>
		PgDn,
		/// <summary>
		/// The insert key.
		/// </summary>
		Insert,
		/// <summary>
		/// The delete key.
		/// </summary>
		Delete,
		/// <summary>
		/// The left Windows key.
		/// </summary>
		LWin,
		/// <summary>
		/// The right Windows key.
		/// </summary>
		RWin,
		/// <summary>
		/// The apps key.
		/// </summary>
		Apps,
		/// <summary>
		/// The OEM_102 key.
		/// </summary>
		OEM_102,

		#endregion
		#region Mouse input

		/// <summary>
		/// The left mouse button.
		/// </summary>
		Mouse1 = 256u,
		/// <summary>
		/// The right mouse button.
		/// </summary>
		Mouse2,
		/// <summary>
		/// The middle mouse button.
		/// </summary>
		Mouse3,
		/// <summary>
		/// The fourth mouse button.
		/// </summary>
		Mouse4,
		/// <summary>
		/// The fifth mouse button.
		/// </summary>
		Mouse5,
		/// <summary>
		/// The sixth mouse button.
		/// </summary>
		Mouse6,
		/// <summary>
		/// The seventh mouse button.
		/// </summary>
		Mouse7,
		/// <summary>
		/// The eight mouse button.
		/// </summary>
		Mouse8,
		/// <summary>
		/// The mouse wheel up axis.
		/// </summary>
		MouseWheelUp,
		/// <summary>
		/// The mouse wheel down axis.
		/// </summary>
		MouseWheelDown,
		/// <summary>
		/// The relative sideways movement of the mouse.
		/// </summary>
		MouseX,
		/// <summary>
		/// The relative forward movement of the mouse.
		/// </summary>
		MouseY,
		/// <summary>
		/// The relative upward movement of the mouse.
		/// </summary>
		MouseZ,
		/// <summary>
		/// The absolute x position of the mouse.
		/// </summary>
		MouseXAbsolute,
		/// <summary>
		/// The absolute y position of the mouse.
		/// </summary>
		MouseYAbsolute,
		/// <summary>
		/// The last position of the mouse.
		/// </summary>
		MouseLast,

		#endregion
		#region XInput controller

		/// <summary>
		/// The up direction on the directional pad of the XInput controller.
		/// </summary>
		XI_DPadUp = 512u,
		/// <summary>
		/// The down direction on the directional pad of the XInput controller.
		/// </summary>
		XI_DPadDown,
		/// <summary>
		/// The left direction on the directional pad of the XInput controller.
		/// </summary>
		XI_DPadLeft,
		/// <summary>
		/// The right direction on the directional pad of the XInput controller.
		/// </summary>
		XI_DPadRight,
		/// <summary>
		/// The start button of the XInput controller.
		/// </summary>
		XI_Start,
		/// <summary>
		/// The back button of the XInput controller.
		/// </summary>
		XI_Back,
		/// <summary>
		/// The left thumb stick button on the XInput controller.
		/// </summary>
		XI_ThumbL,
		/// <summary>
		/// The right thumb stick button on the XInput controller.
		/// </summary>
		XI_ThumbR,
		/// <summary>
		/// The left shoulder button on the XInput controller.
		/// </summary>
		XI_ShoulderL,
		/// <summary>
		/// The right shoulder button on the XInput controller.
		/// </summary>
		XI_ShoulderR,
		/// <summary>
		/// The A button on the XInput controller.
		/// </summary>
		XI_A,
		/// <summary>
		/// The B button on the XInput controller.
		/// </summary>
		XI_B,
		/// <summary>
		/// The X button on the XInput controller.
		/// </summary>
		XI_X,
		/// <summary>
		/// The Y button on the XInput controller.
		/// </summary>
		XI_Y,
		/// <summary>
		/// The left trigger axis on the XInput controller.
		/// </summary>
		XI_TriggerL,
		/// <summary>
		/// The right trigger axis on the XInput controller.
		/// </summary>
		XI_TriggerR,
		/// <summary>
		/// The horizontal motion of the left thumb stick on the XInput controller.
		/// </summary>
		XI_ThumbLX,
		/// <summary>
		/// The vertical motion of the left thumb stick on the XInput controller.
		/// </summary>
		XI_ThumbLY,
		/// <summary>
		/// Indicates whether the left thumb stick is pressed upward on the XInput controller.
		/// </summary>
		XI_ThumbLUp,
		/// <summary>
		/// Indicates whether the left thumb stick is pressed downward on the XInput controller.
		/// </summary>
		XI_ThumbLDown,
		/// <summary>
		/// Indicates whether the left thumb stick is pressed to the left on the XInput controller.
		/// </summary>
		XI_ThumbLLeft,
		/// <summary>
		/// Indicates whether the left thumb stick is pressed to the right on the XInput controller.
		/// </summary>
		XI_ThumbLRight,
		/// <summary>
		/// The horizontal motion of the right thumb stick on the XInput controller.
		/// </summary>
		XI_ThumbRX,
		/// <summary>
		/// The vertical motion of the right thumb stick on the XInput controller.
		/// </summary>
		XI_ThumbRY,
		/// <summary>
		/// Indicates whether the right thumb stick is pressed upward on the XInput controller.
		/// </summary>
		XI_ThumbRUp,
		/// <summary>
		/// Indicates whether the right thumb stick is pressed downward on the XInput controller.
		/// </summary>
		XI_ThumbRDown,
		/// <summary>
		/// Indicates whether the right thumb stick is pressed to the left on the XInput controller.
		/// </summary>
		XI_ThumbRLeft,
		/// <summary>
		/// Indicates whether the right thumb stick is pressed to the right on the XInput controller.
		/// </summary>
		XI_ThumbRRight,
		/// <summary>
		/// The left trigger button on the XInput controller.
		/// </summary>
		XI_TriggerLBtn,
		/// <summary>
		/// The right trigger button on the XInput controller.
		/// </summary>
		XI_TriggerRBtn,
		/// <summary>
		/// The connect button on the XInput controller.
		/// </summary>
		[System.Obsolete("XI_Connect is deprecated because all devices can be connected. Use SYS_ConnectDevice instead.")]
		XI_Connect,
		/// <summary>
		/// The disconnect button on the XInput controller.
		/// </summary>
		[System.Obsolete("XI_Disconnect is deprecated because all devices can be connected. Use SYS_DisconnectDevice instead.")]
		XI_Disconnect,

		#endregion
		#region Orbis controller

		/// <summary>
		/// The options button on the PS4 controller.
		/// </summary>
		Orbis_Options = 1024u,
		/// <summary>
		/// The left thumb stick button on the PS4 controller.
		/// </summary>
		Orbis_L3,
		/// <summary>
		/// The right thumb stick button on the PS4 controller.
		/// </summary>
		Orbis_R3,
		/// <summary>
		/// The up directional pad button on the PS4 controller.
		/// </summary>
		Orbis_Up,
		/// <summary>
		/// The right directional pad button on the PS4 controller.
		/// </summary>
		Orbis_Right,
		/// <summary>
		/// The down directional pad button on the PS4 controller.
		/// </summary>
		Orbis_Down,
		/// <summary>
		/// The left directional pad button on the PS4 controller.
		/// </summary>
		Orbis_Left,
		/// <summary>
		/// The L2 button on the PS4 controller.
		/// </summary>
		Orbis_L2,
		/// <summary>
		/// The R2 button on the PS4 controller.
		/// </summary>
		Orbis_R2,
		/// <summary>
		/// The L1 button on the PS4 controller.
		/// </summary>
		Orbis_L1,
		/// <summary>
		/// The R1 button on the PS4 controller.
		/// </summary>
		Orbis_R1,
		/// <summary>
		/// The triangle button on the PS4 controller.
		/// </summary>
		Orbis_Triangle,
		/// <summary>
		/// The circle button on the PS4 controller.
		/// </summary>
		Orbis_Circle,
		/// <summary>
		/// The cross button on the PS4 controller.
		/// </summary>
		Orbis_Cross,
		/// <summary>
		/// The square button on the PS4 controller.
		/// </summary>
		Orbis_Square,
		/// <summary>
		/// The horizontal motion of the left thumb stick on the PS4 controller.
		/// </summary>
		Orbis_StickLX,
		/// <summary>
		/// The vertical motion of the left thumb stick on the PS4 controller.
		/// </summary>
		Orbis_StickLY,
		/// <summary>
		/// The horizontal motion of the right thumb stick on the PS4 controller.
		/// </summary>
		Orbis_StickRX,
		/// <summary>
		/// The vertical motion of the right thumb stick on the PS4 controller.
		/// </summary>
		Orbis_StickRY,
		/// <summary>
		/// The x axis rotation of the PS4 controller.
		/// </summary>
		Orbis_RotX,
		/// <summary>
		/// The y axis rotation of the PS4 controller.
		/// </summary>
		Orbis_RotY,
		/// <summary>
		/// The z axis rotation of the PS4 controller.
		/// </summary>
		Orbis_RotZ,
		/// <summary>
		/// Not implemented.
		/// </summary>
		Orbis_RotX_KeyL,
		/// <summary>
		/// Not implemented.
		/// </summary>
		Orbis_RotX_KeyR,
		/// <summary>
		/// Not implemented.
		/// </summary>
		Orbis_RotZ_KeyD,
		/// <summary>
		/// Not implemented.
		/// </summary>
		Orbis_RotZ_KeyU,
		/// <summary>
		/// The left trigger axis on the PS4 controller.
		/// </summary>
		Orbis_LeftTrigger,
		/// <summary>
		/// The right trigger axis on the PS4 controller.
		/// </summary>
		Orbis_RightTrigger,
		/// <summary>
		/// The touchpad on the PS4 controller.
		/// </summary>
		Orbis_Touch,

		#endregion
		#region Oculus touch
		/// <summary>
		/// The A button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_A = 2048u,
		/// <summary>
		/// The B button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_B,
		/// <summary>
		/// The X button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_X,
		/// <summary>
		/// The Y button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_Y,
		/// <summary>
		/// The left thumb button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_L3,
		/// <summary>
		/// The right thumb button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_R3,
		/// <summary>
		/// The left trigger button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_TriggerBtnL,
		/// <summary>
		/// The right trigger button on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_TriggerBtnR,
		/// <summary>
		/// The left index finger trigger axis on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_L1,
		/// <summary>
		/// The right index finger trigger axis on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_R1,
		/// <summary>
		/// The left hand trigger axis on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_L2,
		/// <summary>
		/// The right hand trigger axis on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_R2,
		/// <summary>
		/// The vertical motion of the left thumb stick the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_StickL_Y,
		/// <summary>
		/// The vertical motion of the right thumb stick on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_StickR_Y,
		/// <summary>
		/// The horizontal motion of the left thumb stick on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_StickL_X,
		/// <summary>
		/// The horizontal motion of the right thumb stick on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_StickR_X,
		/// <summary>
		/// The left thumb up gesture on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_Gesture_ThumbUpL,
		/// <summary>
		/// The right thumb up gesture on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_Gesture_ThumbUpR,
		/// <summary>
		/// The left index finger pointing gesture on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_Gesture_IndexPointingL,
		/// <summary>
		/// The right index finger pointing gesture on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_Gesture_IndexPointingR,
		
		/// <summary>
		/// The number of symbols on the Oculus Touch.
		/// </summary>
		Motion_OculusTouch_NUM_SYMBOLS = 16u,
		/// <summary>
		/// The index of the last Oculus Touch button in this enum.
		/// </summary>
		Motion_OculusTouch_LastButtonIndex = Motion_OculusTouch_TriggerBtnR,
		/// <summary>
		/// The index of the first gesture for the Oculus Touch in this enum.
		/// </summary>
		Motion_OculusTouch_FirstGestureIndex = Motion_OculusTouch_Gesture_ThumbUpL,
		/// <summary>
		/// The index of the last gesture for the Oculus Touch in this enum.
		/// </summary>
		Motion_OculusTouch_LastGestureIndex = Motion_OculusTouch_Gesture_IndexPointingR,
		/// <summary>
		/// The index of the first trigger for the Oculus Touch in this enum.
		/// </summary>
		Motion_OculusTouch_FirstTriggerIndex = Motion_OculusTouch_L1,
		/// <summary>
		/// The index of the last trigger for the Oculus Touch in this enum.
		/// </summary>
		Motion_OculusTouch_LastTriggerIndex = Motion_OculusTouch_R2,

		#endregion
		#region Eyetracker
		/// <summary>
		/// The horizontal position of the eyetracker.
		/// </summary>
		EyeTracker_X = 4096u,
		/// <summary>
		/// The vertical position of the eyetracker.
		/// </summary>
		EyeTracker_Y,

		#endregion
		#region Open VR
		/// <summary>
		/// The system button on an Open VR controller.
		/// </summary>
		Motion_OpenVR_System = 2064u,
		/// <summary>
		/// The application menu button on an Open VR controller.
		/// </summary>
		Motion_OpenVR_ApplicationMenu,
		/// <summary>
		/// The grip button on an Open VR controller.
		/// </summary>
		Motion_OpenVR_Grip,
		/// <summary>
		/// The horizontal position on the touch pad of an Open VR controller.
		/// </summary>
		Motion_OpenVR_TouchPad_X,
		/// <summary>
		/// The vertical position on the touch pad of an Open VR controller.
		/// </summary>
		Motion_OpenVR_TouchPad_Y,
		/// <summary>
		/// The trigger axis on an Open VR controller.
		/// </summary>
		Motion_OpenVR_Trigger,
		/// <summary>
		/// The trigger button on an Open VR controller.
		/// </summary>
		Motion_OpenVR_TriggerBtn,
		/// <summary>
		/// The touch pad button on an Open VR controller.
		/// </summary>
		Motion_OpenVR_TouchPadBtn,
		/// <summary>
		/// The number of symbols on an Open VR controller.
		/// </summary>
		Motion_OpenVR_NUM_SYMBOLS = 8u,

		#endregion
		#region Other

		/// <summary>
		/// SYS_Commit will be ignored by input blocking functionality.
		/// </summary>
		SYS_Commit = 8192u,
		/// <summary>
		/// SYS_ConnectDevice will be ignored by input blocking functionality.
		/// </summary>
		SYS_ConnectDevice,
		/// <summary>
		/// SYS_DisconnectDevice will be ignored by input blocking functionality.
		/// </summary>
		SYS_DisconnectDevice,
		/// <summary>
		/// Terminator entry.
		/// </summary>
		Unknown = uint.MaxValue
		
		#endregion
	}
}
