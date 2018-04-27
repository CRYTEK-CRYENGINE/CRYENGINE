// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.IO;
using CryEngine.Resources;
using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// Represents a single vertical or horizontal scroll bar.
	/// </summary>
	public class ScrollBar : Panel
	{
		/// <summary>
		/// Invoked if the scroll value changed. Provides scroll value between 0 and 1.
		/// </summary>
		public event Action<float> Scrolling;

		Panel _scrollArea;
		bool _isVertical;
		float _currentAlpha;
		float _scrollAreaPos = 0;

		/// <summary>
		/// Input control for the ScrollBar.
		/// </summary>
		/// <value>The SBC trl.</value>
		public ScrollBarCtrl SBCtrl { get; private set; }

		/// <summary>
		/// Modifies color multiplier of scroll area by given alpha.
		/// </summary>
		/// <value>The current alpha.</value>
		public float CurrentAlpha
		{
			set
			{
				_currentAlpha = value;
				Background.Color = Color.White.WithAlpha(_currentAlpha / 2);
				_scrollArea.Background.Color = Color.White.WithAlpha((0.5f + _currentAlpha) / 1.5f);
			}
		}

		/// <summary>
		/// Initialized the ScrollBar on either state.
		/// </summary>
		/// <value><c>true</c> if is vertical; otherwise, <c>false</c>.</value>
		public bool IsVertical
		{
			set
			{
				_isVertical = value;
				SBCtrl.IsVertical = IsVertical;
				if (value)
				{
					RectTransform.Alignment = Alignment.RightVStretch;
					RectTransform.Width = 10;
					Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "vsplit_bar.png"), false);
					Background.SliceType = SliceType.ThreeVertical;
					_scrollArea.Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "vsplit_bar_knob.png"), false);
					_scrollArea.Background.SliceType = SliceType.ThreeVertical;
					_scrollArea.RectTransform.Alignment = Alignment.Top;
					_scrollArea.RectTransform.Size = new Point(10, 30);
				}
				else
				{
					RectTransform.Alignment = Alignment.BottomHStretch;
					RectTransform.Height = 10;
					Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "hsplit_bar.png"), false);
					Background.SliceType = SliceType.ThreeHorizontal;
					_scrollArea.Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "hsplit_bar_knob.png"), false);

					_scrollArea.Background.SliceType = SliceType.ThreeHorizontal;
					_scrollArea.RectTransform.Alignment = Alignment.Left;
					_scrollArea.RectTransform.Size = new Point(30, 10);
				}
			}
			get
			{
				return _isVertical;
			}
		}

		/// <summary>
		/// Moves the ScrollBar relative to its current position. The absolute scroll value is clamped between 0 and 1.
		/// </summary>
		/// <param name="delta">The Delta value for the bar to be moved.</param>
		public void MoveScrollAreaPos(float delta)
		{
			_scrollAreaPos = Math.Min(0.999f, Math.Max(0, _scrollAreaPos + delta));
			var sbSize = IsVertical ? RectTransform.Height : RectTransform.Width;
			var knobSize = Math.Max(30, SBCtrl.VisibleArea * sbSize);
			if (IsVertical)
			{
				_scrollArea.RectTransform.Size = new Point(10, knobSize);
				_scrollArea.RectTransform.Padding.Top = knobSize / 2 + _scrollAreaPos * (sbSize - knobSize);
			}
			else
			{
				_scrollArea.RectTransform.Size = new Point(knobSize, 10);
				_scrollArea.RectTransform.Padding.Left = knobSize / 2 + _scrollAreaPos * (sbSize - knobSize);
			}
			if (Scrolling != null)
			{
				Scrolling(_scrollAreaPos);
			}
		}

		/// <summary>
		/// Sets the amount of currently visible edge on the ScrollBar's scroll axis, relative to the full edge length of the underlying scroll panel.
		/// </summary>
		/// <param name="area">Value between 0 and 1, indicating the relative visible edge.</param>
		public void SetVisibleArea(float area)
		{
			SBCtrl.VisibleArea = area;
			MoveScrollAreaPos(0);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnRender()
		{
			CurrentAlpha = _currentAlpha * 0.8f + SBCtrl.TargetAlpha * 0.2f;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			base.OnAwake();
			Background.Color = Color.White.WithAlpha(0.5f);
			_scrollArea = Instantiate<Panel>(this);
			_scrollArea.RectTransform.Alignment = Alignment.Center;

			SBCtrl = AddComponent<ScrollBarCtrl>();
			CurrentAlpha = SBCtrl.TargetAlpha;
			SBCtrl.Parent = this;
			IsVertical = true;

			CurrentAlpha = SBCtrl.TargetAlpha;
		}
	}

	/// <summary>
	/// Provides an area for content, which is movable by ScrollBar elements.
	/// </summary>
	public class ScrollPanel : UIElement
	{
		/// <summary>
		/// Invoked if the content area has been moved.
		/// </summary>
		public event Action<float, float> Scrolling;

		ScrollBar _vScrollBar;
		ScrollBar _hScrollBar;
		Point _sbOfs = new Point(0, 0);

		/// <summary>
		/// The Content area to be filled.
		/// </summary>
		/// <value>The panel.</value>
		public UIElement Panel { get; set; }

		/// <summary>
		/// Provides the coordinates of the current upper left location inside the visible content area.
		/// </summary>
		/// <value>The scroll offset.</value>
		public Point ScrollOffset { get { return _sbOfs; } }

		/// <summary>
		/// Determines whether the vertical scrollbar is active in a moment.
		/// </summary>
		/// <value><c>true</c> if VS croll bar active; otherwise, <c>false</c>.</value>
		public bool VScrollBarActive { get { return _vScrollBar.Active; } }

		/// <summary>
		/// Determines whether the horizontal scrollbar is active in a moment.
		/// </summary>
		/// <value><c>true</c> if HS croll bar active; otherwise, <c>false</c>.</value>
		public bool HScrollBarActive { get { return _hScrollBar.Active; } }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			RectTransform.ClampMode = ClampMode.Full;

			Panel = Instantiate<UIElement>(this);
			Panel.RectTransform.ClampMode = ClampMode.Full;

			_vScrollBar = Instantiate<ScrollBar>(this);
			_vScrollBar.Scrolling += (ofs) =>
			{
				_sbOfs.y = ofs * (RectTransform.Height - Panel.RectTransform.Height - (HScrollBarActive ? 8 : 0));
				UpdatePanelPadding();
				if (Scrolling != null)
					Scrolling(_sbOfs.x, _sbOfs.y);
			};
			_hScrollBar = Instantiate<ScrollBar>(this);
			_hScrollBar.IsVertical = false;
			_hScrollBar.Scrolling += (ofs) =>
			{
				_sbOfs.x = ofs * (RectTransform.Width - Panel.RectTransform.Width - (VScrollBarActive ? 8 : 0));
				UpdatePanelPadding();
				if(Scrolling != null)
				{
					Scrolling(_sbOfs.x, _sbOfs.y);
				}
			};

			RectTransform.Spacing = new Rect(1, 1, 1, 1);
			RectTransform.Alignment = Alignment.Stretch;
			RectTransform.LayoutChanging += () =>
			{
				Panel.RectTransform.Padding = new Padding();
			};
			RectTransform.LayoutChanged += AdaptScrollPanel;
			UpdatePanelPadding();
		}

		void UpdatePanelPadding()
		{
			var prt = Panel.RectTransform;
			var hActive = _hScrollBar.Active = prt.Width + 8 > RectTransform.Width;
			var vActive = _vScrollBar.Active = prt.Height + 8 > RectTransform.Height;
			var wDif = (prt.Width + 9) - RectTransform.Width + _sbOfs.x;
			var hDif = (prt.Height + 9) - RectTransform.Height + _sbOfs.y;

			prt.Padding = new Padding(prt.Width / 2 + _sbOfs.x, prt.Height / 2 + _sbOfs.y, 0, 0);
			prt.Spacing = new Rect(0, 0, vActive ? wDif : 0, hActive ? hDif : 0);
			prt.PerformLayout(true);
		}

		void AdaptScrollPanel()
		{
			var prt = Panel.RectTransform;
			var hActive = _hScrollBar.Active = prt.Width + 8 > RectTransform.Width;
			var vActive = _vScrollBar.Active = prt.Height + 8 > RectTransform.Height;

			_hScrollBar.RectTransform.Padding = new Padding(0, 0, vActive ? 8 : 0, 4);
			_vScrollBar.RectTransform.Padding = new Padding(0, 0, 4, hActive ? 8 : 0);
			if (hActive)
				_hScrollBar.SetVisibleArea(RectTransform.Width / (prt.Width + 8));
			if (vActive)
				_vScrollBar.SetVisibleArea(RectTransform.Height / (prt.Height + 8));

			UpdatePanelPadding();
		}

		/// <summary>
		/// Sets the intended virtual size for the content panel.
		/// </summary>
		/// <param name="w">The width.</param>
		/// <param name="h">The height.</param>
		public void SetPanelSize(float w, float h)
		{
			var prt = Panel.RectTransform;
			prt.Size = new Point(w, h);
			AdaptScrollPanel();
		}

		/// <summary>
		/// Removes all content from the content panel.
		/// </summary>
		public void Clear()
		{
			var children = new List<Transform>(Panel.Transform.Children);
			foreach (var t in children)
				t.Owner.Destroy();
			var components = new List<UIComponent>(Panel.Components);
			foreach (var c in components)
				c.Destroy();
			_sbOfs = new Point(0, 0);

			Panel.RectTransform.Size = new Point(0, 0);
			_hScrollBar.Active = _vScrollBar.Active = false;
		}
	}
}
