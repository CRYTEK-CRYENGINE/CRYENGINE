//
// SLCParser.jsx
//
// $Id: SLCParser.jsx,v 1.12 2010/05/04 01:46:12 anonymous Exp $
// Copyright: (c)2009, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

SLCItem = function() {
  var self = this;

  self.literal = '';    // the literal code from the log
  self.src = '';        // the cleansed code

  self.desc = '';       // the ActionDescriptor
  self.event = '';
  self.eid = '';
  self.mode = '';
};
SLCItem.prototype.typename = "SLCItem";

SLCItem.create = function(literal, src) {
  var item = new SLCItem();
  item.literal = literal;
  item.src = src;
  return item;
};
SLCItem.prototype.toString = function() {
  var self = this;
  var str = '[SLCItem\n';

  if (self.literal) {
    str += 'Literal:\n\t' + self.literal.replace(/\n/g, "\n\t") + '\n';
  }
  if (self.src) {
    str += 'Source:\n\t' + self.src.replace(/\n/g, "\n\t") + '\n';
  }

  str += 'Descriptor:' + (!!(self.desc)) + '\n';
  str += 'Event:' + self.event + '\n';
  str += 'eid:' + self.eid + '\n';
  str += 'Mode:' + self.mode + '\n';

  str += ']\n';

  return str;
};

SLCParser = function() {
  var self = this;

  self.idMap = {};       // maps ids to names
                         // e.g. id22 -> cTID("Rtte") or PSEvent.Rotate
  self.nameMap = {};     // maps syms to names, e.g. "Rtte" -> PSEvent.Rotate

  self.ftnIndex = 1;
  self.useFtns = true;

  self.first = true;     // init flag

  self.ftnNameFmt = "step_%02d";

  self.genSource = true; // not yet used

  self.items = [];

  self.errorCnt = 0;
};
SLCItem.prototype.typename = "SLCParser";

SLCParser.checkPSConstants = function() {
  try {
    eval("PSConstants");
    return true;
  } catch (e) {
    return false;
  }
};

SLCParser.usePSConstants = SLCParser.checkPSConstants();
SLCParser.insertFtnCalls = false;

SLCParser.executeAction =  function(eid, desc, mode) {
  var item = SLCParser.item;

  Stdlib.log('Adding item: event: ' + id2char(eid, "Event"));

  item.eid = eid;
  item.desc = desc;
  item.mode = mode;
  var event = undefined;
  try {
    event = app.typeIDToCharID(eid);
  } catch (e) {
  }
  if (!event) {
    try {
      event = app.typeIDToStringID(eid);
    } catch (e) {
    }
  }
  item.event = event || '';
  item.name = id2char(eid, "Event");
};


SLCParser.prototype.addItem = function(lit, src) {
  var self = this;

  var lstr = lit.join('\n');
  var sstr = src.join('\n');
  var item = SLCItem.create(lstr, sstr);

  SLCParser.item = item;
  var estr = sstr.replace("executeAction", "SLCParser.executeAction");

  try {
    //
    // PS doesn't always handle File objects intelligently when the
    // file does not exist or if the path in native.
    // The code here and in the exception handler below
    // try to deal with this somewhat intelligently by moving the path
    // to the Desktop. This should take care of most cases
    //
    if (estr.contains('new File')) {
      try {
        // This only handles cases with a single path
        var rex = /new File\( "(.*)" \)/;
        var m = estr.match(rex);
        if (m) {
          var fname = m[1];
          var file = File(fname);

          if (!file.parent.exists || !file.exists) {
            var parts = decodeURI(fname).split(/\/|\\/);
            if (parts.length > 1) {
              file = File(parts.pop());
            }

            estr = estr.replace(rex, 'new File( "' + Folder.desktop + '/' +
                                file.name + '" )');
          }
        }
      } catch (e) {
      }
    }

    eval(estr);
    self.items.push(item);

  } catch (e) {
    var hasError = true;
    // check for path related errors here and deal with them

    if (estr.contains('new File')) {
      try {
        // This only handles cases with a single path
        var rex = /new File\( "(.*)" \)/;
        var m = estr.match(rex);
        if (m) {
          var file = File(m[1]);
          if (!file.parent.exists) {
            estr = estr.replace(rex, 'new File( "~/Desktop/' + file.name + '" )');
            eval(estr);
            self.items.push(item);
            hasError = false;
          }
        }
      } catch (e) {
      }
    }

    if (hasError) {
      Stdlib.logException(e, estr);
      self.errorCnt++;
    }
  }
};

