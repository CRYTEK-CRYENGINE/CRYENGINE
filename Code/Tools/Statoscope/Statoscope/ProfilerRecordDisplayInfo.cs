// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Serialization;

namespace Statoscope
{
	public class ProfilerRDI : RecordDisplayInfo<ProfilerRDI>
	{
		protected string m_leafName = "";
		protected string m_cachedLeafPath = null;

		[XmlAttribute] public bool IsCollapsed = false;
		[XmlAttribute] public EItemType ItemType = EItemType.NotSet;

		[XmlAttribute]
		public string LeafName
		{
			get { return m_leafName; }
			set { m_leafName = value; InvalidateCachedLeafPath(); }
		}

		public string LeafPath
		{
			get
			{
				if (m_cachedLeafPath == null)
					m_cachedLeafPath = Path + "/" + LeafName;
				return m_cachedLeafPath;
			}
		}

		protected override bool UsingDefaultValues
		{
			get
			{
				return base.UsingDefaultValues && !IsCollapsed;
			}

			set
			{
				base.UsingDefaultValues = value;
				if (value)
					IsCollapsed = false;
			}
		}

		public ProfilerRDI()
		{
		}

		public ProfilerRDI(string path, float displayScale)
			: base(path, displayScale)
		{
		}

		public void SetMetaData(string leafName, EItemType itemType)
		{
			LeafName = leafName;
			ItemType = itemType;

			foreach (ProfilerRDI child in Children)
			{
				child.SetMetaData(leafName, itemType);
			}
		}

		private void InvalidateCachedLeafPath()
		{
			m_cachedLeafPath = null;
		}

		protected override void InvalidateCachedPath()
		{
			base.InvalidateCachedPath();
			InvalidateCachedLeafPath();
		}

		private float GetDrawnTotal(FrameRecord fr)
		{
			if (IsLeaf)
			{
				return fr.Values[LeafPath] * DisplayScale;
			}
			else
			{
				float total = 0.0f;

				foreach (ProfilerRDI child in Children)
				{
					if (child.Displayed)
					{
						total += child.GetDrawnTotal(fr);
					}
				}

				return total;
			}
		}

		public float AddElementValuesToList(FrameRecord fr, List<RDIElementValue<ProfilerRDI>> elementValues, List<RDIElementValue<ProfilerRDI>> nonLeafElementValues)
		{
			float value;
			bool isLeafNode = IsLeaf;

			if (isLeafNode)
			{
				value = fr.Values[LeafPath] * DisplayScale;
			}
			else
			{
				value = 0.0f;

				foreach (ProfilerRDI childRdi in Children)
				{
					if (childRdi.Displayed)
					{
						if (IsCollapsed)
						{
							value += childRdi.GetDrawnTotal(fr);
						}
						else
						{
							value += childRdi.AddElementValuesToList(fr, elementValues, nonLeafElementValues);
						}
					}
					else if (nonLeafElementValues != null)
					{
						childRdi.AddElementValuesToList(fr, null, nonLeafElementValues);
					}
				}
			}

			if (elementValues != null && (isLeafNode || IsCollapsed))
			{
				elementValues.Add(new RDIElementValue<ProfilerRDI>(this, value));
			}

			if (nonLeafElementValues != null && (!isLeafNode || IsCollapsed))
			{
				nonLeafElementValues.Add(new RDIElementValue<ProfilerRDI>(this, value));
			}

			return value;
		}

		private void FillListItems(List<ListItem> items, IReadOnlyFRPC dataPaths)
		{
			if (IsLeaf)
			{
				int pathIdx = dataPaths.TryGetIndexFromPath(LeafPath);
				if (pathIdx != -1)
					items.Add(new ListItem(false, pathIdx, DisplayScale));
			}
			else
			{
				foreach (ProfilerRDI child in Children)
					if (child.Displayed)
						child.FillListItems(items, dataPaths);
			}
		}

		private void FillLeafDisplayList(DisplayList dl, IReadOnlyFRPC dataPaths)
		{
			bool isLeafNode = IsLeaf;

			if (isLeafNode)
			{
				int pathIdx = dataPaths.TryGetIndexFromPath(LeafPath);
				if (pathIdx != -1)
				{
					dl.Cmds.Add(new DisplayListCmd(dl.Items.Count, 1, this));
					dl.Items.Add(new ListItem(false, pathIdx, DisplayScale));
				}
			}
			else if (IsCollapsed)
			{
				int firstItem = dl.Items.Count;
				FillListItems(dl.Items, dataPaths);

				int numItems = dl.Items.Count - firstItem;
				if (numItems > 0)
					dl.Cmds.Add(new DisplayListCmd(firstItem, numItems, this));
			}
			else
			{
				foreach (ProfilerRDI childRdi in Children)
				{
					if (childRdi.Displayed)
						childRdi.FillLeafDisplayList(dl, dataPaths);
				}
			}
		}

