// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Text.RegularExpressions;
using System.Xml.Serialization;

namespace Statoscope
{
	public class UserMarker
	{
		[XmlAttribute] public string Path, Name;

		public UserMarker()
		{
		}

		public UserMarker(string path, string name)
		{
			Path = path;
			Name = name;
		}

		public virtual bool Matches(UserMarkerLocation uml)
		{
			return (uml.m_path == Path) && (uml.m_name == Name);
		}
	}

	class RegexUserMarker : UserMarker
	{
		Regex PathRegex, NameRegex;

		public RegexUserMarker(string path, string name)
			: base(path, name)
		{
			PathRegex = new Regex(path);
			NameRegex = new Regex(name);
		}

		public override bool Matches(UserMarkerLocation uml)
		{
			return PathRegex.IsMatch(uml.m_path) && NameRegex.IsMatch(uml.m_name);
		}
	}
}