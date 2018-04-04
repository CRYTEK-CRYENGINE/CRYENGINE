#target photoshop
//
// ActionManager
//
// $Id: ActionManager.jsx,v 1.4 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//-include "xlib/PSConstants.js"
//-include "xlib/stdlib.js"
//-include "xlib/Stream.js"
//-include "xlib/Action.js"
//-include "xlib/xml/atn2bin.jsx"
//-include "xlib/xml/atn2js.jsx"
//-include "xlib/ActionStream.js"
//-include "xlib/ieee754.js"
//

ActionManager = function() {
};

ActionManager.prototype.saveRuntimePalette = function(fptr) {
  var self = this;

  var actFile = ActionFile.readFrom("/c/work/scratch/Work.atn");

  // This code loads and stores the entire ActionsPalette
  if (false) {
    var pal = new ActionsPalette();
    pal.loadRuntime();
  
    var palf = new ActionsPaletteFile();
    palf.actionsPalette = pal;
    palf.write(fptr);
  }

  // This code loads and stores the CSX ActionSet as a .atn file.
  if (true) {
    var asetName = "CSX";
    var asets = Stdlib.getActionSets();
    var aset = Stdlib.getByName(asets, asetName);
    
    if (!aset) {
      alert("Unable to find ActionSet: " + asetName);
      return;
    }
    
    var actSet = new ActionSet();
    actSet.loadRuntime(aset.name, aset.index);
    
    var actFile = new ActionFile();
    actFile.actionSet = actSet;
    actFile.write("/c/work/scratch/" + asetName + "-x.atn");
  }
};

function main() {
  var am = new ActionManager();
  am.saveRuntimePalette("/c/tmp/test.psp");
};
main();

"ActionManager.jsx";
// EOF
