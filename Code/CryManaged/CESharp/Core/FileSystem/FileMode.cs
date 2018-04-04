namespace CryEngine.FileSystem
{
	/// <summary>
	/// The various modes that can be used to open files.
	/// </summary>
	public enum FileMode
	{
		// TODO Add cases for w, a, +, c, n, S, R, T, D

		/// <summary>
		/// Opens the file as plain text.
		/// </summary>
		PlainText,
		/// <summary>
		/// Opens the file as a binary file.
		/// </summary>
		Binary,
		/// <summary>
		/// Opens the file in direct access mode as a binary file. 
		/// This will, if possible, prevent the file from being read from memory.
		/// </summary>
		DirectAccess
	}
}
