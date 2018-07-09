// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Xml.Serialization;

namespace Statoscope
{
	[XmlInclude(typeof(FPSTargetLineRDI))]
	public class TargetLineRDI : RecordDisplayInfo<TargetLineRDI>
	{
		public enum ELabelLocation
		{
			ELL_Left,
			ELL_Right
		}

		[XmlAttribute] public float Value;
		[XmlAttribute] public bool NameFromValue;
		[XmlAttribute] public bool ValueIsFPS;
		[XmlAttribute] public ELabelLocation LabelLocation;

		public float DisplayValue
		{
			get
			{
				float displayValue = ValueIsFPS ? (1000.0f / Value) : Value;
				return displayValue * DisplayScale;
			}
		}

		public TargetLineRDI()
		{
		}

		public TargetLineRDI(string basePath, string name)
			: base(basePath, name, 1.0f)
		{
			CanHaveChildren = true;
		}

		public TargetLineRDI(string basePath, string name, float value, RGB colour, ELabelLocation labelLocation)
			: this(basePath, name, value, colour, 1.0f, labelLocation)
		{
		}

		public TargetLineRDI(string basePath, string name, float value, RGB colour, float displayScale, ELabelLocation labelLocation)
			: base(basePath, name, displayScale)
		{
			Colour = colour;
			Value = value;
			NameFromValue = false;
			ValueIsFPS = false;
			LabelLocation = labelLocation;
		}
	}

	public class FPSTargetLineRDI : TargetLineRDI
	{
		public FPSTargetLineRDI()
		{
		}

		public FPSTargetLineRDI(string path, float value, RGB colour)
			: base(path, String.Format("{0:g}", value), value, colour, 1.0f, ELabelLocation.ELL_Left)
		{
			ValueIsFPS = true;
			NameFromValue = true;
		}
	}
}

