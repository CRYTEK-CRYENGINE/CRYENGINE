// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using Aga.Controls.Properties;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace Aga.Controls.Tree.NodeControls
{
	internal class NodePlusMinus : NodeControl
	{
		public const int ImageSize = 9;
		public const int Width = 16;
		private Bitmap _plus;
		private Bitmap _minus;

		private VisualStyleRenderer _openedRenderer;
		private VisualStyleRenderer OpenedRenderer
		{
			get
			{
				if (_openedRenderer == null)
					_openedRenderer = new VisualStyleRenderer(VisualStyleElement.TreeView.Glyph.Opened);
				return _openedRenderer;

			}
		}

		private VisualStyleRenderer _closedRenderer;
		private VisualStyleRenderer ClosedRenderer
		{
			get
			{
				if (_closedRenderer == null)
					_closedRenderer = new VisualStyleRenderer(VisualStyleElement.TreeView.Glyph.Closed);
				return _closedRenderer;
			}
		}

		public NodePlusMinus()
		{
			_plus = Resources.plus;
			_minus = Resources.minus;
		}

		public override Size MeasureSize(TreeNodeAdv node, DrawContext context)
		{
			return new Size(Width, Width);
		}

		public override void Draw(TreeNodeAdv node, DrawContext context)
		{
			if (node.CanExpand)
			{
				Rectangle r = context.Bounds;
				int dy = (int)Math.Round((float)(r.Height - ImageSize) / 2);
				if (Application.RenderWithVisualStyles)
				{
					VisualStyleRenderer renderer;
					if (node.IsExpanded)
						renderer = OpenedRenderer;
					else
						renderer = ClosedRenderer;
					renderer.DrawBackground(context.Graphics, new Rectangle(r.X, r.Y + dy, ImageSize, ImageSize));
				}
				else
				{
					Image img;
					if (node.IsExpanded)
						img = _minus;
					else
						img = _plus;
					context.Graphics.DrawImageUnscaled(img, new Point(r.X, r.Y + dy));
				}
			}
		}

		public override void MouseDown(TreeNodeAdvMouseEventArgs args)
		{
			if (args.Button == MouseButtons.Left)
			{
				args.Handled = true;
				if (args.Node.CanExpand)
					args.Node.IsExpanded = !args.Node.IsExpanded;
			}
		}

		public override void MouseDoubleClick(TreeNodeAdvMouseEventArgs args)
		{
			args.Handled = true; // Supress expand/collapse when double click on plus/minus
		}
	}
}
