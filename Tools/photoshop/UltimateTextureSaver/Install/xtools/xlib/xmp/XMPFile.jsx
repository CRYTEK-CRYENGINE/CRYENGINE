//
// XMPFile.jsx
//
// $Id: XMPFile.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//-include "xlib/XMPConst.jsx"
//-include "xlib/XMPMeta.jsx"
//-include "xlib/XMPPacketInfo.jsx"
//-include "xlib/XMPFileInfo.jsx"
//

XMPFile = function XMPFile(arg, format, openFlags) {
  var self = this;

  self.filePath = undefined;

  var src = arg;
  if (arg.constructor == String) {
    if (!arg.contains('<?xpacket') && !arg.contains('<x:xmpmeta')) {
      self.filePath = arg;
      self.file = new File(arg);
      src = self.file;
    }
  } else if (arg instanceof File) {
    self.file = arg;
    self.filePath = arg.toString();
  }

  if (format != undefined) {
    self.format = format;
  }
  if (openFlags != undefined) {
    self.openFlags = openFlags;
  }

  self.xmpData = new XMPData(src, format, openFlags);
};
XMPFile.version = "$Id: XMPFile.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $";

XMPFile.prototype.filePath = undefined;
XMPFile.prototype.format = XMPConst.FILE_UNKNOWN;
XMPFile.prototype.openFlags = XMPConst.OPEN_FOR_READ;

XMPFile.getFormatInfo = function(format) {
  return XMPConst.FILE_UNKNOWN;
};

XMPFile.prototype.canPutXMP = function(arg) {
  // arg is one of xmpObj/XMPMeta, xmpPacket/String,
  // xmpBuffer/Arrary of Number
  return false;
};
XMPFile.prototype.closeFile = function(closeFlags) {
  return undefined;
};
XMPFile.prototype.getXMP = function() {
  var self = this;

  if (!self.xmpData) {
    return null;
  }

  if (!self.xmpData.xmlstr) {
    return null;
  }

  return new XMPMeta(self.xmpData.xmlstr);
};
XMPFile.prototype.getPacketInfo = function() {
  return null;  // or XMPPacketInfo
};
XMPFile.prototype.getFileInfo = function() {
  return ;  // or XMPPacketInfo
};
XMPFile.prototype.putXMP = function(arg) {
  // arg is one of xmpObj/XMPMeta, xmpPacket/String, xmpBuffer/Arrary of Number
  return false;
};

XMPFileInfo = function XMPFileInfo() {
  var self = this;
  self.format = XMPConst.FILE_UNKNOWN;
  self.handlerFlags = 0;
  self.openFlags = XMPConst.OPEN_FOR_READ;
};

"XMPFile.jsx";
// EOF