		public DisplayList BuildDisplayList(IReadOnlyFRPC dataPaths)
		{
			DisplayList dl = new DisplayList();
			FillLeafDisplayList(dl, dataPaths);
			return dl;
		}

		private void FillNonLeafValueList(ValueList vl, IReadOnlyFRPC dataPaths, IReadOnlyFRPC viewPaths, IEnumerable<string> restrictToViewPaths)
		{
			if (!IsLeaf && (restrictToViewPaths == null || restrictToViewPaths.Contains(LeafPath)))
			{
				int leafViewPathIdx = viewPaths.TryGetIndexFromPath(LeafPath);
				if (leafViewPathIdx != -1)	// if the LeafPath isn't present in viewPaths, none of the children will be either
				{
					if (!IsCollapsed)
					{
						// Make sure that all non-leaf children are computed first
						foreach (ProfilerRDI childRdi in Children)
						{
							if (!childRdi.IsLeaf)
								childRdi.FillNonLeafValueList(vl, dataPaths, viewPaths, restrictToViewPaths);
						}

						// Produce a cmd that accumulates all children - their values should now be valid
						int firstItem = vl.Items.Count;
						{
							foreach (ProfilerRDI childRdi in Children)
							{
								if (childRdi.Displayed)
								{
									IReadOnlyFRPC paths = childRdi.IsLeaf ? dataPaths : viewPaths;
									int pathIdx = paths.TryGetIndexFromPath(childRdi.LeafPath);
									if (pathIdx != -1)
										vl.Items.Add(new ListItem(!childRdi.IsLeaf, pathIdx, childRdi.DisplayScale));
								}
							}
						}

						int numItems = vl.Items.Count - firstItem;
						vl.Cmds.Add(new ValueListCmd(firstItem, numItems, leafViewPathIdx));
					}
					else
					{
						int firstItem = vl.Items.Count;
						FillListItems(vl.Items, dataPaths);

						int numItems = vl.Items.Count - firstItem;
						vl.Cmds.Add(new ValueListCmd(firstItem, numItems, leafViewPathIdx));
					}
				}
			}
		}

		public ValueList BuildValueList(IReadOnlyFRPC dataPaths, IReadOnlyFRPC viewPaths, IEnumerable<string> restrictToViewPaths)
		{
			ValueList vl = new ValueList();
			FillNonLeafValueList(vl, dataPaths, viewPaths, restrictToViewPaths);
			return vl;
		}

		public IEnumerable<RDIElementValue<ProfilerRDI>> GetValueEnumerator(FrameRecord fr)
		{
			List<RDIElementValue<ProfilerRDI>> elementValues = new List<RDIElementValue<ProfilerRDI>>();

			AddElementValuesToList(fr, elementValues, null);
			return elementValues;
		}

		public class ListCmd
		{
			public int StartIndex;
			public int Count;

			public ListCmd(int startIndex, int count)
			{
				StartIndex = startIndex;
				Count = count;
			}
		}

		public class DisplayListCmd : ListCmd
		{
			public ProfilerRDI PRDI;
			public uint Colour;

			public DisplayListCmd(int startIndex, int count, ProfilerRDI prdi)
				: base(startIndex, count)
			{
				PRDI = prdi;
				Colour = prdi.Colour.ConvertToArgbUint();
			}
		}

		public class ValueListCmd : ListCmd
		{
			public int ViewPathIdx;

			public ValueListCmd(int startIndex, int count, int viewPathIdx)
				: base(startIndex, count)
			{
				ViewPathIdx = viewPathIdx;
			}
		}

		public struct ListItem
		{
			public bool ValueIsPerLogView;
			public int ValuePathIdx;	// indexing into either FrameRecord (false) or ViewFrameRecord (true) values depending on ValueIsPerLogView
			public float Scale;

			public ListItem(bool valueIsPerLogView, int valuePathIdx, float scale)
			{
				ValueIsPerLogView = valueIsPerLogView;
				ValuePathIdx = valuePathIdx;
				Scale = scale;
			}
		}

		public class DisplayList
		{
			public List<DisplayListCmd> Cmds = new List<DisplayListCmd>();
			public List<ListItem> Items = new List<ListItem>();
		}

		public class ValueList
		{
			public List<ValueListCmd> Cmds = new List<ValueListCmd>();
			public List<ListItem> Items = new List<ListItem>();
		}
	}

	public class RDIElementValue<RDIType> where RDIType : RecordDisplayInfo<RDIType>, new()
	{
		public RDIType m_rdi;
		public float m_value;

		public RDIElementValue(RDIType rdi, float value)
		{
			m_rdi = rdi;
			m_value = value;
		}
	}
}
