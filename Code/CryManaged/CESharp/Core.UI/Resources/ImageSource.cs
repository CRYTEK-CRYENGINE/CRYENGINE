// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.Serialization;
using CryEngine.UI;

namespace CryEngine.Resources
{
	/// <summary>
	/// Handles and Creates Texture instances from various sources.
	/// </summary>
	[DataContract]
	public class ImageSource
	{
		private static ImageSource _blank;

		private readonly Bitmap _rawImage = null;
		private readonly string _path;

		/// <summary>
		/// The underlying Texture object.
		/// </summary>
		/// <value>The texture.</value>
		public Graphic Texture { get; private set; }

		/// <summary>
		/// Height of the raw image.
		/// </summary>
		/// <value>The width.</value>
		public int Width { get { return _rawImage.Width; } }

		/// <summary>
		/// Width of the raw image.
		/// </summary>
		/// <value>The height.</value>
		public int Height { get { return _rawImage.Height; } }

		/// <summary>
		/// Returns a static white texture.
		/// </summary>
		/// <value>The blank.</value>
		public static ImageSource Blank
		{
			get
			{
				if (_blank == null)
				{
					var bmp = new Bitmap(1, 1, PixelFormat.Format32bppPArgb);
					bmp.SetPixel(0, 0, Color.White.ToSystemColor());
					_blank = new ImageSource(bmp);
				}
				return _blank;
			}
		}

		/// <summary>
		/// Creates an image from a given Bitmap.
		/// </summary>
		/// <param name="bmp">Data source.</param>
		private ImageSource(Bitmap bmp)
		{
			_path = "[Blank]";
			_rawImage = bmp;
			Texture = new Graphic(Width, Height, bmp.GetPixels(), false, false, false, "BLANK_IMAGE");
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
				{
					throw new System.IO.FileNotFoundException("Cannot load image with path: " + path);
				}

				_path = path;
				_rawImage = new Bitmap(path);

				Log.Info("Loaded '" + path + "' (" + Width + "x" + Height + ")");
				string filename = System.IO.Path.GetFileNameWithoutExtension(path);
				Texture = new Graphic(Width, Height, _rawImage.GetPixels(), false, false, false, filename + "_Image");
			}
			catch (Exception ex)
			{
				Log.Exception(ex);
			}
		}

		/// <summary>
		/// Returns a <see cref="string"/> that represents the current <see cref="ImageSource"/>.
		/// </summary>
		/// <returns>A <see cref="string"/> that represents the current <see cref="ImageSource"/>.</returns>
		public override string ToString()
		{
			return new System.IO.FileInfo(_path).Name;
		}
	}
}
