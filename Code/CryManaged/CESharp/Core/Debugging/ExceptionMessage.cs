using System;
using System.Diagnostics;
using System.Windows.Forms;

using CryEngine.Common;

namespace CryEngine
{
	internal partial class ExceptionMessage : Form
	{
		public ExceptionMessage(Exception ex, bool fatal)
		{
            InitializeComponent();

            uxContinueBtn.Click += OnContinueButtonPressed;
            uxReportBtn.Click += (s, a) => Process.Start("https://answers.cryengine.com/");
			uxCancelBtn.Click += (s, a) => Process.GetCurrentProcess().Kill();

			var text = "";

			if (fatal)
			{
				text += "Exceptions are currently treated as fatal errors." + Environment.NewLine;
				text += "The application cannot continue." + Environment.NewLine + Environment.NewLine;
			}

			text += ex.ToString();
			uxStackTextbox.Text = text;

			var selected = ActiveControl;
			ActiveControl = uxStackTextbox;

			uxStackTextbox.SelectionStart = MathHelpers.Clamp(0, 0, uxStackTextbox.TextLength);
			uxStackTextbox.ScrollToCaret();

			ActiveControl = selected;

			if (fatal)
				uxContinueBtn.Enabled = false;
		}

		void OnContinueButtonPressed(object sender, EventArgs e)
		{
			Global.gEnv.pHardwareMouse.UseSystemCursor(false);
			Close();
		}
	}
}