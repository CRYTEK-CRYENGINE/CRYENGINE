// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.ComponentModel.Design;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing.Design;

namespace Aga.Controls.Tree.NodeControls
{
	internal class NodeControlsCollection : Collection<NodeControl>
	{
		private TreeViewAdv _tree;

		public NodeControlsCollection(TreeViewAdv tree)
		{
			_tree = tree;
		}

		protected override void ClearItems()
		{
			_tree.BeginUpdate();
			try
			{
				while (this.Count != 0)
					this.RemoveAt(this.Count - 1);
			}
			finally
			{
				_tree.EndUpdate();
			}
		}

		protected override void InsertItem(int index, NodeControl item)
		{
			if (item == null)
				throw new ArgumentNullException("item");

			if (item.Parent != _tree)
			{
				if (item.Parent != null)
				{
					item.Parent.NodeControls.Remove(item);
				}
				base.InsertItem(index, item);
				item.AssignParent(_tree);
				_tree.FullUpdate();
			}
		}

		protected override void RemoveItem(int index)
		{
			NodeControl value = this[index];
			value.AssignParent(null);
			base.RemoveItem(index);
			_tree.FullUpdate();
		}

		protected override void SetItem(int index, NodeControl item)
		{
			if (item == null)
				throw new ArgumentNullException("item");

			_tree.BeginUpdate();
			try
			{
				RemoveAt(index);
				InsertItem(index, item);
			}
			finally
			{
				_tree.EndUpdate();
			}
		}
	}

	internal class NodeControlCollectionEditor : CollectionEditor
	{
		private Type[] _types;

		public NodeControlCollectionEditor(Type type)
			: base(type)
		{
			_types = new Type[] { typeof(NodeTextBox), typeof(NodeIntegerTextBox), typeof(NodeDecimalTextBox), 
				typeof(NodeComboBox), typeof(NodeCheckBox),
				typeof(NodeStateIcon), typeof(NodeIcon), typeof(NodeNumericUpDown), typeof(ExpandingIcon)  };
		}

		protected override System.Type[] CreateNewItemTypes()
		{
			return _types;
		}
	}
}
