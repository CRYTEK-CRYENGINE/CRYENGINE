namespace CryEngine
{
	/// <summary>
	/// States an action from the ActionHandler can be in.
	/// </summary>
	public enum InputState
	{
		/// <summary>
		/// Unable to identify the current state.
		/// </summary>
		Unknown = 0,
		/// <summary>
		/// The Action has been pressed down.
		/// </summary>
		Pressed = 1,
		/// <summary>
		/// The Action has been released and is now up.
		/// </summary>
		Released = 2,
		/// <summary>
		/// The Action is being hold down.
		/// </summary>
		Down = 4,
		/// <summary>
		/// The value of the Action has changed. For example the mouse has moved.
		/// </summary>
		Changed = 8
	}
}
