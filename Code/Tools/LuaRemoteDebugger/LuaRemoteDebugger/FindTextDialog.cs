// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace LuaRemoteDebugger
{
	public partial class FindTextDialog : Form
	{
		[Category("Action")]
		public event EventHandler<FindTextEventArgs> FindText;

		public RichTextBoxFinds FindOptions
		{
			get
			{
				RichTextBoxFinds findOptions = RichTextBoxFinds.None;
				if (checkBoxMatchCase.Checked)
				{
					findOptions |= RichTextBoxFinds.MatchCase;
				}
				if (checkBoxWholeWord.Checked)
				{
					findOptions |= RichTextBoxFinds.WholeWord;
				}
				if (checkBoxSearchUp.Checked)
				{
					findOptions |= RichTextBoxFinds.Reverse;
				}
				return findOptions;
			}
			set
			{
				checkBoxMatchCase.Checked = (value & RichTextBoxFinds.MatchCase) != 0;
				checkBoxWholeWord.Checked = (value & RichTextBoxFinds.WholeWord) != 0;
				checkBoxSearchUp.Checked = (value & RichTextBoxFinds.Reverse) != 0;
			}
		}

		public FindTextDialog()
		{
			InitializeComponent();
		}

		private void buttonCancel_Click(object sender, EventArgs e)
		{
			Close();
		}

		private void buttonFindNext_Click(object sender, EventArgs e)
		{
			textBoxFind.SelectionStart = 0;
			textBoxFind.SelectionLength = textBoxFind.TextLength;
			if (FindText != null)
			{
				FindText(this, new FindTextEventArgs(textBoxFind.Text, FindOptions));
			}
		}
	}

	public class FindTextEventArgs : EventArgs
	{
		string text;
		RichTextBoxFinds options;

		public FindTextEventArgs(string text, RichTextBoxFinds options) { this.text = text; this.options = options; }

		public string Text { get { return text; } }
		public RichTextBoxFinds Options { get { return options; } }
	}
}
