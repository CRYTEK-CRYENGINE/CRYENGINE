// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Drawing;

namespace Aga.Controls.Tree
{
	public class TreeViewRowDrawEventArgs: PaintEventArgs
	{
		TreeNodeAdv _node;
		DrawContext _context;
		int _row;
		Rectangle _rowRect;

		public TreeViewRowDrawEventArgs(Graphics graphics, Rectangle clipRectangle, TreeNodeAdv node, DrawContext context, int row, Rectangle rowRect)
			: base(graphics, clipRectangle)
		{
			_node = node;
			_context = context;
			_row = row;
			_rowRect = rowRect;
		}

		public TreeNodeAdv Node
		{
			get { return _node; }
		}

		public DrawContext Context
		{
			get { return _context; }
		}

		public int Row
		{
			get { return _row; }
		}

		public Rectangle RowRect
		{
			get { return _rowRect; }
		}
	}

}
