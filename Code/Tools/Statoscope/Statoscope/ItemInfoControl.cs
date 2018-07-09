// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace Statoscope
{
	partial class ItemInfoControl : UserControl
	{
		protected readonly LogControl LogControl;

		public ItemInfoControl()
		{
		}

		public ItemInfoControl(LogControl logControl)
		{
			LogControl = logControl;
		}

		public virtual void RefreshItem() {}
		public virtual void SetItem(object item) {}
		public virtual void OnLogChanged() {}
	}
}