SLCParser.prototype.nextFunctionName = function() {
  var self = this;
  var idx = self.ftnIndex++;
  var ftnName;

  if (self.ftnNameFmt.contains('%')) {
    ftnName = self.ftnNameFmt.sprintf(idx);
  } else {
    ftnName = self.ftnNameFmt + idx;
  }

  return ftnName;
};

SLCParser.prototype.nextFunction = function() {
  var self = this;
  var ftnName = self.nextFunctionName();

  var str = "function " + ftnName + "() {\n";
  str += "  function cTID(s) { return app.charIDToTypeID(s); };\n";
  str += "  function sTID(s) { return app.stringIDToTypeID(s); };\n";

  return str;
};

SLCParser.prototype.mapSym = function(idName, sym, ftn) {
  var self = this;
  var v;

  sym = sym.replace(/\'|\"/g, '');  // trim and remove any quotes

//   if (sym == 'pointsUnit') {
//     $.level = 1; debugger;
//   }
  if (SLCParser.usePSConstants) {
    v = self.nameMap[sym];
    if (!v) {
      var id = eval(ftn + "('" + sym + "')");
      if (ftn == 'sTID') {
        var v = PSString._reverseName[id];
        if (v) {
          v = "PSString." + v;
        } else {
          v = "sTID('" + sym + "')";
        }
      } else {
        var tbl = PSConstants.symbolTypes;

        for (var name in tbl) {
          var kind = tbl[name];
          v = kind._reverseName[id];
          if (v) {
            v = "PS" + kind._name + "." + v;
            break;
          }
        }
        if (!v) {
          if (sym.length > 4) {
            ftn = 'sTID';
          }
          v = ftn + "('" + sym + "')";
        }
        if (v.endsWith(".null")) {
          v = "cTID('null')";
        }
      }
      self.nameMap[sym] = v;
    }

  } else {
     v = ftn + "('" + sym + "')";
  }
  self.idMap[idName] = v;
};

SLCParser.prototype.header = function() {
  var self = this;
  var str = '';
  str += "//\n";
  str += "// Generated from " + self.infile.absoluteURI + " on " + Date() + "\n";
  str += "//\n";
  return str.split('\n');
};

SLCParser.prototype.trailer = function() {
  var self = this;
  var str = '';

  str += "cTID = function(s) { return app.charIDToTypeID(s); };\n";
  str += "sTID = function(s) { return app.stringIDToTypeID(s); };\n\n";

  str += "function _initDefs() {\n";
  str += ("  var needDefs = true;\n" +
          "  try {\n" +
          "    PSClass;\n" +
          "    needDefs = false;\n" +
          "  } catch (e) {\n" +
          "  }\n");

  str += "  if (needDefs) {\n";

  var tbl = PSConstants.symbolTypes;
  for (var name in tbl) {
    var kind = tbl[name];
    str += "    PS" + kind._name + " = function() {};\n";
  }
  str += "  }\n};\n\n";

  str += "_initDefs();\n\n";

  var names = [];
  for (var sym in self.nameMap) {
    var n = self.nameMap[sym];
    if (n.startsWith("cTID(") || n.startsWith('sTID')) {
      continue;
    }
    var idk = (n.startsWith("PSString") ? 'sTID' : 'cTID');
    names.push(n + " = " + idk + "('" + sym + "');\n");
  }
  names.sort();
  str += names.join("");
  return str;
};


SLCParser.prototype.exec = function(infile, outfile) {
  var self = this;
  self.symIDMap = {};
  self.infile = infile;
  self.items = [];
  self.multiline = false;

  if (!infile.encoding) {
    infile.encoding = (isWindows() ? 'CP1252' : 'MACINTOSH');
  }

  var proc = new TextProcessor(infile, outfile, SLCParser.handleLine);
  proc.parent = this;
  proc.list = [];
  proc.literal = [];
  proc.src = [];
  proc.exec();

  return proc.list;
};

SLCParser.handleLine = function(line, index, outputBuffer) {
  var self = this;
  var parser = self.parent;

  if (parser.first) {
    // if (line.length == 0) { return TextProcessorStatus.OK; }
    // this new bit of code should fix parsing of SL log segments that
    // do not have a // ======= prefix
    parser.first = false;

    if (parser.useFtns) {
      outputBuffer.push(parser.nextFunction());
      if (line.startsWith("// ========")) {
        self.literal.push(line);
        outputBuffer.push(line);
        return TextProcessorStatus.OK;
      }
    }
  }

  // At the end of the file, print out the trailier containing
  // all of the symbol table information
  if (line == undefined) {  // EOF
    if (SLCParser.usePSConstants) {
      var str = parser.trailer();
      var ar = str.split('\n');
      for (var i = ar.length-1; i >= 0; i--) {
        outputBuffer.unshift(ar[i]);
      }
    }

    // now, for some odd reason, we print out the header block
    var ar = parser.header();
    for (var i = ar.length-1; i >= 0; i--) {
      outputBuffer.unshift(ar[i]);
    }

    return TextProcessorStatus.OK; // EOF
  }

  // pass empty lines through
  if (line == '') {
    var txt = '';
    outputBuffer.push(txt);

    self.literal.push(txt);
    self.src.push(txt);

    return TextProcessorStatus.OK;
  }

  self.literal.push(line);

  // handle a charID variable definition
  var m;
  if ((m = line.match(/\s*var (id\w+) = charIDToTypeID\((.+)\);/)) != null) {
    parser.mapSym(m[1], m[2].trim(), "cTID");
    return TextProcessorStatus.OK;
  }

  // handle a stringID variable definition
  if ((m = line.match(/\s*var (id\w+) = stringIDToTypeID\((.+)\);/)) != null){
    parser.mapSym(m[1], m[2].trim(), "sTID");
    return TextProcessorStatus.OK;
  }

  // swap out the SL var usages with our symbols
  if ((m = line.match(/\Wid\w+/g)) != null) {
    for (var i = 0; i < m.length; i++) {
      var id = m[i].replace(/^\W/, '');
      line = line.replace(id, parser.idMap[id]);
    }
  }

  // Fix up the mangled File references
  var fps;
  if ((fps = line.match(/new File\((.+)\)/)) != null) {
     line = line.replace(fps[1], fps[1].replace(/\\\\?/g, '/'));
  }

  var xps;
  if ((xps = line.match(/\.putString.*"<\?xpacket begin="/)) != null) {
    line = line.replace(/\"<\?xpacket/, "\'<xpacket") + "\' +";
    self.multiline = true;

  } else if (line.match(/<\?xpacket end="w"\?>"/)) {
    line = line.replace(/<\?xpacket end="w"\?>"/, "'<?xpacket end=\"w\"?>'");
    self.multiline = false;

  } else if (self.multiline) {
    line = "'" + line + "' +";
  }

  // Look for the beginning (and ending of) SL code segments
  if (parser.useFtns) {
    if (line.startsWith("// ========")) {
      outputBuffer.push(line);
      self.src.push(line);
      line = parser.nextFunction();
      // XXX if src has functions self.src.push(line);

    } else if (line.match("executeAction")) {
      var txt = "    " + line;
      outputBuffer.push(txt);
      self.src.push(txt);

      line = "};";
      // XXX if src has functions self.src.push(line);

      parser.addItem(self.literal, self.src);
      self.literal = [];
      self.src = [];

      if (SLCParser.insertFtnCalls) {
        outputBuffer.push(line);

        line = "ftn" + (parser.ftnIndex-1) + "();";
      }
    } else {
      self.src.push(line);
    }
  }

  outputBuffer.push(line);

  return TextProcessorStatus.OK;
};

SLCParser.test = function() {
  var start = new Date().getTime();

  var src = new File("~/Desktop/ScriptingListenerJS.log");
  var dest = new File("~/Desktop/ScriptingListenerJS.txt");

  SLCParser.usePSConstants = false;

  var parser = new SLCParser();
  parser.exec(src, undefined);

  var stop = new Date().getTime();
  var elapsed = (stop - start)/1000;
  alert("Done (" + Number(elapsed).toFixed(3) + " secs).");
};


//
//-include "xlib/stdlib.js"
//-include "xlib/TextProcessor.js"
//-include "xlib/PSConstants.js"
//-include "xlib/Stream.js"
//-include "xlib/Action.js"
//-include "xlib/ActionStream.js"
//-include "xlib/xml/atn2bin.jsx"
//

SLCParser.main = function() {
  Stdlib.log = function(msg) {
    $.writeln(msg);
  };
  SLCParser.test();
};

// SLCParser.main();

"SLCParser.jsx";
// EOF
