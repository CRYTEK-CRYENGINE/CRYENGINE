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
	public partial class LogListItem : UserControl
	{
		enum ETrackingState
		{
			Off,
			Smart,
			On,

			Num
		}

		LogView m_logView;
		bool m_bUpdateLogFRR = true;
		ETrackingState m_trackingState;
		ToolTip m_trackingButtonToolTip;

		public LogListItem(LogView logView)
		{
			InitializeComponent();
			m_logView = logView;
			logView.m_logListItem = this;

			ListViewItem nameLvi = new ListViewItem(m_logView.m_name);
			nameLvi.ToolTipText = m_logView.m_baseLogData.Name;
			nameLvi.Checked = true;
			nameListView.Items.Add(nameLvi);

			startFrameNumericUpDown.ValueChanged += new EventHandler(startFrameNumericUpDown_ValueChanged);
			endFrameNumericUpDown.ValueChanged += new EventHandler(endFrameNumericUpDown_ValueChanged);

			if (m_logView.m_logData is SocketLogData)
			{
				m_trackingButtonToolTip = new ToolTip();
				m_trackingState = ETrackingState.Smart;
				UpdateTracking();
			}
			else
			{
				trackingButton.Hide();
			}

			SetFrameNumericUpDowns();
			UpdateContents();

			Invalidated += new InvalidateEventHandler(LogListItem_Invalidated);
		}

		public void UpdateContents()
		{
			if (m_logView.m_singleOrdiColour != null)
			{
				colourButton.BackColor = m_logView.m_singleOrdiColour.ConvertToSysDrawCol();
				colourButton.Show();
				itemColourRandomButton.Show();
				trackingButton.Location = new Point(125, 0);
			}
			else
			{
				colourButton.Hide();
				itemColourRandomButton.Hide();
				trackingButton.Location = new Point(193, 0);
			}

			// this would be done better with some kind of flow/table container, next time!
			nameListView.Width = 116;
			if (!(m_logView.m_logData is SocketLogData))
				nameListView.Width += 26;
			if (m_logView.m_singleOrdiColour == null)
				nameListView.Width += 70;
		}

		void SetFrameNumericUpDowns()
		{
			m_bUpdateLogFRR = false;

			FrameRecordRange frr = m_logView.GetFrameRecordRange();
			int maxValue = Math.Max(frr.EndIdx, m_logView.m_baseLogData.FrameRecords.Count - 1);
			startFrameNumericUpDown.Maximum = maxValue;
			endFrameNumericUpDown.Maximum = maxValue;

			startFrameNumericUpDown.Value = frr.StartIdx;
			endFrameNumericUpDown.Value = frr.EndIdx;

			m_bUpdateLogFRR = true;
		}

		void LogListItem_Invalidated(object sender, InvalidateEventArgs e)
		{
			SetFrameNumericUpDowns();
		}

		private void nameListView_ItemChecked(object sender, ItemCheckedEventArgs e)
		{
			m_logView.m_displayedOnGraph = e.Item.Checked;
			m_logView.m_logControl.InvalidateGraphControl();
		}

		private void colourButton_Click(object sender, EventArgs e)
		{
			ColorDialog cd = new ColorDialog();
			if (cd.ShowDialog() == DialogResult.OK)
				SetColour(new RGB(cd.Color));
		}

		private void itemColourRandomButton_Click(object sender, EventArgs e)
		{
			SetColour(RGB.RandomHueRGB());
		}

		void SetColour(RGB colour)
		{
			m_logView.SetSingleOrdiColour(colour);
			m_logView.m_logControl.SetUMTreeViewNodeColours();
			colourButton.BackColor = colour.ConvertToSysDrawCol();
		}

		private void nameListView_AfterLabelEdit(object sender, LabelEditEventArgs e)
		{
			if (e.Label != null)
			{
				if (e.Label.Length > 0)
				{
					m_logView.m_name = e.Label;
					m_logView.m_logControl.OnLogChanged();
				}
				else
				{
					e.CancelEdit = true;
				}
			}
		}

		private void nameListView_KeyDown(object sender, KeyEventArgs e)
		{
			if (e.KeyData == Keys.F2 && nameListView.SelectedItems.Count == 1)
				nameListView.SelectedItems[0].BeginEdit();
		}

		private void startFrameNumericUpDown_ValueChanged(object sender, EventArgs e)
		{
			if (startFrameNumericUpDown.Value > endFrameNumericUpDown.Value)
			{
				m_bUpdateLogFRR = false;
				endFrameNumericUpDown.Value = startFrameNumericUpDown.Value;
				m_bUpdateLogFRR = true;
			}

			if (m_bUpdateLogFRR)
				m_logView.SetFrameRecordRange((int)startFrameNumericUpDown.Value, (int)endFrameNumericUpDown.Value);

			m_logView.m_startUM = null;
		}

		private void endFrameNumericUpDown_ValueChanged(object sender, EventArgs e)
		{
			if (endFrameNumericUpDown.Value < startFrameNumericUpDown.Value)
			{
				m_bUpdateLogFRR = false;
				startFrameNumericUpDown.Value = endFrameNumericUpDown.Value;
				m_bUpdateLogFRR = true;
			}

			if (m_bUpdateLogFRR)
				m_logView.SetFrameRecordRange((int)startFrameNumericUpDown.Value, (int)endFrameNumericUpDown.Value);

			m_logView.m_endUM = null;
		}

		void SetTrackingState(ETrackingState ts)
		{
			m_trackingState = ts;

			if (!trackingButton.Visible)
				return;

			switch (m_trackingState)
			{
				case ETrackingState.Off:
					trackingButton.BackColor = SystemColors.Control;
					trackingButton.ForeColor = SystemColors.ControlText;
					SetTrackingButtonToolTip("off");
					break;
				case ETrackingState.Smart:
					trackingButton.BackColor = Color.CornflowerBlue;
					trackingButton.ForeColor = Color.White;
					SetTrackingButtonToolTip("smart");
					break;
				case ETrackingState.On:
					trackingButton.BackColor = Color.LightGreen;
					trackingButton.ForeColor = SystemColors.ControlText;
					SetTrackingButtonToolTip("on");
					break;
			}
		}

		void IncrementTrackingState()
		{
			m_trackingState += 1;
			if (m_trackingState >= ETrackingState.Num)
				m_trackingState = ETrackingState.Off;
		}

		void UpdateTracking()
		{
			foreach (LogView logView in m_logView.m_logControl.m_logViews)
				logView.m_logListItem.SetTrackingState(logView == m_logView ? m_trackingState : ETrackingState.Off);

			m_logView.m_logControl.SetTracking(
				m_trackingState != ETrackingState.Off ? m_logView : null,
				m_trackingState == ETrackingState.Smart);
		}

		private void trackingButton_Click(object sender, EventArgs e)
		{
			IncrementTrackingState();
			UpdateTracking();
		}

		void SetTrackingButtonToolTip(string str)
		{
			m_trackingButtonToolTip.SetToolTip(trackingButton, "End frame tracking: " + str);
		}
	}
}
