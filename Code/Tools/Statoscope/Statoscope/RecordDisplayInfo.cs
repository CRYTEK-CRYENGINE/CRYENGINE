// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Serialization;

namespace Statoscope
{
	[Flags]
	public enum ERDIEnumeration
	{
		Full = 0,
		OnlyLeafNodes = 1,
		OnlyDrawn = 2,
		OnlyDrawnLeafNodes = OnlyLeafNodes | OnlyDrawn,
		OnlyThis = 4
	}

	public enum ERDIPathType
	{
		Vanilla,
		Displayed,
		Num	// must be last
	}

	public class RecordDisplayInfo<T> where T : RecordDisplayInfo<T>, new()
	{
		protected string	m_cachedPath = null;
		protected string	m_basePath = "";
		protected string	m_name = "";
		protected string	m_displayName = "";
		protected RGB			m_colour = RGB.RandomHueRGB();
		protected bool		m_bColourHasBeenChanged;

		protected RecordDisplayInfo<T> m_parent;
		protected Dictionary<string, RecordDisplayInfo<T>> m_children = new Dictionary<string, RecordDisplayInfo<T>>();

		protected virtual bool UsingDefaultValues
		{
			get { return !m_bColourHasBeenChanged; }
			set { if (value) m_bColourHasBeenChanged = false; }
		}

		// e.g. "/UserMarkers/ShaderMisses/0.435.3 LCM illum"
		public string Path
		{
			get
			{
				if (m_cachedPath == null)
					m_cachedPath = BasePath + "/" + Name;
				return m_cachedPath;
			}
		}

		// e.g. "/UserMarkers/ShaderMisses"
		[XmlAttribute] [SerializeDontCopy]
		public string BasePath
		{
			get { return m_basePath; }
			set { m_basePath = value; InvalidateCachedPath(); }
		}

		// the unique name among siblings, e.g. "0.435.3 LCM illum"
		[XmlAttribute] [SerializeDontCopy]
		public string Name
		{
			get
			{
				return m_name;
			}

			set
			{
				if (m_parent != null)
				{
					m_parent.m_children.Remove(m_name);
					m_parent.AddChild(value, this);
				}

				m_name = value;
				InvalidateCachedPath();

				foreach (RecordDisplayInfo<T> child in Children)
				{
					child.BasePath = Path;
				}
			}
		}

		// the non-unique path seen in the treeview, e.g. "/UserMarkers/ShaderMisses/LCM illum"
		public string DisplayPath
		{
			get { return BasePath + "/" + DisplayName; }
		}

		// the non-unique name, e.g. "LCM illum"
		[XmlAttribute] [SerializeDontCopy]
		public string DisplayName
		{
			get { return m_displayName; }
			set { m_displayName = value; }
		}

		[XmlAttribute]	public bool		Displayed;
		[XmlAttribute]	public float	DisplayScale;	// ultimately may not want this in here if it's going to be changed
		[XmlAttribute]	public bool		CanHaveChildren;
										public IEnumerable<RecordDisplayInfo<T>> Children { get { return m_children.Values; } }
										public bool		HasChild(string name) { return m_children.ContainsKey(name); }
										public bool		IsLeaf { get { return !CanHaveChildren; } }
		
		[XmlElement]
		public RGB Colour
		{
			get { return m_colour; }
			set { m_colour = value; m_bColourHasBeenChanged = true; }
		}

		public virtual bool ShouldSerialize
		{
			get { return (Displayed || !UsingDefaultValues); }
		}

		public IEnumerable<T> Ancestors()
		{
			if (m_parent == null)
				yield break;

			yield return (T)m_parent;

			foreach (T ancestor in m_parent.Ancestors())
				yield return ancestor;
		}

		public RecordDisplayInfo()
			: this("", "", 1.0f)
		{
			CanHaveChildren = true;
		}

		public RecordDisplayInfo(string path, float displayScale)
			: this(path.BasePath(), path.Name(), displayScale)
		{
		}

		public RecordDisplayInfo(string basePath, string name, float displayScale)
		{
			BasePath = basePath;
			Name = name;
			DisplayName = name;
			DisplayScale = displayScale;
			Displayed = true;
			CanHaveChildren = false;
			UsingDefaultValues = true;
		}

		public static T CreateNewRDI(string basePath, string name)
		{
			T newRDI = new T();
			newRDI.BasePath = basePath;
			newRDI.Name = name;
			newRDI.DisplayName = name;
			return newRDI;
		}

