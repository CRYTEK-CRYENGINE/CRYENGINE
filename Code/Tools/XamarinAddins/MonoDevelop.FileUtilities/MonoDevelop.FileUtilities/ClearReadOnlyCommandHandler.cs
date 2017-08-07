using System.IO;
using MonoDevelop.Components.Commands;
using MonoDevelop.Core;
using MonoDevelop.Ide;

namespace MonoDevelop.FileUtilities
{
	/// <summary>
	/// Clear read only command handler.
	/// </summary>
	public class ClearReadOnlyCommandHandler : CommandHandler
	{
		/// <summary>
		/// Get current active file document
		/// </summary>
		/// <returns>The active document.</returns>
		private Ide.Gui.Document GetActiveDocument()
		{
			var document = IdeApp.Workbench.ActiveDocument;
			if (document == null || !document.IsFile)
			{
				return null;
			}

			return document;
		}

		/// <summary>
		/// Checks if file can be unlocked (is read only).
		/// </summary>
		/// <returns><c>true</c>, if file can be unlocked, <c>false</c> otherwise.</returns>
		/// <param name="file">File.</param>
		private bool CanBeUnlocked(FilePath file)
		{
			if (file == null)
			{
				return false;
			}

			var attributes = File.GetAttributes(file);

			return (attributes & FileAttributes.ReadOnly) != 0;
		}

		/// <summary>
		/// Update command
		/// </summary>
		/// <param name="info">Info.</param>
		protected override void Update(CommandInfo info)
		{
			base.Update(info);

			var document = GetActiveDocument();

			// Only allow read only files to be unlocked
			info.Enabled = document != null && CanBeUnlocked(document.FileName);
		}

		/// <summary>
		/// Run this command
		/// </summary>
		protected override void Run()
		{
			base.Run();

			var document = GetActiveDocument();
			if (document == null)
			{
				return;
			}

			try
			{
				var file = document.FileName;
				var project = document.Project;

				var attributes = File.GetAttributes(file);

				File.SetAttributes(file, attributes & ~FileAttributes.ReadOnly);

				document.IsDirty = false;
				document.Close();

				IdeApp.Workbench.OpenDocument(file, project, Ide.Gui.OpenDocumentOptions.BringToFront).Wait();
			}
			catch
			{
				throw new UserException(string.Format("Unable to unlock file {0}.", document.FileName));
			}
		}
	}
}