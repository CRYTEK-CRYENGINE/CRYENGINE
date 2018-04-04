//
// A PS-JS command line shell
//
// $Id: JShell.js,v 1.21 2009/02/19 18:03:55 anonymous Exp $
//
//@show include

cTID = function(s) { return app.charIDToTypeID(s); };
sTID = function(s) { return app.stringIDToTypeID(s); };

JShell = function JShell(title, bounds) {
  var self = this;

  self.title = (title || 'JShell');
  self.bounds = (bounds || [100,100,740,580]);
  self.text = '';
  self.useTS = true;
  self.textType = 'edittext'; // or 'statictext'
  self.inset = 15;
  self.init();
  self.windowCreationProperties = { resizeable : true }; 

  return self;
};

JShell.prototype.outtextBounds = function() {
  var self = this;
  var ins = self.inset;
  var bnds = self.bounds;
  var tbnds = [ins,ins,bnds[2]-bnds[0]-ins,bnds[3]-bnds[1]-135];
  return tbnds; 
};
JShell.prototype.intextBounds = function() {
  var self = this;
  var ins = self.inset;
  var bnds = self.bounds;
  var tbnds = [ins,bnds[3]-bnds[1]-135,bnds[2]-bnds[0]-ins,bnds[3]-bnds[1]-35];
  return tbnds; 
};
JShell.prototype.btnPanelBounds = function() {
  var self = this;
  var ins = self.inset;
  var bnds = self.bounds;
  var tbnds = [ins,bnds[3]-bnds[1]-35,bnds[2]-bnds[0]-ins,bnds[3]-bnds[1]];
  return tbnds; 
};
  
JShell.prototype.setText = function setText(text) {
  var self = this;
  self.text = text;
  //fullStop();
  if (self.win != null) {
    try { self.win.log.text = self.text; } catch (e) {}
  }
};
JShell.prototype.init = function(text) {
  var self = this;
  if (!text) text = '';
  self.win = new Window('dialog', self.title, self.bounds,
                        self.windowCreationProperties);
  self.win.owner = self;
  self.win.log      = self.win.add(self.textType, self.outtextBounds(),
                                   text, {multiline:true});
  self.win.input    = self.win.add(self.textType, self.intextBounds(),
                                   '', {multiline:true});

  self.win.btnPanel = self.win.add('panel', self.btnPanelBounds());
  var bp = self.win.btnPanel;
  bp.evalBtn  = bp.add('button', [15,5,115,25],  'Eval',  {name:'eval'});
  bp.clearBtn = bp.add('button', [150,5,265,25], 'Clear', {name:'clear'});
  bp.debugBtn = bp.add('button', [300,5,415,25], 'Debug', {name:'debug'});
  bp.quitBtn  = bp.add('button', [450,5,565,25], 'Quit',  {name:'quit'});

  //self.win.defaultElement = self.win.btnPanel.evalBtn;
  //self.win.cancelElement = self.win.btnPanel.okBtn;
  self.setupCallbacks();
};
JShell.prototype.setupCallbacks = function() {
  var self = this;
  var bp = self.win.btnPanel;
  bp.quitBtn.onClick  = function() { this.parent.parent.owner.quitBtn(); }
  bp.clearBtn.onClick = function() { this.parent.parent.owner.clearBtn(); }
  bp.debugBtn.onClick = function() { this.parent.parent.owner.debugBtn(); }
  bp.evalBtn.onClick  = function() { this.parent.parent.owner.evalBtn(); }
};
JShell.prototype.quitBtn  = function() { this.close(1); };
JShell.prototype.clearBtn = function() { this.clear(); };
JShell.prototype.debugBtn = function() { $.level = 1; debugger; };
JShell.prototype.evalBtn  = function() {
  var self = this;
  var input = self.win.input.text;
  self.append(input);
  
  try {
    var out = eval(input);
    self.win.input.text = '';
    if (typeof out == "boolean") {
      self.append(out);
    } else if (out != undefined && out != '') {
      self.append(out.toString());
    } else if (typeof out == "number") {
      self.append(out.toString());
    }
  } catch (e) {
    try {
      Stdlib;
      self.append(Stdlib.exceptionMessage(e));
    } catch (e) {
      self.append(e);
    }
  }
  waitForRedraw();
};

waitForRedraw = function() {
  var desc = new ActionDescriptor();
  desc.putEnumerated(cTID("Stte"), cTID("Stte"), cTID("RdCm"));
  executeAction(cTID("Wait"), desc, DialogModes.NO)
};

