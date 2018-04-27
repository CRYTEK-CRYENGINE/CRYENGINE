// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using CsGL.OpenGL;

namespace Statoscope
{
  class GLFont : IDisposable
  {
    public int Height
    {
      get { return m_height; }
    }

    public GLFont(Font font)
    {
      m_atlasWidth = 512;
      m_atlasHeight = 512;

      m_font = font;
      m_bitmap = new Bitmap((int)m_atlasWidth, (int)m_atlasHeight, PixelFormat.Format32bppArgb);
      m_height = MeasureHeight();

      unsafe
      {
        uint tex;
        OpenGL.glGenTextures(1, &tex);
        m_texture = tex;

        OpenGL.glBindTexture(OpenGL.GL_TEXTURE_2D, m_texture);
        OpenGL.glPixelStorei(OpenGL.GL_UNPACK_ALIGNMENT, 1);
      }
    }

    public void Draw(float x, float y, Color color, string text)
    {
      Draw(x, y, color, text, 1.0f, 1.0f);
    }
    
    public void Draw(float x, float y, Color color, string text, float xScale, float yScale)
    {
      foreach (char c in text)
        LookupGlyph(c);

      if (m_dirty)
      {
        SyncTexture();
      }

      float txs = 1.0f / m_atlasWidth;
      float tys = 1.0f / m_atlasHeight;
      float cs = 1.0f / 255.0f;

      OpenGL.glPushAttrib(OpenGL.GL_ENABLE_BIT | OpenGL.GL_TEXTURE_BIT);
      OpenGL.glBindTexture(OpenGL.GL_TEXTURE_2D, m_texture);
			OpenGL.glTexParameteri(OpenGL.GL_TEXTURE_2D, OpenGL.GL_TEXTURE_MIN_FILTER, (int)OpenGL.GL_LINEAR_MIPMAP_LINEAR);
			OpenGL.glTexParameteri(OpenGL.GL_TEXTURE_2D, OpenGL.GL_TEXTURE_MAG_FILTER, (int)OpenGL.GL_LINEAR_MIPMAP_LINEAR);

      OpenGL.glEnable(OpenGL.GL_TEXTURE_2D);

      OpenGL.glBegin(OpenGL.GL_QUADS);
      OpenGL.glColor3f(color.R * cs, color.G * cs, color.B * cs);

      foreach (char c in text)
      {
        Glyph g = LookupGlyph(c);
        float gw = g.W * xScale;
        float gh = g.H * yScale;

        OpenGL.glTexCoord2f(g.X * txs, g.Y * tys);
        OpenGL.glVertex2f(x, y);

        OpenGL.glTexCoord2f(g.X * txs, (g.Y + g.H) * tys);
        OpenGL.glVertex2f(x, y + gh);

        OpenGL.glTexCoord2f((g.X + g.W) * txs, (g.Y + g.H) * tys);
        OpenGL.glVertex2f(x + gw, y + gh);

        OpenGL.glTexCoord2f((g.X + g.W) * txs, g.Y * tys);
        OpenGL.glVertex2f(x + gw, y);

        x += gw;
      }

      OpenGL.glEnd();
      OpenGL.glPopAttrib();
    }

    private Glyph LookupGlyph(char c)
    {
      ushort glyphId = m_glyphMap[(int)c];
      if (glyphId > 0)
        return m_glyphs[glyphId - 1];
      return BakeGlyph(c);
    }

    private int MeasureHeight()
    {
      using (Graphics g = Graphics.FromImage(m_bitmap))
      {
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
        string s = "l";
        StringFormat fmt = new StringFormat(StringFormat.GenericTypographic);
        fmt.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces;
        fmt.FormatFlags |= StringFormatFlags.NoClip;
        fmt.SetMeasurableCharacterRanges(new CharacterRange[] { new CharacterRange(0, 1) });

        Region[] regions = g.MeasureCharacterRanges(s, m_font, new RectangleF(0.0f, 0.0f, 512.0f, 512.0f), fmt);
        RectangleF bounds = regions[0].GetBounds(g);
        return (int)Math.Ceiling(bounds.Height);
      }
    }

