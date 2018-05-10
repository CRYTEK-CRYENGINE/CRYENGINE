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
	public partial class GotoLineDialog : Form
	{
		public int LineNumber
		{
			get
			{
				int lineNumber = 0;
				Int32.TryParse(textBoxLineNumber.Text, out lineNumber);
				return lineNumber;
			}
		}

		public GotoLineDialog()
		{
			InitializeComponent();
		}
	}
}
