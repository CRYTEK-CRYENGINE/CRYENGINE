// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using CryEngine.Resources;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// Renders arbitrary text to a Texture and displays it according to its layout properties, relative to owners layout.
	/// </summary>
	public class Text : UIComponent
	{
		static Dictionary<int, int> _heightToSize = new Dictionary<int, int>();

		/// <summary>
		/// Target Height of the font in pixels.
		/// </summary>
		public static byte DefaultHeight = 15;

		private string _content = "";
		private byte _height = DefaultHeight;
		private string _fontName = "Arial";
		private FontStyle _fontStyle = FontStyle.Regular;
		private Alignment _alignment = Alignment.Left;
		private Point _offset = new Point(0, 0);
		private bool _requiresUpdate = true;
		private Graphic _texture = null;
		private Color _color = Color.White;
		private readonly Point _alignedOffset = new Point();
		private readonly Point _alignedSize = new Point();
		private bool _dropsShadow = true;

		/// <summary>
		/// The text to be displayed.
		/// </summary>
		/// <value>The content.</value>
		public string Content
		{
			get
			{ 
				return _content; 
			}
			set
			{
				if(_content != value) 
				{
					_requiresUpdate = true;
				}
				_content = value;
			}
		}

		/// <summary>
		/// Aligned width for the text content.
		/// </summary>
		/// <value>The width.</value>
		public int Width 
		{ 
			get 
			{ 
				return _texture.Width; 
			} 
		}

		/// <summary>
		/// Aligned height for the text content.
		/// </summary>
		/// <value>The height.</value>
		public byte Height 
		{ 
			get 
			{ 
				return _height; 
			} 
			set 
			{ 
				if(_height != value) 
				{
					_requiresUpdate = true;
				}
				_height = value; 
			} 
		}

		/// <summary>
		/// Offset for the output of the text, relative to the layouted position.
		/// </summary>
		/// <value>The offset.</value>
		public Point Offset 
		{ 
			get 
			{ 
				return _offset; 
			} 
			set 
			{ 
				if(_offset != value)
				{
					_requiresUpdate = true;
				}
				_offset = value; 
			} 
		}

		/// <summary>
		/// Determines the font to be used for rendering.
		/// </summary>
		/// <value>The name of the font.</value>
		public string FontName 
		{ 
			get 
			{ 
				return _fontName; 
			} 
			set 
			{ 
				if(_fontName != value) 
				{
					_requiresUpdate = true;
				}
				_fontName = value; 
			} 
		}

		/// <summary>
		/// Determines the font style to be used for rendering.
		/// </summary>
		/// <value>The font style.</value>
		public FontStyle FontStyle 
		{ 
			get 
			{ 
				return _fontStyle; 
			}
			set 
			{ 
				if(_fontStyle != value)
				{
					_requiresUpdate = true;
				}
				_fontStyle = value; 
			} 
		}

		/// <summary>
		/// Determines the alignment of text, relative to owners layout.
		/// </summary>
		/// <value>The alignment.</value>
		public Alignment Alignment 
		{ 
			get 
			{ 
				return _alignment; 
			} 
			set 
			{ 
				if(_alignment != value) 
				{
					_requiresUpdate = true;
				}
				_alignment = value;
			}
		}

		/// <summary>
		/// Color for the rendered texture to be multiplied with.
		/// </summary>
		/// <value>The color.</value>
		public Color Color 
		{ 
			get 
			{ 
				return _color; 
			} 
			set 
			{
				_color = value; 
			}
		}

		/// <summary>
		/// Determines whether the rendered text is shadowed.
		/// </summary>
		/// <value><c>true</c> if drops shadow; otherwise, <c>false</c>.</value>
		public bool DropsShadow 
		{ 
			get 
			{ 
				return _dropsShadow; 
			} 
			set 
			{ 
				if(_dropsShadow != value) 
				{
					_requiresUpdate = true;
				}
				_dropsShadow = value; 
			} 
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			var rt = Owner.GetComponent<RectTransform>();
			if(rt == null)
			{
				rt = Owner.AddComponent<RectTransform>();
			}
			rt.LayoutChanged += UpdateLayout;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnRender()
		{
			UpdateContent();
			var rt = Owner.GetComponent<RectTransform>();
			if(rt != null && _texture != null)
			{
				_texture.ClampRect = rt.ClampRect;
				_texture.Color = Color;
				_texture.TargetCanvas = ParentCanvas;
				_texture.Draw(_alignedOffset.x, _alignedOffset.y, _alignedSize.x, _alignedSize.y);
			}
		}

		float VAlignMultiplier
		{
			get
			{
				if(Alignment == Alignment.Top || Alignment == Alignment.TopLeft || Alignment == Alignment.TopRight)
				{
					return 0;
				}

				if(Alignment == Alignment.Bottom || Alignment == Alignment.BottomLeft || Alignment == Alignment.BottomRight)
				{
					return 1;
				}

				return 0.5f;
			}
		}

		float HAlignMultiplier
		{
			get
			{
				if(Alignment == Alignment.Left || Alignment == Alignment.TopLeft || Alignment == Alignment.BottomLeft)
				{
					return 0;
				}

				if(Alignment == Alignment.Right || Alignment == Alignment.TopRight || Alignment == Alignment.BottomRight)
				{
					return 1;
				}

				return 0.5f;
			}
		}

		int GetSizeForHeight(byte height)
		{
			if(_heightToSize.ContainsKey(height))
			{
				return _heightToSize[height];
			}

			var g = Graphics.FromHwnd(IntPtr.Zero);
			var max = _heightToSize.Count > 0 ? _heightToSize.Values.Max() : 6;
			SizeF size;
			do
			{
				max++;
				var font = new Font(FontName, max, _fontStyle);

				size = g.MeasureString("Test", font);
				for(int i = (int)size.Height; i >= 0; i--)
				{
					if(_heightToSize.ContainsKey(i))
						break;
					_heightToSize[i] = max;
				}
			}
			while((int)size.Height < height);
			return _heightToSize[height];
		}

		/// <summary>
		/// Returns the relative position of a letter by its index.
		/// </summary>
		/// <returns>The relative position of the indexed letter.</returns>
		/// <param name="index">The index value.</param>
		public int GetOffsetAt(int index)
		{
			if(index < 1 || index > _content.Length)
				return 4;
			if(index == _content.Length)
				return (int)_alignedSize.x;

			var g = Graphics.FromHwnd(IntPtr.Zero);
			var font = new Font(FontName, GetSizeForHeight(Height), _fontStyle);
			var nullSize = g.MeasureString("XX", font);
			var fillSize = g.MeasureString("X" + _content.Substring(0, index) + "X", font);
			var width = fillSize.Width - nullSize.Width;
			return (int)width + 1;
		}

		/// <summary>
		/// Computes the relative layout alignment for this component.
		/// </summary>
		public void UpdateLayout()
		{
			var g = Graphics.FromHwnd(IntPtr.Zero);
			var font = new Font(FontName, GetSizeForHeight(Height), _fontStyle);
			var nullSize = g.MeasureString("XX", font);
			var fillSize = g.MeasureString("X" + _content + "X", font);
			var rt = Owner.GetComponent<RectTransform>();
			if(rt == null)
			{
				rt = Owner.AddComponent<RectTransform>();
			}

			var tl = rt.TopLeft;
			var width = fillSize.Width - nullSize.Width * 0.8f;
			var height = fillSize.Height;
			_alignedOffset.x = tl.x + HAlignMultiplier * (rt.Width - width) + Offset.x;
			_alignedOffset.y = tl.y + VAlignMultiplier * (rt.Height - height) + Offset.y;
			_alignedSize.x = (int)width + 1;
			_alignedSize.y = (int)height + 1;
		}

		/// <summary>
		/// Renders the text content to an ITexture, in case it was modified.
		/// </summary>
		public void UpdateContent()
		{
			if(!_requiresUpdate)
			{
				return;
			}

			UpdateLayout();
			var ax = Math.Max(1, (int)_alignedSize.x);
			var ay = Math.Max(1, (int)_alignedSize.y);

			var font = new Font(FontName, GetSizeForHeight(Height), _fontStyle);
			var bmp = new Bitmap(ax, ay, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
			using(var g = Graphics.FromImage(bmp))
			{
				g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAlias;
				if(DropsShadow)
				{
					g.DrawString(_content, font, Brushes.Black, 1, 1);
				}

				g.DrawString(_content, font, Brushes.White, 0, 0);
			}
			var data = bmp.GetPixels();

			if(_texture == null)
			{
				_texture = new Graphic(ax, ay, data, true, false, true, Owner.Name + "_Text");
			}
			else
			{
				_texture.UpdateData(ax, ay, data);
			}

			_requiresUpdate = false;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnDestroy()
		{
			if(_texture != null)
			{
				_texture.Destroy();
			}
			_texture = null;
		}

		/// <summary>
		/// Returns the layouted relative alignment.
		/// </summary>
		/// <returns>The aligned rect.</returns>
		public override Rect GetAlignedRect()
		{
			return new Rect(_alignedOffset.x, _alignedOffset.y, _alignedSize.x, _alignedSize.y);
		}
	}
}
