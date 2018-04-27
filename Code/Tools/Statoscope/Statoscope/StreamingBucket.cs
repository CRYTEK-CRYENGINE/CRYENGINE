// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace Statoscope
{
  class BucketTimeRange
  {
    private TimeSpan m_min;
    private TimeSpan m_max;

    public TimeSpan Min
    {
      get { return m_min; }
    }

    public TimeSpan Max
    {
      get { return m_max; }
    }

    public BucketTimeRange(TimeSpan min, TimeSpan max)
    {
      m_min = min;
      m_max = max;
    }
  }

  class StreamingTimeBucket : Bucket
  {
    [Description("Count")]
    public int Num
    {
      get { return m_num; }
      set { m_num = value; }
    }

    [Description("%")]
    public float NumPc
    {
      get { return m_numPc; }
      set { m_numPc = value; }
    }

    [Description("% Cumulative")]
    public float NumPcCumulative
    {
      get { return m_numPcCumulative; }
      set { m_numPcCumulative = value; }
    }

    public StreamingTimeBucket(string rangeStr)
      : base(rangeStr)
    {
    }

    private int m_num;
    private float m_numPc;
    private float m_numPcCumulative;
  }

  class StreamingTypeBucket : Bucket
  {
    [Description("Count")]
    public int Num
    {
      get { return m_num; }
      set { m_num = value; }
    }

    [Description("%")]
    public float NumPc
    {
      get { return m_numPc; }
      set { m_numPc = value; }
    }

    public StreamingTypeBucket(string rangeStr)
      : base(rangeStr)
    {
    }

    private int m_num;
    private float m_numPc;
  }

  class StreamingTypeBucketSet : BucketSet
  {
    private readonly StreamTaskType[] m_types;
    private readonly Dictionary<StreamTaskType, int> m_typeToBucket = new Dictionary<StreamTaskType, int>();
    private int m_total;

    public override bool IsEmpty
    {
      get { return m_total == 0; }
    }

    public StreamingTypeBucketSet(StreamTaskType[] types)
      : base(CreateBuckets(types))
    {
      m_types = types;

      for (int i = 0, c = types.Length; i != c; ++i)
        m_typeToBucket[types[i]] = i;
    }

    public void AddInterval(StreamingInterval iv)
    {
      ++m_total;

      int bucketIdx = m_typeToBucket[iv.Source];
      StreamingTypeBucket bucket = m_buckets[bucketIdx] as StreamingTypeBucket;
      ++bucket.Num;

      UpdatePc();
    }

    private void UpdatePc()
    {
      float total = m_total;
      for (int i = 0; i < m_buckets.Count; ++i)
      {
        StreamingTypeBucket bucket = m_buckets[i] as StreamingTypeBucket;
        bucket.NumPc = Math.Min(100.0f, 100.0f * bucket.Num / total);
      }
    }

    private static List<Bucket> CreateBuckets(StreamTaskType[] types)
    {
      List<Bucket> l = new List<Bucket>(types.Length);

      foreach (StreamTaskType i in types)
        l.Add(new StreamingTypeBucket(i.ToString()));

      return l;
    }
  }

  class StreamingTimeBucketSet : BucketSet
  {
    private readonly BucketTimeRange[] m_ranges;
		private int m_total;

    public override bool IsEmpty
    {
      get { return m_total == 0; }
    }

    public StreamingTimeBucketSet(BucketTimeRange[] ranges)
      : base(CreateBuckets(ranges))
    {
      m_ranges = ranges;
      m_total = 0;
    }

    public void AddIntervals(IEnumerable<StreamingInterval> ivs)
    {
      foreach (StreamingInterval iv in ivs)
        AddInterval(iv.Length);
    }

    public void AddInterval(double length)
    {
      ++m_total;

      TimeSpan ivts = new TimeSpan((long)(length * 1000.0 * 1000.0 * 10.0));
      int bucketIdx = m_ranges.MappedBinarySearchIndex(ivts, (x) => x.Min, (a, b) => a < b);
      StreamingTimeBucket bucket = (StreamingTimeBucket)m_buckets[bucketIdx];

      ++bucket.Num;
      bucket.NumPc = Math.Min(100.0f, 100.0f * (bucket.Num / (float)m_total));

      UpdatePc();
    }

    private void UpdatePc()
    {
      float total = (float)m_total;
      int numCumulative = 0;

      for (int i = 0, c = m_buckets.Count; i < c; ++i)
      {
        StreamingTimeBucket sb = m_buckets[i] as StreamingTimeBucket;
        numCumulative += sb.Num;
        sb.NumPc = Math.Min(100.0f, 100.0f * sb.Num / total);
        sb.NumPcCumulative = Math.Min(100.0f, 100.0f * (numCumulative / total));
      }
    }

    static private List<Bucket> CreateBuckets(BucketTimeRange[] ranges)
    {
      List<Bucket> buckets = new List<Bucket>();
      foreach (BucketTimeRange r in ranges)
      {
        string s = "";
        if (r.Min > new TimeSpan(0))
          s += string.Format("{0}ms", r.Min.TotalMilliseconds);
        s += "-";
        if (r.Max < TimeSpan.MaxValue)
          s += string.Format("{0}ms", r.Max.TotalMilliseconds);
        buckets.Add(new StreamingTimeBucket(s));
      }
      return buckets;
    }
  }
}
