// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
	public interface IReadOnlyFRPC
	{
		int PathCount { get; }
		string[] Paths { get; }

		bool ContainsPath(string path);
		int TryGetIndexFromPath(string path);
    int GetIndexFromPath(string path);

		string GetPathFromIndex(int idx);
	}

  public class FrameRecordPathCollection : IReadOnlyFRPC
  {
    private System.Threading.ReaderWriterLockSlim m_lock = new System.Threading.ReaderWriterLockSlim();
		private Dictionary<string, int> m_pathsToIdx = new Dictionary<string, int>();
		private List<string> m_idxToPaths = new List<string>();

    public int PathCount
    {
      get { return m_idxToPaths.Count; }
    }

		public string[] Paths
		{
			get
			{
				m_lock.EnterReadLock();
				string[] paths = m_idxToPaths.ToArray();
				m_lock.ExitReadLock();
				return paths;
			}
		}

		public int AddPath(string path)
		{
      m_lock.EnterWriteLock();

			int idx;
      if (m_pathsToIdx.TryGetValue(path, out idx))
      {
        m_lock.ExitWriteLock();
        return idx;
      }

      idx = m_idxToPaths.Count;
      m_pathsToIdx.Add(path, idx);
      m_idxToPaths.Add(path);
      m_lock.ExitWriteLock();

      return idx;
		}

		public void AddPaths(IEnumerable<string> paths)
		{
			m_lock.EnterWriteLock();

			foreach (string path in paths)
			{
				int idx;
				if (!m_pathsToIdx.TryGetValue(path, out idx))
				{
					idx = m_idxToPaths.Count;
					m_pathsToIdx.Add(path, idx);
					m_idxToPaths.Add(path);
				}
			}

			m_lock.ExitWriteLock();
		}

		public bool ContainsPath(string path)
		{
			return TryGetIndexFromPath(path) != -1;
		}

		public int TryGetIndexFromPath(string path)
		{
			m_lock.EnterReadLock();

			int idx;
			if (m_pathsToIdx.TryGetValue(path, out idx))
			{
				m_lock.ExitReadLock();
				return idx;
			}

			m_lock.ExitReadLock();
			return -1;
		}

    public int GetIndexFromPath(string path)
    {
			int idx = TryGetIndexFromPath(path);
			if (idx != -1)
				return idx;
			else
				throw new Exception("Path does not exist: " + path);
    }

    public string GetPathFromIndex(int idx)
    {
      string res = string.Empty;

      m_lock.EnterReadLock();
      if (idx < m_idxToPaths.Count)
        res = m_idxToPaths[idx];
      m_lock.ExitReadLock();

      return res;
    }
  }
}
