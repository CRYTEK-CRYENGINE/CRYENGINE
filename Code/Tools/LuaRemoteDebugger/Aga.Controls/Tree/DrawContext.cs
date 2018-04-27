// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using Aga.Controls.Tree.NodeControls;

namespace Aga.Controls.Tree
{
	public struct DrawContext
	{
		private Graphics _graphics;
		public Graphics Graphics
		{
			get { return _graphics; }
			set { _graphics = value; }
		}

		private Rectangle _bounds;
		public Rectangle Bounds
		{
			get { return _bounds; }
			set { _bounds = value; }
		}

		private Font _font;
		public Font Font
		{
			get { return _font; }
			set { _font = value; }
		}

		private DrawSelectionMode _drawSelection;
		public DrawSelectionMode DrawSelection
		{
			get { return _drawSelection; }
			set { _drawSelection = value; }
		}

		private bool _drawFocus;
		public bool DrawFocus
		{
			get { return _drawFocus; }
			set { _drawFocus = value; }
		}

		private NodeControl _currentEditorOwner;
		public NodeControl CurrentEditorOwner
		{
			get { return _currentEditorOwner; }
			set { _currentEditorOwner = value; }
		}

		private bool _enabled;
		public bool Enabled
		{
			get { return _enabled; }
			set { _enabled = value; }
		}
	}
}
