// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CsGL.OpenGL;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Linq;
using System.ComponentModel;

namespace Statoscope
{
	public class ViewFrameRecord
	{
		public FrameRecordValues Values;		// only MA and LMs

		public ViewFrameRecord(FrameRecordPathCollection paths)
		{
			Values = new FrameRecordValues(paths);
		}

		// will have vertex buffer for bars and CTmpElems
	}

	public class LogView : Component
	{
		public LogControl m_logControl;
		public LogData m_logData;			// (potentially) restricted to a range of frame records
		public LogData m_baseLogData;	// full original as constructed (which, in turn may be a sub range of a parent log)
		public LogListItem m_logListItem;
		public FrameRecordPathCollection m_viewPaths;
		public IReadOnlyFRPC m_dataPaths { get { return m_baseLogData.Paths; } }
		public List<ViewFrameRecord> m_viewFRs;			// a subrange of m_baseViewFRs
		public List<ViewFrameRecord> m_baseViewFRs;	// the full range of view FRs, one for each FR in m_baseLogData
		public SortedList<string, ValueStats> m_valueStats = new SortedList<string, ValueStats>();

		public Dictionary<ZoneHighlighterRDI, ZoneHighlighterInstance> m_zoneHighlighterInstances = new Dictionary<ZoneHighlighterRDI, ZoneHighlighterInstance>();
		public Dictionary<string, List<uint>> m_displayLists = new Dictionary<string, List<uint>>();
		public HashSet<string> m_cdiProcessedPaths = new HashSet<string>();

		IGraphDesc[] m_graphDescs;

		public bool m_displayedOnGraph = true;
		public RGB m_singleOrdiColour = null;
		public int m_maxValidIdx = -1;			// the index of the last FR (in m_viewFRs/m_logData.FrameRecords) that has been updated by UpdateControls
		public int m_maxValidBaseIdx = -1;	// the same as m_maxValidIdx in relation to m_baseViewFRs/m_baseLogData.FrameRecords
		public string m_name;
		public UserMarker m_startUM, m_endUM;	// the usermarkers used to set the start and end frames, if any

		public LogViewSerializeState SerializeState
		{
			get { return new LogViewSerializeState(m_startUM, m_endUM, m_singleOrdiColour); }
		}

		public LogView(LogControl logControl, LogRange logRange)
		{
			m_logControl = logControl;
			m_baseLogData = logRange.m_logData;
			m_baseViewFRs = new List<ViewFrameRecord>(m_baseLogData.FrameRecords.Count);
			m_viewPaths = new FrameRecordPathCollection();
			ExpandBaseVFRs(m_baseLogData.FrameRecords.Count - 1);

			SocketLogData socketLogData = m_baseLogData as SocketLogData;

			if (socketLogData != null)
			{
				socketLogData.AddListeningLogView(this);
			}

			if (logRange.m_frr != null)
			{
				FrameRecordRange frr = logRange.m_frr;
				m_logData = new LogData(m_baseLogData, frr);
				m_viewFRs = new List<ViewFrameRecord>(m_baseViewFRs.GetRange(frr.StartIdx, frr.EndIdx - frr.StartIdx + 1));
			}
			else
			{
				m_logData = m_baseLogData;
				m_viewFRs = m_baseViewFRs;
			}

			string logName = m_logData.Name;

			int idx = logName.LastIndexOf('\\') + 1;

			if (idx != -1)
			{
				logName = logName.Substring(idx);  // get the filename without the path
			}

			idx = logName.IndexOf(".bin");

			if (idx != -1)
			{
				logName = logName.Substring(0, idx);  // remove .bin
			}

			m_name = logName;

			// these are searched in order
			m_graphDescs = new IGraphDesc[]
			{
				new CProfileGraphDesc("/Threads/", new ProfileGraphLeafDesc("selfTimeInMS", EItemType.Float)),
				new CProfileGraphDesc("/ParticlesColliding/", new ProfileGraphLeafDesc("count", EItemType.Int)),
				new CProfileGraphDesc("/PhysEntities/", new ProfileGraphLeafDesc("time", EItemType.Float)),
				new CProfileGraphDesc("/DrawCalls/", new ProfileGraphLeafDesc("totalDrawCallCount", EItemType.Int)),
				new CProfileGraphDesc("/NetworkProfile/", new ProfileGraphLeafDesc("totalBits", EItemType.Int)),
				new CProfileGraphDesc("/TexStrm/", new ProfileGraphLeafDesc("requiredMB", EItemType.Float), new ProfileGraphLeafDesc("mip", EItemType.Float)),
				new CProfileGraphDesc("/TexPools/", new ProfileGraphLeafDesc("n", EItemType.Int)),
				new CProfileGraphDesc("/SoundInfo/", new ProfileGraphLeafDesc("count", EItemType.Int)),
				new CProfileGraphDesc("/ObjMan/", new ProfileGraphLeafDesc("sizeKB", EItemType.Float)),
				new COverviewGraphDesc("/**")
			};

			m_viewPaths.AddPath(logControl.m_prdiTree.LeafPath);
		}

