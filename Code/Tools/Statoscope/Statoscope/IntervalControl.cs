// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using CsGL.OpenGL;
using System.Reflection;
using System.Text;
using System.Runtime.InteropServices;
using System.ComponentModel;

namespace Statoscope
{
  class IntervalControl : AAOpenGLControl
  {
    public RectD View
    {
      get { return m_view; }
      set { m_view = value; ClampView(); UpdateScroll(); OnViewChanged(); Invalidate(); }
    }

    public RectD PhysExtents
    {
      get { return m_physExtents; }
    }

    public SpanD TimeRange
    {
      get { return new SpanD(m_physExtents.Left, m_physExtents.Right); }
      set
      {
        if (value.Length < double.Epsilon)
          value = new SpanD(value.Min, value.Min + 1.0);

        m_physExtents = new RectD(value.Min, m_physExtents.Top, value.Max, m_physExtents.Bottom);
        ClampView();
        UpdateScroll();
        Invalidate();
      }
    }

    public PointD VirtualSize
    {
      get { return m_physExtents.Size; }
    }

    public double SelectionStart
    {
      get { return Math.Min(m_selectionAnchor, m_selectionEnd); }
    }

    public double SelectionEnd
    {
      get { return Math.Max(m_selectionAnchor, m_selectionEnd); }
    }

    public int ActiveRailId
    {
      get { return m_activeRailId; }
    }

    public Interval ActiveInterval
    {
      get { return m_activeInterval; }
    }

    public IntervalTree Tree
    {
      get { return m_tree; }
      set
      {
        if (m_tree != value)
        {
          m_tree = value;
          RefreshTree();
        }
      }
    }

    public event EventHandler SelectionChanged;
    public event EventHandler ViewChanged;
    public event EventHandler ActiveIntervalChanged;

    public IntervalControl()
    {
      SetStyle(ControlStyles.AllPaintingInWmPaint, true);
      SetStyle(ControlStyles.ResizeRedraw, true);

      m_tooltip = new ToolTip();
      m_tooltip.AutoPopDelay = 5000;
      m_tooltip.InitialDelay = 1000;
      m_tooltip.ReshowDelay = 500;
    }

    public bool IsGroupVisible(int groupId)
    {
      return m_groupViews[groupId].Visible;
    }

    public void ShowGroup(RailGroup rg, bool show)
    {
      int gid;
      lock (m_tree)
      {
        gid = m_tree.Groups.IndexOf(rg);
      }

      if (gid >= 0)
      {
        m_groupViews[gid].Visible = show;
        ClampView();
        ClampView();
        UpdateScroll();
        Invalidate();
      }
    }

    public void ShowGroupRail(RailGroup rg, int railId, bool show)
    {
      int gid;
      lock (m_tree)
      {
        gid = m_tree.Groups.IndexOf(rg);
      }

      if (gid >= 0)
      {
        if (show)
          m_groupViews[gid].VisibleRailIds.Add(railId);
        else
          m_groupViews[gid].VisibleRailIds.Remove(railId);

        RefreshVisibleRails(gid);
        Invalidate();
      }
    }

    public void RefreshTree()
    {
      lock (m_tree)
      {
        for (int gid = 0, gc = m_groupViews.Count; gid != gc; ++gid)
        {
          RailGroup rg = m_tree.Groups[gid];
          GroupView gv = m_groupViews[gid];

          Dictionary<int, bool> existingRails = new Dictionary<int, bool>();
          List<int> newRails = new List<int>(rg.Rails.Count);
          foreach (int i in gv.RailIds)
          {
            existingRails.Add(i, true);
            newRails.Add(i);
          }

          Dictionary<int, bool> existingVisibleRails = new Dictionary<int, bool>();
          foreach (int i in gv.VisibleRailIds)
            existingVisibleRails.Add(i, true);

          foreach (int di in rg.Rails)
          {
            bool exists = existingRails.ContainsKey(di);
            if (!exists)
              newRails.Add(di);
            if (!existingVisibleRails.ContainsKey(di) && !exists)
              gv.VisibleRailIds.Add(di);
          }

          gv.RailIds = newRails.ToArray();
        }

        for (int gid = m_groupViews.Count, gc = m_tree.Groups.Count; gid != gc; ++gid)
        {
          RailGroup rg = m_tree.Groups[gid];

          GroupView gv = new GroupView();
          gv.GroupId = gid;
          gv.RailIds = rg.Rails.ToArray();
          gv.VisibleRailIds = new List<int>(gv.RailIds);
          m_groupViews.Add(gv);
        }
      }

      ClampView();
      UpdateScroll();
      Invalidate();
    }

