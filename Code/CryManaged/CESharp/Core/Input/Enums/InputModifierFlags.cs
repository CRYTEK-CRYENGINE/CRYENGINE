using System;

namespace CryEngine
{
	//Wraps the native EModifierMask

	[Flags]
	public enum InputModifierFlags
	{
		None       = 0,
		LCtrl      = (1 << 0),
		LShift     = (1 << 1),
		LAlt       = (1 << 2),
		LWin       = (1 << 3),
		RCtrl      = (1 << 4),
		RShift     = (1 << 5),
		RAlt       = (1 << 6),
		RWin       = (1 << 7),
		NumLock    = (1 << 8),
		CapsLock   = (1 << 9),
		ScrollLock = (1 << 10),

		Ctrl       = (LCtrl | RCtrl),
		Shift      = (LShift | RShift),
		Alt        = (LAlt | RAlt),
		Win        = (LWin | RWin),
		Modifiers  = (Ctrl | Shift | Alt | Win),
		LockKeys   = (CapsLock | NumLock | ScrollLock)
	}
}
