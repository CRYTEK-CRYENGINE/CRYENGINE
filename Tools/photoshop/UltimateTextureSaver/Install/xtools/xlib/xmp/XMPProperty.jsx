//
// XMPProperty.jsx
//
// $Id: XMPProperty.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

XMPProperty = function XMPProperty() {
  var self = this;

  self.locale = '';
  self.namespace = ''; // XMPConst.NS_*
  self.options = XMPConst.PROP_IS_SIMPLE;
  self.path = '';
  self.value = undefined;
};
XMPProperty.prototype.toString = function() {
  return this.value.toString();
};
XMPProperty.prototype.valueOf = function() {
  return this.value;
};

"XMPProperty.jsx";
// EOF
