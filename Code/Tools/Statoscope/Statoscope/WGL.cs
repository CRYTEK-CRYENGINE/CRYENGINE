// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using CsGL.OpenGL;

namespace Statoscope
{
  static class WGL
  {
    [DllImport("opengl32.dll", CharSet=CharSet.Ansi)]
    public static extern IntPtr wglGetProcAddress(string name);

    [DllImport("opengl32.dll", CharSet = CharSet.Ansi, SetLastError=true)]
    public static extern IntPtr wglCreateContext(IntPtr hDC);

    [DllImport("opengl32.dll", CharSet = CharSet.Ansi)]
    public static extern int wglMakeCurrent(IntPtr hDC, IntPtr hRC);

    [DllImport("opengl32.dll", CharSet = CharSet.Ansi)]
    public static extern int wglDeleteContext(IntPtr hRC);

    public unsafe delegate int wglChoosePixelFormatARBHandler(IntPtr hdc, int* attribIList, float* attribFList, int maxFormats, int* formats, int* numFormats);

    public const int WGL_NUMBER_PIXEL_FORMATS_ARB = 0x2000;
    public const int WGL_DRAW_TO_WINDOW_ARB = 0x2001;
    public const int WGL_DRAW_TO_BITMAP_ARB = 0x2002;
    public const int WGL_ACCELERATION_ARB = 0x2003;
    public const int WGL_NEED_PALETTE_ARB = 0x2004;
    public const int WGL_NEED_SYSTEM_PALETTE_ARB = 0x2005;
    public const int WGL_SWAP_LAYER_BUFFERS_ARB = 0x2006;
    public const int WGL_SWAP_METHOD_ARB = 0x2007;
    public const int WGL_NUMBER_OVERLAYS_ARB = 0x2008;
    public const int WGL_NUMBER_UNDERLAYS_ARB = 0x2009;
    public const int WGL_TRANSPARENT_ARB = 0x200A;
    public const int WGL_TRANSPARENT_RED_VALUE_ARB = 0x2037;
    public const int WGL_TRANSPARENT_GREEN_VALUE_ARB = 0x2038;
    public const int WGL_TRANSPARENT_BLUE_VALUE_ARB = 0x2039;
    public const int WGL_TRANSPARENT_ALPHA_VALUE_ARB = 0x203A;
    public const int WGL_TRANSPARENT_INDEX_VALUE_ARB = 0x203B;
    public const int WGL_SHARE_DEPTH_ARB = 0x200C;
    public const int WGL_SHARE_STENCIL_ARB = 0x200D;
    public const int WGL_SHARE_ACCUM_ARB = 0x200E;
    public const int WGL_SUPPORT_GDI_ARB = 0x200F;
    public const int WGL_SUPPORT_OPENGL_ARB = 0x2010;
    public const int WGL_DOUBLE_BUFFER_ARB = 0x2011;
    public const int WGL_STEREO_ARB = 0x2012;
    public const int WGL_PIXEL_TYPE_ARB = 0x2013;
    public const int WGL_COLOR_BITS_ARB = 0x2014;
    public const int WGL_RED_BITS_ARB = 0x2015;
    public const int WGL_RED_SHIFT_ARB = 0x2016;
    public const int WGL_GREEN_BITS_ARB = 0x2017;
    public const int WGL_GREEN_SHIFT_ARB = 0x2018;
    public const int WGL_BLUE_BITS_ARB = 0x2019;
    public const int WGL_BLUE_SHIFT_ARB = 0x201A;
    public const int WGL_ALPHA_BITS_ARB = 0x201B;
    public const int WGL_ALPHA_SHIFT_ARB = 0x201C;
    public const int WGL_ACCUM_BITS_ARB = 0x201D;
    public const int WGL_ACCUM_RED_BITS_ARB = 0x201E;
    public const int WGL_ACCUM_GREEN_BITS_ARB = 0x201F;
    public const int WGL_ACCUM_BLUE_BITS_ARB = 0x2020;
    public const int WGL_ACCUM_ALPHA_BITS_ARB = 0x2021;
    public const int WGL_DEPTH_BITS_ARB = 0x2022;
    public const int WGL_STENCIL_BITS_ARB = 0x2023;
    public const int WGL_AUX_BUFFERS_ARB = 0x2024;
    public const int WGL_NO_ACCELERATION_ARB = 0x2025;
    public const int WGL_GENERIC_ACCELERATION_ARB = 0x2026;
    public const int WGL_FULL_ACCELERATION_ARB = 0x2027;
    public const int WGL_SWAP_EXCHANGE_ARB = 0x2028;
    public const int WGL_SWAP_COPY_ARB = 0x2029;
    public const int WGL_SWAP_UNDEFINED_ARB = 0x202A;
    public const int WGL_TYPE_RGBA_ARB = 0x202B;
    public const int WGL_TYPE_COLORINDEX_ARB = 0x202C;
    public const int WGL_SAMPLE_BUFFERS_ARB = 0x2041;
    public const int WGL_SAMPLES_ARB = 0x2042;

    public static System.Delegate GetProc<T>(string name)
    {
      IntPtr p = wglGetProcAddress(name);
      if (p != IntPtr.Zero)
      {
        return Marshal.GetDelegateForFunctionPointer(p, typeof(T));
      }
      return null;
    }

    class DummyWindow : OpenGLControl
    {
    }
  }
}