    protected override void glDraw()
    {
      if (m_tree != null)
      {
        lock (m_tree)
        {
          ResourceInit();

          PrepareViewport();

          m_visible.Clear();

          double unitsPerPy = GetSamplesPerPixelY();
          double pyPerUnit = 1.0 / unitsPerPy;

          bool showEdges = RailHeightUnits * pyPerUnit >= 2.5;
          float contraction = 0.0f;//RailHeightUnits * pyPerUnit >= 4.0 ? 2.0f * (float)unitsPerPy : 0.0f;

          double groupHeaderHeightUnits = unitsPerPy * GroupHeaderHeightPx;

          Int64 peLeft = (Int64)(m_physExtents.Left * 1000000.0);
          Int64 peRight = (Int64)(m_physExtents.Right * 1000000.0);
          Int64 vLeft = (Int64)(m_view.Left * 1000000.0);
          Int64 vRight = (Int64)(m_view.Right * 1000000.0);

          {
            double groupTopUnits = 0;

            for (int gvi = 0, gvc = m_groupViews.Count; gvi < gvc; ++gvi)
            {
              GroupView gv = m_groupViews[gvi];

              if (gv.Visible)
              {
                RailGroup rg = m_tree.Groups[gv.GroupId];

                double groupHeaderTopUnits = groupTopUnits;
                double groupHeaderBottomUnits = groupTopUnits + groupHeaderHeightUnits;
                double groupHeightUnits = gv.VisibleRailIds.Count * RailHeightUnits;
                double groupBottomUnits = groupHeaderBottomUnits + groupHeightUnits;

                gv.ProjectedTop = (int)((groupHeaderTopUnits - m_view.Top) * pyPerUnit);

                double top = groupHeaderBottomUnits;
                foreach (int railId in gv.VisibleRailIds)
                {
                  if (top + RailHeightUnits > m_view.Top)
                  {
                    foreach (Interval iv in m_tree.RangeEnum(railId, peLeft, peRight))
                    {
                      if ((Int64)iv.EndUs <= vLeft) continue;
                      if ((Int64)iv.StartUs >= vRight) continue;

                      BakedInterval biv = new BakedInterval(
                        railId,
                        rg.Style.GetIntervalColor(iv),
                        new RectD(
                          ((Int64)iv.StartUs - vLeft) / 1000000.0,
                          top + contraction, 
                          Math.Min(peRight, ((Int64)iv.EndUs - vLeft)) / 1000000.0,
                          top + RailHeightUnits - contraction),
                        iv);
                      m_visible.Add(biv);
                    }
                  }

                  top += RailHeightUnits;

                  if (top >= m_view.Bottom)
                    break;
                }

                groupTopUnits = groupHeaderBottomUnits + groupHeightUnits;
              }
            }
          }

          if (RailHeightUnits * pyPerUnit >= 7.0)
          {
            double groupTopUnits = 0;

            OpenGL.glBegin(OpenGL.GL_LINES);
            OpenGL.glColor3f(0.0f, 0.0f, 0.0f);

            for (int gvi = 0, gvc = m_groupViews.Count; gvi < gvc; ++gvi)
            {
              GroupView gv = m_groupViews[gvi];

              if (gv.Visible)
              {
                RailGroup rg = m_tree.Groups[gv.GroupId];

                double groupHeaderTopUnits = groupTopUnits;
                double groupHeaderBottomUnits = groupTopUnits + groupHeaderHeightUnits;
                double groupHeightUnits = gv.VisibleRailIds.Count * RailHeightUnits;
                double groupBottomUnits = groupHeaderBottomUnits + groupHeightUnits;

                gv.ProjectedTop = (int)((groupHeaderTopUnits - m_view.Top) * pyPerUnit);

                double top = groupHeaderBottomUnits + RailHeightUnits / 2.0f;

                foreach (int railId in gv.VisibleRailIds)
                {
                  OpenGL.glVertex2f(0.0f, (float)(top - m_view.Top));
                  OpenGL.glVertex2f((float)m_view.Width, (float)(top - m_view.Top));

                  top += RailHeightUnits;
                }

                groupTopUnits = groupHeaderBottomUnits + groupHeightUnits;
              }
            }

            OpenGL.glEnd();
          }

          {
            OpenGL.glBegin(OpenGL.GL_QUADS);

            foreach (var iv in m_visible)
            {
              float l = (float)(iv.BBox.Left);
              float t = (float)(iv.BBox.Top - m_view.Top);
              float r = (float)(iv.BBox.Right);
              float b = (float)(iv.BBox.Bottom - m_view.Top);

              Color color = iv.Color;

              OpenGL.glColor3f(color.R * (1.0f / 255.0f), color.G * (1.0f / 255.0f), color.B * (1.0f / 255.0f));

              OpenGL.glVertex2f(l, t);
              OpenGL.glVertex2f(l, b);
              OpenGL.glVertex2f(r, b);
              OpenGL.glVertex2f(r, t);
            }

            OpenGL.glEnd();
          }

          if (showEdges)
          {
            OpenGL.glBegin(OpenGL.GL_LINES);

            OpenGL.glColor3f(0.2f, 0.2f, 0.2f);

            foreach (var iv in m_visible)
            {
              float l = (float)(iv.BBox.Left);
              float t = (float)(iv.BBox.Top - m_view.Top);
              float r = (float)(iv.BBox.Right);
              float b = (float)(iv.BBox.Bottom - m_view.Top);

              OpenGL.glVertex2f(l, t);
              OpenGL.glVertex2f(l, b);

              OpenGL.glVertex2f(l, b);
              OpenGL.glVertex2f(r, b);

              OpenGL.glVertex2f(r, b);
              OpenGL.glVertex2f(r, t);

              OpenGL.glVertex2f(r, t);
              OpenGL.glVertex2f(l, t);
            }

            OpenGL.glEnd();
          }

          {
            OpenGL.glBegin(OpenGL.GL_LINES);

            OpenGL.glColor3f(0.6f, 0.6f, 0.6f);

            float h = (float)m_view.Height;
            for (float x = (float)Math.Floor(m_view.Left), xe = (float)Math.Floor(m_view.Right); x <= xe; ++x)
            {
              OpenGL.glVertex2f((float)(x - m_view.Left), 0);
              OpenGL.glVertex2f((float)(x - m_view.Left), h);
            }

            OpenGL.glEnd();
          }

          DrawSelection();
        }

        PreparePxViewport();

        foreach (GroupView gv in m_groupViews)
        {
          if (gv.Visible)
          {
            OpenGL.glBegin(OpenGL.GL_QUADS);
            OpenGL.glColor3f(1.0f, 1.0f, 1.0f);
            OpenGL.glVertex2f(0.0f, gv.ProjectedTop);
            OpenGL.glVertex2f(0.0f, gv.ProjectedTop + 20);
            OpenGL.glVertex2f(ClientSize.Width, gv.ProjectedTop + 20);
            OpenGL.glVertex2f(ClientSize.Width, gv.ProjectedTop);
            OpenGL.glEnd();

            m_groupFont.Draw(0, gv.ProjectedTop + 2, Color.Black, m_tree.Groups[gv.GroupId].Name);
          }
        }
      }
    }

