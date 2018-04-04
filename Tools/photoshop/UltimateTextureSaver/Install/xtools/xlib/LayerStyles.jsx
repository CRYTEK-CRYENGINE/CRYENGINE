//
// LayerStyles
//
// $Id: LayerStyles.jsx,v 1.2 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/stdlib.js"
//
LayerStyles = function() {};

LayerStyles.createStroke = function(size, color, opacity, pos, blend) {

  if (size == undefined) {
    Error.runtimeError(2, "size");   // undefined
  }

  if (color == undefined) {
    Error.runtimeError(2, "color");   // undefined
  }

  if (opacity == undefined) {
    opacity = 100.00;
  }

  if (pos == undefined) {
    pos = 'InsF';
  }

  if (blend == undefined) {
     blend = 'Nrml';
  }
 
  var desc = new ActionDescriptor();

  // Active Layer
  var tref = new ActionReference();
  tref.putProperty( cTID('Prpr'), cTID('Lefx') );
  tref.putEnumerated( cTID('Lyr '), cTID('Ordn'), cTID('Trgt') );
  desc.putReference( cTID('null'), tref );

  var sdesc = new ActionDescriptor();

  // Scale
  sdesc.putUnitDouble( cTID('Scl '), cTID('#Prc'), 100.0000 );
  
  var adesc = new ActionDescriptor();

  // Enabled
  adesc.putBoolean( cTID('enab'), true );

  // Position InsF,CtrF,OutF
  adesc.putEnumerated( cTID('Styl'), cTID('FStl'), cTID(pos) );

  // PaintType, FrameFill, SolidColor
  adesc.putEnumerated( cTID('PntT'), cTID('FrFl'), cTID('SClr') );

  // Mode, BlendMode, Normal
  adesc.putEnumerated( cTID('Md  '), cTID('BlnM'), cTID(blend) );

  // Opacity
  adesc.putUnitDouble( cTID('Opct'), cTID('#Prc'), opacity );

  // Size
  adesc.putUnitDouble( cTID('Sz  '), cTID('#Pxl'), size );

  // Color
  var rgb = (color instanceof RGBColor) ? color : color.rgb;
  var clrDesc = new ActionDescriptor();
  clrDesc.putDouble( cTID('Rd  '), rgb.red );
  clrDesc.putDouble( cTID('Grn '), rgb.green );
  clrDesc.putDouble( cTID('Bl  '), rgb.blue );
  adesc.putObject( cTID('Clr '), cTID('RGBC'), clrDesc );

  sdesc.putObject( cTID('FrFX'), cTID('FrFX'), adesc );
  desc.putObject( cTID('T   '), cTID('Lefx'), sdesc );
  executeAction( cTID('setd'), desc, DialogModes.NO );
};


LayerStyles.createDropShadow = function(size, color) {
  if (size == undefined) {
    Error.runtimeError(2, "size");   // undefined
  }

  if (color == undefined) {
    Error.runtimeError(2, "color");   // undefined
  }

  var desc = new ActionDescriptor();

  // Active Layer
  var ref = new ActionReference();
  ref.putProperty( cTID('Prpr'), cTID('Lefx') );
  ref.putEnumerated( cTID('Lyr '), cTID('Ordn'), cTID('Trgt') );
  desc.putReference( cTID('null'), ref );

  // Scale
  var sdesc = new ActionDescriptor();
  sdesc.putUnitDouble( cTID('Scl '), cTID('#Prc'), 100.00 );

  var adesc = new ActionDescriptor();

  // Enabled
  adesc.putBoolean( cTID('enab'), true );

  // Mode, BlendMode Multiply
  adesc.putEnumerated( cTID('Md  '), cTID('BlnM'), cTID('Mltp') );

  // Color
  var clrDesc = new ActionDescriptor();
  clrDesc.putDouble( cTID('Rd  '), 0.000000 );
  clrDesc.putDouble( cTID('Grn '), 0.000000 );
  clrDesc.putDouble( cTID('Bl  '), 0.000000 );
  adesc.putObject( cTID('Clr '), cTID('RGBC'), clrDesc );

  // Opacity
  adesc.putUnitDouble( cTID('Opct'), cTID('#Prc'), 75.000000 );

  // UseGlobalAngle
  adesc.putBoolean( cTID('uglg'), true );

  // LocalLightingAngle
  adesc.putUnitDouble( cTID('lagl'), cTID('#Ang'), 120.000000 );

  // Distance
  adesc.putUnitDouble( cTID('Dstn'), cTID('#Pxl'), 5.000000 );

  // ChokeMatte
  adesc.putUnitDouble( cTID('Ckmt'), cTID('#Pxl'), 0.000000 );

  // Blur
  adesc.putUnitDouble( cTID('blur'), cTID('#Pxl'), 5.000000 );

  // Noise
  adesc.putUnitDouble( cTID('Nose'), cTID('#Prc'), 0.000000 );

  // AntiAlias
  adesc.putBoolean( cTID('AntA'), false );

  // Name (Quality) Linear
  var qdesc = new ActionDescriptor();
  qdesc.putString( cTID('Nm  '), "Linear" );

  // TransferSpec, ShapingCurve, 
  adesc.putObject( cTID('TrnS'), cTID('ShpC'), qdesc );

  // layerConceals
  adesc.putBoolean( sTID('layerConceals'), true );

  sdesc.putObject( cTID('DrSh'), cTID('DrSh'), adesc );
  desc.putObject( cTID('T   '), cTID('Lefx'), sdesc );
  executeAction( cTID('setd'), desc, DialogModes.NO );
};


LayerStyles.main = function() {
  LayerStyles.createStroke(20, app.foregroundColor);
};

LayerStyles.main();

"LayerStyles.jsx";

// EOF
