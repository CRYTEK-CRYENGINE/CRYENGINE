// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// Input control for ScrollBar.
	/// </summary>
	public class ScrollBarCtrl : UIComponent
	{
		bool _drag = false;
		Point _startPos;

		/// <summary>
		/// Normed visible area, relative to the Content panel's size.
		/// </summary>
		public float VisibleArea;

		/// <summary>
		/// Determines whether or not ths ScrollBar is vertical.
		/// </summary>
		public bool IsVertical;

		/// <summary>
		/// Defines the alpha for this ScrollBar.
		/// </summary>
		/// <value>The target alpha.</value>
		public float TargetAlpha { get; private set; }

		/// <summary>
		/// Owning ScrollBar.
		/// </summary>
		public ScrollBar Parent;

		/// <summary>
		/// Initialized by ScrollBar element.
		/// </summary>
		public ScrollBarCtrl()
		{
			TargetAlpha = 0.3f;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnMouseMove(int x, int y)
		{
			if (_drag && (Math.Abs((y - _startPos.y)) > MathHelpers.Epsilon || Math.Abs(x - _startPos.x) > MathHelpers.Epsilon))
			{
				var prt = Parent.RectTransform;
				var knobSize = IsVertical ? Math.Max(30, VisibleArea * prt.Height) : Math.Max(30, VisibleArea * prt.Width);
				Parent.MoveScrollAreaPos(IsVertical ? ((y - _startPos.y) / (prt.Height - knobSize)) : ((x - _startPos.x) / (prt.Width - knobSize)));
				_startPos = new Point(x, y);
			}
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnMouseEnter(int x, int y)
		{
			TargetAlpha = 1;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnMouseLeave(int x, int y)
		{
			if (!_drag)
				TargetAlpha = 0.3f;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeftMouseDown(int x, int y)
		{
			_drag = true;
			_startPos = new Point(x, y);
			TargetAlpha = 1;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeftMouseUp(int x, int y, bool inside)
		{
			_drag = false;
			TargetAlpha = 0.3f;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		public override bool HitTest(int x, int y)
		{
			var ort = (Owner as UIElement).RectTransform;
			return ort.Bounds.Contains(x, y);
		}
	}
}

