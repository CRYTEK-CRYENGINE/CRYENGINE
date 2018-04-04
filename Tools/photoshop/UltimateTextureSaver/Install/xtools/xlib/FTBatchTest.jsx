//
// FTBatchTest
//
// $Id: FTBatchTest.jsx,v 1.4 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/FTBatch.jsx"

//
// This method must be defined in the top-level
// script file
//
FTBatch.getScriptFileName = function() {
  var dbLevel = $.level;
  $.level = 0;
  var path = undefined;

  try {
    some_undefined_variable;
  } catch (e) {
    path = e.fileName;
  }

  $.level = dbLevel;

  return path;
};

// This is our reference to our image folder.
// It is not used by FTBatch.
FTBatch.imageFolder = "~/ximages";

//
// This function must be defined to return the complete
// list of files to be processed
//
FTBatch.prototype.init = function() {
  var res = [];

  var folder = new Folder(FTBatch.imageFolder);
  var files = folder.getFiles("*.jpg");

  return files;
};

//
// This propery may be overridden as needed. The default is 10
// files to be processed before a restart
//
FTBatch.prototype.batchSize = 7;

//
// This method will be called for each file to be processed
//
FTBatch.prototype.processFile = function(file) {
  alert("File: " + file.absoluteURI);
  return true;
};

// Create a new instance
var ftest = new FTBatch();

// Start (or continue) processing
ftest.main();

// EOF

