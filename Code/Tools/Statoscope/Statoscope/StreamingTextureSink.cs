// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace Statoscope
{
  class StreamingTextureInterval : Interval
  {
    [Category("Request")]
    public string Filename
    {
      get { return filename; }
    }

    [Category("State")]
    public int MinMipWanted
    {
      get { return minMipWanted; }
    }

    [Category("State")]
    public int MinMipAvailable
    {
      get { return minMipAvailable; }
    }

    [Category("State")]
    public bool InUse
    {
      get { return inUse > 0; }
    }

    private string filename;
    private int minMipWanted = 100;
    private int minMipAvailable = 100;
    private int inUse;

    public StreamingTextureInterval() { }
    public StreamingTextureInterval(StreamingTextureInterval clone)
      : base(clone)
    {
      this.filename = clone.filename;
      this.minMipWanted = clone.minMipWanted;
      this.minMipAvailable = clone.minMipAvailable;
      this.inUse = clone.inUse;
    }

    #region ICloneable Members

    public override object Clone()
    {
      return new StreamingTextureInterval(this);
    }

    #endregion
  }
  
  class StreamingTextureSink : IIntervalSink
  {
    public StreamingTextureSink(LogData log, string group, uint classId)
    {
      m_log = log;
      m_group = group;
      m_classId = classId;

      lock (m_log.IntervalTree)
      {
        m_groupId = m_log.IntervalTree.AddGroup(group, new StreamingTextureIntervalGroupStyle());
      }

    }

    #region IIntervalSink Members

    public uint ClassId
    {
      get { return m_classId; }
    }

    public Type PropertiesType
    {
      get { return typeof(StreamingTextureInterval); }
    }

    public void OnBegunInterval(UInt64 ctxId, Interval iv)
    {
      StreamingTextureInterval siv = (StreamingTextureInterval)iv;
      TextureCtx ctx = GetRailIdForPath(ctxId, siv.Filename);

      ctx.MinMipAvailable = siv.MinMipAvailable;
      ctx.MinMipWanted = siv.MinMipWanted;
      ctx.IsVisible = siv.InUse;

      if (!m_seenInterval)
      {
        m_seenInterval = true;

        lock (m_log)
        {
          m_log.SetItemMetaData("/StreamingTextures/mipDelta", EItemType.Float);
          m_log.SetItemMetaData("/StreamingTextures/mipDeltaSD", EItemType.Float);
          m_log.SetItemMetaData("/StreamingTextures/mipDeltaMean", EItemType.Float);
        }
      }

      lock (m_log.IntervalTree)
      {
        m_log.IntervalTree.AddInterval(ctx.RailId, iv);
      }
    }

    public void OnFinalisedInterval(UInt64 ctxId, Interval iv, bool isModification)
    {
      StreamingTextureInterval siv = (StreamingTextureInterval)iv;
      TextureCtx ctx = GetRailIdForPath(ctxId, siv.Filename);

      lock (m_log.IntervalTree)
      {
        m_log.IntervalTree.AddInterval(ctx.RailId, iv);
      }

      if (!isModification)
      {
        m_rails.Remove(ctxId);
      }
    }

    public void OnFrameRecord(FrameRecordValues values)
    {
			if (m_rails.Count == 0)
				return;

      int totalDelta = 0, count = 0;

      foreach (var kv in m_rails)
      {
        TextureCtx ctx = kv.Value;
        if (ctx.IsVisible && ctx.MinMipWanted <= ctx.MinMipAvailable)
        {
          totalDelta += Math.Abs(ctx.MinMipWanted - ctx.MinMipAvailable);
          ++count;
        }
      }

      double mean = totalDelta / (double)count;
      double v = 0;

      foreach (var kv in m_rails)
      {
        TextureCtx ctx = kv.Value;
        if (ctx.IsVisible && ctx.MinMipWanted <= ctx.MinMipAvailable)
        {
          int d = Math.Abs(ctx.MinMipWanted - ctx.MinMipAvailable);
          v += (d - mean) * (d - mean);
        }
      }

      double vmean = v / count;
      double sd = Math.Sqrt(vmean);

      values["/StreamingTextures/mipDelta"] = (float)totalDelta;
      values["/StreamingTextures/mipDeltaSD"] = (float)sd;
      values["/StreamingTextures/mipDeltaMean"] = (float)mean;
    }

    #endregion

    private TextureCtx GetRailIdForPath(UInt64 ctxId, string path)
    {
      TextureCtx t = null;
      if (m_rails.ContainsKey(ctxId))
      {
        t = m_rails[ctxId];
      }
      else
      {
        lock (m_log.IntervalTree)
        {
          int railId = m_log.IntervalTree.AddRail(path);
          t = new TextureCtx();
          t.RailId = railId;
          m_rails.Add(ctxId, t);

          m_log.IntervalTree.AddRailToGroup(railId, m_groupId);
        }
      }
      return t;
    }

    private class TextureCtx
    {
      public int RailId;
      public int MinMipWanted;
      public int MinMipAvailable;
      public bool IsVisible;
    }

    private LogData m_log;
    private string m_group;
    private uint m_classId;

    private bool m_seenInterval;

    private Dictionary<UInt64, TextureCtx> m_rails = new Dictionary<UInt64, TextureCtx>();
    private int m_groupId = 0;
  }
}
