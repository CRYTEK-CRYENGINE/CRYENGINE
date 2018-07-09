// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using CsGL.OpenGL;
using System.Drawing;
using System.Windows.Forms;

namespace Statoscope
{
  class TreemapNode
  {
    public double Size
    {
      get { return m_size; }
      set { m_size = value; }
    }

    public object Tag
    {
      get { return m_tag; }
      set { m_tag = value; } 
    }

    public string Name
    {
      get { return m_name; }
      set { m_name = value; }
    }

    public GLTexture Texture
    {
      get { return m_texture; }
      set { m_texture = value; }
    }

    internal RectD Bounds;
    internal List<TreemapNode> Children = new List<TreemapNode>();

    public TreemapNode()
    {
    }

    private double m_size;
    private object m_tag;
    private string m_name = string.Empty;
    private GLTexture m_texture = null;
  }

  class TreemapControl : CanvasControl
  {
    private double m_pxNodeMargin = 1.5;

    private ToolTip m_tooltip;

    private TreemapNode m_hoverNode;
    private TreemapNode m_selectNode;
    private Point m_lastPoint;

    public TreemapNode Tree
    {
      get { return m_tree; }
    }

    public TreemapNode SelectedNode
    {
      get { return m_selectNode; }
    }

    public double NodeMarginPx
    {
      get { return m_pxNodeMargin; }
      set { m_pxNodeMargin = value; }
    }

    public EventHandler SelectionChanged;

    public TreemapControl()
    {
      m_tooltip = new ToolTip();
      m_tooltip.AutoPopDelay = 5000;
      m_tooltip.InitialDelay = 1000;
      m_tooltip.ReshowDelay = 500;
    }

    public TreemapNode CreateTree()
    {
      m_tree = new TreemapNode();
      m_tree.Bounds = new RectD(0, 0, 1, 1);
      m_dirty = true;
      m_selectNode = null;
      VirtualBounds = m_tree.Bounds;
      return m_tree;
    }

    public TreemapNode CreateChild(TreemapNode parent)
    {
      TreemapNode n = new TreemapNode();
      parent.Children.Add(n);
      m_dirty = true;
      return n;
    }

    public void InvalidateLayout()
    {
      m_dirty = true;
      Invalidate();
    }

    private TreemapNode HitTest(TreemapNode n, double x, double y)
    {
      if (x >= n.Bounds.Left && x < n.Bounds.Right)
      {
        if (y >= n.Bounds.Top && y < n.Bounds.Bottom)
        {
          foreach (TreemapNode c in n.Children)
          {
            TreemapNode hn = HitTest(c, x, y);
            if (hn != null)
              return hn;
          }

          return n;
        }
      }

      return null;
    }

    public TreemapNode HitTest(Point clientPt)
    {
      double smpX = SampleXFromClientX(clientPt.X);
      double smpY = SampleYFromClientY(clientPt.Y);

      if (m_tree != null)
      {
        TreemapNode hit = null;
        EnumTree(
          m_tree, new RectD(0, 0, 1, 1), smpX, smpY, 0,
          (TreemapNode n, RectD nBounds, int depth) =>
          {
            if (smpX < nBounds.Left) return false;
            if (smpX >= nBounds.Right) return false;
            if (smpY < nBounds.Top) return false;
            if (smpY >= nBounds.Bottom) return false;

            hit = n;

            return true;
          });
        return hit;
      }

      return null;
    }

