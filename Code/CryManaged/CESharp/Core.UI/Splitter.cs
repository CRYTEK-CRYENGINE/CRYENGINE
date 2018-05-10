// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using CryEngine.Resources;
using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// Used in Splitter to allow for modifying the size of two split areae.
	/// </summary>
	public class SplitBar : Panel
	{
		float _currentAlpha;
		Image _knobImg;

		/// <summary>
		/// The input controller for this element. 
		/// </summary>
		/// <value>The SBC trl.</value>
		public SplitBarCtrl SBCtrl { get; private set; }

		/// <summary>
		/// Sets the orientation of the SplitBar. Modified by owning Splitter.
		/// </summary>
		/// <value><c>true</c> if is vertical; otherwise, <c>false</c>.</value>
		public bool IsVertical
		{
			set
			{
				if (value)
				{
					Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "vsplit_bar.png"), false);
					_knobImg.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "vsplit_bar_knob.png"), false);
				}
				else
				{
					Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "hsplit_bar.png"), false);
					_knobImg.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "hsplit_bar_knob.png"), false);
				}
			}
		}

		/// <summary>
		/// Sets the transparency of the SplitBar.
		/// </summary>
		/// <value>The current alpha.</value>
		public float CurrentAlpha
		{
			set
			{
				_currentAlpha = value;
				Background.Color = Color.White.WithAlpha(_currentAlpha / 2);
				_knobImg.Color = Color.White.WithAlpha((0.5f + _currentAlpha) / 1.5f);
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			base.OnAwake();
			Background.SliceType = SliceType.ThreeVertical;
			_knobImg = AddComponent<Image>();
			_knobImg.KeepRatio = true;
			SBCtrl = AddComponent<SplitBarCtrl>();
			CurrentAlpha = SBCtrl.TargetAlpha;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnRender()
		{
			CurrentAlpha = _currentAlpha * 0.8f + SBCtrl.TargetAlpha * 0.2f;
		}
	}

	/// <summary>
	/// Divides assigned area into either two horizontally or two vertically split areae. The splitter in between can be moved. Areae may be collapsed.
	/// </summary>
	public class Splitter : UIElement
	{
		/// <summary>
		/// Invoked if one of the areae was collapsed or expanded. Parameter indicates the corresponding state change.
		/// </summary>
		public event Action<bool> CollapsedChanged;

		SplitBar _splittBar;
		Panel _paneA;
		Panel _paneB;

		bool _isVertical = true;
		int _splitterWidth = 10;
		float _splitterPos = 50.0f;
		float _splitterMin = 10.0f;
		float _splitterMax = 90.0f;
		float _modSplitterPos;

		/// <summary>
		/// The first panel for content.
		/// </summary>
		/// <value>The pane a.</value>
		public Panel PaneA { get { return _paneA; } }

		/// <summary>
		/// The second panel for content.
		/// </summary>
		/// <value>The pane b.</value>
		public Panel PaneB { get { return _paneB; } }

		/// <summary>
		/// Determines the orientation of the Splitter.
		/// </summary>
		/// <value><c>true</c> if is vertical; otherwise, <c>false</c>.</value>
		public bool IsVertical
		{
			set
			{
				_isVertical = value;
				_splittBar.IsVertical = value;
			}
			get
			{
				return _isVertical;
			}
		}

		//TODO Remove Set-only properties.
		/// <summary>
		/// Sets the min. size in % of the total Splitter size, which the first plane can be reduced to.
		/// </summary>
		/// <value>The splitter span minimum.</value>
		public float SplitterSpanMin { set { _splitterMin = value; SplitterPos = _splitterPos; } }

		//TODO Remove Set-only properties.
		/// <summary>
		/// Sets the max. size in % of the total Splitter size, which the first plane can be expanded to.
		/// </summary>
		/// <value>The splitter span max.</value>
		public float SplitterSpanMax { set { _splitterMax = value; SplitterPos = _splitterPos; } }

		/// <summary>
		/// Determines whether content panels can be collapsed.
		/// </summary>
		/// <value><c>true</c> if can collapse; otherwise, <c>false</c>.</value>
		public bool CanCollapse { get; set; }

		/// <summary>
		/// Indicates the position of the SplitBar in %, relative to the total SplitBar size.
		/// </summary>
		/// <value>The splitter position.</value>
		public float SplitterPos
		{
			set
			{
				_splitterPos = value;
				LayoutChanged();
			}
			get
			{
				return _splitterPos;
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			_paneA = Instantiate<Panel>(this);
			_paneA.Name = "PaneA";
			_paneA.GetComponent<Image>().Color = Color.Transparent;
			_paneA.RectTransform.Alignment = Alignment.TopLeft;

			_paneB = Instantiate<Panel>(this);
			_paneB.Name = "PaneB";
			_paneB.GetComponent<Image>().Color = Color.Transparent;
			_paneB.RectTransform.Alignment = Alignment.TopLeft;

			_splittBar = Instantiate<SplitBar>(this);
			_splittBar.RectTransform.Alignment = Alignment.TopLeft;
			_splittBar.RectTransform.Size = new Point(_splitterWidth, _splitterWidth);
			_splittBar.IsVertical = IsVertical;
			_splittBar.SBCtrl.Pulling += (x, y) =>
			{
				if (_isVertical)
					SplitterPos = (RectTransform.Width * _splitterPos / 100.0f + x) * 100.0f / RectTransform.Width;
				else
					SplitterPos = (RectTransform.Height * _splitterPos / 100.0f + y) * 100.0f / RectTransform.Height;
			};

			RectTransform.LayoutChanged += LayoutChanged;
		}

		void LayoutChanged()
		{
			var w = RectTransform.Width;
			var h = RectTransform.Height;
			var prevModSplitterPos = _modSplitterPos;
			_modSplitterPos = Math.Min(_splitterMax, Math.Max(_splitterMin, _splitterPos));
			var splitterMaxCollapseThreshold = 100 - (100 - _splitterMax) / 2;
			if(CanCollapse)
			{
				if(_splitterPos < _splitterMin / 2)
				{
					_modSplitterPos = 0;
				}

				if(_splitterPos > splitterMaxCollapseThreshold)
				{
					_modSplitterPos = 100;
				}
			}
			_paneA.Active = _modSplitterPos >= _splitterMin;
			_paneB.Active = _modSplitterPos <= _splitterMax;

			bool modSplitterCollapsed = _modSplitterPos < _splitterMin / 2 || _modSplitterPos > splitterMaxCollapseThreshold;
			bool prevModSplitterCollapsed = prevModSplitterPos < _splitterMin / 2 || prevModSplitterPos > splitterMaxCollapseThreshold;
			if(prevModSplitterCollapsed != modSplitterCollapsed)
			{
				CollapsedChanged?.Invoke(modSplitterCollapsed);
			}

			if (_isVertical)
			{
				var splitterOfs = _splitterWidth / 2 + _modSplitterPos * (w - _splitterWidth) / 100.0f;
				_paneA.RectTransform.Size = new Point(splitterOfs - _splitterWidth / 2, h);
				_paneA.RectTransform.Padding.TopLeft = new Point(splitterOfs / 2 - _splitterWidth / 4, h / 2);

				_paneB.RectTransform.Size = new Point(w - splitterOfs - _splitterWidth / 2, h);
				_paneB.RectTransform.Padding.TopLeft = new Point(splitterOfs + _splitterWidth / 2 + (w - splitterOfs - _splitterWidth / 2) / 2, h / 2);

				_splittBar.RectTransform.Size = new Point(_splitterWidth, h - 10);
				_splittBar.RectTransform.Padding.Top = h / 2;
				_splittBar.RectTransform.Padding.Left = splitterOfs;
			}
			else
			{
				var splitterOfs = _splitterWidth / 2 + _modSplitterPos * (h - _splitterWidth) / 100.0f;
				_paneA.RectTransform.Size = new Point(w, splitterOfs - _splitterWidth / 2);
				_paneA.RectTransform.Padding.TopLeft = new Point(w / 2, splitterOfs / 2 - _splitterWidth / 4);

				_paneB.RectTransform.Size = new Point(w, h - splitterOfs - _splitterWidth / 2);
				_paneB.RectTransform.Padding.TopLeft = new Point(w / 2, splitterOfs + _splitterWidth / 2 + (h - splitterOfs - _splitterWidth / 2) / 2);

				_splittBar.RectTransform.Size = new Point(w - 10, _splitterWidth);
				_splittBar.RectTransform.Padding.Left = w / 2;
				_splittBar.RectTransform.Padding.Top = splitterOfs;
			}
		}
	}
}