		public float MeasureWidth(string s)
		{
      using (Graphics g = Graphics.FromImage(m_bitmap))
      {
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
        StringFormat fmt = new StringFormat(StringFormat.GenericTypographic);
        fmt.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces;
        fmt.FormatFlags |= StringFormatFlags.NoClip;
				return g.MeasureString(s, m_font, new PointF(), fmt).Width;
      }
		}

    private Glyph BakeGlyph(char c)
    {
      ushort glyphIdx = (ushort)m_glyphs.Count;
      m_glyphMap[(int)c] = (ushort)(glyphIdx + 1);

      Glyph glyph;

      using (Graphics g = Graphics.FromImage(m_bitmap))
      {
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
        string s = c.ToString();
        StringFormat fmt = new StringFormat(StringFormat.GenericTypographic);
        fmt.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces;
        fmt.FormatFlags |= StringFormatFlags.NoClip;
        fmt.SetMeasurableCharacterRanges(new CharacterRange[] { new CharacterRange(0, 1) });

        Region[] regions = g.MeasureCharacterRanges(s, m_font, new RectangleF(0.0f, 0.0f, 512.0f, 512.0f), fmt);
        RectangleF bounds = regions[0].GetBounds(g);

        if (m_atlasCursorX + bounds.Right > m_atlasWidth)
        {
          m_atlasCursorY += m_atlasCursorHeight;
          m_atlasCursorX = 0;
          m_atlasCursorHeight = 0;
        }

        g.DrawString(s, m_font, Brushes.White, new PointF(m_atlasCursorX, m_atlasCursorY), fmt);

        ushort gw = (ushort)Math.Ceiling(bounds.Right);
        ushort gh = (ushort)Math.Ceiling(bounds.Bottom);
        glyph = new Glyph((ushort)m_atlasCursorX, (ushort)m_atlasCursorY, gw, gh);

        m_atlasCursorX += (int)gw + 1;
        m_atlasCursorHeight = Math.Max(gh + 1, m_atlasCursorHeight);
      }

      m_dirty = true;

      m_glyphs.Add(glyph);
      return glyph;
    }

    private void SyncTexture()
    {
      OpenGL.glBindTexture(OpenGL.GL_TEXTURE_2D, m_texture);

      unsafe
      {
        BitmapData bd = m_bitmap.LockBits(new Rectangle(0, 0, m_bitmap.Width, m_bitmap.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);

        if (bd.Stride != bd.Width * 4)
          throw new ApplicationException("Stride not a multiple of width");

        //OpenGL.glTexImage2D(OpenGL.GL_TEXTURE_2D, 0, (int)OpenGL.GL_RGBA8, (int)m_atlasWidth, (int)m_atlasHeight, 0, OpenGL.GL_BGRA_EXT, OpenGL.GL_UNSIGNED_BYTE, bd.Scan0.ToPointer());
        GLU.gluBuild2DMipmaps(OpenGL.GL_TEXTURE_2D, (int)OpenGL.GL_ALPHA, (int)m_atlasWidth, (int)m_atlasHeight, OpenGL.GL_BGRA_EXT, OpenGL.GL_UNSIGNED_BYTE, bd.Scan0.ToPointer());

        m_bitmap.UnlockBits(bd);
      }

      m_dirty = false;
    }

    #region IDisposable Members

    public void Dispose()
    {
      if (m_texture != 0)
      {
        unsafe
        {
          uint tex = m_texture;
          OpenGL.glDeleteTextures(1, &tex);
          m_texture = 0;
        }
      }

      if (m_bitmap != null)
      {
        m_bitmap.Dispose();
        m_bitmap = null;
      }
    }

    #endregion

    private struct Glyph
    {
      public ushort X, Y, W, H;

      public Glyph(ushort x, ushort y, ushort w, ushort h)
      {
        X = x;
        Y = y;
        W = w;
        H = h;
      }
    }

    private Font m_font;
    private uint m_texture;

    private ushort[] m_glyphMap = new ushort[64 * 1024];
    private List<Glyph> m_glyphs = new List<Glyph>();
    private Bitmap m_bitmap;
    private int m_height;
    private bool m_dirty;

    private uint m_atlasWidth;
    private uint m_atlasHeight;
    private int m_atlasCursorX;
    private int m_atlasCursorY;
    private int m_atlasCursorHeight;
  }
}