    protected override void glDraw()
    {
      if (m_tree != null)
      {
        InitResources();

        ResetViewport();
        PrepareViewport();

        if (m_dirty)
        {
          LayoutTree(m_tree);
          m_dirty = false;
        }

        RectD vBounds = View;

        double pxUnitsX = GetSamplesPerPixelX();
        double pxUnitsY = GetSamplesPerPixelY();

        OpenGL.glBegin(OpenGL.GL_QUADS);
        FillNode(m_tree, new RectD(0, 0, 1, 1), vBounds, pxUnitsX, pxUnitsY, 0);
        OpenGL.glEnd();

        float windowAspect = ClientSize.Width / (float)ClientSize.Height;
        float viewportAspect = (float)(View.Width / View.Height);

        EnumTree(
          m_tree, new RectD(0, 0, 1, 1), pxUnitsX, pxUnitsY, 0,
          (TreemapNode n, RectD nBounds, int depth) =>
          {
            if (Overlaps(nBounds, vBounds))
            {
              RectD titleBounds = new RectD(
                nBounds.Left,
                nBounds.Top,
                nBounds.Right,
                nBounds.Top + nBounds.Height * 0.03f);
              if (titleBounds.Height > pxUnitsY * 3)
              {
                float yTextScale = (float)(titleBounds.Height / m_titleFont.Height);
                float xTextScale = yTextScale / (windowAspect / viewportAspect);
                m_titleFont.Draw((float)titleBounds.Left, (float)titleBounds.Top, Color.Black, n.Name, xTextScale, yTextScale);
              }
              return true;
            }

            return false;
          });

        OpenGL.glBegin(OpenGL.GL_LINES);
        StrokeNode(m_tree, new RectD(0, 0, 1, 1), vBounds, pxUnitsX, pxUnitsY, 0);
        OpenGL.glEnd();
      }
    }

    protected virtual string FormatTooltip(TreemapNode node)
    {
      return node.Name;
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
      if (e.Button == MouseButtons.None)
      {
        if (e.X != m_lastPoint.X || e.Y != m_lastPoint.Y)
        {
          TreemapNode selectedNode = HitTest(e.Location);

          if (selectedNode != m_hoverNode)
          {
            string tt = string.Empty;

            m_hoverNode = selectedNode;
            if (selectedNode != null)
              tt = FormatTooltip(selectedNode);

            m_tooltip.SetToolTip(this, tt);
          }
        }

        m_lastPoint = new Point(e.X, e.Y);
      }

      base.OnMouseMove(e);
    }

    protected virtual void OnSelectedNode()
    {
      if (SelectionChanged != null)
        SelectionChanged(this, EventArgs.Empty);
    }

    protected override void OnLClick(MouseEventArgs e)
    {
      base.OnLClick(e);

      TreemapNode node = HitTest(e.Location);
      m_selectNode = node;
      if (m_selectNode != null)
        OnSelectedNode();
    }

    private void InitResources()
    {
      if (m_titleFont == null)
      {
        Font font = new Font(FontFamily.GenericSansSerif, 64.0f, GraphicsUnit.Pixel);
        m_titleFont = new GLFont(font);
      }
    }

    private delegate bool EnumTreeHandler(TreemapNode n, RectD nBounds, int depth);

    private void EnumTree(TreemapNode n, RectD nBounds, double pxX, double pxY, int depth, EnumTreeHandler h)
    {
      if (h(n, nBounds, depth))
      {
        RectD conBounds = new RectD(
          nBounds.Left + nBounds.Width * 0.01f,
          nBounds.Top + nBounds.Height * 0.03f,
          nBounds.Right - nBounds.Width * 0.01f,
          nBounds.Bottom - nBounds.Height * 0.01f);
        for (int i = 0, c = n.Children.Count; i != c; ++i)
        {
          TreemapNode cn = n.Children[i];

          RectD cBounds = new RectD(
            cn.Bounds.Left * conBounds.Width + conBounds.Left,
            cn.Bounds.Top * conBounds.Height + conBounds.Top,
            cn.Bounds.Right * conBounds.Width + conBounds.Left,
            cn.Bounds.Bottom * conBounds.Height + conBounds.Top);

          if (cBounds.Width > pxX * 5)
          {
            cBounds.Left += pxX * m_pxNodeMargin;
            cBounds.Right -= pxX * m_pxNodeMargin;
          }

          if (cBounds.Height > pxY * 5)
          {
            cBounds.Top += pxY * m_pxNodeMargin;
            cBounds.Bottom -= pxY * m_pxNodeMargin;
          }

          EnumTree(cn, cBounds, pxX, pxY, depth + 1, h);
        }
      }
    }

    private static bool Overlaps(RectD a, RectD b)
    {
      if (a.Right <= b.Left) return false;
      if (b.Right <= a.Left) return false;
      if (a.Bottom <= b.Top) return false;
      if (b.Bottom <= a.Top) return false;
      return true;
    }

