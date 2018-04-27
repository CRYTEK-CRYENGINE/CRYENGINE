// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace Aga.Controls.Tree.NodeControls
{
	public class LabelEventArgs : EventArgs
	{
		private object _subject;
		public object Subject
		{
			get { return _subject; }
		}

		private string _oldLabel;
		public string OldLabel
		{
			get { return _oldLabel; }
		}

		private string _newLabel;
		public string NewLabel
		{
			get { return _newLabel; }
		}

		public LabelEventArgs(object subject, string oldLabel, string newLabel)
		{
			_subject = subject;
			_oldLabel = oldLabel;
			_newLabel = newLabel;
		}
	}
}
