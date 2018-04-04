//
// LogWindow
// This is UI code that provides a window for logging information
//
// $Id: LogWindow.js,v 1.15 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include

LogWindow = function LogWindow(title, bounds, text) {
  var self = this;

  self.title = (title || 'Log Window');
  self.bounds = (bounds || [100,100,740,580]);
  self.text = (text ? text : '');
  self.useTS = false;
  self.textType = 'edittext'; // or 'statictext'
  self.inset = 15;
  self.debug = false;

  LogWindow.prototype.textBounds = function() {
    var self = this;
    var ins = self.inset;
    var bnds = self.bounds;
    var tbnds = [ins,ins,bnds[2]-bnds[0]-ins,bnds[3]-bnds[1]-35];
    return tbnds; 
  }
  LogWindow.prototype.btnPanelBounds = function() {
    var self = this;
    var ins = self.inset;
    var bnds = self.bounds;
    var tbnds = [ins,bnds[3]-bnds[1]-35,bnds[2]-bnds[0]-ins,bnds[3]-bnds[1]];
    return tbnds; 
  }
  
  LogWindow.prototype.setText = function setText(text) {
    var self = this;
    self.text = text;
    //fullStop();
    if (self.win != null) {
      try { self.win.log.text = self.text; } catch (e) {}
    }
  }
  LogWindow.prototype.init = function(text) {
    var self = this;
    if (!text) text = '';
    self.win = new Window('dialog', self.title, self.bounds);
    var win = self.win;
    win.owner = self;
    win.log = win.add(self.textType, self.textBounds(), text,
                                {multiline:true});
    win.btnPanel = win.add('panel', self.btnPanelBounds());
    var pnl = win.btnPanel;
    pnl.okBtn = pnl.add('button', [15,5,115,25], 'OK', {name:'ok'});
    pnl.clearBtn = pnl.add('button', [150,5,265,25], 'Clear', {name:'clear'});
    if (self.debug) {
      pnl.debugBtn = pnl.add('button', [300,5,415,25], 'Debug',
                             {name:'debug'});
    }
    pnl.saveBtn = pnl.add('button', [450,5,565,25], 'Save', {name:'save'});
    self.setupCallbacks();
  }
  LogWindow.prototype.setupCallbacks = function() {
    var self = this;
    var pnl = self.win.btnPanel;
    
    pnl.okBtn.onClick = function()    { this.parent.parent.owner.okBtn(); }
    pnl.clearBtn.onClick = function() { this.parent.parent.owner.clearBtn(); }
    if (self.debug) {
      pnl.debugBtn.onClick = function() {
        this.parent.parent.owner.debugBtn();
      }
    }
    pnl.saveBtn.onClick = function()  { this.parent.parent.owner.saveBtn(); }
  }
  LogWindow.prototype.okBtn    = function() { this.close(1); }
  LogWindow.prototype.clearBtn = function() { this.clear(); }
  LogWindow.prototype.debugBtn = function() { $.level = 1; debugger; }
  LogWindow.prototype.saveBtn    = function() {
    var self = this;
    // self.setText(self.text + self._prefix() + '\r\n');
    self.save();
  }

  LogWindow.prototype.save = function() {
    try {
      var self = this;
      var f = LogWindow.selectFileSave("Log File",
                                       "Log file:*.log,All files:*",
                                       "/c/temp");
      if (f) {
        f.open("w") || throwError(f.error);
        try { f.write(self.text); }
        finally { try { f.close(); } catch (e) {} }
      }
    } catch (e) {
      alert(e.toSource());
    }
  }
  
  LogWindow.prototype.show = function(text) {
    var self = this;
    if (self.win == undefined) {
      self.init();
    }
    self.setText(text || self.text);
    return self.win.show();
  }
  LogWindow.prototype.close = function(v) {
    var self = this;
    self.win.close(v);
    self.win = undefined;
  }
  LogWindow.prototype._prefix = function() {
    var self = this;
    if (self.useTS) {
      return LogWindow.toISODateString() + "$ ";
    }
    return '';
  }
  LogWindow.prototype.prefix = LogWindow.prototype._prefix;
  LogWindow.prototype.append = function(str) {
    var self = this;
    self.setText(self.text + self.prefix() + str + '\r\n');
  }
  LogWindow.prototype.clear = function clear() {
    this.setText('');
  }

  LogWindow.toISODateString = function (date) {
    if (!date) date = new Date();
    var str = '';
    function _zeroPad(val) { return (val < 10) ? '0' + val : val; }
    if (date instanceof Date) {
      str = date.getFullYear() + '-' +
      _zeroPad(date.getMonth()+1) + '-' +
      _zeroPad(date.getDate()) + ' ' +
      _zeroPad(date.getHours()) + ':' +
      _zeroPad(date.getMinutes()) + ':' +
      _zeroPad(date.getSeconds());
    }
    return str;
  }

 LogWindow.selectFileSave = function(prompt, select, startFolder) {
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
};

LogWindow.open = function(str, title) {
  var logwin = new LogWindow(title, undefined, str);
  logwin.show();
  return logwin;
};

function throwError(e) {
  throw e;
};

"LogWindow.js";
// EOF

