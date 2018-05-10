// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.ComponentModel;

namespace LuaRemoteDebuggerControls
{
	class DoubleClickListView : ListView
	{
		[Category("Action")]
		public event EventHandler<ItemDoubleClickEventArgs> ItemDoubleClicked;

		protected override void OnSelectedIndexChanged(EventArgs e)
		{
			base.OnSelectedIndexChanged(e);
			if (this.SelectedItems.Count == 1)
			{
				this.SelectedItems[0].EnsureVisible();
			}
		}

		protected override void WndProc(ref Message m)
		{
			// Filter WM_LBUTTONDBLCLK
			if (m.Msg != 0x203)
				base.WndProc(ref m);
			else
				OnDoubleClick();
		}

		private void OnDoubleClick()
		{
			if (this.SelectedItems.Count == 1)
			{
				ListViewItem item = this.SelectedItems[0];
				if (ItemDoubleClicked != null)
				{
					ItemDoubleClicked(this, new ItemDoubleClickEventArgs(item));
				}
			}
		}
	}

	public class ItemDoubleClickEventArgs : EventArgs
	{
		ListViewItem item;

		public ItemDoubleClickEventArgs(ListViewItem item) { this.item = item; }

		public ListViewItem Item { get { return item; } }
	}
}
