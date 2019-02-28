// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Xml.Serialization;

namespace Statoscope
{
	partial class LogControl
	{
		Dictionary<string, float> m_displayScales = new Dictionary<string, float>();
		Dictionary<string, float> m_userDisplayScales = new Dictionary<string, float>(); // additional scaling by the user

		// ultimately these will be driven by a config
		void InitializeDisplayScales()
		{
			m_displayScales["/Graphics/numTris"] = 1.0f / 13750.0f;	// obscure scaling numbers are to separate target lines (from fps in this case)
			m_displayScales["/Graphics/numDrawCalls"] = 1.0f / 35.0f;
			m_displayScales["/Graphics/numShadowDrawCalls"] = 1.0f / 35.0f;
			m_displayScales["/Graphics/numGeneralDrawCalls"] = 1.0f / 35.0f;
			m_displayScales["/Graphics/numTransparentDrawCalls"] = 1.0f / 35.0f;
			m_displayScales["/Graphics/numTotalDrawCalls"] = 1.0f / 35.0f;
			m_displayScales["/Graphics/numTotalDrawCallsLM"] = 1.0f / 35.0f;
			m_displayScales["/Graphics/numDrawCallsRejectedByConditionalRendering"] = 1.0f / 35.0f;
			m_displayScales["/Graphics/numPostEffects"] = 10.0f;
			m_displayScales["/Graphics/numForwardLights"] = 10.0f;
			m_displayScales["/Graphics/numForwardShadowCastingLights"] = 10.0f;

			m_displayScales["/ArtProfile/numDrawcalls"] = 1.0f / 35.0f;

			m_displayScales["/Particles/numParticlesRendered"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesActive"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesAllocated"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesRequested"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numEmittersRendered"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numEmittersActive"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numEmittersAllocated"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesReiterated"] = 1.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesRejected"] = 1.0f / 1000.0f;

			m_displayScales["/Particles/numParticlesRenderedMA"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesActiveMA"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesAllocatedMA"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numEmittersRenderedMA"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numEmittersActiveMA"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numEmittersAllocatedMA"] = 16.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesReiteratedMA"] = 1.0f / 1000.0f;
			m_displayScales["/Particles/numParticlesRejectedMA"] = 1.0f / 1000.0f;

			m_displayScales["/Overview/numDrawCalls"] = 1.0f / 35.0f;
			m_displayScales["/Overview/FrameScaleFactor"] = 100.0f;
		}

		public void SetDisplayScale(string path, float scale)
		{
			m_userDisplayScales[path] = scale;
		}

		public float GetUserDisplayScale(string path)
		{
			if (m_userDisplayScales.ContainsKey(path))
			{
				return m_userDisplayScales[path];
			}
			else
			{
				return 1.0f;
			}
		}

		public float GetDisplayScale(string path)
		{
			float userScale = 1.0f;
			float displayScale = 1.0f;

			if (m_userDisplayScales.ContainsKey(path))
			{
				userScale = m_userDisplayScales[path];
			}

			if (m_displayScales.ContainsKey(path))
			{
				displayScale = m_displayScales[path];
			}

			if (path.StartsWith("/Network"))
			{
				// a temporary hack, much like the rest of the display scale stuff! (which ultimately will be replaced with cfgs)
				return 100.0f / 6000.0f;
			}
			else
			{
				return userScale * displayScale;
			}
		}

		Dictionary<string, TreeNode> m_pathsInIntervalTree = new Dictionary<string, TreeNode>();

		void PopulateIntervalTreeView(CheckboxTreeView treeView, IntervalTree tree)
		{
			lock (tree)
			{
				foreach (RailGroup rg in tree.Groups)
				{
					TreeNode node = null;

					string name = rg.Name;

					if (!m_pathsInIntervalTree.ContainsKey(name))
					{
						TreeNodeCollection nodes = treeView.Nodes;

						foreach (string location in name.PathLocations())
						{
							if (nodes.ContainsKey(location))
							{
								node = nodes[location];
							}
							else
							{
								node = new TreeNode(location);
								node.Name = location;
								node.Checked = true;
								node.Tag = rg;
								nodes.Add(node);
							}

							nodes = node.Nodes;
						}

						m_pathsInIntervalTree.Add(name, node);
					}
					else
					{
						node = m_pathsInIntervalTree[name];
					}

					foreach (int railId in rg.Rails)
					{
						string railName = tree.GetRailName(railId);
						TreeNode railNode = node;

						if (!m_pathsInIntervalTree.ContainsKey(railName))
						{
							TreeNodeCollection nodes = railNode.Nodes;

							foreach (string location in railName.PathLocations())
							{
								if (nodes.ContainsKey(location))
								{
									railNode = nodes[location];
								}
								else
								{
									railNode = new TreeNode(location);
									railNode.Name = location;
									railNode.Checked = true;
									railNode.Tag = new KeyValuePair<RailGroup, int>(rg, railId);
									nodes.Add(railNode);
								}

								nodes = railNode.Nodes;
							}

							m_pathsInIntervalTree.Add(railName, railNode);

							/*
							TreeNode leafNode = new TreeNode(location);
							leafNode.Name = location;
							leafNode.Checked = true;
							leafNode.Tag = new KeyValuePair<RailGroup, int>(rg, railId);
							node.Nodes.Add(leafNode);
							m_pathsInIntervalTree.Add(location, leafNode);
							 */
						}
					}
				}
			}
		}

