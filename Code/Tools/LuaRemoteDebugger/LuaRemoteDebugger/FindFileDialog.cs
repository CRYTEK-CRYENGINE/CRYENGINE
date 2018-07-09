// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace LuaRemoteDebugger
{
	public partial class FindFileDialog : Form
	{
		struct FileEntry
		{
			public string Filename;
			public string Path;
		}

		List<FileEntry> allFiles = new List<FileEntry>();
		List<FileEntry> filteredFiles = new List<FileEntry>();

		public string SelectedFile
		{ 
			get
			{
				if (listViewFiles.SelectedItems.Count > 0)
				{
					return listViewFiles.SelectedItems[0].Tag as string;
				}
				return string.Empty;
			}
		}

		public FindFileDialog()
		{
			InitializeComponent();
		}

		public void SetFiles(List<string> filenames)
		{
			allFiles.Clear();
			foreach (string fullpath in filenames)
			{
				FileEntry entry;
				entry.Filename = Path.GetFileName(fullpath);
				entry.Path = fullpath;
				allFiles.Add(entry);
				filteredFiles.Add(entry);
			}
			PopulateListView();
		}

		private void PopulateListView()
		{
			listViewFiles.Items.Clear();
			foreach (FileEntry entry in filteredFiles)
			{
				ListViewItem item = listViewFiles.Items.Add(entry.Filename);
				item.SubItems.Add(entry.Path);
				item.Tag = entry.Path;
			}
			if (listViewFiles.Items.Count > 0)
			{
				listViewFiles.SelectedIndices.Add(0);
			}
		}

		private void textBoxFilter_TextChanged(object sender, EventArgs e)
		{
			string lowerCaseText = textBoxFilter.Text.ToLower();
			filteredFiles.Clear();
			foreach (FileEntry entry in allFiles)
			{
				if (entry.Filename.ToLower().Contains(lowerCaseText))
				{
					filteredFiles.Add(entry);
				}
			}
			PopulateListView();
		}

		private void listViewFiles_DoubleClick(object sender, EventArgs e)
		{
			if (listViewFiles.SelectedItems.Count > 0)
			{
				this.DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void textBoxFilter_KeyDown(object sender, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.Down || e.KeyCode == Keys.Up)
			{
				if (listViewFiles.SelectedIndices.Count > 0)
				{
					int selectedIndex = listViewFiles.SelectedIndices[0];
					int newIndex = -1;
					if (e.KeyCode == Keys.Down)
					{
						newIndex = selectedIndex + 1;
					}
					else if (e.KeyCode == Keys.Up)
					{
						newIndex = selectedIndex - 1;
					}
					if (newIndex >= 0 && newIndex < listViewFiles.Items.Count)
					{
						listViewFiles.SelectedIndices.Clear();
						listViewFiles.SelectedIndices.Add(newIndex);
					}
				}
				else if (listViewFiles.Items.Count > 0)
				{
					listViewFiles.SelectedIndices.Add(0);
				}
			}
		}
	}
}
