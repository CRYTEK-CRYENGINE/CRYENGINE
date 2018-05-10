// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace Aga.Controls.Tree
{
	internal class NormalInputState : InputState
	{
		private bool _mouseDownFlag = false;

		public NormalInputState(TreeViewAdv tree) : base(tree)
		{
		}

		public override void KeyDown(KeyEventArgs args)
		{
			if (Tree.CurrentNode == null && Tree.Root.Nodes.Count > 0)
				Tree.CurrentNode = Tree.Root.Nodes[0];

			if (Tree.CurrentNode != null)
			{
				switch (args.KeyCode)
				{
					case Keys.Right:
						if (!Tree.CurrentNode.IsExpanded)
							Tree.CurrentNode.IsExpanded = true;
						else if (Tree.CurrentNode.Nodes.Count > 0)
							Tree.SelectedNode = Tree.CurrentNode.Nodes[0];
						args.Handled = true;
						break;
					case Keys.Left:
						if (Tree.CurrentNode.IsExpanded)
							Tree.CurrentNode.IsExpanded = false;
						else if (Tree.CurrentNode.Parent != Tree.Root)
							Tree.SelectedNode = Tree.CurrentNode.Parent;
						args.Handled = true;
						break;
					case Keys.Down:
						NavigateForward(1);
						args.Handled = true;
						break;
					case Keys.Up:
						NavigateBackward(1);
						args.Handled = true;
						break;
					case Keys.PageDown:
						NavigateForward(Math.Max(1, Tree.CurrentPageSize - 1));
						args.Handled = true;
						break;
					case Keys.PageUp:
						NavigateBackward(Math.Max(1, Tree.CurrentPageSize - 1));
						args.Handled = true;
						break;
					case Keys.Home:
						if (Tree.RowMap.Count > 0)
							FocusRow(Tree.RowMap[0]);
						args.Handled = true;
						break;
					case Keys.End:
						if (Tree.RowMap.Count > 0)
							FocusRow(Tree.RowMap[Tree.RowMap.Count-1]);
						args.Handled = true;
						break;
					case Keys.Subtract:
						Tree.CurrentNode.Collapse();
						args.Handled = true;
						args.SuppressKeyPress = true;
						break;
					case Keys.Add:
						Tree.CurrentNode.Expand();
						args.Handled = true;
						args.SuppressKeyPress = true;
						break;
					case Keys.Multiply:
						Tree.CurrentNode.ExpandAll();
						args.Handled = true;
						args.SuppressKeyPress = true;
						break;
					case Keys.A:
						if (args.Modifiers == Keys.Control)
							Tree.SelectAllNodes();
						break;
				}
			}
		}

		public override void MouseDown(TreeNodeAdvMouseEventArgs args)
		{
			if (args.Node != null)
			{
				Tree.ItemDragMode = true;
				Tree.ItemDragStart = args.Location;

				if (args.Button == MouseButtons.Left || args.Button == MouseButtons.Right)
				{
					Tree.BeginUpdate();
					try
					{
						Tree.CurrentNode = args.Node;
						if (args.Node.IsSelected)
							_mouseDownFlag = true;
						else
						{
							_mouseDownFlag = false;
							DoMouseOperation(args);
						}
					}
					finally
					{
						Tree.EndUpdate();
					}
				}

			}
			else
			{
				Tree.ItemDragMode = false;
				MouseDownAtEmptySpace(args);
			}
		}

		public override void MouseUp(TreeNodeAdvMouseEventArgs args)
		{
			Tree.ItemDragMode = false;
			if (_mouseDownFlag && args.Node != null)
			{
				if (args.Button == MouseButtons.Left)
					DoMouseOperation(args);
				else if (args.Button == MouseButtons.Right)
					Tree.CurrentNode = args.Node;
			}
			_mouseDownFlag = false;
		}


		private void NavigateBackward(int n)
		{
			int row = Math.Max(Tree.CurrentNode.Row - n, 0);
			if (row != Tree.CurrentNode.Row)
				FocusRow(Tree.RowMap[row]);
		}

		private void NavigateForward(int n)
		{
			int row = Math.Min(Tree.CurrentNode.Row + n, Tree.RowCount - 1);
			if (row != Tree.CurrentNode.Row)
				FocusRow(Tree.RowMap[row]);
		}

		protected virtual void MouseDownAtEmptySpace(TreeNodeAdvMouseEventArgs args)
		{
			Tree.ClearSelection();
		}

		protected virtual void FocusRow(TreeNodeAdv node)
		{
			Tree.SuspendSelectionEvent = true;
			try
			{
				Tree.ClearSelectionInternal();
				Tree.CurrentNode = node;
				Tree.SelectionStart = node;
				node.IsSelected = true;
				Tree.ScrollTo(node);
			}
			finally
			{
				Tree.SuspendSelectionEvent = false;
			}
		}

		protected bool CanSelect(TreeNodeAdv node)
		{
			if (Tree.SelectionMode == TreeSelectionMode.MultiSameParent)
			{
				return (Tree.SelectionStart == null || node.Parent == Tree.SelectionStart.Parent);
			}
			else
				return true;
		}

		protected virtual void DoMouseOperation(TreeNodeAdvMouseEventArgs args)
		{
			if (Tree.SelectedNodes.Count == 1 && args.Node != null && args.Node.IsSelected)
				return;

			Tree.SuspendSelectionEvent = true;
			try
			{
				Tree.ClearSelectionInternal();
				if (args.Node != null)
					args.Node.IsSelected = true;
				Tree.SelectionStart = args.Node;
			}
			finally
			{
				Tree.SuspendSelectionEvent = false;
			}
		}
	}
}
