//
// EXIFTool.js
//
// $Id: ExifTool.jsx,v 1.4 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/stdlib.js"
//@include "xlib/PSError.jsx"
//@include "xlib/xexec.js"
//@include "xlib/LogWindow.js"

EXIFTool = function() {
};

EXIFTool.prototype.exec = function(str) {
  //  var app = new File("/c/cygwin/bin/perl.exe");
  var app = new File("perl.exe");
  var exif = new File("/c/cygwin/bin/exiftool.pl").fsName
  var cmd = "perl" + ' ' +  exif + " -n -s " + str;
  return Exec.system(cmd);
};

//
// Extract information for the 
// 
EXIFTool.prototype.extract = function(tags) {

};

function main() {
  var exif = new EXIFTool();
  var doc = app.activeDocument;

  var str = exif.exec('\"' + doc.fullName.fsName + '\"').replace(/\n/g, "\r\n");
  LogWindow.open(str);
};

main();

"EXIFTool.jsx";
// EOF