		void CreateDisplayInfo(LogView logView, int fromIdx, int toIdx, LogView.CDIResult cdiRes)
		{
			InvalidateGraphControl();

			m_ordiTreeView.AddRDIs(cdiRes.NewORDIs);
			m_prdiTreeView.AddRDIs(cdiRes.NewPRDIs);
			m_urdiTreeView.AddRDIs(cdiRes.NewUMRDIs);

			PopulateIntervalTreeView(intervalTreeView, logView.m_baseLogData.IntervalTree);
		}

		public void SetUMTreeViewNodeColours()
		{
			m_urdiTreeView.SetNodeColours(m_urdiTree.GetEnumerator(ERDIEnumeration.OnlyLeafNodes));
		}

		void InitializeTargetLines()
		{
			m_trdiTreeView.AddRDIToTree(new FPSTargetLineRDI("/TargetLines/FPS", 60.0f, new RGB(1.0f, 0.0f, 0.0f)));
			m_trdiTreeView.AddRDIToTree(new FPSTargetLineRDI("/TargetLines/FPS", 30.0f, new RGB(0.0f, 1.0f, 0.0f)));
			m_trdiTreeView.AddRDIToTree(new FPSTargetLineRDI("/TargetLines/FPS", 25.0f, new RGB(0.0f, 0.0f, 1.0f)));
			m_trdiTreeView.AddRDIToTree(new FPSTargetLineRDI("/TargetLines/FPS", 20.0f, new RGB(0.9f, 0.9f, 0.0f)));
			m_trdiTreeView.AddRDIToTree(new FPSTargetLineRDI("/TargetLines/FPS", 15.0f, new RGB(1.0f, 0.0f, 1.0f)));
			m_trdiTreeView.AddRDIToTree(new FPSTargetLineRDI("/TargetLines/FPS", 10.0f, new RGB(0.0f, 1.0f, 1.0f)));
			m_trdiTreeView.AddRDIToTree(new FPSTargetLineRDI("/TargetLines/FPS",  5.0f, new RGB(0.0f, 0.0f, 0.0f)));

			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/User", "A", 0, new RGB(0.5f, 0.5f, 0.0f), TargetLineRDI.ELabelLocation.ELL_Right));
			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/FPS", "User", 0, new RGB(0.0f, 0.5f, 0.5f), TargetLineRDI.ELabelLocation.ELL_Left));

			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/Memory", "Main", 211, new RGB(1.0f, 0.0f, 0.0f), TargetLineRDI.ELabelLocation.ELL_Right));
			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/Memory", "RSX",  240, new RGB(0.0f, 0.0f, 1.0f), TargetLineRDI.ELabelLocation.ELL_Right));

			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/Triangles", "500k", 500000, new RGB(1.0f, 0.50f, 0.0f), GetDisplayScale("/Graphics/numTris"), TargetLineRDI.ELabelLocation.ELL_Right));
			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/Triangles", "750k", 750000, new RGB(0.5f, 0.25f, 0.0f), GetDisplayScale("/Graphics/numTris"), TargetLineRDI.ELabelLocation.ELL_Right));

			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/DrawCalls", "1800", 1800, new RGB(0.3f, 0.6f, 0.3f), GetDisplayScale("/Graphics/numDrawCalls"), TargetLineRDI.ELabelLocation.ELL_Right));
			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/DrawCalls", "2000", 2000, new RGB(0.3f, 0.0f, 0.3f), GetDisplayScale("/Graphics/numDrawCalls"), TargetLineRDI.ELabelLocation.ELL_Right));

			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/Network", "4370", 4370, new RGB(1.0f, 0.5f, 0.0f), GetDisplayScale("/Network"), TargetLineRDI.ELabelLocation.ELL_Left));
			m_trdiTreeView.AddRDIToTree(new TargetLineRDI("/TargetLines/Network", "8740", 8740, new RGB(1.0f, 0.0f, 0.0f), GetDisplayScale("/Network"), TargetLineRDI.ELabelLocation.ELL_Left));

			foreach (TreeNode node in m_trdiTreeView.TreeNodeHierarchy)
			{
				node.Checked = false;
			}
		}

