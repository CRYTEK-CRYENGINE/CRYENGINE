// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
	public interface IReadOnlyFRVs
	{
    bool IsEmpty { get; }
    IEnumerable<string> Paths { get; }

    bool ContainsPath(int path);
    bool ContainsPath(string path);

		float this[int pathId] { get; }
		float this[string path] { get; }
	}

	public class FrameRecordValues : IReadOnlyFRVs
	{
    private readonly FrameRecordPathCollection m_paths;
    private readonly List<float> m_values = new List<float>();
    private readonly List<UInt16> m_pathIds = new List<UInt16>();

    public bool IsEmpty
    {
      get
      {
        return m_pathIds.Count == 0;
      }
    }

    public IEnumerable<string> Paths
    {
      get
      {
				for (int i = 0, c = m_pathIds.Count; i != c; ++i)
				{
					int idx = m_pathIds[i];
					yield return m_paths.GetPathFromIndex(idx);
				}
      }
    }

    public FrameRecordValues(FrameRecordPathCollection paths)
    {
      m_paths = paths;
    }

    public FrameRecordValues(FrameRecordPathCollection paths, FrameRecordValues other)
    {
			m_paths = paths;

			// add all in one go so that LogView.CreateDisplayInfo doesn't act on a partial path list
			m_paths.AddPaths(other.Paths);

			for (int i = 0; i < other.m_pathIds.Count; i++)
			{
				string path = other.m_paths.GetPathFromIndex(other.m_pathIds[i]);
				this[path] = other.m_values[i];
			}
    }

    public void Remove(int idx)
    {
      int i = m_pathIds.BinarySearch((ushort)idx);
      if (i >= 0)
      {
        m_pathIds.RemoveAt(i);
        m_values.RemoveAt(i);
      }
    }

    public void Remove(string path)
    {
			int pathId = m_paths.TryGetIndexFromPath(path);
			if (pathId != -1)
				Remove(pathId);
    }

    public bool ContainsPath(int path)
    {
      return m_pathIds.BinarySearch((ushort)path) >= 0;
    }

    public bool ContainsPath(string path)
    {
      int idx = m_paths.TryGetIndexFromPath(path);
      return (idx != -1) && ContainsPath(idx);
    }

		public float this[int pathId]
		{
			get
      {
        int i = m_pathIds.BinarySearch((ushort)pathId);
        if (i >= 0)
          return m_values[i];
        return 0;
      }

			set
      {
        int i = m_pathIds.BinarySearch((ushort)pathId);
        if (i >= 0)
        {
          m_values[i] = value;
        }
        else
        {
          if (m_pathIds.Count == m_pathIds.Capacity)
          {
            m_pathIds.Capacity = m_pathIds.Capacity + m_pathIds.Capacity / 2;
            m_values.Capacity = m_pathIds.Capacity;
          }

          m_pathIds.Insert(~i, (ushort)pathId);
          m_values.Insert(~i, value);
        }
			}
		}

		public float this[string path]
		{
			get
      {
        int pathId = m_paths.TryGetIndexFromPath(path);
				if (pathId != -1)
					return this[pathId];
				else
					return 0.0f;
      }

			set
      {
        int pathId = m_paths.TryGetIndexFromPath(path);
				if (pathId == -1)
					pathId = m_paths.AddPath(path);
        this[pathId] = value;
      }
		}
	}
}
