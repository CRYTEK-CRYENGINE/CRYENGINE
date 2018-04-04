//
// PatternFile
//
// $Id: PatternFile.js,v 1.12 2015/01/14 03:33:45 anonymous Exp $
// Copyright: (c)2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//

PatternFile = function() {
  var self = this;
  self.file = null;
  self.patterns = [];
};
PatternFile.prototype.typename = "PatternFile";

PatternFile.psVersion = parseInt(app.version);

PatternFile.readFile = function(file) {
  file.open("r") || throwFileError(file, "Unable to open input file ");
  file.encoding = 'BINARY';
  var str = file.read();
  file.close();
  return str;
};

PatternFile.prototype.toString = function() {
  var self = this;
  var str = '';
  var len = self.patterns.length;
  for (var i = 0; i < len; i++) {
    str += self.patterns[i] + ",\r\n";
  }

  return str;
};


PatternFile.prototype.read = function(fptr) {
  var self = this;

  self.file = (fptr instanceof File) ? fptr : new File(fptr);
  var str = PatternFile.readFile(self.file);

  var re = /(\x00\w|\x00\d)(\x00\-|\x00\w|\x00\s|\x00\d)+\x00\x00\$[-a-z\d]+/g;
  var parts = str.match(re);

  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    var sp = p.replace(/\x00/g, '').split('$');
    var pattern;

    if (PatternFile.psVersion >= 11) {
      pattern = new Pattern(sp[0], sp[1]);
    } else {
      pattern = new Pattern(sp[0], '$' + sp[1]);
    }

    self.patterns.push(pattern);
    self[pattern.name] = pattern.id;
  }
};

Pattern = function(name, id) {
  var self = this;
  self.name = name;
  self.id = id;
};
Pattern.prototype.typename = "Pattern";
Pattern.prototype.toString = function() {
  return "{ name : \"" + this.name + "\", id : \"" + this.id + "\" }";
};

Pattern.fillPattern = function(doc, name, id) {
  function cTID(s) { return app.charIDToTypeID(s); };

  var desc203 = new ActionDescriptor();
  desc203.putEnumerated( cTID('Usng'), cTID('FlCn'), cTID('Ptrn') );

  var desc204 = new ActionDescriptor();
  if (name) {
    desc204.putString( cTID('Nm  '), name);
  }
  if (id) {
    desc204.putString( cTID('Idnt'), id);
  }

  desc203.putObject( cTID('Ptrn'), cTID('Ptrn'), desc204 );
  desc203.putUnitDouble( cTID('Opct'), cTID('#Prc'), 100.000000 );
  desc203.putEnumerated( cTID('Md  '), cTID('BlnM'), cTID('Nrml') );
  executeAction( cTID('Fl  '), desc203, DialogModes.NO );
};

Pattern.prototype.fill = function(doc) {
  var self = this;

  Pattern.fillPattern(doc, self.name, self.id);
};


//
// This test function reads all of the files found in Presets/Patterns
// and attempts to fill a new layer the active document with the first
// pattern in the file. It will fail if that pattern has not been loaded
//
PatternFile.test = function() {
  function waitForRedraw() {
    function cTID(s) { return app.charIDToTypeID(s); };
    var desc = new ActionDescriptor();
    desc.putEnumerated(cTID("Stte"), cTID("Stte"), cTID("RdCm"));
    executeAction(cTID("Wait"), desc, DialogModes.NO);
  };

  var patternFolder = new Folder(app.path + "/Presets/Patterns");
  var patternFiles = patternFolder.getFiles("*.pat");
  var doc = (app.documents.length > 0) ? app.activeDocument : null;

  for (var i = 0; i < patternFiles.length; i++) {
    var pfile = patternFiles[i];
    var patternSet = new PatternFile();
    patternSet.read(pfile);

    if (doc) {
      try {
        var p = patternSet.patterns[0];
        doc.artLayers.add();
        doc.activeLayer.name = p.name + ' - ' + p.id;
        p.fill(doc);
        waitForRedraw();

      } catch (e) {
        doc.activeLayer.remove();
        alert("Unable to fill pattern: " + p.toString());
      }
    }
    
    var str = decodeURI(pfile.name) + "\r\n";
    str += patternSet.toString();
    alert(str);
  }
};

//PatternFile.test();

"Patterns.js";
// EOF
