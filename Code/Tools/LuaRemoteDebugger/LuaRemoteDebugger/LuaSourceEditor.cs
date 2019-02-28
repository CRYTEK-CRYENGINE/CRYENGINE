// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Drawing.Drawing2D;
using System.Media;

namespace LuaRemoteDebugger
{
	public partial class LuaSourceEditor : UserControl
	{
		string currentOpenFile;
		Dictionary<int, bool> breakpoints = new Dictionary<int, bool>();
		int currentExecutionLine;
		Point lastHoverPos;
		Timer hoverTimer = new Timer();
		char[] tokenSeperators = new char[] { ' ', '\n', '\r', '\t', '(', ')', '[', ']', '{', '}', ',', '.', ';', ':', '=', '*', '+', '-', '/', '~', '"' };

		public string FileName { get { return currentOpenFile; } }

		public event EventHandler OnFileOpened;
		public event EventHandler<HoverVariableEventArgs> OnHoverOverVariable;

		public LuaSourceEditor()
		{
			InitializeComponent();

			// Set tabs to 2 spaces wide (which is 16 pixels as long as the font doesn't change)
			const int tabSpacing = 16;
			int[] array = new int[32];
			for (int i = 0; i < array.Length; ++i)
			{
				array[i] = (i + 1) * tabSpacing;
			}
			richTextBox.SelectionTabs = array;

			hoverTimer.Interval = SystemInformation.MouseHoverTime;
			hoverTimer.Tick += hoverTimer_Tick;
		}

		public void OnExecutionLocationChanged(int line)
		{
			currentExecutionLine = line;

			if (currentExecutionLine > 0)
			{
				// Code to make sure execution line is visible (somewhat hacky but it will do until a neater solution is found)
				int topIndex = richTextBox.GetCharIndexFromPosition(new Point(0, 0));
				int bottomIndex = richTextBox.GetCharIndexFromPosition(new Point(0, richTextBox.Height - 1));
				int charIndex = richTextBox.GetFirstCharIndexFromLine(line - 1);
				if (charIndex >= 0)
				{
					richTextBox.SelectionStart = charIndex;
					richTextBox.SelectionLength = 0;
					if (charIndex <= topIndex || charIndex >= bottomIndex)
					{
						// Execution line is out of sight, let's scroll to it
						richTextBox.ScrollToCaret();
					}
				}
			}

			Invalidate(false);
		}

		public void ClearExecutionLocation()
		{
			currentExecutionLine = 0;
			Invalidate(false);
		}

		public int GetSelectedLine()
		{
			int startPos = richTextBox.SelectionStart;
			int line = richTextBox.GetLineFromCharIndex(startPos) + 1;
			return line;
		}

		public void ClearBreakpoints()
		{
			breakpoints.Clear();
			Invalidate(false);
		}

		public void ClearBreakpoint(int line)
		{
			breakpoints.Remove(line);
			Invalidate(false);
		}

		public void SetBreakpoint(int line, bool enabled)
		{
			if (enabled)
			{
				breakpoints[line] = true;
			}
			else
			{
				breakpoints[line] = false;
			}
			Invalidate(false);
		}

		public void OpenFile(string fileName)
		{
			fileName = Path.GetFullPath(fileName);
			currentOpenFile = fileName;
			string text = File.ReadAllText(fileName);
			SetContents(text);

			breakpoints.Clear();
			currentExecutionLine = 0;

			if (OnFileOpened != null)
			{
				OnFileOpened(this, new EventArgs());
			}

			Invalidate(false);
		}

		public void SetContents(string contents)
		{
			richTextBox.Hide();	// Hide the control while we set the contents, otherwise updates are visible in real-time
			richTextBox.Clear();
			richTextBox.Text = contents;

			HighlightSyntax();
			richTextBox.Show();	// Then show it again at the end
		}

