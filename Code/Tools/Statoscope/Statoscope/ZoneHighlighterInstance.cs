// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;
using System.Linq;
using CsGL.OpenGL;

namespace Statoscope
{
	public class ZoneHighlighterInstance
	{
		public readonly ZoneHighlighterRDI ZRDI;
		public readonly LogView LogView;
		List<XRange> Ranges = new List<XRange>();

		public ZoneHighlighterInstance(ZoneHighlighterRDI zrdi, LogView logView)
		{
			ZRDI = zrdi;
			LogView = logView;
			Reset();
		}

		public void Update(int fromIdx, int toIdx)
		{
			ZRDI.UpdateInstance(this, fromIdx, toIdx);
		}

		public void Reset()
		{
			Ranges.Clear();
			Update(0, LogView.m_maxValidBaseIdx);
		}

		public void Draw(HighlightZoneContext ctx)
		{
			if (!ZRDI.Displayed)
				return;

			float z = 0.5f;
      OpenGL.glColor4f(ZRDI.Colour.r, ZRDI.Colour.g, ZRDI.Colour.b, 0.25f);
      OpenGL.glBegin(OpenGL.GL_QUADS);

			foreach (XRange range in Ranges)
			{
				float start = range.Start.GetDisplayXValue(LogView.m_logData, ctx.m_againstFrameTime);
				if (range.IsOpen)
				{
					OpenGL.glVertex3f(start, ctx.m_minY, z);
					OpenGL.glVertex3f(start, ctx.m_maxY, z);
					OpenGL.glVertex3f(ctx.m_maxX, ctx.m_maxY, z);
					OpenGL.glVertex3f(ctx.m_maxX, ctx.m_minY, z);
				}
				else
				{
					float end = range.End.GetDisplayXValue(LogView.m_logData, ctx.m_againstFrameTime);
					OpenGL.glVertex3f(start, ctx.m_minY, z);
					OpenGL.glVertex3f(start, ctx.m_maxY, z);
					OpenGL.glVertex3f(end, ctx.m_maxY, z);
					OpenGL.glVertex3f(end, ctx.m_minY, z);
				}
			}

      OpenGL.glEnd();
		}

		public XRange StartRange(FrameRecord fr)
		{
			XRange activeRange = new XRange();
			activeRange.Start = fr;
			Ranges.Add(activeRange);
			return activeRange;
		}

		public XRange EndRange(FrameRecord fr, XRange activeRange)
		{
			activeRange.End = fr;
			return null;
		}

		public XRange ActiveRange
		{
			get
			{
				if (Ranges.Count > 0 && Ranges.Last().IsOpen)
					return Ranges.Last();
				else
					return null;
			}
		}

		public class XRange
		{
			public FrameRecord Start = null;
			public FrameRecord End = null;
			public bool IsOpen { get { return End == null; } }
		}

		public class HighlightZoneContext
		{
			public bool m_againstFrameTime;
			public float m_minX;
			public float m_maxX;
			public float m_minY;
			public float m_maxY;

			public HighlightZoneContext(bool againstFrameTime, float minX, float maxX, float minY, float maxY)
			{
				m_againstFrameTime = againstFrameTime;
				m_minX = minX;
				m_maxX = maxX;
				m_minY = minY;
				m_maxY = maxY;
			}
		}
	}
}
