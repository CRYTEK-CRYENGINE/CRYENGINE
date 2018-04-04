//
// JSLog
// A common facility for logging messages in PSCS-JS
//
// $Id: JSLog.js,v 1.15 2014/11/25 02:32:13 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//

JSBaseLogger = function JSBaseLogger() {
  this.level = JSLog.DEFAULT_LEVEL;

  JSBaseLogger.prototype.write_msg = function(msg, level, tag) {
    var self = this;
    if (level >= self.level) {
      self.write(toISODateString() + " - " + tag + " - " + msg);
    }
  };
  JSBaseLogger.prototype.debug = function(msg) {
    this.write_msg(msg, JSLog.DEBUG_LEVEL, "DEBUG");
  };
  JSBaseLogger.prototype.info = function(msg) {
    this.write_msg(msg, JSLog.INFO_LEVEL, "INFO");
  };
  JSBaseLogger.prototype.error = function(msg) {
    this.write_msg(msg, JSLog.ERROR_LEVEL, "ERROR");
  };

  // subclasses implement these to methods to make this
  // framework function
  JSBaseLogger.prototype.write = function(msg) {}
  JSBaseLogger.prototype.flush = function() {}

  return this;
};

JSLog = function JSLog(level, logger) {
  var self = this;

  // create a logger delegate
  if (logger == undefined) {
    self.logger = JSLogType.create(JSLogType.DEFAULT);  
  } else if (logger instanceof JSBaseLogger) {
    self.logger = logger;
  } else if (logger instanceof JSLogType) {
    self.logger = JSLogType.create(logger);
  }
  // set this logger's level
  self.level = (level != undefined) ? level : self.logger.level;
  // set the delegate's level to ALL. let this logger
  // do the filtering
  self.logger.level = JSLog.ALL;

  return self;
};
JSLog.prototype = new JSBaseLogger();
JSLog.prototype.constructor = JSLog;
JSLog.prototype.write = function(msg) { this.logger.write(msg); }
JSLog.prototype.flush = function()    { this.logger.flush(); }

JSLog.OFF = 255;
JSLog.ALL = 0;
JSLog.DEBUG_LEVEL = 1;
JSLog.INFO_LEVEL  = 2;
JSLog.ERROR_LEVEL = 3;
if (JSLog.DEFAULT_LEVEL == undefined) { JSLog.DEFAULT_LEVEL = JSLog.DEBUG_LEVEL; }

JSLogType = function JSLogType(id) { this.toString = function() { return id;} }
JSLogType.FILE     = new JSLogType("file-logger")
JSLogType.DEBUGGER = new JSLogType("debugger-logger");
JSLogType.UI       = new JSLogType("ui-logger");
JSLogType.NULL     = new JSLogType("null-logger");
JSLogType.DEFAULT  = JSLogType.UI;

JSLogType.create = function(logType) {
  var l;
  switch (logType) {
     case JSLogType.FILE:     l = new JSFileLogger(); break;
     case JSLogType.DEBUGGER: l = new JSDebugLogger(); break;
     case JSLogType.UI:       l = new JSUILogger(); break;
     case JSLogType.NULL:     l = new JSNullLogger(); break;
  }
  return l;
};

JSNullLogger = function JSNullLogger() {
  JSNullLogger.prototype.write_msg = function(msg, level, tag) {}
  JSNullLogger.prototype.debug     = function(msg) {}
  JSNullLogger.prototype.info      = function(msg) {}
  JSNullLogger.prototype.error     = function(msg) {}
  JSNullLogger.prototype.write     = function(msg) {}
  JSNullLogger.prototype.flush     = function(msg) {}
};

