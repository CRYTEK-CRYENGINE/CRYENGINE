// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// Allows for text input by keyboard.
	/// </summary>
	public class TextInput : Panel
	{
		/// <summary>
		/// The current content.
		/// </summary>
		/// <value>The text.</value>
		public Text Text { get; private set; }

		/// <summary>
		/// The input controller for this element.
		/// </summary>
		/// <value>The ctrl.</value>
		public TextCtrl Ctrl { get; private set; }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			base.OnAwake();
			Background.Color = Color.Black.WithAlpha(0.75f);
			RectTransform.Size = new Point(0, 17);
			RectTransform.Alignment = Alignment.TopHStretch;
			RectTransform.Spacing = new Rect(0, 0, 1, 1);
			Text = AddComponent<Text>();
			Text.Height = 15;
			Text.Offset = new Point(2, 1);
			Ctrl = AddComponent<TextCtrl>();
		}
	}
}
