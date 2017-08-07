using System;
using MonoDevelop.CryEngine.Projects;
using Gtk;
using MonoDevelop.Components;
using MonoDevelop.Core;

namespace MonoDevelop.CryEngine.Execution
{
	public class CryEngineOptionsPanelWidget : VBox
	{
		#region Fields

		private readonly FileEntry _launcherPath = new FileEntry { DisplayAsRelativePath = true };
		private readonly FileEntry _projectPath = new FileEntry { DisplayAsRelativePath = true };
		private readonly Entry _commandArguments = new Entry();

		#endregion

		#region Constructors

		public CryEngineOptionsPanelWidget(CryEngineGameProjectExtension projectExtension)
		{
			if (projectExtension == null)
			{
				throw new ArgumentNullException(nameof(projectExtension));
			}

			var engineParameters = new CryEngineParameters(projectExtension.Project);

			_launcherPath.Path = engineParameters.LauncherPath;
			_projectPath.Path = engineParameters.ProjectPath;
			_commandArguments.Text = engineParameters.CommandArguments;

			Build();
		}

		#endregion

		#region Methods

		private void Build()
		{
			VBox vbContent = new VBox
			{
				Name = "content",
				Spacing = 6
			};

			{
				Label lblHeader = new Label
				{
					Name = "headerDebug",
					Xalign = 0F,
					LabelProp = GettextCatalog.GetString("<b>Engine settings</b>"),
					UseMarkup = true
				};

				vbContent.Add(lblHeader);
				Gtk.Box.BoxChild c1 = (Gtk.Box.BoxChild)vbContent[lblHeader];
				c1.Position = 0;
				c1.Expand = false;
				c1.Fill = false;

				HBox hbIdent = new HBox
				{
					Name = "identDebug"
				};
				Label lblBlank = new Label
				{
					Name = "blankDebug",
					WidthRequest = 18
				};

				hbIdent.Add(lblBlank);
				Gtk.Box.BoxChild c2 = (Gtk.Box.BoxChild)hbIdent[lblBlank];
				c2.Position = 0;
				c2.Expand = false;
				c2.Fill = false;
				VBox vbTable = new VBox
				{
					Name = "table_boxDebug",
					Spacing = 6
				};
				Table table = new Table(3, 2, false)
				{
					Name = "tableDebug",
					RowSpacing = 6,
					ColumnSpacing = 6
				};

				// debug command
				{
					Label lblLauncher = new Label
					{
						Name = "debugCommand",
						Xalign = 0F,
						LabelProp = GettextCatalog.GetString("Launcher path:"),
						UseUnderline = true
					};
					table.Add(lblLauncher);
					Gtk.Table.TableChild cLblLauncher = (Gtk.Table.TableChild)table[lblLauncher];
					cLblLauncher.XOptions = AttachOptions.Fill;
					cLblLauncher.YOptions = (AttachOptions)0;

					table.Add(_launcherPath);
					Gtk.Table.TableChild cLauncher = (Gtk.Table.TableChild)table[_launcherPath];
					cLauncher.LeftAttach = 1;
					cLauncher.RightAttach = 2;
					cLauncher.YOptions = (AttachOptions)0;
				}

				//debug command arguments 
				{
					Label lblProject = new Label
					{
						Name = "debugCommandArguments",
						Xalign = 0F,
						LabelProp = GettextCatalog.GetString("Project path:"),
						UseUnderline = true
					};
					table.Add(lblProject);
					Gtk.Table.TableChild c5 = (Gtk.Table.TableChild)table[lblProject];
					c5.TopAttach = 2;
					c5.BottomAttach = 3;
					c5.XOptions = AttachOptions.Fill;
					c5.YOptions = (AttachOptions)0;

					table.Add(_projectPath);
					Gtk.Table.TableChild c6 = (Gtk.Table.TableChild)table[_projectPath];
					c6.TopAttach = 2;
					c6.BottomAttach = 3;
					c6.LeftAttach = 1;
					c6.RightAttach = 2;
					c6.XOptions = (AttachOptions)4;
					c6.YOptions = (AttachOptions)4;
				}

				//debug working directory
				{
					Label lblArguments = new Label
					{
						Name = "debugWorkingDirectory",
						Xalign = 0F,
						LabelProp = GettextCatalog.GetString("Command arguments:"),
						UseUnderline = true
					};
					table.Add(lblArguments);
					Gtk.Table.TableChild c5 = (Gtk.Table.TableChild)table[lblArguments];
					c5.TopAttach = 3;
					c5.BottomAttach = 4;
					c5.XOptions = AttachOptions.Fill;
					c5.YOptions = (AttachOptions)0;

					table.Add(_commandArguments);
					Gtk.Table.TableChild c6 = (Gtk.Table.TableChild)table[_commandArguments];
					c6.TopAttach = 3;
					c6.BottomAttach = 4;
					c6.LeftAttach = 1;
					c6.RightAttach = 2;
					c6.XOptions = (AttachOptions)4;
					c6.YOptions = (AttachOptions)4;
				}

				vbTable.Add(table);
				Gtk.Box.BoxChild c7 = (Gtk.Box.BoxChild)vbTable[table];
				c7.Position = 0;
				c7.Expand = false;
				c7.Expand = false;
				hbIdent.Add(vbTable);
				Gtk.Box.BoxChild c8 = (Gtk.Box.BoxChild)hbIdent[vbTable];
				c8.Position = 1;
				vbContent.Add(hbIdent);
				Gtk.Box.BoxChild c9 = (Gtk.Box.BoxChild)vbContent[hbIdent];
				c9.Position = 1;
				Add(vbContent);
				Gtk.Box.BoxChild c10 = (Gtk.Box.BoxChild)this[vbContent];
				c10.Position = 0;
				c10.Expand = false;
				c10.Fill = false;
			}

			ShowAll();
		}

		public void Store(CryEngineGameProjectExtension projectExtension)
		{
			if (projectExtension == null)
			{
				return;
			}

			var engineParameters = new CryEngineParameters(projectExtension.Project)
			{
				LauncherPath = _launcherPath.Path,
				ProjectPath = _projectPath.Path,
				CommandArguments = _commandArguments.Text
			};

			engineParameters.Save(projectExtension.Project);

			projectExtension.ApplyParameters();
		}

		#endregion
	}
}
