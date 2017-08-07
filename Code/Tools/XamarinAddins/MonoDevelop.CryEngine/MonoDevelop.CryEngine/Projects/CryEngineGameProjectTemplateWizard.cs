using MonoDevelop.Ide.Templates;

namespace MonoDevelop.CryEngine.Projects
{
	public class CryEngineGameProjectTemplateWizard : TemplateWizard
	{
		#region Properties

		public override string Id { get { return "CryEngineGameWizard"; } }

		#endregion

		#region Methods

		public override WizardPage GetPage(int pageNumber)
		{
			return new CryEngineGameProjectTemplateWizardPage(this);
		}

		#endregion
	}
}