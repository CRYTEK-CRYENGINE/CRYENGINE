//
// SmartSharpen.jsx
//
// $Id: SmartSharpen.jsx,v 1.4 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//

cTID = function(s) { return app.charIDToTypeID(s); };
sTID = function(s) { return app.stringIDToTypeID(s); };

PSClass = function() {};
PSEnum = function() {};
PSEvent = function() {};
PSForm = function() {};
PSKey = function() {};
PSType = function() {};
PSUnit = function() {};
PSString = function() {};

PSEvent.GaussianBlur = cTID('GsnB');
PSEvent.MotionBlur = cTID('MtnB');
PSKey.Angle = cTID('Angl');
PSKey.Amount = cTID('Amnt');
PSKey.Blur = cTID('blur');
PSKey.HighlightMode = cTID('hglM');
PSKey.Radius = cTID('Rds ');
PSKey.ShadowMode = cTID('sdwM');
PSKey.Threshold = cTID('Thsh');
PSKey.Width = cTID('Wdth');
PSString.blurType = sTID('blurType');
PSString.lensBlur = sTID('lensBlur');
PSString.moreAccurate = sTID('moreAccurate');
PSString.preset = sTID('preset');
PSString.smartSharpen = sTID('smartSharpen');
PSUnit.Percent = cTID('#Prc');
PSUnit.Pixels = cTID('#Pxl');

SmartSharpenOptions = function() {
  var self = this;

  self.amount = 100;          // 1% - 500%
  self.radius = 1.0;           // 0.1 px - 64.0 px
  self.blurType = SmartSharpenBlurType.GAUSSIAN_BLUR;
  self.threshold = 0;         // ???
  self.angle = 0;             // 0.0 - 360 ??? only when blurType = MOTION_BLUR
  self.moreAccurate = false;
  self.preset = "Default";    // ???

  self.shadowAmount = 0;      // 0% - 100%
  self.shadowTonalWidth = 50;  // 0% - 100%
  self.shadowRadius = 1;      // 1 px - 100 px
  self.shadowMode = sTID('adaptCorrectedTones');

  self.hiliteAmount = 0;      // 0% - 100%
  self.hiliteTonalWidth = 50; // 0% - 100%
  self.hiliteRadius = 1;      // 1 px - 100 px
  self.hiliteMode = sTID('adaptCorrectedTones');

  self.interactive = false;
};
SmartSharpenOptions.typename = "SmartSharpenOptions";

SmartSharpenBlurType = {};
SmartSharpenBlurType.GAUSSIAN_BLUR = PSEvent.GaussianBlur;
SmartSharpenBlurType.LENS_BLUR     = PSString.lensBlur;
SmartSharpenBlurType.MOTION_BLUR   = PSEvent.MotionBlur;



smartSharpen = function(opts) {
  var ssDesc = new ActionDescriptor();

  ssDesc.putUnitDouble( PSKey.Amount, PSUnit.Percent, opts.amount );
  ssDesc.putUnitDouble( PSKey.Radius, PSUnit.Pixels, opts.radius );
  ssDesc.putInteger( PSKey.Threshold, opts.threshold );
  ssDesc.putInteger( PSKey.Angle, opts.angle );
  ssDesc.putBoolean( PSString.moreAccurate, opts.moreAccurate );
  ssDesc.putEnumerated( PSKey.Blur, PSString.blurType, opts.blurType );
  ssDesc.putString( PSString.preset, opts.preset );

  var shDesc = new ActionDescriptor();
  shDesc.putUnitDouble( PSKey.Amount, PSUnit.Percent, opts.shadowAmount );
  shDesc.putUnitDouble( PSKey.Width, PSUnit.Percent, opts.shadowTonalWidth );
  shDesc.putInteger( PSKey.Radius, opts.radius );

  ssDesc.putObject( PSKey.ShadowMode, opts.shadowMode, shDesc );

  var hiDesc = new ActionDescriptor();
  hiDesc.putUnitDouble( PSKey.Amount, PSUnit.Percent, opts.hiliteAmount );
  hiDesc.putUnitDouble( PSKey.Width, PSUnit.Percent, opts.hiliteTonalWidth );
  hiDesc.putInteger( PSKey.Radius, opts.hiliteRadius );

  ssDesc.putObject( PSKey.HighlightMode, opts.hiliteMode, hiDesc );

  executeAction( PSString.smartSharpen, ssDesc,
                 (opts.interactive ? DialogModes.ALL : DialogModes.NO) );
};

function demo() {
  var opts = new SmartSharpenOptions();
  opts.amount = 200;
  opts.radius = 10;

  smartSharpen(opts);
};

//demo();

"SmartSharpen.jsx";
// EOF

