//
// ActionSelector.js
//
//
//@show include
//

ActionSelector = function() {};

ActionSelector.fileUI =
"dialog{text:'Select an Action',bounds:[100,100,490,300]," +
"		statictext0:StaticText{bounds:[10,10,250,27] , text:'Select an Action from a File ' ,}," +
"		ok:Button{bounds:[70,140,170,160] , text:'OK ' }," +
"		cancel:Button{bounds:[250,140,350,160] , text:'Cancel ' }," +
"		statictext2:StaticText{bounds:[15,80,90,97] , text:'Action: '}," +
"		actionDropdown:DropDownList{bounds:[120,80,270,105]}," +
"		browseBtn:Button{bounds:[10,40,110,60] , text:'Browse...' }," +
"		fileText:EditText{bounds:[120,40,370,65] , text:'' ,properties:{multiline:false,noecho:false,readonly:true}}" +
"}";

ActionSelector.prototype.loadActionFile = function(infile) {
  var infile = Stdlib.convertFptr(infile);
  var fname = infile.name.toLowerCase();
  var atnFile;

  if (fname.endsWith(".xml")) {
    var xmlstr = Stdlib.readFromFile(infile);
    atnFile = ActionFile.deserialize(xmlstr);

  } else if (fname.endsWith(".atn")) {
    var atnFile = new ActionFile();
    atnFile.read(infile);

  } else {
    throw "Unknow file type. Cannot load action file";
  }
  return atnFile;
};

ActionSelector.prototype.loadAction = function(infile, name) {
  var self = this;
  var atnFile;
  var act;

  atnFile = self.loadActionFile(infile);
  if (atnFile) {
    act = atnFile.actionSet.getByName(name);
  }
  return act;
};

ActionSelector.prototype.getFile = function() {
    var self = this;
    if (self.folder) {
      self.oldCurrent = Folder.current;
      Folder.current = self.folder;
    }
    var fmask =  "Action Files: *.atn;*.xml";
    var file = File.openDialog("Select an Action or XML file", fmask);

    if (self.oldCurrent) {
      Folder.current = self.oldCurrent;
      self.oldCurrent = undefined;
    }
    if (!file) {
      self.folder = file.parent;
    }
    return file;
};

ActionSelector.prototype.getActionFromFile = function(infile, browse) {
  var self = this;
  var win = new Window(ActionSelector.fileUI);

  win.actionFiles = [];
  win.owner = self;

  win.browseBtn.onClick = function(file) {
    if (!file) {
      file = win.owner.getFile();
    }
    if (!file) {
      return; // win.cancel.onClick();
    }
    win.fileText.text = decodeURI(file);
    win.actionDropdown.removeAll();

    var atnFile = win.actionFiles[file.toString()];
    if (!atnFile) {
      try {
        atnFile = win.owner.loadActionFile(file);
      } catch (e) {
        alert("Error reading Action file: " + e);
        return;
      }
      win.actionFiles[file.toString()] = atnFile;
    }

    var acts = atnFile.actionSet.actions;
    if (acts.length) {
      for (var i = 0; i < acts.length; i++) {
        win.actionDropdown.add("item", acts[i].name);
      }
      win.actionDropdown.items[0].selected = true;
    }
  }

  win.ok.onClick = function() {
    var file = new File(win.fileText.text);
    var act = win.actionDropdown.selection.text;
    win.results = { actionFile: file,
                    action: act };
    win.close(1);
  }

  win.cancel.onClick = function() {
    win.close(1);
  }

  if (infile) {
    if (browse != true) {
      win.browseBtn.enabled = false;
    }
    win.browseBtn.onClick(infile);
  }

  win.center();
  win.show();

  return win.results;
};

ActionSelector.runtimeUI =
"dialog{text:'Select an Action',bounds:[100,100,400,260]," +
"		statictext0:StaticText{bounds:[10,10,200,27] , text:'Select an Action' ,properties:{scrolling:undefined,multiline:undefined}}," +
"		statictext1:StaticText{bounds:[10,40,90,57] , text:'Action Set:' ,properties:{scrolling:undefined,multiline:undefined}}," +
"		ok:Button{bounds:[40,120,140,140] , text:'OK' }," +
"		cancel:Button{bounds:[160,120,260,140] , text:'Cancel' }," +
"		statictext2:StaticText{bounds:[10,70,90,87] , text:'Action:' ,properties:{scrolling:undefined,multiline:undefined}}," +
"		setDropdown:DropDownList{bounds:[100,40,290,63]}," +
"		actionDropdown:DropDownList{bounds:[100,70,290,93]}" +
"}";

ActionSelector.prototype.getActionFromRuntime = function() {
  var self = this;
  var pal = new ActionsPalette();
  pal.readRuntime();

  var win = new Window(ActionSelector.runtimeUI);

  win.owner = self;

  var sets = pal.actionSets;
  for (var i = 0; i < sets.length; i++) {
    win.setDropdown.add("item", sets[i].name);
  }
  win.setDropdown.items[0].selected = true;
  win.setDropdown.selection = win.setDropdown.items[0];

  win.setDropdown.onChange = function() {
    var self = this;

    win.actionDropdown.removeAll();
    var acts = sets[self.selection.index].actions;
    for (var i = 0; i < acts.length; i++) {
      win.actionDropdown.add("item", acts[i].name);
    }
    if (acts.length) {
      win.actionDropdown.items[0].selected = true;
    }
  }

  win.ok.onClick = function() {
    var aset = win.setDropdown.selection.text;
    var act = win.actionDropdown.selection.text;
    win.results = { action: act,
                    actionSet: aset };
    win.close(1);
  }
  win.cancel.onClick = function() {
    win.close(1);
  }

  if (CSVersion() > 2) {
    win.setDropdown.onChange();
  }

  if (self.parent) {
    win.center(self.parent);
  }
  win.center();
  win.show();

  return win.results;
};


//
//-includepath "/c/Program Files/Adobe/xtools"
//
//-include "xlib/PSConstants.js"
//-include "xlib/stdlib.js"
//-include "xlib/Stream.js"
//-include "xlib/Action.js"
//-include "xlib/xml/atn2bin.jsx"
//-include "xlib/xml/atn2xml.jsx"
//
function demo() {
   var selector = new ActionSelector();
   var res;

   // Select an action from the runtime palette
   if (true) {
     res = selector.getActionFromRuntime();
     if (res) {
       alert("actionSet: " + res.actionSet + '\r' +
             "action    : " + res.action);
     } else {
       alert("No action selected");     res = selector.getActionFromRuntime();

     }
   }

   // Select an action from an action file
   if (true) {
     res = selector.getActionFromFile();
     if (res) {
       alert("actionFile: " + res.actionFile + '\r' +
             "action     : " + res.action);
     } else {
       alert("No action selected");
     }
   }

   // Select an action from an XML file
   if (false) {
     res = selector.getActionFromFile(new File("/c/work/XActions.xml"));
     if (res) {
       alert("actionFile: " + res.actionFile + '\r' +
             "action     : " + res.action);
     } else {
       alert("No action selected");
     }
   }
};

//demo();

"ActionSelector.jsx";
// EOF
