// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Resources;

namespace CryEngine.UI
{
	/// <summary>
	/// A Graphic is a Texture that can be drawn by the Core.UI. It is heavily used in the UI for both images and text.
	/// </summary>
	public class Graphic : Texture
	{
		/// <summary>
		/// The Canvas that this Graphic is targeting.
		/// </summary>
		/// <value>The target canvas.</value>
		public Canvas TargetCanvas { get; set; }

		/// <summary>
		/// Color to multiply the texture with on draw.
		/// </summary>
		/// <value>The color.</value>
		public Color Color { get; set; }

		/// <summary>
		/// Indicates whether draw location is rounded up.
		/// </summary>
		/// <value><c>true</c> if round location; otherwise, <c>false</c>.</value>
		public bool RoundLocation { get; set; }

		/// <summary>
		/// Rotation angle of the texture.
		/// </summary>
		/// <value>The angle.</value>
		public float Angle { get; set; }

		/// <summary>
		/// Determines which area the texture can be drawn in. Texture will be clamped outside the edges of this area if not null.
		/// </summary>
		public Rect ClampRect { get; set; }

		/// <summary>
		/// Creates a graphic with image data and sets some default properties.
		/// </summary>
		/// <param name="width">Intended Texture Width.</param>
		/// <param name="height">Intended Texture Height.</param>
		/// <param name="data">Image source data.</param>
		/// <param name="roundLocation">If set to <c>true</c> texture start location is rounded up on draw.</param>
		/// <param name="isRendertarget">If set to <c>true</c> Texture can be rendered to.</param>
		/// <param name="clamp">If set to <c>true</c> the Texture will clamp at the edges instead of wrapping around.</param>
		/// <param name="name">The name the texture will receive internally. If the name is not unique a number will be appended to it.</param>
		public Graphic(int width, int height, byte[] data, bool roundLocation = false, bool isRendertarget = false, bool clamp = false, string name = "MF")
			: base(width, height, data, isRendertarget, clamp, name)
		{
			Color = Color.White;
			RoundLocation = roundLocation;
		}

		private void DrawSection(float x, float y, float w, float h, float u0, float v0, float u1, float v1)
		{
			float crx = x, cry = y, crw = w, crh = h;
			if(ClampRect.Size > 0)
			{
				crx = ClampRect.x;
				cry = ClampRect.y;
				crw = ClampRect.w;
				crh = ClampRect.h;
			}

			if(MathHelpers.Approximately(ClampRect.Size, 0) || (crx <= x && cry <= y && crx + crw >= x + w && cry + crh >= y + h))
			{
				if(RoundLocation) 
				{ 
					x = (int)x; 
					y = (int)y; 
				}

				TargetCanvas.PushTexturePart(this, x, y, w, h, u0, v0, u1, v1, Angle, Color);
			}
			else
			{
				if(crx >= x + w || crx + crw <= x || cry >= y + h || cry + crh <= y || crw <= 0 || crh <= 0)
					return;

				float cx = x, cy = y, cw = w, ch = h, cu0 = u0, cv0 = v0, cu1 = u1, cv1 = v1;
				if(crx > x)
				{
					cu0 = u0 + (u1 - u0) * (crx - x) / w;
					cx = crx;
					cw = cw - (crx - x);
				}
				if(crx + crw < x + w)
				{
					cu1 = u1 - (u1 - u0) * (x + w - crx - crw) / w;
					cw = crx + crw - cx;
				}
				if(cry > y)
				{
					cv0 = v0 + (v1 - v0) * (cry - y) / h;
					cy = cry;
					ch = ch - (cry - y);
				}
				if(cry + crh < y + h)
				{
					cv1 = v1 - (v1 - v0) * (y + h - cry - crh) / h;
					ch = cry + crh - cy;
				}
				if(RoundLocation) { cx = (int)cx; cy = (int)cy; }
				TargetCanvas.PushTexturePart(this, cx, cy, cw, ch, cu0, cv0, cu1, cv1, Angle, Color);
			}
		}

		/// <summary>
		/// Draws the current texture object, in a given rectangular area.
		/// </summary>
		/// <param name="x">The x coordinate.</param>
		/// <param name="y">The y coordinate.</param>
		/// <param name="w">The Width.</param>
		/// <param name="h">The Height.</param>
		/// <param name="sliceType">SliceType to be used.</param>
		internal void Draw(float x, float y, float w, float h, SliceType sliceType = SliceType.None)
		{
			float sx = (float)System.Math.Round(Width / 3.0f);
			float sy = (float)System.Math.Round(Height / 3.0f);

			switch(sliceType)
			{
			case SliceType.None:
				DrawSection(x, y, w, h, 0, 0, 1, 1);
				break;

			case SliceType.Nine:
				DrawSection(x,           y,           sx,          sy,          0,       0,       0.333f,  0.333f);
				DrawSection(x + sx,      y,           w - sx * 2,  sy,          0.333f,  0,       0.666f,  0.333f);
				DrawSection(x + w - sx,  y,           sx,          sy,          0.666f,  0,       1,       0.333f);

				DrawSection(x,           y + sy,      sx,          h - sy * 2,  0,       0.333f,  0.333f,  0.666f);
				DrawSection(x + sx,      y + sy,      w - sx * 2,  h - sy * 2,  0.333f,  0.333f,  0.666f,  0.666f);
				DrawSection(x + w - sx,  y + sy,      sx,          h - sy * 2,  0.666f,  0.333f,  1,       0.666f);

				DrawSection(x,           y + h - sy,  sx,          sy,          0,       0.666f,  0.333f,  1);
				DrawSection(x + sx,      y + h - sy,  w - sx * 2,  sy,          0.333f,  0.666f,  0.666f,  1);
				DrawSection(x + w - sx,  y + h - sy,  sx,          sy,          0.666f,  0.666f,  1,       1);
				break;

			case SliceType.ThreeHorizontal:
				DrawSection(x,           y,           sx,          h,           0,       0,       0.333f,  1);
				DrawSection(x + sx,      y,           w - sx * 2,  h,           0.333f,  0,       0.666f,  1);
				DrawSection(x + w - sx,  y,           sx,          h,           0.666f,  0,       1,       1);
				break;

			case SliceType.ThreeVertical:
				DrawSection(x,           y,           w,           sy,          0,       0,       1,       0.333f);
				DrawSection(x,           y + sy,      w,           h - sy * 2,  0,       0.333f,  1,       0.666f);
				DrawSection(x,           y + h - sy,  w,           sy,          0,       0.666f,  1,       1);
				break;
			}
		}
	}
}
