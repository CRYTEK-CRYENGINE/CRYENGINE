// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using CsGL.OpenGL;

namespace Statoscope
{
	public class OpenGLGraphingControl : AAOpenGLControl
	{
		public FrameRecord HighlightedFrameRecord
		{
			get { return m_highlightedFR; }
		}

		LogControl m_logControl;
		List<LogView> m_logViews;

		public float m_centreX, m_centreY;
		public float m_sizeX, m_sizeY;

		MouseButtons m_mouseDownButton;
		float m_lastButtonPressedX;
		float m_lastButtonPressedY;

		bool m_zoomBoxing;
		float m_zoomBoxStartPosX, m_zoomBoxStartPosY;
		float m_zoomBoxEndPosX, m_zoomBoxEndPosY;

		private Point m_scaleDragStart;
		private Point m_scaleDragOrigin;
		private bool m_isScaling;

		private bool? m_scaleAboutCentre;

		public LogView m_trackingLogView = null;	// currently tracked log view
		public bool m_bSmartTracking = false;	// track the end frame if it's on screen
		bool m_bTrackingEndFrame = false;			// the view x centre follows the end frame of m_trackingLogView
		bool m_bTrackingCatchingUp = false;		// allows the end frame to reach m_fTrackingCatchupEndFrac along the screen before tracking
		const float m_fTrackingCatchupEndFrac = 0.9f;
		float m_centreTrackOffsetX = 0.0f;

		Label m_infoLabel;
		string m_currentLabel;

		float m_playheadX;
		FrameRecord m_highlightedFR;
		ViewFrameRecord m_highlightedVFR;
		ProfilerRDI m_highlightedPrdi;

		public bool m_againstFrameTime;

		public bool ScaleAboutCentre
		{
			get { return m_scaleAboutCentre.HasValue ? m_scaleAboutCentre.Value : Properties.Settings.Default.GraphControl_ScaleAboutCentre; }
			set { m_scaleAboutCentre = value; }
		}

		GLFont m_glFont;
		GLFont GLFont
		{
			get
			{
				if (m_glFont == null)
				{
					Font font = new Font(FontFamily.GenericSansSerif, 13.0f, GraphicsUnit.Pixel);
					m_glFont = new GLFont(font);
				}

				return m_glFont;
			}
		}

		public OpenGLGraphingControl(LogControl logControl)
		{
			m_logControl = logControl;
			m_logViews = logControl.m_logViews;

			m_centreX = 50.0f;
			m_centreY = 50.0f;
			m_sizeX = 100.0f;
			m_sizeY = 100.0f;

			m_mouseDownButton = MouseButtons.None;
			m_zoomBoxing = false;

			m_playheadX = -1.0f;

			this.SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.Opaque, true);

			m_infoLabel = new Label();
			m_infoLabel.AutoSize = true;
			m_infoLabel.Hide();
			m_infoLabel.MouseMove += new MouseEventHandler(infoLabel_MouseMove);
			m_infoLabel.Location = new Point(100, 100);
			m_infoLabel.BorderStyle = BorderStyle.FixedSingle;
			Controls.Add(m_infoLabel);
			m_infoLabel.BringToFront();

			m_highlightedFR = null;
			m_highlightedVFR = null;
			m_highlightedPrdi = null;

			m_againstFrameTime = false;

			DragEnter += new DragEventHandler(COpenGLGraphingControl_DragEnter);
			DragDrop += new DragEventHandler(COpenGLGraphingControl_DragDrop);
			AllowDrop = true;

			FitViewToFrameRecords();
		}

		private void COpenGLGraphingControl_DragEnter(object sender, DragEventArgs e)
		{
			m_logControl.LogControl_DragEnter(sender, e);
		}

		private void COpenGLGraphingControl_DragDrop(object sender, DragEventArgs e)
		{
			m_logControl.LogControl_DragDrop(sender, e);
		}

		public void SetTracking(LogView logView, bool bSmartTracking)
		{
			m_trackingLogView = logView;
			m_bSmartTracking = bSmartTracking;
			UpdateCentreTrackOffsetX();
		}

		void SetInfoLabelLocationFromMousePos(int mouseX, int mouseY)
		{
			int x = mouseX + 5;
			int y = mouseY + 20;

			if (x + m_infoLabel.Size.Width > this.Width)
			{
				x = Math.Max(mouseX - 5 - m_infoLabel.Size.Width, 0);
			}

			if (y + m_infoLabel.Size.Height > this.Height)
			{
				y = Math.Max(mouseY - 5 - m_infoLabel.Size.Height, 0);
			}

			m_infoLabel.Location = new Point(x, y);
		}

		void infoLabel_MouseMove(object sender, MouseEventArgs e)
		{
			SetInfoLabelLocationFromMousePos(e.X, e.Y);
		}

		public void CopyInfoLabelTextToClipboard()
		{
			Clipboard.SetText(m_infoLabel.Text);
		}

