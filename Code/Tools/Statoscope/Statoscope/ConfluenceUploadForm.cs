// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using Statoscope.ConfluenceSOAP;

namespace Statoscope
{
	partial class ConfluenceUploadForm : Form
	{
		ConfluenceService m_confluenceService;
		LogData m_logData;
		Dictionary<string, RemoteSpaceSummary> m_spaces = new Dictionary<string, RemoteSpaceSummary>();	// indexed by space name
		Dictionary<string, RemotePageSummary> m_pages = new Dictionary<string, RemotePageSummary>();	// indexed by platformPage title
		string m_initialSpaceName;
		string m_initialRootPageName;
		string m_initialPlatformPageName;
		string m_initialBuildPageName;
		string m_logFilePath;

		public ConfluenceUploadForm(ConfluenceService confluenceService, LogData logData,
																string initialSpaceName, string initialRootPageName,
																string initialPlatformPageName, string initialBuildPageName, string logFilePath)
		{
			m_confluenceService = confluenceService;
			m_logData = logData;
			m_logFilePath = logFilePath; 
			m_initialSpaceName = initialSpaceName;
			m_initialRootPageName = initialRootPageName;
			m_initialPlatformPageName = initialPlatformPageName + " - FPS data";
			m_initialBuildPageName = initialBuildPageName + " - " + m_initialPlatformPageName;

			FormBorderStyle = FormBorderStyle.FixedDialog;
			InitializeComponent();

			foreach (FrameRecordRange frr in m_logData.LevelRanges)
			{
				LevelsCheckedListBox.Items.Add(frr.Name, CheckState.Checked);
			}

			SpaceComboBox.SelectedValueChanged += new EventHandler(SpaceComboBox_SelectedValueChanged);
			RootPageComboBox.SelectedValueChanged += new EventHandler(RootPageComboBox_SelectedValueChanged);
			PlatformComboBox.SelectedValueChanged += new EventHandler(PlatformComboBox_SelectedValueChanged);
			BuildComboBox.SelectedValueChanged += new EventHandler(BuildComboBox_SelectedValueChanged);

			SpaceComboBox.LostFocus += new EventHandler(SpaceComboBox_SelectedValueChanged);
			RootPageComboBox.LostFocus += new EventHandler(RootPageComboBox_SelectedValueChanged);
			PlatformComboBox.LostFocus += new EventHandler(PlatformComboBox_SelectedValueChanged);
			BuildComboBox.LostFocus += new EventHandler(BuildComboBox_SelectedValueChanged);

			SetSpaces();
		}

		void SpaceComboBox_SelectedValueChanged(object sender, EventArgs e)
		{
			string spaceName = SpaceComboBox.Text;
			RemoteSpaceSummary space = m_spaces.ContainsKey(spaceName) ? m_spaces[spaceName] : null;

			SpaceExistsLabel.Text = (space == null) ? "New" : "Exists";
			SetRootPages(space);
		}

		void RootPageComboBox_SelectedValueChanged(object sender, EventArgs e)
		{
			string rootPageName = RootPageComboBox.Text;
			RemotePageSummary rootPage = m_pages.ContainsKey(rootPageName) ? m_pages[rootPageName] : null;

			RootPageExistsLabel.Text = (rootPage == null) ? "New" : "Exists";
			SetPlatforms(rootPage);
		}

		void PlatformComboBox_SelectedValueChanged(object sender, EventArgs e)
		{
			string platformPageName = PlatformComboBox.Text;
			RemotePageSummary platformPage = m_pages.ContainsKey(platformPageName) ? m_pages[platformPageName] : null;

			PlatformPageExistsLabel.Text = (platformPage == null) ? "New" : "Exists";

			if (platformPage != null)
			{
				string rootPageName = RootPageComboBox.Text;
				RemotePageSummary rootPage = m_pages.ContainsKey(rootPageName) ? m_pages[rootPageName] : null;
				if ((rootPage != null) && (platformPage.parentId != rootPage.id))
				{
					PlatformPageExistsLabel.Text = "Root not parent!";
				}
			}

			SetBuilds(platformPage);
		}

		void BuildComboBox_SelectedValueChanged(object sender, EventArgs e)
		{
			string buildPageName = BuildComboBox.Text;
			RemotePageSummary buildPage = m_pages.ContainsKey(buildPageName) ? m_pages[buildPageName] : null;

			BuildPageExistsLabel.Text = (buildPage == null) ? "New" : "Exists";
			ReplaceRadioButton.Enabled = (buildPage != null);
			AppendRadioButton.Enabled = (buildPage != null);

			if (buildPage != null)
			{
				string platformPageName = PlatformComboBox.Text;
				RemotePageSummary platformPage = m_pages.ContainsKey(platformPageName) ? m_pages[platformPageName] : null;
				if ((platformPage != null) && (buildPage.parentId != platformPage.id))
				{
					BuildPageExistsLabel.Text = "Platform not parent!";
				}
			}
		}

