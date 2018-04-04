//
// atn2bin.jsx
// This script converts ActionDescriptors to action files and back
//
// $Id: atn2bin.jsx,v 1.43 2014/11/25 04:11:22 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//

//include "xlib/Stream.js"
//include "xlib/ActionStream.js"

var atn2bin = true;

var atn2binTestMode = false;

var ATN_ERR = 9100;

//$.level = 1;

//=========================== ActionsPaletteFile ==============================

ActionsPaletteFile.readFrom = function(file) {
  var apf = new ActionsPaletteFile();
  apf.read(file);
  return apf;
};
ActionsPaletteFile.prototype.read = function(fptr) {
  var self = this;
  if (!fptr) {
    Error.runtimeError(ATN_ERR, "File argument must be specified for ActionsPaletteFile.read.");
  }

  ActionStream.log("ActionsPaletteFile.read(" + fptr + ")");

  self.file = Stdlib.convertFptr(fptr);
  if (!self.file.exists) {
    Error.runtimeError(ATN_ERR, "File does not exist \"" + self.file + "\".");
  }

  self.actionsPalette = new ActionsPalette();
  var str = Stream.readStream(self.file);
  self.version = str.readWord();
  self.actionsPalette.parent = self;
  self.actionsPalette.version = self.version;
  self.actionsPalette.read(str);
  ActionStream.log("ActionsPaletteFile.read(" + fptr + ") - Done");
};
ActionsPaletteFile.prototype.write = function(fptr) {
  var self = this;
  if (!fptr) {
    Error.runtimeError(ATN_ERR, "File argument must be specified for ActionsPaletteFile.write.");
  }

  ActionStream.log("ActionsPaletteFile.write(" + fptr + ")");

  self.file = Stdlib.convertFptr(fptr);
  var str = new Stream();
  str.writeWord(self.version);
  self.actionsPalette.version = self.version;
  self.actionsPalette.write(str);

  // XXX this is a magic block of bytes that work. maybe.
  str.writeInt16(0);
  str.writeInt16(0);
  str.writeInt16(0);
  str.writeInt16(0);
  str.writeInt16(0x0102);

  // XXX This probably needs to be changed to
  // str.writeToFile(self.file);
  Stream.writeToFile(self.file, str.toStream());

  ActionStream.log("ActionsPaletteFile.write(" + fptr + ") - Done");
};

ActionsPaletteFile.iterateOverFile = function(file, iterator) {
  var apf = new ActionsPaletteFile();
  apf.iterate(file, iterator);
  return apf;
};
ActionsPaletteFile.prototype.iterate = function(fptr, iterator) {
  var self = this;
  if (!fptr) {
    Error.runtimeError(ATN_ERR, "File argument must be specified for ActionsPaletteFile.read.");
  }

  if (!iterator) {
    Error.runtimeError(ATN_ERR, "No iterator specified");
  }

  self.file = Stdlib.convertFptr(fptr);
  if (!self.file.exists) {
    Error.runtimeError(ATN_ERR, "File does not exist \"" + self.file + "\".");
  }

  self.actionsPalette = new ActionsPalette();
  var str = Stream.readStream(self.file);
  self.version = str.readWord();
  self.actionsPalette.parent = self;
  self.actionsPalette.version = self.version;
  self.actionsPalette.iterate(str, iterator);
};


//================================ ActionFile =================================

ActionFile.readFrom = function(file) {
  var af = new ActionFile();
  af.read(file);
  return af;
};

ActionFile.prototype.read = function(fptr) {
  var self = this;
  if (!fptr) {
    Error.runtimeError(ATN_ERR, "File argument must be specified for ActionFile.read.");
  }
  ActionStream.log("ActionFile.read(" + fptr + ")");

  self.file = Stdlib.convertFptr(fptr);
  if (!self.file.exists) {
    Error.runtimeError(ATN_ERR, "File does not exist \"" + self.file + "\".");
  }

  self.actionSet = new ActionSet();
  var str = new Stream.readStream(self.file);
  self.actionSet.read(str);
  self.actionSet.parent = self;

  ActionStream.log("ActionFile.read(" + fptr + ") - Done");
};

ActionFile.prototype.write = function(fptr) {
  var self = this;
  var file;

  ActionStream.log("ActionFile.write(" + fptr + ")");

  if (fptr) {
    file = Stdlib.convertFptr(fptr);
  } else {
    file = self.file;
  }

  file.open("w") ||  Error.runtimeError(9002, "Unable to open output file \"" +
                                        file + "\".\r" + file.error);
  file.encoding = 'BINARY';

  var str = new Stream();
  self.actionSet.writeToFile(file, true);

  file.close();

  // str.writeToFile(self.file);

  ActionStream.log("ActionsFile.write(" + file + ") - Done");
};


