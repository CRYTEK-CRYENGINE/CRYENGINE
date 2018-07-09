// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;
using System.Windows.Forms;

namespace Statoscope
{
	class StatsListView : ListView
	{
		public StatsListView(List<LogView> logViews, string path)
		{
			Columns.Add(LogControl.CreateColumnHeader("", 92));
			Columns.Add(LogControl.CreateColumnHeader("#Frames", 55));
			Columns.Add(LogControl.CreateColumnHeader("Min", 55));
			Columns.Add(LogControl.CreateColumnHeader("Max", 55));
			Columns.Add(LogControl.CreateColumnHeader("Avg", 55));

			Columns[0].TextAlign = HorizontalAlignment.Left;

			View = View.Details;
			Dock = DockStyle.Fill;
			ShowItemToolTips = true;
			VirtualMode = true;
			VirtualListSize = logViews.Count;
			RetrieveVirtualItem += new RetrieveVirtualItemEventHandler(statsListView_RetrieveVirtualItem);
			CacheVirtualItems += new CacheVirtualItemsEventHandler(statsListView_CacheVirtualItems);
			StatoscopeForm.SetDoubleBuffered(this);

			m_logViews = logViews;
			m_path = path;
			m_lviCache = new List<ListViewItem>(logViews.Count);
			UpdateLviCache();
		}

		public void UpdateLviCache()
		{
			m_lviCache.Clear();

			for (int i = 0; i < m_logViews.Count; i++)
			{
				LogView logView = m_logViews[i];
				ListViewItem lvi = new ListViewItem(logView.m_name);
				lvi.ToolTipText = logView.m_name;

				if (logView.m_valueStats.ContainsKey(m_path))
				{
					string formatString = logView.m_baseLogData.ItemMetaData[m_path].m_type == EItemType.Int ? "{0:n0}" : "{0:n2}";
					ValueStats valueStats = logView.m_valueStats[m_path];
					lvi.SubItems.Add(string.Format("{0:n0}", valueStats.m_numFrames));
					lvi.SubItems.Add(string.Format(formatString, valueStats.m_numFrames == 0 ? 0 : valueStats.m_min));
					lvi.SubItems.Add(string.Format(formatString, valueStats.m_numFrames == 0 ? 0 : valueStats.m_max));
					lvi.SubItems.Add(string.Format(formatString, valueStats.m_avg));
				}
				else
				{
					lvi.SubItems.Add("-");
					lvi.SubItems.Add("-");
					lvi.SubItems.Add("-");
					lvi.SubItems.Add("-");
				}
				m_lviCache.Add(lvi);
			}
		}

		private void statsListView_RetrieveVirtualItem(object sender, RetrieveVirtualItemEventArgs e)
		{
			e.Item = m_lviCache[e.ItemIndex];
		}

		private void statsListView_CacheVirtualItems(object sender, CacheVirtualItemsEventArgs e)
		{
			UpdateLviCache();
		}

		List<LogView> m_logViews;
		string m_path;
		List<ListViewItem> m_lviCache;
	}
}