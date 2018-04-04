using System.Runtime.CompilerServices;

namespace CryEngine.FileSystem
{
	//Use an alias so we don't run into conflicts with the various names.
	using SystemPath = System.IO.Path;

	/// <summary>
	/// The Path class wraps the System.IO.Path class to avoid conflicts by having to use the CryEngine.FileSystem and System.IO at the same time.
	/// </summary>
	public static class Path
	{
		#region properties

		/// <summary>
		/// Provides a platform-specific alternate character used to separate directory levels in a path string that reflects a hierarchical file system organization.
		/// </summary>
		/// <value>The alternate directory separator char.</value>
		public static char AltDirectorySeparatorChar => SystemPath.AltDirectorySeparatorChar;

		/// <summary>
		/// Provides a platform-specific character used to separate directory levels in a path string that reflects a hierarchical file system organization.
		/// </summary>
		/// <value>The directory separator char.</value>
		public static char DirectorySeparatorChar => SystemPath.DirectorySeparatorChar;

		/// <summary>
		/// A platform-specific separator character used to separate path strings in environment variables.
		/// </summary>
		/// <value>The path separator.</value>
		public static char PathSeparator => SystemPath.PathSeparator;

		/// <summary>
		/// Provides a platform-specific volume separator character.
		/// </summary>
		/// <value>The volume separator char.</value>
		public static char VolumeSeparatorChar => SystemPath.VolumeSeparatorChar;

		#endregion //properties

		#region methods

		/// <summary>
		/// Changes the extension of a path string.
		/// </summary>
		/// <returns>The modified path information.</returns>
		/// <param name="path">The path information to modify. The path cannot contain any of the characters defined in GetInvalidPathChars.</param>
		/// <param name="extension">The new extension (with or without a leading period). Specify <c>null</c> to remove an existing extension from <paramref name="path"/>.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string ChangeExtension(string path, string extension)
		{
			return SystemPath.ChangeExtension(path, extension);
		}

		/// <summary>
		/// Combines two strings into a path.
		/// </summary>
		/// <returns>
		/// The combined paths. 
		/// If one of the specified paths is a zero-length string, this method returns the other path. 
		/// If <paramref name="path2"/> contains an absolute path, this method returns <paramref name="path2"/>.
		/// </returns>
		/// <param name="path1">The first path to combine.</param>
		/// <param name="path2">The second path to combine.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string Combine(string path1, string path2)
		{
			return SystemPath.Combine(path1, path2);
		}

		/// <summary>
		/// Combines three strings into a path.
		/// </summary>
		/// <returns>The combined paths.</returns>
		/// <param name="path1">The first path to combine.</param>
		/// <param name="path2">The second path to combine.</param>
		/// <param name="path3">The third path to combine.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string Combine(string path1, string path2, string path3)
		{
			return SystemPath.Combine(path1, path2, path3);
		}

		/// <summary>
		/// Combines three strings into a path.
		/// </summary>
		/// <returns>The combined paths.</returns>
		/// <param name="path1">The first path to combine.</param>
		/// <param name="path2">The second path to combine.</param>
		/// <param name="path3">The third path to combine.</param>
		/// <param name="path4">The fourth path to combine.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string Combine(string path1, string path2, string path3, string path4)
		{
			return SystemPath.Combine(path1, path2, path3, path4);
		}

		/// <summary>
		/// Combines an array of strings into a path.
		/// </summary>
		/// <returns>The combined paths.</returns>
		/// <param name="paths">An array of parts of the path.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string Combine(params string[] paths)
		{
			return SystemPath.Combine(paths);
		}

		/// <summary>
		/// Returns the directory information for the specified path string.
		/// </summary>
		/// <returns>The path of a file or directory.</returns>
		/// <param name="path">
		/// Directory information for <paramref name="path"/>, or <c>null</c> if <paramref name="path"/> denotes a root directory or is null. 
		/// Returns String.Empty if path does not contain directory information.
		/// </param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetDirectoryName(string path)
		{
			return SystemPath.GetDirectoryName(path);
		}