		// distance squared (in pixels) from p to the infinite line through a and b
		float DistanceSqToLine(float px, float py, float ax, float ay, float bx, float by)
		{
			float pax = px - ax;
			float pay = py - ay;
			float bax = bx - ax;
			float bay = by - ay;
			float baDotba = (bax * bax) + (bay * bay);
			float paDotba = (pax * bax) + (pay * bay);
			float lambda = paDotba / baDotba;
			lambda = Math.Max(0.0f, Math.Min(lambda, 1.0f));
			// q is the closest point on the line
			float qx = ax + lambda * bax;
			float qy = ay + lambda * bay;
			float pqx = px - qx;
			float pqy = py - qy;
			float pqxPixels = UnitsToPixelsX(pqx);
			float pqyPixels = UnitsToPixelsY(pqy);
			return (pqxPixels * pqxPixels) + (pqyPixels * pqyPixels);
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			float minPlayheadDist = float.MaxValue;
			string indentStr = m_logViews.Count > 1 ? " " : "";
			string newLabelText = "";
			Image newScreenshotImage = null;
			float globalX = WindowXToUnitsX(e.X);
			float globalY = WindowYToUnitsY(e.Y);

			m_playheadX = -1.0f;

			foreach (LogView logView in m_logViews)
			{
				if (!logView.m_displayedOnGraph || (logView.m_maxValidIdx == 0))
				{
					continue;
				}

				LogData logData = logView.m_logData;

				lock (logData)
				{
					if (e.Button == MouseButtons.None)
					{
						List<FrameRecord> frameRecords = logData.FrameRecords;
						int playheadFRIdx = -1;

						m_highlightedFR = null;
						m_highlightedVFR = null;
						m_highlightedPrdi = null;

						if (m_againstFrameTime)
						{
							float startMidTime = frameRecords.First().FrameMidTimeInS;

							if ((frameRecords.First().FrameMidTimeInS - startMidTime <= globalX) && (globalX < frameRecords.Last().FrameMidTimeInS - startMidTime))
							{
								int closestIdx = logData.FindClosestFrameIdxFromTime(globalX);
								closestIdx = Math.Min(closestIdx, logView.m_maxValidIdx);
								float playheadDist = Math.Abs(frameRecords[closestIdx].FrameMidTimeInS - startMidTime - globalX);

								if (playheadDist < minPlayheadDist)
								{
									minPlayheadDist = playheadDist;
									m_playheadX = frameRecords[closestIdx].GetDisplayXValue(logData, m_againstFrameTime);
								}

								playheadFRIdx = closestIdx;
							}
						}
						else
						{
							if ((0 <= globalX) && ((int)globalX <= logView.m_maxValidIdx))
							{
								playheadFRIdx = (int)globalX;
								m_playheadX = frameRecords[playheadFRIdx].GetDisplayXValue(logData, m_againstFrameTime);
							}
						}

						if (playheadFRIdx != -1)
						{
							int longestFrameStartIndex = 0;
							int longestFrameEndIndex = -1;

							float lastBarEndTime;
							float width = GetDisplayedElementBarWidth();
							int drawEvery = Math.Max(1, (int)Math.Ceiling(width));

							float startValue = LeftUnits;
							float endValue = (float)Math.Ceiling(RightUnits);

							int startIndex;

							if (m_againstFrameTime)
							{
								FrameRecord firstFullFR = frameRecords.Find(fr => fr.FrameTimeInS > startValue);
								startIndex = firstFullFR != null ? logData.GetDisplayIndex(firstFullFR.Index) - 1 : 0;
							}
							else
							{
								width = drawEvery;
								startIndex = ((int)startValue) / drawEvery * drawEvery;
							}

							startIndex = Math.Max(0, Math.Min(startIndex, logView.m_maxValidIdx));
							lastBarEndTime = frameRecords[startIndex].FrameTimeInS;

							for (int i = 0; i <= logView.m_maxValidIdx; i++)
							{
								FrameRecord fr = frameRecords[i];

								float x;

								if (m_againstFrameTime)
								{
									float nextBarEndTime = fr.FrameMidTimeInS + (fr.FrameMidTimeInS - fr.FrameTimeInS);

									if (nextBarEndTime < lastBarEndTime + GetDisplayedElementBarWidth())
									{
										continue;
									}

									x = lastBarEndTime;
									width = nextBarEndTime - lastBarEndTime;
									lastBarEndTime = nextBarEndTime;
								}
								else
								{
									if (((i + 1) % drawEvery) != 0)
									{
										continue;
									}

									x = logData.GetDisplayIndex(frameRecords[i + 1 - drawEvery].Index);
								}

								longestFrameStartIndex = longestFrameEndIndex + 1;
								longestFrameEndIndex = i;

								if (x + width > globalX)
								{
									break;
								}
							}

							float longestFrameLength = -1.0f;
							string displayedTotalPath = m_logControl.m_prdiTree.LeafPath;

							for (int i = longestFrameStartIndex; i <= longestFrameEndIndex; i++)
							{
								float frameLength = logView.m_viewFRs[i].Values[displayedTotalPath];

								if (frameLength > longestFrameLength)
								{
									m_highlightedFR = frameRecords[i];
									m_highlightedVFR = logView.m_viewFRs[i];
									longestFrameLength = frameLength;
								}
							}

							MemoryStream screenShotData = logData.FindClosestScreenshotFrameFromFrameIdx(playheadFRIdx).ScreenshotImage;

							if (screenShotData != null)
							{
								newScreenshotImage = Image.FromStream(screenShotData);  // the jpeg decompress happens here
							}

							FrameRecord playheadFR = frameRecords[playheadFRIdx];

							if (m_logViews.Count > 1)
							{
								newLabelText += string.Format("{0}  ", logView.m_name);

								if (m_againstFrameTime)
								{
									newLabelText += string.Format("Frame: {0:n0}\n", logData.GetDisplayIndex(playheadFR.Index));
								}
								else
								{
									newLabelText += string.Format("Time: {0:n3}s\n", playheadFR.FrameTimeInS);
								}
							}
							else
							{
								newLabelText = string.Format("Frame: {0:n0}, Time: {1:n3}s, y: {2:n2}\n", logData.GetDisplayIndex(playheadFR.Index), playheadFR.FrameTimeInS, globalY);
							}

							FrameRecord leftFR = frameRecords[Math.Max(playheadFRIdx - 1, 0)];
							FrameRecord rightFR = frameRecords[Math.Min(playheadFRIdx + 1, logView.m_maxValidIdx)];

							m_currentLabel = "";

							if (leftFR.FrameTimeInS <= rightFR.FrameTimeInS)
							{
								foreach (OverviewRDI ordi in m_logControl.m_ordiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
								{
									GetRecordValue ldGrv = fr => fr.Values[ordi.ValuesPath];
									GetRecordValue lvGrv = fr => logView.m_viewFRs[logData.GetDisplayIndex(fr.Index)].Values[ordi.ValuesPath];
									GetRecordValue grv = ordi.ValueIsPerLogView ? lvGrv : ldGrv;
									GetRecordValue grvDisplay = fr => grv(fr) * ordi.DisplayScale;

									float leftFRx = leftFR.GetDisplayXValue(logData, m_againstFrameTime);
									float closestFRx = playheadFR.GetDisplayXValue(logData, m_againstFrameTime);
									float rightFRx = rightFR.GetDisplayXValue(logData, m_againstFrameTime);

									float distSqToLeftLine = DistanceSqToLine(globalX, globalY, leftFRx, grvDisplay(leftFR), closestFRx, grvDisplay(playheadFR));
									float distSqToRightLine = DistanceSqToLine(globalX, globalY, closestFRx, grvDisplay(playheadFR), rightFRx, grvDisplay(rightFR));

									if (Math.Min(distSqToLeftLine, distSqToRightLine) < 400)
									{
										newLabelText += indentStr + string.Format("{0}: {1}\n", ordi.ValuesPath, grv(playheadFR));
										m_currentLabel = ordi.ValuesPath;
									}
								}

								m_infoLabel.Show();
							}

							if (m_highlightedFR != null)
							{
								if (globalY > 0.0f)
								{
									float y = 0.0f;

									foreach (RDIElementValue<ProfilerRDI> prdiEv in m_logControl.m_prdiTree.GetValueEnumerator(m_highlightedFR))
									{
										y += prdiEv.m_value;

										if (globalY < y)
										{
											m_highlightedPrdi = prdiEv.m_rdi;
											break;
										}
									}
								}

								if (m_highlightedPrdi != null)
								{
									string highlightedPath = m_highlightedPrdi.Path;
									newLabelText += indentStr + highlightedPath + "\n";

									if (m_highlightedPrdi.IsCollapsed)
									{
										string valuePath = m_highlightedPrdi.LeafPath;
										newLabelText += indentStr + string.Format(" {0}: {1}{2}\n", m_highlightedPrdi.LeafName, m_highlightedVFR.Values[valuePath], m_highlightedPrdi.LeafName.Equals("selfTimeInMS") ? "ms" : "");
									}
									else
									{
										List<string> subItemStrs = new List<string>();

										foreach (string path in m_highlightedFR.GetPathEnumerator(highlightedPath + "/*"))
										{
											int indexOfLastSlash = path.LastIndexOf("/");
											string name = path.Substring(indexOfLastSlash + 1);
											subItemStrs.Add(string.Format("{0}: {1}{2}", name, m_highlightedFR.Values[path], name.Equals("selfTimeInMS") ? "ms" : ""));
										}

										for (int i = 0; i < subItemStrs.Count; i++)
										{
											if (i == 0)
											{
												newLabelText += indentStr + "  ";
											}

											newLabelText += subItemStrs[i];

											if (i != subItemStrs.Count - 1)
											{
												newLabelText += ", ";
											}
											else
											{
												newLabelText += "\n";
											}
										}
									}
								}

								string userMarkerText = "";
								int numMarkers = 0;
								const int maxNumMarkers = 30;

								for (int i = longestFrameStartIndex; i <= longestFrameEndIndex; i++)
								{
									FrameRecord fr = frameRecords[i];

									foreach (UserMarkerLocation userMarker in fr.UserMarkers)
									{
										UserMarkerRDI umrdi = m_logControl.m_urdiTree.GetIfDisplayed(userMarker, logView);

										if (umrdi != null)
										{
											userMarkerText += indentStr + umrdi.DisplayPath + "\n";
											numMarkers++;

											if (numMarkers > maxNumMarkers)
											{
												userMarkerText += indentStr + "...\n";
												goto doneAddingMarkers;
											}
										}
									}
								}

								doneAddingMarkers:
								newLabelText += userMarkerText;

								if (m_highlightedFR.Values.ContainsPath("/posx"))
								{
									float px = m_highlightedFR.Values["/posx"];
									float py = m_highlightedFR.Values["/posy"];
									float pz = m_highlightedFR.Values["/posz"];

									float rx = m_highlightedFR.Values["/rotx"];
									float ry = m_highlightedFR.Values["/roty"];
									float rz = m_highlightedFR.Values["/rotz"];

									newLabelText += indentStr + string.Format("\nPos: {0}, {1}, {2}\nRot: {3}, {4}, {5}\n", px, py, pz, rx, ry, rz);
								}
							}

							SetInfoLabelLocationFromMousePos(e.X, e.Y);
						}
					}
				}
			}

			if (newLabelText.Length > 0)
			{
				m_infoLabel.Show();
			}
			else
			{
				m_infoLabel.Hide();
			}

			if (m_logViews.Count > 1)
			{
				if (m_againstFrameTime)
				{
					newLabelText = string.Format("Time: {0:n3}s, y: {1:n2}\n", m_playheadX, globalY) + newLabelText;
				}
				else
				{
					newLabelText = string.Format("Frame: {0:n0}, y: {1:n2}\n", (int)m_playheadX, globalY) + newLabelText;
				}
			}

			m_infoLabel.Text = newLabelText;
			m_infoLabel.PaintNow();

			m_logControl.screenshotPictureBox.Image = newScreenshotImage;
			m_logControl.screenshotPictureBox.PaintNow();

			if ((e.Button == MouseButtons.Left || e.Button == MouseButtons.Right) && (m_mouseDownButton != MouseButtons.None))	// stupid mouse clicking behaviour on load
			{
				float xChange = (float)(e.X - m_lastButtonPressedX) / this.Width;
				float yChange = -(float)(e.Y - m_lastButtonPressedY) / this.Height;

				if (e.Button == MouseButtons.Left)
				{
					if (StatoscopeForm.m_shiftIsBeingPressed)
					{
						if (!m_zoomBoxing)
						{
							m_zoomBoxStartPosX = WindowXToUnitsX(e.X);
							m_zoomBoxStartPosY = WindowYToUnitsY(e.Y);
							m_zoomBoxing = true;
						}
					}

					if (m_zoomBoxing)
					{
						m_zoomBoxEndPosX = WindowXToUnitsX(e.X);
						m_zoomBoxEndPosY = WindowYToUnitsY(e.Y);
					}
					else
					{
						m_centreX -= m_sizeX * xChange;
						m_centreY -= m_sizeY * yChange;
						m_centreTrackOffsetX += m_sizeX * xChange;
					}
				}
				else if (m_isScaling)
				{
					if (ScaleAboutCentre)
					{
						m_sizeX -= 5.0f * m_sizeX * xChange;
						m_sizeY -= 5.0f * m_sizeY * yChange;

						const float epsilon = 0.0001f;

						if (m_sizeX < epsilon)
						{
							m_sizeX = epsilon;
						}

						if (m_sizeY < epsilon)
						{
							m_sizeY = epsilon;
						}
					}
					else
					{
						double sxCurrent = WindowXToUnitsX(m_scaleDragOrigin.X);
						double syCurrent = WindowYToUnitsY(m_scaleDragOrigin.Y);

						Point ptDisplacement = new Point(e.Location.X - m_scaleDragStart.X, e.Location.Y - m_scaleDragStart.Y);
						m_scaleDragStart = e.Location;

						double samplesPerPixelX = PixelsToUnitsX(1);
						double samplesPerPixelY = PixelsToUnitsY(1);
						double sx = ptDisplacement.X * samplesPerPixelX;
						double sy = -ptDisplacement.Y * samplesPerPixelY;

						const double ZoomDragScaleX = 2.5;
						const double ZoomDragScaleY = 2.5;

						RectD view = new RectD(LeftUnits, TopUnits, RightUnits, BottomUnits);

						view = new RectD(
						  view.Left + sx * ZoomDragScaleX, view.Top + sy * ZoomDragScaleY,
						  view.Right - sx * ZoomDragScaleX, view.Bottom - sy * ZoomDragScaleY);
						SetViewBounds(view);

						double sxNew = WindowXToUnitsX(m_scaleDragOrigin.X);
						double syNew = WindowYToUnitsY(m_scaleDragOrigin.Y);

						view = new RectD(
						  view.Left - (sxNew - sxCurrent), view.Top - (syNew - syCurrent),
						  view.Right - (sxNew - sxCurrent), view.Bottom - (syNew - syCurrent));
						SetViewBounds(view);
					}
				}

				m_lastButtonPressedX = e.X;
				m_lastButtonPressedY = e.Y;
			}
			else if (m_selecting && ((e.Button & MouseButtons.Middle) != 0))
			{
				m_selectEndSeconds = UnitsToSeconds(WindowXToUnitsX(e.X));
			}

			Invalidate(true);

			base.OnMouseMove(e);
		}

		protected override void OnMouseLeave(EventArgs e)
		{
			m_infoLabel.Hide();
			m_playheadX = -1.0f;
			base.OnMouseLeave(e);
		}

		private void SetViewBounds(RectD view)
		{
			m_centreX = (float)((view.Left + view.Right) / 2.0);
			m_centreY = (float)((view.Top + view.Bottom) / 2.0);
			m_sizeX = (float)view.Width;
			m_sizeY = (float)view.Height;
		}

		private bool m_selecting;
		private float m_selectAnchorSeconds;
		private float m_selectEndSeconds;

		public float SelectionStartInSeconds
		{
			get
			{
				return Math.Min(m_selectAnchorSeconds, m_selectEndSeconds);
			}
			set
			{
				if (m_selectAnchorSeconds != value)
				{
					m_selectAnchorSeconds = value;
					Invalidate();
				}
			}
		}

		public float SelectionEndInSeconds
		{
			get
			{
				return Math.Max(m_selectAnchorSeconds, m_selectEndSeconds);
			}
			set
			{
				if (m_selectEndSeconds != value)
				{
					m_selectEndSeconds = value;
					Invalidate();
				}
			}
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			m_mouseDownButton = e.Button;
			m_lastButtonPressedX = e.X;
			m_lastButtonPressedY = e.Y;
			this.Focus();

			if ((e.Button == MouseButtons.Left) && !StatoscopeForm.m_shiftIsBeingPressed)
			{
				m_logControl.UpdateCallstacks(m_highlightedFR);

				if ((m_highlightedFR != null) && (m_highlightedPrdi != null))
				{
					m_logControl.m_prdiTreeView.SelectedRDINode = m_highlightedPrdi;

					OnSelectedHighlight();
				}
			}
			else if ((e.Button & MouseButtons.Middle) != 0)
			{
				m_selecting = true;
				m_selectAnchorSeconds = UnitsToSeconds(WindowXToUnitsX(e.X));
				m_selectEndSeconds = m_selectAnchorSeconds;
				Capture = true;
			}
			else if ((e.Button & MouseButtons.Right) != 0)
			{
				m_isScaling = true;
				m_scaleDragStart = e.Location;
				m_scaleDragOrigin = e.Location;
			}

			base.OnMouseDown(e);
		}

		protected override void OnMouseDoubleClick(MouseEventArgs e)
		{
			base.OnMouseDoubleClick(e);

			if ((m_highlightedPrdi != null) && (e.Button == MouseButtons.Left) && !StatoscopeForm.m_shiftIsBeingPressed)
			{
				m_logControl.SelectTreeViewTabPage("functionProfileTabPage");
				m_logControl.m_prdiTreeView.SelectedRDINode = m_highlightedPrdi;
			}
		}

		public event EventHandler SelectionChanged;
		public event EventHandler SelectedHighlight;

		protected void OnSelectionChanged()
		{
			if (SelectionChanged != null)
			{
				SelectionChanged(this, EventArgs.Empty);
			}
		}

		protected void OnSelectedHighlight()
		{
			if (SelectedHighlight != null)
			{
				SelectedHighlight(this, EventArgs.Empty);
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			base.OnMouseUp(e);

			m_mouseDownButton = MouseButtons.None;

			if (m_zoomBoxing)
			{
				m_sizeX = Math.Abs(m_zoomBoxEndPosX - m_zoomBoxStartPosX);
				m_sizeY = Math.Abs(m_zoomBoxEndPosY - m_zoomBoxStartPosY);
				m_centreX = (m_zoomBoxEndPosX + m_zoomBoxStartPosX) / 2.0f;
				m_centreY = (m_zoomBoxEndPosY + m_zoomBoxStartPosY) / 2.0f;
				m_zoomBoxing = false;
			}

			if ((e.Button & MouseButtons.Middle) != 0 && m_selecting)
			{
				Capture = false;
				m_selecting = false;
				OnSelectionChanged();
			}

			if ((e.Button & MouseButtons.Right) != 0)
			{
				m_isScaling = false;
			}

			UpdateCentreTrackOffsetX();
		}

		protected override void OnKeyDown(KeyEventArgs e)
		{
			if (e.KeyCode >= Keys.NumPad1 && e.KeyCode <= Keys.NumPad9)
			{
				/*
				int idx;
				switch (e.KeyCode)
				{
				  case Keys.NumPad1: idx = 0; break;
				  case Keys.NumPad2: idx = 1; break;
				  case Keys.NumPad3: idx = 2; break;
				  case Keys.NumPad4: idx = 3; break;
				  case Keys.NumPad5: idx = 4; break;
				  case Keys.NumPad6: idx = 5; break;
				  case Keys.NumPad7: idx = 6; break;
				  case Keys.NumPad8: idx = 7; break;
				  case Keys.NumPad9: idx = 8; break;
				  default: idx = 0; break;
				}
				if (m_statoscopeForm.m_markers[idx]==null)
				  m_statoscopeForm.m_markers[idx]=new StatoscopeForm.Marker();
				StatoscopeForm.Marker m = m_statoscopeForm.m_markers[idx];
				if (m.m_bStart)
				{
				  m.m_start = m_playheadFR;
				  m.m_system = m_currentLabel;
				}
				else
				{
				  m.m_end = m_playheadFR;
				}
				m.m_bStart = !m.m_bStart;
				 * */
			}
		}

		public void PrepareViewport()
		{
			OpenGL.glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
			OpenGL.glClear(OpenGL.GL_COLOR_BUFFER_BIT | OpenGL.GL_DEPTH_BUFFER_BIT);

			OpenGL.glDisable(OpenGL.GL_TEXTURE_2D);
			OpenGL.glEnable(OpenGL.GL_COLOR_MATERIAL);
			OpenGL.glEnable(OpenGL.GL_BLEND);
			OpenGL.glEnable(OpenGL.GL_DEPTH_TEST);
			OpenGL.glDepthFunc(OpenGL.GL_LEQUAL);
			OpenGL.glBlendFunc(OpenGL.GL_SRC_ALPHA, OpenGL.GL_ONE_MINUS_SRC_ALPHA);

			OpenGL.glViewport(0, 0, this.Width, this.Height);

			OpenGL.glMatrixMode(OpenGL.GL_PROJECTION);
			OpenGL.glLoadIdentity();
			GLU.gluOrtho2D(LeftUnits, RightUnits, TopUnits, BottomUnits);

			OpenGL.glMatrixMode(OpenGL.GL_MODELVIEW);
			OpenGL.glLoadIdentity();
		}

		protected override void OnPaint(PaintEventArgs pevent)
		{
			base.OnPaint(pevent);
		}

		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				foreach (FRElementBarsDrawBuffer db in m_drawBuffers)
				{
					db.Dispose();
				}

				m_drawBuffers.Clear();
			}
            if (m_glFont != null)
            {
                m_glFont.Dispose();
            }
			base.Dispose(disposing);
		}

