// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class ConnectMessageBox
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
      this.components = new System.ComponentModel.Container();
      System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ConnectMessageBox));
      this.textBoxIpAddress = new System.Windows.Forms.TextBox();
      this.labelIpAddress = new System.Windows.Forms.Label();
      this.buttonConnect = new System.Windows.Forms.Button();
      this.buttonCancel = new System.Windows.Forms.Button();
      this.labelTarget = new System.Windows.Forms.Label();
      this.labelPort = new System.Windows.Forms.Label();
      this.textBoxPort = new System.Windows.Forms.TextBox();
      this.logToFileCheckBox = new System.Windows.Forms.CheckBox();
      this.targetListView = new System.Windows.Forms.ListView();
      this.targetImageList = new System.Windows.Forms.ImageList(this.components);
      this.refreshButton = new System.Windows.Forms.Button();
      this.SuspendLayout();
      // 
      // textBoxIpAddress
      // 
      this.textBoxIpAddress.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.textBoxIpAddress.Location = new System.Drawing.Point(70, 148);
      this.textBoxIpAddress.Name = "textBoxIpAddress";
      this.textBoxIpAddress.Size = new System.Drawing.Size(96, 20);
      this.textBoxIpAddress.TabIndex = 3;
      this.textBoxIpAddress.TextChanged += new System.EventHandler(this.textBoxIpAddress_TextChanged);
      // 
      // labelIpAddress
      // 
      this.labelIpAddress.AutoSize = true;
      this.labelIpAddress.Location = new System.Drawing.Point(6, 151);
      this.labelIpAddress.Name = "labelIpAddress";
      this.labelIpAddress.Size = new System.Drawing.Size(58, 13);
      this.labelIpAddress.TabIndex = 2;
      this.labelIpAddress.Text = "IP Address";
      // 
      // buttonConnect
      // 
      this.buttonConnect.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
      this.buttonConnect.DialogResult = System.Windows.Forms.DialogResult.OK;
      this.buttonConnect.Location = new System.Drawing.Point(107, 192);
      this.buttonConnect.Name = "buttonConnect";
      this.buttonConnect.Size = new System.Drawing.Size(75, 23);
      this.buttonConnect.TabIndex = 4;
      this.buttonConnect.Text = "Connect";
      this.buttonConnect.UseVisualStyleBackColor = true;
      this.buttonConnect.Click += new System.EventHandler(this.buttonConnect_Click);
      // 
      // buttonCancel
      // 
      this.buttonCancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
      this.buttonCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
      this.buttonCancel.Location = new System.Drawing.Point(188, 192);
      this.buttonCancel.Name = "buttonCancel";
      this.buttonCancel.Size = new System.Drawing.Size(75, 23);
      this.buttonCancel.TabIndex = 5;
      this.buttonCancel.Text = "Cancel";
      this.buttonCancel.UseVisualStyleBackColor = true;
      // 
      // labelTarget
      // 
      this.labelTarget.AutoSize = true;
      this.labelTarget.Location = new System.Drawing.Point(4, 17);
      this.labelTarget.Name = "labelTarget";
      this.labelTarget.Size = new System.Drawing.Size(43, 13);
      this.labelTarget.TabIndex = 0;
      this.labelTarget.Text = "Targets";
      // 
      // labelPort
      // 
      this.labelPort.AutoSize = true;
      this.labelPort.Location = new System.Drawing.Point(174, 151);
      this.labelPort.Name = "labelPort";
      this.labelPort.Size = new System.Drawing.Size(26, 13);
      this.labelPort.TabIndex = 6;
      this.labelPort.Text = "Port";
      // 
      // textBoxPort
      // 
      this.textBoxPort.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.textBoxPort.Location = new System.Drawing.Point(206, 148);
      this.textBoxPort.Name = "textBoxPort";
      this.textBoxPort.Size = new System.Drawing.Size(57, 20);
      this.textBoxPort.TabIndex = 7;
      this.textBoxPort.TextChanged += new System.EventHandler(this.textBoxPort_TextChanged);
      // 
      // logToFileCheckBox
      // 
      this.logToFileCheckBox.AutoSize = true;
      this.logToFileCheckBox.Location = new System.Drawing.Point(7, 174);
      this.logToFileCheckBox.Name = "logToFileCheckBox";
      this.logToFileCheckBox.Size = new System.Drawing.Size(72, 17);
      this.logToFileCheckBox.TabIndex = 8;
      this.logToFileCheckBox.Text = "Log to file";
      this.logToFileCheckBox.UseVisualStyleBackColor = true;
      // 
      // targetListView
      // 
      this.targetListView.HideSelection = false;
      this.targetListView.LargeImageList = this.targetImageList;
      this.targetListView.Location = new System.Drawing.Point(7, 36);
      this.targetListView.MultiSelect = false;
      this.targetListView.Name = "targetListView";
      this.targetListView.Size = new System.Drawing.Size(258, 106);
      this.targetListView.SmallImageList = this.targetImageList;
      this.targetListView.TabIndex = 9;
      this.targetListView.UseCompatibleStateImageBehavior = false;
      this.targetListView.ItemActivate += new System.EventHandler(this.targetListView_ItemActivate);
      this.targetListView.SelectedIndexChanged += new System.EventHandler(this.targetListView_SelectedIndexChanged);
      // 
      // targetImageList
      // 
      this.targetImageList.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("targetImageList.ImageStream")));
      this.targetImageList.TransparentColor = System.Drawing.Color.Transparent;
      this.targetImageList.Images.SetKeyName(0, "PC");
      this.targetImageList.Images.SetKeyName(1, "X360");
      this.targetImageList.Images.SetKeyName(2, "PS3");
      this.targetImageList.Images.SetKeyName(3, "Durango");
      this.targetImageList.Images.SetKeyName(4, "Orbis");
      // 
      // refreshButton
      // 
      this.refreshButton.Location = new System.Drawing.Point(190, 7);
      this.refreshButton.Name = "refreshButton";
      this.refreshButton.Size = new System.Drawing.Size(75, 23);
      this.refreshButton.TabIndex = 10;
      this.refreshButton.Text = "Refresh";
      this.refreshButton.UseVisualStyleBackColor = true;
      this.refreshButton.Click += new System.EventHandler(this.refreshButton_Click);
      // 
      // ConnectMessageBox
      // 
      this.AcceptButton = this.buttonConnect;
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.CancelButton = this.buttonCancel;
      this.ClientSize = new System.Drawing.Size(272, 227);
      this.Controls.Add(this.refreshButton);
      this.Controls.Add(this.targetListView);
      this.Controls.Add(this.logToFileCheckBox);
      this.Controls.Add(this.labelPort);
      this.Controls.Add(this.textBoxPort);
      this.Controls.Add(this.labelTarget);
      this.Controls.Add(this.buttonCancel);
      this.Controls.Add(this.buttonConnect);
      this.Controls.Add(this.labelIpAddress);
      this.Controls.Add(this.textBoxIpAddress);
      this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
      this.MaximizeBox = false;
      this.MinimizeBox = false;
      this.Name = "ConnectMessageBox";
      this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
      this.Text = "Connect";
      this.ResumeLayout(false);
      this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox textBoxIpAddress;
		private System.Windows.Forms.Label labelIpAddress;
		private System.Windows.Forms.Button buttonConnect;
		private System.Windows.Forms.Button buttonCancel;
		private System.Windows.Forms.Label labelTarget;
		private System.Windows.Forms.Label labelPort;
		private System.Windows.Forms.TextBox textBoxPort;
		public System.Windows.Forms.CheckBox logToFileCheckBox;
		public System.Windows.Forms.ListView targetListView;
		private System.Windows.Forms.ImageList targetImageList;
		private System.Windows.Forms.Button refreshButton;
	}
}