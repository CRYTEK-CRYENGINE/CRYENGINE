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
	partial class TargetLineItemInfoControl : DesignableBaseTIIC
	{
		public TargetLineItemInfoControl(LogControl logControl, TargetLineRDI trdiTree, RDICheckboxTreeView<TargetLineRDI> trdiCTV)
			: base(logControl, trdiTree, trdiCTV)
		{
			InitializeComponent();
			ItemPathLabel = itemPathLabel;
		}

		protected override TextBox NameTextBox
		{
			get { return nameTextBox; }
		}

		protected override TargetLineRDI RDI
		{
			set
			{
				base.RDI = value;

				SetPathText();

				if (RDI.CanHaveChildren)
				{
					itemColourButton.Hide();
					itemColourRandomButton.Hide();
					itemColourGroupBox.Enabled = false;

					SetNameText();
					nameFromValueCheckBox.Hide();

					valueGroupBox.Enabled = false;
					valueTextBox.Hide();
					fpsValuesCheckBox.Hide();

					leftLabelSideRadioButton.Hide();
					rightLabelSideRadioButton.Hide();
					labelLocationGroupBox.Enabled = false;

					return;
				}

				itemColourButton.BackColor = RDI.Colour.ConvertToSysDrawCol();
				itemColourGroupBox.Enabled = true;
				itemColourButton.Show();
				itemColourRandomButton.Show();

				SetValueText();
				SetNameText();

				nameFromValueCheckBox.Checked = RDI.NameFromValue;
				nameFromValueCheckBox.Show();

				valueGroupBox.Enabled = true;
				valueTextBox.Show();
				fpsValuesCheckBox.Checked = RDI.ValueIsFPS;
				fpsValuesCheckBox.Show();

				leftLabelSideRadioButton.Show();
				leftLabelSideRadioButton.Checked = (RDI.LabelLocation == TargetLineRDI.ELabelLocation.ELL_Left);

				rightLabelSideRadioButton.Show();
				rightLabelSideRadioButton.Checked = (RDI.LabelLocation == TargetLineRDI.ELabelLocation.ELL_Right);

				labelLocationGroupBox.Enabled = true;
			}
		}

		protected override void SetNameText()
		{
			nameTextBox.Text = RDI.NameFromValue ? valueTextBox.Text : RDI.Name;
			nameTextBox.Enabled = !RDI.NameFromValue;
			SetNameTextBoxBackColor();
		}

		void SetValueText()
		{
			valueTextBox.Text = RDI.Value.ToString();
			valueTextBox.BackColor = SystemColors.Control;
		}

		protected override void SetItemColour(RGB colour)
		{
			base.SetItemColour(colour);
			itemColourButton.BackColor = colour.ConvertToSysDrawCol();
		}

		private void itemColourButton_Click(object sender, EventArgs e)
		{
			ItemColourButton_Click();
		}

		private void itemColourRandomButton_Click(object sender, EventArgs e)
		{
			ItemColourRandomButton_Click();
		}

		private void leftLabelSideRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			UpdateLabelLocation();
		}

		private void rightLabelSideRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			UpdateLabelLocation();
		}

		void UpdateLabelLocation()
		{
			RDI.LabelLocation = leftLabelSideRadioButton.Checked ?
				TargetLineRDI.ELabelLocation.ELL_Left :
				TargetLineRDI.ELabelLocation.ELL_Right;
			LogControl.InvalidateGraphControl();
		}

		private void nameTextBox_TextChanged(object sender, EventArgs e)
		{
			NameTextBox_TextChanged();
		}

		private void nameTextBox_Leave(object sender, EventArgs e)
		{
			NameTextBox_Leave();
		}

		private void valueTextBox_TextChanged(object sender, EventArgs e)
		{
			if (float.TryParse(valueTextBox.Text, out RDI.Value))
				valueTextBox.BackColor = SystemColors.Control;
			else
				valueTextBox.BackColor = Color.Red;

			if (RDI.NameFromValue)
				nameTextBox.Text = valueTextBox.Text;

			LogControl.InvalidateGraphControl();
		}

		private void nameFromValueCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			RDI.NameFromValue = nameFromValueCheckBox.Checked;
			if (RDI.NameFromValue)
				nameTextBox.Text = valueTextBox.Text;
			nameTextBox.Enabled = !nameFromValueCheckBox.Checked;
		}

		private void fpsValuesCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			RDI.ValueIsFPS = fpsValuesCheckBox.Checked;
			LogControl.InvalidateGraphControl();
		}
	}

	class DesignableBaseTIIC : RDIInfoControl<TargetLineRDI>
	{
		public DesignableBaseTIIC()
			: this(null, null, null)
		{
		}

		public DesignableBaseTIIC(LogControl logControl, TargetLineRDI trdiTree, RDICheckboxTreeView<TargetLineRDI> trdiCTV)
			: base(logControl, trdiTree, trdiCTV)
		{
		}
	}
}