		float FindMaxFrameLength()
		{
			float maxFrameLength = 0.0f;
			//foreach (FrameRecord fr in m_logData.FrameRecords)
			{
				maxFrameLength = 1000.0f; // Math.Max(maxFrameLength, fr.m_frameLengthInMS);
			}
			return maxFrameLength;
		}

		int RoundUp(float value, int toMultipleOf)
		{
			return (((int)Math.Ceiling(value) + toMultipleOf - 1) / toMultipleOf) * toMultipleOf;
		}

		const int bigXTicsEvery = 10;
		const int bigYTicsEvery = 10;
		const int smallXTicsEvery = 1;
		const int smallYTicsEvery = 2;

		float MinX()
		{
			return 0.0f;
		}

		float MaxX()
		{
			float largestX = float.MinValue;

			foreach (LogView logView in m_logViews)
			{
				LogData logData = logView.m_logData;
				List<FrameRecord> frameRecords = logData.FrameRecords;

				if (frameRecords.Count == 0 || logView.m_maxValidIdx < 0)
				{
					continue;
				}

				FrameRecord lastFR = frameRecords[logView.m_maxValidIdx];
				float maxX = m_againstFrameTime ? lastFR.FrameTimeInS - frameRecords.First().FrameTimeInS : logData.GetDisplayIndex(lastFR.Index);

				if (maxX > largestX)
				{
					largestX = maxX;
				}
			}

			return largestX != float.MinValue ? largestX : 0.0f;
		}