		protected override void Dispose(bool disposing)
		{
			SocketLogData socketLogData = m_baseLogData as SocketLogData;

			if (socketLogData != null)
			{
				socketLogData.RemoveListeningLogView(this);
			}

			foreach (List<uint> displayListList in m_displayLists.Values)
			{
				foreach (uint displayList in displayListList)
				{
					// this crashes on exit sometimes, dunno why
					//OpenGL.glDeleteLists(displayList, 1);
				}
			}

			base.Dispose(disposing);
		}

		public int Index
		{
			get
			{
				for (int i = 0; i < m_logControl.m_logViews.Count; i++)
					if (m_logControl.m_logViews[i] == this)
					{
						return i;
					}

				return -1;
			}
		}

		public List<uint> GetDisplayLists(string path)
		{
			if (!m_displayLists.ContainsKey(path))
			{
				m_displayLists.Add(path, new List<uint>());
			}

			return m_displayLists[path];
		}

		public void DeleteDisplayLists()
		{
			foreach (List<uint> displayListList in m_displayLists.Values)
			{
				DeleteDisplayLists(displayListList);
			}
		}

		public void DeleteDisplayLists(string path)
		{
			if (m_displayLists.ContainsKey(path))
			{
				DeleteDisplayLists(m_displayLists[path]);
			}
		}

		public void DeleteDisplayListsEndingIn(string pathEnding)
		{
			foreach (string path in m_displayLists.Keys)
			{
				if (path.EndsWith(pathEnding))
				{
					DeleteDisplayLists(m_displayLists[path]);
				}
			}
		}

		void DeleteDisplayLists(List<uint> displayListList)
		{
			foreach (uint displayList in displayListList)
			{
				OpenGL.glDeleteLists(displayList, 1);
			}

			displayListList.Clear();
		}

		public void SetFrameRecordRange(int newStartIdx, int newEndIdx)
		{
			m_logData = new LogData(m_baseLogData, new FrameRecordRange(m_baseLogData.Name, newStartIdx, newEndIdx));
			m_viewFRs = new List<ViewFrameRecord>(m_baseViewFRs.GetRange(newStartIdx, newEndIdx - newStartIdx + 1));

			UpdateValidIdxs(m_maxValidBaseIdx);

			ResetValueStats();
			CalculateValueStats(newStartIdx, newEndIdx);
			m_logControl.OnLogChanged();

			DeleteDisplayLists();
			m_logControl.InvalidateGraphControl();
		}

		public FrameRecordRange GetFrameRecordRange()
		{
			if (m_logData == m_baseLogData)
			{
				return new FrameRecordRange(m_baseLogData.Name, 0, Math.Max(0, m_baseLogData.FrameRecords.Count - 1));
			}
			else
			{
				return m_logData.FrameRecordRange;
			}
		}

		public void UpdateValidIdxs(int toIdx)
		{
			FrameRecordRange frr = GetFrameRecordRange();
			m_maxValidIdx = Math.Max(0, Math.Min(toIdx - frr.StartIdx, frr.EndIdx - frr.StartIdx));
			m_maxValidBaseIdx = toIdx;
			m_logListItem.Invalidate();
		}

		public void ResetValueStats()
		{
			ResetValueStats(m_valueStats.Keys);
		}

		public void ResetValueStats(IEnumerable<string> paths)
		{
			foreach (string path in paths)
			{
				if (m_valueStats.ContainsKey(path))
				{
					m_valueStats[path].Reset();
				}
			}
		}

		public void ExpandBaseVFRs(int toIdx)
		{
			while (m_baseViewFRs.Count <= toIdx)
			{
				m_baseViewFRs.Add(new ViewFrameRecord(m_viewPaths));
			}
		}

		public void SetSingleOrdiColour(RGB rgb)
		{
			m_singleOrdiColour = rgb;
			m_logControl.InvalidateGraphControl();
		}

