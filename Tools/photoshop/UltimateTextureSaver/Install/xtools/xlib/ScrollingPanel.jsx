//
// ScrollingPanel.jsx
//   This file contains code originally written by Bob Stucky.
//   I've made mods as needed.
//
// $Id: ScrollingPanel.jsx,v 1.7 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2009, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

//-@include "GenericUI.jsx"

GenericUI.prototype.createVScrollingPanel = function(pnl, ctype, height,
                                                     margin) {
  var self = this;

  var panelW = pnl.bounds.width;
  var panelH = pnl.bounds.height;

  var sbarSize = 22;

  var m = toNumber(margin);
  if (isNaN(m)) {
    m = 0;
  }

  pnl._height = height;

  pnl.contents = pnl.add(ctype, [0,0,panelW-sbarSize,height]);
  pnl.scrollbar = pnl.add('scrollbar',[(panelW-sbarSize),0,(panelW-2),panelH-m]);

 	pnl.scrollbar.stepdelta = 40;
  // 	pnl.scrollbar.jumpdelta = (self.scrollbar.panelHeight - self.objSize +
  //                               self.spacing);

  pnl.scrollbar.minvalue = 0;
  pnl.scrollbar.maxvalue = height;

	pnl.scrollbar.scroll = function(scrollTo) {
    var pnl = this.parent;

    pnl.contents.bounds.top = -scrollTo;
    pnl.contents.bounds.bottom = -scrollTo + pnl._height;

    // $.writeln(pnl.contents.bounds.top + ', ' + pnl.contents.bounds.bottom);
	}
	pnl.scrollbar.onChange = function() {
    var pnl = this.parent;
		pnl.scrollbar.scroll(pnl.scrollbar.value);
	}
	pnl.scrollbar.onChanging = function() {
		pnl.scrollbar.scroll(pnl.scrollbar.value);
	}
};

GenericUI.prototype.createHScrollingPanel = function(pnl, ctype, width,
                                                     margin, osize) {
  var self = this;

  var sbarSize = 22;

  var panelW = pnl.bounds.width;
  var panelH = pnl.bounds.height;

  var m = toNumber(margin);
  if (isNaN(m)) {
    m = 0;
  }

  pnl._width = width;

  pnl.contents = pnl.add(ctype, [0,0,width,panelH-sbarSize]);
  pnl.scrollbar = pnl.add('scrollbar',[0,(panelH-sbarSize),panelW-m,(panelH-2)]);

  pnl.scrollbar.stepdelta = 40;

  if (osize) {
    pnl.objSize = osize;
 	  pnl.scrollbar.jumpdelta = (self.scrollbar.panelWidth - pnl.objSize +
                               self.spacing);
  }

  pnl.scrollbar.minvalue = 0;
  pnl.scrollbar.maxvalue = width;

	pnl.scrollbar.scroll = function(scrollTo) {
    var pnl = this.parent;

    var width = pnl._width;

    pnl.contents.bounds.left = -scrollTo;
    pnl.contents.bounds.right = -scrollTo + width;
	}
	pnl.scrollbar.onChange = function() {
    var pnl = this.parent;
		pnl.scrollbar.scroll(pnl.scrollbar.value);
	}
	pnl.scrollbar.onChanging = function() {
		pnl.scrollbar.scroll(pnl.scrollbar.value);
	}
};


// SampleUI.prototype.createPanel = function(pnl, ini) {
//   var self = this;

//   self.createVScrollingPanel(pnl, 'panel', 800);
//   self.createInnerPanel(pnl.contents, ini);
// };



"ScrollingPanel.jsx";
// EOF
