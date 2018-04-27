// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace Statoscope
{
	enum StreamingObjectStage
	{
		Unloaded,
		Requested,
		LoadedUnused,
		LoadedUsed,
	}

  class StreamingObjectInterval : Interval
  {
    [Category("Request")]
    public string Filename
    {
      get { return filename; }
    }

    [Category("Progress")]
    public StreamingObjectStage Stage
    {
      get { return (StreamingObjectStage)stage; }
    }

    private string filename;
    private int stage;

    public StreamingObjectInterval() { }
    public StreamingObjectInterval(StreamingObjectInterval clone)
      : base(clone)
    {
      this.filename = clone.filename;
      this.stage = clone.stage;
    }

    #region ICloneable Members

    public override object Clone()
    {
      return new StreamingObjectInterval(this);
    }

    #endregion
  }
  
  class StreamingObjectSink : IIntervalSink
  {
    public StreamingObjectSink(LogData log, string group, uint classId)
    {
      m_log = log;
      m_group = group;
      m_classId = classId;

      lock (m_log.IntervalTree)
      {
        m_groupId = m_log.IntervalTree.AddGroup(group, new StreamingObjectIntervalGroupStyle());
      }
    }

    #region IIntervalSink Members

    public uint ClassId
    {
      get { return m_classId; }
    }

    public Type PropertiesType
    {
      get { return typeof(StreamingObjectInterval); }
    }

    public void OnBegunInterval(UInt64 ctxId, Interval iv)
    {
      StreamingObjectInterval siv = (StreamingObjectInterval)iv;
      RailTail tail = GetRailIdForPath(ctxId, siv.Filename);

      lock (m_log.IntervalTree)
      {
        m_log.IntervalTree.AddInterval(tail.id, iv);

        if (tail.lastIv != null)
          throw new ApplicationException("blasd2");
        tail.lastIv = iv;
      }
    }

    public void OnFinalisedInterval(UInt64 ctxId, Interval iv, bool isModification)
    {
      if (iv.EndUs == Int64.MaxValue)
        throw new ApplicationException("bleurgh");

      StreamingObjectInterval siv = (StreamingObjectInterval)iv;
      RailTail tail = GetRailIdForPath(ctxId, siv.Filename);

      lock (m_log.IntervalTree)
      {
        if (tail.lastIv == iv)
          tail.lastIv = null;
        else
          m_log.IntervalTree.AddInterval(tail.id, iv);
      }
    }

    public void OnFrameRecord(FrameRecordValues values)
    {
    }

    #endregion

    private RailTail GetRailIdForPath(UInt64 ctxId, string path)
    {
      RailTail t = null;
      if (m_rails.ContainsKey(ctxId))
      {
        t = m_rails[ctxId];
      }
      else
      {
        lock (m_log.IntervalTree)
        {
          int railId = m_log.IntervalTree.AddRail(path);
          t = new RailTail();
          t.id = railId;
          m_rails.Add(ctxId, t);

          m_log.IntervalTree.AddRailToGroup(railId, m_groupId);
        }
      }
      return t;
    }

    private class RailTail
    {
      public int id;
      public Interval lastIv;
    }

    private LogData m_log;
    private string m_group;
    private uint m_classId;

    private Dictionary<UInt64, RailTail> m_rails = new Dictionary<UInt64, RailTail>();
    private int m_groupId = 0;
  }
}
