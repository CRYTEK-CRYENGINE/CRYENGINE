// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;

namespace Statoscope
{
	enum EFrameElementValueType
	{
		Float,
		Int
	}

	enum EFrameElementType
	{
		None = 0,
		Float,
		Int,
		String,
		B64Texture,
		Int64,
	}

	enum EEndian
	{
		LITTLE_ENDIAN = 0,
		BIG_ENDIAN
	}

	class PerfStatFrameElement
	{
		public EFrameElementType m_type;
		public string m_name;
		public string m_units;

		public PerfStatFrameElement(string type, string name, string units)
		{
			switch (type)
			{
			case "float":
				m_type = EFrameElementType.Float;
				break;

			case "int":
				m_type = EFrameElementType.Int;
				break;

			case "string":
				m_type = EFrameElementType.String;
				break;

			case "b64texture":
				m_type = EFrameElementType.B64Texture;
				break;
			}

			m_name = name;
			m_units = units;
		}

		public PerfStatFrameElement(EFrameElementType _type, string name, string units)
		{
			m_type = _type;
			m_name = name;
			m_units = units;
		}

		public static string GetRegexPatternFromFrameElementType(EFrameElementType type)
		{
			switch (type)
			{
			case EFrameElementType.Float:
			case EFrameElementType.Int:
				return @"-?\d+(\.\d+)?|1\.\#QNAN0";

			case EFrameElementType.String:
				return @"'.*?'";

			case EFrameElementType.B64Texture:
				return @"[A-Z|a-z|0-9|\+|\/]*";
			}

			return "";
		}

		public string GetRegexPattern()
		{
			return string.Format("(?<{0}>{1}) *", m_name, GetRegexPatternFromFrameElementType(m_type));
		}
	}

	struct ProfileTotalData
	{
		public FrameRecord m_fr;
		public List<string> m_paths;
		public MTCounter m_profileTotalCounter;

		public ProfileTotalData(FrameRecord fr, List<string> paths, MTCounter profileTotalCounter)
		{
			m_fr = fr;
			m_paths = paths;
			m_profileTotalCounter = profileTotalCounter;
		}
	}

	public class Callstack
	{
		public string m_tag;
		public List<string> m_functions;

		public Callstack(string tag)
		{
			m_tag = tag;
			m_functions = new List<string>();
		}
	}

	public class FrameRecord
	{
		public readonly int			Index;						// the index into the frame records of the LogData that this belongs to
		public readonly float		FrameTimeInS;			// when the frame started in seconds
		public					float		FrameMidTimeInS;	// half way between the start of this frame and the start of the next frame (or FrameTimeInS for the last frame)
		public readonly MemoryStream	ScreenshotImage;

		public readonly IReadOnlyFRVs										Values;
		public readonly IEnumerable<UserMarkerLocation> UserMarkers;
		public readonly IEnumerable<Callstack>					Callstacks;

		public FrameRecord(int index, float frameTimeInS, MemoryStream screenshotImage, IReadOnlyFRVs values, IEnumerable<UserMarker> userMarkers, IEnumerable<Callstack> callstacks)
		{
			Index = index;
			FrameTimeInS = frameTimeInS;
			FrameMidTimeInS = frameTimeInS;
			ScreenshotImage = screenshotImage;
			Values = values;

			List<UserMarkerLocation> userMarkerLocs = new List<UserMarkerLocation>();

			foreach (UserMarker userMarker in userMarkers)
			{
				UserMarkerLocation userMarkerLocation = new UserMarkerLocation(userMarker, this, userMarkerLocs.Count);
				userMarkerLocs.Add(userMarkerLocation);
			}

			UserMarkers = userMarkerLocs;

			Callstacks = callstacks;
		}

		public void CalculateFrameMidTime(FrameRecord nextFR)
		{
			FrameMidTimeInS = (this.FrameTimeInS + nextFR.FrameTimeInS) / 2.0f;
		}

