//
// FileNamingDialog
//
// $Id: FileNamingDialog.jsx,v 1.6 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
app;
//
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/stdlib.js"
//@include "xlib/GenericUI.jsx"

FileNamingDialog = function() {
  var self = this;

  self.title = "File Naming Dialog";
  self.notesSize = 0;
  self.winRect = {
    x: 100, 
    y: 100,
    w: 600,
    h: 180
  };
  self.documentation = undefined;
  self.iniFile = undefined;
  self.saveIni = false;
  self.hasBorder = false;
  self.center = false;

  self.useSerial = false;
  self.useCompatibility = false;
  self.prefix = "fn_";
  self.processTxt = "OK";
};

FileNamingDialog.prototype = new GenericUI();

FileNamingDialog.prototype._createWindow = GenericUI.prototype.createWindow;
FileNamingDialog.prototype.createWindow = function(ini, doc) {
  var self = this;

  if (self.useSerial) {
    self.winRect.h += 40;
  }
  
  if (self.useCompatibility) {
    self.winRect.h += 40;
  }

  return self._createWindow(ini, doc);
};

FileNamingDialog.prototype.createPanel = function(pnl, ini) {
  var self = this;

  self.createFileNamingPanel(pnl, ini, self.prefix,
                             self.useSerial, self.useCompatibility);
  self.startOpts = pnl.getFileNamingOptions();
  return pnl;
};

FileNamingDialog.prototype.validatePanel = function(pnl, opts) {
  var self = this;
  return self.validateFileNamingPanel(pnl, opts);
};

FileNamingDialog.test = function() {
  var ui = new FileNamingDialog();
  ui.useSerial = true;
  ui.useCompatibility = true;

  var ini = {
    fn_startingSerial: 13,
    fn_macintoshCompatible: true,
    fn_unixCompatible: false,
    fn_fileNaming: "Name,-,ddmm,lowerCaseExtension"
  };

  ui.process = function(opts) {
    alert(listProps(opts));
  };

  ui.exec(ini);
};

FileNamingDialog.test();

"FileNamingDialog.jsx";
// EOF
