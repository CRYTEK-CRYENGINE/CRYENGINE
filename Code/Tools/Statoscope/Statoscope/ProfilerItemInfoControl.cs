// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace Statoscope
{
	partial class ProfilerItemInfoControl : DesignableBasePIIC
	{
		public ProfilerItemInfoControl(LogControl logControl, ProfilerRDI prdiTree, RDICheckboxTreeView<ProfilerRDI> prdiCTV)
			: base(logControl, prdiTree, prdiCTV)
		{
			InitializeComponent();
			ItemPathLabel = itemPathLabel;
		}

		protected override ProfilerRDI RDI
		{
			set
			{
				base.RDI = value;

				itemPathLabel.Text = RDI.Path;
				ItemPathLabelToolTip.SetToolTip(itemPathLabel, RDI.Path);

				itemColourGroupBox.Enabled = (RDI.IsLeaf || RDI.IsCollapsed);
				itemColourButton.BackColor = RDI.Colour.ConvertToSysDrawCol();
				itemColourGroupBox.Show();

				itemDescriptionLabel.Text = "Value: " + RDI.LeafName + Environment.NewLine;
				if (LogControl.m_logViews.Count > 1)
				{
					EItemType itemType = LogControl.m_logViews[0].m_baseLogData.ItemMetaData[RDI.LeafPath].m_type;
					bool itemTypeMatches = true;

					for (int i = 0; i < LogControl.m_logViews.Count; i++)
					{
						ItemMetaData itemMetaData;
						bool hasMetaData = LogControl.m_logViews[i].m_baseLogData.ItemMetaData.TryGetValue(RDI.LeafPath, out itemMetaData);
						if (hasMetaData && itemMetaData.m_type != itemType)
						{
							itemTypeMatches = false;
							break;
						}
					}

					itemDescriptionLabel.Text += "Type: " + (itemTypeMatches ? (itemType == EItemType.Float ? "Float" : "Int") : "Mixed");
				}
				else
				{
					ItemMetaData itemMetaData = LogControl.m_logViews[0].m_baseLogData.ItemMetaData[RDI.LeafPath];
					itemDescriptionLabel.Text += "Type: " + (itemMetaData.m_type == EItemType.Float ? "Float" : "Int");
				}

				itemStatsGroupBox.Hide();
				itemStatsGroupBox.Controls.Clear();
				itemStatsGroupBox.Controls.Add(new StatsListView(LogControl.m_logViews, RDI.LeafPath));
				itemStatsGroupBox.Enabled = true;
				itemStatsGroupBox.Show();
			}
		}

		protected override void SetItemColour(RGB colour)
		{
			base.SetItemColour(colour);
			itemColourButton.BackColor = colour.ConvertToSysDrawCol();
		}

		public override void OnLogChanged()
		{
			itemStatsGroupBox.Invalidate(true);
		}

		private void itemColourButton_Click(object sender, EventArgs e)
		{
			ItemColourButton_Click();
		}

		private void itemColourRandomButton_Click(object sender, EventArgs e)
		{
			ItemColourRandomButton_Click();
		}
	}

	class DesignableBasePIIC : RDIInfoControl<ProfilerRDI>
	{
		public DesignableBasePIIC()
			: this(null, null, null)
		{
		}

		public DesignableBasePIIC(LogControl logControl, ProfilerRDI prdiTree, RDICheckboxTreeView<ProfilerRDI> prdiCTV)
			: base(logControl, prdiTree, prdiCTV)
		{
		}
	}
}