		void AutoEnableTargetLines()
		{
			/*
			// ultimately this should be driven by the metadata from the engine side data groups
			foreach (LogView logView in m_logViews)
			{
				foreach (string itemName in logView.m_logData.m_itemMetaData.Keys)
				{
					if (itemName == "/frameLengthInMS")
						fpsTargetLineCheckBox.Checked = true;
					else if (itemName.StartsWith("/Memory"))
						memTargetLineCheckBox.Checked = true;
					else if (itemName.StartsWith("/Network"))
						networkTargetLineCheckBox.Checked = true;
				}
			}
			 * */
		}

		void InitializeZoneHighlighters()
		{
			m_zrdiTreeView.AddRDIToTree(new ZoneHighlighterRDI("", "ZoneHighlighters"));

			/*
			//			m_zoneHighlighterCfg.Add(new KeyValuePair<string, ZHCfgEntry>(
			//				"/frameLengthInMS", ordi => new StdThresholdZoneHighlighter(new RGB(1.0f, 0.0f, 0.0f), ordi, 1000.0f / 30.0f, -1.0f)));
			//			m_zoneHighlighterCfg.Add(new KeyValuePair<string, ZHCfgEntry>(
			//				"/frameLengthInMS", ordi => new StdThresholdZoneHighlighter(new RGB(1.0f, 1.0f, 0.0f), ordi, 1000.0f / 30.0f, 1000.0f / 28.0f)));

			m_zoneHighlighterInstances.Add(new UserMarkerZoneHighlighter(new RGB(0.0f, 0.0f, 1.0f),
				new UserMarker("/UserMarkers/LoadtimeFlashPlayback", "Start"),
				new UserMarker("/UserMarkers/LoadtimeFlashPlayback", "Stop")));

			m_zoneHighlighterInstances.Add(new UserMarkerZoneHighlighter(new RGB(0.0f, 1.0f, 1.0f),
				new RegexUserMarker("/UserMarkers/KillCam", "Start.*"),
				new RegexUserMarker("/UserMarkers/KillCam", "Stop.*")));

			m_zoneHighlighterInstances.Add(new UserMarkerZoneHighlighter(new RGB(0.0f, 1.0f, 1.0f),
				new RegexUserMarker("/UserMarkers/Telemetry", "sessionStart.*"),
				new RegexUserMarker("/UserMarkers/Overload", "OverloadSceneManager::Reset()")));
			 * */

			/*
			AddZoneHighlighterInstance(new UserMarkerZoneHighlighter("Kill cam", new RGB(0.0f, 1.0f, 1.0f),
				new RegexUserMarker("/UserMarkers/KillCam", "Start.*"),
				new RegexUserMarker("/UserMarkers/KillCam", "Stop.*")));
			 * */

			/*	old ones
			CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/frameLengthMAInMS", 1000.0f / 27.0f, 1000.0f / 25.0f), new RGB(1.0f, 1.0f, 0.0f));

			CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Threading/RTLoadInMSMA", 1000.0f / 25.0f, -1.0f), new RGB(1.0f, 0.0f, 0.0f));
			//CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Threading/RTLoadInMSMA", 1000.0f / 27.0f, 1000.0f / 25.0f), new RGB(1.0f, 1.0f, 0.0f));

			CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Graphics/GPUFrameLengthMAInMS", 1000.0f / 25.0f, -1.0f), new RGB(1.0f, 0.0f, 0.0f));
			//CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Graphics/GPUFrameLengthMAInMS", 1000.0f / 27.0f, 1000.0f / 25.0f), new RGB(1.0f, 1.0f, 0.0f));

			CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Graphics/numTris", 750000.0f, -1.0f), new RGB(1.0f, 0.0f, 0.0f));
			CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Graphics/numTris", 500000.0f, 750000.0f), new RGB(1.0f, 1.0f, 0.0f));

			CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Graphics/numTotalDrawCallsLM", 2000.0f, -1.0f), new RGB(1.0f, 0.0f, 0.0f));
			//CreateHighlightZoneDisplayList(new ThresholdZoneHighlighter("/Graphics/numTotalDrawCallsLM", 1800.0f, 22000.0f), new RGB(1.0f, 1.0f, 0.0f));

			CreateHighlightZoneDisplayList(new CSignedDifferenceThresholdZoneHighlighter("/Particles/numParticlesAllocated", "/Particles/numParticlesRequested", -1, 500), new RGB(1.0f, 0.0f, 0.0f));
			 * */
		}
	}
}
