// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace Statoscope
{
  enum StreamingStage
	{
		Waiting     = 0,
		IO          = 1<<0,
		Inflate     = 1<<1,
		Async       = 1<<2,
    Preempted   = 1<<3,

    IO_Inflate  = IO | Inflate,
    Preempted_Inflate  = Preempted | Inflate,
	}

  enum StreamPriority
	{
		Urgent = 0,
		Preempted = 1, //For internal use only
		AboveNormal = 2,
		Normal = 3,
		BelowNormal = 4,
		Idle = 5,
	}

  enum StreamTaskType
  {
		Count = 13,
    Pak = 12,
    Flash = 11,
    Video = 10,
    ReadAhead = 9,
    Shader = 8,
    Sound = 7,
    Music = 6,
    FSBCache = 5,
    Animation = 4,
    Terrain = 3,
    Geometry = 2,
    Texture = 1,
		Unknown = 0,
  }

  enum StreamSourceMediaType
  {
    Unknown = 0,
    HDD,
    Disc,
    Memory,

    Num
  }

  class StreamingInterval : Interval
  {
    [Category("Request")]
    public string Filename
    {
      get { return filename; }
    }

    [Category("Progress")]
    public StreamingStage Stage
    {
      get { return (StreamingStage)stage; }
    }

    [Category("Request")]
    public StreamTaskType Source
    {
      get { return (StreamTaskType)source; }
    }

    [Category("Request")]
    public int PerceptualImportance
    {
      get { return perceptualImportance; }
    }

    [Category("Request")]
    public int CompressedSize
    {
      get { return compressedSize; }
    }

    [Category("Request")]
    public StreamSourceMediaType MediaType
    {
      get { return (StreamSourceMediaType)mediaType; }
    }

    [Category("Ordering")]
    public StreamPriority Priority
    {
      get { return (StreamPriority)priority; }
    }

    [Category("Ordering")]
    public int TimeGroup
    {
      get { return (int)(((UInt64)sortKey)>>40) & ((1<<20)-1); }
    }

    [Category("Ordering")]
    public int Sweep
    {
      get { return (int)(((UInt64)sortKey)>>30) & ((1<<10)-1); }
    }

    [Category("Ordering")]
    public int DiskOffset
    {
      get { return (int)sortKey & ((1<<30)-1); }
    }

    [Category("Stats")]
    public double IORatePerMS
    {
      get { return CompressedSize / (1024.0 * Length * 1000.0); }
    }

		[Category("Stats")]
		public UInt32 ThreadId
		{
			get { return (UInt32)threadId; }
		}

    private string filename;
    private int stage;
    private int priority;
    private int source;
    private int perceptualImportance;
    private Int64 sortKey;
    private int compressedSize;
    private int mediaType;
		private int threadId;

    public StreamingInterval() { }
    public StreamingInterval(StreamingInterval clone)
      : base(clone)
    {
      this.filename = clone.filename;
      this.stage = clone.stage;
      this.priority = clone.priority;
      this.source = clone.source;
      this.perceptualImportance = clone.perceptualImportance;
      this.sortKey = clone.sortKey;
      this.compressedSize = clone.compressedSize;
      this.mediaType = clone.mediaType;
			this.threadId = clone.threadId;
    }

    #region ICloneable Members

    public override object Clone()
    {
      return new StreamingInterval(this);
    }

    #endregion
  }

  class StreamingIntervalSink : IIntervalSink
  {
    public StreamingIntervalSink(LogData log)
    {
      m_log = log;

      lock (m_log.IntervalTree)
      {
        m_threadsGroup = m_log.IntervalTree.AddGroup("/Streaming/Threads", new StreamingIntervalThreadGroupStyle());
        m_jobsGroup = m_log.IntervalTree.AddGroup("/Streaming/Jobs", new StreamingIntervalJobGroupStyle());

        for (int i = 0; i < (int)StreamSourceMediaType.Num; ++i)
        {
					m_ioRail.Add(m_log.IntervalTree.AddRail(string.Format("IOMT {0}", ((StreamSourceMediaType)i).ToString())));
        }
        m_inflateRail = m_log.IntervalTree.AddRail("Inflate");
        m_asyncRail = m_log.IntervalTree.AddRail("ASync Process");

        for (int i = 0; i < m_ioRail.Count; ++ i)
          m_log.IntervalTree.AddRailToGroup(m_ioRail[i], m_threadsGroup);

        m_log.IntervalTree.AddRailToGroup(m_inflateRail, m_threadsGroup);
        m_log.IntervalTree.AddRailToGroup(m_asyncRail, m_threadsGroup);
      }

      StreamTaskType[] types = Enum.GetValues(typeof(StreamTaskType)).Cast<StreamTaskType>().ToArray();

      m_waitTimes = new StreamingTimeBucketSet(m_waitTimeRanges);
      m_ioTimes = new StreamingTimeBucketSet(m_ioTimeRanges);
      m_inflateTimes = new StreamingTimeBucketSet(m_ioTimeRanges);
      m_ioTypes = new StreamingTypeBucketSet(types);
      m_inflateRates = new StreamingTimeBucketSet(m_inflateRateRanges);

      m_requestBucketsByType[StreamTaskType.Texture] = new StreamingTimeBucketSet(m_requestTimeRanges);
      m_requestBucketsByType[StreamTaskType.Geometry] = new StreamingTimeBucketSet(m_requestTimeRanges);
      m_requestBucketsByType[StreamTaskType.Sound] = new StreamingTimeBucketSet(m_requestTimeRanges);

      lock (m_log.m_bucketSets)
      {
        m_log.m_bucketSets["Stream Wait Time"] = m_waitTimes;
        m_log.m_bucketSets["Stream IO Time"] = m_ioTimes;
        m_log.m_bucketSets["Stream Inflate Time"] = m_inflateTimes;
        m_log.m_bucketSets["Stream Inflate Rate"] = m_inflateRates;
        m_log.m_bucketSets["Stream IO Type"] = m_ioTypes;

        foreach (var kv in m_requestBucketsByType)
          m_log.m_bucketSets[string.Format("Stream Request Time ({0})", kv.Key.ToString())] = kv.Value;
      }
    }

    #region IIntervalSink Members

    public uint ClassId
    {
      get { return 115; }
    }

    public Type PropertiesType
    {
      get { return typeof(StreamingInterval); }
    }

    public void OnBegunInterval(UInt64 ctxId, Interval iv)
    {
    }

    public void OnFinalisedInterval(UInt64 ctxId, Interval iv, bool isModification)
    {
      StreamingInterval siv = (StreamingInterval)iv;

      RailCtx ctx;

      lock (m_log.IntervalTree)
      {
        ctx = GetRailIdForPath_Locked(siv.Source.ToString() + "/" + siv.Filename);

        if (ctx.StartTime == 0.0f)
          ctx.StartTime = iv.Start;

				if ((siv.Stage & StreamingStage.IO) != 0)
				{
					if (siv.MediaType == StreamSourceMediaType.Unknown)
					{
						// Find an io rail by thread id
						int railIdx;
						if (!m_ioRailDict.TryGetValue(siv.ThreadId, out railIdx))
						{
							m_ioRail.Add(m_log.IntervalTree.AddRail(string.Format("IO Thread {0:x}", siv.ThreadId)));
							m_ioRailDict.Add(siv.ThreadId, m_ioRail.Count - 1);
							railIdx = m_ioRail.Count - 1;
							m_log.IntervalTree.AddRailToGroup(m_ioRail[railIdx], m_threadsGroup);
						}
						m_log.IntervalTree.AddInterval(m_ioRail[railIdx], iv);
					}
					else
					{
						m_log.IntervalTree.AddInterval(m_ioRail[(int)siv.MediaType], iv);
					}
        }

        if ((siv.Stage & StreamingStage.Inflate) != 0)
            m_log.IntervalTree.AddInterval(m_inflateRail, iv);

        if ((siv.Stage & StreamingStage.Async) != 0)
            m_log.IntervalTree.AddInterval(m_asyncRail, iv);

        m_log.IntervalTree.AddInterval(ctx.RailId, iv);
      }

      if ((siv.Stage & StreamingStage.IO) != 0)
        ctx.IOTime += siv.Length;

      if ((siv.Stage & StreamingStage.Inflate) != 0)
        ctx.InflateTime += siv.Length;

      lock (m_log.m_bucketSets)
      {
        if (siv.Stage == StreamingStage.Waiting)
          m_waitTimes.AddInterval(siv.Length);

        if ((siv.Stage & StreamingStage.IO) != 0)
          m_ioTypes.AddInterval(siv);

        if (!isModification)
        {
          StreamingTimeBucketSet buckets;
          if (m_requestBucketsByType.TryGetValue(siv.Source, out buckets))
            buckets.AddInterval(iv.End - ctx.StartTime);

          m_ioTimes.AddInterval(ctx.IOTime);
          m_inflateTimes.AddInterval(ctx.InflateTime);
          m_inflateRates.AddInterval(siv.CompressedSize / (1024.0 * ctx.InflateTime * 1000.0));
        }
      }

      if (!isModification)
      {
        ctx.StartTime = 0.0;
        ctx.InflateTime = 0.0;
        ctx.IOTime = 0.0;
      }
    }

    public void OnFrameRecord(FrameRecordValues values)
    {
    }

    #endregion

    private RailCtx GetRailIdForPath_Locked(string path)
    {
      RailCtx ctx = null;
      if (m_rails.ContainsKey(path))
      {
        ctx = m_rails[path];
      }
      else
      {
        ctx = new RailCtx();
        ctx.RailId = m_log.IntervalTree.AddRail(path);
        m_rails.Add(path, ctx);

        m_log.IntervalTree.AddRailToGroup(ctx.RailId, m_jobsGroup);
      }
      return ctx;
    }

    static private BucketTimeRange[] m_waitTimeRanges = new BucketTimeRange[] {
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 0), new TimeSpan(0, 0, 0, 0, 10)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 10), new TimeSpan(0, 0, 0, 0, 50)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 50), new TimeSpan(0, 0, 0, 0, 100)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 100), new TimeSpan(0, 0, 0, 0, 500)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 500), new TimeSpan(0, 0, 0, 1, 0)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 1, 0), new TimeSpan(0, 0, 0, 10, 0)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 10, 0), TimeSpan.MaxValue),
    };

    static private BucketTimeRange[] m_ioTimeRanges = new BucketTimeRange[] {
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 0), new TimeSpan(0, 0, 0, 0, 1)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 1), new TimeSpan(0, 0, 0, 0, 5)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 5), new TimeSpan(0, 0, 0, 0, 10)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 10), new TimeSpan(0, 0, 0, 0, 50)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 50), new TimeSpan(0, 0, 0, 0, 100)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 100), new TimeSpan(0, 0, 0, 0, 500)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 500), TimeSpan.MaxValue),
    };

    static private BucketTimeRange[] m_requestTimeRanges = new BucketTimeRange[] {
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 0), new TimeSpan(0, 0, 0, 0, 10)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 10), new TimeSpan(0, 0, 0, 0, 50)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 50), new TimeSpan(0, 0, 0, 0, 100)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 100), new TimeSpan(0, 0, 0, 0, 500)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 500), new TimeSpan(0, 0, 0, 1, 0)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 1, 0), new TimeSpan(0, 0, 0, 10, 0)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 10, 0), TimeSpan.MaxValue),
    };

    static private BucketTimeRange[] m_inflateRateRanges = new BucketTimeRange[] {
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 0), new TimeSpan(0, 0, 0, 0, 1)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 1), new TimeSpan(0, 0, 0, 0, 5)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 5), new TimeSpan(0, 0, 0, 0, 10)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 10), new TimeSpan(0, 0, 0, 0, 15)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 15), new TimeSpan(0, 0, 0, 0, 20)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 20), new TimeSpan(0, 0, 0, 0, 30)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 30), new TimeSpan(0, 0, 0, 0, 50)),
      new BucketTimeRange(new TimeSpan(0, 0, 0, 0, 50), TimeSpan.MaxValue),
    };

    private class RailCtx
    {
      public double StartTime;
      public double IOTime;
      public double InflateTime;
      public int RailId;
    }

    private LogData m_log;
    private StreamingTimeBucketSet m_waitTimes;
    private StreamingTimeBucketSet m_ioTimes;
    private StreamingTimeBucketSet m_inflateTimes;
    private StreamingTimeBucketSet m_inflateRates;
    private StreamingTypeBucketSet m_ioTypes;

    private Dictionary<string, RailCtx> m_rails = new Dictionary<string, RailCtx>();
    private Dictionary<StreamTaskType, StreamingTimeBucketSet> m_requestBucketsByType = new Dictionary<StreamTaskType, StreamingTimeBucketSet>();
    private int m_threadsGroup = 0;
    private int m_jobsGroup = 0;
		private List<int> m_ioRail = new List<int>();
		private Dictionary<UInt32, int> m_ioRailDict = new Dictionary<UInt32, int>();
    private int m_inflateRail = 0;
    private int m_asyncRail = 0;
  }
}