    protected void OnSelectionChanged()
    {
      for (int gvi = 0, gvc = m_groupViews.Count; gvi != gvc; ++gvi)
      {
        GroupView gv = m_groupViews[gvi];

        lock (m_tree)
        {
          RailGroup rg = m_tree.Groups[gv.GroupId];
          gv.RailIds = rg.Style.ReorderRails(m_tree, gv.RailIds, SelectionStart, SelectionEnd);
        }

        RefreshVisibleRails(gvi);
      }

      if (SelectionChanged != null)
        SelectionChanged(this, EventArgs.Empty);

      Invalidate();
    }

    protected void OnViewChanged()
    {
      if (ViewChanged != null)
        ViewChanged(this, EventArgs.Empty);
    }

    protected void OnActiveIntervalChanged()
    {
      if (ActiveIntervalChanged != null)
        ActiveIntervalChanged(this, EventArgs.Empty);
    }

    protected override void OnPaintBackground(PaintEventArgs pevent)
    {
      // Prevent flickering by preventing this
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
      if ((e.Button & MouseButtons.Left) != 0)
      {
        IncCapture();
        m_isPanPriming = true;
        m_panDragStart = e.Location;
      }

      if ((e.Button & MouseButtons.Right) != 0)
      {
        IncCapture();
        m_isScaling = true;
        m_scaleDragStart = e.Location;
        m_scaleDragOrigin = e.Location;
      }

      if ((e.Button & MouseButtons.Middle) != 0)
      {
        IncCapture();
        m_isSelecting = true;
        m_selectionAnchor = m_selectionEnd = SampleXFromClientX(e.X);
        Invalidate();
      }

      base.OnMouseDown(e);
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
      if ((e.Button & MouseButtons.Left) != 0)
      {
        if (m_isPanPriming)
        {
          var selectedIv = HitTest(e.Location);
          if (selectedIv.Value != m_activeInterval)
          {
            m_activeInterval = selectedIv.Value;
            m_activeRailId = selectedIv.Key;
            OnActiveIntervalChanged();
          }

          m_isPanPriming = false;
        }
        else if (m_isPanning)
        {
          m_isPanning = false;
        }
        DecCapture();
      }

      if ((e.Button & MouseButtons.Right) != 0)
      {
        m_isScaling = false;
        DecCapture();
      }

      if ((e.Button & MouseButtons.Middle) != 0)
      {
        m_isSelecting = false;
        DecCapture();

        OnSelectionChanged();
      }

      base.OnMouseUp(e);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
      bool hasViewChanged = false;

      if ((e.Button & MouseButtons.Left) != 0)
      {
        Point displacement = new Point(e.Location.X - m_panDragStart.X, e.Location.Y - m_panDragStart.Y);

        if (m_isPanPriming)
        {
          if (displacement.X * displacement.X + displacement.Y * displacement.Y >= 25)
          {
            m_isPanning = true;
            m_isPanPriming = false;
          }
        }

        if (m_isPanning)
        {
          double samplesXPerPixel = m_view.Width / ClientRectangle.Width;
          double samplesYPerPixel = m_view.Height / ClientRectangle.Height;

          m_view = new RectD(
            m_view.Left - samplesXPerPixel * displacement.X,
            m_view.Top - samplesYPerPixel * displacement.Y,
            m_view.Right - samplesXPerPixel * displacement.X,
            m_view.Bottom - samplesYPerPixel * displacement.Y);

          m_panDragStart = e.Location;

          ClampView();
          UpdateScroll();
          Invalidate();

          hasViewChanged = true;
        }
      }

      if ((e.Button & MouseButtons.Right) != 0)
      {
        if (m_isScaling)
        {
          double sxCurrent = SampleXFromClientX(m_scaleDragOrigin.X);
          double syCurrent = SampleYFromClientY(m_scaleDragOrigin.Y);

          Point ptDisplacement = new Point(e.Location.X - m_scaleDragStart.X, e.Location.Y - m_scaleDragStart.Y);
          m_scaleDragStart = e.Location;

          double samplesPerPixelX = GetSamplesPerPixelX();
          double samplesPerPixelY = GetSamplesPerPixelY();
          double sx = ptDisplacement.X * samplesPerPixelX;
          double sy = -ptDisplacement.Y * samplesPerPixelY;

          const double ZoomDragScaleX = 2.5;
          const double ZoomDragScaleY = 2.5;

          m_view = new RectD(
            m_view.Left + sx * ZoomDragScaleX, m_view.Top + sy * ZoomDragScaleY,
            m_view.Right - sx * ZoomDragScaleX, m_view.Bottom - sy * ZoomDragScaleY);

          ClampView();

          double sxNew = SampleXFromClientX(m_scaleDragOrigin.X);
          double syNew = SampleYFromClientY(m_scaleDragOrigin.Y);

          m_view = new RectD(
            m_view.Left - (sxNew - sxCurrent), m_view.Top - (syNew - syCurrent),
            m_view.Right - (sxNew - sxCurrent), m_view.Bottom - (syNew - syCurrent));

          ClampView();
          UpdateScroll();
          Invalidate();

          hasViewChanged = true;
        }
      }

      if ((e.Button & MouseButtons.Middle) != 0)
      {
        if (m_isSelecting)
        {
          m_selectionEnd = SampleXFromClientX(e.X);
          Invalidate();
        }
      }

      if (e.Button == 0)
      {
        if (e.X != m_lastPoint.X || e.Y != m_lastPoint.Y)
        {
          var hitResult = HitTest(e.Location);
          Interval selectedInterval = hitResult.Value;

          if (selectedInterval != m_hoverInterval)
          {
            m_hoverInterval = selectedInterval;
            if (selectedInterval != null)
            {
              string path;
              lock (m_tree)
              {
                path = m_tree.GetRailName(hitResult.Key);
              }
              int sep = path.LastIndexOfAny(new char[] { '/', '\\' });
              if (sep >= 0)
                path = path.Substring(sep + 1);

              double selectedStart = selectedInterval.Start;
              double selectedEnd = selectedInterval.End;
              double duration = selectedEnd - selectedStart;
              string durationText = duration > 1.0 ? string.Format("{0}s", duration) : string.Format("{0}ms", duration * 1000.0);
              m_tooltip.SetToolTip(this, string.Format("{0}\n{1}\n{2}s-{3}s ({4})", path, GetTextForIntervalProps(selectedInterval), selectedStart, selectedEnd, durationText));
            }
            else
            {
              m_tooltip.SetToolTip(this, string.Empty);
            }
          }
        }
        m_lastPoint = new Point(e.X, e.Y);
      }

      if (hasViewChanged)
        OnViewChanged();

      base.OnMouseMove(e);
    }

