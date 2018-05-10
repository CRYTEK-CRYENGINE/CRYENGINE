// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class OverviewItemInfoControl
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
            this.filteringGroupBox = new System.Windows.Forms.GroupBox();
            this.filteringPanel = new System.Windows.Forms.FlowLayoutPanel();
            this.noneFilteringRadioButton = new System.Windows.Forms.RadioButton();
            this.maFilteringRadioButton = new System.Windows.Forms.RadioButton();
            this.maUpDown = new NumericUpDownEx();
            this.lmFilteringRadioButton = new System.Windows.Forms.RadioButton();
            this.lmUpDown = new NumericUpDownEx();
            this.itemStatsGroupBox = new System.Windows.Forms.GroupBox();
            this.itemColourGroupBox = new System.Windows.Forms.GroupBox();
            this.itemColourButton = new System.Windows.Forms.Button();
            this.itemColourRandomButton = new System.Windows.Forms.Button();
            this.itemPathGroupBox = new System.Windows.Forms.GroupBox();
            this.itemPathLabel = new System.Windows.Forms.Label();
            this.frameOffsetGroupBox = new System.Windows.Forms.GroupBox();
            this.frameOffsetNumericUpDown = new NumericUpDownEx();
            this.CustomDisplayScalingBox = new System.Windows.Forms.GroupBox();
            this.CustomDisplayScaleUpDown = new NumericUpDownEx();
            this.filteringGroupBox.SuspendLayout();
            this.filteringPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.maUpDown)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.lmUpDown)).BeginInit();
            this.itemColourGroupBox.SuspendLayout();
            this.itemPathGroupBox.SuspendLayout();
            this.frameOffsetGroupBox.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.frameOffsetNumericUpDown)).BeginInit();
            this.CustomDisplayScalingBox.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.CustomDisplayScaleUpDown)).BeginInit();
            this.SuspendLayout();
            // 
            // filteringGroupBox
            // 
            this.filteringGroupBox.Controls.Add(this.filteringPanel);
            this.filteringGroupBox.Location = new System.Drawing.Point(3, 230);
            this.filteringGroupBox.Name = "filteringGroupBox";
            this.filteringGroupBox.Size = new System.Drawing.Size(322, 45);
            this.filteringGroupBox.TabIndex = 9;
            this.filteringGroupBox.TabStop = false;
            this.filteringGroupBox.Text = "Filtering";
            // 
            // filteringPanel
            // 
            this.filteringPanel.Controls.Add(this.noneFilteringRadioButton);
            this.filteringPanel.Controls.Add(this.maFilteringRadioButton);
            this.filteringPanel.Controls.Add(this.maUpDown);
            this.filteringPanel.Controls.Add(this.lmFilteringRadioButton);
            this.filteringPanel.Controls.Add(this.lmUpDown);
            this.filteringPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.filteringPanel.Location = new System.Drawing.Point(3, 16);
            this.filteringPanel.Name = "filteringPanel";
            this.filteringPanel.Size = new System.Drawing.Size(316, 26);
            this.filteringPanel.TabIndex = 1;
            // 
            // noneFilteringRadioButton
            // 
            this.noneFilteringRadioButton.AutoSize = true;
            this.noneFilteringRadioButton.Checked = true;
            this.noneFilteringRadioButton.Location = new System.Drawing.Point(3, 3);
            this.noneFilteringRadioButton.Name = "noneFilteringRadioButton";
            this.noneFilteringRadioButton.Size = new System.Drawing.Size(51, 17);
            this.noneFilteringRadioButton.TabIndex = 8;
            this.noneFilteringRadioButton.TabStop = true;
            this.noneFilteringRadioButton.Text = "None";
            this.noneFilteringRadioButton.UseVisualStyleBackColor = true;
            this.noneFilteringRadioButton.CheckedChanged += new System.EventHandler(this.noneFilteringRadioButton_CheckedChanged);
            // 
            // maFilteringRadioButton
            // 
            this.maFilteringRadioButton.AutoSize = true;
            this.maFilteringRadioButton.Location = new System.Drawing.Point(60, 3);
            this.maFilteringRadioButton.Margin = new System.Windows.Forms.Padding(3, 3, 0, 3);
            this.maFilteringRadioButton.Name = "maFilteringRadioButton";
            this.maFilteringRadioButton.Size = new System.Drawing.Size(82, 17);
            this.maFilteringRadioButton.TabIndex = 6;
            this.maFilteringRadioButton.Text = "Moving Avg";
            this.maFilteringRadioButton.UseVisualStyleBackColor = true;
            this.maFilteringRadioButton.CheckedChanged += new System.EventHandler(this.maFilteringRadioButton_CheckedChanged);
            // 
            // maUpDown
            // 
            this.maUpDown.Location = new System.Drawing.Point(145, 3);
            this.maUpDown.Maximum = new decimal(new int[] {
            999,
            0,
            0,
            0});
            this.maUpDown.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.maUpDown.Name = "maUpDown";
            this.maUpDown.Size = new System.Drawing.Size(42, 20);
            this.maUpDown.TabIndex = 2;
            this.maUpDown.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
            this.maUpDown.Value = new decimal(new int[] {
            999,
            0,
            0,
            0});
            this.maUpDown.ValueChanged += new System.EventHandler(this.maUpDown_ValueChanged);
            // 
            // lmFilteringRadioButton
            // 
            this.lmFilteringRadioButton.AutoSize = true;
            this.lmFilteringRadioButton.Location = new System.Drawing.Point(193, 3);
            this.lmFilteringRadioButton.Margin = new System.Windows.Forms.Padding(3, 3, 0, 3);
            this.lmFilteringRadioButton.Name = "lmFilteringRadioButton";
            this.lmFilteringRadioButton.Size = new System.Drawing.Size(74, 17);
            this.lmFilteringRadioButton.TabIndex = 7;
            this.lmFilteringRadioButton.Text = "Local Max";
            this.lmFilteringRadioButton.UseVisualStyleBackColor = true;
            this.lmFilteringRadioButton.CheckedChanged += new System.EventHandler(this.lmFilteringRadioButton_CheckedChanged);
            // 
            // lmUpDown
            // 
            this.lmUpDown.Location = new System.Drawing.Point(270, 3);
            this.lmUpDown.Maximum = new decimal(new int[] {
            999,
            0,
            0,
            0});
            this.lmUpDown.Name = "lmUpDown";
            this.lmUpDown.Size = new System.Drawing.Size(42, 20);
            this.lmUpDown.TabIndex = 5;
            this.lmUpDown.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
            this.lmUpDown.Value = new decimal(new int[] {
            999,
            0,
            0,
            0});
            this.lmUpDown.ValueChanged += new System.EventHandler(this.lmUpDown_ValueChanged);
            // 
            // itemStatsGroupBox
            // 
            this.itemStatsGroupBox.AutoSize = true;
            this.itemStatsGroupBox.Location = new System.Drawing.Point(3, 101);
            this.itemStatsGroupBox.Name = "itemStatsGroupBox";
            this.itemStatsGroupBox.Size = new System.Drawing.Size(322, 123);
            this.itemStatsGroupBox.TabIndex = 7;
            this.itemStatsGroupBox.TabStop = false;
            this.itemStatsGroupBox.Text = "Stats";
            // 
            // itemColourGroupBox
            // 
            this.itemColourGroupBox.Controls.Add(this.itemColourButton);
            this.itemColourGroupBox.Controls.Add(this.itemColourRandomButton);
            this.itemColourGroupBox.Location = new System.Drawing.Point(3, 43);
            this.itemColourGroupBox.Name = "itemColourGroupBox";
            this.itemColourGroupBox.Size = new System.Drawing.Size(86, 58);
            this.itemColourGroupBox.TabIndex = 6;
            this.itemColourGroupBox.TabStop = false;
            this.itemColourGroupBox.Text = "Colour";
            // 
            // itemColourButton
            // 
            this.itemColourButton.BackColor = System.Drawing.Color.White;
            this.itemColourButton.Location = new System.Drawing.Point(7, 21);
            this.itemColourButton.Name = "itemColourButton";
            this.itemColourButton.Size = new System.Drawing.Size(23, 23);
            this.itemColourButton.TabIndex = 0;
            this.itemColourButton.UseVisualStyleBackColor = false;
            this.itemColourButton.Click += new System.EventHandler(this.itemColourButton_Click);
            // 
            // itemColourRandomButton
            // 
            this.itemColourRandomButton.Location = new System.Drawing.Point(36, 21);
            this.itemColourRandomButton.Name = "itemColourRandomButton";
            this.itemColourRandomButton.Size = new System.Drawing.Size(40, 23);
            this.itemColourRandomButton.TabIndex = 2;
            this.itemColourRandomButton.Text = "Rnd";
            this.itemColourRandomButton.UseVisualStyleBackColor = true;
            this.itemColourRandomButton.Click += new System.EventHandler(this.itemColourRandomButton_Click);
            // 
            // itemPathGroupBox
            // 
            this.itemPathGroupBox.Controls.Add(this.itemPathLabel);
            this.itemPathGroupBox.Location = new System.Drawing.Point(3, 3);
            this.itemPathGroupBox.Name = "itemPathGroupBox";
            this.itemPathGroupBox.Size = new System.Drawing.Size(322, 39);
            this.itemPathGroupBox.TabIndex = 5;
            this.itemPathGroupBox.TabStop = false;
            this.itemPathGroupBox.Text = "Path";
            // 
            // itemPathLabel
            // 
            this.itemPathLabel.AutoSize = true;
            this.itemPathLabel.Location = new System.Drawing.Point(9, 16);
            this.itemPathLabel.Name = "itemPathLabel";
            this.itemPathLabel.Size = new System.Drawing.Size(57, 13);
            this.itemPathLabel.TabIndex = 1;
            this.itemPathLabel.Text = "/blah/blah";
            // 
            // frameOffsetGroupBox
            // 
            this.frameOffsetGroupBox.Controls.Add(this.frameOffsetNumericUpDown);
            this.frameOffsetGroupBox.Location = new System.Drawing.Point(244, 43);
            this.frameOffsetGroupBox.Name = "frameOffsetGroupBox";
            this.frameOffsetGroupBox.Size = new System.Drawing.Size(81, 58);
            this.frameOffsetGroupBox.TabIndex = 10;
            this.frameOffsetGroupBox.TabStop = false;
            this.frameOffsetGroupBox.Text = "Frame Offset";
            // 
            // frameOffsetNumericUpDown
            // 
            this.frameOffsetNumericUpDown.Location = new System.Drawing.Point(11, 24);
            this.frameOffsetNumericUpDown.Maximum = new decimal(new int[] {
            1000,
            0,
            0,
            0});
            this.frameOffsetNumericUpDown.Minimum = new decimal(new int[] {
            1000,
            0,
            0,
            -2147483648});
            this.frameOffsetNumericUpDown.Name = "frameOffsetNumericUpDown";
            this.frameOffsetNumericUpDown.Size = new System.Drawing.Size(58, 20);
            this.frameOffsetNumericUpDown.TabIndex = 0;
            this.frameOffsetNumericUpDown.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
            this.frameOffsetNumericUpDown.ValueChanged += new System.EventHandler(this.frameOffsetNumericUpDown_ValueChanged);
            // 
            // CustomDisplayScalingBox
            // 
            this.CustomDisplayScalingBox.Controls.Add(this.CustomDisplayScaleUpDown);
            this.CustomDisplayScalingBox.Location = new System.Drawing.Point(102, 43);
            this.CustomDisplayScalingBox.Name = "CustomDisplayScalingBox";
            this.CustomDisplayScalingBox.Size = new System.Drawing.Size(128, 58);
            this.CustomDisplayScalingBox.TabIndex = 11;
            this.CustomDisplayScalingBox.TabStop = false;
            this.CustomDisplayScalingBox.Text = "Custom displayScaling";
            // 
            // CustomDisplayScaleUpDown
            // 
            this.CustomDisplayScaleUpDown.DecimalPlaces = 3;
            this.CustomDisplayScaleUpDown.Increment = new decimal(new int[] {
            1,
            0,
            0,
            131072});
            this.CustomDisplayScaleUpDown.Location = new System.Drawing.Point(6, 24);
            this.CustomDisplayScaleUpDown.Maximum = new decimal(new int[] {
            1000000,
            0,
            0,
            0});
            this.CustomDisplayScaleUpDown.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            196608});
            this.CustomDisplayScaleUpDown.Name = "CustomDisplayScaleUpDown";
            this.CustomDisplayScaleUpDown.Size = new System.Drawing.Size(108, 20);
            this.CustomDisplayScaleUpDown.TabIndex = 0;
            this.CustomDisplayScaleUpDown.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
            this.CustomDisplayScaleUpDown.Value = new decimal(new int[] {
            1,
            0,
            0,
            196608});
            this.CustomDisplayScaleUpDown.ValueChanged += new System.EventHandler(this.CustomDisplayScaleUpDown_ValueChanged);
            // 
            // OverviewItemInfoControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.CustomDisplayScalingBox);
            this.Controls.Add(this.frameOffsetGroupBox);
            this.Controls.Add(this.filteringGroupBox);
            this.Controls.Add(this.itemStatsGroupBox);
            this.Controls.Add(this.itemColourGroupBox);
            this.Controls.Add(this.itemPathGroupBox);
            this.Name = "OverviewItemInfoControl";
            this.Size = new System.Drawing.Size(331, 281);
            this.filteringGroupBox.ResumeLayout(false);
            this.filteringPanel.ResumeLayout(false);
            this.filteringPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.maUpDown)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.lmUpDown)).EndInit();
            this.itemColourGroupBox.ResumeLayout(false);
            this.itemPathGroupBox.ResumeLayout(false);
            this.itemPathGroupBox.PerformLayout();
            this.frameOffsetGroupBox.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.frameOffsetNumericUpDown)).EndInit();
            this.CustomDisplayScalingBox.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.CustomDisplayScaleUpDown)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.GroupBox filteringGroupBox;
		private System.Windows.Forms.FlowLayoutPanel filteringPanel;
		private System.Windows.Forms.RadioButton noneFilteringRadioButton;
		private System.Windows.Forms.RadioButton maFilteringRadioButton;
		private NumericUpDownEx maUpDown;
		private System.Windows.Forms.RadioButton lmFilteringRadioButton;
        private NumericUpDownEx lmUpDown;
		private System.Windows.Forms.GroupBox itemStatsGroupBox;
		private System.Windows.Forms.GroupBox itemColourGroupBox;
		private System.Windows.Forms.Button itemColourButton;
		private System.Windows.Forms.Button itemColourRandomButton;
		private System.Windows.Forms.GroupBox itemPathGroupBox;
		private System.Windows.Forms.Label itemPathLabel;
		private System.Windows.Forms.GroupBox frameOffsetGroupBox;
		private NumericUpDownEx frameOffsetNumericUpDown;
        private System.Windows.Forms.GroupBox CustomDisplayScalingBox;
        private NumericUpDownEx CustomDisplayScaleUpDown;

	}
}
