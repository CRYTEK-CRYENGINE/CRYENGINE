// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using CryEngine.Resources;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// Controls input on a Checkbox element.
	/// </summary>
	public class CheckBoxCtrl : UIComponent
	{
		/// <summary>
		/// Invoked if the checked state of checkbox changed.
		/// </summary>
		public event Action<bool> CheckedChanged;

		bool _isChecked;
		Panel _frame;

		Image _unchecked { get; set; }
		Image _checked { get; set; }

		/// <summary>
		/// Determines whether checkbox is currently checked.
		/// </summary>
		/// <value><c>true</c> if is checked; otherwise, <c>false</c>.</value>
		public bool IsChecked
		{
			set
			{
				if(_isChecked != value)
				{
					_isChecked = value;
					_unchecked.Active = !value;
					_checked.Active = value;
					if(CheckedChanged != null)
						CheckedChanged(value);
				}
			}
			get
			{
				return _isChecked;
			}
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnEnterFocus()
		{
			_frame.Active = true;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeaveFocus()
		{
			_frame.Active = false;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeftMouseUp(int x, int y, bool inside)
		{
			if(inside)
				IsChecked = !IsChecked;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			_unchecked = Owner.AddComponent<Image>();
			_unchecked.Source = ResourceManager.ImageFromFile(Path.Combine(UIElement.DataDirectory, "cb_unchecked.png"));
			_checked = Owner.AddComponent<Image>();
			_checked.Source = ResourceManager.ImageFromFile(Path.Combine(UIElement.DataDirectory, "cb_checked.png"));

			_frame = SceneObject.Instantiate<Panel>(Owner);
			_frame.Background.Source = ResourceManager.ImageFromFile(Path.Combine(UIElement.DataDirectory, "frame.png"), false);
			_frame.Background.SliceType = SliceType.Nine;
			_frame.Background.Color = Color.SkyBlue;
			_frame.RectTransform.Alignment = Alignment.Stretch;
			_frame.RectTransform.ClampMode = ClampMode.Full;
			_frame.Active = false;

			IsChecked = true;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnKey(InputEvent e)
		{
			if(e.KeyPressed(KeyId.Space))
				IsChecked = !IsChecked;
		}
	}
}

