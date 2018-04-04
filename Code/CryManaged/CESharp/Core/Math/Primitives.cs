using System;

namespace CryEngine
{
	/// <summary>
	/// Rectangle representation.
	/// </summary>
	public struct Rect
    {
        public float x;
        public float y;
        public float w;
		public float h;

		/// <summary>
		/// The width of the rectangle.
		/// </summary>
		/// <value>The width.</value>
		public float Width
		{
			get
			{
				return w;
			}
			set
			{
				w = value;
			}
		}

		/// <summary>
		/// The height of the rectangle.
		/// </summary>
		/// <value>The height.</value>
		public float Height
		{
			get
			{
				return h;
			}
			set
			{
				h = value;
			}
		}

		/// <summary>
		/// The square size of the rectangle.
		/// </summary>
		/// <value>The size.</value>
		public float Size
		{
			get
			{
				return w * h;
			}
		}

		/// <summary>
		/// Creates a new rectangle 
		/// </summary>
		/// <param name="x">X.</param>
		/// <param name="y">Y.</param>
		/// <param name="width">W.</param>
		/// <param name="height">H.</param>
		public Rect(float x, float y, float width, float height)
        {
            this.x = x;
			this.y = y;
			w = width;
			h = height;
        }

        /// <summary>
        /// Checks if point is inside this Rect.
        /// </summary>
        /// <param name="v">The point to be checked.</param>
        public bool Contains(Point v)
        {
            return Contains(v.x, v.y);
        }

        /// <summary>
        /// Checks if coordinates are inside this Rect.
        /// </summary>
        /// <param name="_x">The x coordinate.</param>
        /// <param name="_y">The y coordinate.</param>
        public bool Contains(float _x, float _y)
        {
            return _x >= x && _x < x + w && _y >= y && _y < y + h;
        }

        /// <summary>
        /// Returns new rectangle, modified by the delta of 's'.
        /// </summary>
        /// <param name="s">The rect to be used for delta movement.</param>
        public Rect Pad(Rect s)
        {
            return new Rect(x + s.x, y + s.y, w - s.w - s.x, h - s.h - s.y);
        }

        /// <summary>
        /// Returns new rectangle, which includes 'r' and 's'.
        /// </summary>
        /// <param name="r">First Rect to be included.</param>
        /// <param name="s">Second Rect to be included.</param>
        public static Rect operator &(Rect r, Rect s)
        {
            var x1 = Math.Max(r.x, s.x);
            var y1 = Math.Max(r.y, s.y);
            var x2 = Math.Max(0, Math.Min(r.x + r.w, s.x + s.w));
            var y2 = Math.Max(0, Math.Min(r.y + r.h, s.y + s.h));
            return new Rect(x1, y1, x2 - x1, y2 - y1);
        }

		/// <summary>
		/// Returns a <see cref="string"/> that represents the current <see cref="Rect"/>.
		/// </summary>
		/// <returns>A <see cref="string"/> that represents the current <see cref="Rect"/>.</returns>
		public override string ToString()
        {
            return x.ToString("0") + "," + y.ToString("0") + "," + w.ToString("0") + "," + h.ToString("0");
        }
    }

    /// <summary>
    /// Point representation.
    /// </summary>
    public class Point
    {
        public float x, y;

        public Point()
        {
        }

        public Point(float _x, float _y)
        {
            x = _x;
            y = _y;
        }

        public static Point operator +(Point u, Point v)
        {
            return new Point(u.x + v.x, u.y + v.y);
        }

		/// <summary>
		/// Returns a <see cref="string"/> that represents the current <see cref="Point"/>.
		/// </summary>
		/// <returns>A <see cref="string"/> that represents the current <see cref="Point"/>.</returns>
		public override string ToString()
        {
            return x.ToString("0") + "," + y.ToString("0");
        }
    }
}
