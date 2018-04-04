using System;
using MonoDevelop.Ide.Templates;

namespace MonoDevelop.CryEngine.Projects
{
	public class CryEngineGameProjectTemplateWizardPage : WizardPage
	{
		#region Constants

		private const string CryBuild = "BuildLocation";
		private const string CryProject = "ProjectLocation";

		#endregion

		#region Fields

		private CryEngineGameProjectTemplateWizard _wizard;
		private CryEngineGameProjectTemplateWizardPageWidget _view;

		#endregion

		#region Properties

		public override string Title => "CryEngine Game Configuration";

		public string BuildLocation
		{
			get { return _wizard.Parameters[CryBuild]; }
			set { _wizard.Parameters[CryBuild] = value; }
		}

		public string ProjectLocation
		{
			get { return _wizard.Parameters[CryProject]; }
			set { _wizard.Parameters[CryProject] = value; }
		}

		#endregion

		#region Constructors

		public CryEngineGameProjectTemplateWizardPage(CryEngineGameProjectTemplateWizard wizard)
		{
			_wizard = wizard;
			CanMoveToNextPage = true;

			InitializeValues();
		}

		#endregion

		#region Methods

		private void InitializeValues()
		{
			InitializeDefault(CryBuild, String.Empty);
			InitializeDefault(CryProject, String.Empty);
		}

		void InitializeDefault(string name, string defaultVal)
		{
			if (string.IsNullOrEmpty(_wizard.Parameters[name]))
			{
				_wizard.Parameters[name] = defaultVal;
			}
		}

		protected override object CreateNativeWidget<T>()
		{
			if (_view == null)
			{
				_view = new CryEngineGameProjectTemplateWizardPageWidget(this);
			}

			return _view;
		}

		protected override void Dispose(bool disposing)
		{
			if (_view != null)
			{
				_view.Dispose();
				_view = null;
			}

			base.Dispose(disposing);
		}

		#endregion
	}
}