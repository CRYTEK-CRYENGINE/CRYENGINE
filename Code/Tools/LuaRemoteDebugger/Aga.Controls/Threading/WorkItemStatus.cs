// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace Aga.Controls.Threading
{
	public enum WorkItemStatus 
	{ 
		Completed, 
		Queued, 
		Executing, 
		Aborted 
	}
}