		public IEnumerable<T> GetEnumerator(ERDIEnumeration enumType)
		{
			if (((enumType & ERDIEnumeration.OnlyThis) == ERDIEnumeration.OnlyThis) ||
					((enumType & ERDIEnumeration.OnlyLeafNodes) != ERDIEnumeration.OnlyLeafNodes) ||
					IsLeaf)
			{
				if (Displayed || ((enumType & ERDIEnumeration.OnlyDrawn) != ERDIEnumeration.OnlyDrawn))
				{
					yield return (T)this;
				}
			}

			if (((enumType & ERDIEnumeration.OnlyThis) != ERDIEnumeration.OnlyThis))
			{
				foreach (T child in Children)
				{
					if (child.Displayed || ((enumType & ERDIEnumeration.OnlyDrawn) != ERDIEnumeration.OnlyDrawn))
					{
						foreach (T rdi in child.GetEnumerator(enumType))
						{
							yield return rdi;
						}
					}
				}
			}
		}

		public T[] GetSerializeableArray()
		{
			return GetEnumerator(ERDIEnumeration.Full).Where(rdi => rdi.ShouldSerialize).ToArray();
		}

		public bool ContainsPath(string path)
		{
			RecordDisplayInfo<T> rdi = this;

			if (path.Length > 0)
			{
				string[] locations = path.Split('/');
				foreach (string location in locations)
				{
					if (location.Length == 0)
					{
						continue;
					}

					if (!rdi.m_children.ContainsKey(location))
					{
						return false;
					}

					rdi = rdi.m_children[location];
				}
			}

			return true;
		}

		public T this[string path]
		{
			get
			{
				RecordDisplayInfo<T> rdi = this;

				if (path.Length > 0)
				{
					string[] locations = path.Split('/');
					foreach (string location in locations)
					{
						if (location.Length == 0)
						{
							continue;
						}

						if (!rdi.m_children.ContainsKey(location))
						{
							return null;
						}

						rdi = rdi.m_children[location];
					}
				}

				return (T)rdi;
			}
		}

		public List<T> Add(T rdiToAdd)
		{
			string[] locations = rdiToAdd.GetPathLocations(ERDIPathType.Vanilla);
			string name = locations[locations.Length - 1];
			List<T> rdis = new List<T>();

			RecordDisplayInfo<T> rdiNode = this;
			string pathSoFar = "";

			for (int i = 0; i < locations.Length - 1; i++)
			{
				string location = locations[i];
				T rdi;

				if (rdiNode.m_children.ContainsKey(location))
				{
					rdi = (T)rdiNode.m_children[location];
				}
				else
				{
					rdi = CreateNewRDI(pathSoFar, location);
					rdiNode.AddChild(location, rdi);
				}

				rdis.Add(rdi);
				pathSoFar += "/" + location;
				rdiNode = rdiNode.m_children[location];
			}

			if (!rdiNode.m_children.ContainsKey(name))
				rdiNode.AddChild(name, rdiToAdd);

			rdis.Add(rdiToAdd);

			return rdis;
		}

		public List<T> Remove(T rdiToRemove)
		{
			List<T> removedChildren = new List<T>();

			foreach (T childRDI in rdiToRemove.Children.ToArray())	// using ToArray() as Children will be modified in the loop
				removedChildren.AddRange(Remove(childRDI));

			rdiToRemove.m_parent.RemoveChild(rdiToRemove.Name);
			removedChildren.Add(rdiToRemove);

			return removedChildren;
		}

		void AddChild(string name, RecordDisplayInfo<T> child)
		{
			if (!CanHaveChildren)
				throw new Exception(string.Format("Trying to add {0}/{1} to {2}, but CanHaveChildren is false", child.BasePath, name, Path));
			child.m_parent = this;
			m_children.Add(name, child);
		}

		void RemoveChild(string name)
		{
			m_children.Remove(name);
		}

		public string GetNewChildName(string name)
		{
			int i = 1;
			while (HasChild(name + i))
				i++;
			return name + i;
		}

		public string GetNewName()
		{
			int i = 1;
			while (m_parent.HasChild(Name + i))
				i++;
			return Name + i;
		}

		// Vanilla:		Path				e.g. "/UserMarkers/ShaderMisses/0.435.3 LCM illum"
		// Displayed: DisplayPath	e.g. "/UserMarkers/ShaderMisses/LCM illum"
		public string[] GetPathLocations(ERDIPathType type)
		{
			string[] baseLocations = BasePath.PathLocations();
			string[] locations = new string[baseLocations.Length + 1];
			baseLocations.CopyTo(locations, 0);

			switch (type)
			{
				case ERDIPathType.Vanilla:
					if (Name == "")
						return baseLocations;
					locations[locations.Length - 1] = Name;
					break;
				case ERDIPathType.Displayed:
					if (DisplayName == "")
						return baseLocations;
					locations[locations.Length - 1] = DisplayName;
					break;
			}

			return locations;
		}

		protected virtual void InvalidateCachedPath()
		{
			m_cachedPath = null;
		}
	}
}