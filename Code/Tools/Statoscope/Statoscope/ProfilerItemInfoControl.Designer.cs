// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class ProfilerItemInfoControl
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
			this.itemDescriptionGroupBox = new System.Windows.Forms.GroupBox();
			this.itemDescriptionLabel = new System.Windows.Forms.Label();
			this.itemStatsGroupBox = new System.Windows.Forms.GroupBox();
			this.itemColourGroupBox = new System.Windows.Forms.GroupBox();
			this.itemColourButton = new System.Windows.Forms.Button();
			this.itemColourRandomButton = new System.Windows.Forms.Button();
			this.itemPathGroupBox = new System.Windows.Forms.GroupBox();
			this.itemPathLabel = new System.Windows.Forms.Label();
			this.itemDescriptionGroupBox.SuspendLayout();
			this.itemColourGroupBox.SuspendLayout();
			this.itemPathGroupBox.SuspendLayout();
			this.SuspendLayout();
			// 
			// itemDescriptionGroupBox
			// 
			this.itemDescriptionGroupBox.Controls.Add(this.itemDescriptionLabel);
			this.itemDescriptionGroupBox.Location = new System.Drawing.Point(102, 49);
			this.itemDescriptionGroupBox.Name = "itemDescriptionGroupBox";
			this.itemDescriptionGroupBox.Size = new System.Drawing.Size(223, 58);
			this.itemDescriptionGroupBox.TabIndex = 13;
			this.itemDescriptionGroupBox.TabStop = false;
			this.itemDescriptionGroupBox.Text = "Description";
			// 
			// itemDescriptionLabel
			// 
			this.itemDescriptionLabel.AutoSize = true;
			this.itemDescriptionLabel.Location = new System.Drawing.Point(6, 16);
			this.itemDescriptionLabel.Name = "itemDescriptionLabel";
			this.itemDescriptionLabel.Size = new System.Drawing.Size(105, 13);
			this.itemDescriptionLabel.TabIndex = 0;
			this.itemDescriptionLabel.Text = "itemDescriptionLabel";
			// 
			// itemStatsGroupBox
			// 
			this.itemStatsGroupBox.AutoSize = true;
			this.itemStatsGroupBox.Location = new System.Drawing.Point(3, 113);
			this.itemStatsGroupBox.Name = "itemStatsGroupBox";
			this.itemStatsGroupBox.Size = new System.Drawing.Size(322, 91);
			this.itemStatsGroupBox.TabIndex = 12;
			this.itemStatsGroupBox.TabStop = false;
			this.itemStatsGroupBox.Text = "Stats";
			// 
			// itemColourGroupBox
			// 
			this.itemColourGroupBox.Controls.Add(this.itemColourButton);
			this.itemColourGroupBox.Controls.Add(this.itemColourRandomButton);
			this.itemColourGroupBox.Location = new System.Drawing.Point(3, 49);
			this.itemColourGroupBox.Name = "itemColourGroupBox";
			this.itemColourGroupBox.Size = new System.Drawing.Size(86, 58);
			this.itemColourGroupBox.TabIndex = 11;
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
			this.itemPathGroupBox.TabIndex = 10;
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
			// ProfilerItemInfoControl
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.itemDescriptionGroupBox);
			this.Controls.Add(this.itemStatsGroupBox);
			this.Controls.Add(this.itemColourGroupBox);
			this.Controls.Add(this.itemPathGroupBox);
			this.Name = "ProfilerItemInfoControl";
			this.Size = new System.Drawing.Size(331, 266);
			this.itemDescriptionGroupBox.ResumeLayout(false);
			this.itemDescriptionGroupBox.PerformLayout();
			this.itemColourGroupBox.ResumeLayout(false);
			this.itemPathGroupBox.ResumeLayout(false);
			this.itemPathGroupBox.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.GroupBox itemDescriptionGroupBox;
		private System.Windows.Forms.Label itemDescriptionLabel;
		private System.Windows.Forms.GroupBox itemStatsGroupBox;
		private System.Windows.Forms.GroupBox itemColourGroupBox;
		private System.Windows.Forms.Button itemColourButton;
		private System.Windows.Forms.Button itemColourRandomButton;
		private System.Windows.Forms.GroupBox itemPathGroupBox;
		private System.Windows.Forms.Label itemPathLabel;
	}
}
