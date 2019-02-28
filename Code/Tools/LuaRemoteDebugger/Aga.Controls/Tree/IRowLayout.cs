// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;

namespace Aga.Controls.Tree
{
	internal interface IRowLayout
	{
		int PreferredRowHeight
		{
			get;
			set;
		}

		int PageRowCount
		{
			get;
		}

		int CurrentPageSize
		{
			get;
		}

		Rectangle GetRowBounds(int rowNo);

		int GetRowAt(Point point);

		int GetFirstRow(int lastPageRow);

		void ClearCache();
	}
}