		// checks if path matches pattern, delimitted by /, where a * in pattern matches any single item in path
		// and a ** in pattern matches any number of / seperated items
		// e.g. PathMatchesPattern("/a/b/c", "/a/b/c") == true
		//			PathMatchesPattern("/a/b/c", "/a/*/c") == true
		//			PathMatchesPattern("/a/b/c", "/*/c") == false
		//			PathMatchesPattern("/a/b/c", "/**/c") == true
		public static bool PathMatchesPattern(string path, string pattern)
		{
			string[] pathLocations = path.PathLocations();
			string[] patternLocations = pattern.PathLocations();
			bool containsDoubleStar = false;
			int iPattern = 0;

			for (int iPath = 0; iPath < pathLocations.Length && iPattern < patternLocations.Length; iPath++)
			{
				if (patternLocations[iPattern] == "*")
				{
					iPattern++;
					continue;
				}

				if (patternLocations[iPattern] == "**")
				{
					if (iPattern == patternLocations.Length - 1)
					{
						return true;
					}

					containsDoubleStar = true;

					if (patternLocations[iPattern + 1] == pathLocations[iPath])
					{
						iPattern++;
					}

					continue;
				}

				if (patternLocations[iPattern] != pathLocations[iPath])
				{
					return false;
				}

				iPattern++;
			}

			return (containsDoubleStar && iPattern == patternLocations.Length) || (pathLocations.Length == patternLocations.Length);
		}

		// value paths
		public IEnumerable<string> GetPathEnumerator(string pathToDo)
		{
			foreach (string path in Values.Paths)
			{
				if (PathMatchesPattern(path, pathToDo))
				{
					yield return path;
				}
			}
		}

		public float GetDisplayXValue(LogData logData, bool againstFrameTime)
		{
			if (againstFrameTime)
			{
				float startMidTime = logData.FrameRecords.First().FrameMidTimeInS;
				return FrameMidTimeInS - startMidTime;
			}
			else
			{
				return logData.GetDisplayIndex(Index) + 0.5f;
			}
		}
	}

	public delegate float GetRecordValue(FrameRecord record);
	delegate bool AcceptRecord(FrameRecord record);
	public delegate IReadOnlyFRVs GetValuesFromIndex(int i);

	class PerfStatLogLineDescriptor
	{
		public List<PerfStatFrameElement> m_formatElements = new List<PerfStatFrameElement>();
		public string m_path;
		public Regex m_regex;

		public void CreateRegex()
		{
			string formatRegexPattern = @"\[";

			foreach (PerfStatFrameElement fe in m_formatElements)
			{
				formatRegexPattern += fe.GetRegexPattern();
			}

			formatRegexPattern += @"\]";

			m_regex = new Regex(formatRegexPattern);
		}
	}

	class SocketLogData : LogData
	{
		List<LogView> LogViews = new List<LogView>();
		SocketLogBinaryDataStream SocketDataStream;
		public bool m_bTempFile = false;
		public string m_logWriteFilename = "";

		public SocketLogBinaryDataStream DataStream { set { SocketDataStream = value; } }

		public SocketLogData(SocketSessionInfo session)
		: base(session)
		{
		}

		public void SaveToFile(string fileName)
		{
			File.Copy(m_logWriteFilename, fileName, true);
		}

		public override void AddFrameRecord(FrameRecord fr, List<KeyValuePair<string, EItemType>> newMetaDatas)
		{
			lock (this)
			{
				base.AddFrameRecord(fr, newMetaDatas);
				ProcessRecords(fr.Index);
			}

			foreach (LogView logView in LogViews)
			{
				logView.m_logControl.NewRecordsAvailable();
			}

			//m_logControl.UpdateControls(fr.m_index);	// change this to a listener setup? and also only do it for the views that have this log data
		}

		public void AddListeningLogView(LogView logView)
		{
			LogViews.Add(logView);
		}

		public void RemoveListeningLogView(LogView logView)
		{
			LogViews.Remove(logView);

			if (LogViews.Count == 0)
			{
				// trigger the socket thread shutdown
				SocketDataStream.CloseNetStream();

				if (m_bTempFile && m_logWriteFilename != "")
				{
					File.Delete(m_logWriteFilename);
					m_logWriteFilename = "";
				}

				for (int i = 0; (i < 10) && SocketDataStream.FileWriterNeedsClosing; i++)
				{
					Thread.Sleep(100);
				}
			}
		}
	}

	public class FrameRecordRange
	{
		public readonly string Name;	// this is only really used for the Confluence stuff now and ideally should be removed
		public readonly int StartIdx;
		public readonly int EndIdx;

		public FrameRecordRange()
		{
			Name = "";
			StartIdx = 0;
			EndIdx = 0;
		}

