// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class LogListItem
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
			this.colourButton = new System.Windows.Forms.Button();
			this.itemColourRandomButton = new System.Windows.Forms.Button();
			this.nameListView = new System.Windows.Forms.ListView();
			this.panel1 = new System.Windows.Forms.Panel();
			this.trackingButton = new System.Windows.Forms.Button();
			this.startFrameNumericUpDown = new NumericUpDownEx();
			this.endFrameNumericUpDown = new NumericUpDownEx();
			this.panel1.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.startFrameNumericUpDown)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.endFrameNumericUpDown)).BeginInit();
			this.SuspendLayout();
			// 
			// colourButton
			// 
			this.colourButton.BackColor = System.Drawing.Color.White;
			this.colourButton.Location = new System.Drawing.Point(152, 0);
			this.colourButton.Name = "colourButton";
			this.colourButton.Size = new System.Drawing.Size(23, 23);
			this.colourButton.TabIndex = 2;
			this.colourButton.UseVisualStyleBackColor = false;
			this.colourButton.Click += new System.EventHandler(this.colourButton_Click);
			// 
			// itemColourRandomButton
			// 
			this.itemColourRandomButton.Location = new System.Drawing.Point(176, 0);
			this.itemColourRandomButton.Name = "itemColourRandomButton";
			this.itemColourRandomButton.Size = new System.Drawing.Size(40, 23);
			this.itemColourRandomButton.TabIndex = 3;
			this.itemColourRandomButton.Text = "Rnd";
			this.itemColourRandomButton.UseVisualStyleBackColor = true;
			this.itemColourRandomButton.Click += new System.EventHandler(this.itemColourRandomButton_Click);
			// 
			// nameListView
			// 
			this.nameListView.BackColor = System.Drawing.SystemColors.Control;
			this.nameListView.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.nameListView.CheckBoxes = true;
			this.nameListView.LabelEdit = true;
			this.nameListView.LabelWrap = false;
			this.nameListView.Location = new System.Drawing.Point(3, 5);
			this.nameListView.Name = "nameListView";
			this.nameListView.ShowItemToolTips = true;
			this.nameListView.Size = new System.Drawing.Size(116, 14);
			this.nameListView.TabIndex = 4;
			this.nameListView.UseCompatibleStateImageBehavior = false;
			this.nameListView.View = System.Windows.Forms.View.List;
			this.nameListView.ItemChecked += new System.Windows.Forms.ItemCheckedEventHandler(this.nameListView_ItemChecked);
			this.nameListView.AfterLabelEdit += new System.Windows.Forms.LabelEditEventHandler(this.nameListView_AfterLabelEdit);
			this.nameListView.KeyDown += new System.Windows.Forms.KeyEventHandler(this.nameListView_KeyDown);
			// 
			// panel1
			// 
			this.panel1.Controls.Add(this.trackingButton);
			this.panel1.Controls.Add(this.nameListView);
			this.panel1.Controls.Add(this.itemColourRandomButton);
			this.panel1.Controls.Add(this.colourButton);
			this.panel1.Location = new System.Drawing.Point(0, 0);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(216, 23);
			this.panel1.TabIndex = 5;
			// 
			// trackingButton
			// 
			this.trackingButton.Location = new System.Drawing.Point(125, 0);
			this.trackingButton.Name = "trackingButton";
			this.trackingButton.Size = new System.Drawing.Size(23, 23);
			this.trackingButton.TabIndex = 5;
			this.trackingButton.Text = "T";
			this.trackingButton.UseVisualStyleBackColor = true;
			this.trackingButton.Click += new System.EventHandler(this.trackingButton_Click);
			// 
			// startFrameNumericUpDown
			// 
			this.startFrameNumericUpDown.Location = new System.Drawing.Point(222, 2);
			this.startFrameNumericUpDown.Maximum = new decimal(new int[] {
            12345,
            0,
            0,
            0});
			this.startFrameNumericUpDown.Name = "startFrameNumericUpDown";
			this.startFrameNumericUpDown.Size = new System.Drawing.Size(51, 20);
			this.startFrameNumericUpDown.TabIndex = 6;
			this.startFrameNumericUpDown.Value = new decimal(new int[] {
            12345,
            0,
            0,
            0});
			// 
			// endFrameNumericUpDown
			// 
			this.endFrameNumericUpDown.Location = new System.Drawing.Point(279, 2);
			this.endFrameNumericUpDown.Maximum = new decimal(new int[] {
            123456,
            0,
            0,
            0});
			this.endFrameNumericUpDown.Name = "endFrameNumericUpDown";
			this.endFrameNumericUpDown.Size = new System.Drawing.Size(51, 20);
			this.endFrameNumericUpDown.TabIndex = 7;
			this.endFrameNumericUpDown.Value = new decimal(new int[] {
            12345,
            0,
            0,
            0});
			// 
			// LogListItem
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.endFrameNumericUpDown);
			this.Controls.Add(this.startFrameNumericUpDown);
			this.Controls.Add(this.panel1);
			this.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.MinimumSize = new System.Drawing.Size(0, 23);
			this.Name = "LogListItem";
			this.Size = new System.Drawing.Size(333, 24);
			this.panel1.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.startFrameNumericUpDown)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.endFrameNumericUpDown)).EndInit();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Button colourButton;
		private System.Windows.Forms.Button itemColourRandomButton;
		public System.Windows.Forms.ListView nameListView;
		private System.Windows.Forms.Panel panel1;
		private NumericUpDownEx startFrameNumericUpDown;
		private NumericUpDownEx endFrameNumericUpDown;
		private System.Windows.Forms.Button trackingButton;
	}
}