JSFileLogger = function JSFileLogger(file) {
  var self = this;

  if (file == undefined && JSFileLogger.DEFAULT_FILENAME != "TEMP") {
    file = JSFileLogger.DEFAULT_FILENAME;
  }

  if (typeof file == "string") {
    self.fileName = file;
    var f = new File(file);
    f.open("e") || f.open("w", "TEXT", "????") || throwError(f.error);
    f.encoding = "UTF8";
    f.lineFeed = "unix";
    f.seek(0,2);
    self.file = f;

  } else {
    self.file = file;
    self.fileName = file.fsName;
  }

  return self;
};
JSFileLogger.prototype = new JSBaseLogger();
JSFileLogger.prototype.constructor = JSFileLogger;
JSFileLogger.prototype.write = function(msg) { this.file.writeln(msg); }
JSFileLogger.prototype.flush = function() {
  var self = this;
  var file = self.file;
  file.close();
  file.open("e", "TEXT", "????");
  file.encoding = "UTF8";
  file.lineFeed = "unix";
  file.seek(0,2);
};
if (JSFileLogger.DEFAULT_FILENAME == undefined) {
  JSFileLogger.DEFAULT_FILENAME = "/c/temp/debug.log";
}

JSUILogger = function JSUILogger() {}
JSUILogger.prototype = new JSBaseLogger();
JSUILogger.prototype.write = function(msg) {
  var self = this;
  if (self.win == undefined) {
    self.win = new LogWindow("JSLog");
    self.win.prefix = function() { return ''; }
  }
  self.win.append(msg);
};
JSUILogger.prototype.flush = function() { this.win.show(); }

JSDebugLogger = function JSDebugLogger() {}
JSDebugLogger.prototype = new JSBaseLogger();
JSDebugLogger.prototype.write = function(msg) { $.writeln(msg); }
JSDebugLogger.prototype.flush = function() { fullStop(); }

LogWindow = function LogWindow(title, bounds) {
  var self = this;

  self.title = (title || 'Log Window');
  self.bounds = (bounds || [100,100,740,580]);
  self.text = '';
  self.useTS = true;
  self.textType = 'edittext'; // or 'statictext'
  self.inset = 15;

  LogWindow.prototype.textBounds = function() {
    var self = this;
    var ins = self.inset;
    var bnds = self.bounds;
    var tbnds = [ins,ins,bnds[2]-bnds[0]-ins,bnds[3]-bnds[1]-35];
    return tbnds; 
  };
  LogWindow.prototype.btnPanelBounds = function() {
    var self = this;
    var ins = self.inset;
    var bnds = self.bounds;
    var tbnds = [ins,bnds[3]-bnds[1]-35,bnds[2]-bnds[0]-ins,bnds[3]-bnds[1]];
    return tbnds; 
  };
  
  LogWindow.prototype.setText = function setText(text) {
    var self = this;
    self.text = text;
    //fullStop();
    if (self.win != null) {
      try { self.win.log.text = self.text; } catch (e) {}
    }
  };
  LogWindow.prototype.init = function(text) {
    var self = this;
    if (!text) text = '';
    self.win = new Window('dialog', self.title, self.bounds);
    self.win.owner = self;
    self.win.log = self.win.add(self.textType, self.textBounds(), text,
                                {multiline:true});
    self.win.btnPanel = self.win.add('panel', self.btnPanelBounds());
    self.win.btnPanel.okBtn = self.win.btnPanel.add('button', [15,5,115,25],
                                                    'OK', {name:'ok'});
    self.win.btnPanel.clearBtn = self.win.btnPanel.add('button',
                                                       [150,5,265,25],
                                                       'Clear',
                                                       {name:'clear'});

    if (self.debug) {
      self.win.btnPanel.debugBtn = self.win.btnPanel.add('button',
                                                         [300,5,415,25],
                                                         'Debug',
                                                         {name:'debug'});
    }

    self.win.btnPanel.saveBtn = self.win.btnPanel.add('button',
                                                      [450,5,565,25],
                                                      'Save',
                                                      {name:'save'});
    self.setupCallbacks();
  };
  LogWindow.prototype.setupCallbacks = function() {
    var self = this;
    self.win.btnPanel.okBtn.onClick = function() {
      this.parent.parent.owner.okBtn();
    }
    self.win.btnPanel.clearBtn.onClick = function() {
      this.parent.parent.owner.clearBtn();
    }
    if (self.debug) {
      self.win.btnPanel.debugBtn.onClick = function() {
        this.parent.parent.owner.debugBtn();
      }
    }
    self.win.btnPanel.saveBtn.onClick = function() {
      this.parent.parent.owner.saveBtn();
    }
  };
  LogWindow.prototype.okBtn    = function() { this.close(1); }
  LogWindow.prototype.clearBtn = function() { this.clear(); }
  LogWindow.prototype.debugBtn = function() { $.level = 1; debugger; }
  LogWindow.prototype.saveBtn    = function() {
    var self = this;
    // self.setText(self.text + self._prefix() + '\r\n');
    self.save();
  };

  LogWindow.prototype.save = function() {
    var self = this;
    var f = selectFileSave("Log File",
                           "Log file:*.log,All files:*",
                           "/c/temp");
    if (f) {
      f.open("w") || throwError(f.error);
      f.encoding = "UTF8";
      f.lineFeed = "unix";
      try { f.write(self.text); }
      finally { try { f.close(); } catch (e) {} }
    };
  }
  
  LogWindow.prototype.show = function(text) {
    var self = this;
    if (self.win == undefined) {
      self.init();
    }
    self.setText(text || self.text);
    return self.win.show();
  };
  LogWindow.prototype.close = function(v) {
    var self = this;
    self.win.close(v);
    self.win = undefined;
  };
  LogWindow.prototype._prefix = function() {
    var self = this;
    if (self.useTS) {
      return toISODateString() + "$ ";
    }
  };
  LogWindow.prototype.prefix = LogWindow.prototype._prefix;
  LogWindow.prototype.append = function(str) {
    var self = this;
    self.setText(self.text + self.prefix() + str + '\r\n');
  };
  LogWindow.prototype.clear = function clear() {
    this.setText('');
  };
};