		public FrameRecordRange(string name, int startIdx, int endIdx)
		{
			Name = name;
			StartIdx = startIdx;
			EndIdx = endIdx;
		}
	}

	public class LogRange
	{
		public LogData m_logData;
		public FrameRecordRange m_frr;

		public LogRange(LogData logData, FrameRecordRange frr)
		{
			m_logData = logData;
			m_frr = frr;
		}
	}

	public class UserMarkerLocation
	{
		public string m_path;
		public string m_name;
		public FrameRecord m_fr;
		public int m_index;	// the index of this UserMarkerLocation in m_fr.UserMarkers

		public UserMarkerLocation(UserMarker userMarker, FrameRecord fr, int index)
		{
			m_path = userMarker.Path;
			m_name = userMarker.Name;
			m_fr = fr;
			m_index = index;
		}

		public string GetNameForLogView(LogView logView)
		{
			return logView.Index + "." + m_fr.Index + "." + m_index + " " + m_name;
		}

		static string oldLevelStartString = "level_start ";
		static string newLevelStartString = "Start ";

		bool IsOldLevelStart()	{ return m_name.StartsWith(oldLevelStartString); }
		bool IsNewLevelStart()	{ return m_path == "/UserMarkers/Level" && m_name.StartsWith(newLevelStartString); }
		public bool IsLevelStart()			{ return IsOldLevelStart() || IsNewLevelStart(); }
		public bool IsLevelEnd()				{	return (m_name == "level_end") || ((m_path == "/UserMarkers/Level") && (m_name == "Start")); }

		public string GetLevelName()
		{
			if (IsOldLevelStart())
			{
				return m_name.Substring(oldLevelStartString.Length);
			}
			else if (IsNewLevelStart())
			{
				return m_name.Substring(newLevelStartString.Length);
			}
			else
			{
				throw new Exception("Not a level user marker");
			}
		}
	}

	public class Stats
	{
		public float m_min;
		public float m_max;
		public float m_avg;

		public Stats()
		{
			m_min = 9999999999999.0f;
			m_max = 0.0f;
			m_avg = 0.0f;
		}
	}

	public class GroupItemStats
	{
		public GroupItemStats()
		{
			m_groupItemStats = new Dictionary<string, Stats>();
		}
		public Stats GetItemStats(string item)
		{
			return m_groupItemStats[item];
		}
		public bool ContainsItem(string item)
		{
			return m_groupItemStats.ContainsKey(item);
		}
		public Dictionary<string, Stats> m_groupItemStats;
	}

	public class GroupsStats
	{
		public GroupsStats()
		{
			m_groupStats = new Dictionary<string, GroupItemStats>();
		}
		public GroupItemStats GetGroupItemsStats(string group)
		{
			return m_groupStats[group];
		}
		public bool ContainsGroup(string group)
		{
			return m_groupStats.ContainsKey(group);
		}
		public Dictionary<string, GroupItemStats> m_groupStats;
	}

	public class ValueStats
	{
		public readonly bool m_bValueIsPerLogView;
		public int m_numFrames;
		public float m_min;
		public float m_max;
		public float m_avg;
		double m_total;

		public ValueStats(bool bValueIsPerLogView)
		{
			m_bValueIsPerLogView = bValueIsPerLogView;
			Reset();
		}

		public void Reset()
		{
			m_numFrames = 0;
			m_min = float.MaxValue;
			m_max = float.MinValue;
			m_avg = 0.0f;
			m_total = 0.0;
		}

		public void Update(float value)
		{
			if (float.IsNaN(value))
			{
				return;
			}

			m_numFrames++;

			if (value < m_min)
			{
				m_min = value;
			}

			if (value > m_max)
			{
				m_max = value;
			}

			m_total += value;
			m_avg = (float)(m_total / m_numFrames);
		}
	}

	public enum EItemType
	{
		NotSet,
		Float,
		Int
	}

	public class ItemMetaData
	{
		public EItemType m_type;

		public ItemMetaData(EItemType type)
		{
			m_type = type;
		}

		public override bool Equals(object obj)
		{
			if (obj == null || GetType() != obj.GetType())
			{
				return false;
			}

			ItemMetaData imd = (ItemMetaData)(obj);
			return imd.m_type == m_type;
		}

		public override int GetHashCode()
		{
			return m_type.GetHashCode();
		}
	}

