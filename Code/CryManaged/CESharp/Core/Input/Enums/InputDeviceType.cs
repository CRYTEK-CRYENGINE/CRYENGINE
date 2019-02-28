// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

namespace CryEngine
{
	/// <summary>
	/// The various input devices from which input can be received.
	/// </summary>
	public enum InputDeviceType
	{
		/// <summary>
		/// Generic keyboard device.
		/// </summary>
		Keyboard,
		/// <summary>
		/// Generic mouse device.
		/// </summary>
		Mouse,
		/// <summary>
		/// Generic joystick device.
		/// </summary>
		Joystick,
		/// <summary>
		/// An eye-tracking device.
		/// </summary>
		EyeTracker,
		/// <summary>
		/// Generic gamepad device.
		/// </summary>
		Gamepad,
		/// <summary>
		/// Generic motion-controller device.
		/// </summary>
		MotionController,
		/// <summary>
		/// Device is unknown.
		/// </summary>
		Unknown = 255
	}
}
