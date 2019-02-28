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
	partial class OverviewItemInfoControl : DesignableBaseOIIC
	{
		public OverviewItemInfoControl(LogControl logControl, OverviewRDI ordiTree, RDICheckboxTreeView<OverviewRDI> ordiCTV)
		: base(logControl, ordiTree, ordiCTV)
		{
			InitializeComponent();
			ItemPathLabel = itemPathLabel;
		}

		protected override OverviewRDI RDI
		{
			set
			{
				base.RDI = value;

				itemPathLabel.Text = RDI.Path;
				ItemPathLabelToolTip.SetToolTip(itemPathLabel, RDI.Path);

				CustomDisplayScaleUpDown.Value = (decimal)LogControl.GetUserDisplayScale(itemPathLabel.Text);

				filteringGroupBox.Show();

				if (LogControl.m_logViews.Count == 1)
				{
					itemColourGroupBox.Show();

					if (!RDI.IsLeaf)
					{
						itemColourButton.Hide();
						itemColourRandomButton.Hide();
						itemColourGroupBox.Enabled = false;

						frameOffsetGroupBox.Enabled = false;
						frameOffsetNumericUpDown.Hide();

						filteringPanel.Hide();
						filteringGroupBox.Enabled = false;

						itemStatsGroupBox.Controls.Clear();
						itemStatsGroupBox.Enabled = false;

						CustomDisplayScaleUpDown.Hide();
						CustomDisplayScalingBox.Enabled = false;

						return;
					}

					itemColourButton.BackColor = RDI.Colour.ConvertToSysDrawCol();
				}
				else
				{
					itemColourGroupBox.Hide();
					filteringGroupBox.Enabled = true;
					filteringPanel.Show();
				}


				if (!RDI.IsLeaf)
				{
					CustomDisplayScaleUpDown.Hide();
					CustomDisplayScalingBox.Enabled = false;

					itemColourButton.Hide();
					itemColourRandomButton.Hide();
					itemColourGroupBox.Enabled = false;

					frameOffsetGroupBox.Enabled = false;
					frameOffsetNumericUpDown.Hide();

					filteringPanel.Hide();
					filteringGroupBox.Enabled = false;

					itemStatsGroupBox.Controls.Clear();
					itemStatsGroupBox.Enabled = false;
					return;
				}

				CustomDisplayScaleUpDown.Show();
				CustomDisplayScalingBox.Enabled = true;

				itemColourGroupBox.Enabled = true;
				itemColourButton.Show();
				itemColourRandomButton.Show();

				itemStatsGroupBox.Hide();
				itemStatsGroupBox.Controls.Clear();
				itemStatsGroupBox.Controls.Add(new StatsListView(LogControl.m_logViews, RDI.Path));
				itemStatsGroupBox.Enabled = true;
				itemStatsGroupBox.Show();

				noneFilteringRadioButton.Checked = (RDI.Filtering == OverviewRDI.EFiltering.None);
				maFilteringRadioButton.Checked = (RDI.Filtering == OverviewRDI.EFiltering.MovingAverage);
				lmFilteringRadioButton.Checked = (RDI.Filtering == OverviewRDI.EFiltering.LocalMax);

				maUpDown.Enabled = (RDI.Filtering == OverviewRDI.EFiltering.MovingAverage);
				lmUpDown.Enabled = (RDI.Filtering == OverviewRDI.EFiltering.LocalMax);
				maUpDown.Value = RDI.MANumFrames;
				lmUpDown.Value = RDI.LMNumFrames;

				frameOffsetNumericUpDown.Value = RDI.FrameOffset;
				frameOffsetGroupBox.Enabled = true;
				frameOffsetNumericUpDown.Show();

				filteringGroupBox.Enabled = true;
				filteringPanel.Show();
			}
		}

		private void itemColourButton_Click(object sender, EventArgs e)
		{
			ItemColourButton_Click();
		}

		private void itemColourRandomButton_Click(object sender, EventArgs e)
		{
			ItemColourRandomButton_Click();
		}

		private void maUpDown_ValueChanged(object sender, EventArgs e)
		{
			RDI.MANumFrames = (int)maUpDown.Value;

			if (RDI.Filtering == OverviewRDI.EFiltering.MovingAverage)
			{
				foreach (LogView logView in LogControl.m_logViews)
				{
					logView.CalculateMovingAverage(RDI, 0);
					logView.DeleteDisplayLists(RDI.ValuesPath);
					LogControl.InvalidateGraphControl();
				}
			}
		}

		private void lmUpDown_ValueChanged(object sender, EventArgs e)
		{
			RDI.LMNumFrames = (int)lmUpDown.Value;

			if (RDI.Filtering == OverviewRDI.EFiltering.LocalMax)
			{
				foreach (LogView logView in LogControl.m_logViews)
				{
					logView.CalculateLocalMax(RDI, 0);
					logView.DeleteDisplayLists(RDI.ValuesPath);
					LogControl.InvalidateGraphControl();
				}
			}
		}

		private void noneFilteringRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			if (noneFilteringRadioButton.Checked)
			{
				RDI.Filtering = OverviewRDI.EFiltering.None;

				maUpDown.Enabled = false;
				lmUpDown.Enabled = false;

				LogControl.InvalidateGraphControl();
			}
		}

		private void maFilteringRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			if (maFilteringRadioButton.Checked && (RDI.Filtering != OverviewRDI.EFiltering.MovingAverage))
			{
				RDI.Filtering = OverviewRDI.EFiltering.MovingAverage;

				maUpDown.Enabled = true;
				lmUpDown.Enabled = false;

				foreach (LogView logView in LogControl.m_logViews)
				{
					logView.CalculateMovingAverage(RDI, 0);
				}

				LogControl.InvalidateGraphControl();
			}
		}

		private void lmFilteringRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			if (lmFilteringRadioButton.Checked && (RDI.Filtering != OverviewRDI.EFiltering.LocalMax))
			{
				RDI.Filtering = OverviewRDI.EFiltering.LocalMax;

				maUpDown.Enabled = false;
				lmUpDown.Enabled = true;

				foreach (LogView logView in LogControl.m_logViews)
				{
					logView.CalculateLocalMax(RDI, 0);
				}

				LogControl.InvalidateGraphControl();
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

		private void frameOffsetNumericUpDown_ValueChanged(object sender, EventArgs e)
		{
			RDI.FrameOffset = (int)frameOffsetNumericUpDown.Value;

			foreach (LogView logView in LogControl.m_logViews)
			{
				logView.DeleteDisplayLists(RDI.ValuesPath);
				LogControl.InvalidateGraphControl();
			}
		}

		private void CustomDisplayScaleUpDown_ValueChanged(object sender, EventArgs e)
		{
			float scale = (float)CustomDisplayScaleUpDown.Value;

			LogControl.SetDisplayScale(itemPathLabel.Text, scale);
			LogControl.UpdateDisplayScale(itemPathLabel.Text);

			foreach (LogView logView in LogControl.m_logViews)
			{
				logView.DeleteDisplayLists(RDI.ValuesPath);
				LogControl.InvalidateGraphControl();
			}
		}
	}

	class DesignableBaseOIIC : RDIInfoControl<OverviewRDI>
	{
		public DesignableBaseOIIC()
		: this(null, null, null)
		{
		}

		public DesignableBaseOIIC(LogControl logControl, OverviewRDI ordiTree, RDICheckboxTreeView<OverviewRDI> ordiCTV)
		: base(logControl, ordiTree, ordiCTV)
		{
		}
	}
}
