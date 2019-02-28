// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using System;
using System.IO;
using System.Runtime.InteropServices;
using CryEngine.Common;
using CryEngine.Core.Util;

namespace CryEngine.FileSystem
{
	/// <summary>
	/// The File class is used to easily access files on the disk or in .pak files.
	/// </summary>
	public static class File
	{
		/// <summary>
		/// Checks if the file at the specified path exists on disk or in a .pak file.
		/// </summary>
		/// <returns><c>true</c>, if a file exists, <c>false</c> otherwise.</returns>
		/// <param name="path">Path to the file.</param>
		public static bool Exists(string path)
		{
			if(path == null)
			{
				throw new ArgumentNullException(nameof(path));
			}

			return Global.gEnv.pCryPak.IsFileExist(path, ICryPak.EFileSearchLocation.eFileLocation_Any);
		}

		/// <summary>
		/// Reads all the bytes from start to finish of the specified file on the disk or in a .pak file.
		/// Throws a <c>FileNotFoundException</c> if the file doesn't exist.
		/// </summary>
		/// <returns>All the bytes of the file in a byte array.</returns>
		/// <param name="path">Path to the file.</param>
		public static byte[] ReadAllBytes(string path)
		{
			if(path == null)
			{
				throw new ArgumentNullException(nameof(path));
			}

			var handle = Open(path, FileMode.Binary);
			if(handle == null)
			{
				throw new FileNotFoundException($"File {path} not found.", path);
			}

			var size = GetSize(handle);
			var buffer = new byte[size];

			if(size > 0)
			{
				var readBytes = Read(handle, buffer, size);
				if(readBytes != size)
				{
					throw new InvalidOperationException($"File {path} cannot be fully read.");
				}
			}

			Close(handle);

			return buffer;
		}

		/// <summary>
		/// Closes the filehandle <paramref name="handle"/>.
		/// </summary>
		/// <param name="handle">The file handle.</param>
		public static void Close(CryPakFile handle)
		{
			if(handle == null)
			{
				throw new ArgumentNullException(nameof(handle));
			}

			Global.gEnv.pCryPak.FClose(handle.NativeHandle);
		}

		/// <summary>
		/// Read raw data into <paramref name="buffer"/> from a file on the disk or in a .pak file, without endian conversion.
		/// </summary>
		/// <returns>The amount of bytes read.</returns>
		/// <param name="handle">Handle to the file.</param>
		/// <param name="buffer">Buffer that contains the read data.</param>
		/// <param name="length">Length of the data to be read.</param>
		public static uint Read(CryPakFile handle, byte[] buffer, uint length)
		{
			if(handle == null)
			{
				throw new ArgumentNullException(nameof(handle));
			}

			if(buffer == null)
			{
				throw new ArgumentNullException(nameof(buffer));
			}

			if(length == 0)
			{
				throw new ArgumentOutOfRangeException(nameof(length), "Buffer length has to be greater than 0.");
			}

			using(var pinner = new AutoPinner(buffer))
			{
				var cryPakHandle = GetCryPakHandle();
				var dataHandle = new HandleRef(pinner.Handle, pinner.Handle.AddrOfPinnedObject());
				var fileHandle = GetFileHandle(handle.NativeHandle);

				var bytesRead = GlobalPINVOKE.ICryPak_FReadRaw(cryPakHandle, dataHandle, sizeof(byte), length, fileHandle);

				return bytesRead;
			}
		}

		/// <summary>
		/// Open the file at the specified path. 
		/// Use <paramref name="mode"/> to specify how to open the file.
		/// </summary>
		/// <returns>The handle to the file.</returns>
		/// <param name="path">Path to the specified file.</param>
		/// <param name="mode">The FileMode to use to open the file.</param>
		public static CryPakFile Open(string path, FileMode mode)
		{
			string sMode = "";
			switch(mode)
			{
				case FileMode.Binary:
					sMode = "rb";
					break;

				case FileMode.PlainText:
					sMode = "rt";
					break;

				case FileMode.DirectAccess:
					sMode = "rbx";
					break;
			}

			var fileHandle = Global.gEnv.pCryPak.FOpen(path, sMode);

			return fileHandle != null ? new CryPakFile(fileHandle) : null;
		}

		/// <summary>
		/// Get the file size of file belonging to <paramref name="handle"/>.
		/// </summary>
		/// <returns>The file size.</returns>
		/// <param name="handle">Filehandle of the file.</param>
		public static uint GetSize(CryPakFile handle)
		{
			if(handle == null)
			{
				return 0;
			}

			return Global.gEnv.pCryPak.FGetSize(handle.NativeHandle);
		}

		private static HandleRef GetCryPakHandle()
		{
			return ICryPak.getCPtr(Global.gEnv.pCryPak);
		}

		private static HandleRef GetFileHandle(_CrySystem_cs_SWIGTYPE_p_FILE handle)
		{
			return _CrySystem_cs_SWIGTYPE_p_FILE.getCPtr(handle);
		}
	}
}
