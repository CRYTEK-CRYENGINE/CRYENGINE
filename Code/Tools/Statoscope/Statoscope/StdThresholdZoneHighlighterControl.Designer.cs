// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class StdThresholdZoneHighlighterControl
	{
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary> 
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.colourGroupBox = new System.Windows.Forms.GroupBox();
			this.itemColourButton = new System.Windows.Forms.Button();
			this.colourRandomButton = new System.Windows.Forms.Button();
			this.trackedPathTextBox = new System.Windows.Forms.TextBox();
			this.minValueNumericUpDown = new NumericUpDownEx();
			this.minLabel = new System.Windows.Forms.Label();
			this.maxLabel = new System.Windows.Forms.Label();
			this.maxValueNumericUpDown = new NumericUpDownEx();
			this.rangeGroupBox = new System.Windows.Forms.GroupBox();
			this.fpsValuesCheckBox = new System.Windows.Forms.CheckBox();
			this.detailsGroupBox = new System.Windows.Forms.GroupBox();
			this.nameFromPathCheckBox = new System.Windows.Forms.CheckBox();
			this.nameTextBox = new System.Windows.Forms.TextBox();
			this.nameLabel = new System.Windows.Forms.Label();
			this.typeLabel2 = new System.Windows.Forms.Label();
			this.typeLabel1 = new System.Windows.Forms.Label();
			this.trackedPathGroupBox = new System.Windows.Forms.GroupBox();
			this.itemPathLabel = new System.Windows.Forms.Label();
			this.itemPathGroupBox = new System.Windows.Forms.GroupBox();
			this.colourGroupBox.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.minValueNumericUpDown)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.maxValueNumericUpDown)).BeginInit();
			this.rangeGroupBox.SuspendLayout();
			this.detailsGroupBox.SuspendLayout();
			this.trackedPathGroupBox.SuspendLayout();
			this.itemPathGroupBox.SuspendLayout();
			this.SuspendLayout();
			// 
			// colourGroupBox
			// 
			this.colourGroupBox.Controls.Add(this.itemColourButton);
			this.colourGroupBox.Controls.Add(this.colourRandomButton);
			this.colourGroupBox.Location = new System.Drawing.Point(252, 3);
			this.colourGroupBox.Name = "colourGroupBox";
			this.colourGroupBox.Size = new System.Drawing.Size(84, 52);
			this.colourGroupBox.TabIndex = 3;
			this.colourGroupBox.TabStop = false;
			this.colourGroupBox.Text = "Colour";
			// 
			// itemColourButton
			// 
			this.itemColourButton.BackColor = System.Drawing.Color.White;
			this.itemColourButton.Location = new System.Drawing.Point(9, 19);
			this.itemColourButton.Name = "itemColourButton";
			this.itemColourButton.Size = new System.Drawing.Size(23, 23);
			this.itemColourButton.TabIndex = 0;
			this.itemColourButton.UseVisualStyleBackColor = false;
			this.itemColourButton.Click += new System.EventHandler(this.itemColourButton_Click);
			// 
			// colourRandomButton
			// 
			this.colourRandomButton.Location = new System.Drawing.Point(38, 19);
			this.colourRandomButton.Name = "colourRandomButton";
			this.colourRandomButton.Size = new System.Drawing.Size(40, 23);
			this.colourRandomButton.TabIndex = 2;
			this.colourRandomButton.Text = "Rnd";
			this.colourRandomButton.UseVisualStyleBackColor = true;
			this.colourRandomButton.Click += new System.EventHandler(this.itemColourRandomButton_Click);
			// 
			// trackedPathTextBox
			// 
			this.trackedPathTextBox.AllowDrop = true;
			this.trackedPathTextBox.Location = new System.Drawing.Point(6, 19);
			this.trackedPathTextBox.Name = "trackedPathTextBox";
			this.trackedPathTextBox.Size = new System.Drawing.Size(321, 20);
			this.trackedPathTextBox.TabIndex = 4;
			this.trackedPathTextBox.TextChanged += new System.EventHandler(this.trackedPathTextBox_TextChanged);
			this.trackedPathTextBox.DragDrop += new System.Windows.Forms.DragEventHandler(this.trackedPathTextBox_DragDrop);
			this.trackedPathTextBox.DragEnter += new System.Windows.Forms.DragEventHandler(this.trackedPathTextBox_DragEnter);
			// 
			// minValueNumericUpDown
			// 
			this.minValueNumericUpDown.DecimalPlaces = 2;
			this.minValueNumericUpDown.Location = new System.Drawing.Point(66, 18);
			this.minValueNumericUpDown.Maximum = new decimal(new int[] {
            1410065408,
            2,
            0,
            0});
			this.minValueNumericUpDown.Minimum = new decimal(new int[] {
            1410065408,
            2,
            0,
            -2147483648});
			this.minValueNumericUpDown.Name = "minValueNumericUpDown";
			this.minValueNumericUpDown.Size = new System.Drawing.Size(90, 20);
			this.minValueNumericUpDown.TabIndex = 6;
			this.minValueNumericUpDown.ThousandsSeparator = true;
			this.minValueNumericUpDown.Value = new decimal(new int[] {
            10000000,
            0,
            0,
            0});
			this.minValueNumericUpDown.ValueChanged += new System.EventHandler(this.minValueNumericUpDown_ValueChanged);
			// 
			// minLabel
			// 
			this.minLabel.AutoSize = true;
			this.minLabel.Location = new System.Drawing.Point(6, 20);
			this.minLabel.Name = "minLabel";
			this.minLabel.Size = new System.Drawing.Size(54, 13);
			this.minLabel.TabIndex = 7;
			this.minLabel.Text = "Min Value";
			// 
			// maxLabel
			// 
			this.maxLabel.AutoSize = true;
			this.maxLabel.Location = new System.Drawing.Point(6, 46);
			this.maxLabel.Name = "maxLabel";
			this.maxLabel.Size = new System.Drawing.Size(57, 13);
			this.maxLabel.TabIndex = 9;
			this.maxLabel.Text = "Max Value";
			// 
			// maxValueNumericUpDown
			// 
			this.maxValueNumericUpDown.DecimalPlaces = 2;
			this.maxValueNumericUpDown.Location = new System.Drawing.Point(66, 44);
			this.maxValueNumericUpDown.Maximum = new decimal(new int[] {
            1410065408,
            2,
            0,
            0});
			this.maxValueNumericUpDown.Minimum = new decimal(new int[] {
            1410065408,
            2,
            0,
            -2147483648});
			this.maxValueNumericUpDown.Name = "maxValueNumericUpDown";
			this.maxValueNumericUpDown.Size = new System.Drawing.Size(90, 20);
			this.maxValueNumericUpDown.TabIndex = 8;
			this.maxValueNumericUpDown.ThousandsSeparator = true;
			this.maxValueNumericUpDown.ValueChanged += new System.EventHandler(this.maxValueNumericUpDown_ValueChanged);
			// 
			// rangeGroupBox
			// 
			this.rangeGroupBox.Controls.Add(this.fpsValuesCheckBox);
			this.rangeGroupBox.Controls.Add(this.minLabel);
			this.rangeGroupBox.Controls.Add(this.maxLabel);
			this.rangeGroupBox.Controls.Add(this.minValueNumericUpDown);
			this.rangeGroupBox.Controls.Add(this.maxValueNumericUpDown);
			this.rangeGroupBox.Location = new System.Drawing.Point(3, 193);
			this.rangeGroupBox.Name = "rangeGroupBox";
			this.rangeGroupBox.Size = new System.Drawing.Size(327, 74);
			this.rangeGroupBox.TabIndex = 10;
			this.rangeGroupBox.TabStop = false;
			this.rangeGroupBox.Text = "Range";
			// 
			// fpsValuesCheckBox
			// 
			this.fpsValuesCheckBox.AutoSize = true;
			this.fpsValuesCheckBox.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.fpsValuesCheckBox.Location = new System.Drawing.Point(162, 19);
			this.fpsValuesCheckBox.Name = "fpsValuesCheckBox";
			this.fpsValuesCheckBox.Size = new System.Drawing.Size(100, 17);
			this.fpsValuesCheckBox.TabIndex = 10;
			this.fpsValuesCheckBox.Text = "Values Are FPS";
			this.fpsValuesCheckBox.UseVisualStyleBackColor = true;
			this.fpsValuesCheckBox.CheckedChanged += new System.EventHandler(this.fpsValuesCheckBox_CheckedChanged);
			// 
			// detailsGroupBox
			// 
			this.detailsGroupBox.Controls.Add(this.nameFromPathCheckBox);
			this.detailsGroupBox.Controls.Add(this.nameTextBox);
			this.detailsGroupBox.Controls.Add(this.nameLabel);
			this.detailsGroupBox.Controls.Add(this.typeLabel2);
			this.detailsGroupBox.Controls.Add(this.typeLabel1);
			this.detailsGroupBox.Location = new System.Drawing.Point(3, 61);
			this.detailsGroupBox.Name = "detailsGroupBox";
			this.detailsGroupBox.Size = new System.Drawing.Size(333, 70);
			this.detailsGroupBox.TabIndex = 12;
			this.detailsGroupBox.TabStop = false;
			this.detailsGroupBox.Text = "Details";
			// 
			// nameFromPathCheckBox
			// 
			this.nameFromPathCheckBox.AutoSize = true;
			this.nameFromPathCheckBox.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.nameFromPathCheckBox.Checked = true;
			this.nameFromPathCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.nameFromPathCheckBox.Location = new System.Drawing.Point(253, 40);
			this.nameFromPathCheckBox.Name = "nameFromPathCheckBox";
			this.nameFromPathCheckBox.Size = new System.Drawing.Size(74, 17);
			this.nameFromPathCheckBox.TabIndex = 4;
			this.nameFromPathCheckBox.Text = "From Path";
			this.nameFromPathCheckBox.UseVisualStyleBackColor = true;
			this.nameFromPathCheckBox.CheckedChanged += new System.EventHandler(this.nameFromPathCheckBox_CheckedChanged);
			// 
			// nameTextBox
			// 
			this.nameTextBox.Location = new System.Drawing.Point(47, 38);
			this.nameTextBox.Name = "nameTextBox";
			this.nameTextBox.Size = new System.Drawing.Size(200, 20);
			this.nameTextBox.TabIndex = 3;
			this.nameTextBox.TextChanged += new System.EventHandler(this.nameTextBox_TextChanged);
			// 
			// nameLabel
			// 
			this.nameLabel.AutoSize = true;
			this.nameLabel.Location = new System.Drawing.Point(6, 41);
			this.nameLabel.Name = "nameLabel";
			this.nameLabel.Size = new System.Drawing.Size(35, 13);
			this.nameLabel.TabIndex = 2;
			this.nameLabel.Text = "Name";
			// 
			// typeLabel2
			// 
			this.typeLabel2.AutoSize = true;
			this.typeLabel2.Location = new System.Drawing.Point(44, 18);
			this.typeLabel2.Name = "typeLabel2";
			this.typeLabel2.Size = new System.Drawing.Size(100, 13);
			this.typeLabel2.TabIndex = 1;
			this.typeLabel2.Text = "Standard Threshold";
			// 
			// typeLabel1
			// 
			this.typeLabel1.AutoSize = true;
			this.typeLabel1.Location = new System.Drawing.Point(6, 18);
			this.typeLabel1.Name = "typeLabel1";
			this.typeLabel1.Size = new System.Drawing.Size(31, 13);
			this.typeLabel1.TabIndex = 0;
			this.typeLabel1.Text = "Type";
			// 
			// trackedPathGroupBox
			// 
			this.trackedPathGroupBox.Controls.Add(this.trackedPathTextBox);
			this.trackedPathGroupBox.Location = new System.Drawing.Point(3, 137);
			this.trackedPathGroupBox.Name = "trackedPathGroupBox";
			this.trackedPathGroupBox.Size = new System.Drawing.Size(333, 50);
			this.trackedPathGroupBox.TabIndex = 13;
			this.trackedPathGroupBox.TabStop = false;
			this.trackedPathGroupBox.Text = "Tracked Path";
			// 
			// itemPathLabel
			// 
			this.itemPathLabel.AutoSize = true;
			this.itemPathLabel.Location = new System.Drawing.Point(9, 24);
			this.itemPathLabel.Name = "itemPathLabel";
			this.itemPathLabel.Size = new System.Drawing.Size(57, 13);
			this.itemPathLabel.TabIndex = 1;
			this.itemPathLabel.Text = "/blah/blah";
			// 
			// itemPathGroupBox
			// 
			this.itemPathGroupBox.Controls.Add(this.itemPathLabel);
			this.itemPathGroupBox.Location = new System.Drawing.Point(3, 3);
			this.itemPathGroupBox.Name = "itemPathGroupBox";
			this.itemPathGroupBox.Size = new System.Drawing.Size(243, 52);
			this.itemPathGroupBox.TabIndex = 14;
			this.itemPathGroupBox.TabStop = false;
			this.itemPathGroupBox.Text = "Path";
			// 
			// StdThresholdZoneHighlighterControl
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.detailsGroupBox);
			this.Controls.Add(this.trackedPathGroupBox);
			this.Controls.Add(this.itemPathGroupBox);
			this.Controls.Add(this.colourGroupBox);
			this.Controls.Add(this.rangeGroupBox);
			this.Name = "StdThresholdZoneHighlighterControl";
			this.Size = new System.Drawing.Size(339, 306);
			this.colourGroupBox.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.minValueNumericUpDown)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.maxValueNumericUpDown)).EndInit();
			this.rangeGroupBox.ResumeLayout(false);
			this.rangeGroupBox.PerformLayout();
			this.detailsGroupBox.ResumeLayout(false);
			this.detailsGroupBox.PerformLayout();
			this.trackedPathGroupBox.ResumeLayout(false);
			this.trackedPathGroupBox.PerformLayout();
			this.itemPathGroupBox.ResumeLayout(false);
			this.itemPathGroupBox.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.GroupBox colourGroupBox;
		private System.Windows.Forms.Button itemColourButton;
		private System.Windows.Forms.Button colourRandomButton;
		private System.Windows.Forms.TextBox trackedPathTextBox;
		private NumericUpDownEx minValueNumericUpDown;
		private System.Windows.Forms.Label minLabel;
		private System.Windows.Forms.Label maxLabel;
		private NumericUpDownEx maxValueNumericUpDown;
		private System.Windows.Forms.GroupBox rangeGroupBox;
		private System.Windows.Forms.CheckBox fpsValuesCheckBox;
		private System.Windows.Forms.GroupBox detailsGroupBox;
		private System.Windows.Forms.GroupBox trackedPathGroupBox;
		private System.Windows.Forms.TextBox nameTextBox;
		private System.Windows.Forms.Label nameLabel;
		private System.Windows.Forms.Label typeLabel2;
		private System.Windows.Forms.Label typeLabel1;
		private System.Windows.Forms.CheckBox nameFromPathCheckBox;
		private System.Windows.Forms.Label itemPathLabel;
		private System.Windows.Forms.GroupBox itemPathGroupBox;
	}
}