		float MinY()
		{
			return 0.0f;
		}

		float MaxY()
		{
			return (float)RoundUp(FindMaxFrameLength(), bigYTicsEvery);
		}

		public void DrawAxes()
		{
			OpenGL.glLineWidth(2.0f);
			OpenGL.glColor4f(0.75f, 0.75f, 0.75f, 0.75f);
			OpenGL.glBegin(OpenGL.GL_LINES);

			float bigTicSize = 0.35f;
			float smallTicSize = 0.2f;

			float minX = MinX();
			float maxX = MaxX();

			float minY = MinY();
			float maxY = MaxY();

			// x-axis
			OpenGL.glVertex2f(minX, minY);
			OpenGL.glVertex2f(maxX, minY);

			int numBigXTics = 1 + (int)(maxX - minX) / bigXTicsEvery;
			int numSmallXTics = 1 + (int)(maxX - minX) / smallXTicsEvery;

			for (int i = 0; i < numBigXTics; i++)
			{
				int x = RoundUp(minX + (i * bigXTicsEvery), bigXTicsEvery);
				OpenGL.glVertex2f(x, minY - bigTicSize);
				OpenGL.glVertex2f(x, minY);
			}

			for (int i = 0; i < numSmallXTics; i++)
			{
				int x = RoundUp(minX + (i * smallXTicsEvery), smallXTicsEvery);

				if ((x % bigXTicsEvery) != 0)
				{
					OpenGL.glVertex2f(x, minY - smallTicSize);
					OpenGL.glVertex2f(x, minY);
				}
			}

			// y-axis
			OpenGL.glVertex2f(minX, minY);
			OpenGL.glVertex2f(minX, maxY);

			int numBigYTics = 1 + (int)(maxY - minY) / bigYTicsEvery;
			int numSmallYTics = 1 + (int)(maxY - minY) / smallYTicsEvery;

			for (int i = 0; i < numBigYTics; i++)
			{
				int y = RoundUp(minY + (i * bigYTicsEvery), bigYTicsEvery);
				OpenGL.glVertex2f(minX - bigTicSize, y);
				OpenGL.glVertex2f(minX, y);
			}

			for (int i = 0; i < numSmallYTics; i++)
			{
				int y = RoundUp(minY + (i * smallYTicsEvery), smallYTicsEvery);

				if ((y % bigYTicsEvery) != 0)
				{
					OpenGL.glVertex2f(minX - smallTicSize, y);
					OpenGL.glVertex2f(minX, y);
				}
			}

			OpenGL.glEnd();
		}

		void DrawPlayHead()
		{
			if (m_playheadX != -1.0f)
			{
				OpenGL.glLineWidth(1.0f);
				OpenGL.glColor4f(1.0f, 0.0f, 0.0f, 1.0f);

				OpenGL.glBegin(OpenGL.GL_LINES);
				OpenGL.glVertex3f(m_playheadX, MinY(), 1.0f);
				OpenGL.glVertex3f(m_playheadX, MaxY(), 1.0f);
				OpenGL.glEnd();
			}
		}

		void DrawMarkers()
		{
			/*
			const float z = 1.0f;
			OpenGL.glLineWidth(1.0f);
			foreach (StatoscopeForm.Marker m in m_statoscopeForm.m_markers)
			{
			  if (m != null)
			  {
			    OpenGL.glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
			    OpenGL.glBegin(OpenGL.GL_LINES);
			    if (m.m_start != null)
			    {
			      float x = m.m_start.FrameTimeInS;
			      OpenGL.glVertex3f(x, MinY(), z);
			      OpenGL.glVertex3f(x, MaxY(), z);
			    }
			    if (m.m_end != null)
			    {
			      float x = m.m_end.FrameTimeInS;
			      OpenGL.glVertex3f(x, MinY(), z);
			      OpenGL.glVertex3f(x, MaxY(), z);
			    }
			    OpenGL.glEnd();
			  }
			}
			*/
		}