	public partial class LogData
	{
		public readonly string Name;
		public readonly List<FrameRecord> FrameRecords;
		public readonly Dictionary<string, ItemMetaData> ItemMetaData;	// things selectable in the treeview or possibly in bucket stats need meta data
		public IEnumerable<FrameRecordRange> LevelRanges { get { return m_levelRanges; } }
		public readonly FrameRecordRange FrameRecordRange;
		public readonly BuildInfo BuildInfo;	// only used for Confluence, which I'd like to remove; however, FFM may still use it
		public readonly SessionInfo SessionInfo;
		public IReadOnlyFRPC Paths { get { return m_paths; } }

		public readonly IntervalTree IntervalTree = new IntervalTree();

		readonly List<FrameRecordRange> m_levelRanges = new List<FrameRecordRange>();
		readonly FrameRecordPathCollection m_paths;

		public LogData(SessionInfo sessionInfo)
		{
			Name = sessionInfo.Summary;
			FrameRecords = new List<FrameRecord>();
			ItemMetaData = new Dictionary<string, ItemMetaData>();
			FrameRecordRange = new FrameRecordRange();
			BuildInfo = new BuildInfo(Name);
			SessionInfo = sessionInfo;
			m_paths = new FrameRecordPathCollection();
		}

		public LogData(LogData logData, FrameRecordRange frr)
		{
			int startIdx = frr.StartIdx;
			int endIdx = frr.EndIdx;
			int numFrames = endIdx - startIdx + 1;

			Name = frr.Name;
			FrameRecords = new List<FrameRecord>(logData.FrameRecords.GetRange(startIdx, numFrames));
			ItemMetaData = new Dictionary<string, ItemMetaData>(logData.ItemMetaData);	// this may not need to be copied, MT a problem though?
			FrameRecordRange = new FrameRecordRange(Name, logData.FrameRecordRange.StartIdx + startIdx,
			                                        logData.FrameRecordRange.StartIdx + endIdx);
			m_paths = logData.m_paths;
			BuildInfo = logData.BuildInfo;
			SessionInfo = logData.SessionInfo;
		}

		public FrameRecordValues CreateNewFrameRecordValues(FrameRecordValues values)
		{
			// make a new one based on m_paths
			return new FrameRecordValues(m_paths, values);
		}

		public override string ToString()
		{
			return Name;
		}

		public virtual void AddFrameRecord(FrameRecord fr, List<KeyValuePair<string, EItemType>> newMetaDatas)
		{
			for (int i = 0, c = newMetaDatas.Count; i != c; ++ i)
			{
				SetItemMetaData(newMetaDatas[i].Key, newMetaDatas[i].Value);
			}

			FrameRecords.Add(fr);
		}

		public float LogStartInSeconds
		{
			get
			{
				if (FrameRecords.Count > 0)
				{
					return FrameRecords.First().FrameTimeInS;
				}

				return 0.0f;
			}
		}

		public float LogEndInSeconds
		{
			get
			{
				if (FrameRecords.Count > 0)
				{
					return FrameRecords.Last().FrameTimeInS;
				}

				return 0.0f;
			}
		}

		public int GetDisplayIndex(int idx)
		{
			return idx - FrameRecordRange.StartIdx;
		}

		public void ProcessRecords()
		{
			ProcessRecords(0);
			CalculateLevelRanges();
		}

		public void ProcessRecords(int fromIdx)
		{
			ClipIncompleteFrames(fromIdx);
			CalculateMidTimes(fromIdx);
			CalculateBucketSets(fromIdx);
			UpdateSessionInfo();
		}

		void UpdateSessionInfo()
		{
			float startTime = (FrameRecords.Count > 0) ? FrameRecords.First().FrameTimeInS : 0.0f;
			float endTime		= (FrameRecords.Count > 0) ? FrameRecords.Last().FrameTimeInS : 0.0f;
			SessionInfo.Update(FrameRecords.Count, startTime, endTime);
		}

		protected void CalculateMidTimes(int fromIdx)
		{
			if (FrameRecords.Count == 0)
			{
				return;
			}

			for (int i = fromIdx; i < FrameRecords.Count - 1; i++)
			{
				FrameRecords[i].CalculateFrameMidTime(FrameRecords[i + 1]);
			}
		}

