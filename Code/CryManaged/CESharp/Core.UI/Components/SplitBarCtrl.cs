// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// Input control for SplitBar.
	/// </summary>
	public class SplitBarCtrl : UIComponent
	{
		/// <summary>
		/// Raised if mouse is pulling the SplitBar. Coordinates define mouse delta since pull start.
		/// </summary>
		public event Action<int, int> Pulling;

		bool _drag = false;
		Point _startPos;

		///Defines the color alpha which the SplitBar should have.
		public float TargetAlpha { get; private set; }

		/// <summary>
		/// Initialized by SplitBar.
		/// </summary>
		public SplitBarCtrl()
		{
			TargetAlpha = 0.3f;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnMouseMove(int x, int y)
		{
			if (_drag && Pulling != null)
			{
				Pulling((int)(x - _startPos.x), (int)(y - _startPos.y));
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
			var ort = Owner.GetComponent<RectTransform>();
			return ort != null && ort.Bounds.Contains(x, y);
		}
	}
}