		private void HighlightSyntax()
		{
			// Highlight reserved words as defined in http://www.lua.org/manual/5.1/manual.html
			List<string> szKeywords = new List<string>()
			{
				"function",
				"do",
				"for",
				"end",
				"and",
				"or",
				"not",
				"while",
				"return",
				"if",
				"then",
				"else",
				"elseif",
				"local",
				"in",
				"nil",
				"repeat",
				"until",
				"break",
				"true",
				"false",
			};

			string scriptText = richTextBox.Text;
			int textLength = richTextBox.TextLength;
			int textPos = 0;
			while (textPos < textLength)
			{
				// Scan next token
				int textStartPos = textPos;
				while (textPos < textLength && !tokenSeperators.Contains(scriptText[textPos]))
				{
					textPos++;
				}
				if (textPos == textStartPos)
				{
					textPos++;
				}

				// Get token
				string token = scriptText.Substring(textStartPos, textPos - textStartPos);

				// Comment
				if (token == "-" && textPos < textLength && scriptText[textPos] == '-')
				{
					textPos++;
					// Block comment
					if (textPos + 1 < textLength && scriptText[textPos] == '[' && scriptText[textPos + 1] == '[')
					{
						textPos += 2;
						// Parse until closing ]] comment
						while (textPos < textLength && !(scriptText[textPos - 2] == ']' && scriptText[textPos - 1] == ']'))
						{
							textPos++;
						}
					}
					else
					{
						// Single line comment, parse until newline
						while (textPos < textLength && scriptText[textPos] != '\n')
						{
							textPos++;
						}
					}

					// Green
					richTextBox.SelectionStart = textStartPos;
					richTextBox.SelectionLength = textPos - textStartPos;
					richTextBox.SelectionColor = Color.Green;

					continue;
				}
				
				// String
				if (token == "\"")
				{
					// Parse until end of string (or newline, which next iteration will handle)
					while (textPos < textLength && scriptText[textPos] != '\n' && scriptText[textPos] != '"')
					{
						textPos++;
					}
					
					// If we finished on a quote, advance to include it
					if (textPos < textLength && scriptText[textPos] == '"')
					{
						textPos++;
					}

					// Gray
					richTextBox.SelectionStart = textStartPos;
					richTextBox.SelectionLength = textPos - textStartPos;
					richTextBox.SelectionColor = Color.Gray;

					continue;
				}

				// Number
				if (token[0] >= '0' && token[0] <= '9')
				{
					// Parse until the end of the number
					while (textPos < textLength && ((scriptText[textPos] >= '0' && scriptText[textPos] <= '9') || scriptText[textPos] == '.'))
					{
						textPos++;
					}

					// Orange
					richTextBox.SelectionStart = textStartPos;
					richTextBox.SelectionLength = textPos - textStartPos;
					richTextBox.SelectionColor = Color.DarkOrange;
					continue;
				}

				// Have we parsed a keyword ?
				if (szKeywords.Contains(token))
				{
					// Blue
					richTextBox.SelectionStart = textStartPos;
					richTextBox.SelectionLength = textPos - textStartPos;
					richTextBox.SelectionColor = Color.Blue;
					continue;
				}
			}

			richTextBox.SelectionStart = 0;
			richTextBox.SelectionLength = 0;
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			// Draw breakpoints
			foreach (var kvp in breakpoints)
			{
				int charIndex = richTextBox.GetFirstCharIndexFromLine(kvp.Key - 1);
				if (charIndex >= 0)
				{
					Point pos = richTextBox.GetPositionFromCharIndex(charIndex);
					pos.X = 0;
					pos.Y += 3;
					imageList.Draw(e.Graphics, pos, kvp.Value ? 0 : 1);
				}
			}

			// Draw execution pointer
			if (currentExecutionLine > 0)
			{
				int charIndex = richTextBox.GetFirstCharIndexFromLine(currentExecutionLine - 1);
				if (charIndex >= 0)
				{
					Point pos = richTextBox.GetPositionFromCharIndex(charIndex);
					pos.X = 0;
					pos.Y += 3;
					imageList.Draw(e.Graphics, pos, 2);
				}
			}

			// Draw line numbers
			int topIndex = richTextBox.GetCharIndexFromPosition(new Point(0, 0));
			int bottomIndex = richTextBox.GetCharIndexFromPosition(new Point(0, richTextBox.Height - 1));
			int topLine = richTextBox.GetLineFromCharIndex(topIndex);
			int bottomLine = richTextBox.GetLineFromCharIndex(bottomIndex);
			for (int i = topLine; i <= bottomLine; ++i)
			{
				int charIndex = richTextBox.GetFirstCharIndexFromLine(i);
				Point linePos = richTextBox.GetPositionFromCharIndex(charIndex);
				linePos.X = 16;
				linePos.Y += 3;
				e.Graphics.DrawString((i + 1).ToString(), richTextBox.Font, Brushes.Teal, linePos);
			}
		}

