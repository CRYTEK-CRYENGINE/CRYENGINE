// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.IO;
using CryEngine.Resources;
using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// Represents a window with Header and Caption.
	/// </summary>
	public class Window : Panel
	{
		Text _caption;

		/// <summary>
		/// The header text.
		/// </summary>
		/// <value>The caption.</value>
		public string Caption { set { _caption.Content = value; } }

		/// <summary>
		/// The height of the caption of this window.
		/// </summary>
		/// <value>The height of the caption.</value>
		public byte CaptionHeight { set { _caption.Height = value; } }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			base.OnAwake();
			Background.Source = ResourceManager.ImageFromFile(Path.Combine(DataDirectory, "dialog.png"));
			Background.SliceType = SliceType.Nine;
			Background.IgnoreClamping = true;
			RectTransform.Spacing = new Rect(13, 13, 14, 14);
			RectTransform.ClampMode = ClampMode.Self;

			_caption = AddComponent<Text>();
			_caption.Height = 20;
			_caption.Alignment = Alignment.Top;
			_caption.Offset = new Point(0, 11);
		}
	}
}
