// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Statoscope
{
	class ImageProcessor
	{
    public static MemoryStream CreateImageStreamFromScreenshotBytes(byte[] data)
    {
      int scaleFactor = (data[2] == 0) ? 1 : data[2];
      int width = data[0] * scaleFactor;
      int height = data[1] * scaleFactor;

      Bitmap bmp = new Bitmap(width, height, System.Drawing.Imaging.PixelFormat.Format24bppRgb);

      System.Drawing.Rectangle rect = new System.Drawing.Rectangle(0, 0, width, height);
      System.Drawing.Imaging.BitmapData bmpData = bmp.LockBits(rect, System.Drawing.Imaging.ImageLockMode.WriteOnly, bmp.PixelFormat);
      System.Runtime.InteropServices.Marshal.Copy(data, 3, bmpData.Scan0, bmpData.Stride * height);
      bmp.UnlockBits(bmpData);

			// jpeg compress the image to save memory
			MemoryStream memStrm = new MemoryStream();
			bmp.Save(memStrm, JpegEncoder, EncoderParams);

      return memStrm;
    }

		static ImageCodecInfo s_jpegEncoder = null;
		static ImageCodecInfo JpegEncoder
		{
			get
			{
				if (s_jpegEncoder == null)
				{
					ImageCodecInfo[] codecs = ImageCodecInfo.GetImageDecoders();

					foreach (ImageCodecInfo codec in codecs)
					{
						if (codec.FormatID == ImageFormat.Jpeg.Guid)
						{
							s_jpegEncoder = codec;
							break;
						}
					}
				}

				return s_jpegEncoder;
			}
		}

		static EncoderParameters s_encoderParams = null;
		static EncoderParameters EncoderParams
		{
			get
			{
				if (s_encoderParams == null)
				{
					s_encoderParams = new EncoderParameters(1);
					EncoderParameter encoderParam = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, 75L);
					s_encoderParams.Param[0] = encoderParam;
				}

				return s_encoderParams;
			}
		}
	}
}