    protected override void OnSizeChanged(EventArgs e)
    {
      base.OnSizeChanged(e);
    }

    private void ResourceInit()
    {
      if (m_railFont == null)
      {
        Font font = new Font(FontFamily.GenericSansSerif, 8.0f, GraphicsUnit.Pixel);
        m_railFont = new GLFont(font);
      }

      if (m_groupFont == null)
      {
        Font font = new Font(FontFamily.GenericSansSerif, 12.0f, GraphicsUnit.Pixel);
        m_groupFont = new GLFont(font);
      }
    }

    private void IncCapture()
    {
      ++m_captureRefs;
      if (m_captureRefs == 1)
        Capture = true;
    }

    private void DecCapture()
    {
      --m_captureRefs;
      if (m_captureRefs == 0)
        Capture = false;
    }

    private void RefreshVisibleRails(int gid)
    {
      GroupView gv = m_groupViews[gid];
      RailGroup rg;
      lock (m_tree)
      {
        rg = m_tree.Groups[gv.GroupId];
      }

      Dictionary<int, bool> railVis = new Dictionary<int, bool>();
      foreach (int railId in gv.VisibleRailIds)
        railVis[railId] = true;

      gv.VisibleRailIds.Clear();
      foreach (int railId in gv.RailIds)
      {
        if (railVis.ContainsKey(railId))
          gv.VisibleRailIds.Add(railId);
      }
    }

