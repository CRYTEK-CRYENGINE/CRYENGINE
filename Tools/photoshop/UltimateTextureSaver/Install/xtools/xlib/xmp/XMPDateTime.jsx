//
// XMPDateTime.jsx
//
// $Id: XMPDateTime.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

XMPDateTime = function XMPDateTime(arg) {
  // arg is one of (Date, iso8601Date/String, undefined)

  var self = this;

  self.year = 0;
  self.month = 1;
  self.day = 1;
  self.hour = 1;
  self.minute = 0;
  self.second = 0;
  self.nanosecond = 0.0;
  self.tzSign = 0; // 1,-1
  self.tzHour = 0;
  self.tzMinute = 0;

  if (arg) {
    self.setDate(arg);
  }
};

XMPDateTime.prototype.compareTo = function(xmpDateTime) {
  return 0;
};
XMPDateTime.prototype.convertToLocalTime = function() {
  return undefined;
};
XMPDateTime.prototype.convertToUTCTime = function() {
  return undefined;
};
XMPDateTime.prototype.getDate = function() {
  return new Date();
};
XMPDateTime.prototype.setLocalTime = function() {
  return undefined;
};

XMPDateTime.prototype.setDate = function(date) {
  var self = this;

  if (!date) {
    throw Error("Date not specified");
  }
  if (date.constructor == String) {
    date = Stdlib.parseISODateString(date);
    if (!date) {
      throw Error("Bad ISO date string");
    }
  }
  if (date instanceof XMPDateTime) {
    Stdlib.copyFromTo(date, self);
    return;
  }

  self.year = date.getFullYear();
  self.month = date.getMonth() + 1;
  self.day = date.getDate();
  self.hour = date.getHours();
  self.minute = date.getMinutes();
  self.second = date.getSeconds();
  self.nanosecond = date.getMilliseconds() * 1000;
  self.tzSign = 0; // 1,-1
  self.tzHour = 0;
  self.tzMinute = 0;

  if (date.tzd) {
    var m = date.tzd.match(/Z(\+|\-)(\d{2}):(\d{2})/);
    if (m) {
      self.tzSign = (m[1] == '+') ?  1 : -1;
      self.tzHour = Number(m[2]);
      self.tzMinute = Number(m[3]);
    }
  }
};

"XMPDateTime.jsx";
// EOF
