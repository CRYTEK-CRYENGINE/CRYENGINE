// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;

namespace CryEngine
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// Contains the extension methods for <see cref="Bitmap"/>s.
	/// </summary>
	public static class BitmapExtensions
	{
		/// <summary>
		/// Returns the pixel format of the <see cref="Bitmap"/>.
		/// </summary>
		/// <param name="bmp"></param>
		/// <returns></returns>
		public static int GetBPP(this Bitmap bmp)
		{
			switch (bmp.PixelFormat)
			{
				case PixelFormat.Format8bppIndexed:
					return 1;
				case PixelFormat.Format24bppRgb:
					return 3;
				case PixelFormat.Format32bppArgb:
				case PixelFormat.Format32bppPArgb:
					return 4;
			}
			return 0;
		}

		/// <summary>
		/// Returns the pixels of the <see cref="Bitmap"/> as an array of bytes.
		/// </summary>
		/// <param name="bmp"></param>
		/// <returns></returns>
		public static byte[] GetPixels(this Bitmap bmp)
		{
			var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
			var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, bmp.PixelFormat);
			int bytes = Math.Abs(data.Stride) * bmp.Height;
			byte[] pix = new byte[bytes];
			System.Runtime.InteropServices.Marshal.Copy(data.Scan0, pix, 0, bytes);
			bmp.UnlockBits(data);

			var bpp = bmp.GetBPP();
			if(bpp == 3)
			{
				List<byte> pixels = new List<byte>(pix.Length / 3 * 4);
				for(int i = 0; i < pix.Length; i += bpp)
				{
					for(int p = 0; p < 3; ++p)
					{
						pixels.Add(pix[i + p]);
					}
					pixels.Add(byte.MaxValue);
				}
				pix = pixels.ToArray();
				bpp = 4;
			}
			Action<int> SwapRnB = (ofs) =>
			{
				byte p0 = pix[ofs];
				pix[ofs] = pix[ofs + 2];
				pix[ofs + 2] = p0;
			};
			Action<int> Swap = bpp == 4 ? SwapRnB : null;

			if (Swap == null)
				throw new Exception("Unsupported Pixelformat");

			for (int i = 0; i < bytes; i += bpp)
				Swap(i);

			return pix;
		}
	}
}
