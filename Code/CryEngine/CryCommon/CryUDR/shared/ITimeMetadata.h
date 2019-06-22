// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry 
{
	namespace UDR
	{

		struct ITimeMetadata
		{
			virtual uint32     GetFrameID() const = 0;
			virtual CTimeValue GetTimestamp() const = 0;

			// Set/Update functions?

			//! Checks if the metadata object is valid. The metadata is valid if it was successfully initialized.
			//! \return True if the metadata object is valid. False otherwise.
			virtual bool       IsValid() const = 0;

		protected:
			~ITimeMetadata() {}
		};

	}
}
