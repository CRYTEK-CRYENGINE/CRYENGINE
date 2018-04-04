//
// ListSortDialog
// This file constains a panel for sorting a list of items.
// A 'data' property is kept with each element in the list
//
// $Id: ListSortDialog.jsx,v 1.7 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//app;
//-include "xlib/stdlib.js"
//-include "xlib/GenericUI.jsx"
//

GenericUI.prototype.createListSortPanel = function(pnl, ini, list) {
  var win = GenericUI.getWindow(pnl);

  var panelW = pnl.bounds.width;
  var panelH = pnl.bounds.height;
  var btnW = 25;
  var btnH = 30;
  var ofs = 20;

  var listText = [];
  for (var i = 0; i < list.length; i++) {
    listText.push(list[i]);
  }

  pnl.list = pnl.add('listbox',
                     [ofs,ofs,panelW-2*ofs-btnW,panelH-2*ofs],
                     listText);

  pnl.list.onChange = function() {
    var pnl = this.parent;

    var st = (pnl.list.selection != null); // is something selected
    if (st) {
      var idx = pnl.list.selection.index;
      pnl.buttonUp.enabled = idx != 0; 
      pnl.buttonDown.enabled = idx != (pnl.list.items.length-1);
    } else {
      pnl.buttonUp.enabled = pnl.buttonDown.enabled = false;
    }
  }

  pnl.buttonUp = pnl.add('button',
                         [panelW-ofs-btnW,
                          (panelH/2)-btnH,
                          panelW-ofs,
                          (panelH/2)],
                         '^');

  pnl.buttonDown = pnl.add('button',
                           [panelW-ofs-btnW,
                            (panelH/2),
                            panelW-ofs,
                            (panelH/2)+btnH],
                           'v');

  if (isMac()) {
    win.bounds.width += 25;
    pnl.bounds.width += 25;
  }

  pnl.buttonUp.onClick = function() {
    var pnl = this.parent;
    var sel = pnl.list.selection;
    if (!sel) {
      return;
    }
    var idx = sel.index;
    if (idx == 0) {
      return;
    }
    var text = sel.text;
    pnl.list.remove(idx);
    pnl.list.add('item', text, idx-1);
    pnl.list.items[idx-1].selected = true;
  }

  pnl.buttonDown.onClick = function() {
    var pnl = this.parent;
    var sel = pnl.list.selection;
    if (!sel) {
      return;
    }
    var items = pnl.list.items;
    var idx = sel.index;
    if (idx == (items.length-1)) {
      return;
    }
    var text = sel.text;
    pnl.list.remove(idx);
    pnl.list.add('item', text, idx+1);
    pnl.list.items[idx+1].selected = true;
  }

  pnl.list.onChange();

  return pnl;
};
GenericUI.prototype.validateListSortPanel = function(pnl, ini, name) {
  var self = this;

  var listText = [];
  var items = pnl.list.items;
  for (var i = 0; i < items.length; i++) {
    listText.push(items[i].text);
  }
  ini[name] = listText;

  return ini;
};

ListSortDialog = function(cwin) {
  var self = this;

  self.title = "List Sort Dialog";
  self.notesSize = 0;
  self.winRect = {
    x: 100, 
    y: 100,
    w: 300,
    h: 400
  };
  self.documentation = undefined;
  self.iniFile = undefined;
  self.saveIni = false;
  self.hasBorder = false;
  self.center = false;
  self.cwin = cwin;

  self.processTxt = "OK";
};

ListSortDialog.prototype = new GenericUI();

ListSortDialog.prototype.createPanel = function(pnl, ini, list) {
  var self = this;

  if (!list) {
    list = ini[self.listName];

    if (list && list.constructor == String) {
      list = list.split(',');
    }
  }

  self.createListSortPanel(pnl, ini, list);

//   if (self.cwin) {
//     var w = GenericUI.getWindow(pnl);
//     self.cwin.center(w);
//   }

  return pnl;
};

ListSortDialog.prototype.validatePanel = function(pnl, opts) {
  var self = this;

  return self.validateListSortPanel(pnl, opts, self.listName);
};


ListSortDialog.test = function() {
  var ui = new ListSortDialog();
  var opts = {};

  opts.testSet = ["set1","set4","set6","set7"];

  ui.listName = "testSet";

  ui.process = function(opts) {
    ui.sortedList = opts.testSet;
    //alert(listProps(opts));
  }
  ui.sortedList = undefined;
  ui.exec(opts);
  alert(ui.sortedList);
};

//ListSortDialog.test();

"ListSortDialog.jsx";
// EOF