    private void FillNode(TreemapNode n, RectD nBounds, RectD vBounds, double pxX, double pxY, int depth)
    {
      if (Overlaps(nBounds, vBounds))
      {
        GLTexture tex = n.Texture;
        if (tex != null && tex.Valid)
        {
          OpenGL.glEnd();
          OpenGL.glEnable(OpenGL.GL_TEXTURE_2D);
          OpenGL.glDisable(OpenGL.GL_BLEND);
          tex.Bind();

          OpenGL.glColor3f(1.0f, 1.0f, 1.0f);
          OpenGL.glBegin(OpenGL.GL_QUADS);
        }
        else
        {
          RGB col = new RGB(new HSV(depth * (1.0f / 6.0f), 0.5f - depth / 256.0f, 1));
          OpenGL.glColor3f(col.r, col.g, col.b);
        }

        {
          OpenGL.glTexCoord2f(0.0f, 0.0f);
          OpenGL.glVertex2f((float)nBounds.Left, (float)nBounds.Top);

          OpenGL.glTexCoord2f(0.0f, 1.0f);
          OpenGL.glVertex2f((float)nBounds.Left, (float)nBounds.Bottom);

          OpenGL.glTexCoord2f(1.0f, 1.0f);
          OpenGL.glVertex2f((float)nBounds.Right, (float)nBounds.Bottom);

          OpenGL.glTexCoord2f(1.0f, 0.0f);
          OpenGL.glVertex2f((float)nBounds.Right, (float)nBounds.Top);
        }

        if (tex != null && tex.Valid)
        {
          OpenGL.glEnd();
          tex.Unbind();
          OpenGL.glDisable(OpenGL.GL_TEXTURE_2D);
          OpenGL.glEnable(OpenGL.GL_BLEND);
          OpenGL.glBegin(OpenGL.GL_QUADS);
        }

        RectD conBounds = new RectD(
          nBounds.Left + nBounds.Width * 0.01f,
          nBounds.Top + nBounds.Height * 0.03f,
          nBounds.Right - nBounds.Width * 0.01f,
          nBounds.Bottom - nBounds.Height * 0.01f);

        for (int i = 0, c = n.Children.Count; i != c; ++i)
        {
          TreemapNode cn = n.Children[i];

          RectD cBounds = new RectD(
            cn.Bounds.Left * conBounds.Width + conBounds.Left,
            cn.Bounds.Top * conBounds.Height + conBounds.Top,
            cn.Bounds.Right * conBounds.Width + conBounds.Left,
            cn.Bounds.Bottom * conBounds.Height + conBounds.Top);

          if (cBounds.Width > pxX * 5)
          {
            cBounds.Left += pxX * m_pxNodeMargin;
            cBounds.Right -= pxX * m_pxNodeMargin;
          }

          if (cBounds.Height > pxY * 5)
          {
            cBounds.Top += pxY * m_pxNodeMargin;
            cBounds.Bottom -= pxY * m_pxNodeMargin;
          }

          FillNode(cn, cBounds, vBounds, pxX, pxY, depth + 1);
        }
      }
    }

    private void StrokeNode(TreemapNode n, RectD nBounds, RectD vBounds, double pxX, double pxY, int depth)
    {
      if (Overlaps(nBounds, vBounds))
      {
        OpenGL.glColor3f(0, 0, 0);

        {
          OpenGL.glVertex2f((float)nBounds.Left, (float)nBounds.Top);
          OpenGL.glVertex2f((float)nBounds.Left, (float)nBounds.Bottom);

          OpenGL.glVertex2f((float)nBounds.Left, (float)nBounds.Bottom);
          OpenGL.glVertex2f((float)nBounds.Right, (float)nBounds.Bottom);

          OpenGL.glVertex2f((float)nBounds.Right, (float)nBounds.Bottom);
          OpenGL.glVertex2f((float)nBounds.Right, (float)nBounds.Top);

          OpenGL.glVertex2f((float)nBounds.Right, (float)nBounds.Top);
          OpenGL.glVertex2f((float)nBounds.Left, (float)nBounds.Top);
        }

        RectD conBounds = new RectD(
          nBounds.Left + nBounds.Width * 0.01f,
          nBounds.Top + nBounds.Height * 0.03f,
          nBounds.Right - nBounds.Width * 0.01f,
          nBounds.Bottom - nBounds.Height * 0.01f);

        for (int i = 0, c = n.Children.Count; i != c; ++i)
        {
          TreemapNode cn = n.Children[i];

          RectD cBounds = new RectD(
            cn.Bounds.Left * conBounds.Width + conBounds.Left,
            cn.Bounds.Top * conBounds.Height + conBounds.Top,
            cn.Bounds.Right * conBounds.Width + conBounds.Left,
            cn.Bounds.Bottom * conBounds.Height + conBounds.Top);

          if (cBounds.Width > pxX * 5)
          {
            cBounds.Left += pxX * m_pxNodeMargin;
            cBounds.Right -= pxX * m_pxNodeMargin;
          }

          if (cBounds.Height > pxY * 5)
          {
            cBounds.Top += pxY * m_pxNodeMargin;
            cBounds.Bottom -= pxY * m_pxNodeMargin;
          }

          StrokeNode(cn, cBounds, vBounds, pxX, pxY, depth + 1);
        }
      }
    }

