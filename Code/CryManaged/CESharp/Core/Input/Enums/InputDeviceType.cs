namespace CryEngine
{
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
