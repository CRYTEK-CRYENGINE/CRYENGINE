using System;
using System.Collections.Generic;
using System.IO;
using CryEngine.Common;

namespace CryEngine.FileSystem
{
	/// <summary>
	/// The Directory class is used to easily access folders on the disk or in .pak files.
	/// </summary>
	public static class Directory
	{
		/// <summary>
		/// Checks if the directory at the specified path exists on disk or in a .pak file.
		/// Empty directories are not included in .pak files and will always return false.
		/// </summary>
		/// <returns><c>true</c>, if the directory exists, <c>false</c> otherwise.</returns>
		/// <param name="path">Path to the directory.</param>
		public static bool Exists(string path)
		{
			if(path == null)
			{
				throw new ArgumentNullException(nameof(path));
			}

			if(Global.gEnv.pCryPak.IsFileExist(path, ICryPak.EFileSearchLocation.eFileLocation_Any))
			{
				return true;
			}

			//IsFileExists only detects directories outside of a pak-file, so we check again here to see if it's in the pak-file.
			var fd = Global.gEnv.pCryPak.FindAllocateData();
			var findIterator = Global.gEnv.pCryPak.FindFirst(path + "/*.*", fd, 0, true);
			var exists = findIterator != null && Global.gEnv.pCryPak.FindIsResultValid(findIterator);
			Global.gEnv.pCryPak.FindFreeData(fd);
			return exists;
		}

		/// <summary>
		/// Enumerates over all the files in the specified directory on the disk or in a .pak file.
		/// The search pattern can be used to filter for specific files.
		/// If the directory does not exists this will throw an FileNotFoundException.
		/// Please note that empty directories in a .pak file will also throw an FileNotFoundException.
		/// </summary>
		/// <returns>Enumerable of all the files in the directory.</returns>
		/// <param name="path">Path to the directory.</param>
		/// <param name="searchPattern">Search pattern to filter the files with.</param>
		public static IEnumerable<string> EnumerateFiles(string path, string searchPattern = "*.*")
		{
			if(path == null)
			{
				throw new ArgumentNullException(nameof(path));
			}

			if(!Exists(path))
			{
				throw new FileNotFoundException($"Folder {path} not found.", path);
			}

			var fd = Global.gEnv.pCryPak.FindAllocateData();

			var findIterator = Global.gEnv.pCryPak.FindFirst(path + "/" + searchPattern, fd, 0, true);
			if(findIterator != null && Global.gEnv.pCryPak.FindIsResultValid(findIterator))
			{
				yield return Global.gEnv.pCryPak.FindDataGetPath(fd);
			}
			else
			{
				Global.gEnv.pCryPak.FindFreeData(fd);

				yield break;
			}

			while(true)
			{
				var resultValid = Global.gEnv.pCryPak.FindNext(findIterator, fd) == 0;

				if(resultValid)
				{
					yield return Global.gEnv.pCryPak.FindDataGetPath(fd);
				}
				else
				{
					break;
				}
			}

			Global.gEnv.pCryPak.FindClose(findIterator);
			Global.gEnv.pCryPak.FindFreeData(fd);
		}
	}
}
