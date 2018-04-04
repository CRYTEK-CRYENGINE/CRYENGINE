//
// XMPPacketInfo.jsx
//
// $Id: XMPPacketInfo.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

XMPPacketInfo = function XMPPacketInfo(arg) {
  var self = this;

//   0: UTF8
//   2: UTF-16, MSB-first (big-endian)
//   3: UTF-16, LSB-first (little-endian)
//   4: UTF 32, MSB-first (big-endian)
//   5: UTF 32, LSB-first (little-endian)
  self.charForm = 0;
  self.length  = 0;
  self.offset = 0;
  self.packet = '';
  self.padSize = 0;
  self.writeable = false;
};


"XMPPacketInfo.jsx";
// EOF
