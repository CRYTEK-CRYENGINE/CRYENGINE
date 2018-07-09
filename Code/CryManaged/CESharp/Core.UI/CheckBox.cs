// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// Represents a simple Checkbox element.
	/// </summary>
	public class CheckBox : UIElement
	{
		/// <summary>
		/// The UIComponent controlling this element.
		/// </summary>
		public CheckBoxCtrl Ctrl;

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			RectTransform.Size = new Point(16, 16);
			RectTransform.ClampMode = ClampMode.Full;
			Ctrl = AddComponent<CheckBoxCtrl>();
			Ctrl.IsChecked = true;
		}
	}
}