		/// <summary>
		/// Returns the extension of the specified path string.
		/// </summary>
		/// <returns>
		/// The extension of the specified <paramref name="path"/> (including the period "."), or <c>null</c>, or String.Empty. 
		/// If <paramref name="path"/> is <c>null</c>, GetExtension returns <c>null</c>. If path does not have extension information, GetExtension returns String.Empty.
		/// </returns>
		/// <param name="path">The path string from which to get the extension.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetExtension(string path)
		{
			return SystemPath.GetExtension(path);
		}

		/// <summary>
		/// Returns the file name and extension of the specified path string.
		/// </summary>
		/// <returns>
		/// The characters after the last directory character in <paramref name="path"/>. 
		/// If the last character of <paramref name="path"/> is a directory or volume separator character, this method returns String.Empty. 
		/// If <paramref name="path"/> is <c>null</c>, this method returns <c>null</c>.
		/// </returns>
		/// <param name="path">The path string from which to obtain the file name and extension.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetFileName(string path)
		{
			return SystemPath.GetFileName(path);
		}

		/// <summary>
		/// Returns the file name of the specified path string without the extension.
		/// </summary>
		/// <returns>The string returned by <see cref="GetFileName(string)"/>, minus the last period (.) and all characters following it.</returns>
		/// <param name="path">The path of the file.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetFileNameWithoutExtension(string path)
		{
			return SystemPath.GetFileNameWithoutExtension(path);
		}

		/// <summary>
		/// Returns the absolute path for the specified path string.
		/// </summary>
		/// <returns>The fully qualified location of path, such as "C:\MyFile.txt".</returns>
		/// <param name="path">The file or directory for which to obtain absolute path information.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetFullPath(string path)
		{
			return SystemPath.GetFullPath(path);
		}

		/// <summary>
		/// Gets an array containing the characters that are not allowed in file names.
		/// </summary>
		/// <returns>An array containing the characters that are not allowed in file names.</returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static char[] GetInvalidFileNameChars()
		{
			return SystemPath.GetInvalidFileNameChars();
		}

		/// <summary>
		/// Gets an array containing the characters that are not allowed in path names.
		/// </summary>
		/// <returns>An array containing the characters that are not allowed in path names.</returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static char[] GetInvalidPathChars()
		{
			return SystemPath.GetInvalidPathChars();
		}

		/// <summary>
		/// Gets the root directory information of the specified path.
		/// </summary>
		/// <returns>The root directory of path, such as "C:\", or <c>null</c> if <paramref name="path"/> is <c>null</c>, or an empty string if path does not contain root directory information.</returns>
		/// <param name="path">The path from which to obtain root directory information.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetPathRoot(string path)
		{
			return SystemPath.GetPathRoot(path);
		}

		/// <summary>
		/// Returns a random folder name or file name.
		/// </summary>
		/// <returns>A random folder name or file name.</returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetRandomFileName()
		{
			return SystemPath.GetRandomFileName();
		}

		/// <summary>
		/// Creates a uniquely named, zero-byte temporary file on disk and returns the full path of that file.
		/// </summary>
		/// <returns>The full path of the temporary file.</returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetTempFileName()
		{
			return SystemPath.GetTempFileName();
		}

		/// <summary>
		/// Returns the path of the current user's temporary folder.
		/// </summary>
		/// <returns>The path to the temporary folder, ending with a backslash.</returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static string GetTempPath()
		{
			return SystemPath.GetTempPath();
		}

		/// <summary>
		/// Determines whether a path includes a file name extension.
		/// </summary>
		/// <returns>
		/// <c>true</c> if the characters that follow the last directory separator (\\ or /) 
		/// or volume separator (:) in the <paramref name="path"/> include a period (.) followed by one or more characters; 
		/// otherwise, <c>false</c>.
		/// </returns>
		/// <param name="path">The path to search for an extension.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool HasExtension(string path)
		{
			return SystemPath.HasExtension(path);
		}

		/// <summary>
		/// Gets a value indicating whether the specified path string contains a root.
		/// </summary>
		/// <returns><c>true</c> if <paramref name="path"/> contains a root; otherwise, <c>false</c>.</returns>
		/// <param name="path">The path to test.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool IsPathRooted(string path)
		{
			return SystemPath.IsPathRooted(path);
		}

		#endregion //methods
	}
}
