// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using CsGL.OpenGL;

namespace Statoscope
{
  class CanvasControl : AAOpenGLControl
  {
    public RectD View
    {
      get { return m_view; }
      set { m_view = value; ClampView(); UpdateScroll(); OnViewChanged(); Invalidate(); }
    }

    public RectD VirtualBounds
    {
      get { return m_physExtents; }
      set
      {
        m_physExtents = value;
        ClampView();
        UpdateScroll();
        Invalidate();
      }
    }

    public PointD VirtualSize
    {
      get { return m_physExtents.Size; }
    }

    public event EventHandler ViewChanged;
    public event MouseEventHandler LClicked;

    public CanvasControl()
    {
      SetStyle(ControlStyles.AllPaintingInWmPaint, true);
      SetStyle(ControlStyles.ResizeRedraw, true);
    }

    protected void ResetViewport()
    {
      OpenGL.glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
      OpenGL.glClear(OpenGL.GL_COLOR_BUFFER_BIT | OpenGL.GL_DEPTH_BUFFER_BIT);

      OpenGL.glEnable(OpenGL.GL_BLEND);
      OpenGL.glEnable(OpenGL.GL_DEPTH_TEST);
      OpenGL.glDepthFunc(OpenGL.GL_LEQUAL);
      OpenGL.glBlendFunc(OpenGL.GL_SRC_ALPHA, OpenGL.GL_ONE_MINUS_SRC_ALPHA);

      OpenGL.glViewport(0, 0, this.ClientSize.Width, this.ClientSize.Height);
    }

    protected void PrepareViewport()
    {
      OpenGL.glMatrixMode(OpenGL.GL_PROJECTION);
      OpenGL.glLoadIdentity();
      GLU.glOrtho(m_view.Left, m_view.Right, m_view.Bottom, m_view.Top, 0.0, 1.0);

      OpenGL.glMatrixMode(OpenGL.GL_MODELVIEW);
      OpenGL.glLoadIdentity();
    }

    protected void PrepareBiasedViewport()
    {
      OpenGL.glMatrixMode(OpenGL.GL_PROJECTION);
      OpenGL.glLoadIdentity();
      GLU.glOrtho(0.0, m_view.Width, m_view.Height, 0.0, 0.0, 1.0);

      OpenGL.glMatrixMode(OpenGL.GL_MODELVIEW);
      OpenGL.glLoadIdentity();
    }

    protected void PreparePxViewport()
    {
      OpenGL.glMatrixMode(OpenGL.GL_PROJECTION);
      OpenGL.glLoadIdentity();
      GLU.glOrtho(0.0, ClientSize.Width, ClientSize.Height, 0.0, 0.0, 1.0);

      OpenGL.glMatrixMode(OpenGL.GL_MODELVIEW);
      OpenGL.glLoadIdentity();
    }

    protected void IncCapture()
    {
      ++m_captureRefs;
      if (m_captureRefs == 1)
        Capture = true;
    }

    protected void DecCapture()
    {
      --m_captureRefs;
      if (m_captureRefs == 0)
        Capture = false;
    }

    protected double GetSamplesPerPixelX()
    {
      return m_view.Width / ClientRectangle.Width;
    }

    protected double GetSamplesPerPixelY()
    {
      return m_view.Height / ClientRectangle.Height;
    }

    protected double SampleXFromClientX(int x)
    {
      double samplesXPerPixel = GetSamplesPerPixelX();
      return m_view.Left + x * samplesXPerPixel;
    }

    protected double SampleYFromClientY(int y)
    {
      double samplesYPerPixel = GetSamplesPerPixelY();
      return m_view.Top + y * samplesYPerPixel;
    }

    protected void OnViewChanged()
    {
      if (ViewChanged != null)
        ViewChanged(this, EventArgs.Empty);
    }

    protected virtual void OnLClick(MouseEventArgs e)
    {
      if (LClicked != null)
        LClicked(this, e);
    }

    protected override void OnCreateControl()
    {
      base.OnCreateControl();

      UpdateScroll();
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

      base.OnMouseDown(e);
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
      if ((e.Button & MouseButtons.Left) != 0)
      {
        if (m_isPanPriming)
        {
          OnLClick(e);

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

      if (hasViewChanged)
        OnViewChanged();

      base.OnMouseMove(e);
    }

    private void ClampView()
    {
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
    const double ScrollScale = 65536.0;

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
            {
              double tp = si.nTrackPos / ScrollScale * m_physExtents.Width + m_physExtents.Left;
              m_view = new RectD(tp, m_view.Top, tp + m_view.Width, m_view.Bottom);
            }
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
            {
              double tp = si.nTrackPos / ScrollScale * m_physExtents.Height + m_physExtents.Top;
              m_view = new RectD(m_view.Left, tp, m_view.Right, tp + m_view.Height);
            }
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

      double xpc = (m_view.Left - m_physExtents.Left) / m_physExtents.Width;
      double wpc = m_view.Width / m_physExtents.Width;

      si.nPos = (int)(xpc * ScrollScale);
      si.nMin = 0;
      si.nMax = (int)ScrollScale;
      si.nPage = (int)(wpc * ScrollScale);

      GDI.SetScrollInfo(Handle, (int)GDI.ScrollBarDirection.SB_HORZ, ref si, true);

      double ypc = (m_view.Top - m_physExtents.Top) / m_physExtents.Height;
      double hpc = m_view.Height / m_physExtents.Height;

      si.nPos = (int)(ypc * ScrollScale);
      si.nMin = 0;
      si.nMax = (int)ScrollScale;
      si.nPage = (int)(hpc * ScrollScale);

      GDI.SetScrollInfo(Handle, (int)GDI.ScrollBarDirection.SB_VERT, ref si, true);
    }
    #endregion

    private RectD m_view;
    private RectD m_physExtents;

    private Point m_panDragStart;
    private bool m_isPanPriming;
    private bool m_isPanning;

    private Point m_scaleDragStart;
    private Point m_scaleDragOrigin;
    private bool m_isScaling;

    private int m_captureRefs = 0;
  }
}
