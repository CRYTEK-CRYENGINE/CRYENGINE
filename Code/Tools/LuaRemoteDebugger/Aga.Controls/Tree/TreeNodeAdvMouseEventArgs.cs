// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using Aga.Controls.Tree.NodeControls;

namespace Aga.Controls.Tree
{
	public class TreeNodeAdvMouseEventArgs : MouseEventArgs
	{
		private TreeNodeAdv _node;
		public TreeNodeAdv Node
		{
			get { return _node; }
			internal set { _node = value; }
		}

		private NodeControl _control;
		public NodeControl Control
		{
			get { return _control; }
			internal set { _control = value; }
		}

		private Point _viewLocation;
		public Point ViewLocation
		{
			get { return _viewLocation; }
			internal set { _viewLocation = value; }
		}

		private Keys _modifierKeys;
		public Keys ModifierKeys
		{
			get { return _modifierKeys; }
			internal set { _modifierKeys = value; }
		}

		private bool _handled;
		public bool Handled
		{
			get { return _handled; }
			set { _handled = value; }
		}

		private Rectangle _controlBounds;
		public Rectangle ControlBounds
		{
			get { return _controlBounds; }
			internal set { _controlBounds = value; }
		}

		public TreeNodeAdvMouseEventArgs(MouseEventArgs args)
			: base(args.Button, args.Clicks, args.X, args.Y, args.Delta)
		{
		}
	}
}
