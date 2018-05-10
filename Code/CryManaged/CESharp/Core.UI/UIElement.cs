// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.IO;
using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// Determines a buffer which is applied to make a rect smaller.
	/// </summary>
	public class Padding
	{
		/// <summary>
		/// The left buffer.
		/// </summary>
		public float Left;

		/// <summary>
		/// The top buffer.
		/// </summary>
		public float Top;

		/// <summary>
		/// The right buffer.
		/// </summary>
		public float Right;

		/// <summary>
		/// The bottom buffer.
		/// </summary>
		public float Bottom;

		/// <summary>
		/// The upper left buffer.
		/// </summary>
		/// <value>The top left.</value>
		public Point TopLeft { get { return new Point(Left, Top); } set { Left = value.x; Top = value.y; } }

		/// <summary>
		/// Constructs Padding with one default value for all buffers.
		/// </summary>
		/// <param name="def">Default buffer value.</param>
		public Padding(float def = 0)
		{
			Top = Left = Right = Bottom = def;
		}

		/// <summary>
		/// Only initializes upper left buffer. lower right will be 0.
		/// </summary>
		/// <param name="left">Left buffer value.</param>
		/// <param name="top">Top buffer value.</param>
		public Padding(float left, float top)
		{
			Top = top; Left = left;
		}

		/// <summary>
		/// Initializes Padding with specific values for all buffers.
		/// </summary>
		/// <param name="left">Left buffer value.</param>
		/// <param name="top">Top buffer value.</param>
		/// <param name="right">Right buffer value.</param>
		/// <param name="bottom">Bottom buffer value.</param>
		public Padding(float left, float top, float right, float bottom)
		{
			Top = top; Left = left; Right = right; Bottom = bottom;
		}

		/// <summary>
		/// Returns a <see cref="string"/> that represents the current <see cref="Padding"/>.
		/// </summary>
		/// <returns>A <see cref="string"/> that represents the current <see cref="Padding"/>.</returns>
		public override string ToString()
		{
			return Left.ToString("0") + "," + Top.ToString("0") + "," + Right.ToString("0") + "," + Bottom.ToString("0");
		}
	}

	/// <summary>
	/// Alignment for Layout computation of UIElements. UIElements will be centered around an alignment target.
	/// </summary>
	public enum Alignment
	{
		/// <summary>
		/// Center the element.
		/// </summary>
		Center,
		/// <summary>
		/// Align the element to the top.
		/// </summary>
		Top,
		/// <summary>
		/// Align the element to the top-right.
		/// </summary>
		TopRight,
		/// <summary>
		/// Align the element to the right.
		/// </summary>
		Right,
		/// <summary>
		/// Align the element to the bottom-right.
		/// </summary>
		BottomRight,
		/// <summary>
		/// Align the element to the bottom.
		/// </summary>
		Bottom,
		/// <summary>
		/// Align the element to the bottom-left.
		/// </summary>
		BottomLeft,
		/// <summary>
		/// Align the element to the left.
		/// </summary>
		Left,
		/// <summary>
		/// Align the element to the top-left.
		/// </summary>
		TopLeft,
		/// <summary>
		/// Align the element to the top and stretch it horizontally.
		/// </summary>
		TopHStretch,
		//CenterHStretch,
		/// <summary>
		/// Align the element to the bottom and stretch it horizontally.
		/// </summary>
		BottomHStretch,
		//LeftVStretch,
		//CenterVStretch,
		/// <summary>
		/// Align the element to the right, and stretch it vertically.
		/// </summary>
		RightVStretch,
		/// <summary>
		/// Stretch the element in all directions.
		/// </summary>
		Stretch
	}

	/// <summary>
	/// Base class for all UI elements. Provides RectTransform for Layout computation.
	/// </summary>
	public class UIElement : SceneObject
	{
		/// <summary>
		/// Path where UI System textures are stored.
		/// </summary>
		/// <value>The data directory.</value>
		public static string DataDirectory { get { return Path.Combine(Engine.DataDirectory, "libs/ui"); } }

		/// <summary>
		/// Defines and computes layout in "d space, for this UIElement.
		/// </summary>
		/// <value>The rect transform.</value>
		public RectTransform RectTransform { get; private set; }

		/// <summary>
		/// Checks if cursor is inside a UIElement, considering its Bounds.
		/// </summary>
		public bool IsCursorInside { get { return RectTransform.Bounds.Contains(Mouse.CursorPosition); } }

		/// <summary>
		/// Creates this element along with a RectTransform.
		/// </summary>
		public UIElement()
		{
			RectTransform = AddComponent<RectTransform>();
		}
	}
}
