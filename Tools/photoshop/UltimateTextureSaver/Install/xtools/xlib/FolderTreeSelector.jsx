//
// FolderTreeSelector.jsx
//
// $Id: FolderTreeSelector.jsx,v 1.5 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2009, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//


FolderTreeSelector = function() {

};
FolderTreeSelector.DEFAULT_BOUNDS = [200, 200, 700, 700];

FolderTreeSelector.prototype.typename = "FolderTreeSelector";

FolderTreeSelector.createDialog = function(rootFolder, bnds) {

  if (!rootFolder) {
    Error.runtimeError(2, "rootFolder");
  }

  if (!(rootFolder instanceof Folder)) {
    rootFolder = new Folder(rootFolder.toString());
  }

  if (!rootFolder.exists) {
    Error.runtimeError(19, "rootFolder");
  }

  if (!bnds) {
    bnds = FolderTreeSelector.DEFAULT_BOUNDS;
  }

  var win = new Window('dialog', 'Folder Selector', bnds);

  win.rootFolder = rootFolder;
  win.orientation = 'column';
  win.alignChildren = 'center';

  win.tree = win.add('treeview', [0, 0, bnds[2]-bnds[0], bnds[3]-bnds[1]-60]);

  win.rootNode = win.tree.add('node', rootFolder.displayName);

  win.tree.onExpand = function(item) {
    var win = this.parent;
    var path = '';

    var it = item;
		while (!(it.parent instanceof TreeView)) {
      var txt = (it.text[0] == '*' ? it.text.substring(1) : it.text);
			path = "/" + txt + path;
			it = it.parent;
		}

    var folder = new Folder(win.rootFolder.toString() + path);

    if (folder instanceof Folder && folder.exists && !folder.alias) {
      var subdirs = folder.getFiles(function(f) { return f instanceof Folder; });
      subdirs.sort();

      var selMark = (item.text[0] == '*' ? '*' : '');

      for (var i = 0; i < subdirs.length; i++) {
        var subdir = subdirs[i];
        var ch = subdir.getFiles(function(f) { return f instanceof Folder; });
        var type = (ch.length == 0) ? 'item' : 'node';
        item.add(type, selMark + subdir.displayName);
      }
    }
  };

  function doubleClick(e) {
    var win = doubleClick.win;
    if (e.detail == 2) {
      var it = win.tree.selection;
      if (it) {
        var txt = it.text;

        if (txt[0] == '*') {
          it.text = txt.substring(1);

        } else {
          // txt = '*' + txt;
          // it.text = txt;

          // make sure the parent nodes are selected
		      while (!(it instanceof TreeView)) {
            if (it.text[0] != '*') {
              it.text = '*' + it.text;
            }
            it = it.parent;
          }
        }
      }
    }
  }
  doubleClick.win = win;

  win.tree.addEventListener('click', doubleClick);

  var w = win.tree.bounds.width;
  var half = w/2;
  var btnW = 70;
  var x = Math.floor(half - btnW/2);
  var y = win.tree.bounds.height + 20;
  var bBnds = [x,y,x+btnW,y+22];

  win.ok = win.add("button", bBnds, "OK" );
  win.ok.onClick = function() {
    var win = this.parent;

    win.folders = FolderTreeSelector.flattenTree(win.tree, win.rootFolder.parent);
    win.close(1);
  };

  return win;
};


FolderTreeNode = function(folder, allsubs) {
  var self = this;

  self.folder = folder;
  self.recurse = allsubs;

  self.toString = function() {
    var self = this;
    return ("[FolderTreeNode " +
            (self.folder ? decodeURI(self.folder.fsName) : "") +
            " " + self.recurse + "]");
  };
};

FolderTreeSelector.flattenNode = function(node, path) {
  var folders = [];

  if (node.text[0] == '*') {
    var f = new Folder(path + '/' + node.text.substring(1));
    var fpath = f.absoluteURI;

    var ftn = new FolderTreeNode(f, true);
    folders.push(ftn);

    if (node.type == 'node') {
      var items = node.items;
      var len = node.items.length;

      if (len > 0) {
        ftn.recurse = false;
        var subs = [];

        for (var i = 0; i < len; i++) {
          var it = items[i];
          subs.push(FolderTreeSelector.flattenNode(it, fpath));
        }

        for (var i = 0; i < subs.length; i++) {
          var sub = subs[i];
          if (sub.length != 0) {
            folders = folders.concat(sub);
          }
        }
      }
    } else {
      ftn.recurse = false;
    }
  }

  return folders;
};
FolderTreeSelector.flattenTree = function(tree, root) {
  var node = tree.items[0];

  return FolderTreeSelector.flattenNode(node, root.absoluteURI);
};

FolderTreeSelector.run = function(rootFolder) {
  var win = FolderTreeSelector.createDialog(rootFolder);
  var rc = win.show();

  var folders = [];

  if (rc == 1) {
    folders = win.folders;
  }

  return folders;
};

FolderTreeSelector.test = function() {
  var f = Folder.selectDialog("Please select a Root Folder");
  if (f) {
    FolderTreeSelector.run(f);
  }
};

// FolderTreeSelector.test();

"FolderTreeSelector.jsx";
// EOF