//=========================== ActionsPalette ==================================

ActionsPalette.prototype.read = function(str) {
  var self = this;

  ActionStream.log("ActionsPalette.read()");

  // if a psp file was not specified, read from the PS runtime
  if (!str) {
    return self.readRuntime();
  }

  var count = str.readWord();
  for (var i = 1; i <= count; i++) {
    var as = new ActionSet();
    as.parent = self;
    as.index = i;
    as.read(str, false);
    if (!as.version) {
      as.version = self.version;
    }
    self.add(as);
  }
  self.count = self.actionSets.length;

  ActionStream.log("ActionsPalette.read() - Done");
};

ActionsPalette.prototype.write = function(str) {
  var self = this;

  ActionStream.log("ActionsPalette.write()");

  var asets = self.actionSets;
  var count = asets.length;

  str.writeWord(count);

  for (var i = 0; i < count; i++) {
    var as = asets[i];
    as.write(str, false);
  }

  ActionStream.log("ActionsPalette.write() - Done");
};

ActionsPalette.prototype.writeToFile = function(file) {
  var self = this;

  ActionStream.log("ActionsPalette.writeToFile(" + file.toUIString()  + ")");

  var asets = self.actionSets;
  var count = asets.length;

  var str = new Stream();
  str.writeWord(count);
  file.write(str.toStream());

  for (var i = 0; i < count; i++) {
    var as = asets[i];
    as.writeToFile(file, false);
  }

  ActionStream.log("ActionsPalette.writeToFile() - Done");
};


ActionsPalette.prototype.readRuntime = function() {
  var self = this;
  var i = 1;

  ActionStream.log("ActionsPalette.readRuntime()");

  //$.level = 1; debugger;
  while (true) {
    var ref = new ActionReference();
    ref.putIndex(PSClass.ActionSet, i);
    var desc;
    try {
      desc = executeActionGet(ref);
    } catch (e) {
      break;    // all done
    }
    var as = new ActionSet();
    as.parent = self;
    as.index = i;
    if (desc.hasKey(PSKey.Name)) {
      as.name = localize(desc.getString(PSKey.Name));
    }
    if (desc.hasKey(PSKey.NumberOfChildren)) {
      as.count = desc.getInteger(PSKey.NumberOfChildren);
      as.readRuntime(i);
    }
    self.add(as);
    i++;
  }
  self.count = self.actionSets.length;

  ActionStream.log("ActionsPalette.readRuntime() - Done");
};

ActionsPalette.prototype.loadRuntime = function() {
  var self = this;

  self.readRuntime();

  var asets = self.actionSets;

  for (var i = 0; i < asets.length; i++) {
    var aset = asets[i];
    aset.loadRuntime();
  }
};

ActionsPalette.prototype.iterate = function(str, iterator) {
  var self = this;

  // if a psp file was not specified, read from the PS runtime
  if (!str) {
    return self.iterateRuntime(iterator);
  }

  self.count = str.readWord();
  for (var i = 1; i <= self.count; i++) {
    var as = new ActionSet();
    as.parent = self;
    as.index = i;
    as.read(str, false);
    if (!as.version) {
      as.version = self.version;
    }
    iterator.exec(as);
    delete as;
    $.gc();
  }
};
ActionsPalette.prototype.iterateRuntime = function(iterator) {
  var self = this;
  var i = 1;

  //$.level = 1; debugger;
  while (true) {
    var ref = new ActionReference();
    ref.putIndex(PSClass.ActionSet, i);
    var desc;
    try {
      desc = executeActionGet(ref);
    } catch (e) {
      break;    // all done
    }
    var as = new ActionSet();
    as.parent = self;
    as.index = i;
    if (desc.hasKey(PSKey.Name)) {
      as.name = localize(desc.getString(PSKey.Name));
    }
    if (desc.hasKey(PSKey.NumberOfChildren)) {
      as.count = desc.getInteger(PSKey.NumberOfChildren);
      as.readRuntime(i);
    }
    iterator.exec(as);
    delete as;
    $.gc();
    i++;
  }
  self.count = i-1;
};


