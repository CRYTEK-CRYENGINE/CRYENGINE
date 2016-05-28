// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.Serialization;

/// <summary>
/// Manages assets (Scripts, Textures, etc.) which are used by application and framework.
/// </summary>
namespace CryEngine.Resources
{
	/// <summary>
	/// Handles and Creates Texture instances from various sources.
	/// </summary>
	[DataContract]
	public class ImageSource
	{
		Bitmap _rawImage = null;
		[DataMember]
		string _path;
		static ImageSource _blank;

		public Texture Texture { get; private set; } ///< The underlying Texture object.
		public int Width { get { return _rawImage.Width; } } ///< Height of the raw image.
		public int Height { get { return _rawImage.Height; } } ///< Width of the raw image.

		public static ImageSource Blank
		{
			get 
			{
				if (_blank == null) 
				{
					var bmp = new Bitmap (1, 1, PixelFormat.Format32bppPArgb);
					bmp.SetPixel (0, 0, Color.White.ToSystemColor ());
					_blank = new ImageSource (bmp);
				}
				return _blank;
			}
		} ///< Returns a static white texture.

		/// <summary>
		/// Creates an image from a given Bitmap.
		/// </summary>
		/// <param name="bmp">Data source.</param>
		private ImageSource(Bitmap bmp)
		{
			_path = "[Blank]";
			_rawImage = bmp;
			Texture = new Texture(Width, Height, bmp.GetPixels());
		}

		/// <summary>
		/// Loads an image from file and creates a Texture from it.
		/// </summary>
		/// <param name="path">The image source file.</param>
		/// <param name="filtered">If set to <c>true</c>, texture's high quality filtering is enabled.</param>
		public ImageSource(string path, bool filtered = true)
		{
			try
			{
				if (!System.IO.File.Exists(path))
					throw new System.IO.FileNotFoundException("Cannot load image with path: " + path);
				_path = path;
				_rawImage = new Bitmap(path);

				// Pre-processing option
				/*Rectangle rect = new Rectangle(0, 0, _rawImage.Width, _rawImage.Height);
				System.Drawing.Imaging.BitmapData bmpData = _rawImage.LockBits(rect, System.Drawing.Imaging.ImageLockMode.ReadWrite, _rawImage.PixelFormat);
				IntPtr ptr = bmpData.Scan0;
				int bytes = _rawImage.Width * _rawImage.Height * 4;
				byte[] rgbValues = new byte[bytes];
				System.Runtime.InteropServices.Marshal.Copy(ptr, rgbValues, 0, bytes);
				for (int counter = 0; counter < rgbValues.Length; counter+=4)
				{
					byte ia = (byte)(rgbValues[counter+3]);
					//rgbValues[counter] -= ia;
					//rgbValues[counter+1] -= ia;
					rgbValues[counter+3] = 128;
				}
				System.Runtime.InteropServices.Marshal.Copy(rgbValues, 0, ptr, bytes);
				_rawImage.UnlockBits(bmpData);*/

				Log.Info ("Loaded '" + path + "' (" + Width + "x" + Height + ")");
				Texture = new Texture(Width, Height, _rawImage.GetPixels(), filtered);
			}
			catch(Exception ex)
			{
				Log.Exception (ex);
			}
		}

		/// <summary>
		/// Returns a <see cref="System.String"/> that represents the current <see cref="CryEngine.Resources.ImageSource"/>.
		/// </summary>
		/// <returns>A <see cref="System.String"/> that represents the current <see cref="CryEngine.Resources.ImageSource"/>.</returns>
		public override string ToString()
		{
			return new System.IO.FileInfo(_path).Name;
		}
	}
}
