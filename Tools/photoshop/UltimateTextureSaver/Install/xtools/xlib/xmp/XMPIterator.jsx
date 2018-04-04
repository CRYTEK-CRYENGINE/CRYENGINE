//
// XMPIterator.jsx
//
// $Id: XMPIterator.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

XMPIterator = function XMPIterator() {
};

XMPIterator.prototype.next = function() {
  return null; // XMPProperty
};
                                       
XMPIterator.prototype.skipSiblings = function() {
  return undefined;
};
XMPIterator.prototype.skipSubtree = function() {
  return undefined;
};

"XMPIterator.jsx";
// EOF