ActionsPalette.prototype.add = function(actionSet) {
  var self = this;
  actionSet.parent = self;
  actionSet.version = self.version;
  self.actionSets.push(actionSet);
  self.count = self.actionSets.length;
};

//================================ ActionSet ==================================


ActionSet.prototype.read = function(str, readVersion) {
  var self = this;

  // debugger;
  if (readVersion != false) {
    self.version = str.readWord();
  } else {
    self.version = 0x10;
  }
  self.name = localize(str.readUnicode());
  self.expanded = str.readBoolean();

  // XXX var acts = [];
  var len = str.readWord();
  self.actions = [];
  for (var i = 0; i < len; i++) {
    var act = new Action();
    act.read(str);
    self.add(act);
    //XXX acts.push(act);
  }
  // XXX self.actions = acts;
  // self.count = self.actions.length;

  ActionStream.log("ActionSet.read(" + self.name + ") - Done");

  return self;
};
ActionSet.prototype.getDescriptor = function() {
  var self = this;
  var ref = new ActionReference();

  ref.putIndex(cTID("ASet"), self.index);

  var desc = undefined;
  var lvl = $.level;
  $.level = 0;

  try {
    desc = executeActionGet(ref);

  } catch (e) {
    ; // we had a bad index...
  } finally {
    $.level = lvl;
  }
  return desc;
};
ActionSet.prototype.readRuntime = function() {
  var self = this;

  var desc = self.getDescriptor();
  self.count = desc.getInteger(cTID("NmbC"));
  var max = self.count;
  if (desc.hasKey(PSKey.Name)) {
    self.name = localize(desc.getString(PSKey.Name));
  }

  ActionStream.log("ActionSet.readRuntime(" + self.name + ")");

  self.actions = [];

  for (var i = 1; i <= max; i++) {
    var ref = new ActionReference();
    ref.putIndex(PSClass.Action, i);
    ref.putIndex(PSClass.ActionSet, self.index);
    var desc = executeActionGet(ref);
    var act = new Action();
    act.readRuntime(desc);
    self.add(act);
  }

  ActionStream.log("ActionSet.readRuntime(" + self.name + ") - Done");
};

ActionSet.prototype.loadRuntime = function(name, index) {
  var self = this;

  if (name) {
    self.name = localize(name);
  }

  if (index) {
    self.index = index;
  }

  self.readRuntime();
  self.version = 0x10;

  var acts = self.actions;

  for (var i = 0; i < acts.length; i++) {
    var act = acts[i];
    act.loadRuntime(act.name, self.name);
  }
};

ActionSet.prototype.writeHeader = function(str, writeVersion) {
  var self = this;

  ActionStream.log("ActionSet.writeHeader(" + self.name + ")");

  //debugger;
  if (writeVersion) {
    str.writeWord(self.version);
  }
  str.writeUnicode(self.name);
  str.writeBoolean(self.expanded);
  str.writeWord(self.actions.length);

  ActionStream.log("ActionSet.writeHeader(" + self.name + ") - Done");
};

ActionSet.prototype.writeActions = function(str) {
  var self = this;
  ActionStream.log("ActionSet.writeActions(" + self.name + ")");

  var acts = self.actions;
  for (var i = 0; i < acts.length; i++) {
    var act = acts[i];
    act.write(str);
  }

  ActionStream.log("ActionSet.writeActions(" + self.name + ") - Done");
};

ActionSet.prototype.writeToFile = function(file, writeVersion) {
  var self = this;

  ActionStream.log("ActionSet.writeToFile(" + self.name + ", " +
                   file.toUIString() + ")");

  var str = new Stream();
  self.writeHeader(str, true);
  file.write(str.toStream());

  var acts = self.actions;
  for (var i = 0; i < acts.length; i++) {
    var act = acts[i];
    var str = new Stream();
    act.write(str);
    file.write(str.toStream());
  }

  ActionStream.log("ActionSet.writeToFile() - Done.");
  return file;
};

ActionSet.prototype.write = function(str, writeVersion) {
  var self = this;

  ActionStream.log("ActionSet.write(" + self.name + ")");

  //debugger;
  if (writeVersion) {
    str.writeWord(self.version);
  }
  str.writeUnicode(self.name);
  str.writeBoolean(self.expanded);

  var acts = self.actions;
  str.writeWord(acts.length);
  for (var i = 0; i < acts.length; i++) {
    var act = acts[i];
    act.write(str);
  }

  ActionStream.log("ActionSet.write(" + self.name + ") - Done");
};
ActionSet.prototype.add = function(act) {
  var self = this;

  act.parent = self;
  self.actions.push(act);
  self.count = self.actions.length;
};


