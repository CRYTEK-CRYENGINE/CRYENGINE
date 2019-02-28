// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using CsGL.OpenGL;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace Statoscope
{
  public class AAOpenGLControl : Control
  {
    protected void MakeCurrent()
    {
      WGL.wglMakeCurrent(m_hDC, m_hRC);
    }

    protected override void OnCreateControl()
    {
      base.OnCreateControl();

      int samples = 16;
      int pixelFormat = GetAAPixelFormat(ref samples);

      PIXELFORMATDESCRIPTOR pfd = new PIXELFORMATDESCRIPTOR();
      pfd.nSize = (ushort)Marshal.SizeOf(typeof(PIXELFORMATDESCRIPTOR));
      pfd.nVersion = 1;
      pfd.dwFlags = PixelFlag.PFD_DRAW_TO_WINDOW | PixelFlag.PFD_SUPPORT_OPENGL | PixelFlag.PFD_DOUBLEBUFFER;
      pfd.iPixelType = PixelType.PFD_TYPE_RGBA;
      pfd.cColorBits = 32;

      m_hDC = GDI.GetDC(Handle);

      GDI.SetPixelFormat(m_hDC, pixelFormat, ref pfd);
      m_hRC = WGL.wglCreateContext(m_hDC);
      
      WGL.wglMakeCurrent(m_hDC, m_hRC);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
      MakeCurrent();

      glDraw();
      OpenGL.glFinish();
      GDI.SwapBuffers(m_hDC);
    }

    protected virtual void glDraw()
    {
    }

    private static GDI.WndProc s_defWindowProc = GDI.DefWindowProc;
    private static string s_dummyClass = GetDummyClass();

    static private string GetDummyClass()
    {
      GDI.WNDCLASSEX wc = new GDI.WNDCLASSEX();
      if (wc.cbSize == 0)
      {
        wc.cbSize = (ushort)Marshal.SizeOf(typeof(GDI.WNDCLASSEX));
        wc.style = GDI.ClassStyles.OwnDC;
        wc.lpfnWndProc = s_defWindowProc;
        wc.hInstance = GDI.GetModuleHandle(null);
        wc.lpszClassName = "DummyGLWindowClass";

        if (GDI.RegisterClassEx(ref wc) == 0)
          return null;

        return wc.lpszClassName;
      }

      return wc.lpszClassName;
    }

    static private int GetAAPixelFormat(ref int samples)
    {
      PIXELFORMATDESCRIPTOR pfd = new PIXELFORMATDESCRIPTOR();
      pfd.nSize = (ushort)Marshal.SizeOf(typeof(PIXELFORMATDESCRIPTOR));
      pfd.nVersion = 1;
      pfd.dwFlags = PixelFlag.PFD_DRAW_TO_WINDOW | PixelFlag.PFD_SUPPORT_OPENGL | PixelFlag.PFD_DOUBLEBUFFER;
      pfd.iPixelType = PixelType.PFD_TYPE_RGBA;
      pfd.cColorBits = 32;

      IntPtr dummyHWnd = GDI.CreateWindowEx(0, s_dummyClass, "", GDI.WindowStyles.WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
      IntPtr dummyHDc = GDI.GetDC(dummyHWnd);

      int dummyPF = GDI.ChoosePixelFormat(dummyHDc, ref pfd);
      GDI.SetPixelFormat(dummyHDc, dummyPF, ref pfd);

      IntPtr dummyHRc = WGL.wglCreateContext(dummyHDc);
      WGL.wglMakeCurrent(dummyHDc, dummyHRc);

      int[] iattrs = new int[] {
        WGL.WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL.WGL_ACCELERATION_ARB, WGL.WGL_FULL_ACCELERATION_ARB,
        WGL.WGL_COLOR_BITS_ARB, 24,
        WGL.WGL_ALPHA_BITS_ARB, 8,
        WGL.WGL_DEPTH_BITS_ARB, 24,
        WGL.WGL_STENCIL_BITS_ARB, 8,
        WGL.WGL_DOUBLE_BUFFER_ARB, 1,
        WGL.WGL_SAMPLE_BUFFERS_ARB, 1,
        WGL.WGL_SAMPLES_ARB, 0,
        0, 0
      };

      WGL.wglChoosePixelFormatARBHandler wglChoosePixelFormatARB = (WGL.wglChoosePixelFormatARBHandler)WGL.GetProc<WGL.wglChoosePixelFormatARBHandler>("wglChoosePixelFormatARB");

      int pixelFormat = 0;
      if (wglChoosePixelFormatARB != null)
      {
        int reqSamples;
        for (reqSamples = samples; reqSamples > 0; reqSamples /= 2)
        {
          iattrs[17] = reqSamples;

          int[] pixelFormats = new int[16];
          int numFormats;

          unsafe
          {
            fixed (int* pPixelFormats = pixelFormats)
            {
              fixed (int* pIAttrs = iattrs)
              {
                int success = wglChoosePixelFormatARB(
                  dummyHDc,
                  pIAttrs, (float*)0, 16,
                  pPixelFormats, &numFormats);

                if (success != 0 && numFormats > 0)
                {
                  pixelFormat = pixelFormats[0];
                  samples = reqSamples;
                  break;
                }
              }
            }
          }
        }
      }
      else
      {
        samples = 1;
        pixelFormat = GDI.ChoosePixelFormat(dummyHDc, ref pfd);
      }

      WGL.wglMakeCurrent(dummyHDc, dummyHRc);
      WGL.wglDeleteContext(dummyHRc);
      GDI.ReleaseDC(dummyHWnd, dummyHDc);
      GDI.DestroyWindow(dummyHWnd);

      return pixelFormat;
    }

    private IntPtr m_hDC;
    private IntPtr m_hRC;
  }
}
