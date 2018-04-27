// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Drawing.Imaging;

namespace Aga.Controls
{
	public static class BitmapHelper
	{
		[StructLayout(LayoutKind.Sequential)]
		private struct PixelData
		{
			public byte B;
			public byte G;
			public byte R;
			public byte A;
		}

		public static void SetAlphaChanelValue(Bitmap image, byte value)
		{
			if (image == null)
				throw new ArgumentNullException("image");
			if (image.PixelFormat != PixelFormat.Format32bppArgb)
				throw new ArgumentException("Wrong PixelFormat");

			BitmapData bitmapData = image.LockBits(new Rectangle(0, 0, image.Width, image.Height),
									 ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
			unsafe
			{
				PixelData* pPixel = (PixelData*)bitmapData.Scan0;
				for (int i = 0; i < bitmapData.Height; i++)
				{
					for (int j = 0; j < bitmapData.Width; j++)
					{
						pPixel->A = value;
						pPixel++;
					}
					pPixel += bitmapData.Stride - (bitmapData.Width * 4);
				}
			}
			image.UnlockBits(bitmapData);
		}
	}
}
