// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.IO;

using CryEngine.Resources;
using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// Represents a simple CheckBox element.
	/// </summary>
	public class ComboBox : UIElement
	{
		Button _button;
		Panel _frame;

		/// <summary>
		/// Holds Background image for the drop down area.
		/// </summary>
		public Panel BgPanel;

		/// <summary>
		/// Contains the element's controller.
		/// </summary>
		public ComboBoxCtrl Ctrl;

        /// <summary>
        /// Called by framework. Do not call directly.
        /// </summary>
        protected override void OnAwake()
		{
			BgPanel = Instantiate<Panel>(this);
			BgPanel.Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "button.png"), false);
			BgPanel.Background.SliceType = SliceType.Nine;
			BgPanel.Background.Color = Color.Gray.WithAlpha(0.75f);
			BgPanel.RectTransform.Alignment = Alignment.Stretch;
			BgPanel.RectTransform.Spacing = new Rect { w = 20 };
			BgPanel.RectTransform.ClampMode = ClampMode.Full;
			Ctrl = AddComponent<ComboBoxCtrl>();

			_button = Instantiate<Button>(this);
			_button.RectTransform.Alignment = Alignment.Right;
			_button.RectTransform.Size = new Point(25, 22);
			_button.RectTransform.Padding = new Padding(-13, 0);
			_button.RectTransform.Spacing = new Rect { x = 6 };
			_button.RectTransform.ClampMode = ClampMode.Full;
			_button.Image = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "arrow_dn.png"));

			RectTransform.Alignment = Alignment.TopHStretch;
			RectTransform.Size = new Point(0, 22);
			RectTransform.ClampMode = ClampMode.Full;

			_frame = Instantiate<Panel>(this);
			_frame.Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "frame.png"), false);
			_frame.Background.SliceType = SliceType.Nine;
			_frame.Background.Color = Color.SkyBlue;
			_frame.RectTransform.Alignment = Alignment.Stretch;
			_frame.RectTransform.ClampMode = ClampMode.Full;
			_frame.Active = false;
			_frame.RectTransform.Padding = new Padding(2, 2, 3, 2);

			_button.Ctrl.Enabled = false;
			Ctrl.OnFocusEnter += EnterFocus;
			Ctrl.OnFocusLost += LeaveFocus;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		void EnterFocus()
		{
			_frame.Active = true;
			BgPanel.Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "button_inv.png"), false);
			_button.Ctrl.EnterFocusAction?.Invoke();
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		void LeaveFocus()
		{
			_frame.Active = false;
			BgPanel.Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "button.png"), false);
			_button.Ctrl.LeaveFocusAction?.Invoke();
		}
	}
}