		public float GetMovingAverage(int frame, int numFrames, string path)
		{
			float avgValue = 0.0f;
			int startFrame = Math.Max(0, frame - numFrames);
			int endFrame = Math.Min(frame + numFrames, FrameRecords.Count);
			int numFramesPresent = 0;

			for (int i = startFrame; i < endFrame; i++)
			{
				IReadOnlyFRVs frv = FrameRecords[i].Values;

				if (frv.ContainsPath(path))
				{
					avgValue += frv[path];
					numFramesPresent++;
				}
			}

			return avgValue / numFramesPresent;
		}

		public float GetLocalMax(int frame, int numFrames, string path)
		{
			float maxValue = 0.0f;
			int startFrame = Math.Max(0, frame - numFrames);
			int endFrame = Math.Min(frame, FrameRecords.Count - 1);

			for (int i = startFrame; i <= endFrame; i++)
			{
				IReadOnlyFRVs frv = FrameRecords[i].Values;

				if (frv.ContainsPath(path))
				{
					maxValue = Math.Max(maxValue, frv[path]);
				}
			}

			return maxValue;
		}

		protected void ClipIncompleteFrames(int fromIdx)
		{
			for (int i = fromIdx; i < FrameRecords.Count; i++)
			{
				FrameRecord fr = FrameRecords[i];

				if (fr.Values.IsEmpty) //ContainsKey("/frameTimeInS"))
				{
					FrameRecords.RemoveRange(i, FrameRecords.Count - i);
					break;
				}
			}
		}

		public FrameRecord FindClosestScreenshotFrameFromTime(float x)
		{
			return FrameRecords[FindClosestFrameIdxFromTime(x, true)];
		}

		public FrameRecord FindClosestScreenshotFrameFromFrameIdx(int idx)
		{
			int belowIdx;

			for (belowIdx = idx; belowIdx >= 0; belowIdx--)
			{
				FrameRecord fr = FrameRecords[belowIdx];

				if (fr.ScreenshotImage != null)
				{
					break;
				}
			}

			int aboveIdx;

			for (aboveIdx = idx; aboveIdx < FrameRecords.Count; aboveIdx++)
			{
				FrameRecord fr = FrameRecords[aboveIdx];

				if (fr.ScreenshotImage != null)
				{
					break;
				}
			}

			belowIdx = Math.Max(0, belowIdx);
			aboveIdx = Math.Min(belowIdx, FrameRecords.Count);

			int bestIdx = (idx - belowIdx) < (aboveIdx - idx) ? belowIdx : aboveIdx;
			return FrameRecords[bestIdx];
		}

		public int FindClosestFrameIdxFromTime(float x)
		{
			return FindClosestFrameIdxFromTime(x, false);
		}

		public int FindClosestFrameIdxFromTime(float x, bool screenshotFrame)
		{
			int aboveIdx = 0;
			int belowIdx = 0;
			float startMidTime = FrameRecords.First().FrameMidTimeInS;

			for (int i = 0; i < FrameRecords.Count - 1; i++)
			{
				FrameRecord fr = FrameRecords[i];

				if (screenshotFrame && (fr.ScreenshotImage == null))
				{
					continue;
				}

				if (x < fr.FrameMidTimeInS - startMidTime)
				{
					aboveIdx = i;
					break;
				}

				belowIdx = i;
			}

			aboveIdx = Math.Max(aboveIdx, belowIdx);

			if (x > (FrameRecords[aboveIdx].FrameMidTimeInS - startMidTime + FrameRecords[belowIdx].FrameMidTimeInS - startMidTime) / 2.0f)
			{
				return aboveIdx;
			}
			else
			{
				return belowIdx;
			}
		}

		public void GetFrameTimeData(int fromFrameIdx, int toFrameIdx, out float min, out float max, out float avg)
		{
			min = float.MaxValue;
			max = 0.0f;
			avg = 0.0f;

			for (int i = fromFrameIdx; i <= toFrameIdx; i++)
			{
				FrameRecord fr = FrameRecords[i];
				float frameLengthInMS = fr.Values["/frameLengthInMS"];
				min = Math.Min(min, frameLengthInMS);
				max = Math.Max(max, frameLengthInMS);
				avg += frameLengthInMS;
			}

			avg /= (toFrameIdx - fromFrameIdx + 1);
		}

