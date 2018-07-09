// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;


// UI-Components are meant to be used for modifying, controling or viewing content relative to their owning UI elements.
namespace CryEngine.UI.Components
{
	/// <summary>
	/// Controls input on the Button UIElement.
	/// </summary>
	public class ButtonCtrl : UIComponent
	{
		/// <summary>
		/// Occurs then the mouse enters this ButtonCtrl.
		/// </summary>
		public event Action OnEnterMouse;

		/// <summary>
		/// Occurs when the mouse leaves this ButtonCtrl.
		/// </summary>
		public event Action OnLeaveMouse;

		/// <summary>
		/// Raised if Focus was entered by Canvas.
		/// </summary>
		public event Action OnFocusEnter;

		/// <summary>
		/// Raised if Focus was left by Canvas.
		/// </summary>
		public event Action OnFocusLost;

		/// <summary>
		/// Raised if button was pressed.
		/// </summary>
		public event Action OnPressed;

		/// <summary>
		/// Content of the button.
		/// </summary>
		/// <value>The text.</value>
		public Text Text { get; private set; }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			var button = Owner as Button;
			if(button != null)
			{
				Text = button.AddComponent<Text>();
				Text.Alignment = Alignment.Center;
			}
		}

		/// <summary>
		/// Called when the mouse enters the rectangle of this button.
		/// </summary>
		/// <param name="x">The x coordinate.</param>
		/// <param name="y">The y coordinate.</param>
		protected override void OnMouseEnter(int x, int y)
		{
			if (OnEnterMouse != null)
				OnEnterMouse();
		}

		/// <summary>
		/// Called when the mouse leaves the rectangle of this button.
		/// </summary>
		/// <param name="x">The x coordinate.</param>
		/// <param name="y">The y coordinate.</param>
		protected override void OnMouseLeave(int x, int y)
		{
			if (OnLeaveMouse != null)
				OnLeaveMouse();
		}

		/// <summary>
		/// Invoked by parent Canvas.
		/// </summary>
		protected override void OnEnterFocus()
		{
			if (OnFocusEnter != null)
				OnFocusEnter();
		}

		/// <summary>
		/// Invoked by parent Canvas.
		/// </summary>
		protected override void OnLeaveFocus()
		{
			if (OnFocusLost != null)
				OnFocusLost();
		}

		/// <summary>
		/// Invoked by parent Canvas.
		/// </summary>
		public override bool HitTest(int x, int y)
		{
			var rect = Owner.GetComponent<RectTransform>();
			return rect != null && rect.ClampRect.Contains(x, y);
		}

		/// <summary>
		/// Invoked by parent Canvas.
		/// </summary>
		protected override void OnLeftMouseDown(int x, int y)
		{
			(Owner as Button)?.SetDown();
		}

		/// <summary>
		/// Invoked by parent Canvas.
		/// </summary>
		protected override void OnLeftMouseUp(int x, int y, bool inside)
		{
			(Owner as Button)?.SetUp();
			if (inside && OnPressed != null)
			{
				OnPressed();
			}
		}

		/// <summary>
		/// Called when a key is pressed while this button is focused. If the button is the Space, Enter or XInput-A key,
		/// it will call OnPressed().
		/// </summary>
		/// <param name="e">E.</param>
		protected override void OnKey(InputEvent e)
		{
			if (e.KeyPressed(KeyId.Space) || e.KeyPressed(KeyId.Enter) || e.KeyPressed(KeyId.XI_A))
			{
				if (OnPressed != null)
					OnPressed();
			}
		}
	}
}

