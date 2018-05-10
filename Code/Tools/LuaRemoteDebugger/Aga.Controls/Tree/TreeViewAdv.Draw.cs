// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Drawing;
using System.Diagnostics;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using Aga.Controls.Tree.NodeControls;

namespace Aga.Controls.Tree
{
	public partial class TreeViewAdv
	{
		public void AutoSizeColumn(TreeColumn column)
		{
			if (!Columns.Contains(column))
				throw new ArgumentException("column");

			DrawContext context = new DrawContext();
			context.Graphics = Graphics.FromImage(new Bitmap(1, 1));
			context.Font = this.Font;
			int res = 0;
			for (int row = 0; row < RowCount; row++)
			{
				if (row < RowMap.Count)
				{
					int w = 0;
					TreeNodeAdv node = RowMap[row];
					foreach (NodeControl nc in NodeControls)
					{
						if (nc.ParentColumn == column)
							w += nc.GetActualSize(node, _measureContext).Width;
					}
					res = Math.Max(res, w);
				}
			}

			if (res > 0)
				column.Width = res;
		}

		private void CreatePens()
		{
			CreateLinePen();
			CreateMarkPen();
		}

		private void CreateMarkPen()
		{
			GraphicsPath path = new GraphicsPath();
			path.AddLines(new Point[] { new Point(0, 0), new Point(1, 1), new Point(-1, 1), new Point(0, 0) });
			CustomLineCap cap = new CustomLineCap(null, path);
			cap.WidthScale = 1.0f;

			_markPen = new Pen(_dragDropMarkColor, _dragDropMarkWidth);
			_markPen.CustomStartCap = cap;
			_markPen.CustomEndCap = cap;
		}

		private void CreateLinePen()
		{
			_linePen = new Pen(_lineColor);
			_linePen.DashStyle = DashStyle.Dot;
		}

        protected override void OnPaint(PaintEventArgs e)
        {
            BeginPerformanceCount();
			PerformanceAnalyzer.Start("OnPaint");

            DrawContext context = new DrawContext();
            context.Graphics = e.Graphics;
            context.Font = this.Font;
            context.Enabled = Enabled;

            int y = 0;
            int gridHeight = 0;

            if (UseColumns)
            {
				DrawColumnHeaders(e.Graphics);
				y += ColumnHeaderHeight;
                if (Columns.Count == 0 || e.ClipRectangle.Height <= y)
                    return;
            }

			int firstRowY = _rowLayout.GetRowBounds(FirstVisibleRow).Y;
            y -= firstRowY;

            e.Graphics.ResetTransform();
            e.Graphics.TranslateTransform(-OffsetX, y);
            Rectangle displayRect = DisplayRectangle;
            for (int row = FirstVisibleRow; row < RowCount; row++)
            {
                Rectangle rowRect = _rowLayout.GetRowBounds(row);
                gridHeight += rowRect.Height;
                if (rowRect.Y + y > displayRect.Bottom)
                    break;
                else
                    DrawRow(e, ref context, row, rowRect);
            }

			if ((GridLineStyle & GridLineStyle.Vertical) == GridLineStyle.Vertical && UseColumns)
				DrawVerticalGridLines(e.Graphics, firstRowY);

			if (_dropPosition.Node != null && DragMode && HighlightDropPosition)
                DrawDropMark(e.Graphics);

            e.Graphics.ResetTransform();
            DrawScrollBarsBox(e.Graphics);

            if (DragMode && _dragBitmap != null)
                e.Graphics.DrawImage(_dragBitmap, PointToClient(MousePosition));

			PerformanceAnalyzer.Finish("OnPaint");
			EndPerformanceCount(e);
        }

		private void DrawRow(PaintEventArgs e, ref DrawContext context, int row, Rectangle rowRect)
		{
			TreeNodeAdv node = RowMap[row];
			context.DrawSelection = DrawSelectionMode.None;
			context.CurrentEditorOwner = CurrentEditorOwner;
			if (DragMode)
			{
				if ((_dropPosition.Node == node) && _dropPosition.Position == NodePosition.Inside && HighlightDropPosition)
					context.DrawSelection = DrawSelectionMode.Active;
			}
			else
			{
				if (node.IsSelected && Focused)
					context.DrawSelection = DrawSelectionMode.Active;
				else if (node.IsSelected && !Focused && !HideSelection)
					context.DrawSelection = DrawSelectionMode.Inactive;
			}
			context.DrawFocus = Focused && CurrentNode == node;
			
			OnRowDraw(e, node, context, row, rowRect);

			if (FullRowSelect)
			{
				context.DrawFocus = false;
				if (context.DrawSelection == DrawSelectionMode.Active || context.DrawSelection == DrawSelectionMode.Inactive)
				{
					Rectangle focusRect = new Rectangle(OffsetX, rowRect.Y, ClientRectangle.Width, rowRect.Height);
					if (context.DrawSelection == DrawSelectionMode.Active)
					{
						e.Graphics.FillRectangle(SystemBrushes.Highlight, focusRect);
						context.DrawSelection = DrawSelectionMode.FullRowSelect;
					}
					else
					{
						e.Graphics.FillRectangle(SystemBrushes.InactiveBorder, focusRect);
						context.DrawSelection = DrawSelectionMode.None;
					}
				}
			}

            if ((GridLineStyle & GridLineStyle.Horizontal) == GridLineStyle.Horizontal)
				e.Graphics.DrawLine(SystemPens.InactiveBorder, 0, rowRect.Bottom, e.Graphics.ClipBounds.Right, rowRect.Bottom);

			if (ShowLines)
				DrawLines(e.Graphics, node, rowRect);

			DrawNode(node, context);
		}

