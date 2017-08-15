using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace CryEngine
{
    /// <summary>
    /// Rectangle representation.
    /// </summary>
    public class Rect
    {
        public float x;
        public float y;
        public float w;
        public float h;

        public Rect()
        {
        }

        public Rect(float _x, float _y, float _w, float _h)
        {
            x = _x;
            y = _y;
            w = _w;
            h = _h;
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
            if (s == null)
                return null;
            var x1 = Math.Max(r.x, s.x);
            var y1 = Math.Max(r.y, s.y);
            var x2 = Math.Max(0, Math.Min(r.x + r.w, s.x + s.w));
            var y2 = Math.Max(0, Math.Min(r.y + r.h, s.y + s.h));
            return new Rect(x1, y1, x2 - x1, y2 - y1);
        }

        /// <summary>
        /// Returns a <see cref="System.String"/> that represents the current <see cref="CryEngine.Rect"/>.
        /// </summary>
        /// <returns>A <see cref="System.String"/> that represents the current <see cref="CryEngine.Rect"/>.</returns>
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
        /// Returns a <see cref="System.String"/> that represents the current <see cref="CryEngine.Point"/>.
        /// </summary>
        /// <returns>A <see cref="System.String"/> that represents the current <see cref="CryEngine.Point"/>.</returns>
        public override string ToString()
        {
            return x.ToString("0") + "," + y.ToString("0");
        }
    }
}
