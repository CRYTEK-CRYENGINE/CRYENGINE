// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;

namespace Aga.Controls.Threading
{
	public sealed class WorkItem
	{
		private WaitCallback _callback;
		private object _state;
		private ExecutionContext _ctx;

		internal WorkItem(WaitCallback wc, object state, ExecutionContext ctx)
		{
			_callback = wc; 
			_state = state; 
			_ctx = ctx;
		}

		internal WaitCallback Callback
		{
			get
			{
				return _callback;
			}
		}

		internal object State
		{
			get
			{
				return _state;
			}
		}

		internal ExecutionContext Context
		{
			get
			{
				return _ctx;
			}
		}
	}
}
