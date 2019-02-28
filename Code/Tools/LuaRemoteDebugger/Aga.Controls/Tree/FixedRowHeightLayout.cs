// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;

namespace Aga.Controls.Tree
{
	internal class FixedRowHeightLayout : IRowLayout
	{
		private TreeViewAdv _treeView;

		public FixedRowHeightLayout(TreeViewAdv treeView, int rowHeight)
		{
			_treeView = treeView;
			PreferredRowHeight = rowHeight;
		}

		private int _rowHeight;
		public int PreferredRowHeight
		{
			get { return _rowHeight; }
			set { _rowHeight = value; }
		}

		public Rectangle GetRowBounds(int rowNo)
		{
			return new Rectangle(0, rowNo * _rowHeight, 0, _rowHeight);
		}

		public int PageRowCount
		{
			get
			{
				return Math.Max((_treeView.DisplayRectangle.Height - _treeView.ColumnHeaderHeight) / _rowHeight, 0);
			}
		}

		public int CurrentPageSize
		{
			get
			{
				return PageRowCount;
			}
		}

		public int GetRowAt(Point point)
		{
			point = new Point(point.X, point.Y + (_treeView.FirstVisibleRow * _rowHeight) - _treeView.ColumnHeaderHeight);
			return point.Y / _rowHeight;
		}

		public int GetFirstRow(int lastPageRow)
		{
			return Math.Max(0, lastPageRow - PageRowCount + 1);
		}

		public void ClearCache()
		{
		}
	}
}
