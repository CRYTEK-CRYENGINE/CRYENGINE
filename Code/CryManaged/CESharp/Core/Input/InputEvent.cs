using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Data container for input events.
	/// </summary>
	public struct InputEvent
	{
		/// <summary>
		/// Local index of this particular controller type.
		/// </summary>
		/// <value>The index of the device.</value>
		public int DeviceIndex { get; internal set; }

		/// <summary>
		/// Device type from which the event originated.
		/// </summary>
		/// <value>The type of the device.</value>
		public InputDeviceType DeviceType { get; internal set; }

		/// <summary>
		/// Process wide unique controller ID.
		/// </summary>
		/// <value>The device unique identifier.</value>
		public int DeviceUniqueId { get; internal set; }

		/// <summary>
		/// Device-specific id corresponding to the event.
		/// </summary>
		/// <value>The key identifier.</value>
		public KeyId KeyId { get; internal set; }

		/// <summary>
		/// Human readable name of the event.
		/// </summary>
		/// <value>The name of the key.</value>
		public string KeyName { get; internal set; }

		/// <summary>
		/// Key modifiers enabled at the time of this event.
		/// </summary>
		/// <value>The modifiers.</value>
		public InputModifierFlags InputModifiers { get; internal set; }

		//TODO Implements pSymbol in managed
		internal SInputSymbol Symbol { get; set; }

		/// <summary>
		/// Type of input event.
		/// </summary>
		/// <value>The state.</value>
		public InputState State { get; internal set; }

		/// <summary>
		/// Value associated with the event.
		/// </summary>
		/// <value>The value.</value>
		public float Value { get; internal set; }

		internal InputEvent(SInputEvent nativeEvent)
		{
			DeviceIndex = nativeEvent.deviceIndex;
			DeviceType = (InputDeviceType)nativeEvent.deviceType;
			DeviceUniqueId = nativeEvent.deviceUniqueID;
			KeyId = (KeyId)nativeEvent.keyId;
			KeyName = nativeEvent.keyName.key;
			InputModifiers = (InputModifierFlags)nativeEvent.modifiers;
			State = (InputState)nativeEvent.state;
			Value = nativeEvent.value;
			Symbol = nativeEvent.pSymbol;
		}

		internal SInputEvent ToNative()
		{
			return new SInputEvent
			{
				deviceIndex = (byte)DeviceIndex,
				deviceType = (EInputDeviceType)DeviceType,
				deviceUniqueID = (byte)DeviceUniqueId,
				keyId = (EKeyId)KeyId,
				keyName = new TKeyName(KeyName),
				modifiers = (int)InputModifiers,
				pSymbol = Symbol,
				state = (EInputState)State,
				value = Value
			};
		}

		/// <summary>
		/// Detect if this event is about the specified key being pressed.
		/// </summary>
		/// <returns><c>true</c> if the key was pressed in this event, <c>false</c> otherwise.</returns>
		/// <param name="keyId">Key identifier.</param>
		public bool KeyPressed(KeyId keyId)
		{
			return KeyId == keyId && State == InputState.Pressed;
		}

		/// <summary>
		/// Detect if this event is about the specified key being released.
		/// </summary>
		/// <returns><c>true</c> if the key was released in this event, <c>false</c> otherwise.</returns>
		/// <param name="keyId">Key identifier.</param>
		public bool KeyUp(KeyId keyId)
		{
			return KeyId == keyId && State == InputState.Released;
		}

		/// <summary>
		/// Detect if this event is about the specified key being changed.
		/// </summary>
		/// <returns><c>true</c> if the key has changed in this event, <c>false</c> otherwise.</returns>
		/// <param name="keyId">Key identifier.</param>
		public bool KeyChanged(KeyId keyId)
		{
			return KeyId == keyId && State == InputState.Changed;
		}

		/// <summary>
		/// Detect if the specified key is currently pushed down during this frame.
		/// </summary>
		/// <returns><c>true</c> if key is being pressed during this frame, <c>false</c> otherwise.</returns>
		/// <param name="keyId">Key identifier.</param>
		public bool KeyDown(KeyId keyId)
		{
			return Input.KeyDown(keyId);
		}
	}
}
