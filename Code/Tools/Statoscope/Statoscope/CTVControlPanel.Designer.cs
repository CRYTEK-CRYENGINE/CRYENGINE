// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class CTVControlPanel
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(CTVControlPanel));
			this.toolStrip = new System.Windows.Forms.ToolStrip();
			this.undoButton = new System.Windows.Forms.ToolStripButton();
			this.redoButton = new System.Windows.Forms.ToolStripButton();
			this.separatorItem = new System.Windows.Forms.ToolStripSeparator();
			this.filterText = new System.Windows.Forms.ToolStripTextBox();
			this.toolStrip.SuspendLayout();
			this.SuspendLayout();
			// 
			// toolStrip
			// 
			this.toolStrip.Dock = System.Windows.Forms.DockStyle.Fill;
			this.toolStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
			this.toolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.undoButton,
            this.redoButton,
            this.separatorItem,
            this.filterText});
			this.toolStrip.Location = new System.Drawing.Point(0, 0);
			this.toolStrip.Name = "toolStrip";
			this.toolStrip.TabIndex = 0;
			this.toolStrip.Text = "toolStrip1";
			// 
			// undoButton
			// 
			this.undoButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.undoButton.Image = ((System.Drawing.Image)(resources.GetObject("undoButton.Image")));
			this.undoButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.undoButton.Name = "undoButton";
			this.undoButton.Size = new System.Drawing.Size(23, 22);
			this.undoButton.Text = "Undo check state";
			this.undoButton.Click += new System.EventHandler(this.undoButton_Click);
			// 
			// redoButton
			// 
			this.redoButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.redoButton.Image = ((System.Drawing.Image)(resources.GetObject("redoButton.Image")));
			this.redoButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.redoButton.Name = "redoButton";
			this.redoButton.Size = new System.Drawing.Size(23, 22);
			this.redoButton.Text = "Redo check state";
			this.redoButton.Click += new System.EventHandler(this.redoButton_Click);
			// 
			// separatorItem
			// 
			this.separatorItem.Name = "separatorItem";
			this.separatorItem.Size = new System.Drawing.Size(6, 25);
			// 
			// filterText
			// 
			this.filterText.Name = "filterText";
			this.filterText.Size = new System.Drawing.Size(200, 25);
			this.filterText.TextChanged += new System.EventHandler(this.filterText_TextChanged);
			// 
			// CTVControlPanel
			// 
			this.Dock = System.Windows.Forms.DockStyle.Fill;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.toolStrip);
			this.Name = "CTVControlPanel";
			this.Size = new System.Drawing.Size(250, 25);
			this.toolStrip.ResumeLayout(false);
			this.toolStrip.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.ToolStrip toolStrip;
		private System.Windows.Forms.ToolStripButton undoButton;
		private System.Windows.Forms.ToolStripButton redoButton;
		private System.Windows.Forms.ToolStripSeparator separatorItem;
		private System.Windows.Forms.ToolStripTextBox filterText;
	}
}