function selectFileSave(prompt, select, startFolder) {
  var oldFolder = Folder.current;

  if (startFolder) {
    if (typeof(startFolder) == "object") {
      if (!(startFolder instanceof "Folder")) {
        throw "Folder object wrong type";
      }
      Folder.current = startFolder;

    } else if (typeof(startFolder) == "string") {
      var s = startFolder;
      startFolder = new Folder(s);
      if (startFolder.exists) {
        Folder.current = startFolder;
      } else {
        startFolder = undefined;
        // throw "Folder " + s + "does not exist";
      }
    }
  }

  var file = File.saveDialog(prompt, select);

  //alert("File " + file.path + '/' + file.name + " selected");
  if (Folder.current == startFolder) {
    Folder.current = oldFolder;
  }
  return file;
};
function toISODateString(date) {
  if (!date) date = new Date();
  var str = '';
  function _zeroPad(val) { return (val < 10) ? '0' + val : val; }
  if (date instanceof Date) {
    str = date.getFullYear() + '-' +
      _zeroPad(date.getMonth()+1,2) + '-' +
      _zeroPad(date.getDate(),2) + 'T' +
      _zeroPad(date.getHours(),2) + ':' +
      _zeroPad(date.getMinutes(),2) + ':' +
      _zeroPad(date.getSeconds(),2);
  }
  return str;
};
function fullStop() {
  $.level = 1; debugger;
};
function throwError(e) {
  throw e;
};

JSLog.test = function() {
  var log = new JSLog(JSLog.ALL, JSLogType.FILE);

  fullStop();
  log.level = JSLog.DEBUG_LEVEL;
  log.write("TEST debug level");
  log.debug("debug message");
  log.info("info message");
  log.error("error message");
  log.flush();

  log.level = JSLog.INFO_LEVEL;
  log.write("TEST info level");
  log.debug("debug message");
  log.info("info message");
  log.error("error message");
  log.flush();

  log.level = JSLog.ERROR_LEVEL;
  log.write("TEST error level");
  log.debug("debug message");
  log.info("info message");
  log.error("error message");
  log.flush();

  log.level = JSLog.OFF;
  log.write("TEST off");
  log.debug("debug message");
  log.info("info message");
  log.error("error message");
  log.flush();
};

LogWindow.test = function() {
  var logwin = new LogWindow('Test Log Window');
  logwin.append('Start');
  logwin.append(logwin.bounds);
  logwin.append('more stuff');
  logwin.show();
  logwin.show("That's all, folks...");
};

//JSLog.test();
//LogWindow.test();

"JSLog.js";
// EOF
