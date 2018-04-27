// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
  interface IIntervalSink
  {
    UInt32 ClassId { get; }
    Type PropertiesType { get; }

    void OnBegunInterval(UInt64 ctxId, Interval iv);
    void OnFinalisedInterval(UInt64 ctxId, Interval iv, bool isModification);

    void OnFrameRecord(FrameRecordValues values);
  }
}
