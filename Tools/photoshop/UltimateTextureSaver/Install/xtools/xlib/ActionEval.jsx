//
// ActionEval
//
// $Id: ActionEval.jsx,v 1.22 2012/01/24 18:26:33 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//-includepath "/c/Program Files/Adobe/xtools;"
//
//-include "xlib/PSConstants.js"
//-include "xlib/stdlib.js"
//-include "xlib/Stream.js"
//-include "xlib/Action.js"
//-include "xlib/xml/atn2bin.jsx"
//

ActionEval = function(action) {
  var self = this;

  self.action          = action;
  self.handlers        = {};
  self.context         = {};
  self.playbackOptions = undefined;
  self.enabled         = undefined;
  self.dialogMode      = undefined; // 0, 1, 2, 3 ???

  self.ip = 0; // instruction pointer
};
ActionEval.prototype.typename = "ActionEval";
ActionEval.STATUS_OK   = 'OK';
ActionEval.STATUS_TERM = 'Terminate';
ActionEval.STATUS_FAIL = 'Fail';

ActionEval.Stdlib = Stdlib;

ActionEval.SCRIPT_MARKER = '//!JSXScript';

ActionEval.prototype.exec = function() {
  var self = this;
  self.registerHandlers();
  while (self.step()) {
    ; // empty
  }
};

ActionEval.prototype.step = function() {
  var self = this;
  var action = self.action;

  if (self.ip >= action.getCount()) {
    return false;
  }

  var ai = action.byIndex(self.ip);
  self.executeActionItem(ai);
  self.ip++;
  return true;
};

ActionEval.prototype.registerHandler = function(name, ftn) {
  this.handlers[name] = ftn;
};
ActionEval.prototype.registerHandlers = function() {
  var self = this;

  self.registerHandler("Scripts", ActionEval.runScript);
  self.registerHandler("Play", ActionEval.doAction);
  self.registerHandler("Stop", ActionEval.doStop);
};

ActionEval.prototype.executeActionItem = function(ai) {
  var self = this;
  var rc;

  if (!ai.enabled) {
    return ActionEval.STATUS_OK;
  }
  var handler = self.handlers[ai.name];

  if (ai.identifier == ActionItem.TEXT_ID && handler) {
    rc = handler.call(self, ai);
    Stdlib = ActionEval.Stdlib;
    return rc;
  } 

  var execId;
  if (ai.identifier == ActionItem.TEXT_ID) {
    execId = PSConstants.lookup(ai.name);
    if (!execId) {
      execId = PSConstants.lookup(ai.event);
    }
  } else {
    execId = ai.itemID;
  }

  var desc = ai.hasDescriptor ? ai.descriptor : undefined;
  var dmode = ai.withDialog ? DialogModes.ALL : DialogModes.NO;
  executeAction(execId, desc, dmode);

  return ActionEval.STATUS_OK;
};
ActionEval.prototype.runScriptSegement = function(str) {
  var self = this;
  var doc;
  try { doc = app.activeDocument; } catch (e) {}
  var context = self.context;
  if (doc) {
    var n = doc.name;
    var m = n.match(/^(.*)\.([^.]+)$/);
    if (!m) {
      m = [n, n, ''];
    }
    context.basename = m[1];
    context.ext      = m[2];
  }
  eval(str);
};
ActionEval.runScript = function(ai) {
  var desc = ai.descriptor;
  var path = undefined;
  if (desc.hasKey(cTID('jsCt'))) {
    path = desc.getPath(cTID('jsCt'));
    
  } else if (desc.hasKey(cTID('jsMs'))) {
    path = Stdlib.SCRIPTS_FOLDER + "/" + desc.getString(cTID('jsMs'));
  }

  if (!path) {
    Error.runtimeError(9001, "Unable to determine script path");
  }

  var file = decodeURI(path);

  //Stdlib = undefined;
  try {
    var str = '//@show include\r\n//@include "' + file.absoluteURI + '";\r\n';
    eval(str);

  } finally {
  }

  return ActionEval.STATUS_OK;
};

ActionEval.doAction = function(ai) {
  var self = this;
  var desc = ai.descriptor;
  var ref = desc.getReference(cTID('null'));
  var atn;
  var atnSet;

  while (ref) {
    try {
      var cls = ref.getDesiredClass();
    } catch (e) {
      break;
    }
    var nm = ref.getName();
    if (cls == cTID('Actn')) {
      atn = nm;
    } else if (cls == cTID('ASet')) {
      atnSet = nm;
    }
    ref = ref.getContainer();
  }

  ActionEval.evalAction(atn, atnSet);

  return ActionEval.STATUS_OK;
};
ActionEval.doStop = function(ai) {
  var self = this;
  var desc = ai.descriptor;
  var msg = '';
  if (desc.hasKey(PSKey.Message)) {
    msg = desc.getString(PSKey.Message);
  }
  var cont = false;
  if (desc.hasKey(PSKey.Continue)) {
    cont = true;
  }

  if (msg.startsWith(ActionEval.SCRIPT_MARKER)) {
    self.runScriptSegement(msg);
    if (!cont) {
      return ActionEval.STATUS_TERM;
    }
  } else {
    try {
      executeAction(PSEvent.Stop, desc,
                    ai.withDialog ? DialogModes.ALL : DialogModes.NO);
    } catch (e) {
      if (!e.toString().match("User cancelled the operation")) {
        throw e;
      }
    }
  }
  return ActionEval.STATUS_OK;
};

ActionEval.loadActionFile = function(infile) {
  var infile = Stdlib.convertFptr(infile);
  var fname = infile.name.toLowerCase();
  var actFile;

  if (fname.endsWith(".xml")) {
    var xmlstr = Stdlib.readFromFile(infile);
    actFile = ActionFile.deserialize(xmlstr);

  } else if (fname.endsWith(".atn")) {
    var actFile = new ActionFile();
    actFile.read(infile);

  } else {
    throw "Unknow file type. Cannot load action file";
  }
  return actFile;
};

ActionEval.loadAction = function(infile, name) {
  var self = this;
  var actFile;
  var act;

  actFile = self.loadActionFile(infile);

  if (actFile) {
    act = actFile.actionSet.getByName(name);
  }
  return act;
};

ActionEval.evalAction = function(name, inp) {
  var act;

  if (!name) Error.runtimeError(19, "action");
  if (!inp)  Error.runtimeError(19, "actionSet");

  var fptr = inp;

  if (inp instanceof Action) {
    act = inp;

  } else if (inp.constructor == String) {
    if (!Stdlib.hasAction(name, inp)) {
      throw "Error: Action " + inp + ':' + name + " is not available.";
    }
    var base = name.replace(/\W/g, '_');
    var fptr = File(Folder.temp + '/' + base + ".exe");

    Stdlib.createDroplet(name, inp, fptr);

    var str = Stream.readStream(fptr);
    fptr.remove();

    act = new Action();
    act.readDroplet(str);

  } else if (inp instanceof File) {
    act = ActionEval.loadAction(inp, name);
  }

  if (act == undefined) {
    throw ("Unable to load action \'" + name + "\'" +
           "\' from file \'" + fptr + "\'");
  }

  var runner = new ActionEval(act);
  runner.exec();
};
 
ActionEval.evalDropletAction = function(fptr) {
  var file = Stream.convertFptr(fptr);
  var str = Stream.readStream(file);
  var action = new Action();

  action.readDroplet(str);
  ActionEval.evalAction(action);
};

ActionEval.runAction = function(fptr, action) {
  var act = ActionEval.loadAction(fptr, action);
  var runner = new ActionEval(act);
  runner.exec();
};


"ActionEval.js";
// EOF