		void DrawTargetLines()
		{
			float windowAspect = ClientSize.Width / (float)ClientSize.Height;
			float viewportAspect = m_sizeX / m_sizeY;
			float xyAspect = windowAspect / viewportAspect;
			float textHeightUnits = PixelsToUnitsY(13.0f);
			float yTextScale = textHeightUnits / GLFont.Height;
			float xTextScale = yTextScale / xyAspect;
			float leftBorderWidthUnits = 0.0f;
			float rightBorderWidthUnits = 0.0f;
			float textSpacingUnits = PixelsToUnitsX(7.0f);

			OpenGL.glDisable(OpenGL.GL_DEPTH_TEST);

			foreach (TargetLineRDI trdi in m_logControl.m_trdiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
			{
				float textWidthUnits = GLFont.MeasureWidth(trdi.Name) * xTextScale;

				switch (trdi.LabelLocation)
				{
				case TargetLineRDI.ELabelLocation.ELL_Left:
					leftBorderWidthUnits = Math.Max(leftBorderWidthUnits, textSpacingUnits + textWidthUnits + textSpacingUnits);
					break;

				case TargetLineRDI.ELabelLocation.ELL_Right:
					rightBorderWidthUnits = Math.Max(rightBorderWidthUnits, textSpacingUnits + textWidthUnits + textSpacingUnits);
					break;
				}

				DrawTargetLine(trdi.Colour, trdi.DisplayValue);
			}

			DrawLabelBorders(leftBorderWidthUnits, rightBorderWidthUnits);

			foreach (TargetLineRDI trdi in m_logControl.m_trdiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
			{
				float x = 0.0f;
				float y = trdi.DisplayValue + (textHeightUnits / 2.0f);
				float textWidthUnits = GLFont.MeasureWidth(trdi.Name) * xTextScale;

				switch (trdi.LabelLocation)
				{
				case TargetLineRDI.ELabelLocation.ELL_Left:
					leftBorderWidthUnits = Math.Max(leftBorderWidthUnits, textSpacingUnits + textWidthUnits + textSpacingUnits);
					x = LeftUnits + leftBorderWidthUnits - (textWidthUnits + textSpacingUnits);
					break;

				case TargetLineRDI.ELabelLocation.ELL_Right:
					rightBorderWidthUnits = Math.Max(rightBorderWidthUnits, textSpacingUnits + textWidthUnits + textSpacingUnits);
					x = RightUnits - rightBorderWidthUnits + textSpacingUnits;
					break;
				}

				GLFont.Draw(x, y, trdi.Colour.ConvertToSysDrawCol(), trdi.Name, xTextScale, -yTextScale);
			}

			OpenGL.glEnable(OpenGL.GL_DEPTH_TEST);
		}

		void DrawTargetLine(RGB colour, float value)
		{
			OpenGL.glLineWidth(1.0f);
			OpenGL.glColor4f(colour.r, colour.g, colour.b, 0.5f);

			OpenGL.glBegin(OpenGL.GL_LINES);
			OpenGL.glVertex2f(LeftUnits, value);
			OpenGL.glVertex2f(RightUnits, value);
			OpenGL.glEnd();
		}

		void DrawLabelBorders(float leftWidth, float rightWidth)
		{
			OpenGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			OpenGL.glBegin(OpenGL.GL_QUADS);

			OpenGL.glVertex2f(LeftUnits, TopUnits);
			OpenGL.glVertex2f(LeftUnits, BottomUnits);
			OpenGL.glVertex2f(LeftUnits + leftWidth, BottomUnits);
			OpenGL.glVertex2f(LeftUnits + leftWidth, TopUnits);

			OpenGL.glVertex2f(RightUnits - rightWidth, TopUnits);
			OpenGL.glVertex2f(RightUnits - rightWidth, BottomUnits);
			OpenGL.glVertex2f(RightUnits, BottomUnits);
			OpenGL.glVertex2f(RightUnits, TopUnits);

			OpenGL.glEnd();
		}

		public void DrawUserMarkerLines(LogView logView)
		{
			List<FrameRecord> frameRecords = logView.m_logData.FrameRecords;
			float firstMidTime = frameRecords.First().FrameMidTimeInS;
			float lastMidTime = frameRecords[logView.m_maxValidIdx].FrameMidTimeInS;

			foreach (UserMarkerRDI umrdi in m_logControl.m_urdiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
			{
				if (umrdi.m_logView == logView)
				{
					float xDisplayValue = umrdi.m_uml.m_fr.GetDisplayXValue(logView.m_logData, m_againstFrameTime);
					bool bInRange = m_againstFrameTime ? (firstMidTime <= xDisplayValue && xDisplayValue <= lastMidTime)
					                : (0 <= xDisplayValue && xDisplayValue <= logView.m_maxValidIdx);

					if (bInRange)
					{
						DrawUserMarkerLine(umrdi.Colour, xDisplayValue);
					}
				}
			}
		}

		public void DrawUserMarkerLine(RGB colour, float value)
		{
			OpenGL.glLineWidth(1.0f);
			OpenGL.glColor4f(colour.r, colour.g, colour.b, 0.5f);

			float z = 1.0f;

			OpenGL.glBegin(OpenGL.GL_LINES);
			OpenGL.glVertex3f(value, MinY(), z);
			OpenGL.glVertex3f(value, MaxY(), z);
			OpenGL.glEnd();
		}

		void DrawZoneHighlighters(LogView logView)
		{
			ZoneHighlighterInstance.HighlightZoneContext ctx = new ZoneHighlighterInstance.HighlightZoneContext(m_againstFrameTime, MinX(), MaxX(), MinY(), MaxY());

			foreach (ZoneHighlighterRDI zrdi in m_logControl.m_zrdiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
			{
				logView.m_zoneHighlighterInstances[zrdi].Draw(ctx);
			}
		}

		void DrawFrameRecordLines(LogView logView)
		{
			foreach (OverviewRDI ordi in m_logControl.m_ordiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
			{
				List<uint> displayLists = logView.GetDisplayLists(ordi.ValuesPath);

				if (logView.m_maxValidIdx > (displayLists.Count + 1) * c_numFramesPerLineDisplayList)
				{
					CreateFrameRecordLineDisplayLists(logView, ordi, displayLists);
				}

				// colour isn't set in the display list to make it quicker to change
				RGB colour = logView.m_singleOrdiColour != null ? logView.m_singleOrdiColour : ordi.Colour;
				OpenGL.glColor4f(colour.r, colour.g, colour.b, 1.0f);	// may need to move this into the DL version of DrawFrameRecordLine() - socket logging, new part

				DrawFrameRecordLine(logView, ordi, m_againstFrameTime, (displayLists.Count * c_numFramesPerLineDisplayList) - 1, logView.m_maxValidIdx);
				DrawFrameRecordLine(displayLists);

				// check for user scaling and if so, inform them
				if (m_logControl.GetUserDisplayScale(ordi.ValuesPath) != 1.0f)
				{
					float windowAspect = ClientSize.Width / (float)ClientSize.Height;
					float viewportAspect = m_sizeX / m_sizeY;
					float xyAspect = windowAspect / viewportAspect;
					float textHeightUnits = PixelsToUnitsY(13.0f);
					float yTextScale = textHeightUnits / GLFont.Height;
					float xTextScale = yTextScale / xyAspect;
					float rightBorderWidthUnits = 0.0f;
					float textSpacingUnits = PixelsToUnitsX(7.0f);
					string infoText = "Scaled by user";
					float textWidthUnits = GLFont.MeasureWidth(infoText) * xTextScale;

					// make sure we have enough distance if a target line is being drawn (white borders) - this is duplicated from DrawTargetLines but no way around
					foreach (TargetLineRDI trdi in m_logControl.m_trdiTree.GetEnumerator(ERDIEnumeration.OnlyDrawnLeafNodes))
					{
						float targetLineTextWidthUnits = GLFont.MeasureWidth(trdi.Name) * xTextScale;

						switch (trdi.LabelLocation)
						{
						case TargetLineRDI.ELabelLocation.ELL_Right:
							rightBorderWidthUnits = Math.Max(rightBorderWidthUnits, textSpacingUnits + targetLineTextWidthUnits + textSpacingUnits);
							break;
						}
					}

					// calculate text position and draw - todo: take Y from last visible
					rightBorderWidthUnits = rightBorderWidthUnits + (textSpacingUnits + textWidthUnits + textSpacingUnits);
					float x = RightUnits - rightBorderWidthUnits + textSpacingUnits;

					int frameIdx = Math.Min((int)RightUnits, logView.m_maxValidIdx);
					FrameRecord fr = logView.m_logData.FrameRecords[frameIdx];
					ViewFrameRecord vfr = logView.m_viewFRs[frameIdx];
					IReadOnlyFRVs frvs = ordi.ValueIsPerLogView ? vfr.Values : fr.Values;
					float y = frvs[ordi.ValuesPath] * ordi.DisplayScale;

					GLFont.Draw(x, y, ordi.Colour.ConvertToSysDrawCol(), infoText, xTextScale, -yTextScale);
				}
			}
		}

		const int c_numFramesPerLineDisplayList = 2500;

		public void CreateFrameRecordLineDisplayLists(LogView logView, OverviewRDI ordi, List<uint> displayLists)
		{
			for (int i = displayLists.Count * c_numFramesPerLineDisplayList; i < logView.m_maxValidIdx - c_numFramesPerLineDisplayList; i += c_numFramesPerLineDisplayList)
			{
				uint displayList = OpenGL.glGenLists(1);
				displayLists.Add(displayList);

				OpenGL.glNewList(displayList, OpenGL.GL_COMPILE);
				DrawFrameRecordLine(logView, ordi, m_againstFrameTime, i - 1, i + c_numFramesPerLineDisplayList);
				OpenGL.glEndList();
			}
		}

		public void DrawFrameRecordLine(List<uint> displayLists)
		{
			foreach (uint displayList in displayLists)
			{
				OpenGL.glCallList(displayList);
			}
		}

		void DrawFrameRecordLine(LogView logView, OverviewRDI ordi, bool againstFrameTime, int fromIdx, int toIdx)
		{
			OpenGL.glLineWidth(1.0f);

			OpenGL.glBegin(OpenGL.GL_LINE_STRIP);

			LogData logData = logView.m_logData;
			fromIdx = Math.Max(0, Math.Min(fromIdx, logView.m_maxValidIdx));
			toIdx = Math.Max(0, Math.Min(toIdx, logView.m_maxValidIdx));

			float lastX = -1.0f;

			for (int i = fromIdx; i <= toIdx; i++)
			{
				FrameRecord fr = logData.FrameRecords[i];
				ViewFrameRecord vfr = logView.m_viewFRs[i];
				IReadOnlyFRVs frvs = ordi.ValueIsPerLogView ? vfr.Values : fr.Values;
				float x = fr.GetDisplayXValue(logData, againstFrameTime);
				float y = frvs[ordi.ValuesPath] * ordi.DisplayScale;

				if (!againstFrameTime)
				{
					x += ordi.FrameOffset;
				}

				// is this a leftover from the early comparoscope? - check timelapse view
				if (x < lastX)
				{
					OpenGL.glEnd();
					OpenGL.glBegin(OpenGL.GL_LINE_STRIP);
				}

				lastX = x;

				OpenGL.glVertex3f(x, y, 0.75f);
			}

			OpenGL.glEnd();
		}

		float GetDisplayedElementBarWidth()
		{
			float minWidthPixels = 5;
			return PixelsToUnitsX(minWidthPixels);
		}

		unsafe void BuildElementBar(BarVertex* bv, float x, float y, float width, float height, uint colour, uint outlineColour, float pxInUnitsX, float pxInUnitsY)
		{
			bv[0].x = x;
			bv[0].y = y;
			bv[0].z = 0;
			bv[0].color = outlineColour;
			bv[1].x = x + width;
			bv[1].y = y;
			bv[1].z = 0;
			bv[1].color = outlineColour;
			bv[2].x = x + width;
			bv[2].y = y + height;
			bv[2].z = 0;
			bv[2].color = outlineColour;
			bv[3].x = x;
			bv[3].y = y + height;
			bv[3].z = 0;
			bv[3].color = outlineColour;

			bv[4].x = x + pxInUnitsX;
			bv[4].y = y + pxInUnitsY;
			bv[4].z = 0.5f;
			bv[4].color = colour;
			bv[5].x = x - pxInUnitsX + width;
			bv[5].y = y + pxInUnitsY;
			bv[5].z = 0.5f;
			bv[5].color = colour;
			bv[6].x = x - pxInUnitsX + width;
			bv[6].y = y + height - pxInUnitsY;
			bv[6].z = 0.5f;
			bv[6].color = colour;
			bv[7].x = x + pxInUnitsX;
			bv[7].y = y + height - pxInUnitsY;
			bv[7].z = 0.5f;
			bv[7].color = colour;
		}

		RGB m_segmentHighlightColour = new RGB(1.0f, 1.0f, 1.0f);
		RGB m_frameHighlightColour = new RGB(0.5f, 0.5f, 0.5f);
		RGB m_noHighlightColour = new RGB(0.0f, 0.0f, 0.0f);

		struct SFRElementBarsDraw
		{
			public IReadOnlyFRVs DataValues;
			public IReadOnlyFRVs ViewValues;
			public float X;
			public float Width;
			public float Alpha;
			public bool Highlight;

			public SFRElementBarsDraw(IReadOnlyFRVs dataValues, IReadOnlyFRVs viewValues, float x, float width, float alpha, bool highlight)
			{
				DataValues = dataValues;
				ViewValues = viewValues;
				X = x;
				Width = width;
				Alpha = alpha;
				Highlight = highlight;
			}
		}

		class CFRElementBarsDrawArgs
		{
			public ProfilerRDI.DisplayList DisplayList;
			public MTCounter JobCounter;
			public FRElementBarsDrawBuffer DrawBuffer;
			public SFRElementBarsDraw[] Frames;
		}

		private List<FRElementBarsDrawBuffer> m_drawBuffers = new List<FRElementBarsDrawBuffer>();

		void DrawFrameRecordElementBars(Object state)
		{
			CFRElementBarsDrawArgs args = (CFRElementBarsDrawArgs)state;

			uint uiSegHlColor = m_segmentHighlightColour.ConvertToAbgrUint();
			uint uiFrameHlColor = m_frameHighlightColour.ConvertToAbgrUint();
			uint uiNoHlColor = m_noHighlightColour.ConvertToAbgrUint();

			float onePixelInUnitsX = PixelsToUnitsX(1.0f);
			float onePixelInUnitsY = PixelsToUnitsY(1.0f);

			unsafe
			{
				for (int fri = 0, frc = args.Frames.Length; fri != frc; ++fri)
				{
					SFRElementBarsDraw fr = args.Frames[fri];

					uint uiAlpha = (uint)(fr.Alpha * 255.0f);
					int dlcc = args.DisplayList.Cmds.Count;
					int dlccu = 0;
					KeyValuePair<BarVertex[], int> bvkv = args.DrawBuffer.BeginAllocate(dlcc * 8);

					fixed (BarVertex* bvbase = bvkv.Key)
					{
						BarVertex* bv = bvbase != IntPtr.Zero.ToPointer() ? bvbase + bvkv.Value : bvbase;

						float y = 0.0f;
						float yf = 0.0f;

						for (int dlci = 0; dlci != dlcc; ++dlci)
						{
							ProfilerRDI.DisplayListCmd cmd = args.DisplayList.Cmds[dlci];

							float height = 0.0f;

							for (int i = 0, c = cmd.Count; i != c; ++i)
							{
								ProfilerRDI.ListItem di = args.DisplayList.Items[cmd.StartIndex + i];
								IReadOnlyFRVs values = di.ValueIsPerLogView ? fr.ViewValues : fr.DataValues;
								height += values[di.ValuePathIdx] * di.Scale;
							}

							if (height == 0.0f)
							{
								continue;
							}

							if (y + height - yf > onePixelInUnitsY * 0.2f)
							{
								uint outlineColour;

								if (fr.Highlight)
								{
									if (m_highlightedPrdi == cmd.PRDI)
									{
										outlineColour = uiSegHlColor;
									}
									else
									{
										outlineColour = uiFrameHlColor;
									}
								}
								else
								{
									outlineColour = uiNoHlColor;
								}

								uint colourR = (cmd.Colour >> 16) & 0xff;
								uint colourG = (cmd.Colour >> 8) & 0xff;
								uint colourB = (cmd.Colour >> 0) & 0xff;
								colourR = (colourR * uiAlpha >> 8) + (0xff - uiAlpha);
								colourG = (colourG * uiAlpha >> 8) + (0xff - uiAlpha);
								colourB = (colourB * uiAlpha >> 8) + (0xff - uiAlpha);

								// Encode as ABGR (RGBA little endian)
								uint colour = (uint)((0xff << 24) | (colourB << 16) | (colourG << 8) | colourR);
								outlineColour |= uiAlpha << 24;

								if (bv != IntPtr.Zero.ToPointer())
								{
									BuildElementBar(bv, fr.X, y, fr.Width, height, colour, outlineColour, onePixelInUnitsX, onePixelInUnitsY);
									bv += 8;
								}

								++dlccu;
								yf = y + height;
							}

							y += height;
						}
					}

					args.DrawBuffer.EndAllocate(dlcc * 8, dlccu * 8);
				}
			}

			args.JobCounter.Increment();
		}

		void DrawElementBars(LogView logView)
		{
			LogData logData = logView.m_logData;
			float lastBarEndTime = logData.FrameRecords[0].FrameTimeInS;
			float width = GetDisplayedElementBarWidth();
			int drawEvery = (int)Math.Max(1, Math.Ceiling(width));
			bool comboFrame = false;

			float startValue = LeftUnits;
			float endValue = (float)Math.Ceiling(RightUnits);

			int startIndex, endIndex;

			if (m_againstFrameTime)
			{
				FrameRecord firstFullFR = logData.FrameRecords.Find(fr => fr.FrameTimeInS > startValue);
				FrameRecord oneBeyondEndFR = logData.FrameRecords.Find(fr => fr.FrameTimeInS > endValue);

				startIndex = firstFullFR != null ? logData.GetDisplayIndex(firstFullFR.Index) - 1 : 0;
				endIndex = oneBeyondEndFR != null ? logData.GetDisplayIndex(oneBeyondEndFR.Index) - 1 : logView.m_maxValidIdx;
			}
			else
			{
				width = drawEvery;
				startIndex = ((int)startValue) / drawEvery * drawEvery;
				endIndex = (int)endValue;
				comboFrame = (drawEvery > 1);
			}

			startIndex = Math.Min(Math.Max(0, startIndex), logView.m_maxValidIdx);
			endIndex = Math.Min(logView.m_maxValidIdx, endIndex);

			OpenGL.glLineWidth(1.0f);

			IReadOnlyFRVs longestFRdataValues = null;
			IReadOnlyFRVs longestFRviewValues = null;
			float longestFrameLength = -1.0f;
			bool highlight = false;

			MTCounter numFrameRecordsCounter = new MTCounter();

			int loopStartIdx = m_againstFrameTime ? 0 : Math.Max(0, startIndex - drawEvery);
			string displayedTotalPath = m_logControl.m_prdiTree.LeafPath;

			int numJobs = 1;

			while (m_drawBuffers.Count < numJobs)
			{
				m_drawBuffers.Add(new FRElementBarsDrawBuffer(8 * 16384));
			}

			for (int i = 0; i < numJobs; ++i)
			{
				m_drawBuffers[i].Begin();
			}

			System.Diagnostics.Stopwatch sw1 = new System.Diagnostics.Stopwatch();
			System.Diagnostics.Stopwatch sw2 = new System.Diagnostics.Stopwatch();
			System.Diagnostics.Stopwatch sw3 = new System.Diagnostics.Stopwatch();

			sw1.Start();
			ProfilerRDI.DisplayList dl = m_logControl.m_prdiTree.BuildDisplayList(logView.m_dataPaths);
			sw1.Stop();

			sw2.Start();
			List<SFRElementBarsDraw> draws = new List<SFRElementBarsDraw>();

			for (int i = loopStartIdx; i <= endIndex; i++)
			{
				FrameRecord fr = logData.FrameRecords[i];

				float frameLength = logView.m_viewFRs[i].Values[displayedTotalPath];

				if (frameLength > longestFrameLength)
				{
					longestFRdataValues = fr.Values;
					longestFRviewValues = logView.m_viewFRs[i].Values;
					longestFrameLength = frameLength;
				}

				if (fr == m_highlightedFR)
				{
					highlight = true;
				}

				float x;

				if (m_againstFrameTime)
				{
					float nextBarEndTime = fr.FrameMidTimeInS + (fr.FrameMidTimeInS - fr.FrameTimeInS);

					if (nextBarEndTime < lastBarEndTime + GetDisplayedElementBarWidth())
					{
						comboFrame = true;
						continue;
					}

					x = lastBarEndTime;
					width = nextBarEndTime - lastBarEndTime;
					lastBarEndTime = nextBarEndTime;
				}
				else
				{
					if (((i + 1) % drawEvery) != 0)
					{
						continue;
					}

					x = logData.GetDisplayIndex(logData.FrameRecords[i + 1 - drawEvery].Index);
				}

				if (i >= startIndex)
				{
					draws.Add(new SFRElementBarsDraw(longestFRdataValues, longestFRviewValues, x, width, comboFrame ? 0.5f : 1.0f, highlight));
				}

				longestFrameLength = -1.0f;
				highlight = false;

				if (m_againstFrameTime)
				{
					comboFrame = false;
				}
			}

			{
				int nStep = draws.Count / numJobs;

				for (int i = 0; i < numJobs; ++i)
				{
					int nStart = i * nStep;
					int nEnd = (i != numJobs - 1)
					           ? (i + 1) * nStep
					           : draws.Count;

					CFRElementBarsDrawArgs args = new CFRElementBarsDrawArgs();
					args.DisplayList = dl;
					args.DrawBuffer = m_drawBuffers[i];
					args.Frames = draws.GetRange(nStart, nEnd - nStart).ToArray();
					args.JobCounter = numFrameRecordsCounter;
					//System.Threading.ThreadPool.QueueUserWorkItem(new WaitCallback(DrawFrameRecordElementBars), args);
					DrawFrameRecordElementBars(args);
				}
			}

			sw2.Stop();

			sw3.Start();
			numFrameRecordsCounter.WaitUntil(numJobs);

			unsafe
			{
				for (int i = 0; i < numJobs; ++i)
				{
					int nBuffers = m_drawBuffers[i].End();

					int nVertStride = System.Runtime.InteropServices.Marshal.SizeOf(typeof(BarVertex));

					OpenGL.glEnableClientState(OpenGL.GL_VERTEX_ARRAY);
					OpenGL.glEnableClientState(OpenGL.GL_COLOR_ARRAY);

					for (int bi = 0; bi < nBuffers; ++ bi)
					{
						KeyValuePair<GLBuffer, int> bufferCount = m_drawBuffers[i].GetBuffer(bi);

						if (bufferCount.Key != null)
						{
							bufferCount.Key.Bind();

							OpenGL.glColorPointer(4, OpenGL.GL_UNSIGNED_BYTE, nVertStride, new IntPtr(12).ToPointer());
							OpenGL.glVertexPointer(3, OpenGL.GL_FLOAT, nVertStride, new IntPtr(0).ToPointer());

							OpenGL.glDrawArrays(OpenGL.GL_QUADS, 0, bufferCount.Value);
						}
					}

					OpenGL.glDisableClientState(OpenGL.GL_VERTEX_ARRAY);
					OpenGL.glDisableClientState(OpenGL.GL_COLOR_ARRAY);
				}
			}
			sw3.Stop();

			//System.Diagnostics.Debugger.Log(0, "", string.Format("[of] {0} {1} {2}\n", sw1.ElapsedMilliseconds, sw2.ElapsedMilliseconds, sw3.ElapsedMilliseconds));
		}

		void UpdateCentreTrackOffsetX()
		{
			if (m_trackingLogView == null)
			{
				return;
			}

			if (m_trackingLogView.m_maxValidIdx > 0)
			{
				LogData logData = m_trackingLogView.m_logData;
				FrameRecord lastFR = logData.FrameRecords[m_trackingLogView.m_maxValidIdx];
				float lastX = lastFR.GetDisplayXValue(logData, m_againstFrameTime);
				bool lastFRisOffLeft = lastX < m_centreX - (m_sizeX / 2.0f);
				bool lastFRisOffRight = lastX > m_centreX + (m_sizeX / 2.0f);

				m_bTrackingCatchingUp = lastFRisOffLeft && m_bSmartTracking;

				if (!m_bTrackingCatchingUp && (!lastFRisOffRight || !m_bSmartTracking))
				{
					m_bTrackingEndFrame = true;
					m_centreTrackOffsetX = lastX - m_centreX;
				}
				else
				{
					m_bTrackingEndFrame = false;
				}
			}
			else
			{
				m_bTrackingCatchingUp = m_bSmartTracking;
			}
		}

		void UpdateTracking()
		{
			if (m_trackingLogView == null)
			{
				return;
			}

			if (m_trackingLogView.m_maxValidIdx > 0)
			{
				LogData logData = m_trackingLogView.m_logData;
				FrameRecord lastFR = logData.FrameRecords[m_trackingLogView.m_maxValidIdx];
				float lastX = lastFR.GetDisplayXValue(logData, m_againstFrameTime);

				if (m_bTrackingCatchingUp)
				{
					float fCatchupEndOffsetX = (m_sizeX / 2.0f) * m_fTrackingCatchupEndFrac;

					if (lastX > m_centreX + fCatchupEndOffsetX)
					{
						m_bTrackingEndFrame = true;
						m_centreTrackOffsetX = fCatchupEndOffsetX;
						m_bTrackingCatchingUp = false;
					}
				}

				if (m_bTrackingEndFrame)
				{
					m_centreX = lastX - m_centreTrackOffsetX;
				}
			}
		}

		protected override void glDraw()
		{
			UpdateTracking();
			PrepareViewport();
			DrawAxes();

			foreach (LogView logView in m_logViews)
			{
				if (!logView.m_displayedOnGraph || logView.m_maxValidIdx < 0)
				{
					continue;
				}

				lock (logView.m_logData)
				{
					DrawElementBars(logView);
					DrawFrameRecordLines(logView);
					DrawUserMarkerLines(logView);
					DrawZoneHighlighters(logView);
				}
			}

			DrawPlayHead();
			DrawMarkers();

			if (m_zoomBoxing)
			{
				DrawZoomBox();
			}

			DrawSelection();

			// draw this last as it just splats borders on top
			DrawTargetLines();

			OpenGL.glFlush();
		}

		float LeftUnits
		{
			get { return m_centreX - m_sizeX / 2.0f; }
		}

		float RightUnits
		{
			get { return m_centreX + m_sizeX / 2.0f; }
		}

		float TopUnits
		{
			get { return m_centreY - m_sizeY / 2.0f; }
		}

		float BottomUnits
		{
			get { return m_centreY + m_sizeY / 2.0f; }
		}

		public int LeftIndex(LogView logView)
		{
			float startValue = LeftUnits;

			if (m_againstFrameTime)
			{
				LogData logData = logView.m_logData;
				FrameRecord firstFullFR = logData.FrameRecords.Find(fr => fr.FrameTimeInS > startValue);
				return firstFullFR != null ? logData.GetDisplayIndex(firstFullFR.Index) - 1 : 0;
			}
			else
			{
				return (int)startValue;
			}
		}

		public int RightIndex(LogView logView)
		{
			float endValue = (float)Math.Ceiling(RightUnits);

			if (m_againstFrameTime)
			{
				LogData logData = logView.m_logData;
				FrameRecord oneBeyondEndFR = logData.FrameRecords.Find(fr => fr.FrameTimeInS > endValue);
				return oneBeyondEndFR != null ? logData.GetDisplayIndex(oneBeyondEndFR.Index) - 1 : logView.m_maxValidIdx;
			}
			else
			{
				return (int)endValue - 1;
			}
		}

		float PixelsToUnitsX(float numPixels)
		{
			return numPixels * (m_sizeX / this.Width);
		}

		float PixelsToUnitsY(float numPixels)
		{
			return numPixels * (m_sizeY / this.Height);
		}

		float UnitsToPixelsX(float x)
		{
			return (x * this.Width) / m_sizeX;
		}

		float UnitsToPixelsY(float y)
		{
			return (y * this.Height) / m_sizeY;
		}

		float WindowXToUnitsX(int x)
		{
			float normX = (float)x / this.Width;
			return m_centreX + (normX - 0.5f) * m_sizeX;
		}

		float WindowYToUnitsY(int y)
		{
			float normY = 1.0f - (float)y / this.Height;
			return m_centreY + (normY - 0.5f) * m_sizeY;
		}

		public int UnitsXToWindowX(float x)
		{
			float normX = ((m_centreX - x) / m_sizeX) + 0.5f;
			return (int)((1.0f - normX) * this.Width);
		}

		public int UnitsYToWindowY(float y)
		{
			float normY = ((y - m_centreY) / m_sizeY) + 0.5f;
			return (int)((1.0f - normY) * this.Height);
		}

		public float UnitsToSeconds(float x)
		{
			if (!m_againstFrameTime)
			{
				LogData logData = m_logViews[0].m_logData;		// this function only makes sense for a given logview
				x = Math.Max(x, 0.0f);
				x = Math.Min(x, logData.FrameRecords.Count - 1);

				int frameIdx = (int)x;
				float frac = x - (float)frameIdx;

				FrameRecord fr = logData.FrameRecords[frameIdx];
				x = fr.FrameTimeInS + (fr.FrameMidTimeInS - fr.FrameTimeInS) * 2.0f * frac;
			}

			return x;
		}

		public float SecondsToUnits(float x)
		{
			if (!m_againstFrameTime)
			{
				LogData logData = m_logViews[0].m_logData;		// this function only makes sense for a given logview

				if (logData.FrameRecords.Count > 0)
				{
					int frameIdx = logData.FrameRecords.MappedBinarySearchIndex(x, (fr) => fr.FrameTimeInS, (a, b) => a < b);

					FrameRecord frec = logData.FrameRecords[frameIdx];
					float frameLen = (frec.FrameMidTimeInS - frec.FrameTimeInS) * 2.0f;

					if (frameLen < 1.0f)
					{
						frameLen = 1.0f;
					}

					float frameFrac = (x - frec.FrameTimeInS) / frameLen;

					x = frameIdx + frameFrac;
				}
				else
				{
					x = 0.0f;
				}
			}

			return x;
		}

		public void FitViewToFrameRecords()
		{
			float border = 0.05f;
			float minXSize = m_againstFrameTime ? 1.0f : 100.0f;

			m_sizeX = Math.Max(minXSize, (MaxX() - MinX())) * (1.0f + border);
			m_sizeY = 100.0f;
			m_centreX = Math.Max(minXSize, MinX() + MaxX()) / 2.0f;
			m_centreY = m_sizeY * (0.5f - border);
		}

		public void ZoomViewToXValue(float x)
		{
			float centreX = x;
			float centreY = 45.0f;

			// the idea is that the first time this is called it'll centre the view on the frame record and the second time it'll scale
			if (m_centreX != centreX || m_centreY != centreY)
			{
				m_centreX = centreX;
				m_centreY = centreY;
			}
			else
			{
				m_sizeX = m_againstFrameTime ? 1000.0f : 30.0f;
				m_sizeY = 100.0f;
			}
		}

		public void AdjustCentreX(float value)
		{
			m_centreX += value;
		}

		void DrawFixedSizeCross(float x, float y, float fractionOfWindow)
		{
			float xSize = 0.5f * fractionOfWindow * m_sizeX;
			float ySize = 0.5f * fractionOfWindow * m_sizeY * ((float)this.Width / this.Height);

			OpenGL.glBegin(OpenGL.GL_LINES);
			OpenGL.glVertex2f(x - xSize, y - ySize);
			OpenGL.glVertex2f(x + xSize, y + ySize);

			OpenGL.glVertex2f(x - xSize, y + ySize);
			OpenGL.glVertex2f(x + xSize, y - ySize);
			OpenGL.glEnd();
		}

		void DrawZoomBox()
		{
			OpenGL.glColor4f(0.0f, 0.0f, 1.0f, 1.0f);

			OpenGL.glBegin(OpenGL.GL_LINE_LOOP);
			OpenGL.glVertex3f(m_zoomBoxStartPosX, m_zoomBoxStartPosY, 1.0f);
			OpenGL.glVertex3f(m_zoomBoxEndPosX, m_zoomBoxStartPosY, 1.0f);
			OpenGL.glVertex3f(m_zoomBoxEndPosX, m_zoomBoxEndPosY, 1.0f);
			OpenGL.glVertex3f(m_zoomBoxStartPosX, m_zoomBoxEndPosY, 1.0f);
			OpenGL.glEnd();
		}

		void DrawSelection()
		{
			if (m_selectAnchorSeconds != m_selectEndSeconds)
			{
				float minUnits = SecondsToUnits(SelectionStartInSeconds);
				float maxUnits = SecondsToUnits(SelectionEndInSeconds);

				float fTop = WindowYToUnitsY(0);
				float fBottom = WindowYToUnitsY(ClientSize.Height);

				OpenGL.glBegin(OpenGL.GL_QUADS);

				OpenGL.glColor4f(154.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f, 0.5f);

				OpenGL.glVertex2f(minUnits, fTop);
				OpenGL.glVertex2f(minUnits, fBottom);
				OpenGL.glVertex2f(maxUnits, fBottom);
				OpenGL.glVertex2f(maxUnits, fTop);

				OpenGL.glEnd();

				OpenGL.glBegin(OpenGL.GL_LINES);

				OpenGL.glColor4f(154.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f, 1.0f);

				OpenGL.glVertex2f(minUnits, fTop);
				OpenGL.glVertex2f(minUnits, fBottom);
				OpenGL.glVertex2f(maxUnits, fTop);
				OpenGL.glVertex2f(maxUnits, fBottom);

				OpenGL.glEnd();
			}
		}
	}
}
