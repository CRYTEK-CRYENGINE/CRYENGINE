using System;
using MonoDevelop.CryEngine.Projects;
using MonoDevelop.Ide.Gui.Components;
using MonoDevelop.Projects;

namespace MonoDevelop.CryEngine.ProjectPad
{
	public class CryEngineGameProjectNodeBuilderExtension : NodeBuilderExtension
	{
		#region Methods

		public override bool CanBuildNode(Type dataType)
		{
			return typeof(DotNetProject).IsAssignableFrom(dataType);
		}

		public override void BuildNode(ITreeBuilder treeBuilder, object dataObject, NodeInfo nodeInfo)
		{
			var dotNetProject = dataObject as DotNetProject;
			if (dotNetProject != null)
			{
				var ceGameProjectExtension = dotNetProject.GetFlavor<CryEngineGameProjectExtension>();
				if (ceGameProjectExtension != null)
				{
					nodeInfo.Label = $"{dotNetProject.Name} (CryEngine plugin)";
					nodeInfo.Icon = Context.GetIcon("md-platform-cry");
				}
			}

			base.BuildNode(treeBuilder, dataObject, nodeInfo);
		}

		public override Type CommandHandlerType
		{
			get
			{
				return typeof(CryEngineGameProjectNodeCommandHandler);
			}
		}

		#endregion
	}
}
