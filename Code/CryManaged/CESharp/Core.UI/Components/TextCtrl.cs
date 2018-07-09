// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Resources;
using System;
using System.IO;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// Input control for a TextInput element.
	/// </summary>
	public class TextCtrl : UIComponent
	{
		/// <summary>
		/// Used by OnSubmit event. Indicates whether a suggested string should be accepted by the owning TextInput.
		/// </summary>
		public class SubmitEventArgs
		{
			/// <summary>
			/// The suggested string.
			/// </summary>
			/// <value>The value.</value>
			public string Value { get; private set; }

			/// <summary>
			/// The return value of acceptance determination. If true, the string is accepted.
			/// </summary>
			public bool Handled = false;

			/// <summary>
			/// Simple Constructor.
			/// </summary>
			/// <param name="value">The sugested string value.</param>
			public SubmitEventArgs(string value)
			{
				Value = value;
			}
		}

		/// <summary>
		/// Raised if return was pressed. Will lead to an evaluation of acceptance for the sugested string.
		/// </summary>
		public event Action<TextCtrl, SubmitEventArgs> OnSubmit;

		Text _text;
		Panel _cursor;
		Image _frame;
		DateTime _blinkTime = DateTime.MaxValue;
		string _contentBackup;
		bool _submitDesired = false;
		int _cursorIndex;

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			_cursor = SceneObject.Instantiate<Panel>(Owner);
			_cursor.Background.Source = ImageSource.Blank;
			_cursor.RectTransform.Size = new Point(2, 16);
			_cursor.RectTransform.Alignment = Alignment.Left;
			_cursor.RectTransform.ClampMode = ClampMode.Full;
			_cursor.Background.Source.Texture.RoundLocation = true;
			_cursor.Active = false;

			_frame = Owner.AddComponent<Image>();
			_frame.Source = ResourceManager.ImageFromFile(Path.Combine(UIElement.DataDirectory, "frame.png"), false);
			_frame.SliceType = SliceType.Nine;
			_frame.Color = Color.SkyBlue;
			_frame.Active = false;

			_text = Owner.GetComponent<Text>();
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnKey(InputEvent e)
		{
			if(!_submitDesired)
			{
				if(e.KeyPressed(KeyId.Enter))
				{
					SetCursor(true);
					_submitDesired = true;
				}
				return;
			}
			if(e.KeyPressed(KeyId.Escape))
			{
				_text.Content = _contentBackup;
				PositionContent(true);
				SetCursor(false);
				_submitDesired = false;
			}
			else if(e.KeyPressed(KeyId.Home))
			{
				_cursorIndex = 0;
				SetCursor(true);
				PositionContent();
			}
			else if(e.KeyPressed(KeyId.End))
			{
				_cursorIndex = _text.Content.Length;
				SetCursor(true);
				PositionContent();
			}
			else if(e.KeyPressed(KeyId.Delete))
			{
				if(_cursorIndex < _text.Content.Length)
					_text.Content = _text.Content.Substring(0, _cursorIndex) + _text.Content.Substring(_cursorIndex + 1);
				SetCursor(true);
			}
			else if(e.KeyPressed(KeyId.Enter) || e.KeyPressed(KeyId.XI_A))
			{
				if(TrySubmit())
				{
					SetCursor(false);
					_contentBackup = _text.Content;
				}
			}
			else if(e.KeyPressed(KeyId.Backspace))
			{
				if(_cursorIndex > 0)
				{
					_text.Content = _text.Content.Substring(0, _cursorIndex - 1) + _text.Content.Substring(_cursorIndex);
					SetCursor(true);
					_cursorIndex--;
				}
				PositionContent(true);
			}
			else if(e.KeyPressed(KeyId.Left))
			{
				_cursorIndex = Math.Max(0, _cursorIndex - 1);
				SetCursor(true);
				PositionContent();
			}
			else if(e.KeyPressed(KeyId.Right))
			{
				_cursorIndex = Math.Min(_text.Content.Length, _cursorIndex + 1);
				SetCursor(true);
				PositionContent();
			}
			else if(e.State == InputState.Pressed)
			{
				_text.Content = _text.Content.Insert(_cursorIndex, e.KeyName);
				_cursorIndex += e.KeyName.Length;
				SetCursor(true);
				PositionContent(true);
			}
		}

		bool TrySubmit()
		{
			var e = new SubmitEventArgs(_text.Content);
			if(OnSubmit != null)
				OnSubmit(this, e);
			if(e.Handled)
				_submitDesired = false;
			return e.Handled;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeftMouseDown(int x, int y)
		{
			var nearestIdx = -1;
			var nearestOffsetDelta = int.MaxValue;
			var xStart = (int)_text.GetAlignedRect().x;

			for(int i = 0; i <= _text.Content.Length; i++)
			{
				var ofsDelta = Math.Abs(xStart + _text.GetOffsetAt(i) - x);
				if(ofsDelta < nearestOffsetDelta)
				{
					nearestIdx = i;
					nearestOffsetDelta = ofsDelta;
				}
			}

			_cursorIndex = nearestIdx;
			SetCursor(true);
			PositionContent();
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnEnterFocus()
		{
			_contentBackup = _text.Content;
			_cursorIndex = _text.Content.Length;
			SetCursor(true);
			_frame.Active = true;
			PositionContent();
			_submitDesired = true;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeaveFocus()
		{
			if(_submitDesired)
				_text.Content = _contentBackup;
			SetCursor(false);
			_frame.Active = false;
			_text.Offset = new Point(2, 1);
		}

		/// <summary>
		/// Enables or disables the visibility of a cursor.
		/// </summary>
		public void SetCursor(bool active)
		{
			_cursor.Active = active;
			_blinkTime = active ? DateTime.Now.AddSeconds(0.5f) : DateTime.MaxValue;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnRender()
		{
			if(DateTime.Now > _blinkTime)
			{
				_cursor.Active = !_cursor.Active;
				_blinkTime = DateTime.Now.AddSeconds(0.5f);
			}
		}

		void PositionContent(bool updateTextLayout = false)
		{
			if(updateTextLayout)
				_text.UpdateLayout();
			var cursorOffset = _text.GetOffsetAt(_cursorIndex);
			var rect = Owner.GetComponent<RectTransform>();
			var fieldWidth = rect == null ? 0 : rect.Bounds.w - 2;
			if(cursorOffset < fieldWidth)
			{
				_cursor.RectTransform.Padding = new Padding(cursorOffset, 0);
				_cursor.RectTransform.PerformLayout();
				_text.Offset = new Point(2, 1);
			}
			else
			{
				_cursor.RectTransform.Padding = new Padding(fieldWidth, 0);
				_cursor.RectTransform.PerformLayout();
				_text.Offset = new Point(2 - cursorOffset + fieldWidth, 1);
			}
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		public override bool HitTest(int x, int y)
		{
			var rect = Owner.GetComponent<RectTransform>();
			return rect != null && rect.ClampRect.Contains(x, y);
		}
	}
}