		void SetSpaces()
		{
			m_spaces.Clear();
			SpaceComboBox.Items.Clear();
			SpaceComboBox.Items.Add(m_initialSpaceName);

			List<RemoteSpaceSummary> spaces = m_confluenceService.GetSpaces();
			foreach (RemoteSpaceSummary space in spaces)
			{
				m_spaces[space.name] = space;

				if (space.name != m_initialSpaceName)
				{
					SpaceComboBox.Items.Add(space.name);
				}
			}

			SpaceComboBox.SelectedItem = m_initialSpaceName;
		}

		void SetRootPages(RemoteSpaceSummary space)
		{
			m_pages.Clear();
			RootPageComboBox.Items.Clear();
			RootPageComboBox.Items.Add(m_initialRootPageName);

			if (space != null)
			{
				List<RemotePageSummary> pages = m_confluenceService.GetPages(space.key);
				foreach (RemotePageSummary page in pages)
				{
					m_pages[page.title] = page;

					if (page.title != m_initialRootPageName)
					{
						RootPageComboBox.Items.Add(page.title);
					}
				}
			}

			RootPageComboBox.SelectedItem = m_initialRootPageName;
		}

		void SetPlatforms(RemotePageSummary rootPage)
		{
			PlatformComboBox.Items.Clear();
			PlatformComboBox.Items.Add(m_initialPlatformPageName);

			if (rootPage != null)
			{
				foreach (KeyValuePair<string, RemotePageSummary> pagePair in m_pages)
				{
					RemotePageSummary page = pagePair.Value;

					if ((page.parentId == rootPage.id) && (page.title != m_initialPlatformPageName))
					{
						PlatformComboBox.Items.Add(page.title);
					}
				}
			}

			PlatformComboBox.SelectedItem = m_initialPlatformPageName;
		}

		void SetBuilds(RemotePageSummary platformPage)
		{
			BuildComboBox.Items.Clear();
			BuildComboBox.Items.Add(m_initialBuildPageName);

			if (platformPage != null)
			{
				foreach (KeyValuePair<string, RemotePageSummary> pagePair in m_pages)
				{
					RemotePageSummary page = pagePair.Value;

					if ((page.parentId == platformPage.id) && (page.title != m_initialBuildPageName))
					{
						BuildComboBox.Items.Add(page.title);
					}
				}
			}

			BuildComboBox.SelectedItem = m_initialBuildPageName;
		}

		private void UploadButton_Click(object sender, EventArgs e)
		{
			try
			{
				string spaceName = SpaceComboBox.Text;
				if (m_spaces.ContainsKey(spaceName))
				{
					spaceName = m_spaces[spaceName].key;
				}
				else
				{
					m_confluenceService.CreateSpace(spaceName);
				}

				string rootPageName = RootPageComboBox.Text;
				if (!m_pages.ContainsKey(rootPageName))
				{
					m_confluenceService.CreatePage(rootPageName, spaceName);
				}

				string platformPageName = PlatformComboBox.Text;
				if (!m_pages.ContainsKey(platformPageName))
				{
					m_confluenceService.CreatePage(platformPageName, rootPageName, spaceName);
				}

				string buildPageName = BuildComboBox.Text;
				if (!m_pages.ContainsKey(buildPageName))
				{
					m_confluenceService.CreatePage(buildPageName, platformPageName, spaceName);
				}

				string timeDateString = DateTime.Now.ToString("dd/MM/yy hh:mm:ss \\(\\U\\T\\Cz\\)");

				ConfluenceService.PageContentMetaData pageContentMD = new ConfluenceService.PageContentMetaData(
					timeDateString, m_logData.Name,
					m_logData.BuildInfo.PlatformString, m_logData.BuildInfo.BuildNumberString, "",
					m_confluenceService.m_username, BuildInfoTextBox.Text, AppendRadioButton.Checked);

				ConfluenceService.PageMetaData pageMetaData = new ConfluenceService.PageMetaData(spaceName, buildPageName, platformPageName, pageContentMD);

				if (IncludeOverallCheckBox.Checked)
				{
					m_confluenceService.UploadFPSDataPage(pageMetaData, m_logData, m_logFilePath);
				}

				foreach (FrameRecordRange frr in m_logData.LevelRanges)
				{
					if (LevelsCheckedListBox.CheckedItems.Contains(frr.Name))
					{
						LogData levelLogData = new LogData(m_logData, frr);
						levelLogData.CalculateBucketSets();

						pageContentMD.m_levelName = levelLogData.Name;
						ConfluenceService.PageMetaData levelPageMD = new ConfluenceService.PageMetaData(spaceName, levelLogData.Name + " - " + buildPageName, buildPageName, pageContentMD);

						m_confluenceService.UploadFPSDataPage(levelPageMD, levelLogData, m_logFilePath);
					}
				}

				Close();
			}
			catch (Exception ex)
			{
				MessageBox.Show("Something went wrong! :(\n\n" + ex.ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
			}
		}
	}
}