    private string GetTextForIntervalProps(object p)
    {
      Type t = p.GetType();

      StringBuilder sb = new StringBuilder();

      foreach (PropertyInfo pi in t.GetProperties())
      {
        bool isBrowseable = true;

        foreach (var attr in pi.GetCustomAttributes(typeof(BrowsableAttribute), true))
        {
          if (((BrowsableAttribute)attr).Browsable == false)
          {
            isBrowseable = false;
            break;
          }
        }

        if (isBrowseable)
          sb.AppendFormat("{0}: {1}\n", pi.Name, pi.GetValue(p, null).ToString());
      }

      return sb.ToString();
    }

    private KeyValuePair<int, Interval> HitTest(Point loc)
    {
      double unitsPerPelX = m_view.Width / ClientSize.Width;
      double unitsPerPelY = m_view.Height / ClientSize.Height;

      double ux = loc.X * unitsPerPelX;
      double uy = loc.Y * unitsPerPelY + m_view.Top;

      int railId = -1;
      Interval selectedInterval = null;

      foreach (BakedInterval bi in m_visible)
      {
        if (ux < bi.BBox.Left) continue;
        if (uy < bi.BBox.Top) continue;
        if (ux >= bi.BBox.Right) continue;
        if (uy >= bi.BBox.Bottom) continue;

        railId = bi.RailId;
        selectedInterval = bi.Interval;
        break;
      }

      return new KeyValuePair<int, Interval>(railId, selectedInterval);
    }

    private void UpdatePhysExtents()
    {
      double unitsPerPy = GetSamplesPerPixelY();
      double headerHeightUnits = unitsPerPy * GroupHeaderHeightPx;

      double height = 0.0;

      for (int gid = 0, gc = m_groupViews.Count; gid != gc; ++gid)
      {
        GroupView gv = m_groupViews[gid];
        if (gv.Visible)
          height += /*headerHeightUnits +*/ gv.VisibleRailIds.Count * RailHeightUnits;
      }

      m_physExtents = new RectD(m_physExtents.Left, 0.0, m_physExtents.Right, Math.Max(RailHeightUnits * 2.0, height));
    }

