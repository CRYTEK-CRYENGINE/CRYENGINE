// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace Aga.Controls.Tree.NodeControls
{
	public class NodeControlValueEventArgs : NodeEventArgs
	{
		private object _value;
		public object Value
		{
			get { return _value; }
			set { _value = value; }
		}

		public NodeControlValueEventArgs(TreeNodeAdv node)
			:base(node)
		{
		}
	}
}
