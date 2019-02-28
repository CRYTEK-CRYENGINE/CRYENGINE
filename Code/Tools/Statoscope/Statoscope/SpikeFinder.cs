// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace Statoscope
{
  public partial class SpikeFinder : UserControl
  {
    ListView m_listView;
		LogControl m_logControl;
    SpikeDataComparer m_spikeDataComparer;
		bool m_suppressFPTreeCheckUpdating = false;
		ContextMenuStrip m_pathCMS;

		public SpikeFinder(LogControl logControl)
    {
      InitializeComponent();

			m_logControl = logControl;

			m_pathCMS = new ContextMenuStrip();
			m_pathCMS.Items.Add("Copy path", null, m_pathCMS_CopyPathClicked);

      m_listView = new ListView();
      m_listView.Dock = DockStyle.Fill;
			m_listView.ContextMenuStrip = m_pathCMS;

			m_listView.View = View.Details;
			m_listView.HideSelection = false;
			m_listView.MultiSelect = false;
			m_listView.CheckBoxes = true;
      m_listView.Columns.Add("Function");
      m_listView.Columns.Add("Max");
      m_listView.Columns.Add("Total");
      m_listView.Columns.Add("# Spikes");
      m_listView.ColumnClick += new ColumnClickEventHandler(listViewColumnClickEvent);
			m_listView.MouseClick += new MouseEventHandler(m_listView_MouseClick);
			m_listView.MouseDoubleClick += new MouseEventHandler(m_listView_MouseDoubleClick);
			m_listView.SelectedIndexChanged += new EventHandler(m_listView_SelectedIndexChanged);
			m_listView.ItemCheck += new ItemCheckEventHandler(m_listView_ItemCheck);

      m_spikeDataComparer = new SpikeDataComparer();
      m_listView.ListViewItemSorter = m_spikeDataComparer;

      tableLayoutPanel3.Controls.Add(m_listView);
    }

		void m_pathCMS_CopyPathClicked(object sender, EventArgs e)
		{
			if (m_listView.SelectedItems.Count > 0)
			{
				ListViewItem selectedItem = m_listView.SelectedItems[0];
				Clipboard.SetText(selectedItem.Name);
			}
		}

		void m_listView_MouseClick(object sender, MouseEventArgs e)
		{
			if (StatoscopeForm.m_ctrlIsBeingPressed && (m_listView.SelectedItems.Count > 0))
			{
				ListViewItem selectedItem = m_listView.SelectedItems[0];
				TreeNode fpTreeNode = m_logControl.m_prdiTreeView.TreeNodeHierarchy[selectedItem.Name];
				m_logControl.m_prdiTreeView.IsolateSubTree(fpTreeNode);
				m_logControl.PRDIsInvalidated();
			}
		}

		public void SelectPrdi()
		{
			if (m_listView.SelectedItems.Count > 0)
			{
				ListViewItem selectedItem = m_listView.SelectedItems[0];
				TreeNode fpTreeNode = m_logControl.m_prdiTreeView.TreeNodeHierarchy[selectedItem.Name];
				m_logControl.SelectItemInfo(fpTreeNode.Tag);
			}
		}

		public void UpdateCheckState(string path)
		{
			m_suppressFPTreeCheckUpdating = true;

			path = "/" + path;

			foreach (ListViewItem lvi in m_listView.Items)
			{
				if (lvi.Name.StartsWith(path))
				{
					bool newChecked = m_logControl.m_prdiTreeView.TreeNodeHierarchy.IsChecked(lvi.Name);
					if (lvi.Checked != newChecked)
						lvi.Checked = newChecked;
				}
			}

			m_suppressFPTreeCheckUpdating = false;
		}

		void m_listView_ItemCheck(object sender, ItemCheckEventArgs e)
		{
			if (!m_suppressFPTreeCheckUpdating)
			{
				string path = m_listView.Items[e.Index].Name;
				TreeNode fpNode = m_logControl.m_prdiTreeView.TreeNodeHierarchy[path];
				bool check = (e.NewValue == CheckState.Checked);
				bool prdisInvalidated = false;

				using (m_logControl.m_prdiTreeView.CheckStateBlock())
				{
					if (check)
					{
						for (TreeNode tn = fpNode; tn != null; tn = tn.Parent)
						{
							if (!tn.Checked)
							{
								tn.Checked = true;
								prdisInvalidated = true;
							}
						}
					}
					else
					{
						if (fpNode.Checked)
						{
							fpNode.Checked = false;
							prdisInvalidated = true;
						}
					}
				}

				if (prdisInvalidated)
					m_logControl.PRDIsInvalidated();
			}
		}

		void m_listView_SelectedIndexChanged(object sender, EventArgs e)
		{
			SelectPrdi();
		}

		void m_listView_MouseDoubleClick(object sender, MouseEventArgs e)
		{
			if (m_listView.SelectedItems.Count > 0)
			{
				ListViewItem selectedItem = m_listView.SelectedItems[0];
				TreeNode fpTreeNode = m_logControl.m_prdiTreeView.TreeNodeHierarchy[selectedItem.Name];
				ProfilerRDI prdi = (ProfilerRDI)fpTreeNode.Tag;
				m_logControl.SelectTreeViewTabPage("functionProfileTabPage");
				m_logControl.m_prdiTreeView.SelectedRDINode = prdi;
			}
		}

    private void listViewColumnClickEvent(object sender, ColumnClickEventArgs e)
    {
      m_spikeDataComparer.field = e.Column;
      m_listView.Sort();
    }

    private void MinSpikeTextBox_Leave(object sender, EventArgs e)
    {
      float temp;
      if (float.TryParse(MinSpikeTextBox.Text, out temp))
      {
        MinSpikeTextBox.BackColor = System.Drawing.Color.Empty;
      }
      else
      {
        MinSpikeTextBox.BackColor = System.Drawing.Color.Pink;
      }
    }

    private void MaxSpikeTextBox_Leave(object sender, EventArgs e)
    {
      float temp;
      if (float.TryParse(MaxSpikeTextBox.Text, out temp))
      {
        MaxSpikeTextBox.BackColor = System.Drawing.Color.Empty;
      }
      else
      {
        MaxSpikeTextBox.BackColor = System.Drawing.Color.Pink;
      }
    }

    class SpikeData
    {
			public string Path;
      public int numHits;
      public float totalTime;
      public float maxTime;

      public SpikeData(string path, float ftime)
      {
				this.Path = path;
        this.numHits = 1;
        this.totalTime = ftime;
        this.maxTime = ftime;
      }
    };

    class SpikeDataComparer : IComparer
    {
      public int field;

      public SpikeDataComparer()
      {
        field = 2;
      }

      public int Compare(object a, object b)
      {
        SpikeData ia = ((ListViewItem)a).Tag as SpikeData;
        SpikeData ib = ((ListViewItem)b).Tag as SpikeData;

				switch (field)
				{
					case 3:
						return (ia.numHits > ib.numHits) ? -1 : 1;
					case 2:
						return (ia.totalTime > ib.totalTime) ? -1 : 1;
					case 1:
						return (ia.maxTime > ib.maxTime) ? -1 : 1;
					case 0:
						return string.Compare(ia.Path, ib.Path);
				}
        return 1;
      }
    };

    private void RefreshButton_Click(object sender, EventArgs e)
    {
      float min, max;

      if (float.TryParse(MinSpikeTextBox.Text, out min) && float.TryParse(MaxSpikeTextBox.Text, out max))
      {
        m_listView.Items.Clear();

        Dictionary<string, SpikeData> dct = new Dictionary<string, SpikeData>();

				foreach (LogView logView in m_logControl.m_logViews)
				{
					foreach (FrameRecord fr in logView.m_logData.FrameRecords)
					{
						foreach (RDIElementValue<ProfilerRDI> prdiEv in m_logControl.m_prdiTree.GetValueEnumerator(fr))
						{
							float value = prdiEv.m_value;
							string path = prdiEv.m_rdi.Path;

							if (value > min && value < max)
							{
								if (!dct.ContainsKey(path))
								{
									dct[path] = new SpikeData(path, value);
								}
								else
								{
									dct[path].numHits += 1;
									dct[path].totalTime += value;
									dct[path].maxTime = Math.Max(dct[path].maxTime, value);
								}
							}
						}
					}
				}

				m_listView.BeginUpdate();

        foreach (KeyValuePair<string, SpikeData> pair in dct)
        {
          ListViewItem lvi = new ListViewItem(pair.Key);
					lvi.Name = pair.Key;
          lvi.Tag = pair.Value;
					lvi.Checked = m_logControl.m_prdiTreeView.TreeNodeHierarchy.IsChecked(pair.Key);
          lvi.SubItems.Add(string.Format("{0:n2} ms", pair.Value.maxTime));
          lvi.SubItems.Add(string.Format("{0:n2} ms", pair.Value.totalTime));
          lvi.SubItems.Add(string.Format("{0:d}", pair.Value.numHits));

          m_listView.Items.Add(lvi);
        }

        m_listView.Columns[0].AutoResize(ColumnHeaderAutoResizeStyle.ColumnContent);
        m_listView.Columns[1].AutoResize(ColumnHeaderAutoResizeStyle.ColumnContent);
        m_listView.Columns[2].AutoResize(ColumnHeaderAutoResizeStyle.ColumnContent);
        m_listView.Columns[3].AutoResize(ColumnHeaderAutoResizeStyle.ColumnContent);

				m_listView.EndUpdate();
      }
    }

    private void SpikeTextBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.KeyCode == Keys.Enter)
      {
        RefreshButton_Click(sender, e);
      }
		}

		private void CheckTreeNodes(string path, bool value)
		{
			TreeNode node = m_logControl.m_prdiTreeView.TreeNodeHierarchy[path];
			node.Checked = value;
		}

		private void HighlightButton_Click(object sender, EventArgs e)
		{
			ListView.SelectedListViewItemCollection si = m_listView.SelectedItems;
			ProfilerRDI prdiTree = m_logControl.m_prdiTree;

			foreach (ProfilerRDI prdi in prdiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
			{
				prdi.Displayed = false;
				CheckTreeNodes(prdi.Path, false);
			}

			foreach (ListViewItem item in si)
			{
				if (prdiTree.ContainsPath(item.Text))
				{
					prdiTree[item.Text].Displayed = true;

					CheckTreeNodes(item.Text, true);
				}
			}

			m_logControl.SetFrameLengths();
			m_logControl.m_prdiTreeView.Refresh();
			m_logControl.InvalidateGraphControl();
		}
  }
}
