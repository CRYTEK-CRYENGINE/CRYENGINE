// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using System.Xml;
using System.Xml.Serialization;

namespace Statoscope
{
	public class SerializeState
	{
		public OverviewRDI[] ORDIs;
		public ProfilerRDI[] PRDIs;
		public UserMarkerRDI[] URDIs;
		public TargetLineRDI[] TRDIs;
		public ZoneHighlighterRDI[] ZRDIs;
		public LogViewSerializeState[] LogViewStates;

		public static SerializeState LoadFromFile(string filename)
		{
			using (Stream stream = File.OpenRead(filename))
			{
				XmlSerializer xs = new XmlSerializer(typeof(SerializeState));
				return (SerializeState)xs.Deserialize(stream);
			}
		}

		public void SaveToFile(string filename)
		{
			XmlWriterSettings settings = new XmlWriterSettings() { Indent = true };
			using (XmlWriter w = XmlWriter.Create(filename, settings))
			{
				XmlSerializer xs = new XmlSerializer(typeof(SerializeState));
				xs.Serialize(w, this);
			}
		}
	}

	public class LogViewSerializeState
	{
		public UserMarker StartUM, EndUM;
		public RGB SingleORDIColour;

		public LogViewSerializeState()
		{
		}

		public LogViewSerializeState(UserMarker startUM, UserMarker endUM, RGB singleORDIColour)
		{
			StartUM = startUM;
			EndUM = endUM;
			SingleORDIColour = singleORDIColour;
		}
	}

	class SerializeDontCopy : Attribute
	{
	}
}
