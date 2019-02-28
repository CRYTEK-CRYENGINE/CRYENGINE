// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class ConfluenceUploadForm
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

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.UploadButton = new System.Windows.Forms.Button();
			this.UploadCancelButton = new System.Windows.Forms.Button();
			this.SpaceComboBox = new System.Windows.Forms.ComboBox();
			this.RootPageComboBox = new System.Windows.Forms.ComboBox();
			this.SpaceLabel = new System.Windows.Forms.Label();
			this.RootPageLabel = new System.Windows.Forms.Label();
			this.PlatformLabel = new System.Windows.Forms.Label();
			this.PlatformComboBox = new System.Windows.Forms.ComboBox();
			this.BuildLabel = new System.Windows.Forms.Label();
			this.BuildComboBox = new System.Windows.Forms.ComboBox();
			this.LevelsLabel = new System.Windows.Forms.Label();
			this.LevelsCheckedListBox = new System.Windows.Forms.CheckedListBox();
			this.IncludeOverallCheckBox = new System.Windows.Forms.CheckBox();
			this.AppendRadioButton = new System.Windows.Forms.RadioButton();
			this.ReplaceRadioButton = new System.Windows.Forms.RadioButton();
			this.SpaceExistsLabel = new System.Windows.Forms.Label();
			this.RootPageExistsLabel = new System.Windows.Forms.Label();
			this.PlatformPageExistsLabel = new System.Windows.Forms.Label();
			this.BuildPageExistsLabel = new System.Windows.Forms.Label();
			this.InfoLabel = new System.Windows.Forms.Label();
			this.BuildInfoTextBox = new System.Windows.Forms.TextBox();
			this.BuildInfoLabel = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// UploadButton
			// 
			this.UploadButton.Location = new System.Drawing.Point(244, 385);
			this.UploadButton.Name = "UploadButton";
			this.UploadButton.Size = new System.Drawing.Size(75, 23);
			this.UploadButton.TabIndex = 0;
			this.UploadButton.Text = "Upload";
			this.UploadButton.UseVisualStyleBackColor = true;
			this.UploadButton.Click += new System.EventHandler(this.UploadButton_Click);
			// 
			// UploadCancelButton
			// 
			this.UploadCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.UploadCancelButton.Location = new System.Drawing.Point(325, 385);
			this.UploadCancelButton.Name = "UploadCancelButton";
			this.UploadCancelButton.Size = new System.Drawing.Size(75, 23);
			this.UploadCancelButton.TabIndex = 1;
			this.UploadCancelButton.Text = "Cancel";
			this.UploadCancelButton.UseVisualStyleBackColor = true;
			// 
			// SpaceComboBox
			// 
			this.SpaceComboBox.FormattingEnabled = true;
			this.SpaceComboBox.Location = new System.Drawing.Point(92, 36);
			this.SpaceComboBox.Name = "SpaceComboBox";
			this.SpaceComboBox.Size = new System.Drawing.Size(200, 21);
			this.SpaceComboBox.TabIndex = 2;
			// 
			// RootPageComboBox
			// 
			this.RootPageComboBox.FormattingEnabled = true;
			this.RootPageComboBox.Location = new System.Drawing.Point(92, 63);
			this.RootPageComboBox.Name = "RootPageComboBox";
			this.RootPageComboBox.Size = new System.Drawing.Size(201, 21);
			this.RootPageComboBox.TabIndex = 3;
			// 
			// SpaceLabel
			// 
			this.SpaceLabel.AutoSize = true;
			this.SpaceLabel.Location = new System.Drawing.Point(12, 39);
			this.SpaceLabel.Name = "SpaceLabel";
			this.SpaceLabel.Size = new System.Drawing.Size(38, 13);
			this.SpaceLabel.TabIndex = 4;
			this.SpaceLabel.Text = "Space";
			// 
			// RootPageLabel
			// 
			this.RootPageLabel.AutoSize = true;
			this.RootPageLabel.Location = new System.Drawing.Point(12, 66);
			this.RootPageLabel.Name = "RootPageLabel";
			this.RootPageLabel.Size = new System.Drawing.Size(58, 13);
			this.RootPageLabel.TabIndex = 5;
			this.RootPageLabel.Text = "Root Page";
			// 
			// PlatformLabel
			// 
			this.PlatformLabel.AutoSize = true;
			this.PlatformLabel.Location = new System.Drawing.Point(12, 93);
			this.PlatformLabel.Name = "PlatformLabel";
			this.PlatformLabel.Size = new System.Drawing.Size(73, 13);
			this.PlatformLabel.TabIndex = 6;
			this.PlatformLabel.Text = "Platform Page";
			// 
			// PlatformComboBox
			// 
			this.PlatformComboBox.FormattingEnabled = true;
			this.PlatformComboBox.Location = new System.Drawing.Point(92, 90);
			this.PlatformComboBox.Name = "PlatformComboBox";
			this.PlatformComboBox.Size = new System.Drawing.Size(201, 21);
			this.PlatformComboBox.TabIndex = 7;
			// 
			// BuildLabel
			// 
			this.BuildLabel.AutoSize = true;
			this.BuildLabel.Location = new System.Drawing.Point(12, 120);
			this.BuildLabel.Name = "BuildLabel";
			this.BuildLabel.Size = new System.Drawing.Size(58, 13);
			this.BuildLabel.TabIndex = 8;
			this.BuildLabel.Text = "Build Page";
			// 
			// BuildComboBox
			// 
			this.BuildComboBox.FormattingEnabled = true;
			this.BuildComboBox.Location = new System.Drawing.Point(91, 117);
			this.BuildComboBox.Name = "BuildComboBox";
			this.BuildComboBox.Size = new System.Drawing.Size(201, 21);
			this.BuildComboBox.TabIndex = 9;
			// 
			// LevelsLabel
			// 
			this.LevelsLabel.AutoSize = true;
			this.LevelsLabel.Location = new System.Drawing.Point(12, 165);
			this.LevelsLabel.Name = "LevelsLabel";
			this.LevelsLabel.Size = new System.Drawing.Size(38, 13);
			this.LevelsLabel.TabIndex = 10;
			this.LevelsLabel.Text = "Levels";
			// 
			// LevelsCheckedListBox
			// 
			this.LevelsCheckedListBox.CheckOnClick = true;
			this.LevelsCheckedListBox.FormattingEnabled = true;
			this.LevelsCheckedListBox.Location = new System.Drawing.Point(92, 167);
			this.LevelsCheckedListBox.Name = "LevelsCheckedListBox";
			this.LevelsCheckedListBox.Size = new System.Drawing.Size(201, 124);
			this.LevelsCheckedListBox.TabIndex = 11;
			// 
			// IncludeOverallCheckBox
			// 
			this.IncludeOverallCheckBox.AutoSize = true;
			this.IncludeOverallCheckBox.Checked = true;
			this.IncludeOverallCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.IncludeOverallCheckBox.Location = new System.Drawing.Point(92, 297);
			this.IncludeOverallCheckBox.Name = "IncludeOverallCheckBox";
			this.IncludeOverallCheckBox.Size = new System.Drawing.Size(97, 17);
			this.IncludeOverallCheckBox.TabIndex = 12;
			this.IncludeOverallCheckBox.Text = "Include Overall";
			this.IncludeOverallCheckBox.UseVisualStyleBackColor = true;
			// 
			// AppendRadioButton
			// 
			this.AppendRadioButton.AutoSize = true;
			this.AppendRadioButton.Checked = true;
			this.AppendRadioButton.Location = new System.Drawing.Point(92, 144);
			this.AppendRadioButton.Name = "AppendRadioButton";
			this.AppendRadioButton.Size = new System.Drawing.Size(62, 17);
			this.AppendRadioButton.TabIndex = 14;
			this.AppendRadioButton.TabStop = true;
			this.AppendRadioButton.Text = "Append";
			this.AppendRadioButton.UseVisualStyleBackColor = true;
			// 
			// ReplaceRadioButton
			// 
			this.ReplaceRadioButton.AutoSize = true;
			this.ReplaceRadioButton.Location = new System.Drawing.Point(160, 144);
			this.ReplaceRadioButton.Name = "ReplaceRadioButton";
			this.ReplaceRadioButton.Size = new System.Drawing.Size(65, 17);
			this.ReplaceRadioButton.TabIndex = 15;
			this.ReplaceRadioButton.Text = "Replace";
			this.ReplaceRadioButton.UseVisualStyleBackColor = true;
			// 
			// SpaceExistsLabel
			// 
			this.SpaceExistsLabel.AutoSize = true;
			this.SpaceExistsLabel.Location = new System.Drawing.Point(298, 39);
			this.SpaceExistsLabel.Name = "SpaceExistsLabel";
			this.SpaceExistsLabel.Size = new System.Drawing.Size(29, 13);
			this.SpaceExistsLabel.TabIndex = 16;
			this.SpaceExistsLabel.Text = "New";
			// 
			// RootPageExistsLabel
			// 
			this.RootPageExistsLabel.AutoSize = true;
			this.RootPageExistsLabel.Location = new System.Drawing.Point(298, 66);
			this.RootPageExistsLabel.Name = "RootPageExistsLabel";
			this.RootPageExistsLabel.Size = new System.Drawing.Size(34, 13);
			this.RootPageExistsLabel.TabIndex = 17;
			this.RootPageExistsLabel.Text = "Exists";
			// 
			// PlatformPageExistsLabel
			// 
			this.PlatformPageExistsLabel.AutoSize = true;
			this.PlatformPageExistsLabel.Location = new System.Drawing.Point(298, 93);
			this.PlatformPageExistsLabel.Name = "PlatformPageExistsLabel";
			this.PlatformPageExistsLabel.Size = new System.Drawing.Size(84, 13);
			this.PlatformPageExistsLabel.TabIndex = 18;
			this.PlatformPageExistsLabel.Text = "Root not parent!";
			// 
			// BuildPageExistsLabel
			// 
			this.BuildPageExistsLabel.AutoSize = true;
			this.BuildPageExistsLabel.Location = new System.Drawing.Point(298, 120);
			this.BuildPageExistsLabel.Name = "BuildPageExistsLabel";
			this.BuildPageExistsLabel.Size = new System.Drawing.Size(99, 13);
			this.BuildPageExistsLabel.TabIndex = 19;
			this.BuildPageExistsLabel.Text = "Platform not parent!";
			// 
			// InfoLabel
			// 
			this.InfoLabel.AutoSize = true;
			this.InfoLabel.Location = new System.Drawing.Point(5, 9);
			this.InfoLabel.Name = "InfoLabel";
			this.InfoLabel.Size = new System.Drawing.Size(403, 13);
			this.InfoLabel.TabIndex = 20;
			this.InfoLabel.Text = "The first entries for Platform and Build Pages are suggestions based on the log n" +
					"ame";
			// 
			// BuildInfoTextBox
			// 
			this.BuildInfoTextBox.AcceptsReturn = true;
			this.BuildInfoTextBox.AcceptsTab = true;
			this.BuildInfoTextBox.Location = new System.Drawing.Point(92, 320);
			this.BuildInfoTextBox.Multiline = true;
			this.BuildInfoTextBox.Name = "BuildInfoTextBox";
			this.BuildInfoTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
			this.BuildInfoTextBox.Size = new System.Drawing.Size(201, 59);
			this.BuildInfoTextBox.TabIndex = 21;
			// 
			// BuildInfoLabel
			// 
			this.BuildInfoLabel.AutoSize = true;
			this.BuildInfoLabel.Location = new System.Drawing.Point(12, 323);
			this.BuildInfoLabel.Name = "BuildInfoLabel";
			this.BuildInfoLabel.Size = new System.Drawing.Size(51, 13);
			this.BuildInfoLabel.TabIndex = 22;
			this.BuildInfoLabel.Text = "Build Info";
			// 
			// ConfluenceUploadForm
			// 
			this.AcceptButton = this.UploadButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.UploadCancelButton;
			this.ClientSize = new System.Drawing.Size(412, 420);
			this.ControlBox = false;
			this.Controls.Add(this.BuildInfoLabel);
			this.Controls.Add(this.BuildInfoTextBox);
			this.Controls.Add(this.InfoLabel);
			this.Controls.Add(this.BuildPageExistsLabel);
			this.Controls.Add(this.PlatformPageExistsLabel);
			this.Controls.Add(this.RootPageExistsLabel);
			this.Controls.Add(this.SpaceExistsLabel);
			this.Controls.Add(this.ReplaceRadioButton);
			this.Controls.Add(this.AppendRadioButton);
			this.Controls.Add(this.IncludeOverallCheckBox);
			this.Controls.Add(this.LevelsCheckedListBox);
			this.Controls.Add(this.LevelsLabel);
			this.Controls.Add(this.BuildComboBox);
			this.Controls.Add(this.BuildLabel);
			this.Controls.Add(this.PlatformComboBox);
			this.Controls.Add(this.PlatformLabel);
			this.Controls.Add(this.RootPageLabel);
			this.Controls.Add(this.SpaceLabel);
			this.Controls.Add(this.RootPageComboBox);
			this.Controls.Add(this.SpaceComboBox);
			this.Controls.Add(this.UploadCancelButton);
			this.Controls.Add(this.UploadButton);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ConfluenceUploadForm";
			this.Text = "Confluence Upload";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Button UploadButton;
		private System.Windows.Forms.Button UploadCancelButton;
		private System.Windows.Forms.ComboBox SpaceComboBox;
		private System.Windows.Forms.ComboBox RootPageComboBox;
		private System.Windows.Forms.Label SpaceLabel;
		private System.Windows.Forms.Label RootPageLabel;
		private System.Windows.Forms.Label PlatformLabel;
		private System.Windows.Forms.ComboBox PlatformComboBox;
		private System.Windows.Forms.Label BuildLabel;
		private System.Windows.Forms.ComboBox BuildComboBox;
		private System.Windows.Forms.Label LevelsLabel;
		private System.Windows.Forms.CheckedListBox LevelsCheckedListBox;
		private System.Windows.Forms.CheckBox IncludeOverallCheckBox;
		private System.Windows.Forms.RadioButton AppendRadioButton;
		private System.Windows.Forms.RadioButton ReplaceRadioButton;
		private System.Windows.Forms.Label SpaceExistsLabel;
		private System.Windows.Forms.Label RootPageExistsLabel;
		private System.Windows.Forms.Label PlatformPageExistsLabel;
		private System.Windows.Forms.Label BuildPageExistsLabel;
		private System.Windows.Forms.Label InfoLabel;
		private System.Windows.Forms.TextBox BuildInfoTextBox;
		private System.Windows.Forms.Label BuildInfoLabel;
	}
}