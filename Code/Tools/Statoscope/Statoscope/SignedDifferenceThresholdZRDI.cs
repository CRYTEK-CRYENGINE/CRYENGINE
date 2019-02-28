// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
	class SignedDifferenceThresholdZRDI : ThresholdZRDI
	{
		// may need something to distinguish between values in FrameRecords and ViewFrameRecords
		private readonly string TypeNameUpper, TypeNameLower;

		public SignedDifferenceThresholdZRDI(string basePath, string name, RGB colour, string typeNameLower, string typeNameUpper, float minThreshold, float maxThreshold)
			: base(basePath, name, colour, minThreshold, maxThreshold)
		{
			TypeNameLower = typeNameLower;
			TypeNameUpper = typeNameUpper;
		}

		protected override float GetValue(FrameRecord fr, ViewFrameRecord vfr, LogView logView)
		{
			float lower = fr.Values[TypeNameLower];
			float upper = fr.Values[TypeNameUpper];
			return upper - lower;
		}
	}
}