    static private bool CompareRatio(List<TreemapNode> row, TreemapNode next, double w)
    {
      double mn = float.MaxValue;
      double sum = 0.0f;
      double mx = 0.0f;

      for (int i = 0, c = row.Count; i != c; ++ i)
      {
        TreemapNode n = row[i];
        mx = Math.Max(mx, n.Size);
        mn = Math.Min(mn, n.Size);
        sum += n.Size;
      }

      double sumSq = sum * sum;
      double wSq = w * w;
      double ratio = Math.Max((wSq * mx) / sumSq, sumSq / (wSq * mn));

      double nextMx = Math.Max(mx, next.Size);
      double nextMn = Math.Min(mn, next.Size);
      double nextSum = sum + next.Size;

      double nextSumSq = nextSum * nextSum;
      double nextRatio = Math.Max((wSq * nextMx) / nextSumSq, nextSumSq / (wSq * nextMn));

      return ratio <= nextRatio;
    }

    static private void LayoutRow(ref RectD bounds, List<TreemapNode> nodes, out RectD remainder)
    {
      double sum = 0.0;

      for (int i = 0, c = nodes.Count; i != c; ++ i)
      {
        TreemapNode n = nodes[i];
        sum += n.Size;
      }

      double boundsWidth = bounds.Width;
      double boundsHeight = bounds.Height;
      double stride = sum / Math.Min(boundsWidth, boundsHeight);
      bool horz = bounds.Width < bounds.Height;

      double x = bounds.Left;
      double y = bounds.Top;

      for (int i = 0, c = nodes.Count; i != c; ++i)
      {
        TreemapNode n = nodes[i];
        double d = n.Size / stride;

        if (horz)
        {
          n.Bounds = new RectD(x, y, x + d, y + stride);
          x += d;
        }
        else
        {
          n.Bounds = new RectD(x, y, x + stride, y + d);
          y += d;
        }
      }

      if (horz)
        remainder = new RectD(bounds.Left, bounds.Top + stride, bounds.Right, bounds.Bottom);
      else
        remainder = new RectD(bounds.Left + stride, bounds.Top, bounds.Right, bounds.Bottom);
    }

    static private void Squarify(List<TreemapNode> nodes)
    {
      RectD bounds = new RectD(0, 0, 1, 1);
      int ni = 0;

      List<TreemapNode> row = new List<TreemapNode>();
      do
      {
        row.Clear();

        double w = Math.Min(bounds.Width, bounds.Height);

        for (int c = nodes.Count; ni != c; ++ni)
        {
          TreemapNode n = nodes[ni];
          if (CompareRatio(row, n, w))
            break;
          row.Add(n);
        }

        RectD remainder;
        LayoutRow(ref bounds, row, out remainder);
        bounds = remainder;
      }
      while (ni < nodes.Count);
    }

    static private void LayoutTree(TreemapNode tree)
    {
      Squarify(tree.Children);

      for (int i = 0, c = tree.Children.Count; i != c; ++i)
        LayoutTree(tree.Children[i]);
    }

    private GLFont m_titleFont;
    private TreemapNode m_tree;
    private bool m_dirty;
  }
}
