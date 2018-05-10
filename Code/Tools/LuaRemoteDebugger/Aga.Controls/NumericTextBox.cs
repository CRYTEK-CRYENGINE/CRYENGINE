// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.ComponentModel;
using System.Windows.Forms;
using System.Globalization;


namespace Aga.Controls
{
	/// <summary>
	/// Restricts the entry of characters to digits, the negative sign,
	/// the decimal point, and editing keystrokes (backspace).
	/// It does not handle the AltGr key so any keys that can be created in any
	/// combination with AltGr these are not filtered
	/// </summary>
	public class NumericTextBox : TextBox
	{
		private const int WM_PASTE = 0x302;
		private NumberStyles numberStyle = NumberStyles.AllowDecimalPoint | NumberStyles.AllowLeadingSign;

		/// <summary>
		/// Restricts the entry of characters to digits, the negative sign,
		/// the decimal point, and editing keystrokes (backspace).
		/// It does not handle the AltGr key
		/// </summary>
		/// <param name="e"></param>
		protected override void OnKeyPress(KeyPressEventArgs e)
		{
			base.OnKeyPress(e);

			e.Handled = invalidNumeric(e.KeyChar);
		}


		/// <summary>
		/// Main method for verifying allowed keypresses.
		/// This does not catch cut paste copy ... operations.
		/// </summary>
		/// <param name="key"></param>
		/// <returns></returns>
		private bool invalidNumeric(char key)
		{
			bool handled = false;

			NumberFormatInfo numberFormatInfo = CultureInfo.CurrentCulture.NumberFormat;
			string decimalSeparator = numberFormatInfo.NumberDecimalSeparator;
			string negativeSign = numberFormatInfo.NegativeSign;

			string keyString = key.ToString();

			if (Char.IsDigit(key))
			{
				// Digits are OK
			}
			else if (AllowDecimalSeparator && keyString.Equals(decimalSeparator))
			{
				if (Text.IndexOf(decimalSeparator) >= 0)
				{
					handled = true;
				}
			}
			else if (AllowNegativeSign && keyString.Equals(negativeSign))
			{
				if (Text.IndexOf(negativeSign) >= 0)
				{
					handled = true;
				}
			}
			else if (key == '\b')
			{
				// Backspace key is OK
			}
			else if ((ModifierKeys & (Keys.Control)) != 0)
			{
				// Let the edit control handle control and alt key combinations
			}
			else
			{
				// Swallow this invalid key and beep
				handled = true;
			}
			return handled;
		}


		/// <summary>
		/// Method invoked when Windows sends a message.
		/// </summary>
		/// <param name="m">Message from Windows.</param>
		/// <remarks>
		/// This is over-ridden so that the user can not use
		/// cut or paste operations to bypass the TextChanging event.
		/// This catches ContextMenu Paste, Shift+Insert, Ctrl+V,
		/// While it is generally frowned upon to override WndProc, no
		/// other simple mechanism was apparent to simultaneously and
		/// transparently intercept so many different operations.
		/// </remarks>
		protected override void WndProc(ref Message m)
		{
			// Switch to handle message...
			switch (m.Msg)
			{
				case WM_PASTE:
					{
						// Get clipboard object to paste
						IDataObject clipboardData = Clipboard.GetDataObject();

						// Get text from clipboard data
						string pasteText = (string)clipboardData.GetData(
								DataFormats.UnicodeText);

						// Get the number of characters to replace
						int selectionLength = SelectionLength;

						// If no replacement or insertion, we are done
						if (pasteText.Length == 0)
						{
							break;
						}
						else if (selectionLength != 0)
						{
							base.Text = base.Text.Remove(SelectionStart, selectionLength);
						}

						bool containsInvalidChars = false;
						foreach (char c in pasteText)
						{
							if (containsInvalidChars)
							{
								break;
							}
							else if (invalidNumeric(c))
							{
								containsInvalidChars = true;
							}
						}

						if (!containsInvalidChars)
						{
							base.Text = base.Text.Insert(SelectionStart, pasteText);
						}

						return;
					}

			}
			base.WndProc(ref m);
		}


		public int IntValue
		{
			get
			{
				int intValue;
				Int32.TryParse(this.Text, numberStyle, CultureInfo.CurrentCulture.NumberFormat, out intValue);
				return intValue;
			}
		}

		public decimal DecimalValue
		{
			get
			{
				decimal decimalValue;
				Decimal.TryParse(this.Text, numberStyle, CultureInfo.CurrentCulture.NumberFormat, out decimalValue);
				return decimalValue;
			}
		}


		private bool allowNegativeSign;
		[DefaultValue(true)]
		public bool AllowNegativeSign
		{
			get { return allowNegativeSign; }
			set { allowNegativeSign = value; }
		}

		private bool allowDecimalSeparator;
		[DefaultValue(true)]
		public bool AllowDecimalSeparator
		{
			get { return allowDecimalSeparator; }
			set { allowDecimalSeparator = value; }
		}

	}

}