    private void ClampView()
    {
      UpdatePhysExtents();

      double xMin = m_view.Left, xMax = m_view.Right;
      double yMin = m_view.Top, yMax = m_view.Bottom;

      if ((xMax - xMin) > m_physExtents.Size.X || (xMin > xMax))
      {
        xMin = m_physExtents.Left;
        xMax = m_physExtents.Right;
      }
      else
      {
        if (xMin < m_physExtents.Left)
        {
          xMax += m_physExtents.Left - xMin;
          xMin = m_physExtents.Left;
        }

        if (xMax > m_physExtents.Right)
        {
          xMin -= xMax - m_physExtents.Right;
          xMax = m_physExtents.Right;
        }
      }

      if ((yMax - yMin) > m_physExtents.Size.Y || (yMin > yMax))
      {
        yMin = m_physExtents.Top;
        yMax = m_physExtents.Bottom;
      }
      else
      {
        if (yMin < m_physExtents.Top)
        {
          yMax += m_physExtents.Top - yMin;
          yMin = m_physExtents.Top;
        }

        if (yMax > m_physExtents.Bottom)
        {
          yMin -= yMax - m_physExtents.Bottom;
          yMax = m_physExtents.Bottom;
        }
      }

      m_view = new RectD(xMin, yMin, xMax, yMax);
    }

    private double GetSamplesPerPixelX()
    {
      return m_view.Width / ClientRectangle.Width;
    }

    private double GetSamplesPerPixelY()
    {
      return m_view.Height / ClientRectangle.Height;
    }

    private double SampleXFromClientX(int x)
    {
      double samplesXPerPixel = GetSamplesPerPixelX();
      return m_view.Left + x * samplesXPerPixel;
    }

    private double SampleYFromClientY(int y)
    {
      double samplesYPerPixel = GetSamplesPerPixelY();
      return m_view.Top + y * samplesYPerPixel;
    }

    private void PrepareViewport()
    {
      OpenGL.glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
      OpenGL.glClear(OpenGL.GL_COLOR_BUFFER_BIT | OpenGL.GL_DEPTH_BUFFER_BIT);

      OpenGL.glEnable(OpenGL.GL_BLEND);
      OpenGL.glEnable(OpenGL.GL_DEPTH_TEST);
      OpenGL.glDepthFunc(OpenGL.GL_LEQUAL);
      OpenGL.glBlendFunc(OpenGL.GL_SRC_ALPHA, OpenGL.GL_ONE_MINUS_SRC_ALPHA);

      OpenGL.glViewport(0, 0, this.ClientSize.Width, this.ClientSize.Height);

      OpenGL.glMatrixMode(OpenGL.GL_PROJECTION);
      OpenGL.glLoadIdentity();
      GLU.glOrtho(0.0, m_view.Width, m_view.Height, 0.0, 0.0, 1.0);

      OpenGL.glMatrixMode(OpenGL.GL_MODELVIEW);
      OpenGL.glLoadIdentity();
    }

    private void PreparePxViewport()
    {
      OpenGL.glMatrixMode(OpenGL.GL_PROJECTION);
      OpenGL.glLoadIdentity();
      GLU.glOrtho(0.0, ClientSize.Width, ClientSize.Height, 0.0, 0.0, 1.0);

      OpenGL.glMatrixMode(OpenGL.GL_MODELVIEW);
      OpenGL.glLoadIdentity();
    }

    private void DrawSelection()
    {
      if (m_selectionAnchor != m_selectionEnd)
      {
        float minTime = (float)(Math.Min(m_selectionAnchor, m_selectionEnd) - m_view.Left);
        float maxTime = (float)(Math.Max(m_selectionAnchor, m_selectionEnd) - m_view.Left);

        float fTop = 0.0f;
        float fBottom = (float)m_view.Height;

        OpenGL.glBegin(OpenGL.GL_QUADS);

        OpenGL.glColor4f(154.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f, 0.5f);

        OpenGL.glVertex2f(minTime, fTop);
        OpenGL.glVertex2f(minTime, fBottom);
        OpenGL.glVertex2f(maxTime, fBottom);
        OpenGL.glVertex2f(maxTime, fTop);

        OpenGL.glEnd();

        OpenGL.glBegin(OpenGL.GL_LINES);

        OpenGL.glColor4f(154.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f, 1.0f);

        OpenGL.glVertex2f(minTime, fTop);
        OpenGL.glVertex2f(minTime, fBottom);
        OpenGL.glVertex2f(maxTime, fTop);
        OpenGL.glVertex2f(maxTime, fBottom);

        OpenGL.glEnd();
      }
    }