JShell.prototype.save = function() {
  var self = this;
  var f = selectFileSave("Log File",
                         "Log file:*.log,All files:*",
                         "/c/temp");
  if (f) {
    f.open("w") || throwError(f.error);
    try { f.write(self.text); }
    finally { try { f.close(); } catch (e) {} }
  }
};
  
JShell.prototype.show = function(text) {
  var self = this;
  if (self.win == undefined) {
    self.init();
  }
  self.setText(text || self.text);
  try {
    return self.win.show();
  } catch (e) {
    alert("Internal error in Javascript Interpreter.\r" +
          "Please restart Photoshop");
    throw e;
  }
};
JShell.prototype.close = function(v) {
  var self = this;
  //fullStop();
  self.win.close(v);
  // self.win = undefined;
};
JShell.prototype._prefix = function() {
  var self = this;
  if (self.useTS) {
    return toISODateString() + "$ ";
  }
};
JShell.prototype.prefix = JShell.prototype._prefix;
JShell.prototype.append = function(str) {
  var self = this;
  self.setText(self.text + self.prefix() + str + '\r\n');
};
  
JShell.prototype.clear = function() {
  this.setText('');
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

//var f = selectFile("Log File",
//                   "Log file:*.log,All files:*",
//                   "/c/temp");
function selectFileSave(prompt, select, startFolder) {

  var oldFolder = Folder.current;
  if (startFolder) {
    if (typeof(startFolder) == "object") {
      if (!(startFolder instanceof "Folder")) { throw "Folder object wrong type"; }
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
function selectFileOpen(prompt, select, startFolder) {

  var oldFolder = Folder.current;
  if (startFolder) {
    if (typeof(startFolder) == "object") {
      if (!(startFolder instanceof "Folder")) { throw "Folder object wrong type"; }
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
  var file = File.openDialog(prompt, select);
  //alert("File " + file.path + '/' + file.name + " selected");
  if (Folder.current == startFolder) {
    Folder.current = oldFolder;
  }
  return file;
};
function throwError(e) {
  throw e;
};

function runScript(name) {
  if (!name) {
    name = selectFileOpen("Javascript File",
                          "Source File:*.js,All files:*");
  }
  var str = "";
  fullStop();
  str = "//@show include\r//@include \"" + name + "\"\r";
  if (name.charAt(0) != '/') {
    str = "//@include \"IncludePath.js\"\r" + str;
  }
  eval(str);
  return true;
};
function loadFile(name) {
  if (name == undefined) {
    name = selectFile("Javascript File",
                      "Source File:*.js,All files:*");
  }
  var f = new File(name);
  f.open("r") || throwError(f.error);
  var s = f.read(f.length);
  f.close();
  return s;
};
function evalFile(name) {
  return eval(loadFile(name));
};
function write(str) {
  var sh = JShell.shell;
  sh.setText(sh.text + str);
};
function writeln(str) {
  var sh = JShell.shell;
  sh.setText(sh.text + str + '\r\n');
};
function showProps(obj) {
  if (false) {
    var s = '';
    for (var x in obj) {
      s += x + ": ";
      try {
        var o = obj[x];
        s += (typeof o == "function") ? "[function]" : o;
      } catch (e) {
      }
      s += "\r\n";
    }
  writeln(s);
  }
  writeln(listProps(obj));
};

runScript = function(name, path) {
  var str = "//@include \"" + name + "\";\r";
  if (path) {
    str = "//@include \"" + path + "\";\r" + str;
  }
  eval(str); // can't do this at top-level so some scoping problems
             // are inevitable
  return true;
};

var global = this;
function listGlobals() {
  var str = '';
  for (var i in global) {
    if (i != "str") {
      str += i + ":\t";
      try {
        var o = this[i];
        str += "[" + (typeof o) + "]";
        if (typeof o != "function") {
          str += ":\t" + this[i].toString();
        }
      } catch (e) {
        str += "[]";
      }
      str += "\r\n";
    }
  }
  return str;
};

JShell.exec = function() {
  var jshell = new JShell();
  JShell.shell = jshell;
  jshell.append("Shell UI Started\r\n");
  //   "Global functions must be defined with this syntax:\r\n" +
  //  "\tftn = function() { writeln('in function \\\"ftn\\\"'); }");

  var dbgLevel = $.level;
  $.level = 0;
  try {
    docs = app.documents;
    doc = app.activeDocument;
    layer = doc.activeLayer;
    titem = layer.textItem;
  } catch (e) {
  }
  $.level = dbgLevel;

  jshell.show();
};

// preload some stuff, kinda like an .cshrc or an autoexec.bat
if ((new File("/c/tmp/jshrc.js")).exists) {
  eval('//@include "/c/tmp/jshrc.js\r"');
}

//JShell.test();

"JShell.jsx";

// EOF
