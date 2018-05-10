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
	partial class StdThresholdZoneHighlighterControl : DesignableBaseSTZIIC
	{
		public StdThresholdZoneHighlighterControl(LogControl logControl, ZoneHighlighterRDI zrdiTree, RDICheckboxTreeView<ZoneHighlighterRDI> zrdiCTV)
			: base(logControl, zrdiTree, zrdiCTV)
		{
			InitializeComponent();
			ItemPathLabel = itemPathLabel;
		}

		protected override StdThresholdZRDI ZRDI
		{
			set
			{
				base.ZRDI = value;

				SetPathText();

				ZHIResetEnabled = false;

				nameTextBox.Text = ZRDI.Name;
				nameTextBox.Enabled = !ZRDI.NameFromPath;
				nameFromPathCheckBox.Checked = ZRDI.NameFromPath;
				itemColourButton.BackColor = ZRDI.Colour.ConvertToSysDrawCol();
				minValueNumericUpDown.Value = Convert.ToDecimal(ZRDI.MinThreshold);
				maxValueNumericUpDown.Value = Convert.ToDecimal(ZRDI.MaxThreshold);
				fpsValuesCheckBox.Checked = ZRDI.ValuesAreFPS;
				trackedPathTextBox.Text = ZRDI.TrackedPath;

				ZHIResetEnabled = true;
			}
		}

		protected override TextBox NameTextBox
		{
			get { return nameTextBox; }
		}

		protected override void SetNameText()
		{
			nameTextBox.Text = ZRDI.NameFromPath ? NameFromPath : ZRDI.Name;
			nameTextBox.Enabled = !ZRDI.NameFromPath;
			SetNameTextBoxBackColor();
		}

		string NameFromPath
		{
			get { return trackedPathTextBox.Text.Name(); }
		}

		protected override void SetItemColour(RGB colour)
		{
			base.SetItemColour(colour);
			itemColourButton.BackColor = colour.ConvertToSysDrawCol();
			ResetZHIs();
		}

		private void itemColourButton_Click(object sender, EventArgs e)
		{
			ItemColourButton_Click();
		}

		private void itemColourRandomButton_Click(object sender, EventArgs e)
		{
			ItemColourRandomButton_Click();
		}

		private void minValueNumericUpDown_ValueChanged(object sender, EventArgs e)
		{
			ZRDI.MinThreshold = (float)minValueNumericUpDown.Value;
			ResetZHIs();
		}

		private void maxValueNumericUpDown_ValueChanged(object sender, EventArgs e)
		{
			ZRDI.MaxThreshold = (float)maxValueNumericUpDown.Value;
			ResetZHIs();
		}

		private void nameTextBox_TextChanged(object sender, EventArgs e)
		{
			NameTextBox_TextChanged();
		}

		private void trackedPathTextBox_TextChanged(object sender, EventArgs e)
		{
			ZRDI.TrackedPath = trackedPathTextBox.Text;
			if (nameFromPathCheckBox.Checked)
				nameTextBox.Text = NameFromPath;
			ResetZHIs();
		}

		private void fpsValuesCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			ZRDI.ValuesAreFPS = fpsValuesCheckBox.Checked;
			ResetZHIs();
		}

		private void nameFromPathCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			if (!ZRDI.NameFromPath && nameFromPathCheckBox.Checked)
				nameTextBox.Text = NameFromPath;
			ZRDI.NameFromPath = nameFromPathCheckBox.Checked;
			nameTextBox.Enabled = !nameFromPathCheckBox.Checked;
		}

		private void trackedPathTextBox_DragEnter(object sender, DragEventArgs e)
		{
			if (e.Data.GetDataPresent(DataFormats.Text))
				e.Effect = DragDropEffects.Copy;
		}

		private void trackedPathTextBox_DragDrop(object sender, DragEventArgs e)
		{
			if (e.Data.GetDataPresent(DataFormats.Text))
				trackedPathTextBox.Text = (string)e.Data.GetData(DataFormats.Text);
		}
	}

	class DesignableBaseSTZIIC : ZoneHighlighterControl<StdThresholdZRDI>
	{
		public DesignableBaseSTZIIC()
			: this(null, null, null)
		{
		}

		public DesignableBaseSTZIIC(LogControl logControl, ZoneHighlighterRDI zrdiTree, RDICheckboxTreeView<ZoneHighlighterRDI> zrdiCTV)
			: base(logControl, zrdiTree, zrdiCTV)
		{
		}
	}
}
