namespace CryEngine
{
	partial class ExceptionMessage
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
			this.label1 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.uxStackTextbox = new System.Windows.Forms.TextBox();
			this.uxContinueBtn = new System.Windows.Forms.Button();
			this.uxCancelBtn = new System.Windows.Forms.Button();
			this.uxReportBtn = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 9);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(401, 39);
			this.label1.TabIndex = 0;
			this.label1.Text = "An unhandled exception occurred!\r\n\r\nIf you believe this is an engine bug, please " +
	"report it, including the stack trace below.";
			// 
			// label3
			// 
			this.label3.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
			| System.Windows.Forms.AnchorStyles.Left)));
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(12, 80);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(59, 13);
			this.label3.TabIndex = 2;
			this.label3.Text = "Stacktrace";
			// 
			// uxStackTextbox
			// 
			this.uxStackTextbox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
			| System.Windows.Forms.AnchorStyles.Left)
			| System.Windows.Forms.AnchorStyles.Right)));
			this.uxStackTextbox.Location = new System.Drawing.Point(12, 96);
			this.uxStackTextbox.Multiline = true;
			this.uxStackTextbox.Name = "uxStackTextbox";
			this.uxStackTextbox.ReadOnly = true;
			this.uxStackTextbox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
			this.uxStackTextbox.Size = new System.Drawing.Size(500, 163);
			this.uxStackTextbox.TabIndex = 3;
			// 
			// uxContinueBtn
			// 
			this.uxContinueBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.uxContinueBtn.Location = new System.Drawing.Point(354, 265);
			this.uxContinueBtn.Name = "uxContinueBtn";
			this.uxContinueBtn.Size = new System.Drawing.Size(75, 23);
			this.uxContinueBtn.TabIndex = 4;
			this.uxContinueBtn.Text = "Continue";
			this.uxContinueBtn.UseVisualStyleBackColor = true;
			// 
			// uxCancelBtn
			// 
			this.uxCancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.uxCancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.uxCancelBtn.Location = new System.Drawing.Point(435, 265);
			this.uxCancelBtn.Name = "uxCancelBtn";
			this.uxCancelBtn.Size = new System.Drawing.Size(75, 23);
			this.uxCancelBtn.TabIndex = 5;
			this.uxCancelBtn.Text = "Exit";
			this.uxCancelBtn.UseVisualStyleBackColor = true;
			// 
			// uxReportBtn
			// 
			this.uxReportBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.uxReportBtn.Location = new System.Drawing.Point(15, 265);
			this.uxReportBtn.Name = "uxReportBtn";
			this.uxReportBtn.Size = new System.Drawing.Size(75, 23);
			this.uxReportBtn.TabIndex = 6;
			this.uxReportBtn.Text = "Report Bug";
			this.uxReportBtn.UseVisualStyleBackColor = true;
			// 
			// ExceptionMessage
			// 
			this.AcceptButton = this.uxContinueBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.uxCancelBtn;
			this.ClientSize = new System.Drawing.Size(522, 300);
			this.ControlBox = false;
			this.Controls.Add(this.uxReportBtn);
			this.Controls.Add(this.uxCancelBtn);
			this.Controls.Add(this.uxContinueBtn);
			this.Controls.Add(this.uxStackTextbox);
			this.Controls.Add(this.label3);
			this.Controls.Add(this.label1);
			this.Name = "ExceptionMessage";
			this.Text = ".NET Exception Handled";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.TextBox uxStackTextbox;
		private System.Windows.Forms.Button uxContinueBtn;
		private System.Windows.Forms.Button uxCancelBtn;
		private System.Windows.Forms.Button uxReportBtn;
	}
}