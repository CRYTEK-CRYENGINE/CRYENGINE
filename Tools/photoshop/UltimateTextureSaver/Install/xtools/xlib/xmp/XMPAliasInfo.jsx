//
// XMPAliasInfo.jsx
//
// $Id: XMPAliasInfo.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include

XMPAliasInfo = function XMPAliasInfo() {
  var self = this;

  self.arrayForm = 0;     // XMPConst.ALIAS_TO_(SIMPLE_PROP | ARRAY | 
                          //                    ORDERED_ARRAY
                          //                    ALT_ARRAY | ALT_TEXT)
  self.name = undefined;
  self.namespace = '';    // XMPConst.NS_*
};
XMPAliasInfo.prototype.toString = function() {
  var self = this;
  if (!self.name || !self.namespace) {
    return "[XMPAliasInfo]";

  } else {
    var str = "[XMPAliasInfo " + XMPMeta.getNamespacePrefix(self.namespace);
    str += ':' + self.name;
    if (self.arrayForm != XMPConst.ALIAS_TO_SIMPLE_PROP) {
      str += " (" + "0x%X".sprintf(self.arrayForm) + ")";
    }
    str += ']';
    return str;
  }
};
"XMPAliasInfo.jsx";
// EOF