    protected override void OnCreateControl()
    {
      base.OnCreateControl();

      UpdateScroll();
    }

    #region WndProc
    protected override void WndProc(ref Message m)
    {
      switch (m.Msg)
      {
        case (int)WM.VSCROLL:
        case (int)WM.HSCROLL:
          {
            ScrollEventType type = ScrollEventType.EndScroll;
            switch (m.WParam.ToInt32() & 0xffff)
            {
              case (int)GDI.ScrollBarCommands.SB_LINEUP: type = ScrollEventType.SmallDecrement; break;
              case (int)GDI.ScrollBarCommands.SB_LINEDOWN: type = ScrollEventType.SmallIncrement; break;
              case (int)GDI.ScrollBarCommands.SB_PAGEUP: type = ScrollEventType.LargeDecrement; break;
              case (int)GDI.ScrollBarCommands.SB_PAGEDOWN: type = ScrollEventType.LargeIncrement; break;
              case (int)GDI.ScrollBarCommands.SB_THUMBTRACK: type = ScrollEventType.ThumbTrack; break;
              case (int)GDI.ScrollBarCommands.SB_TOP: type = ScrollEventType.First; break;
              case (int)GDI.ScrollBarCommands.SB_BOTTOM: type = ScrollEventType.Last; break;
              case (int)GDI.ScrollBarCommands.SB_ENDSCROLL: type = ScrollEventType.EndScroll; break;
            }
            OnScroll(new ScrollEventArgs(type, 0, 0, (m.Msg == (int)WM.VSCROLL) ? ScrollOrientation.VerticalScroll : ScrollOrientation.HorizontalScroll));
          }
          break;

        default:
          base.WndProc(ref m);
          break;
      }
    }
    #endregion

    #region Scroll
    protected void OnScroll(ScrollEventArgs se)
    {
      bool hasViewChanged = false;

      if (se.ScrollOrientation == ScrollOrientation.HorizontalScroll)
      {
        GDI.SCROLLINFO si = new GDI.SCROLLINFO();
        si.cbSize = System.Runtime.InteropServices.Marshal.SizeOf(typeof(GDI.SCROLLINFO));
        si.fMask = (int)(GDI.ScrollInfoMask.SIF_TRACKPOS | GDI.ScrollInfoMask.SIF_PAGE | GDI.ScrollInfoMask.SIF_POS);
        GDI.GetScrollInfo(Handle, (int)GDI.ScrollBarDirection.SB_HORZ, ref si);

        switch (se.Type)
        {
          case ScrollEventType.LargeDecrement:
            m_view = new RectD(m_view.Left - m_view.Width, m_view.Top, m_view.Right - m_view.Width, m_view.Bottom);
            break;

          case ScrollEventType.LargeIncrement:
            m_view = new RectD(m_view.Left + m_view.Width, m_view.Top, m_view.Right + m_view.Width, m_view.Bottom);
            break;

          case ScrollEventType.SmallDecrement:
            m_view = new RectD(m_view.Left - m_view.Width * 0.1f, m_view.Top, m_view.Right - m_view.Width * 0.1f, m_view.Bottom);
            break;

          case ScrollEventType.SmallIncrement:
            m_view = new RectD(m_view.Left + m_view.Width * 0.1f, m_view.Top, m_view.Right + m_view.Width * 0.1f, m_view.Bottom);
            break;

          case ScrollEventType.ThumbTrack:
            m_view = new RectD(si.nTrackPos + m_physExtents.Left, m_view.Top, si.nTrackPos + m_physExtents.Left + m_view.Width, m_view.Bottom);
            break;
        }

        ClampView();
        UpdateScroll();
        Invalidate();

        hasViewChanged = true;
      }
      else if (se.ScrollOrientation == ScrollOrientation.VerticalScroll)
      {
        GDI.SCROLLINFO si = new GDI.SCROLLINFO();
        si.cbSize = System.Runtime.InteropServices.Marshal.SizeOf(typeof(GDI.SCROLLINFO));
        si.fMask = (int)(GDI.ScrollInfoMask.SIF_TRACKPOS | GDI.ScrollInfoMask.SIF_PAGE | GDI.ScrollInfoMask.SIF_POS);
        GDI.GetScrollInfo(Handle, (int)GDI.ScrollBarDirection.SB_VERT, ref si);

        switch (se.Type)
        {
          case ScrollEventType.LargeDecrement:
            m_view = new RectD(m_view.Left, m_view.Top - m_view.Height, m_view.Right, m_view.Bottom - m_view.Height);
            break;

          case ScrollEventType.LargeIncrement:
            m_view = new RectD(m_view.Left, m_view.Top + m_view.Height, m_view.Right, m_view.Bottom + m_view.Height);
            break;

          case ScrollEventType.SmallDecrement:
            m_view = new RectD(m_view.Left, m_view.Top - m_view.Height * 0.1f, m_view.Right, m_view.Bottom - m_view.Height * 0.1f);
            break;

          case ScrollEventType.SmallIncrement:
            m_view = new RectD(m_view.Left, m_view.Top + m_view.Height * 0.1f, m_view.Right, m_view.Bottom + m_view.Height * 0.1f);
            break;

          case ScrollEventType.ThumbTrack:
            m_view = new RectD(m_view.Left, si.nTrackPos + m_physExtents.Top, m_view.Right, si.nTrackPos + m_physExtents.Top + m_view.Height);
            break;
        }

        ClampView();
        UpdateScroll();
        Invalidate();

        hasViewChanged = true;
      }

      if (hasViewChanged)
        OnViewChanged();
    }

