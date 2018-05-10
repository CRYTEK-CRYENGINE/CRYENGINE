// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.UI.Components;
using System.Runtime.Serialization;

namespace CryEngine.UI
{
	/// <summary>
	/// Represents a simple Panel element.
	/// </summary>
	[DataContract]
	public class Panel : UIElement
	{
		/// <summary>
		/// Optional Background image for the Panel.
		/// </summary>
		/// <value>The background.</value>
		public Image Background { get; private set; }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			Background = AddComponent<Image>();
		}
	}
}
