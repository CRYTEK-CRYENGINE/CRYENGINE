// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace LuaRemoteDebugger
{
	partial class LuaSourceEditor
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
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(LuaSourceEditor));
			this.richTextBox = new System.Windows.Forms.RichTextBox();
			this.imageList = new System.Windows.Forms.ImageList(this.components);
			this.SuspendLayout();
			// 
			// richTextBox
			// 
			this.richTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
									| System.Windows.Forms.AnchorStyles.Left)
									| System.Windows.Forms.AnchorStyles.Right)));
			this.richTextBox.BackColor = System.Drawing.Color.White;
			this.richTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.richTextBox.EnableAutoDragDrop = true;
			this.richTextBox.Font = new System.Drawing.Font("Courier New", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.richTextBox.HideSelection = false;
			this.richTextBox.Location = new System.Drawing.Point(56, 3);
			this.richTextBox.Name = "richTextBox";
			this.richTextBox.ReadOnly = true;
			this.richTextBox.Size = new System.Drawing.Size(570, 481);
			this.richTextBox.TabIndex = 0;
			this.richTextBox.Text = "";
			this.richTextBox.WordWrap = false;
			this.richTextBox.VScroll += new System.EventHandler(this.richTextBox_VScroll);
			this.richTextBox.HScroll += new System.EventHandler(this.richTextBox_HScroll);
			this.richTextBox.MouseMove += new System.Windows.Forms.MouseEventHandler(this.richTextBox_MouseMove);
			this.richTextBox.MouseLeave += new System.EventHandler(this.richTextBox_MouseLeave);
			// 
			// imageList
			// 
			this.imageList.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("imageList.ImageStream")));
			this.imageList.TransparentColor = System.Drawing.Color.Transparent;
			this.imageList.Images.SetKeyName(0, "breakpoint.png");
			this.imageList.Images.SetKeyName(1, "disabled-breakpoint.png");
			this.imageList.Images.SetKeyName(2, "execution.png");
			// 
			// LuaSourceEditor
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.BackColor = System.Drawing.Color.WhiteSmoke;
			this.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.Controls.Add(this.richTextBox);
			this.DoubleBuffered = true;
			this.Name = "LuaSourceEditor";
			this.Size = new System.Drawing.Size(633, 487);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.RichTextBox richTextBox;
		private System.Windows.Forms.ImageList imageList;
	}
}
