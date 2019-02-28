// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace Statoscope
{
  public class Interval : ICloneable
  {
    [Browsable(false)]
    public double Start
    {
      get { return m_startUs / 1000000.0; }
    }

    [Browsable(false)]
    public double End
    {
      get { return m_endUs / 1000000.0; }
    }

    [Browsable(false)]
    public Int64 StartUs
    {
      get { return m_startUs; }
      set { m_startUs = value; }
    }

    [Browsable(false)]
    public Int64 EndUs
    {
      get { return m_endUs; }
      set { m_endUs = value; }
    }


    [Category("Time")]
    public double Length
    {
      get { return (m_endUs - m_startUs) / 1000000.0; }
    }

    public Interval()
    {
    }

    public Interval(Interval iv)
    {
      m_startUs = iv.m_startUs;
      m_endUs = iv.m_endUs;
    }

    private Int64 m_startUs;
    private Int64 m_endUs;

    #region ICloneable Members

    public virtual object Clone()
    {
      return new Interval(this);
    }

    #endregion
  }

  class Rail
  {
    public readonly string Name;
    public List<Interval> Intervals = new List<Interval>();

    public Rail(string name)
    {
      Name = name;
    }
  }

  public class RailGroup
  {
    public readonly string Name;
    public readonly IIntervalGroupStyle Style;

    public List<int> Rails = new List<int>();

    public int RailCount
    {
      get { return Rails.Count; }
    }

    public RailGroup(string name, IIntervalGroupStyle style)
    {
      Name = name;
      Style = style;
    }
  }

  public class IntervalTree
  {
    private List<Rail> m_rails = new List<Rail>();
    private List<RailGroup> m_groups = new List<RailGroup>();
    private long m_minTimeUs = long.MaxValue;
    private long m_maxTimeUs;

    public IList<RailGroup> Groups
    {
      get { return m_groups; }
    }

    public long MinTimeUs
    {
      get { return m_minTimeUs; }
    }

    public long MaxTimeUs
    {
      get { return m_maxTimeUs; }
    }

    public int AddRail(string name)
    {
      int id = m_rails.Count;
      m_rails.Add(new Rail(name));
      return id;
    }

    public string GetRailName(int railId)
    {
      return m_rails[railId].Name;
    }

    public void AddInterval(int rail, Interval iv)
    {
      m_rails[rail].Intervals.Add(iv);
      m_maxTimeUs = Math.Max(m_maxTimeUs, iv.EndUs);
      m_minTimeUs = Math.Min(m_minTimeUs, iv.StartUs);
    }

    public int AddGroup(string name, IIntervalGroupStyle style)
    {
      m_groups.Add(new RailGroup(name, style));
      return m_groups.Count - 1;
    }

    public void AddRailToGroup(int rail, int group)
    {
      m_groups[group].Rails.Add(rail);
    }

    public IEnumerable<Interval> RangeEnum(int railId, Int64 startUs, Int64 endUs)
    {
      Rail rail = m_rails[railId];
      foreach (Interval iv in rail.Intervals)
      {
        if (iv.EndUs <= startUs) continue;
        if (iv.StartUs >= endUs) continue;
        yield return iv;
      }
    }
  }
}
