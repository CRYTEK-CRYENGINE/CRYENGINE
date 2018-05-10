// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine
{
	/// <summary>
	/// Template extension for System.EventArgs.
	/// </summary>
	public class EventArgs<T1> : EventArgs
	{
		private T1 _item1;

		public T1 Item1 { get { return _item1; } }

		public EventArgs(T1 item1)
		{
			_item1 = item1;
		}
	}
	public class EventArgs<T1, T2> : EventArgs<T1>
	{
		private T2 _item2;

		public T2 Item2 { get { return _item2; } }

		public EventArgs(T1 item1, T2 item2)
			: base(item1)
		{
			_item2 = item2;
		}
	}
	public class EventArgs<T1, T2, T3> : EventArgs<T1, T2>
	{
		private T3 _item3;

		public T3 Item3 { get { return _item3; } }

		public EventArgs(T1 item1, T2 item2, T3 item3)
			: base(item1, item2)
		{
			_item3 = item3;
		}
	}
	public class EventArgs<T1, T2, T3, T4> : EventArgs<T1, T2, T3>
	{
		private T4 _item4;

		public T4 Item4 { get { return _item4; } }

		public EventArgs(T1 item1, T2 item2, T3 item3, T4 item4)
			: base(item1, item2, item3)
		{
			_item4 = item4;
		}
	}
	public class EventArgs<T1, T2, T3, T4, T5> : EventArgs<T1, T2, T3, T4>
	{
		private T5 _item5;

		public T5 Item5 { get { return _item5; } }

		public EventArgs(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5)
			: base(item1, item2, item3, item4)
		{
			_item5 = item5;
		}
	}
	public class EventArgs<T1, T2, T3, T4, T5, T6> : EventArgs<T1, T2, T3, T4, T5>
	{
		private T6 _item6;

		public T6 Item6 { get { return _item6; } }

		public EventArgs(T1 item1, T2 item2, T3 item3, T4 item4, T5 item5, T6 item6)
			: base(item1, item2, item3, item4, item5)
		{
			_item6 = item6;
		}
	}
}
