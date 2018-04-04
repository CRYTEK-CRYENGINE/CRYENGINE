using Gdk;
using Gtk;
using MonoDevelop.Components;
using MonoDevelop.Core;

namespace MonoDevelop.CryEngine.Projects
{
	public class CryEngineGameProjectTemplateWizardPageWidget : HBox
	{
		#region Fields

		private readonly CryEngineGameProjectTemplateWizardPage _page;

		private readonly FileEntry _buildLocation = new FileEntry { DisplayAsRelativePath = true };
		private readonly FileEntry _launcherLocation = new FileEntry() { DisplayAsRelativePath = true };

		#endregion

		#region Constructors
		public CryEngineGameProjectTemplateWizardPageWidget(CryEngineGameProjectTemplateWizardPage page)
		{
			_page = page;

			_launcherLocation.Path = _page.ProjectLocation;
			_buildLocation.Path = _page.BuildLocation;

			_launcherLocation.PathChanged += (sender, e) => _page.ProjectLocation = _launcherLocation.Path;
			_buildLocation.PathChanged += (sender, e) => _page.BuildLocation = _buildLocation.Path;

			Build();
		}
		#endregion

		#region Methods

		private void Build()
		{
			Spacing = 1;

			var box = new VBox { BorderWidth = 12, Spacing = 2 };
			var eb = new EventBox { Child = box };
			var infoEB = new EventBox { WidthRequest = 280 };
			PackStart(eb, true, true, 0);
			PackStart(infoEB, false, false, 0);

			box.PackStart(new Label { Markup = "<b>" + GettextCatalog.GetString("General Options") + "</b>", Xalign = 0 }, false, false, 0);

			var gpadLabel = new Label { WidthRequest = 10 };
			var buildLabel = new Label(GettextCatalog.GetString("Build Location:")) { Xalign = 0 };
			var launcherLabel = new Label(GettextCatalog.GetString("Project location:")) { Xalign = 0 };

			var generalTable = new Table(4, 3, false) { ColumnSpacing = 2, RowSpacing = 2 };
			generalTable.Attach(gpadLabel, 0, 1, 0, 1, AttachOptions.Fill, 0, 0, 0);
			generalTable.Attach(buildLabel, 1, 2, 0, 1, AttachOptions.Fill, 0, 0, 0);
			generalTable.Attach(_buildLocation, 2, 3, 0, 1, AttachOptions.Fill, 0, 0, 0);
			generalTable.Attach(launcherLabel, 1, 2, 2, 3, AttachOptions.Fill, 0, 0, 0);
			generalTable.Attach(_launcherLocation, 2, 3, 2, 3, AttachOptions.Fill, 0, 0, 0);

			box.PackStart(generalTable, false, false, 0);

			ShowAll();
		}

		#endregion
	}
}
