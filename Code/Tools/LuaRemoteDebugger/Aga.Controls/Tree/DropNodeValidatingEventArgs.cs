// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Drawing;

namespace Aga.Controls.Tree
{
	public class DropNodeValidatingEventArgs: EventArgs
	{
		Point _point;
		TreeNodeAdv _node;

		public DropNodeValidatingEventArgs(Point point, TreeNodeAdv node)
		{
			_point = point;
			_node = node;
		}

		public Point Point
		{
			get { return _point; }
		}

		public TreeNodeAdv Node
		{
			get { return _node; }
			set { _node = value; }
		}
	}
}
