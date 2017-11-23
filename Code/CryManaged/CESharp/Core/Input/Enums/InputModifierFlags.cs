// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using System;

namespace CryEngine
{
	//Wraps the native EModifierMask

	/// <summary>
	/// Flags to indicate if certain modifier keys are being pressed.
	/// </summary>
	[Flags]
	public enum InputModifierFlags
	{
		/// <summary>
		/// No key is pressed at the moment.
		/// </summary>
		None = 0,
		/// <summary>
		/// The left control key is pressed.
		/// </summary>
		LCtrl = 1 << 0,
		/// <summary>
		/// The left shift key is pressed.
		/// </summary>
		LShift = 1 << 1,
		/// <summary>
		/// The left alt key is pressed.
		/// </summary>
		LAlt = 1 << 2,
		/// <summary>
		/// The left Windows key is pressed.
		/// </summary>
		LWin = 1 << 3,
		/// <summary>
		/// The right control key is pressed.
		/// </summary>
		RCtrl = 1 << 4,
		/// <summary>
		/// The right shift key is pressed.
		/// </summary>
		RShift = 1 << 5,
		/// <summary>
		/// The right alt key is pressed.
		/// </summary>
		RAlt = 1 << 6,
		/// <summary>
		/// The right Windows key is pressed.
		/// </summary>
		RWin = 1 << 7,
		/// <summary>
		/// The Num Lock key is active.
		/// </summary>
		NumLock = 1 << 8,
		/// <summary>
		/// The Caps Lock key is active.
		/// </summary>
		CapsLock = 1 << 9,
		/// <summary>
		/// The Scroll Lock key is active.
		/// </summary>
		ScrollLock = 1 << 10,

		/// <summary>
		/// A control key is being pressed.
		/// </summary>
		Ctrl = (LCtrl | RCtrl),
		/// <summary>
		/// A shift key is being pressed.
		/// </summary>
		Shift = (LShift | RShift),
		/// <summary>
		/// An alt key is being pressed.
		/// </summary>
		Alt = (LAlt | RAlt),
		/// <summary>
		/// A Windows key is being pressed.
		/// </summary>
		Win = (LWin | RWin),
		/// <summary>
		/// A modifier key is pressed (control, shift, alt or Windows keys).
		/// </summary>
		Modifiers = (Ctrl | Shift | Alt | Win),
		/// <summary>
		/// A lock key is active (Caps Lock, Num Lock or Scroll Lock).
		/// </summary>
		LockKeys = (CapsLock | NumLock | ScrollLock)
	}
}