		interface IGraphDesc
		{
			string GetSearchPath();
		}

		class COverviewGraphDesc : IGraphDesc
		{
			public string m_searchPath;

			public COverviewGraphDesc(string searchPath)
			{
				m_searchPath = searchPath;
			}

			public string GetSearchPath()
			{
				return m_searchPath;
			}
		}

		struct ProfileGraphLeafDesc
		{
			public readonly string Name;
			public readonly EItemType Type;

			public ProfileGraphLeafDesc(string name, EItemType type)
			{
				Name = name;
				Type = type;
			}

			public override string ToString()
			{
				return Name;
			}
		}

		class CProfileGraphDesc : IGraphDesc
		{
			public string m_baseSearchPath;
			public ProfileGraphLeafDesc[] m_leafDescs;
			public int m_activeLeafDesc;

			public CProfileGraphDesc(string baseSearchPath, params ProfileGraphLeafDesc[] leafDescs)
			{
				m_baseSearchPath = baseSearchPath;
				m_leafDescs = leafDescs;
				m_activeLeafDesc = 0;
			}

			public string GetSearchPath()
			{
				return m_baseSearchPath + "**";
			}
		}

		public class CDIResult
		{
			public readonly List<ProfilerRDI> NewPRDIs = new List<ProfilerRDI>();
			public readonly List<OverviewRDI> NewORDIs = new List<OverviewRDI>();
			public readonly List<UserMarkerRDI> NewUMRDIs = new List<UserMarkerRDI>();
		}

		public CDIResult CreateDisplayInfo(int fromIdx, int toIdx)
		{
			CDIResult result = new CDIResult();

			List<string> dontAdds = new List<string>();
			dontAdds.Add("/posx");
			dontAdds.Add("/posy");
			dontAdds.Add("/posz");
			dontAdds.Add("/rotx");
			dontAdds.Add("/roty");
			dontAdds.Add("/rotz");
			dontAdds.Add("/frameTimeInS");
			dontAdds.Add("/gpuTimeInS");
			dontAdds.Add("/TexturePool");

			foreach (string path in m_dataPaths.Paths.Where(p => !m_cdiProcessedPaths.Contains(p)))
			{
				m_cdiProcessedPaths.Add(path);

				foreach (IGraphDesc graphDesc in m_graphDescs)
				{
					if (FrameRecord.PathMatchesPattern(path, graphDesc.GetSearchPath()))
					{
						string strippedPath = path.Substring(path.IndexOf('/'));

						if (!path.StartsWith("/"))
						{
							throw new Exception();
						}

						if (!dontAdds.Contains(strippedPath))
						{
							int indexOfLastSlash = path.LastIndexOf("/");
							string basePath = path.Substring(0, indexOfLastSlash);
							string name = path.Substring(indexOfLastSlash + 1);

							float displayScale = m_logControl.GetDisplayScale(strippedPath);

							if (graphDesc is CProfileGraphDesc)
							{
								ProfilerRDI prdi;

								if (m_logControl.m_prdiTree.ContainsPath(basePath))
								{
									prdi = m_logControl.m_prdiTree[basePath];
								}
								else
								{
									prdi = new ProfilerRDI(basePath, displayScale);
								}

								List<ProfilerRDI> newPRDIs = m_logControl.m_prdiTree.Add(prdi);
								result.NewPRDIs.AddRange(newPRDIs);

								// this is rather wasteful and needs rethinking
								foreach (ProfilerRDI newPRDI in newPRDIs)
								{
									CProfileGraphDesc profGD = graphDesc as CProfileGraphDesc;
									ProfileGraphLeafDesc desc = profGD.m_leafDescs[profGD.m_activeLeafDesc];
									newPRDI.SetMetaData(desc.Name, desc.Type);
								}
							}
							else
							{
								OverviewRDI ordi;

								if (m_logControl.m_ordiTree.ContainsPath(path))
								{
									ordi = m_logControl.m_ordiTree[path];
								}
								else
								{
									ordi = new OverviewRDI(path, displayScale);
								}

								// default frame length to black
								if (path == "/frameLengthInMS")
								{
									ordi.Colour = new Statoscope.RGB(0, 0, 0);
								}

								List<OverviewRDI> newORDIs = m_logControl.m_ordiTree.Add(ordi);
								result.NewORDIs.AddRange(newORDIs);
							}
						}

						break;	// find the first one that matches
					}
				}
			}

			AddValueStatsAndItemData(result.NewPRDIs, result.NewORDIs);

			lock (m_baseLogData)
			{
				for (int i = fromIdx; i <= toIdx; i++)
				{
					FrameRecord fr = m_baseLogData.FrameRecords[i];

					foreach (UserMarkerLocation userMarkerLoc in fr.UserMarkers)
					{
						UserMarkerRDI umrdi = new UserMarkerRDI(userMarkerLoc, this);
						List<UserMarkerRDI> newUMRDIs = m_logControl.m_urdiTree.Add(umrdi);
						result.NewUMRDIs.AddRange(newUMRDIs);
					}
				}
			}

			return result;
		}

