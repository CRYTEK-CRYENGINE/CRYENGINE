// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class TargetLineCTVControlPanel
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(TargetLineCTVControlPanel));
			this.tlPanel = new System.Windows.Forms.Panel();
			this.tlToolStrip = new System.Windows.Forms.ToolStrip();
			this.addTargetLineButton = new System.Windows.Forms.ToolStripButton();
			this.addNodeButton = new System.Windows.Forms.ToolStripButton();
			this.deleteButton = new System.Windows.Forms.ToolStripButton();
			this.flowLayoutPanel = new System.Windows.Forms.FlowLayoutPanel();
			this.ctvControlPanel = new Statoscope.CTVControlPanel();
			this.tlPanel.SuspendLayout();
			this.tlToolStrip.SuspendLayout();
			this.flowLayoutPanel.SuspendLayout();
			this.SuspendLayout();
			// 
			// tlPanel
			// 
			this.tlPanel.Controls.Add(this.tlToolStrip);
			this.tlPanel.Location = new System.Drawing.Point(47, 0);
			this.tlPanel.Margin = new System.Windows.Forms.Padding(0);
			this.tlPanel.Name = "tlPanel";
			this.tlPanel.Size = new System.Drawing.Size(71, 25);
			this.tlPanel.TabIndex = 2;
			// 
			// tlToolStrip
			// 
			this.tlToolStrip.AutoSize = false;
			this.tlToolStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
			this.tlToolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.addTargetLineButton,
            this.addNodeButton,
            this.deleteButton});
			this.tlToolStrip.Location = new System.Drawing.Point(0, 0);
			this.tlToolStrip.Name = "tlToolStrip";
			this.tlToolStrip.Size = new System.Drawing.Size(71, 25);
			this.tlToolStrip.TabIndex = 2;
			this.tlToolStrip.Text = "toolStrip1";
			// 
			// addTargetLineButton
			// 
			this.addTargetLineButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.addTargetLineButton.Image = ((System.Drawing.Image)(resources.GetObject("addTargetLineButton.Image")));
			this.addTargetLineButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.addTargetLineButton.Name = "addTargetLineButton";
			this.addTargetLineButton.Size = new System.Drawing.Size(23, 22);
			this.addTargetLineButton.Text = "Add target line";
			this.addTargetLineButton.Click += new System.EventHandler(this.addTargetLineButton_Click);
			// 
			// addNodeButton
			// 
			this.addNodeButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.addNodeButton.Image = ((System.Drawing.Image)(resources.GetObject("addNodeButton.Image")));
			this.addNodeButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.addNodeButton.Name = "addNodeButton";
			this.addNodeButton.Size = new System.Drawing.Size(23, 22);
			this.addNodeButton.Text = "Create new node";
			this.addNodeButton.Click += new System.EventHandler(this.addNodeButton_Click);
			// 
			// deleteButton
			// 
			this.deleteButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.deleteButton.Image = ((System.Drawing.Image)(resources.GetObject("deleteButton.Image")));
			this.deleteButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.deleteButton.Name = "deleteButton";
			this.deleteButton.Size = new System.Drawing.Size(23, 22);
			this.deleteButton.Text = "Delete";
			this.deleteButton.Click += new System.EventHandler(this.deleteButton_Click);
			// 
			// flowLayoutPanel
			// 
			this.flowLayoutPanel.Controls.Add(this.ctvControlPanel);
			this.flowLayoutPanel.Controls.Add(this.tlPanel);
			this.flowLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.flowLayoutPanel.Location = new System.Drawing.Point(0, 0);
			this.flowLayoutPanel.Margin = new System.Windows.Forms.Padding(0);
			this.flowLayoutPanel.Name = "flowLayoutPanel";
			this.flowLayoutPanel.Size = new System.Drawing.Size(119, 25);
			this.flowLayoutPanel.TabIndex = 3;
			// 
			// ctvControlPanel
			// 
			this.ctvControlPanel.CTV = null;
			this.ctvControlPanel.Location = new System.Drawing.Point(0, 0);
			this.ctvControlPanel.Margin = new System.Windows.Forms.Padding(0);
			this.ctvControlPanel.Name = "ctvControlPanel";
			this.ctvControlPanel.Size = new System.Drawing.Size(47, 25);
			this.ctvControlPanel.TabIndex = 3;
			// 
			// TargetLineCTVControlPanel
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.flowLayoutPanel);
			this.Name = "TargetLineCTVControlPanel";
			this.Size = new System.Drawing.Size(119, 25);
			this.tlPanel.ResumeLayout(false);
			this.tlToolStrip.ResumeLayout(false);
			this.tlToolStrip.PerformLayout();
			this.flowLayoutPanel.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Panel tlPanel;
		private System.Windows.Forms.ToolStrip tlToolStrip;
		private System.Windows.Forms.ToolStripButton addTargetLineButton;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel;
		protected CTVControlPanel ctvControlPanel;
		private System.Windows.Forms.ToolStripButton addNodeButton;
		private System.Windows.Forms.ToolStripButton deleteButton;

	}
}
