// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using System.Xml.Serialization;

namespace Statoscope
{
	public class RGB
	{
		[XmlAttribute] public float r, g, b;

		public RGB()
		{
		}

		public RGB(float r_, float g_, float b_)
		{
			r = r_;
			g = g_;
			b = b_;
		}

		public RGB(HSV hsv)
		{
			float h = hsv.h * 360.0f;
			float hOver60f = h / 60.0f;
			int hOver60i = (int)hOver60f;
			int hi = hOver60i % 6;
			float f = hOver60f - hOver60i;
			float p = hsv.v * (1.0f - hsv.s);
			float q = hsv.v * (1.0f - f * hsv.s);
			float t = hsv.v * (1.0f - (1.0f - f) * hsv.s);
			float v = hsv.v;

			switch (hi)
			{
				case 0:
					r = v; g = t; b = p;
					return;
				case 1:
					r = q; g = v; b = p;
					return;
				case 2:
					r = p; g = v; b = t;
					return;
				case 3:
					r = p; g = q; b = v;
					return;
				case 4:
					r = t; g = p; b = v;
					return;
				case 5:
					r = v; g = p; b = q;
					return;
			}
		}

		public RGB(System.Drawing.Color sysCol)
		{
			r = sysCol.R / 255.0f;
			g = sysCol.G / 255.0f;
			b = sysCol.B / 255.0f;
		}

    public uint ConvertToArgbUint()
    {
      float rc = Math.Min(255.0f, Math.Max(0.0f, r * 255.0f));
      float gc = Math.Min(255.0f, Math.Max(0.0f, g * 255.0f));
      float bc = Math.Min(255.0f, Math.Max(0.0f, b * 255.0f));
      return ((uint)rc << 16) | ((uint)gc << 8) | ((uint)bc);
    }

    public uint ConvertToAbgrUint()
    {
      float rc = Math.Min(255.0f, Math.Max(0.0f, r * 255.0f));
      float gc = Math.Min(255.0f, Math.Max(0.0f, g * 255.0f));
      float bc = Math.Min(255.0f, Math.Max(0.0f, b * 255.0f));
      return ((uint)bc << 16) | ((uint)gc << 8) | ((uint)rc);
    }

		public System.Drawing.Color ConvertToSysDrawCol()
		{
      float rc = Math.Min(255.0f, Math.Max(0.0f, r * 255.0f));
      float gc = Math.Min(255.0f, Math.Max(0.0f, g * 255.0f));
      float bc = Math.Min(255.0f, Math.Max(0.0f, b * 255.0f));
      return System.Drawing.Color.FromArgb((int)rc, (int)gc, (int)bc);
		}

		public static RGB RandomHueRGB()
		{
			return new RGB(new HSV((float)StatoscopeForm.s_rng.NextDouble(), 1.0f, 1.0f));
		}
	}

	public class HSV
	{
		public float h, s, v;
		public HSV(float h_, float s_, float v_)
		{
			h = h_;
			s = s_;
			v = v_;
		}
	}

	class MTCounter
	{
		// a thread safe counter that can be used to check for completion of threaded tasks
		// e.g. each task Increment()s the counter and another thread can WaitUntil() on the number of started tasks

		int m_num;
		int m_max;

		public MTCounter()
		{
			m_num = 0;
			m_max = 0;
		}

		public MTCounter(int max)
		{
			m_num = 0;
			m_max = max;
		}

		public void Increment()
		{
			Increment(1);
		}

		public void Increment(int n)
		{
			lock (this)
			{
				m_num += n;
				Monitor.Pulse(this);
			}
		}

		public void WaitUntil(int val)
		{
			lock (this)
			{
				while (m_num != val)
				{
					Monitor.Wait(this);
				}
			}
		}

		public void WaitUntilMax()
		{
			lock (this)
			{
				while (m_num != m_max)
				{
					Monitor.Wait(this);
				}
			}
		}
	}

	static class Extensions
	{
		public static string[] PathLocations(this string str)
		{
			return str.Split(new char[] { '/' }, StringSplitOptions.RemoveEmptyEntries);
		}

		public static string BasePath(this string path)
		{
			int indexOfLastSlash = path.LastIndexOf("/");
			return path.Substring(0, indexOfLastSlash);
		}

		public static string Name(this string path)
		{
			int indexOfLastSlash = path.LastIndexOf("/");
			return path.Substring(indexOfLastSlash + 1);
		}

    public static void PaintNow(this Control ctrl)
    {
      SafeNativeMethods.SendMessage(ctrl.Handle, 0xf, (IntPtr)0, (IntPtr)0);
    }
	}

	static class SafeNativeMethods
	{
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

		[DllImport("shlwapi.dll", CharSet = CharSet.Unicode)]
		public static extern int StrCmpLogicalW(string psz1, string psz2);
	}
}