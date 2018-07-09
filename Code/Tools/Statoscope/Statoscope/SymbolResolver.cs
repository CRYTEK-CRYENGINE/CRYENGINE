// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Dia2Lib;
using System.Runtime.InteropServices;

namespace Statoscope
{
	class SymbolResolver : IDisposable
	{
		IDiaSession m_session;
		UInt32 m_size;

		public SymbolResolver(IDiaSession session, UInt32 size)
		{
			m_session = session;
			m_size = size;
		}

		public bool AddressIsInRange(UInt64 address)
		{
			return (m_session.loadAddress <= address) && (address < m_session.loadAddress + m_size);
		}

        public string GetSymbolNameFromAddress(UInt64 address)
		{
			if (AddressIsInRange(address))
			{
				IDiaSymbol diaSymbol;
				m_session.findSymbolByVA(address, SymTagEnum.SymTagFunction, out diaSymbol);

				IDiaEnumLineNumbers lineNumbers;
				m_session.findLinesByVA(address, 4, out lineNumbers);

				IDiaLineNumber lineNum;
				uint celt;
				lineNumbers.Next(1, out lineNum, out celt);

				if (celt == 1)
				{
					string baseFilename = lineNum.sourceFile.fileName.Substring(lineNum.sourceFile.fileName.LastIndexOf('\\') + 1);
					return diaSymbol.name + " (" + baseFilename + ":" + lineNum.lineNumber + ")";
				}
				else
				{
					return diaSymbol.name;
				}
			}
			else
			{
				return "<address out of range>";
			}
		}

    #region IDisposable Members

    public void Dispose()
    {
			if (m_session != null)
			{
	      Marshal.ReleaseComObject(m_session);
				m_session = null;
			}
    }

    #endregion
  }
}