//================================ Action =====================================

Action.prototype.read = function(str) {
  var self = this;

  // $.level = 1; debugger;

  self.index      = str.readInt16();

  self.shiftKey   = str.readBoolean();
  self.commandKey = str.readBoolean();
  self.colorIndex = str.readInt16();

  // this is to work around some partially corrupt atn files
  var nm          = localize(str.readUnicode());
  self.name       = nm.replace(/\x00/, '');
  self.expanded   = str.readBoolean();

  ActionStream.log("Action.read(" + self.name + ")");

  var items = [];
  var len = str.readWord();

  for (var i = 0; i < len; i++) {
    var ai = new ActionItem();
    ai.parent = self;
    ai.read(str);
    items.push(ai);
  }

  self.actionItems = items;
  self.count = self.actionItems.length;

  // ActionStream.log("Action.read(" + self.name + ") - Done");

  return self;
};
Action.prototype.readDroplet = function(str) {
  var self = this;

//   $.level = 1; debugger;

  ActionStream.log("Action.readDroplet()");

  //XXX add a search in here for 8BDR instead of the magic offset

  var m = str.str.match(/8BDR/);

  if (!m) {
    Error.runtimeError(ATN_ERR,
                       "Bad droplet format. Unable to extract action.");
  }

  var ofs = m.index + 27;

  str.seek(ofs);

  var len = str.readWord(); // check for a bad offset (PS version problem)

  if (len > 100) {
    Error.runtimeError(ATN_ERR,
                       "Bad droplet format. Unable to extract action.");
  }

  str.ptr -= 4;

  self.name = localize(str.readUnicode());
  str.readByte();     // some odd byte....

  var items = [];
  var len = str.readWord();

  for (var i = 0; i < len; i++) {
//     try {
      var ai = new ActionItem();
      ai.parent = self;
      ai.read(str);
      items.push(ai);
//     } catch (e) {
//       $.level = 1; debugger;
//     }
  }

  self.actionItems = items;
  self.count = self.actionItems.length;

  ActionStream.log("Action.readDroplet() - Done");

  return self;
};
Action.prototype.readFromPalette = function(name, atnSet) {
  var self = this;

  if (!Stdlib.hasAction(name, atnSet)) {
    Error.runtimeError(ATN_ERR, "Error: Action " + atnSet + ':' + name + " is not available.");
  }
  var fptr = new File(Folder.temp + '/' + name.replace(/\W+/g, '_') + ".exe");

  try {
    Stdlib.createDroplet(name, atnSet, fptr);

    if (isMac()) {
      var f = File(fptr + "/Contents/Resources/Droplet.8BDR");
      if (!f.exists) {
        f = File(fptr + "/Contents/MacOS/Droplet");
      }
      if (f.exists) {
        fptr = f;
      }
    }

    var str = Stream.readStream(fptr);
    self.readDroplet(str);

  } catch (e) {
    alert(Stdlib.exceptionMessage(e));
    Stdlib.logException(e);

  } finally {
    // XXX Add code to remove entire .app tree on the Mac...
    try { fptr.remove(); } catch (e) { Stdlib.logException(e); }
  }
};
Action.prototype.loadRuntime = Action.prototype.readFromPalette;

Action.prototype.readRuntime = function(desc) {
  var self = this;

  // most of these properties cannot be retrieved from the runtime palette

  if (desc.hasKey(PSKey.Name)) {
    self.name = localize(desc.getString(PSKey.Name));
  }
  if (desc.hasKey(PSKey.NumberOfChildren)) {
    self.count = desc.getInteger(PSKey.NumberOfChildren);
  }
  if (desc.hasKey(PSKey.ShiftKey)) {
    self.shiftKey = desc.getInteger(PSKey.ShiftKey);
  }
  if (desc.hasKey(PSKey.CommandKey)) {
    self.commandKey = desc.getInteger(PSKey.CommandKey);
  }
  if (desc.hasKey(PSKey.Color)) {
    self.colorIndex = desc.getInteger(PSKey.Color);
  }
//   if (desc.hasKey(PSKey.Expanded)) {
//     self.expanded = desc.getInteger(PSKey.Expanded);
//   }
  var items = [];
  var max = self.count;

  for (var i = 0; i < max; i++) {
    var ai = new ActionItem();
    ai.parent = self;
  }
  self.actionItems = items;
  self.count = self.actionItems.length;
  return;
};
Action.prototype.write = function(str) {
  var self = this;

  ActionStream.log("Action.write(" + self.name + ")");

  str.writeInt16(self.index);
  str.writeBoolean(self.shiftKey);
  str.writeBoolean(self.commandKey);
  str.writeInt16(self.colorIndex);
  str.writeUnicode(self.name);
  str.writeBoolean(self.expanded);

  var items = self.actionItems;
  str.writeWord(items.length);

  for (var i = 0; i < items.length; i++) {
    var ai = items[i];
    ai.write(str);
  }

  // ActionStream.log("Action.write(" + self.name + ") - Done");
};
Action.prototype.add = function(item) {
  var self = this;

  self.actionItems.push(item);
  self.count = self.actionItems.length;
  return item;
};