		void AddValueStatsAndItemData(IEnumerable<ProfilerRDI> PRDIs, IEnumerable<OverviewRDI> ORDIs)
		{
			lock (m_baseLogData)
			{
				foreach (ProfilerRDI prdi in PRDIs)
				{
					AddValueStats(prdi.LeafPath, !prdi.IsLeaf);

					if (!prdi.IsLeaf)
					{
						m_baseLogData.SetItemMetaData(prdi.LeafPath, prdi.ItemType);
						m_viewPaths.AddPath(prdi.LeafPath);
					}
				}
			}

			foreach (OverviewRDI ordi in ORDIs)
			{
				if (ordi.IsLeaf)
				{
					AddValueStats(ordi.Path, false);
				}
			}
		}

		public void AddValueStats(string path, bool bValueIsPerLogView)
		{
			m_valueStats[path] = new ValueStats(bValueIsPerLogView);
		}

		public void CalculateDisplayedValuesAndValueStats(int fromIdx, int toIdx)
		{
			CalculateDisplayedValuesAndValueStats(fromIdx, toIdx, null);
		}

		public void CalculateDisplayedValuesAndValueStats(int fromIdx, int toIdx, IEnumerable<string> restrictToViewPaths)
		{
			lock (m_baseLogData)
			{
				CalculateDisplayedValues(fromIdx, toIdx, restrictToViewPaths);
				CalculateValueStats(fromIdx, toIdx, restrictToViewPaths);
			}
		}

		public void CalculateDisplayedValues(int fromIdx, int toIdx, IEnumerable<string> restrictToViewPaths)
		{
			int numTasks = 32;
			int numFrames = toIdx - fromIdx + 1;
			int segmentSize = (numFrames / numTasks) + 1;
			MTCounter counter = new MTCounter(numFrames);
			ProfilerRDI.ValueList vl = m_logControl.m_prdiTree.BuildValueList(m_dataPaths, m_viewPaths, restrictToViewPaths);

			for (int i = 0; i < numTasks; ++ i)
			{
				int start = fromIdx + (i * segmentSize);
				int end = Math.Min(start + segmentSize, toIdx + 1);

				if (start >= end)
				{
					break;
				}

				ThreadPool.QueueUserWorkItem((o) =>
				{
					for (int j = start; j < end; ++j)
					{
						IReadOnlyFRVs dataValues = m_baseLogData.FrameRecords[j].Values;
						FrameRecordValues viewValues = m_baseViewFRs[j].Values;
						CalcViewValues(dataValues, viewValues, vl);
					}

					counter.Increment(end - start);
				}
				                            );
			}

			counter.WaitUntilMax();
		}

		static void CalcViewValues(IReadOnlyFRVs dataValues, FrameRecordValues viewValues, ProfilerRDI.ValueList vl)
		{
			foreach (ProfilerRDI.ValueListCmd cmd in vl.Cmds)
			{
				float value = 0.0f;

				for (int i = 0, c = cmd.Count; i != c; ++i)
				{
					ProfilerRDI.ListItem di = vl.Items[cmd.StartIndex + i];
					IReadOnlyFRVs values = di.ValueIsPerLogView ? viewValues : dataValues;
					value += values[di.ValuePathIdx] * di.Scale;
				}

				if (value != 0.0f)
				{
					viewValues[cmd.ViewPathIdx] = value;
				}
				else
				{
					viewValues.Remove(cmd.ViewPathIdx);
				}
			}
		}

		void CalculateValueStats(int fromIdx, int toIdx)
		{
			CalculateValueStats(fromIdx, toIdx, null);
		}

