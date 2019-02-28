// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
    partial class SpikeFinder
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
					this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
					this.label2 = new System.Windows.Forms.Label();
					this.label3 = new System.Windows.Forms.Label();
					this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
					this.label4 = new System.Windows.Forms.Label();
					this.MinSpikeTextBox = new System.Windows.Forms.TextBox();
					this.MaxSpikeTextBox = new System.Windows.Forms.TextBox();
					this.label1 = new System.Windows.Forms.Label();
					this.RefreshButton = new System.Windows.Forms.Button();
					this.highlightButton = new System.Windows.Forms.Button();
					this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
					this.tableLayoutPanel1.SuspendLayout();
					this.tableLayoutPanel2.SuspendLayout();
					this.tableLayoutPanel3.SuspendLayout();
					this.SuspendLayout();
					// 
					// tableLayoutPanel1
					// 
					this.tableLayoutPanel1.ColumnCount = 2;
					this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
					this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
					this.tableLayoutPanel1.Controls.Add(this.label2, 0, 0);
					this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
					this.tableLayoutPanel1.Name = "tableLayoutPanel1";
					this.tableLayoutPanel1.RowCount = 1;
					this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
					this.tableLayoutPanel1.Size = new System.Drawing.Size(200, 100);
					this.tableLayoutPanel1.TabIndex = 0;
					// 
					// label2
					// 
					this.label2.Anchor = System.Windows.Forms.AnchorStyles.Right;
					this.label2.AutoSize = true;
					this.label2.Location = new System.Drawing.Point(16, 37);
					this.label2.Name = "label2";
					this.label2.Size = new System.Drawing.Size(81, 26);
					this.label2.TabIndex = 0;
					this.label2.Text = "Minimum Spike Length";
					// 
					// label3
					// 
					this.label3.Anchor = System.Windows.Forms.AnchorStyles.Right;
					this.label3.AutoSize = true;
					this.label3.Location = new System.Drawing.Point(65, 31);
					this.label3.Name = "label3";
					this.label3.Size = new System.Drawing.Size(117, 13);
					this.label3.TabIndex = 1;
					this.label3.Text = "Maximum Spike Length";
					// 
					// tableLayoutPanel2
					// 
					this.tableLayoutPanel2.AutoScroll = true;
					this.tableLayoutPanel2.ColumnCount = 2;
					this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 150F));
					this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
					this.tableLayoutPanel2.Controls.Add(this.label4, 0, 1);
					this.tableLayoutPanel2.Controls.Add(this.MinSpikeTextBox, 1, 0);
					this.tableLayoutPanel2.Controls.Add(this.MaxSpikeTextBox, 1, 1);
					this.tableLayoutPanel2.Controls.Add(this.label1, 0, 0);
					this.tableLayoutPanel2.Controls.Add(this.RefreshButton, 0, 2);
					this.tableLayoutPanel2.Controls.Add(this.highlightButton, 0, 3);
					this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
					this.tableLayoutPanel2.Location = new System.Drawing.Point(3, 3);
					this.tableLayoutPanel2.Name = "tableLayoutPanel2";
					this.tableLayoutPanel2.RowCount = 4;
					this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
					this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
					this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
					this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
					this.tableLayoutPanel2.Size = new System.Drawing.Size(373, 114);
					this.tableLayoutPanel2.TabIndex = 5;
					// 
					// label4
					// 
					this.label4.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
					this.label4.Location = new System.Drawing.Point(3, 35);
					this.label4.Name = "label4";
					this.label4.Size = new System.Drawing.Size(144, 13);
					this.label4.TabIndex = 1;
					this.label4.Text = "Maximum Spike Length (ms)";
					// 
					// MinSpikeTextBox
					// 
					this.MinSpikeTextBox.Dock = System.Windows.Forms.DockStyle.Fill;
					this.MinSpikeTextBox.Location = new System.Drawing.Point(153, 3);
					this.MinSpikeTextBox.MaxLength = 10;
					this.MinSpikeTextBox.Name = "MinSpikeTextBox";
					this.MinSpikeTextBox.Size = new System.Drawing.Size(217, 20);
					this.MinSpikeTextBox.TabIndex = 2;
					this.MinSpikeTextBox.Text = "5";
					this.MinSpikeTextBox.KeyDown += new System.Windows.Forms.KeyEventHandler(this.SpikeTextBox_KeyDown);
					this.MinSpikeTextBox.Leave += new System.EventHandler(this.MinSpikeTextBox_Leave);
					// 
					// MaxSpikeTextBox
					// 
					this.MaxSpikeTextBox.Dock = System.Windows.Forms.DockStyle.Fill;
					this.MaxSpikeTextBox.Location = new System.Drawing.Point(153, 31);
					this.MaxSpikeTextBox.MaxLength = 10;
					this.MaxSpikeTextBox.Name = "MaxSpikeTextBox";
					this.MaxSpikeTextBox.Size = new System.Drawing.Size(217, 20);
					this.MaxSpikeTextBox.TabIndex = 3;
					this.MaxSpikeTextBox.Text = "500";
					this.MaxSpikeTextBox.KeyDown += new System.Windows.Forms.KeyEventHandler(this.SpikeTextBox_KeyDown);
					this.MaxSpikeTextBox.Leave += new System.EventHandler(this.MaxSpikeTextBox_Leave);
					// 
					// label1
					// 
					this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
					this.label1.Location = new System.Drawing.Point(3, 7);
					this.label1.Name = "label1";
					this.label1.Size = new System.Drawing.Size(144, 13);
					this.label1.TabIndex = 0;
					this.label1.Text = "Minimum Spike Length (ms)";
					// 
					// RefreshButton
					// 
					this.tableLayoutPanel2.SetColumnSpan(this.RefreshButton, 2);
					this.RefreshButton.Dock = System.Windows.Forms.DockStyle.Fill;
					this.RefreshButton.Location = new System.Drawing.Point(3, 59);
					this.RefreshButton.Name = "RefreshButton";
					this.RefreshButton.Size = new System.Drawing.Size(367, 22);
					this.RefreshButton.TabIndex = 4;
					this.RefreshButton.Text = "Refresh";
					this.RefreshButton.UseVisualStyleBackColor = true;
					this.RefreshButton.Click += new System.EventHandler(this.RefreshButton_Click);
					// 
					// highlightButton
					// 
					this.tableLayoutPanel2.SetColumnSpan(this.highlightButton, 2);
					this.highlightButton.Dock = System.Windows.Forms.DockStyle.Fill;
					this.highlightButton.Location = new System.Drawing.Point(3, 87);
					this.highlightButton.Name = "highlightButton";
					this.highlightButton.Size = new System.Drawing.Size(367, 24);
					this.highlightButton.TabIndex = 5;
					this.highlightButton.Text = "Highlight";
					this.highlightButton.UseVisualStyleBackColor = true;
					this.highlightButton.Click += new System.EventHandler(this.HighlightButton_Click);
					// 
					// tableLayoutPanel3
					// 
					this.tableLayoutPanel3.ColumnCount = 1;
					this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
					this.tableLayoutPanel3.Controls.Add(this.tableLayoutPanel2, 0, 0);
					this.tableLayoutPanel3.Dock = System.Windows.Forms.DockStyle.Fill;
					this.tableLayoutPanel3.Location = new System.Drawing.Point(0, 0);
					this.tableLayoutPanel3.Name = "tableLayoutPanel3";
					this.tableLayoutPanel3.RowCount = 2;
					this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 120F));
					this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
					this.tableLayoutPanel3.Size = new System.Drawing.Size(379, 409);
					this.tableLayoutPanel3.TabIndex = 7;
					// 
					// SpikeFinder
					// 
					this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
					this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
					this.AutoSize = true;
					this.Controls.Add(this.tableLayoutPanel3);
					this.Name = "SpikeFinder";
					this.Size = new System.Drawing.Size(379, 409);
					this.tableLayoutPanel1.ResumeLayout(false);
					this.tableLayoutPanel1.PerformLayout();
					this.tableLayoutPanel2.ResumeLayout(false);
					this.tableLayoutPanel2.PerformLayout();
					this.tableLayoutPanel3.ResumeLayout(false);
					this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.TextBox MinSpikeTextBox;
        private System.Windows.Forms.TextBox MaxSpikeTextBox;
        private System.Windows.Forms.Label label1;
        public System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
        private System.Windows.Forms.Button RefreshButton;
				private System.Windows.Forms.Button highlightButton;

    }
}
