// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.Serialization;
using CryEngine.Resources;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// View for one image, aligned reltaive to its owning element.
	/// </summary>
	[DataContract]
	public class Image : UIComponent
	{
		/// <summary>
		/// Invoked if the underlying ImageSource was changed.
		/// </summary>
		public event Action OnSourceChanged;

		private Graphic _texture = null;
		[DataMember]
        private ImageSource _source = null;
		[DataMember]
        private SliceType _sliceType = SliceType.None;
		[DataMember]
        private Color _color = Color.White;

		/// <summary>
		/// Source of the image shown by this component.
		/// </summary>
		/// <value>The source.</value>
		public ImageSource Source
		{
			set
			{
				if (_source != value)
				{
					_source = value;
					if (OnSourceChanged != null)
						OnSourceChanged();
				}
			}
			get
			{
				return _source;
			}
		}

		/// <summary>
		/// Adapts image to keep its aspect ratio considering images- and owners size.
		/// </summary>
		/// <value><c>true</c> if keep ratio; otherwise, <c>false</c>.</value>
		public bool KeepRatio { get; set; }

		/// <summary>
		/// Color multiplier for the image.
		/// </summary>
		/// <value>The color.</value>
		public Color Color { get { return _color; } set { _color = value; if (Source == null) Source = ImageSource.Blank; } }

		/// <summary>
		/// SliceType to be used on image drawing.
		/// </summary>
		/// <value>The type of the slice.</value>
		public SliceType SliceType { get { return _sliceType; } set { _sliceType = value; } }

		/// <summary>
		/// Will disregard any clamping options derived by layout system.
		/// </summary>
		/// <value><c>true</c> if ignore clamping; otherwise, <c>false</c>.</value>
		public bool IgnoreClamping { get; set; }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			OnSourceChanged += () =>
			{
				if (Source != null)
					_texture = Source.Texture;
			};
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnRender()
		{
			if (_source != null && _texture != null)
			{
				if (!IgnoreClamping)
				{
					var rect = Owner.GetComponent<RectTransform>();
					if(rect != null)
					{
						_texture.ClampRect = rect.ClampRect;	
					}
				}
				else
				{
					_texture.ClampRect = new Rect();
				}
				if (Color.A > 0.05f)
				{
					var rect = Owner.GetComponent<RectTransform>();
					if(rect != null)
					{
						_texture.Angle = rect.Angle;
					}

					_texture.Color = _color;
					_texture.TargetCanvas = ParentCanvas;

					var r = GetAlignedRect();
					_texture.Draw(r.x, r.y, r.w, r.h, SliceType);
				}
			}
		}

		/// <summary>
		/// Returns the area which the image is supposed to be drawn in, considering this components layout properties.
		/// </summary>
		/// <returns>The aligned Rect.</returns>
		public override Rect GetAlignedRect()
		{
			if (_source == null)
				return new Rect();

			var rt = Owner.GetComponent<RectTransform>();
			if(rt == null)
			{
				return new Rect();
			}
			var tl = rt.TopLeft;
			if(KeepRatio)
			{
				var imageRatio = (float)_source.Width / _source.Height;
				var elementRatio = rt.Width / rt.Height;
				var ctr = rt.Center;

				if(imageRatio > elementRatio)
				{
					var imgHeight = rt.Width / imageRatio;
					return new Rect(tl.x, ctr.y - imgHeight / 2.0f, rt.Width, imgHeight);
				}

				var imgWidth = rt.Height * imageRatio;
				return new Rect(ctr.x - imgWidth / 2.0f, tl.y, imgWidth, rt.Height);
			}

			return new Rect(tl.x, tl.y, rt.Width, rt.Height);
		}
	}
}
