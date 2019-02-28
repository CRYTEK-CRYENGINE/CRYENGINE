// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Linq;
using System.Runtime.Serialization;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// Defines how the area, in which this elements components will react or be visible, is determined by layouting.
	/// For each element, the determined area is defined by property ClampRect. ClampRect is modified by ClampMode options.
	/// </summary>
	public enum ClampMode
	{
		/// <summary>
		/// Defines ClampRect as Bounds property padded by Spacing property, intersected by Parent ClampRect.
		/// </summary>
		Full,

		/// <summary>
		/// Defines ClampRect as Bounds property padded by Spacing property, ignoring Parent ClampRect.
		/// </summary>
		Self,

		/// <summary>
		/// Defines ClampRect by Parent ClampRect.
		/// </summary>
		Parent,

		/// <summary>
		/// No clamping applied.
		/// </summary>
		None
	}

	/// <summary>
	/// Defines and performs layout on UIElements.
	/// </summary>
	public class RectTransform : UIComponent
	{
		/// <summary>
		/// Called before layout is changing.
		/// </summary>
		public event Action LayoutChanging;

		/// <summary>
		/// Called after layout has been changed.
		/// </summary>
		public event Action LayoutChanged;


		Alignment _alignment = Alignment.TopLeft;
		float _width;
		float _height;
		float _angle;
		Padding _padding = new Padding();
		Rect _bounds;
		Rect _clampRect;

		Rect _spacing;
		Point _center;
		Point _pivot = new Point(0.5f, 0.5f);
		bool _needsRefresh = true;
		[DataMember]
		ClampMode _clampMode = ClampMode.Self;
		float _passedCenterX = float.MinValue;
		float _passedCenterY = float.MinValue;
		float _passedWidth = float.MinValue;
		float _passedHeight = float.MinValue;

		/// <summary>
		/// Owning element will be centered around the alignment target. Alignment target is computed relative to its parent's Bounds.
		/// </summary>
		/// <value>The alignment.</value>
		public Alignment Alignment { set { _alignment = value; _needsRefresh = true; } get { return _alignment; } }

		/// <summary>
		/// The owner elements width.
		/// </summary>
		/// <value>The width.</value>
		public float Width { set { _width = value; _needsRefresh = true; } get { return _width; } }

		/// <summary>
		/// The owner elements height.
		/// </summary>
		/// <value>The height.</value>
		public float Height { set { _height = value; _needsRefresh = true; } get { return _height; } }

		/// <summary>
		/// The owner elements rotation.
		/// </summary>
		/// <value>The angle.</value>
		public float Angle { set { _angle = value; } get { return _angle; } }

		/// <summary>
		/// Layouted raw Bounds will be padded by this.
		/// </summary>
		/// <value>The padding.</value>
		public Padding Padding { set { _padding = value; _needsRefresh = true; } get { return _padding; } }

		/// <summary>
		/// Sets the owner elements size.
		/// </summary>
		/// <value>The size.</value>
		public Point Size { set { _width = value.x; _height = value.y; _needsRefresh = true; } }

		/// <summary>
		/// Modifies the potentially visible area, defines by ClampMode.
		/// </summary>
		/// <value>The spacing.</value>
		public Rect Spacing { set { _spacing = value; _needsRefresh = true; } get { return _spacing; } }

		/// <summary>
		/// Defines how clamping should be computed in ClampRect.
		/// </summary>
		/// <value>The clamp mode.</value>
		public ClampMode ClampMode 
		{ 
			set 
			{ 
				_clampMode = value;
			} 
			get 
			{ 
				return _clampMode; 
			} 
		}

		/// <summary>
		/// Defines the center of the element, relative to its size.
		/// </summary>
		/// <value>The pivot.</value>
		public Point Pivot { get { return _pivot; } set { _pivot = value; _needsRefresh = true; } }

		/// <summary>
		/// Indicates whether this RectTransform needs a layout refresh.
		/// </summary>
		/// <value><c>true</c> if needs refresh; otherwise, <c>false</c>.</value>
		public bool NeedsRefresh { get { return _needsRefresh; } }

		/// <summary>
		/// Returns Parent elements RectTransform, if available.
		/// </summary>
		/// <value>The prt.</value>
		public RectTransform PRT 
		{ 
			get 
			{
				var parent = Transform.Parent;
				return parent == null ? null : parent.Owner.GetComponent<RectTransform>();
			}
		}

		/// <summary>
		/// Returns center of owning element.
		/// </summary>
		/// <value>The center.</value>
		public Point Center { get { PerformLayout(); return _center; } }

		/// <summary>
		/// Returns top of owning element.
		/// </summary>
		/// <value>The top.</value>
		public Point Top { get { PerformLayout(); return new Point(Center.x, Center.y - Height / 2.0f); } }

		/// <summary>
		/// Returns top right of owning element.
		/// </summary>
		/// <value>The top right.</value>
		public Point TopRight { get { PerformLayout(); return new Point(Center.x + Width / 2.0f, Center.y - Height / 2.0f); } }

		/// <summary>
		/// Returns right of owning element.
		/// </summary>
		/// <value>The right.</value>
		public Point Right { get { PerformLayout(); return new Point(Center.x + Width / 2.0f, Center.y); } }

		/// <summary>
		/// Returns bottom right of owning element.
		/// </summary>
		/// <value>The bottom right.</value>
		public Point BottomRight { get { PerformLayout(); return new Point(Center.x + Width / 2.0f, Center.y + Height / 2.0f); } }

		/// <summary>
		/// Returns bottom of owning element.
		/// </summary>
		/// <value>The bottom.</value>
		public Point Bottom { get { PerformLayout(); return new Point(Center.x, Center.y + Height / 2.0f); } }

		/// <summary>
		/// Returns bottom left of owning element.
		/// </summary>
		/// <value>The bottom left.</value>
		public Point BottomLeft { get { PerformLayout(); return new Point(Center.x - Width / 2.0f, Center.y + Height / 2.0f); } }

		/// <summary>
		/// Returns left of owning element.
		/// </summary>
		/// <value>The left.</value>
		public Point Left { get { PerformLayout(); return new Point(Center.x - Width / 2.0f, Center.y); } }

		/// <summary>
		/// Returns top left of owning element.
		/// </summary>
		/// <value>The top left.</value>
		public Point TopLeft { get { PerformLayout(); return new Point(Center.x - Width / 2.0f, Center.y - Height / 2.0f); } }

		/// <summary>
		/// Resulting area for owning element after applying the layout.
		/// </summary>
		/// <value>The bounds.</value>
		public Rect Bounds { get { PerformLayout(); return _bounds; } }

		/// <summary>
		/// Resulting area for clamping after applying the layout.
		/// </summary>
		/// <value>The clamp rect.</value>
		public Rect ClampRect { get { PerformLayout(); return _clampRect; } }

		void ComputeClampRect()
		{
			var prt = PRT;
			switch(ClampMode)
			{
			case ClampMode.Full:
				_clampRect = _bounds.Pad(Spacing);

				// Use intersection if parent and own rect exist
				if(prt != null && prt.ClampRect.Size > 0 && _clampRect.Size > 0)
				{
					_clampRect = prt._clampRect & _clampRect;
				}

				// Take over Parent CR if self null
				if(MathHelpers.Approximately(_clampRect.Size, 0) && prt != null)
				{
					_clampRect = prt._clampRect;
				}
				break;

			case ClampMode.Self:
				_clampRect = Bounds.Pad(Spacing);
				break;

			case ClampMode.Parent:
				_clampRect = prt != null ? prt._clampRect : new Rect();
				break;

			case ClampMode.None:
				_clampRect = new Rect();
				break;
			}
		}

		RectTransform GetOutdatedAncestor()
		{
			var parent = Transform.Parent;
			if(parent == null)
			{
				return null;
			}

			var prt = parent.Owner.GetComponent<RectTransform>();
			if(prt == null)
			{
				return null;
			}

			if(prt.NeedsRefresh)
			{
				return prt;
			}

			return prt.GetOutdatedAncestor();
		}

		bool DeltaChanged
		{
			get
			{
				if(Math.Abs(_passedCenterX - _center.x) > 1 || Math.Abs(_passedCenterY - _center.y) > 1 || Math.Abs(_passedWidth - _width) > 1 || Math.Abs(_passedHeight - _height) > 1)
				{
					_passedCenterX = _center.x;
					_passedCenterY = _center.y;
					_passedWidth = _width;
					_passedHeight = _height;
					return true;
				}
				return false;
			}
		}

		/// <summary>
		/// Performs layout of this element, if any layout property changed locally. Will perform layout of any direct Parent before, if required.
		/// If any valiable result has changed after layout, element's children will be layouted too.
		/// </summary>
		/// <param name="forceRefresh">If set to <c>true</c> local refresh is forced.</param>
		public void PerformLayout(bool forceRefresh = false)
		{
			if(!forceRefresh && !NeedsRefresh)
				return;

			if(!forceRefresh)
			{
				var art = GetOutdatedAncestor();
				if(art != null)
				{
					art.PerformLayout();
					if(!NeedsRefresh)
						return;
				}
			}

			if(LayoutChanging != null)
				LayoutChanging();

			var prt = PRT;
			if(prt == null)
			{
				_center = new Point(_width / 2, _height / 2);
			}
			else
			{
				var pivot = _padding.TopLeft + new Point((Pivot.x - 0.5f) * Width, (Pivot.y - 0.5f) * Height);
				switch(_alignment)
				{
				case Alignment.Center:
					_center = prt.Center + pivot; break;
				case Alignment.Top:
					_center = prt.Top + pivot; break;
				case Alignment.TopRight:
					_center = prt.TopRight + pivot; break;
				case Alignment.Right:
					_center = prt.Right + pivot; break;
				case Alignment.BottomRight:
					_center = prt.BottomRight + pivot; break;
				case Alignment.Bottom:
					_center = prt.Bottom + pivot; break;
				case Alignment.BottomLeft:
					_center = prt.BottomLeft + pivot; break;
				case Alignment.Left:
					_center = prt.Left + pivot; break;
				case Alignment.TopLeft:
					_center = prt.TopLeft + pivot; break;

				case Alignment.TopHStretch:
					_width = prt.Width - _padding.Right - _padding.Left;
					_center = prt.TopLeft + pivot + new Point(_width / 2, -_padding.Bottom); break;
				case Alignment.BottomHStretch:
					_width = prt.Width - _padding.Right - _padding.Left;
					_center = prt.BottomLeft + pivot + new Point(_width / 2, -_padding.Bottom); break;
				case Alignment.RightVStretch:
					_height = prt.Height - _padding.Top - _padding.Bottom;
					_center = prt.TopRight + pivot + new Point(-_padding.Right, _height / 2); break;

				case Alignment.Stretch:
					_width = prt.Width - _padding.Right - _padding.Left;
					_height = prt.Height - _padding.Bottom - _padding.Top;
					_center = prt.TopLeft + pivot + new Point(_width / 2, _height / 2); break;
				}
			}

			_needsRefresh = false;
			var tl = TopLeft;
			_bounds = new Rect(tl.x, tl.y, _width, _height);
			ComputeClampRect();

			if(DeltaChanged)
			{
				Transform.Children.Where(t => t.Owner is UIElement).ToList().ForEach(t => (t.Owner as UIElement).RectTransform.PerformLayout(true));
			}

			if(LayoutChanged != null)
				LayoutChanged();
		}
	}
}
