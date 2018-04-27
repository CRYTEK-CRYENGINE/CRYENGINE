// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace Aga.Controls.Tree
{
	public class TreeColumnEventArgs: EventArgs
	{
		private TreeColumn _column;
		public TreeColumn Column
		{
			get { return _column; }
		}

		public TreeColumnEventArgs(TreeColumn column)
		{
			_column = column;
		}
	}
}
