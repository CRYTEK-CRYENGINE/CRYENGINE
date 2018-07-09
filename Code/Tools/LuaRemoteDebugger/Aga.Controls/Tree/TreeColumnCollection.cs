// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows.Forms;

namespace Aga.Controls.Tree
{
	internal class TreeColumnCollection : Collection<TreeColumn>
	{
		private TreeViewAdv _treeView;

		public TreeColumnCollection(TreeViewAdv treeView)
		{
			_treeView = treeView;
		}

		protected override void InsertItem(int index, TreeColumn item)
		{
			base.InsertItem(index, item);
			BindEvents(item);
			_treeView.UpdateColumns();
		}

		protected override void RemoveItem(int index)
		{
			UnbindEvents(this[index]);
			base.RemoveItem(index);
			_treeView.UpdateColumns();
		}

		protected override void SetItem(int index, TreeColumn item)
		{
			UnbindEvents(this[index]);
			base.SetItem(index, item);
			item.Owner = this;
			BindEvents(item);
			_treeView.UpdateColumns();
		}

		protected override void ClearItems()
		{
			foreach (TreeColumn c in Items)
				UnbindEvents(c);
			Items.Clear();
			_treeView.UpdateColumns();
		}

		private void BindEvents(TreeColumn item)
		{
			item.Owner = this;
			item.HeaderChanged += HeaderChanged;
			item.IsVisibleChanged += IsVisibleChanged;
			item.WidthChanged += WidthChanged;
			item.SortOrderChanged += SortOrderChanged;
		}

		private void UnbindEvents(TreeColumn item)
		{
			item.Owner = null;
			item.HeaderChanged -= HeaderChanged;
			item.IsVisibleChanged -= IsVisibleChanged;
			item.WidthChanged -= WidthChanged;
			item.SortOrderChanged -= SortOrderChanged;
		}

		void SortOrderChanged(object sender, EventArgs e)
		{
			TreeColumn changed = sender as TreeColumn;
			//Only one column at a time can have a sort property set
			if (changed.SortOrder != SortOrder.None)
			{
				foreach (TreeColumn col in this)
				{
					if (col != changed)
						col.SortOrder = SortOrder.None;
				}
			}
			_treeView.UpdateHeaders();
		}

		void WidthChanged(object sender, EventArgs e)
		{
			_treeView.ChangeColumnWidth(sender as TreeColumn);
		}

		void IsVisibleChanged(object sender, EventArgs e)
		{
			_treeView.FullUpdate();
		}

		void HeaderChanged(object sender, EventArgs e)
		{
			_treeView.UpdateView();
		}
	}
}