		void CalculateValueStats(int fromIdx, int toIdx, IEnumerable<string> restrictToViewPaths)
		{
			int numTasks = 32;
			int numValueStats = m_valueStats.Count;
			int segmentSize = (numValueStats / numTasks) + 1;
			MTCounter counter = new MTCounter(numValueStats);
			GetValuesFromIndex perViewVFI = i => m_baseViewFRs[i].Values;
			GetValuesFromIndex perLogVFI = i => m_baseLogData.FrameRecords[i].Values;

			for (int i = 0; i < numTasks; ++ i)
			{
				int start = i * segmentSize;
				int end = Math.Min(start + segmentSize, numValueStats);

				if (start >= end)
				{
					break;
				}

				ThreadPool.QueueUserWorkItem((o) =>
				{
					for (int j = start; j < end; ++j)
					{
						string path = m_valueStats.Keys[j];

						if (restrictToViewPaths != null && !restrictToViewPaths.Contains(path))
						{
							continue;
						}

						ValueStats valueStats = m_valueStats.Values[j];
						IReadOnlyFRPC paths = valueStats.m_bValueIsPerLogView ? m_viewPaths : m_dataPaths;
						GetValuesFromIndex valuesFromIdx = valueStats.m_bValueIsPerLogView ? perViewVFI : perLogVFI;
						CalcSingleValueStat(valueStats, paths.GetIndexFromPath(path), fromIdx, toIdx, valuesFromIdx);
					}

					counter.Increment(end - start);
				}
				                            );
			}

			counter.WaitUntilMax();
		}

		static void CalcSingleValueStat(ValueStats valueStats, int path, int fromIdx, int toIdx, GetValuesFromIndex valuesFromIdx)
		{
			for (int i = fromIdx; i <= toIdx; i++)
			{
				IReadOnlyFRVs values = valuesFromIdx(i);

				if (values.ContainsPath(path))
				{
					valueStats.Update(values[path]);
				}
			}
		}

		public void CalculateMovingAverage(OverviewRDI ordi, int fromIdx)
		{
			CalculateMovingAverage(ordi, fromIdx, -1);
		}

		public void CalculateMovingAverage(OverviewRDI ordi, int fromIdx, int toIdx)
		{
			if (toIdx < 0)
			{
				toIdx = m_baseViewFRs.Count - 1;
			}

			if (ordi.Filtering == OverviewRDI.EFiltering.MovingAverage)
			{
				// want to recalculate any moving averages that are affected by FrameRecords[fromIdx] onwards
				int thisFromIdx = Math.Max(0, fromIdx - ordi.MANumFrames);
				string path = ordi.Path;
				string MAPath = ordi.ValuesPath;

				for (int i = thisFromIdx; i <= toIdx; i++)
				{
					if (m_baseLogData.FrameRecords[i].Values.ContainsPath(path))
					{
						m_baseViewFRs[i].Values[MAPath] = m_baseLogData.GetMovingAverage(i, ordi.MANumFrames, path);
					}
				}
			}
		}

		public void CalculateLocalMax(OverviewRDI ordi, int fromIdx)
		{
			CalculateLocalMax(ordi, fromIdx, -1);
		}

		public void CalculateLocalMax(OverviewRDI ordi, int fromIdx, int toIdx)
		{
			if (toIdx < 0)
			{
				toIdx = m_baseViewFRs.Count - 1;
			}

			if (ordi.Filtering == OverviewRDI.EFiltering.LocalMax)
			{
				string path = ordi.Path;
				string LMPath = ordi.ValuesPath;

				for (int i = fromIdx; i <= toIdx; i++)
				{
					if (m_baseLogData.FrameRecords[i].Values.ContainsPath(path))
					{
						m_baseViewFRs[i].Values[LMPath] = m_baseLogData.GetLocalMax(i, ordi.LMNumFrames, path);
					}
				}
			}
		}

		public void AddZoneHighlighterInstance(ZoneHighlighterRDI zrdi)
		{
			m_zoneHighlighterInstances.Add(zrdi, new ZoneHighlighterInstance(zrdi, this));
		}

		public void RemoveZoneHighlighterInstance(ZoneHighlighterRDI zrdi)
		{
			m_zoneHighlighterInstances.Remove(zrdi);
		}

		public void ResetZoneHighlighterInstance(ZoneHighlighterRDI zrdi)
		{
			m_zoneHighlighterInstances[zrdi].Reset();
		}

		public void UpdateZoneHighlighter(ZoneHighlighterRDI zrdi, int fromIdx, int toIdx)
		{
			m_zoneHighlighterInstances[zrdi].Update(fromIdx, toIdx);
		}
	}
}