		private void richTextBox_VScroll(object sender, EventArgs e)
		{
			Invalidate(false);
			ClearToolTip();
		}

		private void ClearToolTip()
		{
			if (OnHoverOverVariable != null)
			{
				OnHoverOverVariable(this, new HoverVariableEventArgs(Point.Empty, string.Empty));
			}
		}

		public void ShowGoToLine()
		{
			using (GotoLineDialog dialog = new GotoLineDialog())
			{
				if (dialog.ShowDialog(this) == DialogResult.OK)
				{
					GoToLine(dialog.LineNumber);
				}
			}
		}

		public void GoToLine(int lineNumber)
		{
			if (lineNumber > 0)
			{
				int charIndex = richTextBox.GetFirstCharIndexFromLine(lineNumber - 1);
				if (charIndex >= 0)
				{
					richTextBox.SelectionStart = charIndex;
					richTextBox.SelectionLength = 0;
					richTextBox.ScrollToCaret();
				}
			}
		}

		public void FindText(string text, RichTextBoxFinds options)
		{
			int start = 0;
			int end = richTextBox.TextLength;
			if ((options & RichTextBoxFinds.Reverse) != 0)
			{
				end = richTextBox.SelectionStart + richTextBox.SelectionLength - 1;
			}
			else
			{
				start = richTextBox.SelectionStart + 1;
			}
			int found = richTextBox.Find(text, start, end, options);
			if (found == -1)
			{
				// Search the whole text file
				found = richTextBox.Find(text, options);
				if (found == -1)
				{
					SystemSounds.Asterisk.Play();
				}
			}
		}

		private void richTextBox_MouseMove(object sender, MouseEventArgs e)
		{
			Point delta = new Point(e.Location.X - lastHoverPos.X, e.Location.Y - lastHoverPos.Y);
			Size hoverSize = SystemInformation.MouseHoverSize;
			if (Math.Abs(delta.X) > hoverSize.Width || Math.Abs(delta.Y) > hoverSize.Height)
			{
				lastHoverPos = e.Location;
				hoverTimer.Start();
			}
		}

		void hoverTimer_Tick(object sender, EventArgs e)
		{
			hoverTimer.Stop();
			Point richTextPos = richTextBox.PointToClient(Cursor.Position);
			int charIndex = richTextBox.GetCharIndexFromPosition(richTextPos);

			string text = richTextBox.Text;

			int wordEnd = text.IndexOfAny(tokenSeperators, charIndex);
			int wordStart = charIndex + 1;
			do
			{
				wordStart = text.LastIndexOfAny(tokenSeperators, wordStart-1);
			} while (wordStart > 0 && text[wordStart] == '.');
			wordStart += 1;
			if (wordStart >= 0 && wordEnd >= 0 && wordEnd > wordStart)
			{
				string variableName = text.Substring(wordStart, wordEnd - wordStart);

				Point charPos = richTextBox.GetPositionFromCharIndex(charIndex);
				Point screenLocation = richTextBox.PointToScreen(charPos);

				screenLocation.Y += 16;

				if (OnHoverOverVariable != null)
				{
					OnHoverOverVariable(this, new HoverVariableEventArgs(screenLocation, variableName));
				}
			}
			else
			{
				ClearToolTip();
			}
		}

		private void richTextBox_MouseLeave(object sender, EventArgs e)
		{
			hoverTimer.Stop();
			ClearToolTip();
		}

		private void richTextBox_HScroll(object sender, EventArgs e)
		{
			ClearToolTip();
		}
	}

	public class HoverVariableEventArgs : EventArgs
	{
		Point hoverScreenLocation;
		string variableName;

		public HoverVariableEventArgs(Point hoverScreenLocation, string variableName) { this.hoverScreenLocation = hoverScreenLocation; this.variableName = variableName; }

		public string VariableName { get { return variableName; } }
		public Point HoverScreenLocation { get { return hoverScreenLocation; } }
	}

}