    void UpdateScroll()
    {
      GDI.SCROLLINFO si = new GDI.SCROLLINFO();
      si.cbSize = System.Runtime.InteropServices.Marshal.SizeOf(typeof(GDI.SCROLLINFO));

      si.fMask = (int)(GDI.ScrollInfoMask.SIF_RANGE | GDI.ScrollInfoMask.SIF_POS | GDI.ScrollInfoMask.SIF_PAGE);

      si.nPos = (int)(m_view.Left - m_physExtents.Left);
      si.nMin = 0;
      si.nMax = (int)m_physExtents.Size.X;
      si.nPage = (int)m_view.Width;

      GDI.SetScrollInfo(Handle, (int)GDI.ScrollBarDirection.SB_HORZ, ref si, true);

      si.nPos = (int)(m_view.Top - m_physExtents.Top);
      si.nMin = 0;
      si.nMax = (int)m_physExtents.Size.Y;
      si.nPage = (int)m_view.Height;

      GDI.SetScrollInfo(Handle, (int)GDI.ScrollBarDirection.SB_VERT, ref si, true);
    }
    #endregion

    private struct BakedInterval
    {
      public int RailId;
      public Color Color;
      public RectD BBox;
      public Interval Interval;

      public BakedInterval(int railId, Color color, RectD bbox, Interval interval)
      {
        RailId = railId;
        Color = color;
        BBox = bbox;
        Interval = interval;
      }
    }

    private class GroupView
    {
      public int GroupId = 0;
      public bool Visible = true;
      public int[] RailIds = null;
      public List<int> VisibleRailIds = new List<int>();

      public int ProjectedTop;
    }

    private const float RailHeightUnits = 1.0f;
    private const float GroupHeaderHeightPx = 20.0f;

    private RectD m_view;
    private RectD m_physExtents;

    private IntervalTree m_tree;

    private ToolTip m_tooltip;

    private List<GroupView> m_groupViews = new List<GroupView>();
    private List<BakedInterval> m_visible = new List<BakedInterval>();

    private GLFont m_railFont;
    private GLFont m_groupFont;

    private Point m_panDragStart;
    private bool m_isPanPriming;
    private bool m_isPanning;

    private Point m_scaleDragStart;
    private Point m_scaleDragOrigin;
    private bool m_isScaling;

    private double m_selectionAnchor = 0.0;
    private double m_selectionEnd = 0.0;
    private bool m_isSelecting = false;

    private Point m_lastPoint = new Point(0, 0);
    private Interval m_hoverInterval;

    private int m_activeRailId = -1;
    private Interval m_activeInterval;

    private int m_captureRefs = 0;
  }
}
