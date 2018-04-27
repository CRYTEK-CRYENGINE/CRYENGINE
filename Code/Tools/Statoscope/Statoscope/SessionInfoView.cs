// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;
using System.Windows.Forms;

namespace Statoscope
{
	class SessionInfoListView : ListView
	{
		SessionInfo Session;
		List<ListViewItem> LviCache;

		public SessionInfoListView(SessionInfo sessionInfo)
		{
			Session = sessionInfo;

			Columns.Add(LogControl.CreateColumnHeader("", 70));
			Columns.Add(LogControl.CreateColumnHeader("", 90));

			Columns[0].TextAlign = HorizontalAlignment.Left;

			View = View.Details;
			Dock = DockStyle.Fill;
			ShowItemToolTips = true;
			VirtualMode = true;
			VirtualListSize = Session.Items.GetUpperBound(0) + 1;
			RetrieveVirtualItem += new RetrieveVirtualItemEventHandler(SessionInfoListView_RetrieveVirtualItem);
			CacheVirtualItems += new CacheVirtualItemsEventHandler(SessionInfoListView_CacheVirtualItems);
			StatoscopeForm.SetDoubleBuffered(this);

			LviCache = new List<ListViewItem>(VirtualListSize);
			UpdateLviCache();
		}

		public void UpdateLviCache()
		{
			LviCache.Clear();
			string[,] Items = Session.Items;

			for (int i = 0; i < VirtualListSize; i++)
			{
				ListViewItem lvi = new ListViewItem(Items[i, 0]);
				lvi.ToolTipText = Items[i, 1];
				lvi.SubItems.Add(Items[i, 1]);
				LviCache.Add(lvi);
			}
		}

		private void SessionInfoListView_RetrieveVirtualItem(object sender, RetrieveVirtualItemEventArgs e)
		{
			e.Item = LviCache[e.ItemIndex];
		}

		private void SessionInfoListView_CacheVirtualItems(object sender, CacheVirtualItemsEventArgs e)
		{
			UpdateLviCache();
		}
	}
}