		public void GetFrameGroupData(int fromFrameIdx, int toFrameIdx, String[] groups, GroupsStats stats)
		{
			for (int i = fromFrameIdx; i <= toFrameIdx; i++)
			{
				FrameRecord fr = FrameRecords[i];

				foreach (string key in fr.Values.Paths)
				{
					foreach (string group in groups)
					{
						if (key.Contains(group))
						{
							if (stats.m_groupStats.Count == 0 || !stats.ContainsGroup(group))
							{
								stats.m_groupStats.Add(group, new GroupItemStats());
							}

							GroupItemStats groupItemStats = stats.GetGroupItemsStats(group);

							string itemKey = key.Substring(key.LastIndexOf('/') + 1);

							if (!groupItemStats.ContainsItem(itemKey))
							{
								groupItemStats.m_groupItemStats.Add(itemKey, new Stats());
							}

							Stats itemStats = groupItemStats.GetItemStats(itemKey);
							itemStats.m_min = Math.Min(itemStats.m_min, fr.Values[key]);
							itemStats.m_max = Math.Max(itemStats.m_max, fr.Values[key]);
							itemStats.m_avg += fr.Values[key];
						}
					}
				}
			}

			foreach (string group in stats.m_groupStats.Keys)
			{
				foreach (string item in stats.GetGroupItemsStats(group).m_groupItemStats.Keys)
				{
					Stats itemStats = stats.GetGroupItemsStats(group).GetItemStats(item);
					itemStats.m_avg /= (toFrameIdx - fromFrameIdx + 1);
				}
			}
		}

		public float GetPercentileValue(GetRecordValue grv, float percentile)
		{
			List<float> values = new List<float>();

			foreach (FrameRecord fr in FrameRecords)
			{
				values.Add(grv(fr));
			}

			values.Sort();

			return values[(int)((values.Count - 1) * percentile / 100.0f)];
		}

		public void CalculateLevelRanges()
		{
			FrameRecord startFR = null;
			string levelName = "unknown_level_name";

			foreach (FrameRecord fr in FrameRecords)
			{
				foreach (UserMarkerLocation userMarkerLoc in fr.UserMarkers)
				{
					if (userMarkerLoc.IsLevelStart())
					{
						if (startFR != null)
						{
							Console.WriteLine("Level start marker found (level name: '" + levelName + "') before previous end!");
						}

						startFR = fr;
						levelName = userMarkerLoc.GetLevelName().Replace("/", " - ");	// confluence doesn't like /
					}
					else if (userMarkerLoc.IsLevelEnd())
					{
						if (startFR != null)
						{
							m_levelRanges.Add(new FrameRecordRange(levelName, startFR.Index, fr.Index));
							startFR = null;
						}
						else
						{
							Console.WriteLine("Level end marker, but no start!");
						}
					}
				}
			}

			if (startFR != null)
			{
				// if there's a start with no end, use the end of the log as the range end
				m_levelRanges.Add(new FrameRecordRange(levelName, startFR.Index, FrameRecords.Count - 1));
			}
		}

		public void SetItemMetaData(string path, EItemType type)
		{
			if (ItemMetaData.ContainsKey(path))
			{
				if (ItemMetaData[path].m_type != type)
				{
					throw new Exception(string.Format("'{0}' type has changed: was {1}, now {2}", path, ItemMetaData[path].m_type, type));
				}
			}
			else
			{
				ItemMetaData[path] = new ItemMetaData(type);
			}
		}
	}

	public class BuildInfo
	{
		public readonly string PlatformString;
		public readonly string BuildNumberString;

		public BuildInfo(string filename)
		{
			Regex regex = new Regex("perflog_(?<platform>\\w*)_(?<buildNum_3>[0-9]*)_(?<buildNum_2>[0-9]*)_(?<buildNum_1>[0-9]*)_(?<buildNum_0>[0-9]*).*.log");
			Match match = regex.Match(filename);

			if (match.Success)
			{
				PlatformString = match.Groups["platform"].Value;
				BuildNumberString = match.Groups["buildNum_3"].Value + "." +
				                    match.Groups["buildNum_2"].Value + "." +
				                    match.Groups["buildNum_1"].Value + "." +
				                    match.Groups["buildNum_0"].Value;
			}
			else
			{
				PlatformString = "<unknown platform>";
				BuildNumberString = "<unknown build number>";
			}
		}
	}
}
