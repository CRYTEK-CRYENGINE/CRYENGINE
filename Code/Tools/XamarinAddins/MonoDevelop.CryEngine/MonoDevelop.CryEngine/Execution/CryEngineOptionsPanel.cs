using System;

using MonoDevelop.CryEngine.Projects;

using MonoDevelop.Ide.Gui.Dialogs;

namespace MonoDevelop.CryEngine.Execution
{
	public class CryEngineOptionsPanel : ItemOptionsPanel
	{
		#region Fields

		private CryEngineOptionsPanelWidget _widget;

		#endregion

		#region Methods

		private CryEngineGameProjectExtension GetProjectExtension()
		{
			var ceGameProjectExtension = ConfiguredProject.GetFlavor<CryEngineGameProjectExtension>();
			if (ceGameProjectExtension == null)
			{
				throw new InvalidOperationException("Invalid project type passed");
			}

			return ceGameProjectExtension;
		}

		public override Components.Control CreatePanelWidget()
		{
			var ceGameProjectExtension = GetProjectExtension();

			_widget = new CryEngineOptionsPanelWidget(ceGameProjectExtension);

			return _widget;
		}

		public override void ApplyChanges()
		{
			var ceGameProjectExtension = GetProjectExtension();

			_widget.Store(ceGameProjectExtension);
		}

		#endregion
	}
}