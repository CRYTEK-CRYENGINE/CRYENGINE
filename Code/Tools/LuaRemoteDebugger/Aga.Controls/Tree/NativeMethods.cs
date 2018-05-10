// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Drawing;
using System.Runtime.InteropServices;


namespace Aga.Controls.Tree
{
    internal static class NativeMethods
    {
        public const int DCX_WINDOW = 0x01;
        public const int DCX_CACHE = 0x02;
        public const int DCX_NORESETATTRS = 0x04;
        public const int DCX_CLIPCHILDREN = 0x08;
        public const int DCX_CLIPSIBLINGS = 0x10;
        public const int DCX_PARENTCLIP = 0x20;
        public const int DCX_EXCLUDERGN = 0x40;
        public const int DCX_INTERSECTRGN = 0x80;
        public const int DCX_EXCLUDEUPDATE = 0x100;
        public const int DCX_INTERSECTUPDATE = 0x200;
        public const int DCX_LOCKWINDOWUPDATE = 0x400;
        public const int DCX_VALIDATE = 0x200000;

        public const int WM_THEMECHANGED = 0x031A;
        public const int WM_NCPAINT = 0x85;
        public const int WM_NCCALCSIZE = 0x83;

        public const int WS_BORDER = 0x800000;
        public const int WS_EX_CLIENTEDGE = 0x200;

        public const int WVR_HREDRAW = 0x100;
        public const int WVR_VREDRAW = 0x200;
        public const int WVR_REDRAW = (WVR_HREDRAW | WVR_VREDRAW);

        [StructLayout(LayoutKind.Sequential)]
        public struct NCCALCSIZE_PARAMS
        {
            public RECT rgrc0, rgrc1, rgrc2;
            public IntPtr lppos;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;

            public static RECT FromRectangle(Rectangle rectangle)
            {
                RECT result = new RECT();
                result.Left = rectangle.Left;
                result.Top = rectangle.Top;
                result.Right = rectangle.Right;
                result.Bottom = rectangle.Bottom;
                return result;
            }

            public Rectangle ToRectangle()
            {
                return new Rectangle(Left, Top, Right - Left, Bottom - Top);
            }
        }

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr GetDCEx(IntPtr hWnd, IntPtr hrgnClip, int flags);

        [DllImport("user32.dll")]
        public static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    }
}