//=============================== ActionItem ==================================

ActionItem.prototype.read = function(str) {
  var self = this;

  self.expanded = str.readBoolean();
  self.enabled = str.readBoolean();
  self.withDialog = str.readBoolean();
  self.dialogOptions = str.readByte();

  self.identifier = str.readString(4);
  if (self.identifier == ActionItem.TEXT_ID) {
    self.event = str.readAscii();
  } else if (self.identifier == ActionItem.LONG_ID) {
    self.itemID = str.readWord();
  } else {
    Error.runtimeError(ATN_ERR, "Bad ActionItem definition: ActionItem.id");
  }

  self.name = str.readAscii();
  self.hasDescriptor = (str.readSignedWord() == -1);

  //Stdlib.fullStop();
  if (self.hasDescriptor) {
    // hack, hack
    //str.ptr -= 4;
    //str.writeWord(0x10); // spoof the version in this part of this stream
    //log.writeln("--> " + self.name);
    var size = 500;
    var ptr = str.ptr;
    while (true) {
      var xstr = new Stream();
      xstr.writeWord(0x10);
      //log.writeln("slice");
      // copy from version to end
      var bytes = str.str.slice(str.ptr, Math.min(str.ptr + size,
                                                  str.str.length));
      //log.writeln("writeRaw");
      xstr.writeRaw(bytes);
      delete bytes;
      //log.writeln("fromStream");
      self.descriptor = new ActionDescriptor();
      if (self.descriptor.fromStream == undefined) {
        Error.runtimeError(ATN_ERR,
          "\rDescriptor.fromStream not defined for this version " +
          "of Photoshop. This currenly only works with CS2. A CS version " +
          "may become available in the future.");
      }
      try {
        self.descriptor.fromStream(xstr.str.join(""));  // read it
        break;
      } catch (e) {
        if (str.ptr + size > str.str.length) {
          Error.runtimeError(ATN_ERR, "Failed reading " + self.name);
        }
        size *= 2;
        str.ptr = ptr;
      }
    }

    var cpstr = self.descriptor.toStream(); // find out how long it really was
    str.ptr += (cpstr.length - 4);      // advance the stream pointer
    //log.writeln("<-- " + self.name);
  }
};
ActionItem.prototype.write = function(str) {
  var self = this;

  //debugger;
  str.writeBoolean(self.expanded);
  str.writeBoolean(self.enabled);
  str.writeBoolean(self.withDialog);
  str.writeByte(self.dialogOptions);

  str.writeString(self.identifier);
  if (self.identifier == ActionItem.TEXT_ID) {
    str.writeAscii(self.event);
  } else if (self.identifier == ActionItem.LONG_ID) {
    str.writeWord(self.itemID);
  } else {
    Error.runtimeError(ATN_ERR, "Bad ActionItem definition: ActionItem.identifier");
  }

  str.writeAscii(self.name);

  // hasDescriptor
  str.writeWord(self.descriptor ? -1 : 0);

  if (self.descriptor) {
    // ActionDescript.toStream() can fail if the descriptor is empty
    if (self.descriptor.count > 0) {
      // $.level = 1; debugger;
      // var str = new Stream();
      // self.descriptor.writeToStream(str);
      // var bytes = str.str;

      var bytes = self.descriptor.toStream();

      // slice off the version number
      str.writeRaw(bytes.slice(4));

    } else {
      str.writeUnicode("");
      str.writeAsciiOrKey(cTID('null'));   // object type
      str.writeWord(0);
    }
  }
};

var atn2bin_test;
if (atn2binTestMode && !atn2bin_test) {
  eval('//@include "xlib/xml/atn2bin-test.jsx";\r');
}

"atn2bin.jsx";
// EOF