		private void DrawVerticalGridLines(Graphics gr, int y)
		{
			int x = 0;
			foreach (TreeColumn c in Columns)
			{
				if (c.IsVisible)
				{
					x += c.Width;
					gr.DrawLine(SystemPens.InactiveBorder, x - 1, y, x - 1, gr.ClipBounds.Bottom);
				}
			}
		}

		private void DrawColumnHeaders(Graphics gr)
		{
			PerformanceAnalyzer.Start("DrawColumnHeaders");
			ReorderColumnState reorder = Input as ReorderColumnState;
			int x = 0;
			TreeColumn.DrawBackground(gr, new Rectangle(0, 0, ClientRectangle.Width + 2, ColumnHeaderHeight - 1), false, false);
			gr.TranslateTransform(-OffsetX, 0);
			foreach (TreeColumn c in Columns)
			{
				if (c.IsVisible)
				{
					if (x >= OffsetX && x - OffsetX < this.Bounds.Width)// skip invisible columns
					{
						Rectangle rect = new Rectangle(x, 0, c.Width, ColumnHeaderHeight - 1);
						gr.SetClip(rect);
						bool pressed = ((Input is ClickColumnState || reorder != null) && ((Input as ColumnState).Column == c));
						c.Draw(gr, rect, Font, pressed, _hotColumn == c);
						gr.ResetClip();

						if (reorder != null && reorder.DropColumn == c)
							TreeColumn.DrawDropMark(gr, rect);
					}
					x += c.Width;
				}
			}

			if (reorder != null)
			{
				if (reorder.DropColumn == null)
					TreeColumn.DrawDropMark(gr, new Rectangle(x, 0, 0, ColumnHeaderHeight));
				gr.DrawImage(reorder.GhostImage, new Point(reorder.Location.X +  + reorder.DragOffset, reorder.Location.Y));
			}
			PerformanceAnalyzer.Finish("DrawColumnHeaders");
		}

		public void DrawNode(TreeNodeAdv node, DrawContext context)
		{
			foreach (NodeControlInfo item in GetNodeControls(node))
			{
				if (item.Bounds.Right >= OffsetX && item.Bounds.X - OffsetX < this.Bounds.Width)// skip invisible nodes
				{
					context.Bounds = item.Bounds;
					context.Graphics.SetClip(context.Bounds);
					item.Control.Draw(node, context);
					context.Graphics.ResetClip();
				}
			}
		}

		private void DrawScrollBarsBox(Graphics gr)
		{
			Rectangle r1 = DisplayRectangle;
			Rectangle r2 = ClientRectangle;
			gr.FillRectangle(SystemBrushes.Control,
				new Rectangle(r1.Right, r1.Bottom, r2.Width - r1.Width, r2.Height - r1.Height));
		}

		private void DrawDropMark(Graphics gr)
		{
			if (_dropPosition.Position == NodePosition.Inside)
				return;

			Rectangle rect = GetNodeBounds(_dropPosition.Node);
			int right = DisplayRectangle.Right - LeftMargin + OffsetX;
			int y = rect.Y;
			if (_dropPosition.Position == NodePosition.After)
				y = rect.Bottom;
			gr.DrawLine(_markPen, rect.X, y, right, y);
		}

		private void DrawLines(Graphics gr, TreeNodeAdv node, Rectangle rowRect)
		{
			if (UseColumns && Columns.Count > 0)
				gr.SetClip(new Rectangle(0, rowRect.Y, Columns[0].Width, rowRect.Bottom));

			TreeNodeAdv curNode = node;
			while (curNode != _root && curNode != null)
			{
				int level = curNode.Level;
				int x = (level - 1) * _indent + NodePlusMinus.ImageSize / 2 + LeftMargin;
				int width = NodePlusMinus.Width - NodePlusMinus.ImageSize / 2;
				int y = rowRect.Y;
				int y2 = y + rowRect.Height;

				if (curNode == node)
				{
					int midy = y + rowRect.Height / 2;
					gr.DrawLine(_linePen, x, midy, x + width, midy);
					if (curNode.NextNode == null)
						y2 = y + rowRect.Height / 2;
				}

				if (node.Row == 0)
					y = rowRect.Height / 2;
				if (curNode.NextNode != null || curNode == node)
					gr.DrawLine(_linePen, x, y, x, y2);

				curNode = curNode.Parent;
			}

			gr.ResetClip();
		}

		#region Performance

		private double _totalTime;
		private int _paintCount;

		[Conditional("PERF_TEST")]
		private void BeginPerformanceCount()
		{
			_paintCount++;
			TimeCounter.Start();
		}

		[Conditional("PERF_TEST")]
		private void EndPerformanceCount(PaintEventArgs e)
		{
			double time = TimeCounter.Finish();
			_totalTime += time;
			string debugText = string.Format("FPS {0:0.0}; Avg. FPS {1:0.0}",
				1 / time, 1 / (_totalTime / _paintCount));
			e.Graphics.FillRectangle(Brushes.White, new Rectangle(DisplayRectangle.Width - 150, DisplayRectangle.Height - 20, 150, 20));
			e.Graphics.DrawString(debugText, Control.DefaultFont, Brushes.Gray,
				new PointF(DisplayRectangle.Width - 150, DisplayRectangle.Height - 20));
		}
		#endregion

	}
}
