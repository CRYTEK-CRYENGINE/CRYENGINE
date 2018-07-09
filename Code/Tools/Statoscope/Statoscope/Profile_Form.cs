// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using System.Windows.Forms;

namespace Statoscope
{
	partial class StatoscopeForm
	{
		private void openToolStripMenuItem_Click(object sender, EventArgs e)
		{
			OpenFileDialog fd = new OpenFileDialog();
			fd.Filter = "XML files|*.xml|All files|*.*";

			if (fd.ShowDialog() == DialogResult.OK)
			{
				LogControl logControl = GetActiveLogControl();
				if (logControl != null)
				{
					if (logControl.LoadProfile(fd.FileName))
						AddProfileMenuItem(new FileInfo(fd.FileName));
				}
			}
		}

		private void saveToolStripMenuItem_Click(object sender, EventArgs e)
		{
			SaveFileDialog fd = new SaveFileDialog();
			fd.Filter = "XML files|*.xml|All files|*.*";

			if (fd.ShowDialog() == DialogResult.OK)
			{
				LogControl logControl = GetActiveLogControl();
				if (logControl != null)
				{
					logControl.SaveProfile(fd.FileName);
					AddProfileMenuItem(new FileInfo(fd.FileName));
				}
			}
		}

		void ProfileMenuItemClick(object sender, EventArgs e)
		{
			ToolStripMenuItem tsmi = (ToolStripMenuItem)sender;
			FileInfo fi = (FileInfo)tsmi.Tag;
			LogControl logControl = GetActiveLogControl();
			if (logControl != null)
			{
				logControl.LoadProfile(fi.FullName);
			}
		}

		string ProfileMenuText(FileInfo fi)
		{
			// remove the extension
			return Path.ChangeExtension(fi.Name, null);
		}

		void AddProfileMenuItem(FileInfo fi)
		{
			string fiMenuText = ProfileMenuText(fi);

			foreach (ToolStripItem tsi in profilesToolStripMenuItem.DropDownItems)
			{
				if (tsi.Text == fiMenuText)
					return;
			}

			ToolStripMenuItem tsmi = new ToolStripMenuItem(fiMenuText, null, ProfileMenuItemClick);
			tsmi.Tag = fi;
			tsmi.ToolTipText = fi.FullName;
			profilesToolStripMenuItem.DropDownItems.Add(tsmi);
		}

		void PopulateProfileMenu()
		{
			FileInfo exeFileInfo = new FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location);
			foreach (FileInfo fi in exeFileInfo.Directory.GetFiles("*.xml"))
			{
				// GetFiles wildcard matching has a peculiar behaviour for 3 letter extensions that ends up including .xml~ files
				if (fi.Extension == ".xml")
				{
					try
					{
						// check that it's a valid profile xml
						if (SerializeState.LoadFromFile(fi.FullName) != null)
							AddProfileMenuItem(fi);
					}
					catch (Exception)
					{
					}
				}
			}
		}
	}
}