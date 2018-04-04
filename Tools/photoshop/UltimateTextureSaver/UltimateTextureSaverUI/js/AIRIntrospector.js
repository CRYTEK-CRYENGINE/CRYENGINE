/* AIRIntrospector.js - Revision: 1.5 */

/*
ADOBE SYSTEMS INCORPORATED
Copyright 2007-2008 Adobe Systems Incorporated. All Rights Reserved.
 
NOTICE:   Adobe permits you to modify and distribute this file only in accordance with
the terms of Adobe AIR SDK license agreement.  You may have received this file from a
source other than Adobe.  Nonetheless, you may modify or
distribute this file only in accordance with such agreement. 
*/

var air;

(function(){
	
if(typeof air=='undefined') air = {};
air.Introspector = {};


// Check the runtime. 
if(typeof window.runtime!='undefined' && typeof window.nativeWindow!='undefined') 
    var isAppSandbox = true;
else 
    var isAppSandbox = false;
	
//=======================================================================================================================================================
//Introspector.js
//=======================================================================================================================================================




//----------------------------------------------------------------------------------------------------
/**
*	@API air.Introspector.Console
*	@description Exposes log, warn, info, error, dump to the user
*	THIS IS THE ONLY SUPPORTED APIs 
*/

air.Introspector.Console = {
		 log: function(){
		   	air.Introspector.logArguments(arguments,
			 		{htmlLoader:isAppSandbox?window.htmlLoader:null});
		 },

		 warn : function(){
		        air.Introspector.logArguments(arguments,
			 			{htmlLoader:isAppSandbox?window.htmlLoader:null, type:'warn'});        
		 },

		 info : function(){
		        air.Introspector.logArguments(arguments,
			 			{htmlLoader:isAppSandbox?window.htmlLoader:null, type:'info'});        
		 },
 
		 error : function(){
		        air.Introspector.logArguments(arguments,
			 			{htmlLoader:isAppSandbox?window.htmlLoader:null, type:'error'});        
		 }, 
		 dump : function(obj, level){
			air.Introspector.logArguments(air.Introspector.dump(obj, level),
			 		{htmlLoader: isAppSandbox?window.htmlLoader:null, usePre:true});
		 }
};

//----------------------------------------------------------------------------------------------------

air.Introspector.config = {
    showTimestamp: true,						//Make the console show time stamps before each line
    showSender: true,							//Make the console show time stamps
    wrapColumns: 2000,							//Source files are soft-wrapped at around 2000 columns by default 
	flashTabLabels: true,						//Console and xhr columns can flash whenever something happend to them (eg. logged something). You can turn it off
												//by setting this to false
	closeIntrospectorOnExit: true,				//Makes the inspector window close when the last window closed
	debugRuntimeObjects: true,					//Also expand ActionScript objects
	introspectorKey:122,						//Inspect key - by default it is F11 (122)
	debuggerKey:123, 							//Toggle inspectors visibility - by default it is F12 (123)
    useAirDebugHtml: false,						//Internal only
};


/**
*	@module air.Introspector
*/

air.Introspector.extend = function(dst, src){
	//Take every property from src and put it in dst
    for(var i in src){
        dst[i]=src[i];
    }
};

//	Checking if the user has configured Introspector using global AIRIntrospectorConfig variable
if(typeof AIRIntrospectorConfig!='undefined'){
	air.Introspector.extend(air.Introspector.config, AIRIntrospectorConfig);
}

var eventListeners = [];

//	Can not expand ActionScript objects from remote sandbox - we can not even access ActionScript from there 
//	just disabled this feature
if(!isAppSandbox) air.Introspector.config.debugRuntimeObjects = false;

//	Made this use g/setters in order to make it easy to send its value over the bridge
air.Introspector.__defineGetter__('inspect', function(){
    return air.Introspector._inspect;
});

air.Introspector.__defineSetter__('inspect', function(value){
    air.Introspector._inspect=value;
    if(!isAppSandbox){
        setTimeout(function(){
			air.Introspector.noBridge(function(){
				parentSandboxBridge.air_Introspector_setInspect(value);
			});
        }, 0);
    }else{
		if(!value){
			try{
				air.Introspector.hideHighlight();
			}catch(e){}
		}
	}
});
	
air.Introspector.extend(air.Introspector, { 

	/**
	*	Makes it easier to acces runtime packages
	*	it makes sense only in the application sandbox
	*/
	runtime: isAppSandbox?{ 
		HTMLLoader : window.runtime.flash.html.HTMLLoader,
		Event : window.runtime.flash.events.Event,
		IOErrorEvent: window.runtime.flash.events.IOErrorEvent,
		NativeApplication: window.runtime.flash.desktop.NativeApplication,
		URLLoader : window.runtime.flash.net.URLLoader,
		URLRequest : window.runtime.flash.net.URLRequest,
		NativeWindowInitOptions : window.runtime.flash.display.NativeWindowInitOptions,
		Capabilities: window.runtime.flash.system.Capabilities,
		trace : window.runtime.trace,
	}:null,
	
	_inspect: false,
	remoteInspect: false,
    canClick: false,

	bridgeCallbacks: [],
	
	
	/**
	*	Different coloring styles for tag names, by default 'default' is used
	*	Undocumented feature
	*		- you can change the color of elements while inspecting by tag name, check bellow the body: 0x00CC00 line,
	*		  uncomment that line, duplicate and change it with your own colors
	*/
    highlightBgColors: {
        'default': 0xFFCC00,
        //body: 0x00CC00,
        
    },

	/**
	*	@function trimRegExp
	*	@description Trims spaces from a string
	*	@private
	*/
    trimRegExp: /^[\s\r\n]*([\s\S]*?)[\s\r\n]*$/g,
    trim:function(str){
    	return str.replace(air.Introspector.trimRegExp, '$1');
    },

	/**
	*	@function blockWrap
	*	@description Wraps a string by air.Introspector.config.wrapColumns columns
	*/
    blockWrap: function(str){
    	//used for spliting large lines in <pre>
    	var cols = air.Introspector.config.wrapColumns;
    	var lines = str.split(/\n/);
    	var buffer = [];
    	var l = lines.length;
    	var lineNumbers = [];
    	for(var i=0;i<l;i++){
    		lineNumbers.push(i+1);
    		var line = lines[i];
    		while(line.length>cols){
    			buffer.push(line.substr(0, cols));
    			line = line.substr(cols);
    			lineNumbers.push('');
    		}
    		buffer.push(line);
    	}
    	lineNumbers.push('EOF');
    	return [buffer.join('\n'), lineNumbers.join('\n')];
    },
	
	/**
	*	@function getTextFormat
	*	@description Returns a new flash TextField
	*/
    createTextField: function(parentSprite, fontSize, fontBold) {
		if(isAppSandbox){
       		var tf = new runtime.flash.text.TextField();
	        tf.embedFonts = false;
	        tf.autoSize = runtime.flash.text.TextFieldAutoSize.LEFT;
	        tf.antiAliasType = runtime.flash.text.AntiAliasType.ADVANCED;
	        tf.defaultTextFormat = air.Introspector.getTextFormat(fontSize, fontBold);
	        tf.selectable = false;
	        tf.mouseEnabled = true;
	        tf.x = 4;
	        tf.text = "";
	        if(parentSprite.numChildren > 0) {
	            var sibling = parentSprite.getChildAt(parentSprite.numChildren - 1);
	            tf.y = sibling.y + sibling.height + 15;
	        }
	        parentSprite.addChild(tf);
	        return tf;
		}else{
			//should not get here
			return null;
		}
    },
    /**
	*	@function getTextFormat
	*	Returns a new flash TextFormat
	*	see createTextField
	*/
    getTextFormat: function(fontSize, fontBold){
		if(isAppSandbox){
        	var format = new runtime.flash.text.TextFormat();
	        format.size = fontSize;
	        format.font = "Tahoma";
	        format.bold = fontBold;
	        format.color = 0x330066;
        	return format;
		}else{
			//should not get here
			return null;
		}
    },
    
	/**
	*	@function extendRect 
	*	@description Initializes the sprite with values from the rectangle
	*/
	extendRect: function(sprite, rect){
		sprite.x = rect.x;
		sprite.y = rect.y;
		sprite.width = rect.width;
		sprite.height = rect.height;
		sprite.scaleX = rect.scaleX;
		sprite.scaleY = rect.scaleY;			
	},
	
	
	/**
	*	@function showHighlight
	*	@description Shows a highlighting flash sprite using coordinates from rectangle
	*/
    showHighlight: function(rect){
		if(isAppSandbox){
	        //dehilight everyone else
	        var ownedWindows = air.Introspector.getHtmlWindows();
	        for(var i=ownedWindows.length-1;i>=0;i--){
	                try{
	                    ownedWindows[i].htmlLoader.window.air.Introspector.hideHighlight();
	                }catch(e){
	                    //no air.Introspector
	                }
	        }
	        air.Introspector.extendRect(air.Introspector.highlightSprite, rect);
		}else{
			setTimeout(function(){
				air.Introspector.noBridge(function(){
					parentSandboxBridge.air_Introspector_showHighlight(rect);				
				});
				
			}, 0);
			
			
		}
    },

	/**
	*	@function hideHighlight
	*	@description Make the higlight box go away
	*/
    hideHighlight: function(){
		if(isAppSandbox){
        	air.Introspector.extendRect(air.Introspector.highlightSprite, {x:0, y:0, width:0, height:0, scaleX:0, scaleY:0});
	        air.Introspector.highlightText.visible = false;
		}else{
			setTimeout(function(){
				try{
					parentSandboxBridge.air_Introspector_hideHighlight();
				}catch(e){ 
						// no bridge yet
					}
			}, 0);
		}
    },

	/**
	*	@function remoteClick
	*	@description Make the remote sandbox know that the inspection finished
	*/
	remoteClick: function(){
		air.Introspector.debugWindow.finishInspect(false);
        air.Introspector.hideHighlight();
	},
	
	
	/**
	*	@function createHighlight
	*	@description 	Creates a flash sprite used to higlight elements
	*				    By using this method we are sure that no dom manipulation is done and  
	*				    no style is changed in HTML.
	*/
    createHighlight: function(){
		if(isAppSandbox){
	        var sprite = new runtime.flash.display.Sprite();
	        sprite.mouseEnabled =  false;
	        sprite.width = 0;
	        sprite.height = 0;
	        sprite.buttonMode = true;
	        var prevent = function(element, event, isClick){
	            air.Introspector.addEventListener(element, event, function(e){
	                if((air.Introspector.inspect||air.Introspector.remoteInspect) &&sprite.hitTestPoint(e.stageX, e.stageY)){
	                    e.preventDefault();
	                    e.stopPropagation();
	                    e.stopImmediatePropagation();
	                   	if(isClick&&air.Introspector.canClick){
							if(air.Introspector.remoteInspect){
								try{
									air.Introspector.inspectFrame.contentWindow.childSandboxBridge.air_Introspector_remoteClick();
								}catch(e){ air.Introspector.noChildBridge(air.Introspector.inspectFrame); }
							}else{
	                        	air.Introspector.debugWindow.finishInspect(false);
		                        air.Introspector.hideHighlight();
							}
	                    }
	                }

	            }, true, 2000000);
	        };
	        var check = function(element, event){
	            air.Introspector.addEventListener(element, event, function(e){
	               if((air.Introspector.inspect||air.Introspector.remoteInspect)&&nativeWindow.active){
	                    setTimeout(function(){
	                        air.Introspector.canClick = true;
	                    }, 100);
	               }
	            }, true, 200000);
	        }
        
	        var labelMover = function(element, event){
	        	air.Introspector.addEventListener(element, event, function(e){
		           if((air.Introspector.inspect||air.Introspector.remoteInspect)){
	                  air.Introspector.highlightText.x = e.stageX+15;
	                  air.Introspector.highlightText.y = e.stageY+15;
//					  air.Introspector.highlightText.visible = true;
	               }else{

	                  air.Introspector.highlightText.visible = false;
	               }
	        	}, true, 200000);
	        }
	       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.CLICK, true);
	       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_DOWN);
	       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_UP);
	       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.DOUBLE_CLICK);
	       check(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_MOVE);
	       check(nativeWindow, runtime.flash.events.Event.ACTIVATE);
	       labelMover(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_MOVE);
	       window.htmlLoader.stage.addChild(sprite); 
	       air.Introspector.highlightSprite = sprite;

	       air.Introspector.highlightText = new runtime.flash.display.Sprite();
	       window.htmlLoader.stage.addChild(air.Introspector.highlightText); 

	       air.Introspector.highlightText.graphics.beginFill(0xeeeeee, 0.8);
	       air.Introspector.highlightText.graphics.lineStyle(1, 0xeeeeee, 0.9, false);
	       air.Introspector.highlightText.graphics.drawRect(0, 0, 250, 40);
	       air.Introspector.highlightText.visible = false;
	       air.Introspector.highlightLine1 = air.Introspector.createTextField(air.Introspector.highlightText, 16, true);
	       air.Introspector.highlightLine2 = air.Introspector.createTextField(air.Introspector.highlightText, 10, false);
       }else{
			//should not be here
	   }
    },
    
	/**
	*	@function addEventListener
	*	@description Add a listener and stores it for future cleanup
	*/
	addEventListener: function(obj, eventName, listener, capture, priority){
		eventListeners.push([obj, eventName, listener, capture]);
		obj.addEventListener(eventName, listener, capture, priority);
	},
	
	/**
	*	@function removeEventListener
	*	@description Removes listener
	*/
	removeEventListener: function(obj, eventName, listener, capture){
		for(var i=eventListeners.length-1;i>=0;i--){
			var l = eventListeners[i];
			if(l[0]==obj && l[1]==eventName && l[2]==listener && l[3]==capture)
				{
					eventListeners.splice(i, 1);
					break;
				}
		}
		obj.removeEventListener(eventName, listener, capture);
	},	
	
	/**
	*	@function drawRect
	*	@description Draw a rectangle using ActionScript, also use tagName to find out which color to use 
	*	@see air.Introspector.highlightBgColors
	*/
	drawRect: function (rect, tagName){
			var htmlLoaderBounds = htmlLoader.getBounds(htmlLoader.stage);
	    	rect.x += htmlLoaderBounds.x;
		    rect.y += htmlLoaderBounds.y;
			rect.scaleX = 1;
		    rect.scaleY = 1;
		    air.Introspector.showHighlight(rect);
		    air.Introspector.highlightSprite.graphics.clear();
		    var bgColor = air.Introspector.highlightBgColors[tagName.toLowerCase()];
		    if(typeof bgColor=='undefined')
		         bgColor = air.Introspector.highlightBgColors['default'];
		    air.Introspector.highlightSprite.graphics.beginFill(bgColor, 0.2);
		    air.Introspector.highlightSprite.graphics.lineStyle(3, bgColor, 0.9, false);
		    air.Introspector.highlightSprite.graphics.drawRect(0, 0, rect.width, rect.height);
	},
	
	/**
	*	@function highlightElement
	*	@description Highlight element e. Get its bounding box and send it directly or over the bridge to air.Introspector.drawRect
	*	@also air.Introspector.drawRect
	*/
    highlightElement: function(e, callback){
		var rect = air.Introspector.getBorderBox(e);
	   	if(rect==false)
			return;
	
		if(isAppSandbox){			
			air.Introspector.drawRect(rect, e.tagName);
		}else{
			setTimeout(function(){
				try{
					if(!isNaN(rect.width)&&!isNaN(rect.x)){
						air.Introspector.noBridge(function(){
							parentSandboxBridge.air_Introspector_drawRect(rect, e.tagName);					
						});
					}
				}catch(e){
				}
				if(typeof callback!='undefined') callback();
			}, 0);
		}
    },
    
	/**
	*	@function addKeyboardEvents
	*	@description 	Registers events on every window that includes AIRDebug.js.
	*
	*	By default F11 enables the inspect tool
	*			   F12 pops up the debug tool
	*/
	addKeyboardEvents: function(sprite){
		air.Introspector.addEventListener(sprite, runtime.flash.events.KeyboardEvent.KEY_DOWN, function(e){
            if(e.keyCode==air.Introspector.config.introspectorKey){ //F11 key pressed
				if(typeof air.Introspector.lastElement!='undefined'&&(air.Introspector.lastElement.nodeName=='IFRAME'||air.Introspector.lastElement.nodeName=='FRAME')){
					try{
						var contentWindow = air.Introspector.lastElement.contentWindow;
						if(typeof contentWindow.childSandboxBridge!='undefined'&&
							typeof contentWindow.childSandboxBridge.air_Introspector_isDebugOpen!='undefined'&&
							typeof contentWindow.childSandboxBridge.air_Introspector_toggleInspect!='undefined')
						{
							if(contentWindow.childSandboxBridge.air_Introspector_isDebugOpen()){
								contentWindow.childSandboxBridge.air_Introspector_toggleInspect();
								e.preventDefault();
								e.stopPropagation();
								return;	
							}
						}
					}catch(e){
						//it looks like no debugger in that iframe. go ahead with app sandbox debugger
					}
				}
                air.Introspector.init(false, true, function(){
                	air.Introspector.debugWindow.toggleInspect();					
				});

                e.preventDefault();
				e.stopPropagation();
            }else if(e.keyCode==air.Introspector.config.debuggerKey){ //F12 key pressed
                air.Introspector.toggleWindow();
                e.preventDefault();
				e.stopPropagation();
            }else if(e.keyCode==27&&air.Introspector.inspect){
                air.Introspector.debugWindow.finishInspect();
                air.Introspector.hideHighlight();
                e.preventDefault();
						e.stopPropagation();
            }else if(e.ctrlKey==true&&e.altKey==false){
				var tab = null;
				switch(e.keyCode){
					case runtime.flash.ui.Keyboard.NUMBER_1:
						tab = 0;
					break;
					case runtime.flash.ui.Keyboard.NUMBER_2:
						tab = 1;
					break;
					case runtime.flash.ui.Keyboard.NUMBER_3:
						tab = 2;
					break;
					case runtime.flash.ui.Keyboard.NUMBER_4:
						tab = 3;
					break;
					case runtime.flash.ui.Keyboard.NUMBER_5:
						tab = 4;
					break;
					case runtime.flash.ui.Keyboard.NUMBER_6:
						tab = 5;
					break;
				}
				if(tab!=null){
						air.Introspector.init(false, true, function(){
							air.Introspector.debugWindow.setTab(tab);							
						});
						e.preventDefault();
						e.stopPropagation();
				}
			}
        }, true, 1000000);
	},
	
	/**
	*	@function showHighlightLabels
	*	@description Make the tooltip labels near the highlighting box appear and tell the id/tag name/outer HTML
	*/
	showHighlightLabels: function(id, nodeName, outerHTML){
			if(typeof id!='undefined'&&id.length!=0){
                air.Introspector.highlightLine1.text = nodeName+' - '+id;
            }else{
                air.Introspector.highlightLine1.text = nodeName;  
            }
            if(air.Introspector.canClick){
                air.Introspector.highlightLine2.text = outerHTML.substr(0, 40).replace(/\n/g, '\\n')+'...';
            }else{
                air.Introspector.highlightLine2.text = 'Click to activate window';
                window.clearTimeout(air.Introspector.clickToActivateTimeout);
                air.Introspector.clickToActivateTimeout = setTimeout(function(){
                    air.Introspector.highlightLine2.text = outerHTML.substr(0, 40).replace(/\n/g, '\\n')+'...';
                }, 400)
            }
        	air.Introspector.highlightText.visible = true;	
	},
	
	
	/**
	*	@function registerUncaughtExceptionListener
	*	@description Catches all uncaught exceptions from javascript and shows them in the console
	*/
	registerUncaughtExceptionListener: function(){
		
			air.Introspector.addEventListener(window.htmlLoader,
					runtime.flash.events.HTMLUncaughtScriptExceptionEvent.UNCAUGHT_SCRIPT_EXCEPTION , 
					function(e){
						if(e.exceptionValue && 
								e.exceptionValue.air_Introspector_setParentSandboxBridge == true &&
								e.exceptionValue.air_Introspector_version == air.Introspector.version)
							{
								air.Introspector.registerFramesParentSandboxBridge();
								e.preventDefault();
								return;
							}
						
						air.Introspector.logError(e.exceptionValue, {htmlLoader:window.htmlLoader});
						//	e.preventDefault();
		        	});
		
		
	},


	/**
	*	@function registerCloseEventListener
	*	@description 
	*/
	registerCloseEventListener: function(){
		air.Introspector.addEventListener(window.nativeWindow, air.Introspector.runtime.Event.CLOSE, function(){
	            var debugWindow = air.Introspector.findDebugWindow();
	            if(debugWindow!=null){
	                debugWindow.closedWindow(window.htmlLoader);
	            }
	
	        });
			
		
	},

	/**
	*	@function registerCompleteEventLisener
	*	@description Make the Introspector window knwo that we are complete. Register parentSandboxBridge on every frame
	*/
	registerCompleteEventListener: function(){
		air.Introspector.addEventListener(window.htmlLoader, air.Introspector.runtime.Event.COMPLETE, function(){
					air.Introspector.removeEventListener(window.htmlLoader, air.Introspector.runtime.Event.COMPLETE, arguments.callee);
					try{
		           	 //announce the debugWindow to refresh DOM and assets
			            var debugWindow = air.Introspector.findDebugWindow();
			            if(debugWindow!=null){
			            	if(debugWindow.isLoaded){
			                    debugWindow.completeWindow(window.htmlLoader);
			            	}
			            }
					
						air.Introspector.registerFramesParentSandboxBridge();
					}catch(e){
						runtime.trace(e);
			            runtime.trace(e.line);
						air.Introspector.Console.log(e);
					}
			});
	},

	/**
	*	@function registerFramesParentSandboxBridge
	*	@description All frames should know about us - registering parentSandboxBridge
	*/
	
	registerFramesParentSandboxBridge: function(){
			//var modified = false;
			var iframes = document.getElementsByTagName('iframe');
			for(var i=iframes.length-1;i>=0;i--){
				air.Introspector.registerFrame(iframes[i]);
			}

			var frames = document.getElementsByTagName('frame');
			for(var i=frames.length-1;i>=0;i--){
				air.Introspector.registerFrame(frames[i]);				
			}
			//return modified;
	},
	
	/**
	*	@function registerDeactivateEventLisener
	*	@description Hides the highlighting rectangle and deactivates inspect-clicking for this window
	*/
	registerDeactivateEventListener: function(){
			air.Introspector.addEventListener(window.nativeWindow, air.Introspector.runtime.Event.DEACTIVATE, function(){ air.Introspector.hideHighlight(); air.Introspector.canClick =false; });
	},
	
	/**
	*	@function registerChildSandboxBridge
	*	@description Register childSandboxBridge for current iframe
	*/
	registerChildSandboxBridge: function(){
		
		if(typeof childSandboxBridge=='undefined')
			childSandboxBridge={};
			try{
		childSandboxBridge.air_Introspector_remoteClick = function (){
			try{
				air.Introspector.remoteClick();
			}catch(e){ alert(e+' '+e.line); }
		}
		
		childSandboxBridge.air_Introspector_isDebugOpen = function(){
			return typeof air.Introspector.debugWindow!='undefined';
		}
		
		childSandboxBridge.air_Introspector_toggleInspect = function (){
			air.Introspector.init(false, true, function()
			{
               	air.Introspector.debugWindow.toggleInspect();							
			});
		}
		
		childSandboxBridge.air_Introspector_bridgeLoaded = function(){
			var l = air.Introspector.bridgeCallbacks;
			for(var i=0;i<l;i++){
				try{
					air.Introspector.bridgeCallbacks[i]();
				}catch(e){
					air.Introspector.logError(e);
				}
			}
			air.Introspector.bridgeCallbacks = [];
		}
			}
			catch(e){}
	},
	
	/**
	*	@function createOpenConsoleButton
	*	@description Creates a button on the top-right corent of the iframe that will open the introspector
	*/
	createOpenConsoleButton: function(){
		var consoleButton = document.createElement('input');
		consoleButton.onclick = function(){
			air.Introspector.init(true, true, function(){ });
		}
		consoleButton.style.zIndex = 1000000;
		consoleButton.style.position = 'fixed';
		consoleButton.style.right = '10px';
		consoleButton.style.top = '10px';
		consoleButton.type = 'button';
		consoleButton.value = 'Open Introspector';
		document.body.appendChild(consoleButton);								
	},
	
	/**
	*	@function registerDOMEventListeners
	*	@description Registers DOMSubtreeModified, DOMCharacterDataModified, mouseover
	*/
	registerDOMEventListeners: function(){
		var hoverTimeout = null;
		//debugWindow should know about any dom change
		document.addEventListener('DOMSubtreeModified', function(e){
            var debugWindow = air.Introspector.findDebugWindow();
            if(debugWindow!=null&&debugWindow.isLoaded){
				debugWindow.dom3Event(e);
            }
		});
		document.addEventListener('DOMCharacterDataModified', function(e){
            var debugWindow = air.Introspector.findDebugWindow();
            if(debugWindow!=null&&debugWindow.isLoaded){
				debugWindow.dom3Event(e);
            }
		});
		

           document.body.addEventListener('mouseover', function(e){
               if(air.Introspector.inspect){
				setTimeout(function(){
					if(isAppSandbox){
							if(!nativeWindow.active)
								nativeWindow.activate();
					}
                    if(e.srcElement){
						if(isAppSandbox){
                        	air.Introspector.highlightElement(e.srcElement);
							air.Introspector.showHighlightLabels(e.srcElement.id, e.srcElement.nodeName, e.srcElement.outerHTML);
						}else{
							air.Introspector.highlightElement(e.srcElement, function(){
								air.Introspector.noBridge(function(){
									parentSandboxBridge.air_Introspector_showHighlightLabels(e.srcElement.id, e.srcElement.nodeName, e.srcElement.outerHTML);								
								});
							});							
						}
						if(hoverTimeout) clearTimeout(hoverTimeout);
						hoverTimeout = setTimeout(function(){
	                        air.Introspector.init(false, true, function (){
								air.Introspector.debugWindow.setInspectElement(e.srcElement);
							});
						}, 100); 
                    }else{
                        air.Introspector.hideHighlight();
                    }
				}, 0);
             }else if(isAppSandbox){
						air.Introspector.lastElement = e.srcElement;
			 }

           }, true);

		//document.body.addEventListener('mouseout', function(e){
		//		air.Introspector.hideHighlight();
        //});
	},
	
	/**
	*	@function cleanup
	*	@description Cleans up the html loader
	*
	*/
    cleanup: function(){
			for(var i=eventListeners.length-1;i>=0;i--){
				var l = eventListeners[i];
				try{
					l[0].removeEventListener(l[1], l[2], l[3]);	
				}catch(e){}
			}
			eventListeners = [];
			try{
		    	window.htmlLoader.stage.removeChild(air.Introspector.highlightText); 
			}catch(e){}	    
	},
	
	/**
	*	@function register
	*	@description Registers current window in debugger
	*
	*	Captures every XHR object created and any uncaught exception 
	*	and sends it to the debugger
	*/
    register: function(){
    	
    	if (window.XMLHttpRequest && window.XMLHttpRequest.prototype){
                    window.XMLHttpRequest.prototype.debugopen = window.XMLHttpRequest.prototype.open;
					window.XMLHttpRequest.prototype.debugsend = window.XMLHttpRequest.prototype.send;
                    window.XMLHttpRequest.prototype.open = function(method, url, asyncFlag, username, password){
						if(typeof this.doNotDebug=='undefined'){
	                   	    var debugWindow = air.Introspector.findDebugWindow();
			                if(debugWindow!=null){
								debugWindow.logNet(this, method, url, asyncFlag);
			                }
						}
                        return this.debugopen(method, url, asyncFlag, username, password);
                    };
					window.XMLHttpRequest.prototype.send = function(obj){
						if(typeof this.doNotDebug=='undefined'){
	                	    var self = this;
							var debugWindow = air.Introspector.findDebugWindow();
				            if(debugWindow!=null){
						        	var a = this.onreadystatechange;
		                            this.onreadystatechange = function(){
		                            	 if (typeof a == 'function')a.call(self);
		                            	 	debugWindow.logNet(self, 'unknown', '', false);
				                    };
								if(typeof self.doNotDebug=='undefined')
		                           	 debugWindow.logNetSend(this, obj);
			                }
	                        var ret = this.debugsend(obj);
							if(debugWindow!=null){
									debugWindow.logNetSend(this, obj);
							}
							return ret;
						}else{
	                        return this.debugsend(obj);
						}
						
					}
        }

		if(isAppSandbox){  
				air.Introspector.addKeyboardEvents(window.htmlLoader);
				air.Introspector.registerUncaughtExceptionListener();
				air.Introspector.registerCloseEventListener();
				air.Introspector.registerCompleteEventListener();
	        	air.Introspector.registerDeactivateEventListener();
	 		    air.Introspector.createHighlight();
			}else{
				air.Introspector.registerChildSandboxBridge();
			}
			air.Introspector.waitForBody(document, function(){
		        try{
					if(!isAppSandbox){ air.Introspector.createOpenConsoleButton(); }
					air.Introspector.registerDOMEventListeners();
					window.addEventListener('unload', function(){
						try{
							air.Introspector.cleanup();
							if(!isAppSandbox){
								//our debugger can NOT live without it's parent
								air.Introspector.debugWindow.window.close();
							}
						}catch(e){  }
					});
				}catch(e){
					if(isAppSandbox){ 
						runtime.trace(e);
		            	runtime.trace(e.line);
					}
					air.Introspector.Console.log(e);
            	}
	     });

	
    },


	/**
	*	@function registerFrame
	*	@description Makes the parentSandboxBridge available to frame 
	*/
	registerFrame: function(frame){
		if(typeof frame.contentWindow.parentSandboxBridge=='undefined')
			frame.contentWindow.parentSandboxBridge = {};
			
		/*frame.contentWindow.parentSandboxBridge.trace = function(a){
			runtime.trace(a);
		};*/
		//checking that the bridge is not already there
		/*var modified = typeof frame.contentWindow.parentSandboxBridge.air_Introspector_hideHighlight=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_showHighlight=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_drawRect=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_setInspect=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_getWindowTitle=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_checkNativeWindow=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToClipboard=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToFile=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_writeConfigFile=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_readConfigFile=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_showHighlightLabels=='undefined'
			|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_getFrameId=='undefined';*/
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_hideHighlight = function(){
				air.Introspector.hideHighlight();
		};
			
		frame.contentWindow.parentSandboxBridge.air_Introspector_showHighlight = function(rect){
				air.Introspector.showHighlight(rect);
		};
			
		frame.contentWindow.parentSandboxBridge.air_Introspector_drawRect = function(rect, tagName){
				var frameRect = air.Introspector.getBorderBox(frame);
				var blw = air.Introspector.getIntProp(frame, "border-left-width");
                var btw = air.Introspector.getIntProp(frame, "border-top-width");
				if(frameRect==null) return;
				rect.x+=frameRect.x+2*blw;
				rect.y+=frameRect.y+2*btw;
				air.Introspector.drawRect(rect, tagName);
		};

		frame.contentWindow.parentSandboxBridge.air_Introspector_setInspect = function(enabled){
				air.Introspector.inspectFrame = enabled?frame:null;
				air.Introspector.remoteInspect = enabled;
				if(!enabled){
					air.Introspector.hideHighlight();
				}
		};
		frame.contentWindow.parentSandboxBridge.air_Introspector_getWindowTitle = function(){
			return document.title;
		};
		frame.contentWindow.parentSandboxBridge.air_Introspector_checkNativeWindow = function(title){
			var htmlWindows = air.Introspector.runtime.NativeApplication.nativeApplication.openedWindows;
			for(var i=htmlWindows.length-1;i>=0;i--){
				if(htmlWindows[i].title==title){
					return true;
				}
			}
			return false;
		};
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToClipboard = function(str){
			air.Introspector.writeConsoleToClipboard(str);
		};
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToFile = function(str){
			air.Introspector.writeConsoleToFile(str);
		};
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_writeConfigFile = function(config){
			return air.Introspector.writeConfigFile(config, true);			
		}
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_readConfigFile = function(){
			return air.Introspector.readConfigFile(true);
		}
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_showHighlightLabels = function(id, nodeName, outerHTML){
			air.Introspector.showHighlightLabels(id, nodeName, outerHTML);
		};
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_getFrameId = function(){
			return frame.id;
		}
		
		frame.contentWindow.parentSandboxBridge.air_Introspector_getNextWindowId = function(){
			return ++air.Introspector.times;
		}
		
		if(typeof frame.contentWindow.childSandboxBridge!='undefined'
				&& typeof frame.contentWindow.childSandboxBridge.air_Introspector_bridgeLoaded!='undefined'){
			frame.contentWindow.childSandboxBridge.air_Introspector_bridgeLoaded();
		}
		
		
		//return modified;
	},
	
	/**
	*	@function waitForBody
	*	@description Wait until document.body is available
	*/
	waitForBody: function(document, callback){
		if(document.body){
			callback();
		}else{
			setTimeout(air.Introspector.waitForBody, 10, document, callback);
		}
	},
    
	/**
	*	@function toggleWindow
	*	@description Shows/Hides the debug tool
	*/
    toggleWindow:function(){
        air.Introspector.init(true, false, function(justCreated){
			if(!justCreated)
				air.Introspector.debugWindow.nativeWindow.visible ^= true;
		});
        
    },
	
	
	/**
	*	@function init
	*	@description Makes sure the debug tool is enabled
	*/
    init: function(showLoader, toggle, callback){
		if(!air.Introspector.canInit())
			return;
	
		if(typeof showLoader=='undefined') showLoader = false;
		if(typeof toggle=='undefined') toggle = true;
	
		if(isAppSandbox){
			
			if(typeof air.Introspector.debugWindow=='undefined' || air.Introspector.debugWindow.nativeWindow.closed){
				delete air.Introspector.debugWindow;
	           var debugWindow = air.Introspector.findDebugWindow();
	           if(debugWindow!=null && !debugWindow.nativeWindow.closed){
	               air.Introspector.debugWindow = debugWindow;
				if(toggle){
					air.Introspector.debugWindow.nativeWindow.visible = true;			   
					if(!showLoader){
						nativeWindow.activate();
					}
				}
				callback(false);
	           }else{
				   air.Introspector.loadDebugger(function(debugWindow){
						air.Introspector.debugWindow = debugWindow;
						callback(true);
				   }, showLoader);
	           }
	        }else{
				if(toggle){
					if(showLoader){
						air.Introspector.debugWindow.nativeWindow.activate(); 
					}
				}
				callback(false);
			}
		}else{
			if(typeof activeWindow=='undefined'){
				air.Introspector.registerChildSandboxBridge();
			}
			
			if(typeof air.Introspector.debugWindow=='undefined'||
				typeof air.Introspector.debugWindow.window.air=='undefined'){ 
/*				(air.Introspector.debugWindow.isWindowCreated
					&&air.Introspector.debugWindow.isLoaded 
					  &&air.Introspector.debugWindow.window 
						&&!parentSandboxBridge.air_Introspector_checkNativeWindow(air.Introspector.parentWindowTitle + ': '+air.Introspector.debugWindow.window.document.title))){*/
				delete air.Introspector.debugWindow;


				air.Introspector.loadDebugger(function(debugWindow){
						air.Introspector.debugWindow = debugWindow;
						callback(true);
				   });
//				air.Introspector.debugWindow = new air.Introspector.DebugWindow ({activateDebug: showLoader, activeWindow: window});
			}else if(!air.Introspector.debugWindow.isWindowCreated){
				return;
			}else{
				callback(false);
			}
		}
    },
    times:0,	//make the window.open page name unique - this is the number of opened and closed introspector windows
	/**
	*	@function tryCreateWindow
	*	@description window.Open in browser/remote sandbox is not allowed if the action is not iniated by the user (eg. user gesture, mouse click)
	*				 We can only wait for that moment. Until that happends we record all the callbacks and run them when the Introspector is laoded 
	*	@runs in remote sandbox only
	*/
	tryCreateWindow: function(callbacks){
//		try{
			var self = this;
			var w;
			var iframeId;
			
/*			if(typeof parentSandboxBridge=='undefined'){
				air.Introspector.noBridge(function(){
					air.Introspector.tryCreateWindow(callbacks);					
				});
				return;
			}*/

			air.Introspector.parentWindowTitle = parentSandboxBridge.air_Introspector_getWindowTitle();

			if(typeof parentSandboxBridge!='undefined'&&typeof parentSandboxBridge.air_Introspector_getFrameId!='undefined')
				iframeId  = parentSandboxBridge.air_Introspector_getFrameId();
			
			if(typeof parentSandboxBridge!='undefined'&&typeof parentSandboxBridge.air_Introspector_getNextWindowId!='undefined')
				air.Introspector.times = parentSandboxBridge.air_Introspector_getNextWindowId();
			else
				air.Introspector.times ++ ; //we should never be here - just in case, we should increment this
			
			if(typeof air.Introspector.config.useAirDebugHtml=='undefined'||air.Introspector.config.useAirDebugHtml==false){
				w = window.open('about:blank', 'debugger'+air.Introspector.times, 'width=640,height=480,resizable=1');
				if(w&&w.document){
					w.isAppSandbox = isAppSandbox;
					w.opener = window;
					w.iframeId = iframeId;
					w.initCallbacks = callbacks;	
					w.activeWindow = window;
					w.isLoaded = false;
					w.config = air.Introspector.config;
					w.document.open();
					w.document.write(air.Introspector.contentString);
					w.document.close();
				}
			}else{
				w = window.open('DebugUI.html', 'debugger'+air.Introspector.times, 'width=640,height=480,resizable=1');
				
				if(w&&w.document){
					w.opener = window;
					w.iframeId = iframeId;
					w.activeWindow = window;				
					w.config = air.Introspector.config;
					w.initCallbacks = callbacks;				
					w.isLoaded = false;
					w.isAppSandbox = isAppSandbox;
				}
			}
			return w;
//		}catch(e){
//			alert(e+' '+e.line);
//		}
	},
	
	/**
	*	@function loadDebugger
	*	@description Loads the debugger window, register callbacks until it is ready
	*	@runs in application sandbox only
	*/
	loadDebugger: function(callback, activateDebug){
			var htmlLoader;
			var loadDebugger = arguments.callee;
			if(loadDebugger.htmlLoader && typeof loadDebugger.htmlLoader.window.isLoaded != 'undefined'){
				if(loadDebugger.htmlLoader.window.isLoaded){
					callback(loadDebugger.htmlLoader.window.debugWindow);
				}else{
					if(loadDebugger.htmlLoader.window.initCallbacks){
						loadDebugger.htmlLoader.window.initCallbacks.push(callback); 
					}else{
						loadDebugger.initCallbacks.push(callback); 							
					}
				}
				return;
			}
			if(typeof loadDebugger.initCallbacks=='undefined'){
				loadDebugger.initCallbacks = [function(){
					delete loadDebugger.initCallbacks;
				}, callback];
			}else{
				loadDebugger.initCallbacks.push(callback);
			}
			if(isAppSandbox){
			    htmlLoader = air.Introspector.runtime.HTMLLoader.createRootWindow(false);
			    air.Introspector.addEventListener(htmlLoader, air.Introspector.runtime.Event.HTML_DOM_INITIALIZE, function(){
					try{ 
						air.Introspector.removeEventListener(htmlLoader, air.Introspector.runtime.Event.HTML_DOM_INITIALIZE, arguments.callee);
						htmlLoader.window.initCallbacks = loadDebugger.initCallbacks;
						htmlLoader.window.isLoaded = false;
						htmlLoader.window.config = air.Introspector.config;
						htmlLoader.window.activateDebug = activateDebug;
						htmlLoader.window.isAppSandbox = isAppSandbox;
					}catch(e){ 
						air.Introspector.runtime.trace(e); 
						air.Introspector.runtime.trace(e.line); 
					}
				});
				htmlLoader.window.isLoaded = false;

			    var nativeWindow = htmlLoader.stage.nativeWindow;
				nativeWindow.width = 640;
				nativeWindow.height = 480;
			    air.Introspector.addEventListener(htmlLoader, runtime.flash.events.HTMLUncaughtScriptExceptionEvent.UNCAUGHT_SCRIPT_EXCEPTION, function(e){
			         air.Introspector.logError(e.exceptionValue, {htmlLoader: self.htmlLoader});
			         e.preventDefault();
			    });
			    if(typeof air.Introspector.config.useAirDebugHtml=='undefined'||air.Introspector.config.useAirDebugHtml==false){
					if(typeof htmlLoader.placeLoadStringContentInApplicationSandbox!='undefined'){
							//since AIR1.5 the htmlLoader will not allow string load in app sandbox
							htmlLoader.placeLoadStringContentInApplicationSandbox= true;
					}
					htmlLoader.loadString(air.Introspector.contentString);
					if(typeof htmlLoader.placeLoadStringContentInApplicationSandbox!='undefined'){
							//switch it back to false after load is complete
							htmlLoader.placeLoadStringContentInApplicationSandbox= false;
					}
			    }else{
			        htmlLoader.load(new air.Introspector.runtime.URLRequest('app:/DebugUI.html'));
			    }
			}else{
				air.Introspector.noBridge(function(){
					var w = air.Introspector.tryCreateWindow(loadDebugger.initCallbacks);
					if(w){
						var htmlLoader = {window: w};	
						loadDebugger.htmlLoader = htmlLoader;	
					}
				});
			}	
			loadDebugger.htmlLoader = htmlLoader;		
	},

	/**
	*	@function findDebugWindow
	*	@description Look up the Introspector in other windows. Maybe somebody else just opened it before us. 
	*/
    findDebugWindow: function(){
		if(isAppSandbox){
			try{
	    		if(air.Introspector.debugWindow&&air.Introspector.debugWindow.nativeWindow.closed==false)
		    	   return air.Introspector.debugWindow;
			}catch(e){
			}
			try{
		        var htmlWindows = air.Introspector.getHtmlWindows(true);
		        for(var i=htmlWindows.length-1;i>=0;i--){
		            try{
		                if(typeof htmlWindows[i].htmlLoader.window.air!='undefined'
		                   && typeof htmlWindows[i].htmlLoader.window.air.Introspector!='undefined'
		                       && typeof htmlWindows[i].htmlLoader.window.air.Introspector.debugWindow!='undefined'
								&& htmlWindows[i].htmlLoader.window.air.Introspector.debugWindow.nativeWindow.closed==false 
									&& htmlWindows[i].htmlLoader.window.isAppSandbox )
		                    {
		                        return htmlWindows[i].htmlLoader.window.air.Introspector.debugWindow;
		                    }
		            }catch(e){
		                //this window is not initialized yet
		                //just get next window
		            }
		        }
			}catch(e){}
		}else{
			return air.Introspector.debugWindow;
		}
        return null;
    },

	//application browser formats
	//		0 - text
	//		1 - images
	//		2 - xml (you may want to add your own xml type here)
    formats : { 'png':1, 'gif':1, 'zip':1, 'air':1, 'jpg':1, 'jpeg':1,
                 'txt':0, 'html':0, 'js':0, 'xml':2, 'opml':2, 'css':0, 'htm':0, '':0 },
    

	/**
	*	@function canInit
	*	@description Check if we got parentSandboxBridge available
	*	@disabled 
	*/
	canInit: function(){
/*		if(!isAppSandbox&&typeof parentSandboxBridge=='undefined'){
			alert('You need to include AIRIntrospector.js in application sandbox too!');
			return false;
		}*/
		return true;
	},

	/**
	*	@function logArguments
	*	@description 
	*/
	logArguments: function(args, config){
		if(!air.Introspector.canInit()) return;
		config.timestamp = new Date();
       	air.Introspector.init(config.type=='error', true, function(){
			air.Introspector.debugWindow.logArguments(args, config);
		});
        
    },
    /**
	*	@function logError
	*	@description 
	*/
    logError: function(error, config){
        air.Introspector.init(false, true, function(){
			air.Introspector.debugWindow.logError(error, config);
		});
    },
    
	/**
	*	@function showCssElement
	*	@description 
	*/
	showCssElement: function(element){
		var debugWindow = air.Introspector.findDebugWindow();
		if(debugWindow){
			debugWindow.showCssElement(element);
		}
	},

	checkIfIsInstanceOfHTMLLoader: function (child){
		var className = runtime.flash.utils.getQualifiedClassName(child);
		if( className == "flash.html::HTMLLoader" ) return true;
		if( className == "mx.core::FlexHTMLLoader" ) return true;
		return false;
	},
	/**
	*	@function findLoader
	*	@description Finds the first HTMLLoader in flash display object list
	*/
    findLoader: function (stage, loaders){
		try{
	        for(var i=stage.numChildren-1;i>=0;i--){
	            var child = stage.getChildAt(i);
				if(air.Introspector.checkIfIsInstanceOfHTMLLoader(child)){
				   loaders.push([child]);
				}else if(child.htmlLoader!=null&&air.Introspector.checkIfIsInstanceOfHTMLLoader(child.htmlLoader)){
				   loaders.push([child.htmlLoader, child.id || child.toString()]);
				}else{
					air.Introspector.findLoader(child, loaders);
	            }
	        }
		}catch(e){
		}
        return null;
    }, 
    
	/**
	*	@function getHtmlWindows
	*	@description Returns an array of all HTML windows
	*/
    getHtmlWindows: function(includeInspectors){
		if(isAppSandbox){
 	       var windowNodes = [];
	        var windows = air.Introspector.runtime.NativeApplication.nativeApplication.openedWindows;
	        for(var i=windows.length-1;i>=0;i--){
				var loaders = [];
	            air.Introspector.findLoader(windows[i].stage, loaders);
	            for(var j=loaders.length-1;j>=0;j--){
					var loaderItem = loaders[j];
					var loader = loaderItem[0];
					var label = loaderItem[1];
					if(typeof includeInspectors=='undefined' && typeof loader.window!='undefined' && typeof loader.window.air!='undefined' && typeof loader.window.air.Introspector!='undefined' &&
						typeof loader.window.air.Introspector.localIframeDebug != 'undefined' ){
							continue;
						}            
	                windowNodes.push({
	                    nativeWindow: windows[i],
	                    stage: windows[i].stage,
	                    htmlLoader : loader,
						label: label
	                });
	            }
	        }
	        return windowNodes;
		}else{
			//should not be here
			return [];
		}
   },
   
	/**
	*	@function twoDigits
	*	@description int 2 string with two digits
	*/
	twoDigits: function(val){
	     if(val<10) return '0'+val;
	     return val+'';
	},

	/**
	*	@function escapeHtml
	*	@description Escapes html in order to display it in html
	*/
	escapeHtml: function(html){      
	     return (html+'').replace(/&/g, '&amp;').replace(/"/g, "&quot;").replace(/</g, '&lt;').replace(/>/g, '&gt;');
	},

   	tree: { },


	/**
	*	@function isNumberObject
	*	@description 
	*/
	isNumberObject: function(obj){
	     try{
	         //can we catch isNaN only for NaN
	         return (obj+0==obj&&!isNaN(obj));
	     }catch(e){
	     }
	     return false;
	 },

	/**
	*	@function isStringObject
	*	@description 
	*/
	isStringObject: function(obj){
	    try{
	        return (typeof(obj.match) != "undefined" && obj.match.toString().indexOf("[native code]")>0);
	    }catch(e){
	    }
	    return false;
	},

	/**
	*	@function isDateObject
	*	@description 
	*/
	isDateObject: function(obj){
	    try{
	        return (typeof(obj.getDate) != "undefined" && obj.getDate.toString().indexOf("[native code]")>0);
	    }catch(e){
	    }
	    return false;
	},

	/**
	*	@function isArgumentsObject
	*	@description 
	*/
   isArgumentsObject: function(obj){
       try{
           return obj.toString()=='[object Arguments]';
       }catch(e){
       }
       return false;
   },

	/**
	*	@function isXMLObject
	*	@description 
	*/
	isXMLObject: function(obj){
		try{
			if(obj.xmlVersion&&obj.firstChild!=null)
	        	return obj.xmlVersion!='';
	    }catch(e){
	    }
	    return false;
	},

	/**
	*	@function isArrayObject
	*	@description 
	*/
	isArrayObject: function(obj){
       try{
           return (typeof(obj.push) != "undefined" && obj.push.toString().indexOf("[native code]")>0);
       }catch(e){
       }
       return false;
	},

	/**
	*	@function isItemNative
	*	@description 
	*/
	isItemNative: function(obj){
       try{
           return (typeof(obj.item) != "undefined" && obj.item.toString().indexOf("[native code]")>0);
       }catch(e){
       }
       return false;
	},
   

	/**
	*	@function dump
	*	@description 
	*/

	dump: function (obj, levels, level){
		if(air.Introspector.isArgumentsObject(obj)&&obj.length==1)
			return air.Introspector.dump(obj[0]);
		if(typeof levels=='undefined') { levels=1; }
		if(typeof level=='undefined') { level=0; }		
        try{
            if(typeof obj=='undefined'){ return '[undefined]'; }
			if(obj==null){	return '[null]'; }
			var list = [];
//            if(air.Introspector.isXMLObject(obj)){
//		disable for the moment
//            	return;
//            }  
			if(air.Introspector.isStringObject(obj)
				||air.Introspector.isNumberObject(obj)
					||air.Introspector.isDateObject(obj)){
				if(level==0){
					try{
						return obj+'';
					}catch(e){ 
						return e+''; 
					};
				}
				return'';
			}
            var isItemNative = air.Introspector.isItemNative(obj);
            var parseArray = air.Introspector.isArrayObject(obj)||air.Introspector.isArgumentsObject(obj)||isItemNative;
            var parseHash =  !parseArray || isItemNative;
            if (parseArray){
				var l = obj.length;
            	for(var i=0;i<l;i++){
                    var value;
                    try{
                        value = obj[i];
						if(typeof value=='undefined') {value = '[undefined]'};
                    }catch(e){
                        value = e+'';
                    }
					list.push([i,value]);
                }
            } 
            if(parseHash) {
                for(var i in obj){
                    var value;
                    try{
                        value = obj[i];
						if(typeof value=='undefined') {value = '[undefined]'};
                    }catch(e){
                        value = e+'';
                    }
					list.push([i,value]);
                }
            }

			if(air.Introspector.config.debugRuntimeObjects && !parseArray){
				try{
					var typeDescription = runtime.flash.utils.describeType(obj);
					if(!this.domParser) this.domParser = new DOMParser();
					var typeXml = this.domParser.parseFromString(typeDescription, "text/xml");
					var child = typeXml.firstChild.firstChild;
					while(child){
						if(child.nodeName=='accessor'||child.nodeName=='constant'||child.nodeName=='method'||child.nodeName=='variable'){
							var name = child.getAttribute('name');
							if(name!=null && name!='prototype'){
			                    try{
									list.push([name,obj[name]]);
			                    }catch(e){
									list.push([name,'']+'');
			                    }
							}
						}
						child = child.nextSibling;
					}
				}catch(e){
					//just hide the error
				}
            }
            list.sort(function(node1, node2){
            	var isNode1Number = parseInt(node1[0])==node1[0];
            	var isNode2Number = parseInt(node2[0])==node2[0];
            	if(isNode1Number&&isNode2Number){
            		return parseInt(node1[0])-parseInt(node2[0]);
            	} 
            	if(isNode1Number){
            		return -1;
            	}
            	if(isNode2Number){
            		return 1;
            	}
            	if(node1[0].toLowerCase()==node2[0].toLowerCase())
                   return 0;
                if(node1[0].toLowerCase()<node2[0].toLowerCase())
                   return -1;
                return 1;
            });
			if(list.length){
				var prefix = '';
				for(var i=level;i>0;i--) prefix+='    ';
				var l = list.length;
				var strList = [];
				if(parseArray)	strList.push(prefix+'[\r\n');
				else			strList.push(prefix+'{\r\n');
				for(var i=0;i<l;i++){
					try{
						var zl = (list[i][0]+'').length+1;
						var miniPrefix = '';
						for(var j=0;j<zl;j++) miniPrefix+=' ';
						if(typeof list[i][1]=='function'){
							strList.push(prefix+'  '+list[i][0]+'=[function]\r\n');
						}else if(air.Introspector.isDateObject(list[i][1])){
							strList.push(prefix+'  '+list[i][0]+'='+(new Date(list[i][1])+'').replace(/\n/g, '\r\n'+prefix+miniPrefix)+',\r\n');
						}else{
							strList.push(prefix+'  '+list[i][0]+'='+(list[i][1]+'').replace(/\n/g, '\r\n'+prefix+miniPrefix)+'\r\n');
						}
					}catch(e){
						strList.push(prefix+'  '+list[i][0]+'='+e+'\r\n');
					}
					if(level<levels){
						strList.push(air.Introspector.dump(list[i][1], levels, level+1));
					}
				}
				if(parseArray)	strList.push(prefix+']\r\n');
				else			strList.push(prefix+'}\r\n');
				if(level){
					return strList.join('');
				}else{
					return (strList.join(''));
				}
			}
        }catch(e){
            air.Introspector.Console.error(e);
        } 
		return '';
	},




    /**
	*	---------------------------------------------------------------------------------------
	*	@extracted This is extracted from spry framework and removed support for other browsers.
	*	@description Finds the precise position of the dom element node
	*/   
	
	/**
	*	@function camelize
	*	@description 
	*/
	
	camelize : function(stringToCamelize)
            {
                if (stringToCamelize.indexOf('-') == -1){
                    return stringToCamelize;    
                }
                var oStringList = stringToCamelize.split('-');
                var isFirstEntry = true;
                var camelizedString = '';
            
                for(var i=0; i < oStringList.length; i++)
                {
                    if(oStringList[i].length>0)
                    {
                        if(isFirstEntry)
                        {
                            camelizedString = oStringList[i];
                            isFirstEntry = false;
                        }
                        else
                        {
                            var s = oStringList[i];
                            camelizedString += s.charAt(0).toUpperCase() + s.substring(1);
                        }
                    }
                }
            
                return camelizedString;
            },

	/**
	*	@function getStyleProp
	*	@description 
	*/
	getStyleProp : function(element, prop)
            {
                var value;
                try
                {
                    if (element.style)
                        value = element.style[air.Introspector.camelize(prop)];
            
                    if (!value)
                    {
                        if (document.defaultView && document.defaultView.getComputedStyle)
                        {
                            var css = document.defaultView.getComputedStyle(element, null);
                            value = css ? css.getPropertyValue(prop) : null;
                        }
                        else if (element.currentStyle) 
                        {
                                value = element.currentStyle[air.Introspector.camelize(prop)];
                        }
                    }
                }
                catch (e) {}
            
                return value == 'auto' ? null : value;
            },
   

	/**
	*	@function getIntProp
	*	@description 
	*/
	getIntProp : function(element, prop){
                var a = parseInt(air.Introspector.getStyleProp(element, prop),10);
                if (isNaN(a))
                    return 0;
                return a;
            },


	/**
	*	@function getBorderBox
	*	@description 
	*/
	getBorderBox : function (el, doc) {
                doc = doc || document;
                if (typeof(el) == 'string') {
                    el = doc.getElementById(el);
                }
            
                if (!el) {
                    return false;
                }
            
                if (el.parentNode === null || air.Introspector.getStyleProp(el, 'display') == 'none') {
                    //element must be visible to have a box
                    return false;
                }
            
                var ret = {x:0, y:0, width:0, height:0};
                var parent = null;
                var box;

            	var str = el.nodeName;
			
                ret.x = el.offsetLeft;
                ret.y = el.offsetTop;
                ret.width = el.offsetWidth;
                ret.height = el.offsetHeight;

				parent = el.offsetParent;

                if (parent != el) {
                    while (parent) {
                        ret.x += parent.offsetLeft;
                        ret.y += parent.offsetTop;
                        ret.x += air.Introspector.getIntProp(parent, "border-left-width");
                        ret.y += air.Introspector.getIntProp(parent, "border-top-width");                        
						str+=':'+parent.nodeName;
                        parent = parent.offsetParent;
                    }
                }

                // opera & (safari absolute) incorrectly account for body offsetTop
                switch (air.Introspector.getStyleProp(el, 'position')){
 					case 'absolute':
                    	ret.y -= doc.body.offsetTop;
						break;
					case 'fixed':
	                    ret.y += doc.body.scrollTop;
	                    ret.x += doc.body.scrollLeft;
						break;
				};
                    
                if (el.parentNode)
                    parent = el.parentNode;
                else
                    parent = null;
                
				if (parent!=null&&parent.nodeName){
                    var cas = parent.nodeName.toUpperCase();
                    while (parent && cas != 'HTML') {
                        cas = parent.nodeName.toUpperCase();
                        ret.x -= parent.scrollLeft;
                        ret.y -= parent.scrollTop;
                        if (parent.parentNode)
                            parent = parent.parentNode;
                        else
                            parent = null;
                    }
                }

/*				ret.y -= el.ownerDocument.body.scrollTop;
				ret.x -= el.ownerDocument.body.scrollLeft;				
*/
                // adjust the margin
                var gi = air.Introspector.getIntProp;
                var btw = gi(el, "margin-top");
                var blw = gi(el, "margin-left");
                var bbw = gi(el, "margin-bottom");
                var brw = gi(el, "margin-right");
                ret.x -= blw;
                ret.y -= btw;
                ret.height += btw + bbw;
                ret.width += blw + brw;

			//	air.Introspector.Console.log(ret);
                return ret;
            },

	/**
	*	---------------------------------------------------------------------------------------
	*/

	/**
	*	@function writeConfigFile
	*	@description 
	*/
	writeConfigFile: function(config, fromRemoteSandbox){
		if(isAppSandbox){
			var file = runtime.flash.filesystem.File.applicationStorageDirectory.resolvePath('AIRIntrospector'+(fromRemoteSandbox?'Remote':'')+'.cfg');
			var fs = new runtime.flash.filesystem.FileStream();
			fs.open(file, runtime.flash.filesystem.FileMode.WRITE);
			fs.writeObject(config);
			fs.close();
		}else{
			if(typeof activeWindow=='undefined'){
				air.Introspector.noBridge(function(){
					parentSandboxBridge.air_Introspector_writeConfigFile(config);
				});
			}else{
				activeWindow.setTimeout(function(){
					try{
						activeWindow.air.Introspector.writeConfigFile(config);
					}catch(e){}
				}, 0);
			}
		}
	},



	/**
	*	@function readConfigFile
	*	@description 
	*/
	readConfigFile: function (fromRemoteSandbox, callback){
		if(isAppSandbox){
			var file = runtime.flash.filesystem.File.applicationStorageDirectory.resolvePath('AIRIntrospector'+(fromRemoteSandbox?'Remote':'')+'.cfg');
			if(file.exists){
				var fs = new runtime.flash.filesystem.FileStream();
				fs.open(file, runtime.flash.filesystem.FileMode.READ);
				var config = fs.readObject();
				fs.close();
				return config;
			}
		}else{
			if(typeof activeWindow=='undefined'){
				air.Introspector.noBridge(function(){
					var config = parentSandboxBridge.air_Introspector_readConfigFile();
					callback(config);
				});				
			}else{
				activeWindow.setTimeout(function(){
					try{
                        activeWindow.air.Introspector.readConfigFile(true, function(config){
                            setTimeout(function(){ callback(config); }, 0);							
                        });
					}catch(e){}
				}, 0);
			}
		}
		return {};
	},
	
	
	
	/**
	*	@function writeConsoleToClipboard
	*	@description 
	*/

	writeConsoleToClipboard: function(str){
		if(isAppSandbox){		
			runtime.flash.desktop.Clipboard.generalClipboard.clear();
			runtime.flash.desktop.Clipboard.generalClipboard.setData(runtime.flash.desktop.ClipboardFormats.TEXT_FORMAT, 
					str, false);		
		}else{
			if(typeof activeWindow=='undefined'){
				air.Introspector.noBridge(function(){
					parentSandboxBridge.air_Introspector_writeConsoleToClipboard(str);
				});				
			}else{
				activeWindow.setTimeout(function(){
					try{
						activeWindow.air.Introspector.writeConsoleToClipboard(str);
					}catch(e){}
				});
			}
		}
	},	


	/**
	*	@function writeConsoleToFile
	*	@description 
	*/
	writeConsoleToFile: function(str){
		if(isAppSandbox){		
			var file = runtime.flash.filesystem.File.desktopDirectory;
			var self = this;
			file.addEventListener(runtime.flash.events.Event.SELECT, function(evt){ 
				var newFile = evt.target;
  			    var stream = new runtime.flash.filesystem.FileStream();
			        stream.open(newFile, runtime.flash.filesystem.FileMode.WRITE);
			        stream.writeUTFBytes(str);
			        stream.close();
				});
			file.browseForSave('Console dump file...');
		}else{
			if(typeof activeWindow=='undefined'){
				air.Introspector.noBridge(function(){
					parentSandboxBridge.air_Introspector_writeConsoleToFile(str);
				});				
			}else{
				activeWindow.setTimeout(function(){
					try{
						activeWindow.air.Introspector.writeConsoleToFile(str);
					}catch(e){}
				});
			}
		}
	},
	
	
	
	
	/**
	*	@function noBridge
	*	@description Alerts the user that no parent sandbox bridge is installed 
	*/
	noBridge: function(callback){
		try{
			callback();
			return;
		}catch(e){
			air.Introspector.bridgeCallbacks.push(callback);
			air.Introspector.registerChildSandboxBridge();
			setTimeout(function(){
				throw { air_Introspector_setParentSandboxBridge: true, air_Introspector_version: air.Introspector.version , toString: function(){ return 'You need to include AIRIntrospector.js in your application sandbox.'; } };
			}, 0);				
		}
		
	},
	
	/**
	*	@function noChildBridge
	*	@description Alerts the user that no child sandbox bridge is installed 
	*/
	noChildBridge: function(iframe){
		if(!air.Introspector.secondBridgeTry){
			var iframeStr = '';
			if(typeof iframe!='undefined'){
				iframeStr = " Check the following iframe [id: "+iframe.id+"]";
			}
			alert('Child sandbox bridge is not defined or has been rewritten. You need to include AIRIntrospector.js in child sandbox.'+iframeStr);			
			air.Introspector.secondBridgeTry = true;
		}
	},
	

});

//-------------------------------------------------------------------------------------------------------------------------------------------------------
air.Introspector.version = '1.5';
})();

air.Introspector.contentString = '<!DOCTYPE html PUBLIC \"-\/\/W3C\/\/DTD XHTML 1.0 Transitional\/\/EN\" \"http:\/\/www.w3.org\/TR\/xhtml1\/DTD\/xhtml1-transitional.dtd\">\n<html xmlns=\"http:\/\/www.w3.org\/1999\/xhtml\">\n<head>\n<meta http-equiv=\"Content-Type\" content=\"text\/html; charset=UTF-8\" \/>\n<title>ADOBE AIR HTML\/JS Application Introspector<\/title>\n<style>\n\tbody{\n\t\toverflow:hidden;\n\t\t-khtml-user-select:none;\n\t    font-size: 11px;\n\t}\n\t\n\t*{ -khtml-user-drag:none; }\n\n\ta:focus{outline:none;}\n\n\t#tabPages{\n\tposition:absolute;\n\tleft:0px;\n\ttop:80px;\n\tright:0px;\n\tbottom:0px;\n\tborder-top:1px solid #999999;\n\t}\n\t\n\t#preventClickingTabLabels{\n\t\t  position:absolute;\n\t\t  left:0px;\n\t\t  top:60px;\n\t\t  height:10px;\n\t\t  right:0px;\n\t\t  background:#666666;\n\t}\n\t\n\t#tabPages>div{\n\t\tposition:absolute;\n\t\tleft:0px;\n\t\ttop:0px;\n\t\tbottom:0px;\n\t\tright:0px;\n\t\tvisibility:hidden;\n\t\toverflow:auto;\n\t}\n\t\n\t#tabPages>div.selected{\n\t\tvisibility:visible;\n\t}\n\t\n\t#windowSelector{\n\tposition:absolute;\n\ttop:0px;\n\tright:0px;\n\ttext-align:right;\n\tbackground:#4F4F4F;\n\tleft:300px;\n\tpadding: 10px;\n\tvertical-align: middle;\n\theight: 30px;\n\t\n\t}\n\t\n\t#domTab{\n\t\toverflow:auto;\n\t}\n\t\n\t#console {\n\t\tposition:absolute;\n\t\tleft:0px;\n\t\ttop:0px;\n\t\tbottom:30px;\n\t\tright:0px;\n\t\tmargin:0px;\n\t\tpadding:0px;\n\t\tlist-style-image:none;\n\t\tlist-style-type:none;\n\t\toverflow:auto;\n\t}\n\t\n\t\n\t#console > li{\n\t\tborder-bottom:1px solid #CCCCCC;\n\t\tposition:relative;\n\t}\n\n\t\n\t.typeBox{\n\t\tposition:absolute;\n\t\tleft:0px;\n\t\ttop:0px;\n\t\twidth:10px;\n\t\tmargin-left:0px !important;\n\t\theight:13px;\n\t\ttext-align:center;\n\t\tpadding:3px;\n\t\tpadding-top:0px;\n\t\tfont-family:Verdana;\n\t\tborder:1px solid #000000;\n\t\t-webkit-border-top-left-radius:9px;\n\t\t-webkit-border-top-right-radius:9px;\t\t\n\t\t-webkit-border-bottom-left-radius:9px;\t\t\n\t\t-webkit-border-bottom-right-radius:9px;\t\t\n\t}\n\t\n\t#console > li > *{\n\t\tmargin-left:10px;\n\t}\n\t\n\t.consoleFrom, .consoleTimestamp{\n\t\tcolor:#999999;\n\t}\n\t.consoleItemText{\n\t\tcolor:#003366;\n\t     \/*font-weight:bold;*\/\n\t\tmargin-left:2px ! important;\n\t\tpadding:3px;\n\t\t-khtml-user-select:auto;\n\t}\n\t\n\t.error .consoleItemText{\n        background:#FFFFE0;\n        color:#FF3030;\n    }\n    \n    .warn .consoleItemText{\n        background:#00FFFF;\n        color: #000000;\n    }\n\n\t.warn .typeBox{\n\t\tborder:1px solid #8db047;\n\t\tcolor:#000000;\n\t\tbackground:#ffff00;\n\t}\n\t.error .typeBox{\n\t\tborder:1px solid #c00504;\n\t\tcolor:#ffffff;\n\t\tfont-weight:bold;\n\t\tbackground:#fe0000;\n\t}\n\t.info .typeBox{\n\t\tborder:1px solid #113c9f;\n\t\tcolor:#ffffff;\n\t\tfont-style:italic;\n\t\tbackground:#0053fe;\n\t}\n\t.warn .consoleItemText, .info .consoleItemText, .error .consoleItemText{\n\t\tmargin-left:20px ! important;\n\t}\n\t\n\t#evalConsole{\n\t\tposition:absolute;\n\t\tleft:0px;\n\t\tbottom:0px;\n\t\theight:30px;\n\t\tright:0px;\n\t\tborder-top:1px solid #999999;\n\t}\n\t#evalConsoleText{\n\t\tposition:absolute;\n\t\tleft:30px;bottom:0px;\n\t\ttop:0px;right:0px;\n\t\tborder:none;\n\t}\n\t#evalConsoleLabel{\n\t\tposition:absolute;\n\t\tleft:0px;bottom:0px;\n\t\ttop:0px;\n\t\tpadding-top:5px;\n\t\tborder:none;\n\t}\n\t#tabLabels{\n\t\toverflow:hidden;\n\tmargin:0px;\n\tlist-style-image:none;\n\tlist-style-type:none;\n\tposition:absolute;\n\ttop:50px;\n\tleft:70px;\n\tright:0px;\n\theight:30px;\n\tpadding-left:5px;\n\tborder-top-width: 0px;\n\tborder-top-style: solid;\n\tborder-top-color: #999999;\n\tbackground-color: #BBBBBB;\n\tborder-left-width: 2px;\n\tborder-left-style: solid;\n\tborder-left-color: #4F4F4F;\n\tpadding-top: 0px;\n\tpadding-right: 0px;\n\tpadding-bottom: 0px;\n\t}\n\t#tabLabels>li, #tabLabels>li.bounceOff{\n\tdisplay:block;\n\tpadding:5px;\n\tcursor:pointer;\n\tfloat:left;\n\theight:12px;\n\t-webkit-border-top-left-radius:5px;\n\t-webkit-border-top-right-radius:5px;\n\tfont-family: Arial, Helvetica, sans-serif;\n\tfont-size: 11px;\n\tfont-style: normal;\n\tfont-weight: 600;\n\tcolor: #5A6F7F;\n\tline-height: normal;\n\tmargin-top: 5px;\n\tmargin-right: 2px;\n\tmargin-bottom: 2px;\n\tmargin-left: 2px;\n\ttext-transform: uppercase;\n\t}\n\t\n\t\n\t#tabLabels>li.bounceOn{\n\t\tbackground:#BDB0FF;\n\t\tborder-bottom:none;\n\t\txcolor:#ffffff;\n\t}\n\t\n\t#tabLabels>li.selected{\n\t\tbackground:#DADADA;\n\t\tcolor:#0066CC;\n\t}\n\t\n\t#tabLabels>li:hover{\n\tbackground-color: #DADADA;\n\tcolor: #0066CC;\n\t}\n\t\n\t\n\t#toolToggle{\n\tmargin:0px;\n\tlist-style-image:none;\n\tlist-style-type:none;\n\tposition:absolute;\n\ttop:50px;\n\tleft:-2px;\n\tright:0px;\n\theight:30px;\n\tpadding-left:5px;\n\tborder-top-width: 0px;\n\tborder-top-style: solid;\n\tborder-top-color: #999999;\n\tbackground-color: #BBBBBB;\n\tborder-left-width: 2px;\n\tborder-left-style: solid;\n\tborder-left-color: #4F4F4F;\n\tpadding-top: 0px;\n\tpadding-right: 0px;\n\tpadding-bottom: 0px;\n\twidth: 95px;\n\t}\n\t\n\t#toolToggle>li, #tabLabels>li.bounceOff{\n\tdisplay:block;\n\tpadding:5px;\n\tcursor:pointer;\n\tfloat:left;\n\theight:12px;\n\t-webkit-border-top-left-radius:5px;\n\t-webkit-border-top-right-radius:5px;\n\tfont-family: Arial, Helvetica, sans-serif;\n\tfont-size: 11px;\n\tfont-style: normal;\n\tfont-weight: bold;\n\tcolor: #4F4F4F;\n\tline-height: normal;\n\tmargin-top: 5px;\n\tmargin-right: 2px;\n\tmargin-bottom: 2px;\n\tmargin-left: 2px;\n\ttext-transform: uppercase;\n\t}\n\t\n\t#toolToggle>li.selected{\n\t\tbackground:#DADADA;\n\t\tcolor:#333333;\n\t}\n\t\n\t#toolToggle>li:hover{\n\tbackground-color: #DADADA;\n\tcolor: #333333;\n\t}\n\t\n\t.selected2Tree > .treeLabel{\n\t\tfont-weight:bold;\n\t}\n\t.selected2Tree {\n\t\tborder:1px solid #eeeeee;\n\t\tmargin:-1px;\n\t}\n\t\n\t\n\t#toolsLabels{\n\tmargin:0px;\n\tlist-style-image:none;\n\tlist-style-type:none;\n\tposition:absolute;\n\ttop:0px;\n\tleft:0px;\n\tpadding-left:10px;\n\twidth:320px;\n\theight:40px;\n\tbackground:#4F4F4F;\n\tpadding-top: 10px;\n\tpadding-right: 10px;\n\tpadding-bottom: 5px;\n\tcolor:#FFFFFF;\n\tfont-size:14px;\n\tfont-family:Verdana, Arial, Helvetica, sans-serif;\n\tfont-weight:bold;\n\ttext-shadow:2px 2px 2px black;\n\t}\n\t#toolsLabels span{\n\t\t\tposition:relative;\n\t\t\ttop:-3px;\n\t\t\tleft:1px;\n\t\t\tfont-size:10px;\n\t\t\tfont-weight:normal;\n\t\t}\n\t\n\t#toolsLabels>li{\n\tpadding:5px;\n\tmargin:3px;\n\tmargin-right:0px;\n\tfloat:left;\n\theight:12px;\n\tbackground-color: #3C3C3C;\n\t}\n\t\n\t#toolsLabels>li.selected{\n\t\tbackground:#666666;\n\t\tcolor:#FFFFFF;\n\t}\n\t\n\t#toolsLabels>li:hover{\n\t\ttext-decoration:underline;\n\t}\n\t\n\t\n\t.treeNode{\n\t\tdisplay:block;\n\t\tposition:relative;\n\t\tpadding-top:20px;\n\t\ttext-decoration:none;\n\t\tcolor:#000000;\n\t}\n\t\n\t.selected3Tree > .treeLabel{\n\t\tfont-weight: normal !important;\n\t\tbackground: #FFCC00 !important;\n\t}\n\t\n\n\t.hover > .treeLabel, .hover  >.treeLabel2{\n\t\tbackground:#FFFFCC;\n\t\t-khtml-user-select:text;\n\t}\n\t\n\t.treeLabel{\n\t\tposition:absolute;\n\t\ttop:0px;\n\t\tleft:20px;\n\t\twidth:200px;\n\t\toverflow:visible;\n\t\tcursor:pointer;\n\t\twhite-space:nowrap;\n\t}\n\t\n\t.noLabel2 > .treeLabel{\n\t\twidth:auto !important;\n\t\toverflow:auto !important;\n\t}\n\t\n\t.treeLabel2, .treeEdit{\n\t\tposition:absolute;\n\t\ttop:0px;\n\t\tleft:220px;\n\t\tpadding-left:2px;\n\t\tcursor:pointer;\n\t\twhite-space:nowrap;\n\t}\t\n\t\n\n\t.editing > .treeEdit{\n\t\tdisplay:block !important;\n\t\twidth:400px;\n\t\tleft:220px;\n\t\tright:0px;\n\t}\n\t\n\t.treeNode > .treeEdit{\n\t\tdisplay:none;\n\t}\n\t\n\t.tagDomNodeName{\n\t\tcolor:#ee00ee;\n\t}\n\t\n\t.domTreeNode .treeLabel{\n\t\twidth:auto;\n\t}\n\t\n\t.tagDomNodeAttribute{\n        color:#0000ee;\n    }\n\t\n\t.editing > .treeLabel2{\n\t\tdisplay:none !important;\n\t}\n\t.treeNode > .treeLabel2{\n\t\tdisplay:block;\n\t}\n\t\t\n\t.treeAnchorMac{\n\t\tposition:absolute;\n\t\ttop:0px;\n\t\tleft:0px;\n\t\tmargin:1px;\n\t\twidth:12px;\n\t\theight:12px;\n\t\tcursor:pointer;\n\t\tfont-size:13px;\n\t\toverflow:hidden;\n\t\tcolor:#ffffff;\n\t\tbackground:#cccccc;\n\t\t-webkit-border-radius:6px;\n\t}\n\t.treeAnchorMac div{\n\t\tmargin-top: -3px;\n\t\twidth:25px;\n\t\tmargin-left: -6px;\n\t\tmargin-right: -2px;\n\t\ttext-align:center;\n\t}\n\t.treeNodeEmpty{\n\t\tdisplay:none;\n\t}\n\t.treeAnchorWin{\n\t\tposition:absolute;\n\t\ttop:0px;\n\t\tleft:0px;\n\t\tmargin:1px;\n\/*\t\tborder:1px solid #999999;\n\t\tborder-left:1px solid #eeeeee;\n\t\tborder-top:1px solid #eeeeee;*\/\n\t\twidth:12px;\n\t\theight:12px;\n\t\tcursor:pointer;\n\t\tfont-size:15px;\n\t\toverflow:hidden;\n\t\tcolor:#ffffff;\n\t\tbackground:#cccccc;\n\t\t-webkit-border-radius:6px;\n\t}\n\t.treeAnchorWin div{\n\t\tmargin-top: -4px;\n\t\twidth:25px;\n\t\tmargin-left: -6px;\n\t\tmargin-right: -3px;\n\t\ttext-align:center;\n\t}\n\t\n\t.treeChildren{\n\t\tmargin-left:20px;\n\t}\n\t\n\t.nodeEndLabelDiv, .nodeEndLabelDiv *{\n\t   display:none;\t\n\t}\n\t\n\t.nodeEndLabelDivVisible{\n       margin-left:20px;\n\t   padding-bottom:5px;  \n\t   white-space:nowrap;\n    }\n\t\n\t.treeText{\n\t\tposition:relative;\n\t\tmargin:0px;\n\t\ttext-decoration:none;\n\t\tcolor:#000000;\n\t\tpadding:0px;\n\t\n\t\t\n\t}\n\t.treePreLine{\n\t\tposition:absolute;\n\t\ttop:0px;\n\t\ttext-align:right;\n\t\twidth:45px;\n\t\tpadding-right:3px;\n\t\tpadding:0px;\n\t\tbackground:#eeeeee;\n\t}\n\t\n\t.selected3Tree > .treePreLine{\n\t\tbackground:#cccccc;\n\t\tcolor:blue;\n\t}\n\t\n\t.selected3Tree > img{\n\t\toutline: 1px solid blue;\n\t}\n\t.treePreText{\n\t\tmargin-left:49px;\n\t\tpadding-left:4px;\n\t\t-khtml-user-select:auto;\n        border: 1px solid #ffffff;\n\t}\n\t\n\t.inPlaceEditor{\n                cursor:pointer;\n                display:inline;\n                position:relative;\n     }\n\t\n\t\/* TREE CLASSES\n\t*\/\n\t#html2Tab{\n\t\toverflow:hidden !important;\n\t}\n\t#html2Div{\n\t\tposition:absolute;\n\t\tleft:0px;\n\t\ttop:0px;\n\t\tbottom:0px;\n\t\toverflow:auto;\n\t}\n\t#html2Split{\n\t\tposition:absolute;\n\t\twidth:5px;\n\t\ttop:0px;\n\t\tbottom:0px;\n\t\tbackground:#cccccc;\n\t\tcursor:pointer;\n\t}\n\t#css2Div{\n\t\tposition:absolute;\n\t\ttop:0px;\n\t\tbottom:0px;\n\t\tright:0px;\n\t}\n\t#css2TabLabels{\n\t\tmargin:0px;\n\t\tpadding:0px;\n\t\tbackground:#cccccc;\n\t\tlist-style-image:none;\n\t\tlist-style-type:none;\n\t\tposition:absolute;\n\t\tborder-bottom:5px solid #666666;\n\t\ttop:0px;\n\t\tleft:0px;\n\t\tright:0px;\n\t\theight:20px;\n\t\tpadding-left:0px;\n\t}\n\t#css2TabLabels>li{\n\t\tdisplay:block;\n\t\tpadding:3px;\n\t\tpadding-bottom:0px;\n\t\tmargin:2px;\n\t\tmargin-left:0px;\n\t\tmargin-bottom:0px;\n\t\tfloat:left;\n\t\theight:15px;\n\t\tbackground:#ffffff;\n\t\t-webkit-border-top-left-radius:5px;\n\t\t-webkit-border-top-right-radius:5px;\t\t\n\t\t\tcursor:pointer;\n\t}\n\t\n\t#css2TabLabels>li.selected{\n\t\tbackground:#666666;\n\t\tcolor:#FFFFFF;\n\t}\n\t\n\t\n\t#css2TabPages{\n\t\tposition:absolute;\n\t\tleft:0px;\n\t\ttop:24px;\n\t\tright:0px;\n\t\tbottom:0px;\n\t\tborder-top:1px solid #999999;\n\t}\n\n\t#css2TabPages>div{\n\t\tposition:absolute;\n\t\tleft:0px;\n\t\ttop:0px;\n\t\tbottom:0px;\n\t\tright:0px;\n\t\tvisibility:hidden;\n\t\toverflow:auto;\n\t}\n\t\n\t#tabPages>div.selected  #css2TabPages>div.selected{\n\t\tvisibility:visible;\n\t}\n\t\n\t#windowList{\n\t\twidth:100px;\n\t\t\n\t}\n\t.menu {\n\t\tposition:absolute;\n\t\tdisplay:none;\n\t\tbackground:white;\n\t\tborder-bottom:1px solid #cccccc;\n\t\tborder-right:1px solid #cccccc;\t\t\n\t\tfont-size:12px;\n\t\tbackground:#eeeeee;\n\t\twidth:170px;\n\t}\n\t.menu ul{\n\t\tmargin:0px;\n\t\tpadding:0px;\n\t\tlist-style-image:none;\n\t\tlist-style-type:none;\n\t}\n\t.menu li a{\n\t\tdisplay:block;\n\t\tpadding:5px;\n\t\tcolor:#333333;\n\t\tborder-bottom:1px solid #eeeeee;\n\t\ttext-decoration:none;\n\t}\n\t.menu li a:hover{\n\t\tbackground:blue;\n\t\tcolor:white;\n\t}\n\t\n\t#searchTextBox{\n\t\tposition:absolute;\n\t\tright:20px;\n\t\tbottom:40px;\n\t\tdisplay:none;\n\t\tbackground:white;\n\t\tborder:1px solid #cccccc;\n\t\tpadding:5px;\n\t\t-webkit-border-radius:5px;\t\t\n\t}\n\t.searchselect{\n\t\t-khtml-user-select:text !important;\n\t}\n\tpre{\n\t\tmargin:0px;\n\t\tpadding:0px;\n\t}\n\t.selectable, .selectable *{\n\t\t-khtml-user-select:text !important; \n\t}\n\t\n\t.searchselect::selection{\n\t\tbackground:#38D878;                    \n\t}\n\t\n<\/style>\n<script>\n\/\/<![CDATA[\n\tvar air;\n\nfunction cloneConfig()\n{\n\tif(typeof config==\'undefined\') return;\n\tfor(var i in config){\n\t\tair.Introspector.config[i]=config[i];\n\t}\n}\n\nfunction doLoad(){\n\n\/*\tif(typeof runtime==\'undefined\'&&opener){\n\t\tvar air = opener.air;\n\t}else{\n\t\tloadMyAir();\n\t}*\/\n\tcloneConfig();\n\tdebugWindow = new air.Introspector.DebugWindow();\n\tair.Introspector.debugWindow = debugWindow;\n\ttry{\n\t\tdebugWindow.init(window);\n\t}catch(e) { alert(e+\' \'+e.line); }\t\n\t\n\ttry{\n\t\tvar l =initCallbacks.length;\n\t\tfor(var i=0;i<l;i++){\n\t\t\tinitCallbacks[i](debugWindow);\n\t\t}\n\t}catch(e){alert(e+\' \'+e.line);}\n\tisLoaded = true;\n\n\t\t\n}\n\nfunction doUnload(){\n\tdebugWindow.dispose();\n}\n\n\nif(typeof air==\'undefined\') air = {};\nair.Introspector = { localIframeDebug: true };\n\n\n\n\n(function(){\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/introspector.js\n\t\t\/\/=======================================================================================================================================================\n\t\t\n\n\n\n\/\/----------------------------------------------------------------------------------------------------\n\/**\n*\t@API air.Introspector.Console\n*\t@description Exposes log, warn, info, error, dump to the user\n*\tTHIS IS THE ONLY SUPPORTED APIs \n*\/\n\nair.Introspector.Console = {\n\t\t log: function(){\n\t\t   \tair.Introspector.logArguments(arguments,\n\t\t\t \t\t{htmlLoader:isAppSandbox?window.htmlLoader:null});\n\t\t },\n\n\t\t warn : function(){\n\t\t        air.Introspector.logArguments(arguments,\n\t\t\t \t\t\t{htmlLoader:isAppSandbox?window.htmlLoader:null, type:\'warn\'});        \n\t\t },\n\n\t\t info : function(){\n\t\t        air.Introspector.logArguments(arguments,\n\t\t\t \t\t\t{htmlLoader:isAppSandbox?window.htmlLoader:null, type:\'info\'});        \n\t\t },\n \n\t\t error : function(){\n\t\t        air.Introspector.logArguments(arguments,\n\t\t\t \t\t\t{htmlLoader:isAppSandbox?window.htmlLoader:null, type:\'error\'});        \n\t\t }, \n\t\t dump : function(obj, level){\n\t\t\tair.Introspector.logArguments(air.Introspector.dump(obj, level),\n\t\t\t \t\t{htmlLoader: isAppSandbox?window.htmlLoader:null, usePre:true});\n\t\t }\n};\n\n\/\/----------------------------------------------------------------------------------------------------\n\nair.Introspector.config = {\n    showTimestamp: true,\t\t\t\t\t\t\/\/Make the console show time stamps before each line\n    showSender: true,\t\t\t\t\t\t\t\/\/Make the console show time stamps\n    wrapColumns: 2000,\t\t\t\t\t\t\t\/\/Source files are soft-wrapped at around 2000 columns by default \n\tflashTabLabels: true,\t\t\t\t\t\t\/\/Console and xhr columns can flash whenever something happend to them (eg. logged something). You can turn it off\n\t\t\t\t\t\t\t\t\t\t\t\t\/\/by setting this to false\n\tcloseIntrospectorOnExit: true,\t\t\t\t\/\/Makes the inspector window close when the last window closed\n\tdebugRuntimeObjects: true,\t\t\t\t\t\/\/Also expand ActionScript objects\n\tintrospectorKey:122,\t\t\t\t\t\t\/\/Inspect key - by default it is F11 (122)\n\tdebuggerKey:123, \t\t\t\t\t\t\t\/\/Toggle inspectors visibility - by default it is F12 (123)\n    useAirDebugHtml: false,\t\t\t\t\t\t\/\/Internal only\n};\n\n\n\/**\n*\t@module air.Introspector\n*\/\n\nair.Introspector.extend = function(dst, src){\n\t\/\/Take every property from src and put it in dst\n    for(var i in src){\n        dst[i]=src[i];\n    }\n};\n\n\/\/\tChecking if the user has configured Introspector using global AIRIntrospectorConfig variable\nif(typeof AIRIntrospectorConfig!=\'undefined\'){\n\tair.Introspector.extend(air.Introspector.config, AIRIntrospectorConfig);\n}\n\nvar eventListeners = [];\n\n\/\/\tCan not expand ActionScript objects from remote sandbox - we can not even access ActionScript from there \n\/\/\tjust disabled this feature\nif(!isAppSandbox) air.Introspector.config.debugRuntimeObjects = false;\n\n\/\/\tMade this use g\/setters in order to make it easy to send its value over the bridge\nair.Introspector.__defineGetter__(\'inspect\', function(){\n    return air.Introspector._inspect;\n});\n\nair.Introspector.__defineSetter__(\'inspect\', function(value){\n    air.Introspector._inspect=value;\n    if(!isAppSandbox){\n        setTimeout(function(){\n\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\tparentSandboxBridge.air_Introspector_setInspect(value);\n\t\t\t});\n        }, 0);\n    }else{\n\t\tif(!value){\n\t\t\ttry{\n\t\t\t\tair.Introspector.hideHighlight();\n\t\t\t}catch(e){}\n\t\t}\n\t}\n});\n\t\nair.Introspector.extend(air.Introspector, { \n\n\t\/**\n\t*\tMakes it easier to acces runtime packages\n\t*\tit makes sense only in the application sandbox\n\t*\/\n\truntime: isAppSandbox?{ \n\t\tHTMLLoader : window.runtime.flash.html.HTMLLoader,\n\t\tEvent : window.runtime.flash.events.Event,\n\t\tIOErrorEvent: window.runtime.flash.events.IOErrorEvent,\n\t\tNativeApplication: window.runtime.flash.desktop.NativeApplication,\n\t\tURLLoader : window.runtime.flash.net.URLLoader,\n\t\tURLRequest : window.runtime.flash.net.URLRequest,\n\t\tNativeWindowInitOptions : window.runtime.flash.display.NativeWindowInitOptions,\n\t\tCapabilities: window.runtime.flash.system.Capabilities,\n\t\ttrace : window.runtime.trace,\n\t}:null,\n\t\n\t_inspect: false,\n\tremoteInspect: false,\n    canClick: false,\n\n\tbridgeCallbacks: [],\n\t\n\t\n\t\/**\n\t*\tDifferent coloring styles for tag names, by default \'default\' is used\n\t*\tUndocumented feature\n\t*\t\t- you can change the color of elements while inspecting by tag name, check bellow the body: 0x00CC00 line,\n\t*\t\t  uncomment that line, duplicate and change it with your own colors\n\t*\/\n    highlightBgColors: {\n        \'default\': 0xFFCC00,\n        \/\/body: 0x00CC00,\n        \n    },\n\n\t\/**\n\t*\t@function trimRegExp\n\t*\t@description Trims spaces from a string\n\t*\t@private\n\t*\/\n    trimRegExp: \/^[\\s\\r\\n]*([\\s\\S]*?)[\\s\\r\\n]*$\/g,\n    trim:function(str){\n    \treturn str.replace(air.Introspector.trimRegExp, \'$1\');\n    },\n\n\t\/**\n\t*\t@function blockWrap\n\t*\t@description Wraps a string by air.Introspector.config.wrapColumns columns\n\t*\/\n    blockWrap: function(str){\n    \t\/\/used for spliting large lines in <pre>\n    \tvar cols = air.Introspector.config.wrapColumns;\n    \tvar lines = str.split(\/\\n\/);\n    \tvar buffer = [];\n    \tvar l = lines.length;\n    \tvar lineNumbers = [];\n    \tfor(var i=0;i<l;i++){\n    \t\tlineNumbers.push(i+1);\n    \t\tvar line = lines[i];\n    \t\twhile(line.length>cols){\n    \t\t\tbuffer.push(line.substr(0, cols));\n    \t\t\tline = line.substr(cols);\n    \t\t\tlineNumbers.push(\'\');\n    \t\t}\n    \t\tbuffer.push(line);\n    \t}\n    \tlineNumbers.push(\'EOF\');\n    \treturn [buffer.join(\'\\n\'), lineNumbers.join(\'\\n\')];\n    },\n\t\n\t\/**\n\t*\t@function getTextFormat\n\t*\t@description Returns a new flash TextField\n\t*\/\n    createTextField: function(parentSprite, fontSize, fontBold) {\n\t\tif(isAppSandbox){\n       \t\tvar tf = new runtime.flash.text.TextField();\n\t        tf.embedFonts = false;\n\t        tf.autoSize = runtime.flash.text.TextFieldAutoSize.LEFT;\n\t        tf.antiAliasType = runtime.flash.text.AntiAliasType.ADVANCED;\n\t        tf.defaultTextFormat = air.Introspector.getTextFormat(fontSize, fontBold);\n\t        tf.selectable = false;\n\t        tf.mouseEnabled = true;\n\t        tf.x = 4;\n\t        tf.text = \"\";\n\t        if(parentSprite.numChildren > 0) {\n\t            var sibling = parentSprite.getChildAt(parentSprite.numChildren - 1);\n\t            tf.y = sibling.y + sibling.height + 15;\n\t        }\n\t        parentSprite.addChild(tf);\n\t        return tf;\n\t\t}else{\n\t\t\t\/\/should not get here\n\t\t\treturn null;\n\t\t}\n    },\n    \/**\n\t*\t@function getTextFormat\n\t*\tReturns a new flash TextFormat\n\t*\tsee createTextField\n\t*\/\n    getTextFormat: function(fontSize, fontBold){\n\t\tif(isAppSandbox){\n        \tvar format = new runtime.flash.text.TextFormat();\n\t        format.size = fontSize;\n\t        format.font = \"Tahoma\";\n\t        format.bold = fontBold;\n\t        format.color = 0x330066;\n        \treturn format;\n\t\t}else{\n\t\t\t\/\/should not get here\n\t\t\treturn null;\n\t\t}\n    },\n    \n\t\/**\n\t*\t@function extendRect \n\t*\t@description Initializes the sprite with values from the rectangle\n\t*\/\n\textendRect: function(sprite, rect){\n\t\tsprite.x = rect.x;\n\t\tsprite.y = rect.y;\n\t\tsprite.width = rect.width;\n\t\tsprite.height = rect.height;\n\t\tsprite.scaleX = rect.scaleX;\n\t\tsprite.scaleY = rect.scaleY;\t\t\t\n\t},\n\t\n\t\n\t\/**\n\t*\t@function showHighlight\n\t*\t@description Shows a highlighting flash sprite using coordinates from rectangle\n\t*\/\n    showHighlight: function(rect){\n\t\tif(isAppSandbox){\n\t        \/\/dehilight everyone else\n\t        var ownedWindows = air.Introspector.getHtmlWindows();\n\t        for(var i=ownedWindows.length-1;i>=0;i--){\n\t                try{\n\t                    ownedWindows[i].htmlLoader.window.air.Introspector.hideHighlight();\n\t                }catch(e){\n\t                    \/\/no air.Introspector\n\t                }\n\t        }\n\t        air.Introspector.extendRect(air.Introspector.highlightSprite, rect);\n\t\t}else{\n\t\t\tsetTimeout(function(){\n\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\tparentSandboxBridge.air_Introspector_showHighlight(rect);\t\t\t\t\n\t\t\t\t});\n\t\t\t\t\n\t\t\t}, 0);\n\t\t\t\n\t\t\t\n\t\t}\n    },\n\n\t\/**\n\t*\t@function hideHighlight\n\t*\t@description Make the higlight box go away\n\t*\/\n    hideHighlight: function(){\n\t\tif(isAppSandbox){\n        \tair.Introspector.extendRect(air.Introspector.highlightSprite, {x:0, y:0, width:0, height:0, scaleX:0, scaleY:0});\n\t        air.Introspector.highlightText.visible = false;\n\t\t}else{\n\t\t\tsetTimeout(function(){\n\t\t\t\ttry{\n\t\t\t\t\tparentSandboxBridge.air_Introspector_hideHighlight();\n\t\t\t\t}catch(e){ \n\t\t\t\t\t\t\/\/ no bridge yet\n\t\t\t\t\t}\n\t\t\t}, 0);\n\t\t}\n    },\n\n\t\/**\n\t*\t@function remoteClick\n\t*\t@description Make the remote sandbox know that the inspection finished\n\t*\/\n\tremoteClick: function(){\n\t\tair.Introspector.debugWindow.finishInspect(false);\n        air.Introspector.hideHighlight();\n\t},\n\t\n\t\n\t\/**\n\t*\t@function createHighlight\n\t*\t@description \tCreates a flash sprite used to higlight elements\n\t*\t\t\t\t    By using this method we are sure that no dom manipulation is done and  \n\t*\t\t\t\t    no style is changed in HTML.\n\t*\/\n    createHighlight: function(){\n\t\tif(isAppSandbox){\n\t        var sprite = new runtime.flash.display.Sprite();\n\t        sprite.mouseEnabled =  false;\n\t        sprite.width = 0;\n\t        sprite.height = 0;\n\t        sprite.buttonMode = true;\n\t        var prevent = function(element, event, isClick){\n\t            air.Introspector.addEventListener(element, event, function(e){\n\t                if((air.Introspector.inspect||air.Introspector.remoteInspect) &&sprite.hitTestPoint(e.stageX, e.stageY)){\n\t                    e.preventDefault();\n\t                    e.stopPropagation();\n\t                    e.stopImmediatePropagation();\n\t                   \tif(isClick&&air.Introspector.canClick){\n\t\t\t\t\t\t\tif(air.Introspector.remoteInspect){\n\t\t\t\t\t\t\t\ttry{\n\t\t\t\t\t\t\t\t\tair.Introspector.inspectFrame.contentWindow.childSandboxBridge.air_Introspector_remoteClick();\n\t\t\t\t\t\t\t\t}catch(e){ air.Introspector.noChildBridge(air.Introspector.inspectFrame); }\n\t\t\t\t\t\t\t}else{\n\t                        \tair.Introspector.debugWindow.finishInspect(false);\n\t\t                        air.Introspector.hideHighlight();\n\t\t\t\t\t\t\t}\n\t                    }\n\t                }\n\n\t            }, true, 2000000);\n\t        };\n\t        var check = function(element, event){\n\t            air.Introspector.addEventListener(element, event, function(e){\n\t               if((air.Introspector.inspect||air.Introspector.remoteInspect)&&nativeWindow.active){\n\t                    setTimeout(function(){\n\t                        air.Introspector.canClick = true;\n\t                    }, 100);\n\t               }\n\t            }, true, 200000);\n\t        }\n        \n\t        var labelMover = function(element, event){\n\t        \tair.Introspector.addEventListener(element, event, function(e){\n\t\t           if((air.Introspector.inspect||air.Introspector.remoteInspect)){\n\t                  air.Introspector.highlightText.x = e.stageX+15;\n\t                  air.Introspector.highlightText.y = e.stageY+15;\n\/\/\t\t\t\t\t  air.Introspector.highlightText.visible = true;\n\t               }else{\n\n\t                  air.Introspector.highlightText.visible = false;\n\t               }\n\t        \t}, true, 200000);\n\t        }\n\t       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.CLICK, true);\n\t       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_DOWN);\n\t       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_UP);\n\t       prevent(htmlLoader.stage, runtime.flash.events.MouseEvent.DOUBLE_CLICK);\n\t       check(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_MOVE);\n\t       check(nativeWindow, runtime.flash.events.Event.ACTIVATE);\n\t       labelMover(htmlLoader.stage, runtime.flash.events.MouseEvent.MOUSE_MOVE);\n\t       window.htmlLoader.stage.addChild(sprite); \n\t       air.Introspector.highlightSprite = sprite;\n\n\t       air.Introspector.highlightText = new runtime.flash.display.Sprite();\n\t       window.htmlLoader.stage.addChild(air.Introspector.highlightText); \n\n\t       air.Introspector.highlightText.graphics.beginFill(0xeeeeee, 0.8);\n\t       air.Introspector.highlightText.graphics.lineStyle(1, 0xeeeeee, 0.9, false);\n\t       air.Introspector.highlightText.graphics.drawRect(0, 0, 250, 40);\n\t       air.Introspector.highlightText.visible = false;\n\t       air.Introspector.highlightLine1 = air.Introspector.createTextField(air.Introspector.highlightText, 16, true);\n\t       air.Introspector.highlightLine2 = air.Introspector.createTextField(air.Introspector.highlightText, 10, false);\n       }else{\n\t\t\t\/\/should not be here\n\t   }\n    },\n    \n\t\/**\n\t*\t@function addEventListener\n\t*\t@description Add a listener and stores it for future cleanup\n\t*\/\n\taddEventListener: function(obj, eventName, listener, capture, priority){\n\t\teventListeners.push([obj, eventName, listener, capture]);\n\t\tobj.addEventListener(eventName, listener, capture, priority);\n\t},\n\t\n\t\/**\n\t*\t@function removeEventListener\n\t*\t@description Removes listener\n\t*\/\n\tremoveEventListener: function(obj, eventName, listener, capture){\n\t\tfor(var i=eventListeners.length-1;i>=0;i--){\n\t\t\tvar l = eventListeners[i];\n\t\t\tif(l[0]==obj && l[1]==eventName && l[2]==listener && l[3]==capture)\n\t\t\t\t{\n\t\t\t\t\teventListeners.splice(i, 1);\n\t\t\t\t\tbreak;\n\t\t\t\t}\n\t\t}\n\t\tobj.removeEventListener(eventName, listener, capture);\n\t},\t\n\t\n\t\/**\n\t*\t@function drawRect\n\t*\t@description Draw a rectangle using ActionScript, also use tagName to find out which color to use \n\t*\t@see air.Introspector.highlightBgColors\n\t*\/\n\tdrawRect: function (rect, tagName){\n\t\t\tvar htmlLoaderBounds = htmlLoader.getBounds(htmlLoader.stage);\n\t    \trect.x += htmlLoaderBounds.x;\n\t\t    rect.y += htmlLoaderBounds.y;\n\t\t\trect.scaleX = 1;\n\t\t    rect.scaleY = 1;\n\t\t    air.Introspector.showHighlight(rect);\n\t\t    air.Introspector.highlightSprite.graphics.clear();\n\t\t    var bgColor = air.Introspector.highlightBgColors[tagName.toLowerCase()];\n\t\t    if(typeof bgColor==\'undefined\')\n\t\t         bgColor = air.Introspector.highlightBgColors[\'default\'];\n\t\t    air.Introspector.highlightSprite.graphics.beginFill(bgColor, 0.2);\n\t\t    air.Introspector.highlightSprite.graphics.lineStyle(3, bgColor, 0.9, false);\n\t\t    air.Introspector.highlightSprite.graphics.drawRect(0, 0, rect.width, rect.height);\n\t},\n\t\n\t\/**\n\t*\t@function highlightElement\n\t*\t@description Highlight element e. Get its bounding box and send it directly or over the bridge to air.Introspector.drawRect\n\t*\t@also air.Introspector.drawRect\n\t*\/\n    highlightElement: function(e, callback){\n\t\tvar rect = air.Introspector.getBorderBox(e);\n\t   \tif(rect==false)\n\t\t\treturn;\n\t\n\t\tif(isAppSandbox){\t\t\t\n\t\t\tair.Introspector.drawRect(rect, e.tagName);\n\t\t}else{\n\t\t\tsetTimeout(function(){\n\t\t\t\ttry{\n\t\t\t\t\tif(!isNaN(rect.width)&&!isNaN(rect.x)){\n\t\t\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\t\t\tparentSandboxBridge.air_Introspector_drawRect(rect, e.tagName);\t\t\t\t\t\n\t\t\t\t\t\t});\n\t\t\t\t\t}\n\t\t\t\t}catch(e){\n\t\t\t\t}\n\t\t\t\tif(typeof callback!=\'undefined\') callback();\n\t\t\t}, 0);\n\t\t}\n    },\n    \n\t\/**\n\t*\t@function addKeyboardEvents\n\t*\t@description \tRegisters events on every window that includes AIRDebug.js.\n\t*\n\t*\tBy default F11 enables the inspect tool\n\t*\t\t\t   F12 pops up the debug tool\n\t*\/\n\taddKeyboardEvents: function(sprite){\n\t\tair.Introspector.addEventListener(sprite, runtime.flash.events.KeyboardEvent.KEY_DOWN, function(e){\n            if(e.keyCode==air.Introspector.config.introspectorKey){ \/\/F11 key pressed\n\t\t\t\tif(typeof air.Introspector.lastElement!=\'undefined\'&&(air.Introspector.lastElement.nodeName==\'IFRAME\'||air.Introspector.lastElement.nodeName==\'FRAME\')){\n\t\t\t\t\ttry{\n\t\t\t\t\t\tvar contentWindow = air.Introspector.lastElement.contentWindow;\n\t\t\t\t\t\tif(typeof contentWindow.childSandboxBridge!=\'undefined\'&&\n\t\t\t\t\t\t\ttypeof contentWindow.childSandboxBridge.air_Introspector_isDebugOpen!=\'undefined\'&&\n\t\t\t\t\t\t\ttypeof contentWindow.childSandboxBridge.air_Introspector_toggleInspect!=\'undefined\')\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\tif(contentWindow.childSandboxBridge.air_Introspector_isDebugOpen()){\n\t\t\t\t\t\t\t\tcontentWindow.childSandboxBridge.air_Introspector_toggleInspect();\n\t\t\t\t\t\t\t\te.preventDefault();\n\t\t\t\t\t\t\t\te.stopPropagation();\n\t\t\t\t\t\t\t\treturn;\t\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}\n\t\t\t\t\t}catch(e){\n\t\t\t\t\t\t\/\/it looks like no debugger in that iframe. go ahead with app sandbox debugger\n\t\t\t\t\t}\n\t\t\t\t}\n                air.Introspector.init(false, true, function(){\n                \tair.Introspector.debugWindow.toggleInspect();\t\t\t\t\t\n\t\t\t\t});\n\n                e.preventDefault();\n\t\t\t\te.stopPropagation();\n            }else if(e.keyCode==air.Introspector.config.debuggerKey){ \/\/F12 key pressed\n                air.Introspector.toggleWindow();\n                e.preventDefault();\n\t\t\t\te.stopPropagation();\n            }else if(e.keyCode==27&&air.Introspector.inspect){\n                air.Introspector.debugWindow.finishInspect();\n                air.Introspector.hideHighlight();\n                e.preventDefault();\n\t\t\t\t\t\te.stopPropagation();\n            }else if(e.ctrlKey==true&&e.altKey==false){\n\t\t\t\tvar tab = null;\n\t\t\t\tswitch(e.keyCode){\n\t\t\t\t\tcase runtime.flash.ui.Keyboard.NUMBER_1:\n\t\t\t\t\t\ttab = 0;\n\t\t\t\t\tbreak;\n\t\t\t\t\tcase runtime.flash.ui.Keyboard.NUMBER_2:\n\t\t\t\t\t\ttab = 1;\n\t\t\t\t\tbreak;\n\t\t\t\t\tcase runtime.flash.ui.Keyboard.NUMBER_3:\n\t\t\t\t\t\ttab = 2;\n\t\t\t\t\tbreak;\n\t\t\t\t\tcase runtime.flash.ui.Keyboard.NUMBER_4:\n\t\t\t\t\t\ttab = 3;\n\t\t\t\t\tbreak;\n\t\t\t\t\tcase runtime.flash.ui.Keyboard.NUMBER_5:\n\t\t\t\t\t\ttab = 4;\n\t\t\t\t\tbreak;\n\t\t\t\t\tcase runtime.flash.ui.Keyboard.NUMBER_6:\n\t\t\t\t\t\ttab = 5;\n\t\t\t\t\tbreak;\n\t\t\t\t}\n\t\t\t\tif(tab!=null){\n\t\t\t\t\t\tair.Introspector.init(false, true, function(){\n\t\t\t\t\t\t\tair.Introspector.debugWindow.setTab(tab);\t\t\t\t\t\t\t\n\t\t\t\t\t\t});\n\t\t\t\t\t\te.preventDefault();\n\t\t\t\t\t\te.stopPropagation();\n\t\t\t\t}\n\t\t\t}\n        }, true, 1000000);\n\t},\n\t\n\t\/**\n\t*\t@function showHighlightLabels\n\t*\t@description Make the tooltip labels near the highlighting box appear and tell the id\/tag name\/outer HTML\n\t*\/\n\tshowHighlightLabels: function(id, nodeName, outerHTML){\n\t\t\tif(typeof id!=\'undefined\'&&id.length!=0){\n                air.Introspector.highlightLine1.text = nodeName+\' - \'+id;\n            }else{\n                air.Introspector.highlightLine1.text = nodeName;  \n            }\n            if(air.Introspector.canClick){\n                air.Introspector.highlightLine2.text = outerHTML.substr(0, 40).replace(\/\\n\/g, \'\\\\n\')+\'...\';\n            }else{\n                air.Introspector.highlightLine2.text = \'Click to activate window\';\n                window.clearTimeout(air.Introspector.clickToActivateTimeout);\n                air.Introspector.clickToActivateTimeout = setTimeout(function(){\n                    air.Introspector.highlightLine2.text = outerHTML.substr(0, 40).replace(\/\\n\/g, \'\\\\n\')+\'...\';\n                }, 400)\n            }\n        \tair.Introspector.highlightText.visible = true;\t\n\t},\n\t\n\t\n\t\/**\n\t*\t@function registerUncaughtExceptionListener\n\t*\t@description Catches all uncaught exceptions from javascript and shows them in the console\n\t*\/\n\tregisterUncaughtExceptionListener: function(){\n\t\t\n\t\t\tair.Introspector.addEventListener(window.htmlLoader,\n\t\t\t\t\truntime.flash.events.HTMLUncaughtScriptExceptionEvent.UNCAUGHT_SCRIPT_EXCEPTION , \n\t\t\t\t\tfunction(e){\n\t\t\t\t\t\tif(e.exceptionValue && \n\t\t\t\t\t\t\t\te.exceptionValue.air_Introspector_setParentSandboxBridge == true &&\n\t\t\t\t\t\t\t\te.exceptionValue.air_Introspector_version == air.Introspector.version)\n\t\t\t\t\t\t\t{\n\t\t\t\t\t\t\t\tair.Introspector.registerFramesParentSandboxBridge();\n\t\t\t\t\t\t\t\te.preventDefault();\n\t\t\t\t\t\t\t\treturn;\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\n\t\t\t\t\t\tair.Introspector.logError(e.exceptionValue, {htmlLoader:window.htmlLoader});\n\t\t\t\t\t\t\/\/\te.preventDefault();\n\t\t        \t});\n\t\t\n\t\t\n\t},\n\n\n\t\/**\n\t*\t@function registerCloseEventListener\n\t*\t@description \n\t*\/\n\tregisterCloseEventListener: function(){\n\t\tair.Introspector.addEventListener(window.nativeWindow, air.Introspector.runtime.Event.CLOSE, function(){\n\t            var debugWindow = air.Introspector.findDebugWindow();\n\t            if(debugWindow!=null){\n\t                debugWindow.closedWindow(window.htmlLoader);\n\t            }\n\t\n\t        });\n\t\t\t\n\t\t\n\t},\n\n\t\/**\n\t*\t@function registerCompleteEventLisener\n\t*\t@description Make the Introspector window knwo that we are complete. Register parentSandboxBridge on every frame\n\t*\/\n\tregisterCompleteEventListener: function(){\n\t\tair.Introspector.addEventListener(window.htmlLoader, air.Introspector.runtime.Event.COMPLETE, function(){\n\t\t\t\t\tair.Introspector.removeEventListener(window.htmlLoader, air.Introspector.runtime.Event.COMPLETE, arguments.callee);\n\t\t\t\t\ttry{\n\t\t           \t \/\/announce the debugWindow to refresh DOM and assets\n\t\t\t            var debugWindow = air.Introspector.findDebugWindow();\n\t\t\t            if(debugWindow!=null){\n\t\t\t            \tif(debugWindow.isLoaded){\n\t\t\t                    debugWindow.completeWindow(window.htmlLoader);\n\t\t\t            \t}\n\t\t\t            }\n\t\t\t\t\t\n\t\t\t\t\t\tair.Introspector.registerFramesParentSandboxBridge();\n\t\t\t\t\t}catch(e){\n\t\t\t\t\t\truntime.trace(e);\n\t\t\t            runtime.trace(e.line);\n\t\t\t\t\t\tair.Introspector.Console.log(e);\n\t\t\t\t\t}\n\t\t\t});\n\t},\n\n\t\/**\n\t*\t@function registerFramesParentSandboxBridge\n\t*\t@description All frames should know about us - registering parentSandboxBridge\n\t*\/\n\t\n\tregisterFramesParentSandboxBridge: function(){\n\t\t\t\/\/var modified = false;\n\t\t\tvar iframes = document.getElementsByTagName(\'iframe\');\n\t\t\tfor(var i=iframes.length-1;i>=0;i--){\n\t\t\t\tair.Introspector.registerFrame(iframes[i]);\n\t\t\t}\n\n\t\t\tvar frames = document.getElementsByTagName(\'frame\');\n\t\t\tfor(var i=frames.length-1;i>=0;i--){\n\t\t\t\tair.Introspector.registerFrame(frames[i]);\t\t\t\t\n\t\t\t}\n\t\t\t\/\/return modified;\n\t},\n\t\n\t\/**\n\t*\t@function registerDeactivateEventLisener\n\t*\t@description Hides the highlighting rectangle and deactivates inspect-clicking for this window\n\t*\/\n\tregisterDeactivateEventListener: function(){\n\t\t\tair.Introspector.addEventListener(window.nativeWindow, air.Introspector.runtime.Event.DEACTIVATE, function(){ air.Introspector.hideHighlight(); air.Introspector.canClick =false; });\n\t},\n\t\n\t\/**\n\t*\t@function registerChildSandboxBridge\n\t*\t@description Register childSandboxBridge for current iframe\n\t*\/\n\tregisterChildSandboxBridge: function(){\n\t\t\n\t\tif(typeof childSandboxBridge==\'undefined\')\n\t\t\tchildSandboxBridge={};\n\t\t\ttry{\n\t\tchildSandboxBridge.air_Introspector_remoteClick = function (){\n\t\t\ttry{\n\t\t\t\tair.Introspector.remoteClick();\n\t\t\t}catch(e){ alert(e+\' \'+e.line); }\n\t\t}\n\t\t\n\t\tchildSandboxBridge.air_Introspector_isDebugOpen = function(){\n\t\t\treturn typeof air.Introspector.debugWindow!=\'undefined\';\n\t\t}\n\t\t\n\t\tchildSandboxBridge.air_Introspector_toggleInspect = function (){\n\t\t\tair.Introspector.init(false, true, function()\n\t\t\t{\n               \tair.Introspector.debugWindow.toggleInspect();\t\t\t\t\t\t\t\n\t\t\t});\n\t\t}\n\t\t\n\t\tchildSandboxBridge.air_Introspector_bridgeLoaded = function(){\n\t\t\tvar l = air.Introspector.bridgeCallbacks;\n\t\t\tfor(var i=0;i<l;i++){\n\t\t\t\ttry{\n\t\t\t\t\tair.Introspector.bridgeCallbacks[i]();\n\t\t\t\t}catch(e){\n\t\t\t\t\tair.Introspector.logError(e);\n\t\t\t\t}\n\t\t\t}\n\t\t\tair.Introspector.bridgeCallbacks = [];\n\t\t}\n\t\t\t}\n\t\t\tcatch(e){}\n\t},\n\t\n\t\/**\n\t*\t@function createOpenConsoleButton\n\t*\t@description Creates a button on the top-right corent of the iframe that will open the introspector\n\t*\/\n\tcreateOpenConsoleButton: function(){\n\t\tvar consoleButton = document.createElement(\'input\');\n\t\tconsoleButton.onclick = function(){\n\t\t\tair.Introspector.init(true, true, function(){ });\n\t\t}\n\t\tconsoleButton.style.zIndex = 1000000;\n\t\tconsoleButton.style.position = \'fixed\';\n\t\tconsoleButton.style.right = \'10px\';\n\t\tconsoleButton.style.top = \'10px\';\n\t\tconsoleButton.type = \'button\';\n\t\tconsoleButton.value = \'Open Introspector\';\n\t\tdocument.body.appendChild(consoleButton);\t\t\t\t\t\t\t\t\n\t},\n\t\n\t\/**\n\t*\t@function registerDOMEventListeners\n\t*\t@description Registers DOMSubtreeModified, DOMCharacterDataModified, mouseover\n\t*\/\n\tregisterDOMEventListeners: function(){\n\t\tvar hoverTimeout = null;\n\t\t\/\/debugWindow should know about any dom change\n\t\tdocument.addEventListener(\'DOMSubtreeModified\', function(e){\n            var debugWindow = air.Introspector.findDebugWindow();\n            if(debugWindow!=null&&debugWindow.isLoaded){\n\t\t\t\tdebugWindow.dom3Event(e);\n            }\n\t\t});\n\t\tdocument.addEventListener(\'DOMCharacterDataModified\', function(e){\n            var debugWindow = air.Introspector.findDebugWindow();\n            if(debugWindow!=null&&debugWindow.isLoaded){\n\t\t\t\tdebugWindow.dom3Event(e);\n            }\n\t\t});\n\t\t\n\n           document.body.addEventListener(\'mouseover\', function(e){\n               if(air.Introspector.inspect){\n\t\t\t\tsetTimeout(function(){\n\t\t\t\t\tif(isAppSandbox){\n\t\t\t\t\t\t\tif(!nativeWindow.active)\n\t\t\t\t\t\t\t\tnativeWindow.activate();\n\t\t\t\t\t}\n                    if(e.srcElement){\n\t\t\t\t\t\tif(isAppSandbox){\n                        \tair.Introspector.highlightElement(e.srcElement);\n\t\t\t\t\t\t\tair.Introspector.showHighlightLabels(e.srcElement.id, e.srcElement.nodeName, e.srcElement.outerHTML);\n\t\t\t\t\t\t}else{\n\t\t\t\t\t\t\tair.Introspector.highlightElement(e.srcElement, function(){\n\t\t\t\t\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\t\t\t\t\tparentSandboxBridge.air_Introspector_showHighlightLabels(e.srcElement.id, e.srcElement.nodeName, e.srcElement.outerHTML);\t\t\t\t\t\t\t\t\n\t\t\t\t\t\t\t\t});\n\t\t\t\t\t\t\t});\t\t\t\t\t\t\t\n\t\t\t\t\t\t}\n\t\t\t\t\t\tif(hoverTimeout) clearTimeout(hoverTimeout);\n\t\t\t\t\t\thoverTimeout = setTimeout(function(){\n\t                        air.Introspector.init(false, true, function (){\n\t\t\t\t\t\t\t\tair.Introspector.debugWindow.setInspectElement(e.srcElement);\n\t\t\t\t\t\t\t});\n\t\t\t\t\t\t}, 100); \n                    }else{\n                        air.Introspector.hideHighlight();\n                    }\n\t\t\t\t}, 0);\n             }else if(isAppSandbox){\n\t\t\t\t\t\tair.Introspector.lastElement = e.srcElement;\n\t\t\t }\n\n           }, true);\n\n\t\t\/\/document.body.addEventListener(\'mouseout\', function(e){\n\t\t\/\/\t\tair.Introspector.hideHighlight();\n        \/\/});\n\t},\n\t\n\t\/**\n\t*\t@function cleanup\n\t*\t@description Cleans up the html loader\n\t*\n\t*\/\n    cleanup: function(){\n\t\t\tfor(var i=eventListeners.length-1;i>=0;i--){\n\t\t\t\tvar l = eventListeners[i];\n\t\t\t\ttry{\n\t\t\t\t\tl[0].removeEventListener(l[1], l[2], l[3]);\t\n\t\t\t\t}catch(e){}\n\t\t\t}\n\t\t\teventListeners = [];\n\t\t\ttry{\n\t\t    \twindow.htmlLoader.stage.removeChild(air.Introspector.highlightText); \n\t\t\t}catch(e){}\t    \n\t},\n\t\n\t\/**\n\t*\t@function register\n\t*\t@description Registers current window in debugger\n\t*\n\t*\tCaptures every XHR object created and any uncaught exception \n\t*\tand sends it to the debugger\n\t*\/\n    register: function(){\n    \t\n    \tif (window.XMLHttpRequest && window.XMLHttpRequest.prototype){\n                    window.XMLHttpRequest.prototype.debugopen = window.XMLHttpRequest.prototype.open;\n\t\t\t\t\twindow.XMLHttpRequest.prototype.debugsend = window.XMLHttpRequest.prototype.send;\n                    window.XMLHttpRequest.prototype.open = function(method, url, asyncFlag, username, password){\n\t\t\t\t\t\tif(typeof this.doNotDebug==\'undefined\'){\n\t                   \t    var debugWindow = air.Introspector.findDebugWindow();\n\t\t\t                if(debugWindow!=null){\n\t\t\t\t\t\t\t\tdebugWindow.logNet(this, method, url, asyncFlag);\n\t\t\t                }\n\t\t\t\t\t\t}\n                        return this.debugopen(method, url, asyncFlag, username, password);\n                    };\n\t\t\t\t\twindow.XMLHttpRequest.prototype.send = function(obj){\n\t\t\t\t\t\tif(typeof this.doNotDebug==\'undefined\'){\n\t                \t    var self = this;\n\t\t\t\t\t\t\tvar debugWindow = air.Introspector.findDebugWindow();\n\t\t\t\t            if(debugWindow!=null){\n\t\t\t\t\t\t        \tvar a = this.onreadystatechange;\n\t\t                            this.onreadystatechange = function(){\n\t\t                            \t if (typeof a == \'function\')a.call(self);\n\t\t                            \t \tdebugWindow.logNet(self, \'unknown\', \'\', false);\n\t\t\t\t                    };\n\t\t\t\t\t\t\t\tif(typeof self.doNotDebug==\'undefined\')\n\t\t                           \t debugWindow.logNetSend(this, obj);\n\t\t\t                }\n\t                        var ret = this.debugsend(obj);\n\t\t\t\t\t\t\tif(debugWindow!=null){\n\t\t\t\t\t\t\t\t\tdebugWindow.logNetSend(this, obj);\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\treturn ret;\n\t\t\t\t\t\t}else{\n\t                        return this.debugsend(obj);\n\t\t\t\t\t\t}\n\t\t\t\t\t\t\n\t\t\t\t\t}\n        }\n\n\t\tif(isAppSandbox){  \n\t\t\t\tair.Introspector.addKeyboardEvents(window.htmlLoader);\n\t\t\t\tair.Introspector.registerUncaughtExceptionListener();\n\t\t\t\tair.Introspector.registerCloseEventListener();\n\t\t\t\tair.Introspector.registerCompleteEventListener();\n\t        \tair.Introspector.registerDeactivateEventListener();\n\t \t\t    air.Introspector.createHighlight();\n\t\t\t}else{\n\t\t\t\tair.Introspector.registerChildSandboxBridge();\n\t\t\t}\n\t\t\tair.Introspector.waitForBody(document, function(){\n\t\t        try{\n\t\t\t\t\tif(!isAppSandbox){ air.Introspector.createOpenConsoleButton(); }\n\t\t\t\t\tair.Introspector.registerDOMEventListeners();\n\t\t\t\t\twindow.addEventListener(\'unload\', function(){\n\t\t\t\t\t\ttry{\n\t\t\t\t\t\t\tair.Introspector.cleanup();\n\t\t\t\t\t\t\tif(!isAppSandbox){\n\t\t\t\t\t\t\t\t\/\/our debugger can NOT live without it\'s parent\n\t\t\t\t\t\t\t\tair.Introspector.debugWindow.window.close();\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}catch(e){  }\n\t\t\t\t\t});\n\t\t\t\t}catch(e){\n\t\t\t\t\tif(isAppSandbox){ \n\t\t\t\t\t\truntime.trace(e);\n\t\t            \truntime.trace(e.line);\n\t\t\t\t\t}\n\t\t\t\t\tair.Introspector.Console.log(e);\n            \t}\n\t     });\n\n\t\n    },\n\n\n\t\/**\n\t*\t@function registerFrame\n\t*\t@description Makes the parentSandboxBridge available to frame \n\t*\/\n\tregisterFrame: function(frame){\n\t\tif(typeof frame.contentWindow.parentSandboxBridge==\'undefined\')\n\t\t\tframe.contentWindow.parentSandboxBridge = {};\n\t\t\t\n\t\t\/*frame.contentWindow.parentSandboxBridge.trace = function(a){\n\t\t\truntime.trace(a);\n\t\t};*\/\n\t\t\/\/checking that the bridge is not already there\n\t\t\/*var modified = typeof frame.contentWindow.parentSandboxBridge.air_Introspector_hideHighlight==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_showHighlight==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_drawRect==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_setInspect==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_getWindowTitle==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_checkNativeWindow==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToClipboard==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToFile==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_writeConfigFile==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_readConfigFile==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_showHighlightLabels==\'undefined\'\n\t\t\t|| typeof frame.contentWindow.parentSandboxBridge.air_Introspector_getFrameId==\'undefined\';*\/\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_hideHighlight = function(){\n\t\t\t\tair.Introspector.hideHighlight();\n\t\t};\n\t\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_showHighlight = function(rect){\n\t\t\t\tair.Introspector.showHighlight(rect);\n\t\t};\n\t\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_drawRect = function(rect, tagName){\n\t\t\t\tvar frameRect = air.Introspector.getBorderBox(frame);\n\t\t\t\tvar blw = air.Introspector.getIntProp(frame, \"border-left-width\");\n                var btw = air.Introspector.getIntProp(frame, \"border-top-width\");\n\t\t\t\tif(frameRect==null) return;\n\t\t\t\trect.x+=frameRect.x+2*blw;\n\t\t\t\trect.y+=frameRect.y+2*btw;\n\t\t\t\tair.Introspector.drawRect(rect, tagName);\n\t\t};\n\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_setInspect = function(enabled){\n\t\t\t\tair.Introspector.inspectFrame = enabled?frame:null;\n\t\t\t\tair.Introspector.remoteInspect = enabled;\n\t\t\t\tif(!enabled){\n\t\t\t\t\tair.Introspector.hideHighlight();\n\t\t\t\t}\n\t\t};\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_getWindowTitle = function(){\n\t\t\treturn document.title;\n\t\t};\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_checkNativeWindow = function(title){\n\t\t\tvar htmlWindows = air.Introspector.runtime.NativeApplication.nativeApplication.openedWindows;\n\t\t\tfor(var i=htmlWindows.length-1;i>=0;i--){\n\t\t\t\tif(htmlWindows[i].title==title){\n\t\t\t\t\treturn true;\n\t\t\t\t}\n\t\t\t}\n\t\t\treturn false;\n\t\t};\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToClipboard = function(str){\n\t\t\tair.Introspector.writeConsoleToClipboard(str);\n\t\t};\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_writeConsoleToFile = function(str){\n\t\t\tair.Introspector.writeConsoleToFile(str);\n\t\t};\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_writeConfigFile = function(config){\n\t\t\treturn air.Introspector.writeConfigFile(config, true);\t\t\t\n\t\t}\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_readConfigFile = function(){\n\t\t\treturn air.Introspector.readConfigFile(true);\n\t\t}\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_showHighlightLabels = function(id, nodeName, outerHTML){\n\t\t\tair.Introspector.showHighlightLabels(id, nodeName, outerHTML);\n\t\t};\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_getFrameId = function(){\n\t\t\treturn frame.id;\n\t\t}\n\t\t\n\t\tframe.contentWindow.parentSandboxBridge.air_Introspector_getNextWindowId = function(){\n\t\t\treturn ++air.Introspector.times;\n\t\t}\n\t\t\n\t\tif(typeof frame.contentWindow.childSandboxBridge!=\'undefined\'\n\t\t\t\t&& typeof frame.contentWindow.childSandboxBridge.air_Introspector_bridgeLoaded!=\'undefined\'){\n\t\t\tframe.contentWindow.childSandboxBridge.air_Introspector_bridgeLoaded();\n\t\t}\n\t\t\n\t\t\n\t\t\/\/return modified;\n\t},\n\t\n\t\/**\n\t*\t@function waitForBody\n\t*\t@description Wait until document.body is available\n\t*\/\n\twaitForBody: function(document, callback){\n\t\tif(document.body){\n\t\t\tcallback();\n\t\t}else{\n\t\t\tsetTimeout(air.Introspector.waitForBody, 10, document, callback);\n\t\t}\n\t},\n    \n\t\/**\n\t*\t@function toggleWindow\n\t*\t@description Shows\/Hides the debug tool\n\t*\/\n    toggleWindow:function(){\n        air.Introspector.init(true, false, function(justCreated){\n\t\t\tif(!justCreated)\n\t\t\t\tair.Introspector.debugWindow.nativeWindow.visible ^= true;\n\t\t});\n        \n    },\n\t\n\t\n\t\/**\n\t*\t@function init\n\t*\t@description Makes sure the debug tool is enabled\n\t*\/\n    init: function(showLoader, toggle, callback){\n\t\tif(!air.Introspector.canInit())\n\t\t\treturn;\n\t\n\t\tif(typeof showLoader==\'undefined\') showLoader = false;\n\t\tif(typeof toggle==\'undefined\') toggle = true;\n\t\n\t\tif(isAppSandbox){\n\t\t\t\n\t\t\tif(typeof air.Introspector.debugWindow==\'undefined\' || air.Introspector.debugWindow.nativeWindow.closed){\n\t\t\t\tdelete air.Introspector.debugWindow;\n\t           var debugWindow = air.Introspector.findDebugWindow();\n\t           if(debugWindow!=null && !debugWindow.nativeWindow.closed){\n\t               air.Introspector.debugWindow = debugWindow;\n\t\t\t\tif(toggle){\n\t\t\t\t\tair.Introspector.debugWindow.nativeWindow.visible = true;\t\t\t   \n\t\t\t\t\tif(!showLoader){\n\t\t\t\t\t\tnativeWindow.activate();\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t\tcallback(false);\n\t           }else{\n\t\t\t\t   air.Introspector.loadDebugger(function(debugWindow){\n\t\t\t\t\t\tair.Introspector.debugWindow = debugWindow;\n\t\t\t\t\t\tcallback(true);\n\t\t\t\t   }, showLoader);\n\t           }\n\t        }else{\n\t\t\t\tif(toggle){\n\t\t\t\t\tif(showLoader){\n\t\t\t\t\t\tair.Introspector.debugWindow.nativeWindow.activate(); \n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t\tcallback(false);\n\t\t\t}\n\t\t}else{\n\t\t\tif(typeof activeWindow==\'undefined\'){\n\t\t\t\tair.Introspector.registerChildSandboxBridge();\n\t\t\t}\n\t\t\t\n\t\t\tif(typeof air.Introspector.debugWindow==\'undefined\'||\n\t\t\t\ttypeof air.Introspector.debugWindow.window.air==\'undefined\'){ \n\/*\t\t\t\t(air.Introspector.debugWindow.isWindowCreated\n\t\t\t\t\t&&air.Introspector.debugWindow.isLoaded \n\t\t\t\t\t  &&air.Introspector.debugWindow.window \n\t\t\t\t\t\t&&!parentSandboxBridge.air_Introspector_checkNativeWindow(air.Introspector.parentWindowTitle + \': \'+air.Introspector.debugWindow.window.document.title))){*\/\n\t\t\t\tdelete air.Introspector.debugWindow;\n\n\n\t\t\t\tair.Introspector.loadDebugger(function(debugWindow){\n\t\t\t\t\t\tair.Introspector.debugWindow = debugWindow;\n\t\t\t\t\t\tcallback(true);\n\t\t\t\t   });\n\/\/\t\t\t\tair.Introspector.debugWindow = new air.Introspector.DebugWindow ({activateDebug: showLoader, activeWindow: window});\n\t\t\t}else if(!air.Introspector.debugWindow.isWindowCreated){\n\t\t\t\treturn;\n\t\t\t}else{\n\t\t\t\tcallback(false);\n\t\t\t}\n\t\t}\n    },\n    times:0,\t\/\/make the window.open page name unique - this is the number of opened and closed introspector windows\n\t\/**\n\t*\t@function tryCreateWindow\n\t*\t@description window.Open in browser\/remote sandbox is not allowed if the action is not iniated by the user (eg. user gesture, mouse click)\n\t*\t\t\t\t We can only wait for that moment. Until that happends we record all the callbacks and run them when the Introspector is laoded \n\t*\t@runs in remote sandbox only\n\t*\/\n\ttryCreateWindow: function(callbacks){\n\/\/\t\ttry{\n\t\t\tvar self = this;\n\t\t\tvar w;\n\t\t\tvar iframeId;\n\t\t\t\n\/*\t\t\tif(typeof parentSandboxBridge==\'undefined\'){\n\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\tair.Introspector.tryCreateWindow(callbacks);\t\t\t\t\t\n\t\t\t\t});\n\t\t\t\treturn;\n\t\t\t}*\/\n\n\t\t\tair.Introspector.parentWindowTitle = parentSandboxBridge.air_Introspector_getWindowTitle();\n\n\t\t\tif(typeof parentSandboxBridge!=\'undefined\'&&typeof parentSandboxBridge.air_Introspector_getFrameId!=\'undefined\')\n\t\t\t\tiframeId  = parentSandboxBridge.air_Introspector_getFrameId();\n\t\t\t\n\t\t\tif(typeof parentSandboxBridge!=\'undefined\'&&typeof parentSandboxBridge.air_Introspector_getNextWindowId!=\'undefined\')\n\t\t\t\tair.Introspector.times = parentSandboxBridge.air_Introspector_getNextWindowId();\n\t\t\telse\n\t\t\t\tair.Introspector.times ++ ; \/\/we should never be here - just in case, we should increment this\n\t\t\t\n\t\t\tif(typeof air.Introspector.config.useAirDebugHtml==\'undefined\'||air.Introspector.config.useAirDebugHtml==false){\n\t\t\t\tw = window.open(\'about:blank\', \'debugger\'+air.Introspector.times, \'width=640,height=480,resizable=1\');\n\t\t\t\tif(w&&w.document){\n\t\t\t\t\tw.isAppSandbox = isAppSandbox;\n\t\t\t\t\tw.opener = window;\n\t\t\t\t\tw.iframeId = iframeId;\n\t\t\t\t\tw.initCallbacks = callbacks;\t\n\t\t\t\t\tw.activeWindow = window;\n\t\t\t\t\tw.isLoaded = false;\n\t\t\t\t\tw.config = air.Introspector.config;\n\t\t\t\t\tw.document.open();\n\t\t\t\t\tw.document.write(air.Introspector.contentString);\n\t\t\t\t\tw.document.close();\n\t\t\t\t}\n\t\t\t}else{\n\t\t\t\tw = window.open(\'DebugUI.html\', \'debugger\'+air.Introspector.times, \'width=640,height=480,resizable=1\');\n\t\t\t\t\n\t\t\t\tif(w&&w.document){\n\t\t\t\t\tw.opener = window;\n\t\t\t\t\tw.iframeId = iframeId;\n\t\t\t\t\tw.activeWindow = window;\t\t\t\t\n\t\t\t\t\tw.config = air.Introspector.config;\n\t\t\t\t\tw.initCallbacks = callbacks;\t\t\t\t\n\t\t\t\t\tw.isLoaded = false;\n\t\t\t\t\tw.isAppSandbox = isAppSandbox;\n\t\t\t\t}\n\t\t\t}\n\t\t\treturn w;\n\/\/\t\t}catch(e){\n\/\/\t\t\talert(e+\' \'+e.line);\n\/\/\t\t}\n\t},\n\t\n\t\/**\n\t*\t@function loadDebugger\n\t*\t@description Loads the debugger window, register callbacks until it is ready\n\t*\t@runs in application sandbox only\n\t*\/\n\tloadDebugger: function(callback, activateDebug){\n\t\t\tvar htmlLoader;\n\t\t\tvar loadDebugger = arguments.callee;\n\t\t\tif(loadDebugger.htmlLoader && typeof loadDebugger.htmlLoader.window.isLoaded != \'undefined\'){\n\t\t\t\tif(loadDebugger.htmlLoader.window.isLoaded){\n\t\t\t\t\tcallback(loadDebugger.htmlLoader.window.debugWindow);\n\t\t\t\t}else{\n\t\t\t\t\tif(loadDebugger.htmlLoader.window.initCallbacks){\n\t\t\t\t\t\tloadDebugger.htmlLoader.window.initCallbacks.push(callback); \n\t\t\t\t\t}else{\n\t\t\t\t\t\tloadDebugger.initCallbacks.push(callback); \t\t\t\t\t\t\t\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t\treturn;\n\t\t\t}\n\t\t\tif(typeof loadDebugger.initCallbacks==\'undefined\'){\n\t\t\t\tloadDebugger.initCallbacks = [function(){\n\t\t\t\t\tdelete loadDebugger.initCallbacks;\n\t\t\t\t}, callback];\n\t\t\t}else{\n\t\t\t\tloadDebugger.initCallbacks.push(callback);\n\t\t\t}\n\t\t\tif(isAppSandbox){\n\t\t\t    htmlLoader = air.Introspector.runtime.HTMLLoader.createRootWindow(false);\n\t\t\t    air.Introspector.addEventListener(htmlLoader, air.Introspector.runtime.Event.HTML_DOM_INITIALIZE, function(){\n\t\t\t\t\ttry{ \n\t\t\t\t\t\tair.Introspector.removeEventListener(htmlLoader, air.Introspector.runtime.Event.HTML_DOM_INITIALIZE, arguments.callee);\n\t\t\t\t\t\thtmlLoader.window.initCallbacks = loadDebugger.initCallbacks;\n\t\t\t\t\t\thtmlLoader.window.isLoaded = false;\n\t\t\t\t\t\thtmlLoader.window.config = air.Introspector.config;\n\t\t\t\t\t\thtmlLoader.window.activateDebug = activateDebug;\n\t\t\t\t\t\thtmlLoader.window.isAppSandbox = isAppSandbox;\n\t\t\t\t\t}catch(e){ \n\t\t\t\t\t\tair.Introspector.runtime.trace(e); \n\t\t\t\t\t\tair.Introspector.runtime.trace(e.line); \n\t\t\t\t\t}\n\t\t\t\t});\n\t\t\t\thtmlLoader.window.isLoaded = false;\n\n\t\t\t    var nativeWindow = htmlLoader.stage.nativeWindow;\n\t\t\t\tnativeWindow.width = 640;\n\t\t\t\tnativeWindow.height = 480;\n\t\t\t    air.Introspector.addEventListener(htmlLoader, runtime.flash.events.HTMLUncaughtScriptExceptionEvent.UNCAUGHT_SCRIPT_EXCEPTION, function(e){\n\t\t\t         air.Introspector.logError(e.exceptionValue, {htmlLoader: self.htmlLoader});\n\t\t\t         e.preventDefault();\n\t\t\t    });\n\t\t\t    if(typeof air.Introspector.config.useAirDebugHtml==\'undefined\'||air.Introspector.config.useAirDebugHtml==false){\n\t\t\t\t\tif(typeof htmlLoader.placeLoadStringContentInApplicationSandbox!=\'undefined\'){\n\t\t\t\t\t\t\t\/\/since AIR1.5 the htmlLoader will not allow string load in app sandbox\n\t\t\t\t\t\t\thtmlLoader.placeLoadStringContentInApplicationSandbox= true;\n\t\t\t\t\t}\n\t\t\t\t\thtmlLoader.loadString(air.Introspector.contentString);\n\t\t\t\t\tif(typeof htmlLoader.placeLoadStringContentInApplicationSandbox!=\'undefined\'){\n\t\t\t\t\t\t\t\/\/switch it back to false after load is complete\n\t\t\t\t\t\t\thtmlLoader.placeLoadStringContentInApplicationSandbox= false;\n\t\t\t\t\t}\n\t\t\t    }else{\n\t\t\t        htmlLoader.load(new air.Introspector.runtime.URLRequest(\'app:\/DebugUI.html\'));\n\t\t\t    }\n\t\t\t}else{\n\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\tvar w = air.Introspector.tryCreateWindow(loadDebugger.initCallbacks);\n\t\t\t\t\tif(w){\n\t\t\t\t\t\tvar htmlLoader = {window: w};\t\n\t\t\t\t\t\tloadDebugger.htmlLoader = htmlLoader;\t\n\t\t\t\t\t}\n\t\t\t\t});\n\t\t\t}\t\n\t\t\tloadDebugger.htmlLoader = htmlLoader;\t\t\n\t},\n\n\t\/**\n\t*\t@function findDebugWindow\n\t*\t@description Look up the Introspector in other windows. Maybe somebody else just opened it before us. \n\t*\/\n    findDebugWindow: function(){\n\t\tif(isAppSandbox){\n\t\t\ttry{\n\t    \t\tif(air.Introspector.debugWindow&&air.Introspector.debugWindow.nativeWindow.closed==false)\n\t\t    \t   return air.Introspector.debugWindow;\n\t\t\t}catch(e){\n\t\t\t}\n\t\t\ttry{\n\t\t        var htmlWindows = air.Introspector.getHtmlWindows(true);\n\t\t        for(var i=htmlWindows.length-1;i>=0;i--){\n\t\t            try{\n\t\t                if(typeof htmlWindows[i].htmlLoader.window.air!=\'undefined\'\n\t\t                   && typeof htmlWindows[i].htmlLoader.window.air.Introspector!=\'undefined\'\n\t\t                       && typeof htmlWindows[i].htmlLoader.window.air.Introspector.debugWindow!=\'undefined\'\n\t\t\t\t\t\t\t\t&& htmlWindows[i].htmlLoader.window.air.Introspector.debugWindow.nativeWindow.closed==false \n\t\t\t\t\t\t\t\t\t&& htmlWindows[i].htmlLoader.window.isAppSandbox )\n\t\t                    {\n\t\t                        return htmlWindows[i].htmlLoader.window.air.Introspector.debugWindow;\n\t\t                    }\n\t\t            }catch(e){\n\t\t                \/\/this window is not initialized yet\n\t\t                \/\/just get next window\n\t\t            }\n\t\t        }\n\t\t\t}catch(e){}\n\t\t}else{\n\t\t\treturn air.Introspector.debugWindow;\n\t\t}\n        return null;\n    },\n\n\t\/\/application browser formats\n\t\/\/\t\t0 - text\n\t\/\/\t\t1 - images\n\t\/\/\t\t2 - xml (you may want to add your own xml type here)\n    formats : { \'png\':1, \'gif\':1, \'zip\':1, \'air\':1, \'jpg\':1, \'jpeg\':1,\n                 \'txt\':0, \'html\':0, \'js\':0, \'xml\':2, \'opml\':2, \'css\':0, \'htm\':0, \'\':0 },\n    \n\n\t\/**\n\t*\t@function canInit\n\t*\t@description Check if we got parentSandboxBridge available\n\t*\t@disabled \n\t*\/\n\tcanInit: function(){\n\/*\t\tif(!isAppSandbox&&typeof parentSandboxBridge==\'undefined\'){\n\t\t\talert(\'You need to include AIRIntrospector.js in application sandbox too!\');\n\t\t\treturn false;\n\t\t}*\/\n\t\treturn true;\n\t},\n\n\t\/**\n\t*\t@function logArguments\n\t*\t@description \n\t*\/\n\tlogArguments: function(args, config){\n\t\tif(!air.Introspector.canInit()) return;\n\t\tconfig.timestamp = new Date();\n       \tair.Introspector.init(config.type==\'error\', true, function(){\n\t\t\tair.Introspector.debugWindow.logArguments(args, config);\n\t\t});\n        \n    },\n    \/**\n\t*\t@function logError\n\t*\t@description \n\t*\/\n    logError: function(error, config){\n        air.Introspector.init(false, true, function(){\n\t\t\tair.Introspector.debugWindow.logError(error, config);\n\t\t});\n    },\n    \n\t\/**\n\t*\t@function showCssElement\n\t*\t@description \n\t*\/\n\tshowCssElement: function(element){\n\t\tvar debugWindow = air.Introspector.findDebugWindow();\n\t\tif(debugWindow){\n\t\t\tdebugWindow.showCssElement(element);\n\t\t}\n\t},\n\n\tcheckIfIsInstanceOfHTMLLoader: function (child){\n\t\tvar className = runtime.flash.utils.getQualifiedClassName(child);\n\t\tif( className == \"flash.html::HTMLLoader\" ) return true;\n\t\tif( className == \"mx.core::FlexHTMLLoader\" ) return true;\n\t\treturn false;\n\t},\n\t\/**\n\t*\t@function findLoader\n\t*\t@description Finds the first HTMLLoader in flash display object list\n\t*\/\n    findLoader: function (stage, loaders){\n\t\ttry{\n\t        for(var i=stage.numChildren-1;i>=0;i--){\n\t            var child = stage.getChildAt(i);\n\t\t\t\tif(air.Introspector.checkIfIsInstanceOfHTMLLoader(child)){\n\t\t\t\t   loaders.push([child]);\n\t\t\t\t}else if(child.htmlLoader!=null&&air.Introspector.checkIfIsInstanceOfHTMLLoader(child.htmlLoader)){\n\t\t\t\t   loaders.push([child.htmlLoader, child.id || child.toString()]);\n\t\t\t\t}else{\n\t\t\t\t\tair.Introspector.findLoader(child, loaders);\n\t            }\n\t        }\n\t\t}catch(e){\n\t\t}\n        return null;\n    }, \n    \n\t\/**\n\t*\t@function getHtmlWindows\n\t*\t@description Returns an array of all HTML windows\n\t*\/\n    getHtmlWindows: function(includeInspectors){\n\t\tif(isAppSandbox){\n \t       var windowNodes = [];\n\t        var windows = air.Introspector.runtime.NativeApplication.nativeApplication.openedWindows;\n\t        for(var i=windows.length-1;i>=0;i--){\n\t\t\t\tvar loaders = [];\n\t            air.Introspector.findLoader(windows[i].stage, loaders);\n\t            for(var j=loaders.length-1;j>=0;j--){\n\t\t\t\t\tvar loaderItem = loaders[j];\n\t\t\t\t\tvar loader = loaderItem[0];\n\t\t\t\t\tvar label = loaderItem[1];\n\t\t\t\t\tif(typeof includeInspectors==\'undefined\' && typeof loader.window!=\'undefined\' && typeof loader.window.air!=\'undefined\' && typeof loader.window.air.Introspector!=\'undefined\' &&\n\t\t\t\t\t\ttypeof loader.window.air.Introspector.localIframeDebug != \'undefined\' ){\n\t\t\t\t\t\t\tcontinue;\n\t\t\t\t\t\t}            \n\t                windowNodes.push({\n\t                    nativeWindow: windows[i],\n\t                    stage: windows[i].stage,\n\t                    htmlLoader : loader,\n\t\t\t\t\t\tlabel: label\n\t                });\n\t            }\n\t        }\n\t        return windowNodes;\n\t\t}else{\n\t\t\t\/\/should not be here\n\t\t\treturn [];\n\t\t}\n   },\n   \n\t\/**\n\t*\t@function twoDigits\n\t*\t@description int 2 string with two digits\n\t*\/\n\ttwoDigits: function(val){\n\t     if(val<10) return \'0\'+val;\n\t     return val+\'\';\n\t},\n\n\t\/**\n\t*\t@function escapeHtml\n\t*\t@description Escapes html in order to display it in html\n\t*\/\n\tescapeHtml: function(html){      \n\t     return (html+\'\').replace(\/&\/g, \'&amp;\').replace(\/\"\/g, \"&quot;\").replace(\/<\/g, \'&lt;\').replace(\/>\/g, \'&gt;\');\n\t},\n\n   \ttree: { },\n\n\n\t\/**\n\t*\t@function isNumberObject\n\t*\t@description \n\t*\/\n\tisNumberObject: function(obj){\n\t     try{\n\t         \/\/can we catch isNaN only for NaN\n\t         return (obj+0==obj&&!isNaN(obj));\n\t     }catch(e){\n\t     }\n\t     return false;\n\t },\n\n\t\/**\n\t*\t@function isStringObject\n\t*\t@description \n\t*\/\n\tisStringObject: function(obj){\n\t    try{\n\t        return (typeof(obj.match) != \"undefined\" && obj.match.toString().indexOf(\"[native code]\")>0);\n\t    }catch(e){\n\t    }\n\t    return false;\n\t},\n\n\t\/**\n\t*\t@function isDateObject\n\t*\t@description \n\t*\/\n\tisDateObject: function(obj){\n\t    try{\n\t        return (typeof(obj.getDate) != \"undefined\" && obj.getDate.toString().indexOf(\"[native code]\")>0);\n\t    }catch(e){\n\t    }\n\t    return false;\n\t},\n\n\t\/**\n\t*\t@function isArgumentsObject\n\t*\t@description \n\t*\/\n   isArgumentsObject: function(obj){\n       try{\n           return obj.toString()==\'[object Arguments]\';\n       }catch(e){\n       }\n       return false;\n   },\n\n\t\/**\n\t*\t@function isXMLObject\n\t*\t@description \n\t*\/\n\tisXMLObject: function(obj){\n\t\ttry{\n\t\t\tif(obj.xmlVersion&&obj.firstChild!=null)\n\t        \treturn obj.xmlVersion!=\'\';\n\t    }catch(e){\n\t    }\n\t    return false;\n\t},\n\n\t\/**\n\t*\t@function isArrayObject\n\t*\t@description \n\t*\/\n\tisArrayObject: function(obj){\n       try{\n           return (typeof(obj.push) != \"undefined\" && obj.push.toString().indexOf(\"[native code]\")>0);\n       }catch(e){\n       }\n       return false;\n\t},\n\n\t\/**\n\t*\t@function isItemNative\n\t*\t@description \n\t*\/\n\tisItemNative: function(obj){\n       try{\n           return (typeof(obj.item) != \"undefined\" && obj.item.toString().indexOf(\"[native code]\")>0);\n       }catch(e){\n       }\n       return false;\n\t},\n   \n\n\t\/**\n\t*\t@function dump\n\t*\t@description \n\t*\/\n\n\tdump: function (obj, levels, level){\n\t\tif(air.Introspector.isArgumentsObject(obj)&&obj.length==1)\n\t\t\treturn air.Introspector.dump(obj[0]);\n\t\tif(typeof levels==\'undefined\') { levels=1; }\n\t\tif(typeof level==\'undefined\') { level=0; }\t\t\n        try{\n            if(typeof obj==\'undefined\'){ return \'[undefined]\'; }\n\t\t\tif(obj==null){\treturn \'[null]\'; }\n\t\t\tvar list = [];\n\/\/            if(air.Introspector.isXMLObject(obj)){\n\/\/\t\tdisable for the moment\n\/\/            \treturn;\n\/\/            }  \n\t\t\tif(air.Introspector.isStringObject(obj)\n\t\t\t\t||air.Introspector.isNumberObject(obj)\n\t\t\t\t\t||air.Introspector.isDateObject(obj)){\n\t\t\t\tif(level==0){\n\t\t\t\t\ttry{\n\t\t\t\t\t\treturn obj+\'\';\n\t\t\t\t\t}catch(e){ \n\t\t\t\t\t\treturn e+\'\'; \n\t\t\t\t\t};\n\t\t\t\t}\n\t\t\t\treturn\'\';\n\t\t\t}\n            var isItemNative = air.Introspector.isItemNative(obj);\n            var parseArray = air.Introspector.isArrayObject(obj)||air.Introspector.isArgumentsObject(obj)||isItemNative;\n            var parseHash =  !parseArray || isItemNative;\n            if (parseArray){\n\t\t\t\tvar l = obj.length;\n            \tfor(var i=0;i<l;i++){\n                    var value;\n                    try{\n                        value = obj[i];\n\t\t\t\t\t\tif(typeof value==\'undefined\') {value = \'[undefined]\'};\n                    }catch(e){\n                        value = e+\'\';\n                    }\n\t\t\t\t\tlist.push([i,value]);\n                }\n            } \n            if(parseHash) {\n                for(var i in obj){\n                    var value;\n                    try{\n                        value = obj[i];\n\t\t\t\t\t\tif(typeof value==\'undefined\') {value = \'[undefined]\'};\n                    }catch(e){\n                        value = e+\'\';\n                    }\n\t\t\t\t\tlist.push([i,value]);\n                }\n            }\n\n\t\t\tif(air.Introspector.config.debugRuntimeObjects && !parseArray){\n\t\t\t\ttry{\n\t\t\t\t\tvar typeDescription = runtime.flash.utils.describeType(obj);\n\t\t\t\t\tif(!this.domParser) this.domParser = new DOMParser();\n\t\t\t\t\tvar typeXml = this.domParser.parseFromString(typeDescription, \"text\/xml\");\n\t\t\t\t\tvar child = typeXml.firstChild.firstChild;\n\t\t\t\t\twhile(child){\n\t\t\t\t\t\tif(child.nodeName==\'accessor\'||child.nodeName==\'constant\'||child.nodeName==\'method\'||child.nodeName==\'variable\'){\n\t\t\t\t\t\t\tvar name = child.getAttribute(\'name\');\n\t\t\t\t\t\t\tif(name!=null && name!=\'prototype\'){\n\t\t\t                    try{\n\t\t\t\t\t\t\t\t\tlist.push([name,obj[name]]);\n\t\t\t                    }catch(e){\n\t\t\t\t\t\t\t\t\tlist.push([name,\'\']+\'\');\n\t\t\t                    }\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}\n\t\t\t\t\t\tchild = child.nextSibling;\n\t\t\t\t\t}\n\t\t\t\t}catch(e){\n\t\t\t\t\t\/\/just hide the error\n\t\t\t\t}\n            }\n            list.sort(function(node1, node2){\n            \tvar isNode1Number = parseInt(node1[0])==node1[0];\n            \tvar isNode2Number = parseInt(node2[0])==node2[0];\n            \tif(isNode1Number&&isNode2Number){\n            \t\treturn parseInt(node1[0])-parseInt(node2[0]);\n            \t} \n            \tif(isNode1Number){\n            \t\treturn -1;\n            \t}\n            \tif(isNode2Number){\n            \t\treturn 1;\n            \t}\n            \tif(node1[0].toLowerCase()==node2[0].toLowerCase())\n                   return 0;\n                if(node1[0].toLowerCase()<node2[0].toLowerCase())\n                   return -1;\n                return 1;\n            });\n\t\t\tif(list.length){\n\t\t\t\tvar prefix = \'\';\n\t\t\t\tfor(var i=level;i>0;i--) prefix+=\'    \';\n\t\t\t\tvar l = list.length;\n\t\t\t\tvar strList = [];\n\t\t\t\tif(parseArray)\tstrList.push(prefix+\'[\\r\\n\');\n\t\t\t\telse\t\t\tstrList.push(prefix+\'{\\r\\n\');\n\t\t\t\tfor(var i=0;i<l;i++){\n\t\t\t\t\ttry{\n\t\t\t\t\t\tvar zl = (list[i][0]+\'\').length+1;\n\t\t\t\t\t\tvar miniPrefix = \'\';\n\t\t\t\t\t\tfor(var j=0;j<zl;j++) miniPrefix+=\' \';\n\t\t\t\t\t\tif(typeof list[i][1]==\'function\'){\n\t\t\t\t\t\t\tstrList.push(prefix+\'  \'+list[i][0]+\'=[function]\\r\\n\');\n\t\t\t\t\t\t}else if(air.Introspector.isDateObject(list[i][1])){\n\t\t\t\t\t\t\tstrList.push(prefix+\'  \'+list[i][0]+\'=\'+(new Date(list[i][1])+\'\').replace(\/\\n\/g, \'\\r\\n\'+prefix+miniPrefix)+\',\\r\\n\');\n\t\t\t\t\t\t}else{\n\t\t\t\t\t\t\tstrList.push(prefix+\'  \'+list[i][0]+\'=\'+(list[i][1]+\'\').replace(\/\\n\/g, \'\\r\\n\'+prefix+miniPrefix)+\'\\r\\n\');\n\t\t\t\t\t\t}\n\t\t\t\t\t}catch(e){\n\t\t\t\t\t\tstrList.push(prefix+\'  \'+list[i][0]+\'=\'+e+\'\\r\\n\');\n\t\t\t\t\t}\n\t\t\t\t\tif(level<levels){\n\t\t\t\t\t\tstrList.push(air.Introspector.dump(list[i][1], levels, level+1));\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t\tif(parseArray)\tstrList.push(prefix+\']\\r\\n\');\n\t\t\t\telse\t\t\tstrList.push(prefix+\'}\\r\\n\');\n\t\t\t\tif(level){\n\t\t\t\t\treturn strList.join(\'\');\n\t\t\t\t}else{\n\t\t\t\t\treturn (strList.join(\'\'));\n\t\t\t\t}\n\t\t\t}\n        }catch(e){\n            air.Introspector.Console.error(e);\n        } \n\t\treturn \'\';\n\t},\n\n\n\n\n    \/**\n\t*\t---------------------------------------------------------------------------------------\n\t*\t@extracted This is extracted from spry framework and removed support for other browsers.\n\t*\t@description Finds the precise position of the dom element node\n\t*\/   \n\t\n\t\/**\n\t*\t@function camelize\n\t*\t@description \n\t*\/\n\t\n\tcamelize : function(stringToCamelize)\n            {\n                if (stringToCamelize.indexOf(\'-\') == -1){\n                    return stringToCamelize;    \n                }\n                var oStringList = stringToCamelize.split(\'-\');\n                var isFirstEntry = true;\n                var camelizedString = \'\';\n            \n                for(var i=0; i < oStringList.length; i++)\n                {\n                    if(oStringList[i].length>0)\n                    {\n                        if(isFirstEntry)\n                        {\n                            camelizedString = oStringList[i];\n                            isFirstEntry = false;\n                        }\n                        else\n                        {\n                            var s = oStringList[i];\n                            camelizedString += s.charAt(0).toUpperCase() + s.substring(1);\n                        }\n                    }\n                }\n            \n                return camelizedString;\n            },\n\n\t\/**\n\t*\t@function getStyleProp\n\t*\t@description \n\t*\/\n\tgetStyleProp : function(element, prop)\n            {\n                var value;\n                try\n                {\n                    if (element.style)\n                        value = element.style[air.Introspector.camelize(prop)];\n            \n                    if (!value)\n                    {\n                        if (document.defaultView && document.defaultView.getComputedStyle)\n                        {\n                            var css = document.defaultView.getComputedStyle(element, null);\n                            value = css ? css.getPropertyValue(prop) : null;\n                        }\n                        else if (element.currentStyle) \n                        {\n                                value = element.currentStyle[air.Introspector.camelize(prop)];\n                        }\n                    }\n                }\n                catch (e) {}\n            \n                return value == \'auto\' ? null : value;\n            },\n   \n\n\t\/**\n\t*\t@function getIntProp\n\t*\t@description \n\t*\/\n\tgetIntProp : function(element, prop){\n                var a = parseInt(air.Introspector.getStyleProp(element, prop),10);\n                if (isNaN(a))\n                    return 0;\n                return a;\n            },\n\n\n\t\/**\n\t*\t@function getBorderBox\n\t*\t@description \n\t*\/\n\tgetBorderBox : function (el, doc) {\n                doc = doc || document;\n                if (typeof(el) == \'string\') {\n                    el = doc.getElementById(el);\n                }\n            \n                if (!el) {\n                    return false;\n                }\n            \n                if (el.parentNode === null || air.Introspector.getStyleProp(el, \'display\') == \'none\') {\n                    \/\/element must be visible to have a box\n                    return false;\n                }\n            \n                var ret = {x:0, y:0, width:0, height:0};\n                var parent = null;\n                var box;\n\n            \tvar str = el.nodeName;\n\t\t\t\n                ret.x = el.offsetLeft;\n                ret.y = el.offsetTop;\n                ret.width = el.offsetWidth;\n                ret.height = el.offsetHeight;\n\n\t\t\t\tparent = el.offsetParent;\n\n                if (parent != el) {\n                    while (parent) {\n                        ret.x += parent.offsetLeft;\n                        ret.y += parent.offsetTop;\n                        ret.x += air.Introspector.getIntProp(parent, \"border-left-width\");\n                        ret.y += air.Introspector.getIntProp(parent, \"border-top-width\");                        \n\t\t\t\t\t\tstr+=\':\'+parent.nodeName;\n                        parent = parent.offsetParent;\n                    }\n                }\n\n                \/\/ opera & (safari absolute) incorrectly account for body offsetTop\n                switch (air.Introspector.getStyleProp(el, \'position\')){\n \t\t\t\t\tcase \'absolute\':\n                    \tret.y -= doc.body.offsetTop;\n\t\t\t\t\t\tbreak;\n\t\t\t\t\tcase \'fixed\':\n\t                    ret.y += doc.body.scrollTop;\n\t                    ret.x += doc.body.scrollLeft;\n\t\t\t\t\t\tbreak;\n\t\t\t\t};\n                    \n                if (el.parentNode)\n                    parent = el.parentNode;\n                else\n                    parent = null;\n                \n\t\t\t\tif (parent!=null&&parent.nodeName){\n                    var cas = parent.nodeName.toUpperCase();\n                    while (parent && cas != \'HTML\') {\n                        cas = parent.nodeName.toUpperCase();\n                        ret.x -= parent.scrollLeft;\n                        ret.y -= parent.scrollTop;\n                        if (parent.parentNode)\n                            parent = parent.parentNode;\n                        else\n                            parent = null;\n                    }\n                }\n\n\/*\t\t\t\tret.y -= el.ownerDocument.body.scrollTop;\n\t\t\t\tret.x -= el.ownerDocument.body.scrollLeft;\t\t\t\t\n*\/\n                \/\/ adjust the margin\n                var gi = air.Introspector.getIntProp;\n                var btw = gi(el, \"margin-top\");\n                var blw = gi(el, \"margin-left\");\n                var bbw = gi(el, \"margin-bottom\");\n                var brw = gi(el, \"margin-right\");\n                ret.x -= blw;\n                ret.y -= btw;\n                ret.height += btw + bbw;\n                ret.width += blw + brw;\n\n\t\t\t\/\/\tair.Introspector.Console.log(ret);\n                return ret;\n            },\n\n\t\/**\n\t*\t---------------------------------------------------------------------------------------\n\t*\/\n\n\t\/**\n\t*\t@function writeConfigFile\n\t*\t@description \n\t*\/\n\twriteConfigFile: function(config, fromRemoteSandbox){\n\t\tif(isAppSandbox){\n\t\t\tvar file = runtime.flash.filesystem.File.applicationStorageDirectory.resolvePath(\'AIRIntrospector\'+(fromRemoteSandbox?\'Remote\':\'\')+\'.cfg\');\n\t\t\tvar fs = new runtime.flash.filesystem.FileStream();\n\t\t\tfs.open(file, runtime.flash.filesystem.FileMode.WRITE);\n\t\t\tfs.writeObject(config);\n\t\t\tfs.close();\n\t\t}else{\n\t\t\tif(typeof activeWindow==\'undefined\'){\n\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\tparentSandboxBridge.air_Introspector_writeConfigFile(config);\n\t\t\t\t});\n\t\t\t}else{\n\t\t\t\tactiveWindow.setTimeout(function(){\n\t\t\t\t\ttry{\n\t\t\t\t\t\tactiveWindow.air.Introspector.writeConfigFile(config);\n\t\t\t\t\t}catch(e){}\n\t\t\t\t}, 0);\n\t\t\t}\n\t\t}\n\t},\n\n\n\n\t\/**\n\t*\t@function readConfigFile\n\t*\t@description \n\t*\/\n\treadConfigFile: function (fromRemoteSandbox, callback){\n\t\tif(isAppSandbox){\n\t\t\tvar file = runtime.flash.filesystem.File.applicationStorageDirectory.resolvePath(\'AIRIntrospector\'+(fromRemoteSandbox?\'Remote\':\'\')+\'.cfg\');\n\t\t\tif(file.exists){\n\t\t\t\tvar fs = new runtime.flash.filesystem.FileStream();\n\t\t\t\tfs.open(file, runtime.flash.filesystem.FileMode.READ);\n\t\t\t\tvar config = fs.readObject();\n\t\t\t\tfs.close();\n\t\t\t\treturn config;\n\t\t\t}\n\t\t}else{\n\t\t\tif(typeof activeWindow==\'undefined\'){\n\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\tvar config = parentSandboxBridge.air_Introspector_readConfigFile();\n\t\t\t\t\tcallback(config);\n\t\t\t\t});\t\t\t\t\n\t\t\t}else{\n\t\t\t\tactiveWindow.setTimeout(function(){\n\t\t\t\t\ttry{\n                        activeWindow.air.Introspector.readConfigFile(true, function(config){\n                            setTimeout(function(){ callback(config); }, 0);\t\t\t\t\t\t\t\n                        });\n\t\t\t\t\t}catch(e){}\n\t\t\t\t}, 0);\n\t\t\t}\n\t\t}\n\t\treturn {};\n\t},\n\t\n\t\n\t\n\t\/**\n\t*\t@function writeConsoleToClipboard\n\t*\t@description \n\t*\/\n\n\twriteConsoleToClipboard: function(str){\n\t\tif(isAppSandbox){\t\t\n\t\t\truntime.flash.desktop.Clipboard.generalClipboard.clear();\n\t\t\truntime.flash.desktop.Clipboard.generalClipboard.setData(runtime.flash.desktop.ClipboardFormats.TEXT_FORMAT, \n\t\t\t\t\tstr, false);\t\t\n\t\t}else{\n\t\t\tif(typeof activeWindow==\'undefined\'){\n\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\tparentSandboxBridge.air_Introspector_writeConsoleToClipboard(str);\n\t\t\t\t});\t\t\t\t\n\t\t\t}else{\n\t\t\t\tactiveWindow.setTimeout(function(){\n\t\t\t\t\ttry{\n\t\t\t\t\t\tactiveWindow.air.Introspector.writeConsoleToClipboard(str);\n\t\t\t\t\t}catch(e){}\n\t\t\t\t});\n\t\t\t}\n\t\t}\n\t},\t\n\n\n\t\/**\n\t*\t@function writeConsoleToFile\n\t*\t@description \n\t*\/\n\twriteConsoleToFile: function(str){\n\t\tif(isAppSandbox){\t\t\n\t\t\tvar file = runtime.flash.filesystem.File.desktopDirectory;\n\t\t\tvar self = this;\n\t\t\tfile.addEventListener(runtime.flash.events.Event.SELECT, function(evt){ \n\t\t\t\tvar newFile = evt.target;\n  \t\t\t    var stream = new runtime.flash.filesystem.FileStream();\n\t\t\t        stream.open(newFile, runtime.flash.filesystem.FileMode.WRITE);\n\t\t\t        stream.writeUTFBytes(str);\n\t\t\t        stream.close();\n\t\t\t\t});\n\t\t\tfile.browseForSave(\'Console dump file...\');\n\t\t}else{\n\t\t\tif(typeof activeWindow==\'undefined\'){\n\t\t\t\tair.Introspector.noBridge(function(){\n\t\t\t\t\tparentSandboxBridge.air_Introspector_writeConsoleToFile(str);\n\t\t\t\t});\t\t\t\t\n\t\t\t}else{\n\t\t\t\tactiveWindow.setTimeout(function(){\n\t\t\t\t\ttry{\n\t\t\t\t\t\tactiveWindow.air.Introspector.writeConsoleToFile(str);\n\t\t\t\t\t}catch(e){}\n\t\t\t\t});\n\t\t\t}\n\t\t}\n\t},\n\t\n\t\n\t\n\t\n\t\/**\n\t*\t@function noBridge\n\t*\t@description Alerts the user that no parent sandbox bridge is installed \n\t*\/\n\tnoBridge: function(callback){\n\t\ttry{\n\t\t\tcallback();\n\t\t\treturn;\n\t\t}catch(e){\n\t\t\tair.Introspector.bridgeCallbacks.push(callback);\n\t\t\tair.Introspector.registerChildSandboxBridge();\n\t\t\tsetTimeout(function(){\n\t\t\t\tthrow { air_Introspector_setParentSandboxBridge: true, air_Introspector_version: air.Introspector.version , toString: function(){ return \'You need to include AIRIntrospector.js in your application sandbox.\'; } };\n\t\t\t}, 0);\t\t\t\t\n\t\t}\n\t\t\n\t},\n\t\n\t\/**\n\t*\t@function noChildBridge\n\t*\t@description Alerts the user that no child sandbox bridge is installed \n\t*\/\n\tnoChildBridge: function(iframe){\n\t\tif(!air.Introspector.secondBridgeTry){\n\t\t\tvar iframeStr = \'\';\n\t\t\tif(typeof iframe!=\'undefined\'){\n\t\t\t\tiframeStr = \" Check the following iframe [id: \"+iframe.id+\"]\";\n\t\t\t}\n\t\t\talert(\'Child sandbox bridge is not defined or has been rewritten. You need to include AIRIntrospector.js in child sandbox.\'+iframeStr);\t\t\t\n\t\t\tair.Introspector.secondBridgeTry = true;\n\t\t}\n\t},\n\t\n\n});\n\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/TreeNode.js\n\t\t\/\/=======================================================================================================================================================\n\t\tair.Introspector.tree.node = function(nodeLabel, config){\n    this.nodeLabel = nodeLabel;\n    this.openable = true;\n    this.nodeLabel2 = \'\';\n    this.items = [];\n    this.editable = false;\n    \n    if(typeof config!=\'undefined\')\n       air.Introspector.extend(this, config);\n    this.unselectOnBlur = true;\n    this.created = false;\n    this.opened = false;\n    this.shouldOpenFlag = false;\n    this.shouldSelectFlag = false;\n    \n};\n\nair.Introspector.tree.node.openedTagMac = \'<div>&gt;<\/div>\';\/\/\'&rarr;\';\nair.Introspector.tree.node.closedTagMac = \'<div>&or;<\/div>\';\/\/\'&darr;\';\n\nair.Introspector.tree.node.openedTagWin = \'<div>+<\/div>\';\/\/\'&rarr;\';\nair.Introspector.tree.node.closedTagWin = \'<div>-<\/div>\';\/\/\'&darr;\';\n\n\nair.Introspector.tree.node.putDisposeInPrototype = function(e){\n\te.prototype.dispose = air.Introspector.tree.node.prototype.dispose;\n\te.prototype.clearItems = air.Introspector.tree.node.prototype.clearItems;\n\te.prototype.clearListeners = air.Introspector.tree.node.prototype.clearListeners;\t\n\te.prototype.registerListener = air.Introspector.tree.node.prototype.registerListener;\n\te.prototype.registerEvents = air.Introspector.tree.node.prototype.registerEvents;\n\te.prototype.select = air.Introspector.tree.node.prototype.select;\n\te.prototype.unselect = air.Introspector.tree.node.prototype.unselect;\n}\n\nair.Introspector.tree.node.prototype={\n    onshow: function(){\n        \/\/dummy function overriden by config\n    },\n    \n    onclick: function(){\n        \/\/dummy function overriden by config\n    },\n    \n    onhide: function(){\n        \/\/dummy function overriden by config\n    },\n\n\tonselect: function(){\n\t\t\n\t},    \n\n    toggle: function(){\n        if(this.opened){\n            this.hide();\n        }else{\n            this.show();\n        }\n    },\n    \n    shouldOpen: function(){\n        this.opened = true;\n        this.shouldOpenFlag=true;\n    },\n    \n    shouldSelect: function(value){\n        if(typeof value==\'undefined\') value=true;\n        this.shouldSelectFlag = value;\n    },\n\n    addSelectedCss: function(){\n\t\tif(self.element){\n\t\t\tself.element.className += \' selected3Tree\';\n\t\t}\n\t},\n\n\tselect: function(throwEvent){\n        this.shouldSelectFlag = true;\n\t\tif(this.element){\n\t\t\tthis.element.className += \' selected3Tree\';\n\t\t}\n\t\t\n\t\t\n\t\tif(this.element){\n\t\t\tthis.focusLink.focus();\n\t\t\tif(this.nodeLabelDiv)\n\t\t\t\tthis.nodeLabelDiv.scrollIntoViewIfNeeded();\n\t\t\telse\n\t\t\t\tthis.element.scrollIntoViewIfNeeded();\n\t\t}\n\t\t\n\t\tif(this.onselect)\n\t\t\tthis.onselect(this, (typeof throwEvent==\'undefined\'||throwEvent));\n\t},\n\n\tunselect: function(){\n        this.shouldSelectFlag = false;\n\t\tif(this.element){\n\t\t\tthis.element.className = this.element.className.replace(\/ selected3Tree\/g, \'\');\n\t\t}\n\n\t},\n\n\tclearItems: function(){\n\t\tif(this.items){\n\t\t\tfor(var i=this.items.length-1;i>=0;i--){\n\t\t\t\ttry{\n\t\t\t\t\tthis.items[i].dispose();\n\t\t\t\t}catch(e){ air.Introspector.Console.log(e);}\n\t\t\t\tthis.items[i]=null;\n\t\t\t}\n\t\t\tthis.items.length=0;\n\t\t}\n\t},\n\tclearListeners: function(){\n\t\tif(this.listeners){\n\t\t\tfor(var i=this.listeners.length-1;i>=0;i--){\n\t\t\t\tvar listener = this.listeners[i];\n\t\t\t\tlistener.element.removeEventListener(listener.ev, listener.fn, listener.capture);\n\t\t\t\tlistener.fn = null;\n\t\t\t\tthis.listeners[i]=null;\n\t\t\t}\n\t\t}\n\t\tdelete this.listeners;\n\t},\n\tdispose: function(){\n\t\tthis.clearItems();\n\t\tthis.clearListeners();\n\t\tvar self = this;\n\t\tfor(var i in self){\n\t\t\tself[i] = null;\n\t\t}\n\t},\n\t\n\tregisterListener:function(element, ev, fn, capture){\n\t\tif(!this.listeners){\n\t\t\tthis.listeners = [];\n\t\t}\n\n\t\tthis.listeners.push({element:element, ev:ev, fn: fn, capture:capture});\t\t\n\t\telement.addEventListener(ev, fn, capture);\t\t\n\t},\n\n\tshowElements: function(){\n\t\tthis.opened = true;\n        this.onshow(this);\n\t\tthis.refreshNodeAnchor();\n        this.refreshChildren();\t\n\t},\n    show: function(){\n        if(!this.openable)\n          return;\n\t\tthis.showElements();        \n        \n        if(air.Introspector.tree.node.traceElement){\n        \tair.Introspector.tree.node.traceElement.className = air.Introspector.tree.node.traceElement.className.replace(\/ selected2Tree\/g, \'\');\n        }\n        this.element.className += \' selected2Tree\';\n        air.Introspector.tree.node.traceElement = this.element;\n        var self = this;\n        setTimeout(function(){\n\t\t\tif(self.element){\n\t\t\t\tself.element.className = self.element.className.replace(\/ selected2Tree\/g, \'\');\n\t\t\t}\n        },1500);\n        air.Introspector.tree.node.traceElement = this.element;\n        \n        \/\/this.nodeChildren.scrollIntoViewIfNeeded();\n        this.element.scrollIntoViewIfNeeded();\n        \n        \n    },\n    refresh: function(){\n\t\tthis.clearListeners();\n\t\tif(this.element){\n\t\t\tvar oldElement = this.element;\n\t\t\tthis.createDiv(oldElement.ownerDocument);\n\t\t\toldElement.parentNode.replaceChild(this.element, oldElement);\n\t\t}\n\t},\n    refreshChildren: function(){\n        if(this.created){\n            if(this.shouldOpenFlag){\n                this.onshow(this);\n                this.shouldOpenFlag = false;\n            }\n            \n            var document = this.element.ownerDocument;\n            \/*if(this.nodeChildren!=null){\n                this.element.removeChild(this.nodeChildren);\n                this.nodeChildren = null;\n            }*\/\n            var nodeChildren = this.nodeChildren;\/\/ document.createElement(\'div\');\n            nodeChildren.innerHTML = \'\';\n            this.nodeChildren.className = \'treeChildren\';\n            for(var i=0;i<this.items.length;i++){\n                 var childElement = this.items[i].createDiv(document, true);\n\t\t\t\t this.items[i].registerEvents(this, i);\n                 nodeChildren.appendChild(childElement);\n            }\n            \/\/this.element.appendChild(nodeChildren);\n            \/\/this.nodeChildren = nodeChildren;\n\t\t\tif(this.nodeEndLabelDiv)\n\t            this.nodeEndLabelDiv.className = \'nodeEndLabelDivVisible\';\n        }\n    },\n\n\tregisterEvents: function(parentNode, index){\n\t\tthis.parentNode = parentNode;\n\t\tthis.parentIndex = index;\n\t\tvar self = this;\n\n\t\tif(this.unselectOnBlur||this.readOnly)\n\t\t{\n\t\t\tthis.registerListener(this.focusLink, \'blur\', function(ev){\n\t\t\t\tself.unselect();\n\t\t\t}, true);\n\t\t}\n\t\t\n\t\tvar findFirstElement = function(node){\n\t\t\tif(node==null) return null;\n\t\t\tif(!node.items) return node;\n\t\t\tif(!node.opened) return node;\t\t\t\n\t\t\tif(node.items.length==0) return node;\n\t\t\treturn node.items[0];\n\t\t\t\/\/return findFirstElement(node.items[0]);\t\n\t\t};\n\t\tvar findLastElement = function(node){\n\t\t\tif(node==null) return null;\n\t\t\tif(!node.items) return node;\n\t\t\tif(!node.opened) return node;\t\t\t\n\t\t\tif(node.items.length==0) return node;\n\t\t\treturn findLastElement(node.items[node.items.length-1]);\t\t\t\n\t\t};\n\n\t\tvar findNextParent = function(node){\n\t\t\tif(node==null||node.parentNode==null||node.parentNode.parentNode==null) return null;\n\t\t\tif(node.parentNode.parentNode&&node.parentNode.parentNode.items.length>node.parentNode.parentIndex+1)\n\t\t\t\treturn node.parentNode.parentNode.items[node.parentNode.parentIndex+1];\n\t\t\treturn findNextParent(node.parentNode);\n\t\t}\n\t\t\n\t\tthis.registerListener(this.element, \'keydown\', function(ev){\n\t\t\tif(self.state){\n\t\t\t\tev.stopPropagation();\n\t\t\t\treturn;\n\t\t\t}\n\t\t\t\t\n\t\t\tvar stopPropagation = true;\n\t\t\tswitch(ev.keyIdentifier){\n\t\t\t\tcase \'Left\':\n\t\t\t\t\tif(self.opened){\n\t\t\t\t\t\tif(self.hide)\n\t\t\t\t\t\t\tself.hide();\n\t\t\t\t\t}\n\t\t\t\tbreak;\n\t\t\t\tcase \'Right\':\n\t\t\t\t\tif(!self.opened){\n\t\t\t\t\t\tif(self.show)\n\t\t\t\t\t\t\tself.show();\n\t\t\t\t\t}\n\t\t\t\tbreak;\n\t\t\t\tcase \'Up\':\n\t\t\t\t\tif(self.parentNode){\n\t\t\t\t\t\tself.unselect();\n\t\t\t\t\t\tif(self.parentIndex==0){\n\t\t\t\t\t\t\tself.parentNode.select(true);\n\t\t\t\t\t\t}else{\n\t\t\t\t\t\t\tvar element = findLastElement(self.parentNode.items[self.parentIndex-1]);\n\t\t\t\t\t\t\tif(element){\n\t\t\t\t\t\t\t\telement.select(true);\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\telse\n\t\t\t\t\t\t\t\tself.parentNode.items[self.parentIndex-1].select(true);\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n\t\t\t\tbreak;\n\t\t\t\tcase \'Down\':\n\t\t\t\t\tif(self.parentNode){\n\t\t\t\t\t\tvar element = findFirstElement(self);\n\t\t\t\t\t\tif(element!=null&&element!=self){\n\t\t\t\t\t\t\tself.unselect();\n\t\t\t\t\t\t\telement.select(true);\n\t\t\t\t\t\t}else if(self.parentIndex+1>=self.parentNode.items.length){\n\t\t\t\t\t\t\tvar element = findNextParent(self);\n\t\t\t\t\t\t\tif(element){\n\t\t\t\t\t\t\t\tself.unselect();\n\t\t\t\t\t\t\t\telement.select(true);\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}else{\n\t\t\t\t\t\t\tself.unselect();\n\t\t\t\t\t\t\tself.parentNode.items[self.parentIndex+1].select(true);\n\t\t\t\t\t\t}\n\t\t\t\t\t}else{\n\t\t\t\t\t\tif(self.items.length>0){\n\t\t\t\t\t\t\tself.unselect();\n\t\t\t\t\t\t\tself.items[0].select(true);\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n\t\t\t\tbreak;\n\t\t\t\tcase \'Enter\':\n\t\t\t\t\tsetTimeout(function(){\n\t\t\t\t\t\tif(self.editable){\n\t\t\t\t\t\t\tself.toggleEdit();\n\t\t\t\t\t\t}\n\t\t\t\t\t}, 0);\n\t\t\t\tbreak;\n\t\t\t\tdefault:\n\t\t\t\t\tstopPropagation = false;\n\t\t\t}\n\t\t\tif(stopPropagation){\n\/\/\t\t\t\truntime.trace(ev.keyIdentifier);\n\t\t\t\tev.stopPropagation();\n\t\t\t\tev.preventDefault();\n\t\t\t}\n\t\t});\n\t},\n    \n    refreshLabel : function(){\n       if(this.created){\n           this.nodeLabelDiv.innerHTML = air.Introspector.escapeHtml(this.nodeLabel);\n\t\t\tif(this.element){\n\t\t\t\tif(this.nodeLabel2.length==0){\n\t\t\t\t\tthis.element.className += \' noLabel2\';\n\t\t\t\t}else{\n\t\t\t\t\tthis.element.className = this.element.className.replace(\/ noLabel2\/g, \'\');\n\t\t\t\t}\n\t\t\t}\n           this.nodeLabel2Div.innerHTML = air.Introspector.escapeHtml(this.nodeLabel2).substr(0, 1000);\n       }\n    },\n    \n    hide: function(){\n        if(!this.openable)\n          return;\n        this.opened = false;\n        this.onhide(this);\n\t\tthis.refreshNodeAnchor();\n        \/*if(this.nodeChildren!=null){\n            this.element.removeChild(this.nodeChildren);\n            this.nodeChildren = null;\n        }*\/\n        \n        if(air.Introspector.tree.node.traceElement){\n            air.Introspector.tree.node.traceElement.className = air.Introspector.tree.node.traceElement.className.replace(\/ selected2Tree\/g, \'\');\n            air.Introspector.tree.node.traceElement = null;\n        }\n        this.element.className = this.element.className.replace(\/ selected2Tree\/g, \'\');\n        this.nodeChildren.innerHTML = \'\';\n        this.nodeChildren.className = \'\';\n        \n        if(this.nodeEndLabelDiv)\n            this.nodeEndLabelDiv.className = \'nodeEndLabelDiv\';\n    },\n    \n    showHover: function(visible){\n        if(visible){\n           this.element.className+=\' hover\';    \n        }else{\n           this.element.className = this.element.className.replace(\/ hover\/g, \'\');\n        }\n    },\n    toggleEdit: function(toggle){\n        if(!this.editVisible&&typeof toggle==\'undefined\'){\n            this.element.className += \' editing\';\n            this.nodeEdit.value = this.getEditValue();\n\t        this.editVisible = true;\n            this.nodeEdit.focus();\t\n\t\t\tthis.addSelectedCss();\n\t\t\tthis.state =1;\n        }else if(this.editVisible){\n            this.element.className = this.element.className.replace(\' editing\', \'\');\n\n            if(toggle){\n                 this.setEditValue(this.nodeEdit.value);\n            }else{\n\t             this.editVisible = false;\n\t\t\t}\n\n\t\t\tvar self = this;\n\t\t\t\n\t\t\tsetTimeout(function(){\n\t\t\t\tself.select();\n            \tself.editVisible = false;\n\t\t\t}, 0);\n\t\t\tthis.state = 0;\n        }\n    },\n    \n\trefreshNodeAnchor: function(){\n\t\tif(!this.nodeAnchor) return;\n\t\tif(!this.openable){\n\t\t\tthis.nodeAnchor.innerHTML= \'\';\n\t\t\tthis.nodeAnchor.className = \'treeNodeEmpty\';\n\t\t}else if(!isAppSandbox){\n\t\t\t\tthis.nodeAnchor.innerHTML = this.opened?air.Introspector.tree.node.closedTagMac:air.Introspector.tree.node.openedTagMac;\n\t\t\t\tthis.nodeAnchor.className=\'treeAnchorMac\';\t\n\t\t}else if(air.Introspector.runtime.Capabilities.os.substr(0,3).toLowerCase() == \'mac\'){\n\t\t\tthis.nodeAnchor.innerHTML = this.opened?air.Introspector.tree.node.closedTagMac:air.Introspector.tree.node.openedTagMac;\n\t\t\tthis.nodeAnchor.className=\'treeAnchorMac\';\n\t\t}else{\n\t\t\tthis.nodeAnchor.innerHTML = this.opened?air.Introspector.tree.node.closedTagWin:air.Introspector.tree.node.openedTagWin;\n\t\t\tthis.nodeAnchor.className=\'treeAnchorWin\';\n\t\t}\n\t\t\n\t},\n\t\n    createDiv: function(document, isChild){\n        var self = this;\n        var element = document.createElement(\'span\');\n\t\t\/\/element.href= \"javascript:void(0)\"; \n\t\tthis.focusLink = document.createElement(\"a\");\n\t\tthis.focusLink.href=\"#\";\n\t\telement.appendChild(this.focusLink);\n\t\n        element.className=\'treeNode\';\n        this.element = element;\n        \n        var nodeLabelDiv = document.createElement(\'div\');\n        nodeLabelDiv.innerHTML = air.Introspector.escapeHtml(this.nodeLabel);\n        nodeLabelDiv.className=\'treeLabel\'; \/\/+ (this.shouldSelectFlag?\' selectedTreeLabel\':\'\');\n\t\t\n\t\t\n\t\tif(this.shouldSelectFlag){\n\t\t\tthis.addSelectedCss();\n\t\t}\n\t\t\n\t\tif(this.onselect){\n\t\t\tthis.onselect(this, false);\n\t\t}\n\t\t\n        if(this.openable){\n           this.registerListener(nodeLabelDiv, \'click\', function(e){ if(!self.opened) self.show(); self.onclick(); \tself.select(); } );\n        }else{\n           this.registerListener(nodeLabelDiv, \'click\', function(e){ self.onclick(); \tself.select(); } );\n        }\n        this.registerListener(nodeLabelDiv, \'mouseover\', function(e){ self.showHover(true) } );\n        this.registerListener(nodeLabelDiv, \'mouseout\', function(e){ self.showHover(false) } );\n        \n        element.appendChild(nodeLabelDiv);\n        this.nodeLabelDiv = nodeLabelDiv;\n        \n        var nodeLabel2 = document.createElement(\'div\');\n        nodeLabel2.innerHTML = air.Introspector.escapeHtml(this.nodeLabel2).substr(0, 1000);\n        nodeLabel2.className=\'treeLabel2\';\n        if(this.openable){\n            this.registerListener(nodeLabel2, \'click\', function(e){ if(!self.opened) self.show() } );\n        }else if(this.editable){\n            this.editVisible = false;\n            this.registerListener(nodeLabel2, \'click\', function(e){ self.toggleEdit() } );\n            var nodeEdit = document.createElement(\'input\');\n            nodeEdit.value = \'\';\n            nodeEdit.className = \'treeEdit\';\n            element.appendChild(nodeEdit);\n            \n            this.registerListener(nodeEdit, \'blur\', function(e){  self.toggleEdit(true); } );\n            this.registerListener(nodeEdit, \'keypress\', function(e){ if(e.keyCode == 13) { self.toggleEdit(true); return false;}  else if(e.keyCode == 27){ self.toggleEdit(false); return false;}} );\n            this.nodeEdit = nodeEdit;\n        }\n        this.registerListener(nodeLabel2, \'mouseover\', function(e){ self.showHover(true) } );\n        this.registerListener(nodeLabel2, \'mouseout\', function(e){ self.showHover(false) } );\n        \n        element.appendChild(nodeLabel2);\n        this.nodeLabel2Div = nodeLabel2;\n        \n\t\tif(this.nodeLabel2.length==0){\n\t\t\tthis.element.className += \' noLabel2\';\n\t\t}\n        var nodeAnchor = document.createElement(\'div\');\n        this.nodeAnchor = nodeAnchor;\n\t\tthis.refreshNodeAnchor();\n        element.appendChild(nodeAnchor);\n        this.registerListener(nodeAnchor, \'click\', function(e){ self.select(); self.toggle() } );\n        \/\/nodeAnchor.style.visibility = this.openable?\'visible\':\'hidden\';\n\n        this.created = true;\n        \n        \n        var nodeChildren =  document.createElement(\'div\');\n        \n        element.appendChild(nodeChildren);\n        this.nodeChildren = nodeChildren;\n        \n        if(this.opened){\n           this.refreshChildren();\n        }\n        \n\t\tsetTimeout(function(){\n\t\t\tif(self.shouldSelectFlag)\n\t\t\t\tself.focusLink.focus();\n\t\n\t\t}, 0);\n\t\n\t\tif(!isChild){\n\t\t\tthis.registerEvents(null, 0);\n\t\t}\n        return element;\n    }\n};\n\nair.Introspector.tree.textNode = function(value){\n    this.nodeValue = value;\n};\n\nair.Introspector.tree.textNode.prototype = {\n    createDiv: function(document){\n\t    var element = document.createElement(\'span\');\n\/\/element.href= \"javascript:void(0)\"; \nthis.focusLink = document.createElement(\"a\");\nthis.focusLink.href=\"#\";\nelement.appendChild(this.focusLink);\n\t    element.className=\'treeText\';\n\t    \n\t    var elementText = document.createElement(\'div\');\n\t    elementText.className=\'treePreText\';\n\t    var elementLines = document.createElement(\'div\');\n\t    elementLines.className=\'treePreLine\';\n\t    var splitText = air.Introspector.blockWrap(this.nodeValue);\n\t    elementText.innerHTML = \'<pre>\'+air.Introspector.escapeHtml(splitText[0])+\'\\n\\n <\/pre>\';\n\t    \n\t    elementLines.innerHTML = \'<pre>\'+splitText[1]+\'<\/pre>\';\n\t    \n\t    element.appendChild(elementLines);\n\t    element.appendChild(elementText);\n\t    \n\t    this.element = element;\n\t    this.elementText = elementText;\n        this.elementLines = elementLines;\n\t    return element;\n    }\n};\n\nair.Introspector.tree.node.putDisposeInPrototype(air.Introspector.tree.textNode);\n\nair.Introspector.tree.textDownloadNode = function(file){\n    this.file = file;\n};\n\nair.Introspector.tree.textDownloadNode.prototype = {\n    createDiv: function(document){\n        var element = document.createElement(\'span\');\n\/\/element.href= \"javascript:void(0)\"; \nthis.focusLink = document.createElement(\"a\");\nthis.focusLink.href=\"#\";\nelement.appendChild(this.focusLink);\n        element.className=\'treeText\';\n\n        var elementText = document.createElement(\'div\');\n        elementText.className=\'treePreText\';\n        var elementLines = document.createElement(\'div\');\n        elementLines.className=\'treePreLine\';\n        \n        elementText.innerHTML =\'Loading...\';\n        \n        \n        element.appendChild(elementLines);\n        element.appendChild(elementText);\n\t\tvar self = this;\n        if(isAppSandbox){\n        \tvar scriptLoader = new air.Introspector.runtime.URLLoader();\n\t        scriptLoader.addEventListener(air.Introspector.runtime.Event.COMPLETE, function(e){\n\t        \t    var value = air.Introspector.blockWrap(scriptLoader.data);\n\t\t        \telementText.innerHTML = \'<pre>\'+air.Introspector.escapeHtml(value[0])+\'\\n\\n<\/pre>\';\n\t\t\t        elementLines.innerHTML = \'<pre>\'+value[1]+\'<\/pre>\';\n\t\t\t\t\tscriptLoader = null;\n\t\t        });\n\t\t\tscriptLoader.addEventListener(air.Introspector.runtime.IOErrorEvent.IO_ERROR, function(e){\n\t\t\t\tvar value = air.Introspector.blockWrap(\'Error reading file \"\'+self.file+\'\"\');\n\t        \telementText.innerHTML = \'<pre>\'+air.Introspector.escapeHtml(value[0])+\'\\n\\n<\/pre>\';\n\t\t        elementLines.innerHTML = \'<pre>\'+value[1]+\'<\/pre>\';\n\t\t\t});\n\t\t\tscriptLoader.load(new air.Introspector.runtime.URLRequest(this.file));\n\t\t}else{\n\t\t\tvar myWindow = window;\n\t\t\tif(typeof activeWindow!=\'undefined\'){\n\t\t\t\tvar self = this;\n\t\t\t\tactiveWindow.setTimeout(function(){\n\t\t\t\t\tvar xhr = new activeWindow.XMLHttpRequest();\n\t\t\t\t\txhr.doNotDebug = true;\n\t\t\t\t\txhr.onreadystatechange = function(){\n\t\t\t\t\t\tif(this.readyState == 4){\n\t\t\t\t\t\t \tif(this.status == 200) {\n\t\t\t\t\t\t\t\tvar responseText = xhr.responseText;\n\t\t\t\t\t\t\t\tmyWindow.setTimeout(function(){\n\t\t\t\t\t\t\t\t\t\tvar value = air.Introspector.blockWrap(responseText);\n\t\t\t\t\t\t\t        \telementText.innerHTML = \'<pre>\'+air.Introspector.escapeHtml(value[0])+\'\\n\\n<\/pre>\';\n\t\t\t\t\t\t\t\t        elementLines.innerHTML = \'<pre>\'+value[1]+\'<\/pre>\';\n\t\t\t\t\t\t\t\t}, 0);\n\t\t\t\t\t\t\t}else{\n\t\t\t\t\t\t\t\tmyWindow.setTimeout(function(){\n\t\t\t\t\t\t\t\t\tvar value = air.Introspector.blockWrap(\'Error reading file \"\'+self.file+\'\"\');\n\t\t\t\t\t\t        \telementText.innerHTML = \'<pre>\'+air.Introspector.escapeHtml(value[0])+\'\\n\\n<\/pre>\';\n\t\t\t\t\t\t\t        elementLines.innerHTML = \'<pre>\'+value[1]+\'<\/pre>\';\n\t\t\t\t\t\t\t\t}, 0);\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\txhr = null;\n\t\t\t\t\t\t}\n\t\t\t\t\t};\n\t\t\t\t\txhr.open(\'GET\', self.file, true);\n\t\t\t\t\txhr.send();\n\t\t\t\t}, 0);\n\t\t\t}\n\t\t}\n        this.element = element;\n        this.elementText = elementText;\n        this.elementLines = elementLines;\n        return element;\n    }\n};\nair.Introspector.tree.node.putDisposeInPrototype(air.Introspector.tree.textDownloadNode);\n\nair.Introspector.tree.imageDownloadNode = function(file){\n    this.file = file;\n};\n\nair.Introspector.tree.imageDownloadNode.prototype = {\n    createDiv: function(document){\n        var element = document.createElement(\'span\');\n\/\/element.href= \"javascript:void(0)\"; \nthis.focusLink = document.createElement(\"a\");\nthis.focusLink.href=\"#\";\nelement.appendChild(this.focusLink);\n        element.className=\'treeImage\';\n        var imgElement = document.createElement(\'img\');\n        imgElement.src = this.file;\n        element.appendChild(imgElement);\n        this.element = element;\n        this.imgElement = imgElement;\n        return element;\n    }\n};\nair.Introspector.tree.node.putDisposeInPrototype(air.Introspector.tree.imageDownloadNode);\n\nair.Introspector.tree.domNode = function(domNode, config){\n   this.domNode = domNode;\n   this.readOnly = false;\n   air.Introspector.tree.node.call(this, \'\', config);\n   this.openable = true;\n   this.unselectOnBlur = false;\n};\n\nair.Introspector.tree.domNode.prototype = {\n\tcreateAttribute: function(document, att){\n\t\tvar element = document.createElement(\'span\');\n\t\t\n\t\telement.appendChild(document.createTextNode(\' \'));\n\t\t\n\t\t var tagName = document.createElement(\'span\');\n         tagName.className = \'tagDomNodeAttribute\';\n         tagName.innerHTML = att.nodeName;\n         element.appendChild(tagName);\n\t\t element.appendChild(document.createTextNode(\'=\"\'));\n\t\t\n\t\tvar editor = new air.Introspector.tree.inPlaceEditor(function(){ return att.nodeValue.replace(\/\\n\/g, \'\\\\n\') }, \n\t\tfunction(value){att.nodeValue = value.replace(\/\\\\n\/g, \'\\n\')});\n\t\t editor.attributeName = att.nodeName;\n\t\tthis.registerEditor(editor);\n\t\telement.appendChild(editor.createDiv(document));\n\t\telement.appendChild(document.createTextNode(\'\"\'));\n\t\t\n\t\tthis.registerListener(element, \'mousedown\', function(e){ air.Introspector.debugWindow.setHoverDomElementAttribute(att); }, true );\n\/\/        this.registerListener(element, \'mouseout\', function(e){ air.Introspector.debugWindow.clearHoverDomElementAttribute(); }, true );\n\t\t\n\t\treturn element;\n\t},\n\t\n\t\n\ttoggleAttribute: function(attributeName){\n\t\tfor(var i=this.editors.length-1;i>=0;i--){\n\t\t\tif(typeof this.editors[i].attributeName!=\'undefined\'&&this.editors[i].attributeName==attributeName){\n\t\t\t\tthis.editors[i].toggleEditing();\n\t\t\t\treturn;\t\n\t\t\t}\n\t\t}\n\t},\n\t\n\tregisterEditor: function(editor){\n\t\tif(!this.editors)\n\t\t\tthis.editors = [];\n\t\teditor.parent = this;\n\t\tthis.editors.push(editor);\n\t},\n\tclearEditors: function(){\n\t\tif(this.editors){\n\t\t\tfor(var i=this.editors.length-1;i>=0;i--){\n\t\t\t\tthis.editors[i].dispose();\n\t\t\t}\n\t\t}\n\t},\n\tdispose: function(){\n\t\tthis.clearEditors();\n\t\tair.Introspector.tree.node.prototype.dispose.call(this);\n\t},\n\tshowDomHover:function(visible){\n\t\t\/\/air.Introspector.init();\n\t\tif(visible){\n\t\t\tair.Introspector.debugWindow.highlight(this.domNode);\n\t\t\tif(!this.readOnly)\n\t\t\t\t{air.Introspector.debugWindow.setHoverDomElement(this);}\n\t\t}else{\n\t\t\tair.Introspector.debugWindow.highlight();\n\t\t\tif(!this.readOnly)\t\t\t\n\t\t\t\t{air.Introspector.debugWindow.clearHoverDomElement();}\n\t\t}\n\t},\n\tonselect: function(sender, throwevent){\n\t\tif(this.readOnly)\n\t\t\treturn;\n\t\t\t\n\t\tif(air.Introspector.tree.domNode.lastSelectedNode&&\n\t\t\tair.Introspector.tree.domNode.lastSelectedNode.unselect&&\n\t\t\t\tair.Introspector.tree.domNode.lastSelectedNode!=sender){\n\t\t\t\t\tair.Introspector.tree.domNode.lastSelectedNode.unselect();\n\t\t}\n\t\t\t\t\t\t\n   \t   air.Introspector.tree.domNode.lastSelectedNode = sender;\n\t\n\t   if(typeof throwevent==\'undefined\'||throwevent==true){\n\t   \t\tair.Introspector.showCssElement(this.domNode);\n\t   }\t\n\t},\n\t\n\texpandAll: function(){\n\t\tthis.show();\n\t\tfor(var i=this.items.length-1;i>=0;i--)\n\t\t\tthis.items[i].expandAll();\n\t},\n\t\n\tcollapse: function(){\n\t\tfor(var i=this.items.length-1;i>=0;i--)\n\t\t\tthis.items[i].collapse();\n\t\tthis.hide();\n\t},\n\t\n\trefreshAttributes: function(){\n\t\tvar attributesTag  = this.attributesTag;\n        var atts  = this.domNode.attributes;\n\t\tattributesTag.innerHTML = \'\';\n        if(atts&&atts.length>0){\n        \tattributesTag.appendChild(document.createTextNode(\' \'));\n             for(var i=0;i<atts.length;i++){\n\t\t\t\tif(atts[i].nodeName.toLowerCase()==\'id\'){\n                \tattributesTag.appendChild(this.createAttribute(document, atts[i]));\n\t\t\t\t}\n             }\n             for(var i=0;i<atts.length;i++){\n\t\t\t\tif(atts[i].nodeName.toLowerCase()!=\'id\'){\n                \tattributesTag.appendChild(this.createAttribute(document, atts[i]));\n\t\t\t\t}\n             }\n        }\n\t},\n    createDiv: function(document, isChild){\n         var self = this;\n        var element = document.createElement(\'span\');\n\t\t\/\/element.href= \"javascript:void(0)\"; \n\t\tthis.focusLink = document.createElement(\"a\");\n\t\tthis.focusLink.href=\"#\";\n\t\telement.appendChild(this.focusLink);\n\n        element.className=\'treeNode domTreeNode\';\n        this.element = element;\n        \n        var nodeLabelDiv = document.createElement(\'div\');\n        var nodeEndLabelDiv = null;\n        \/\/should add parameters here\n        \/\/nodeLabelDiv.innerHTML = air.Introspector.escapeHtml(this.domNode.nodeName);\n        var shouldAddEndTag = false;\n        \n        switch(this.domNode.nodeType){\n        \tcase Node.DOCUMENT_NODE:\n        \tcase Node.ELEMENT_NODE:\n\t\t        nodeLabelDiv.appendChild(document.createTextNode(\'<\'));\n\t\t        var tagName = document.createElement(\'span\');\n\t\t        tagName.className = \'tagDomNodeName\';\n\t\t\t\tvar nodeName = this.domNode.nodeName;\n\t\t\t\tif(this.domNode.nodeType==Node.DOCUMENT_NODE)\n\t\t \t\t    nodeName = \'Root\';\n\t \t\t    tagName.innerHTML = nodeName;\n\t\t        nodeLabelDiv.appendChild(tagName);\n\t\t\n\t\t\n\t\t\t\tvar attributesTag = document.createElement(\'span\');\n\t\t\t\tthis.attributesTag = attributesTag;\n\t\t\t\tnodeLabelDiv.appendChild(attributesTag);\n\t\t\t\tthis.refreshAttributes();\n\t\t\t\t\n\t\t        if(this.domNode.hasChildNodes()){\n\t\t        \tshouldAddEndTag = this.domNode.firstChild.nextSibling!=null || this.domNode.firstChild.nodeType!=Node.TEXT_NODE;\n\t\t        \tif(!shouldAddEndTag){\n\t\t        \t\tnodeLabelDiv.appendChild(document.createTextNode(\'>\'));\n\t\t        \t\tvar editor = new air.Introspector.tree.inPlaceEditor(function(){ return self.domNode.firstChild.nodeValue.replace(\/\\n\/g, \'\\\\n\') }, \n                            function(value){self.domNode.firstChild.nodeValue = value.replace(\/\\\\n\/g, \'\\n\')});\n\t\t\t\t\t\t\tthis.registerEditor(editor);\n                        nodeLabelDiv.appendChild(editor.createDiv(document));\n                        \n                        var endTagName = document.createElement(\'span\');\n                        endTagName.className = \'tagDomNodeName\';\n                        endTagName.innerHTML = nodeName;\n                        nodeLabelDiv.appendChild(document.createTextNode(\'<\/\'));\n                        nodeLabelDiv.appendChild(endTagName);\n\t\t        \t\tnodeLabelDiv.appendChild(document.createTextNode(\'>\'));\n\t\t\n\t\t\n\t\t        \t}else{\n\t\t        \t\t nodeLabelDiv.appendChild(document.createTextNode(\'>\'));\n\t\t        \t}\n\t\t        }else{\n\t\t        \t   nodeLabelDiv.appendChild(document.createTextNode(\'\/>\'));\n\t\t        }\n\t            this.openable = shouldAddEndTag;\n\t            if(shouldAddEndTag){\n\t                    nodeEndLabelDiv = document.createElement(\'span\');\n                \t    nodeEndLabelDiv.className = this.opened?\'nodeEndLabelDivVisible\':\'nodeEndLabelDiv\';\n\t            \t    var endTagName = document.createElement(\'span\');\n                        endTagName.className = \'tagDomNodeName\';\n                        endTagName.innerHTML = nodeName;\n                        \n                        nodeEndLabelDiv.appendChild(document.createTextNode(\'<\/\'));\n                        nodeEndLabelDiv.appendChild(endTagName);\n                        nodeEndLabelDiv.appendChild(document.createTextNode(\'>\'));\n                       \n\t            \t\n                   \/\/ nodeLabelDiv.addEventListener(\'click\', function(e){ self.toggle(); e.stopPropagation(); } );\n                    var nodeAnchor = document.createElement(\'div\');\n\t\t\t        element.appendChild(nodeAnchor);\n\n\t\t\t\t\t\n\t\t\t        this.registerListener(nodeAnchor, \'mouseover\', function(e){ self.showDomHover(true);  self.showHover(true); e.stopPropagation();} );\n                    this.registerListener(nodeAnchor, \'mouseout\', function(e){ self.showDomHover(false);  self.showHover(false); e.stopPropagation(); } );\n        \t\t\tthis.nodeAnchor = nodeAnchor;\n\t\t\t\t\tthis.refreshNodeAnchor();\n\t\t\t\t\t\n\t\t\t        this.registerListener(nodeEndLabelDiv, \'mouseover\', function(e){ self.showDomHover(true);  self.showHover(true); e.stopPropagation();} );\n                    this.registerListener(nodeEndLabelDiv, \'mouseout\', function(e){ self.showDomHover(false);  self.showHover(false); e.stopPropagation(); } );\n\n                    this.registerListener(nodeAnchor, \'click\', function(e){ self.select(); self.toggle(); e.stopPropagation(); } );\n\t\t\t\t\tif(this.domNode.nodeType==Node.DOCUMENT_NODE)\n\t\t\t\t\t\tthis.shouldOpen();\n                }else{\n                \tthis.opened = false;\n                }\n            break;\n            case Node.TEXT_NODE:\n               var editor = new air.Introspector.tree.inPlaceEditor(function(){ return self.domNode.nodeValue.replace(\/\\n\/g, \'\\\\n\') }, \n                            function(value){self.domNode.nodeValue = value.replace(\/\\\\n\/g, \'\\n\')});\n                   this.registerEditor(editor);\n               nodeLabelDiv.appendChild(editor.createDiv(document));\n               this.openable = false;\n               this.opened = false;\n        \tbreak;\n\t\t\tdefault:\n\t\t\t\tnodeLabelDiv.appendChild(document.createTextNode(\'<\'));\n\t\t        var tagName = document.createElement(\'span\');\n\t\t        tagName.className = \'tagDomNodeName\';\n\t \t\t    tagName.innerHTML = this.domNode.nodeName;\n\t\t        nodeLabelDiv.appendChild(tagName);\n\t\t\t\tnodeLabelDiv.appendChild(document.createTextNode(\'>\'));\n\t\t\tbreak;\n        }\n        \n         \/*var child = this.domNode.firstChild;\n         while(child)\n         {\n                node.items.push(this.createDomTreeNode(child)); \/\/, this.createGetter(obj, i), this.createSetter(obj, i)));\n                child = child.nextSibling;\n         }*\/   \n        \n        nodeLabelDiv.className=\'treeLabel\';\/\/ + (this.shouldSelectFlag?\' selectedTreeLabel\':\'\');\n\n\t\tif(this.shouldSelectFlag){\n\t\t\tthis.addSelectedCss();\n\t\t}\n        \n\n       this.registerListener( nodeLabelDiv, \'mouseover\', function(e){ self.showDomHover(true);  self.showHover(true); e.stopPropagation(); }, true );\n       this.registerListener(nodeLabelDiv, \'mouseout\', function(e){ self.showDomHover(false);  self.showHover(false); e.stopPropagation(); }, true );\n\n        element.appendChild(nodeLabelDiv);\n        this.nodeLabelDiv = nodeLabelDiv;\n\n       this.registerListener(nodeLabelDiv, \'click\', function(e){\n\t\t\tself.select();\n\t\t\t   \/\/self.toggle();\n\t\t\t   self.show();\n               air.Introspector.debugWindow.showDomElementPath(self.domNode);\n\t\t});\n        \n        this.registerListener(element, \'dblclick\', function(e){\n\t\t\t   \te.stopPropagation();  \n\t\t\/\/\t\tself.toggle();\n\t\t\/\/\t\tself.select();\n               \/\/air.Introspector.debugWindow.gotoConsoleTab();\n\/\/               air.Introspector.debugWindow.showDomElementPath(self.domNode);\n                } );\n        \n        \n        \/\/nodeAnchor.style.visibility = this.openable?\'visible\':\'hidden\';\n\n        this.created = true;\n        \n        \n        var nodeChildren =  document.createElement(\'div\');\n        element.appendChild(nodeChildren);\n        this.nodeChildren = nodeChildren;\n        \n        if(nodeEndLabelDiv){\n\t\t\tthis.registerListener(nodeEndLabelDiv, \'click\', function(e){\n\/\/\t\t\t\t\tself.toggle();\n\t\t\t\t\tself.select();\n\t               air.Introspector.debugWindow.showDomElementPath(self.domNode);\n\t\t\t});\n\t \t\telement.appendChild(nodeEndLabelDiv);\n\t\t}\n        this.nodeEndLabelDiv = nodeEndLabelDiv;\n        \n        if(this.opened){\n          this.refreshChildren();\n        }\n       \n \t\tif(!isChild){\n\t\t\tthis.registerEvents(null, 0);\n\t\t}\n\t\t\n        return element;\n    }\n};\n\nair.Introspector.tree.domNode.prototype.__proto__ = air.Introspector.tree.node.prototype;\n\nair.Introspector.tree.inPlaceEditor = function(getter, setter, className){\n    this.getter = getter;\n    this.setter = setter; \n    if(typeof className!=\'undefined\'){\n        this.className = \' \'+className;\n    }else{\n        this.className = \'\';\n    }\n};\n\nair.Introspector.tree.inPlaceEditor.prototype = {\n\tnoZeroLength: function(e){\n\t\ttry{\n\t\t  var str = e+\'\';\n\t\t  if(air.Introspector.trim(e).length==0){\n\t\t  \treturn \'[Empty string - click to edit]\';\n\t\t  }\n\t\t  return str;\n\t\t}catch(e){\n\t\t\treturn e+\'\';\n\t\t}\n\t},\n\ttoggleEditing: function(){\n\t\tthis.onclick();\n\t},\n\tselectParent: function(){\n\t\tif(this.parent&&this.parent.select){\n\t\t\tthis.parent.select(true);\n\t\t}\n\t},\n\t\n\tonclick: function(){\n\t\tvar self = this;\n\t\tthis.selectParent();\n\t\tif(this.parent&&this.parent.readOnly)\n\t\t\t{return;}\n\t\tif(this.state==0){\/\/not editing\n\t\t  var editor = document.createElement(\'input\');\n\t\t  editor.type = \'text\';\n          \n\t\t  try{\n\t         editor.value = this.getter();\n\t      }catch(e){\n\t         editor.value = e;  \n\t      }\n          \n          if (this.element.clientWidth > 0) {\n              editor.style.width = this.element.clientWidth + \'px\';\n          }    \n          else {\n              \/\/ newer versions of webkit return 0 as clientWidth for inline elements like span unlike Safari 3.x which was used in AIR1.x\n              \/\/  AIR 2.0 uses Safari 4.x WebKit and therefore must use getBoundingClientRect to get the computed width for the span element\n              \/\/ getBoundingClientRect support was added in WebKit starting with http:\/\/trac.webkit.org\/changeset\/40837\n              \/\/ and bug https:\/\/bugs.webkit.org\/show_bug.cgi?id=15897\n              editor.style.width = this.element.getBoundingClientRect().width + \'px\';  \n          }      \n\t\t  this.editor = editor;\n\t\t  this.element.innerHTML = \'\';\n\t\t  this.element.appendChild(editor);\n\t\t  this.saveValue = true;\n\t\t  \n\t\t  this.registerListener(editor,\'dblclick\', function(e){\n                        e.stopPropagation(); \n          }, true );\n\t\t  this.registerListener(editor, \'click\', function(e){ e.stopPropagation(); }, true );\n\t\t  this.registerListener(editor, \'keypress\', function(e){ if(self.state) { if(e.keyCode == 13) { self.saveValue=true; editor.blur(); return false;} else if(e.keyCode == 27){ self.saveValue=false; editor.blur(); return false;}}} );\n\t\t  this.registerListener(editor, \'blur\', function(){ if(self.state) self.onclick(); } );\n\t\t  editor.focus();\n\t\t}else{ \/\/editing mode\n\t\t\t\/\/set the value to setter\n\t\t\tif(this.saveValue){\n\t\t\t  try{\n\t            this.setter(this.editor.value);\t\t  \n\t\t\t  }catch(e){\n\t\t\t  }\n\t\t\t}\n\t\t  setTimeout(function(){\n\t\t      try{\n\t            self.element.innerHTML = self.noZeroLength(air.Introspector.escapeHtml(self.getter()));\n\t\t      }catch(e){\n\t\t        \/\/self.element.innerHTML = air.Introspector.escapeHtml(e);  \n\t\t      } \n\t      }, 100);\n\t\t}\n\t\tthis.state ^= 1 ;\n\t\tif(this.parent)\n\t\t\tthis.parent.state = this.state;\n\t},\n    createDiv: function(document){\n\t\tvar element = document.createElement(\'div\');\n        element.className=\'inPlaceEditor\'+this.className;\n\n    \tvar self = this;\n    \tthis.state = 0; \/\/not editing\n\n        \n        try{\n            element.innerHTML = this.noZeroLength(air.Introspector.escapeHtml(this.getter()));\n        }catch(e){\n            element.innerHTML = air.Introspector.escapeHtml(e);\t\n        }\n        \n        this.registerListener(element, \'click\', function(e){ if(self.state==0) self.onclick(); e.stopPropagation(); } , true );\n        \n      \/\/  element.addEventListener(\'dblclick\', function(e){\n      \/\/                  e.stopPropagation(); \n      \/\/          }, true );\n        \n        \n        this.element = element;\n        return element;\n    }\n};\nair.Introspector.tree.node.putDisposeInPrototype(air.Introspector.tree.inPlaceEditor);\n\nair.Introspector.split = function(element, leftElement, rightElement, firstWidth, minWidth){\n\tthis.element = element;\n\tthis.leftElement = leftElement;\n\tthis.rightElement = rightElement;\n\tvar self = this;\n\tthis.dragging = false;\n\t\n\tthis.element.style.right = (firstWidth)+\'px\';\n\tthis.leftElement.style.right = (firstWidth+this.element.clientWidth)+\'px\';\n\tthis.rightElement.style.width = (firstWidth)+\'px\';\t\n\tthis.firstWidth = firstWidth;\n\tthis.minWidth = minWidth;\n\tif(isAppSandbox){\n\t\tthis.element.ownerDocument.defaultView.nativeWindow.stage.addEventListener(runtime.flash.events.MouseEvent.MOUSE_MOVE, function(ev){\n\t\t\tself.mousemove(ev);\n\t\t}, true);\n\t}else{\n\t\tthis.element.ownerDocument.addEventListener(\"mousemove\", function(ev){self.mousemove(ev);}, true);\n\t}\n\tthis.element.ownerDocument.addEventListener(\"mouseup\", function(ev){ self.mouseup(ev); }, true);\n\tthis.element.addEventListener(\"mousedown\", function(ev){self.mousedown(ev)}, true);\t\t\n};\n\nair.Introspector.split.prototype = {\n\tmousemove:function(ev){\n\t\tif(this.dragging){\n\t\t\tif(isAppSandbox){\n\t\t\t\tvar delta = this.firstX-ev.localX;\n\t\t\t}else{\n\t\t\t\tvar delta = this.firstX-ev.clientX;\n\t\t\t}\n\t\t\t\t\n\t\t\tvar currentWidth = this.firstWidth + delta;\n\n\t\t\tif(currentWidth<this.minWidth){\n\t\t\t\tcurrentWidth=this.minWidth;\n\t\t\t}\n\t\t\t\n\t\t\tthis.currentWidth = currentWidth;\n\t\t\tthis.element.style.right = (currentWidth)+\'px\';\n\t\t\tthis.leftElement.style.right = (currentWidth+this.element.clientWidth)+\'px\';\n\t\t\tthis.rightElement.style.width = (currentWidth)+\'px\';\t\n\t\t\tif(isAppSandbox){\n\t\t\t\tev.stopImmediatePropagation();\n\t\t\t}\n\t\t\tev.stopPropagation();\t\n\t\t\tev.preventDefault();\n\t\t}\n\t},\n\tmousedown: function(ev){\n\t\tthis.dragging = true;\n\t\tthis.firstX = ev.clientX;\n\t\tthis.currentWidth = this.firstWidth;\n\t\tev.stopPropagation();\t\t\n\t\tev.preventDefault();\n\t},\n\tmouseup: function(ev){\n\t\tif(this.dragging){\n\n\t\t\tthis.dragging = false;\n\t\t\tthis.firstWidth = this.currentWidth;\n\t\t\tev.stopPropagation();\n\t\t\tev.preventDefault();\n\t\t}\n\t}\n};\n\n\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\t\t\n\t    air.Introspector.getCssGroups = function(){\n\t\t\tvar cssGroupNames = {\n\t0: \"Background\",\n\t1: \"Box Model\",\n\t2: \"Layout\",\n\t3: \"Text\",\n\t4: \"Other\",\n\tlength: 5\n};\n\nvar cssGroups = [];\n\n\/*\n$group = 0;\n\/\/Background - 0\ncssGroups[\'background-attachment\']=$group;\ncssGroups[\'background-color\']=$group;\ncssGroups[\'background-image\']=$group;\ncssGroups[\'background-position\']=$group;\ncssGroups[\'background-repeat\']=$group;\ncssGroups[\'opacity\']=$group;\ncssGroups[\'-webkit-background-clip\']=$group;\ncssGroups[\'-webkit-background-composite\']=$group;\ncssGroups[\'-webkit-background-origin\']=$group;\ncssGroups[\'-webkit-background-size\']=$group;\n\n$group = 1;\n\/\/Box Model - 1\ncssGroups[\'border-bottom-color\']=$group;\ncssGroups[\'border-bottom-style\']=$group;\ncssGroups[\'border-bottom-width\']=$group;\ncssGroups[\'border-collapse\']=$group;\ncssGroups[\'border-left-color\']=$group;\ncssGroups[\'border-left-style\']=$group;\ncssGroups[\'border-left-width\']=$group;\ncssGroups[\'border-right-color\']=$group;\ncssGroups[\'border-right-style\']=$group;\ncssGroups[\'border-right-width\']=$group;\ncssGroups[\'border-top-color\']=$group;\ncssGroups[\'border-top-style\']=$group;\ncssGroups[\'border-top-width\']=$group;\ncssGroups[\'bottom\']=$group;\ncssGroups[\'height\']=$group;\ncssGroups[\'left\']=$group;\ncssGroups[\'top\']=$group;\ncssGroups[\'right\']=$group;\ncssGroups[\'width\']=$group;\ncssGroups[\'padding-bottom\']=$group;\ncssGroups[\'padding-left\']=$group;\ncssGroups[\'padding-right\']=$group;\ncssGroups[\'padding-top\']=$group;\ncssGroups[\'margin-bottom\']=$group;\ncssGroups[\'margin-left\']=$group;\ncssGroups[\'margin-right\']=$group;\ncssGroups[\'margin-top\']=$group;\ncssGroups[\'-webkit-border-fit\']=$group;\ncssGroups[\'-webkit-border-horizontal-spacing\']=$group;\ncssGroups[\'-webkit-border-vertical-spacing\']=$group;\ncssGroups[\'-webkit-box-align\']=$group;\ncssGroups[\'-webkit-box-direction\']=$group;\ncssGroups[\'-webkit-box-flex\']=$group;\ncssGroups[\'-webkit-box-flex-group\']=$group;\ncssGroups[\'-webkit-box-lines\']=$group;\ncssGroups[\'-webkit-box-ordinal-group\']=$group;\ncssGroups[\'-webkit-box-orient\']=$group;\ncssGroups[\'-webkit-box-pack\']=$group;\ncssGroups[\'-webkit-box-shadow\']=$group;\ncssGroups[\'-webkit-border-bottom-left-radius\']=$group;\ncssGroups[\'-webkit-border-bottom-right-radius\']=$group;\ncssGroups[\'-webkit-border-top-left-radius\']=$group;\ncssGroups[\'-webkit-border-top-right-radius\']=$group;\ncssGroups[\'-webkit-margin-bottom-collapse\']=$group;\ncssGroups[\'-webkit-margin-top-collapse\']=$group;\n\n\n$group = 2;\n\/\/Layout - 2\ncssGroups[\'position\']=$group;\ncssGroups[\'visibility\']=$group;\ncssGroups[\'display\']=$group;\ncssGroups[\'z-index\']=$group;\ncssGroups[\'overflow-x\']=$group;\ncssGroups[\'overflow-y\']=$group;\ncssGroups[\'white-space\']=$group;\ncssGroups[\'clear\']=$group;\ncssGroups[\'float\']=$group;\ncssGroups[\'-webkit-box-sizing\']=$group;\n\n$group = 3;\n\/\/Text - 3\ncssGroups[\'text-align\']=$group;\ncssGroups[\'text-decoration\']=$group;\ncssGroups[\'text-indent\']=$group;\ncssGroups[\'text-shadow\']=$group;\ncssGroups[\'text-transform\']=$group;\ncssGroups[\'font-family\']=$group;\ncssGroups[\'font-size\']=$group;\ncssGroups[\'font-style\']=$group;\ncssGroups[\'font-variant\']=$group;\ncssGroups[\'font-weight\']=$group;\ncssGroups[\'letter-spacing\']=$group;\ncssGroups[\'line-height\']=$group;\ncssGroups[\'-webkit-text-decorations-in-effect\']=$group;\ncssGroups[\'-webkit-text-fill-color\']=$group;\ncssGroups[\'-webkit-text-security\']=$group;\ncssGroups[\'-webkit-text-stroke-color\']=$group;\ncssGroups[\'-webkit-text-stroke-width\']=$group;\ncssGroups[\'word-spacing\']=$group;\ncssGroups[\'word-wrap\']=$group;\n\n\n\n$group = 4;\n\/\/Other - 4\n\ncssGroups[\'caption-side\']=$group;\ncssGroups[\'color\']=$group;\ncssGroups[\'cursor\']=$group;\ncssGroups[\'direction\']=$group;\ncssGroups[\'empty-cells\']=$group;\n\n\ncssGroups[\'list-style-image\']=$group;\ncssGroups[\'list-style-position\']=$group;\ncssGroups[\'list-style-type\']=$group;\ncssGroups[\'max-height\']=$group;\ncssGroups[\'max-width\']=$group;\ncssGroups[\'min-height\']=$group;\ncssGroups[\'min-width\']=$group;\n\ncssGroups[\'orphans\']=$group;\ncssGroups[\'outline-color\']=$group;\ncssGroups[\'outline-style\']=$group;\ncssGroups[\'outline-width\']=$group;\n\n\ncssGroups[\'page-break-after\']=$group;\ncssGroups[\'page-break-before\']=$group;\ncssGroups[\'page-break-inside\']=$group;\n\ncssGroups[\'resize\']=$group;\n\ncssGroups[\'table-layout\']=$group;\n\ncssGroups[\'unicode-bidi\']=$group;\ncssGroups[\'vertical-align\']=$group;\n\n\ncssGroups[\'widows\']=$group;\n\ncssGroups[\'-webkit-appearance\']=$group;\ncssGroups[\'-webkit-column-break-after\']=$group;\ncssGroups[\'-webkit-column-break-before\']=$group;\ncssGroups[\'-webkit-column-break-inside\']=$group;\ncssGroups[\'-webkit-column-count\']=$group;\ncssGroups[\'-webkit-column-gap\']=$group;\ncssGroups[\'-webkit-column-rule-color\']=$group;\ncssGroups[\'-webkit-column-rule-style\']=$group;\ncssGroups[\'-webkit-column-rule-width\']=$group;\ncssGroups[\'-webkit-column-width\']=$group;\ncssGroups[\'-webkit-highlight\']=$group;\ncssGroups[\'-webkit-line-break\']=$group;\ncssGroups[\'-webkit-line-clamp\']=$group;\ncssGroups[\'-webkit-marquee-direction\']=$group;\ncssGroups[\'-webkit-marquee-increment\']=$group;\ncssGroups[\'-webkit-marquee-repetition\']=$group;\ncssGroups[\'-webkit-marquee-style\']=$group;\ncssGroups[\'-webkit-nbsp-mode\']=$group;\ncssGroups[\'-webkit-rtl-ordering\']=$group;\ncssGroups[\'-webkit-user-drag\']=$group;\ncssGroups[\'-webkit-user-modify\']=$group;\ncssGroups[\'-webkit-user-select\']=$group;\ncssGroups[\'-webkit-dashboard-region\']=$group;\n\n*\/\n\ncssGroups[0]=[\'background-attachment\', \'background-color\', \'background-image\', \'background-position\', \'background-repeat\', \'opacity\', \'-webkit-background-clip\', \'-webkit-background-composite\', \'-webkit-background-origin\', \'-webkit-background-size\', ];\n\n\/\/Box Model - 1\ncssGroups[1]=[\'border-bottom-color\', \'border-bottom-style\', \'border-bottom-width\', \'border-collapse\', \'border-left-color\', \'border-left-style\', \'border-left-width\', \'border-right-color\', \'border-right-style\', \'border-right-width\', \'border-top-color\', \'border-top-style\', \'border-top-width\', \'bottom\', \'height\', \'left\', \'top\', \'right\', \'width\', \'padding-bottom\', \'padding-left\', \'padding-right\', \'padding-top\', \'margin-bottom\', \'margin-left\', \'margin-right\', \'margin-top\', \'-webkit-border-fit\', \'-webkit-border-horizontal-spacing\', \'-webkit-border-vertical-spacing\', \'-webkit-box-align\', \'-webkit-box-direction\', \'-webkit-box-flex\', \'-webkit-box-flex-group\', \'-webkit-box-lines\', \'-webkit-box-ordinal-group\', \'-webkit-box-orient\', \'-webkit-box-pack\', \'-webkit-box-shadow\', \'-webkit-border-bottom-left-radius\', \'-webkit-border-bottom-right-radius\', \'-webkit-border-top-left-radius\', \'-webkit-border-top-right-radius\', \'-webkit-margin-bottom-collapse\', \'-webkit-margin-top-collapse\', ];\n\n\n\/\/Layout - 2\ncssGroups[2]=[\'position\', \'visibility\', \'display\', \'z-index\', \'overflow-x\', \'overflow-y\', \'white-space\', \'clear\', \'float\', \'-webkit-box-sizing\', ];\n\n\/\/Text - 3\ncssGroups[3]=[\'text-align\', \'text-decoration\', \'text-indent\', \'text-shadow\', \'text-transform\', \'font-family\', \'font-size\', \'font-style\', \'font-variant\', \'font-weight\', \'letter-spacing\', \'line-height\', \'-webkit-text-decorations-in-effect\', \'-webkit-text-fill-color\', \'-webkit-text-security\', \'-webkit-text-stroke-color\', \'-webkit-text-stroke-width\', \'word-spacing\', \'word-wrap\', ];\n\n\n\/\/Other - 4\n\ncssGroups[4]=[\'caption-side\', \'color\', \'cursor\', \'direction\', \'empty-cells\', \n\t\'list-style-image\', \'list-style-position\', \'list-style-type\', \'max-height\', \'max-width\', \'min-height\', \'min-width\', \n\t\'orphans\', \'outline-color\', \'outline-style\', \'outline-width\', \n\t\'page-break-after\', \'page-break-before\', \'page-break-inside\', \n\t\'resize\', \n\t\'table-layout\', \n\t\'unicode-bidi\', \'vertical-align\', \n\t\'widows\', \n\t\'-webkit-appearance\', \'-webkit-column-break-after\', \'-webkit-column-break-before\', \'-webkit-column-break-inside\', \'-webkit-column-count\', \'-webkit-column-gap\', \'-webkit-column-rule-color\', \'-webkit-column-rule-style\', \'-webkit-column-rule-width\', \'-webkit-column-width\', \'-webkit-highlight\', \'-webkit-line-break\', \'-webkit-line-clamp\', \'-webkit-marquee-direction\', \'-webkit-marquee-increment\', \'-webkit-marquee-repetition\', \'-webkit-marquee-style\', \'-webkit-nbsp-mode\', \'-webkit-rtl-ordering\', \'-webkit-user-drag\', \'-webkit-user-modify\', \'-webkit-user-select\', \'-webkit-dashboard-region\',];\n\n\n\n\n\n\t\t\treturn [cssGroupNames, cssGroups];\n\t\t}\t\n\t\t\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/DebugWindow.js\n\t\t\/\/=======================================================================================================================================================\n\t\t\n\nair.Introspector.DebugWindow = function(config){\n    var self = this;\n    this.logLines = [];\n    this.domList = [];\n    this.isLoaded =  false;\n\n\tif(isAppSandbox){\n    \tthis.activeWindow = air.Introspector.runtime.NativeApplication.nativeApplication.activeWindow;\n\t}else{\n\t\t\/\/will get it from config later\n\t\tthis.activeWindow = activeWindow;\n\t}\n\n    this.isInspecting = false;\n    this.evalHistory = [];\n    this.evalHistoryPos = -1;\n    this.selectedTab = \'console\';\n    this.scrollDisabled = false;\n    this.requestLog = [];\n    this.activeTab = 0;\n    this.tabs = [\'console\', \'html2\', \'dom\',\'assets\',\'source\',\'net\'];\n\tthis.cssTabs = [\'css2Dom\', \'css2Style\', \'css2Box\'];\n\tthis.activeCssTab = 0;\n\tthis.activateDebug = false;\n\tthis.isWindowCreated = false;\n\tthis.isMenuVisible = false;\n    \/\/var initOptions = new air.Introspector.runtime.NativeWindowInitOptions();\n    \/\/initOptions.transparent = true;\n    \/\/initOptions.systemChrome = \"none\";\n    if(typeof config!=\'undefined\'){\n\t\tair.Introspector.extend(this, config);\n\t}\n\tvar css = air.Introspector.getCssGroups();\n\tthis.cssGroupNames = css[0];\n\tthis.cssGroups = css[1];\n\tthis.isAppSandbox = isAppSandbox;\n}\n\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\tair.Introspector.DebugWindow.prototype = \n\t\t{\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/DebugWindow.INIT.js\n\t\t\/\/=======================================================================================================================================================\n\t\tsaveConfig: function(force){\n\tif(this.evalHistory&&this.evalHistory.length>30){\n\t\tthis.evalHistory.splice(0, this.evalHistory.length-30);\n\t\tthis.evalHistoryPos = this.evalHistory.length;\n\t}\n\t\n\tvar config = {\n\t\tevalHistory : this.evalHistory\n\t};\n\t\n\tif(isAppSandbox){\n\t\tif(this.nativeWindow.displayState==runtime.flash.display.NativeWindowDisplayState.NORMAL){\n\t\t\tconfig.rect = {\n\t\t\t\tx:this.nativeWindow.x,\n\t\t\t\ty:this.nativeWindow.y,\n\t\t\t\twidth:this.nativeWindow.width,\n\t\t\t\theight:this.nativeWindow.height\n\t\t\t};\n\t\t}else if(this.lastConfig){\n\t\t\tconfig.rect = this.lastConfig.rect;\n\t\t}\n\t\tconfig.windowState = this.nativeWindow.displayState;\n\t}else{\n\t\tconfig.rect = {\n\t\t\tx:this.window.screenX,\n\t\t\ty:this.window.screenY,\n\t\t\twidth:this.window.outerWidth,\n\t\t\theight:this.window.outerHeight\n\t\t};\n\t}\n\n\tvar ok = typeof force!=\'undefined\'?force:false;\n\tif(!ok&&this.lastConfig){\n\t\tif(!ok&&this.lastConfig.rect&&config.rect){\n\t\t\tif(config.rect.x!=this.lastConfig.rect.x) ok=true;\n\t\t\tif(config.rect.y!=this.lastConfig.rect.y) ok=true;\n\t\t\tif(config.rect.width!=this.lastConfig.rect.width) ok=true;\n\t\t\tif(config.rect.height!=this.lastConfig.rect.height) ok=true;\t\t\t\n\t\t}else{ ok=true; }\n\t\tif(!ok&&this.lastConfig.windowState&&config.windowState){\n\t\t\tif(config.windowState!=this.lastConfig.windowState) ok=true;\t\t\t\n\t\t}else{ ok=true; }\n\t}\n\tif(!ok)return;\n\tif(this.writeTimeout){\n\t\tclearTimeout(this.writeTimeout);\n\t\tthis.writeTimeout = null;\n\t}\n\t\n\tthis.writeTimeout = setTimeout(function(){ \n\t\tair.Introspector.writeConfigFile(config);\t\t\n\t}, 300);\n\n\tthis.lastConfig = config;\n},\n\nreadConfig: function(config){\n\tif(config.evalHistory){\n\t\tthis.evalHistory = config.evalHistory;\n\t\tthis.evalHistoryPos = config.evalHistory.length;\n\t}\n\tif(isAppSandbox){\n\t\tif(config.rect){\n\t\t\tthis.nativeWindow.x = config.rect.x;\n\t\t\tthis.nativeWindow.y = config.rect.y;\n\t\t\tthis.nativeWindow.width = config.rect.width;\n\t\t\tthis.nativeWindow.height = config.rect.height;\n\t\t}\n\t\tif(config.windowState){\n\t\t\tswitch(config.windowState){\n\t\t\t\tcase runtime.flash.display.NativeWindowDisplayState.MAXIMIZED:\n\t\t\t\t\tthis.nativeWindow.maximize();\t\t\t\t\t\n\t\t\t\t\tbreak;\n\t\t\t\tcase runtime.flash.display.NativeWindowDisplayState.MINIMIZED:\n\t\t\t\t\tthis.nativeWindow.minimize();\t\t\t\t\t\n\t\t\t\t\tbreak;\n\t\t\t}\n\t\t}\n\t}else{\n\t\tif(config.rect){\n\t\t\tthis.window.moveTo(config.rect.x, config.rect.y);\n\t\t\tthis.window.resizeTo(config.rect.width, config.rect.height);\n\t\t}\n\t}\n\tthis.lastConfig = config;\n},\n\n\ninitConfigFile: function(){\n\tvar self = this;\n\n\tif(isAppSandbox){\n\t\tvar config = air.Introspector.readConfigFile();\n\t\tif(typeof config==\'undefined\') config = {};\n\t\tthis.readConfig(config);\n\t\tthis.nativeWindow.addEventListener(runtime.flash.events.NativeWindowBoundsEvent.MOVE , function(){ self.saveConfig(); });\n\t\tthis.nativeWindow.addEventListener(runtime.flash.events.NativeWindowBoundsEvent.RESIZE , function(){ self.saveConfig(); });\t\n\t}else{\n\t\tair.Introspector.readConfigFile(true, function(config){\n\t\t\tif(typeof config==\'undefined\') config = {};\n\t\t\tself.readConfig(config);\n\t\t\tsetInterval(function() { self.saveConfig();  }, 900);\n\t\t});\n\t}\n},\n\n\n\nloadUIElements: function(){\n\t\tthis.consoleDiv = this.htmlLoader.window.document.getElementById(\'console\');\n\t    this.windowsDiv = this.htmlLoader.window.document.getElementById(\'windowList\');\n\t    this.tabLabels = this.htmlLoader.window.document.getElementById(\'tabLabels\');\n\n\t\/\/        this.consoleTabLabel = this.htmlLoader.window.document.getElementById(\'consoleTabLabel\');\n\t\/\/        this.html2TabLabel = this.htmlLoader.window.document.getElementById(\'html2TabLabel\');\n\n\t    this.evalConsoleText = this.htmlLoader.window.document.getElementById(\'evalConsoleText\');\n\t    this.tabPages = this.htmlLoader.window.document.getElementById(\'tabPages\');\n\n\t    this.cssTabLabels = this.htmlLoader.window.document.getElementById(\'css2TabLabels\');\n\t    this.cssTabPages = this.htmlLoader.window.document.getElementById(\'css2TabPages\');\n\n\n\t    this.domDiv = this.htmlLoader.window.document.getElementById(\'domTab\');\n\t    \/\/this.htmlDiv = this.htmlLoader.window.document.getElementById(\'htmlTab\');\n\t    this.netTabDiv = this.htmlLoader.window.document.getElementById(\'netTab\');\n\t    this.html2TabDiv = this.htmlLoader.window.document.getElementById(\'html2Tab\');\n\t\tthis.html2Div = this.htmlLoader.window.document.getElementById(\'html2Div\');\n\t\tthis.html2Split = this.htmlLoader.window.document.getElementById(\'html2Split\');\n\t\tthis.css2Div = this.htmlLoader.window.document.getElementById(\'css2Div\');\n\t\tthis.css2SplitObj = new air.Introspector.split(this.html2Split, this.html2Div, this.css2Div , 300, 150);\n\t    this.assetsTabDiv = this.htmlLoader.window.document.getElementById(\'assetsTab\');\n\t    this.sourcesTabDiv = this.htmlLoader.window.document.getElementById(\'sourceTab\');\n\t    this.inspectToolLabel = this.htmlLoader.window.document.getElementById(\'inspectToolLabel\');\n\t    this.consoleMenuDiv = this.htmlLoader.window.document.getElementById(\'consoleMenuDiv\');\n\t    this.domMenuDiv = this.htmlLoader.window.document.getElementById(\'domMenuDiv\');\t\n\t\tthis.menuDiv = this.htmlLoader.window.document.getElementById(\'menuDiv\');\t\n\n\t\tthis.domRemoveAttributeMenuDiv = this.htmlLoader.window.document.getElementById(\'domRemoveAttributeMenuDiv\');\n\n\t\tthis.css2DomTab = this.htmlLoader.window.document.getElementById(\'css2DomTab\');\n\t\tthis.css2StyleTab = this.htmlLoader.window.document.getElementById(\'css2StyleTab\');\n\n\t\tthis.refreshActiveWindowButton = this.htmlLoader.window.document.getElementById(\'refreshActiveWindow\');\n\t\n\t},\n\t\n\ncreateMenu: function(){\n\tvar self = this;\n\tif(isAppSandbox){\n\t\tvar consoleMenu = new runtime.flash.display.NativeMenu();\n\t\tvar domMenu = new runtime.flash.display.NativeMenu();\n\t\t\n\t\tvar clearMenu = new runtime.flash.display.NativeMenuItem(\'Clear console\');\n\t\tclearMenu.addEventListener(runtime.flash.events.Event.SELECT, function(){ self.clearConsole(); });\n\n\t\tvar saveMenu = new runtime.flash.display.NativeMenuItem(\'Save console to file...\');\n\t\tsaveMenu.addEventListener(runtime.flash.events.Event.SELECT, function(){ self.saveConsoleToFile(); });\n\n\t\tvar clipboardMenu = new runtime.flash.display.NativeMenuItem(\'Save console to clipboard\');\n\t\tclipboardMenu.addEventListener(runtime.flash.events.Event.SELECT, function(){ self.saveConsoleToClipboard(); });\n\n\n\t\tvar copyMenu = new runtime.flash.display.NativeMenuItem(\'Copy\');\n\t\tcopyMenu.addEventListener(runtime.flash.events.Event.SELECT, function(){ air.Introspector.runtime.NativeApplication.nativeApplication.copy() });\n\n\n\t\t\n\t\tvar domMenuExpand = new runtime.flash.display.NativeMenuItem(\'Expand All\');\n\t\tdomMenuExpand.addEventListener(runtime.flash.events.Event.SELECT, function(){ self.expandHoverDomElement(); });\t\t\n\t\tthis.domMenuExpand = domMenuExpand;\n\t\t\n\t\tvar domMenuCollapse = new runtime.flash.display.NativeMenuItem(\'Collapse\');\n\t\tdomMenuCollapse.addEventListener(runtime.flash.events.Event.SELECT, function(){ self.collapseHoverDomElement(); });\t\t\t\t\n\t\tthis.domMenuCollapse = domMenuCollapse;\n\t\t\n\t\tvar domMenuAddAttribute = new runtime.flash.display.NativeMenuItem(\'Add attribute\');\n\t\tdomMenuAddAttribute.addEventListener(runtime.flash.events.Event.SELECT, function(){ self.addHoverDomElementAttribute(); });\t\t\n\t\tthis.domMenuAddAttribute = domMenuAddAttribute;\n\t\t\n\t\tvar domMenuRemoveAttribute = new runtime.flash.display.NativeMenuItem(\'Remove attribute\');\n\t\tdomMenuRemoveAttribute.addEventListener(runtime.flash.events.Event.SELECT, function(){ self.removeHoverDomElementAttribute(); });\t\t\n\t\tthis.domMenuRemoveAttribute = domMenuRemoveAttribute;\n\t\t\n\t\t\n\t\t\n\t\tconsoleMenu.addItem(clearMenu);\n\t\tconsoleMenu.addItem(saveMenu);\t\n\t\tconsoleMenu.addItem(clipboardMenu);\t\n\t\tconsoleMenu.addItem(new runtime.flash.display.NativeMenuItem(\'\', true));\n\t\tconsoleMenu.addItem(copyMenu);\t\t\t\n\t\n\t\tdomMenu.addItem(domMenuExpand);\n\t\tdomMenu.addItem(domMenuCollapse);\t\t\n\t\tdomMenu.addItem(domMenuAddAttribute);\t\n\t\tdomMenu.addItem(domMenuRemoveAttribute);\t\t\t\n\t\t\t\n\t\tthis.consoleMenu = consoleMenu;\n\t\tthis.domMenu = domMenu;\n\t}else{\n\t\t\/\/preventing click when menu is visible\n\t\tthis.htmlLoader.window.document.addEventListener(\'click\', function(evt){\n\t\t\t\tif(self.isMenuVisible){\n\t\t\t\t\tvar parent = evt.srcElement;\n\t\t\t\t\twhile(parent){\n\t\t\t\t\t\tif(parent==self.menuDiv)\n\t\t\t\t\t\t\treturn;\n\t\t\t\t\t\tparent = parent.parentNode;\n\t\t\t\t\t}\n\t\t\t\t\tevt.stopPropagation();\n\t\t\t\t\tself.hideMenu();\n\t\t\t\t}\n\t\t\t}, true);\n\t\t\t\n\t\t\n\t\tvar preventClosingListener = function(){\n\t\t\t\tif(this.style.display==\'block\'){\n\t\t\t\t\tif(self.menuTimeout){\n\t\t\t\t\t\tclearTimeout(self.menuTimeout);\n\t\t\t\t\t\tdelete self.menuTimeout;\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t};\n\n\t\tvar hideMenuListener = function(){\n\t\t\t\tif(this.style.visibility==\'visible\'){\n\t\t\t\t\tif(!self.menuTimeout)\n\t\t\t\t\t\tself.menuTimeout = setTimeout(function(){ self.hideMenu(); }, 400);\n\t\t\t\t}\t\n\t\t\t};\n\n\t\tthis.consoleMenuDiv.addEventListener(\'mouseover\', preventClosingListener, true);\n\t\tthis.domMenuDiv.addEventListener(\'mouseover\', preventClosingListener, true);\n\t\t\n\t\tthis.consoleMenuDiv.addEventListener(\'mouseout\',hideMenuListener , true);\n\t\tthis.domMenuDiv.addEventListener(\'mouseout\',hideMenuListener , true);\t\t\n\t}\n\t\n},\t\n\n\n\n\n\nattachUIWindow: function(window){\n\tif(!isAppSandbox){\n\t\tthis.window = window;\n\t\tthis.htmlLoader = { window: window };\n\t\tthis.window.document.title = \'Non Application Sandbox - frame id=\"\'+iframeId+\'\"\';\n\t}else{\n\t\tthis.htmlLoader = window.htmlLoader;\n\t\tthis.nativeWindow = window.nativeWindow;\n\t\tthis.nativeWindow.width = 640;\n\t    this.nativeWindow.height = 480;\n\t\tthis.window = window;\n\t}\n},\n\ndispose: function(){\n\ttry{\n\t\tthis.finishInspect();\n\t\twindow.closed = true;\n\t}catch(e){}    \n},\n\ninit: function(window){\n   var self = this;\n\n   this.isWindowCreated = true;\n   this.attachUIWindow(window);\n\n\n\n   this.initConfigFile();\n\n\n\n\t\n   this.loadUIElements();\n   this.createMenu();\n\n\n\n\t\/\/restore inspecting to initial state\n\tif(this.isInspecting){\n        this.inspectToolLabel.className+=\' selected\';\n    }else{\n        this.finishInspect();\n    }\n\n\n    this.consoleDiv.addEventListener(\'scroll\', function(){self.scrollDisabled = self.consoleDiv.scrollTop != self.consoleDiv.scrollHeight-self.consoleDiv.clientHeight;});\n    \n\tthis.activateTab();\n\tthis.activateCssTab();\n\n    \n    this.isLoaded = true;\n    if(this.logLines.length>0){\n        this.refreshConsole();\n    }\n    \n    if(this.requestLog.length>0){\n        this.refreshNetConsole();\n    }\n    \n\n\tthis.refreshWindows();\n    \n\tif(isAppSandbox){\n        this.nativeWindow.visible = true;\n        if(typeof window.activateDebug!=\'undefined\'&&window.activateDebug==false)\n        {\n            try{\n               this.activeWindow.activate();\n\/\/               this.activeDebugWindow.orderToFront();\n            }catch(e){\n            }\n        }\n      \n\t   air.Introspector.addKeyboardEvents(this.htmlLoader.stage);\n    }\n\n   this.refreshDomPanel();\n\n},\n\nrefreshWindows: function(){\n    if(this.isAppSandbox){\n        this.ownedWindows = air.Introspector.getHtmlWindows();\n        this.windowsDiv.options.length = 0;\n\t\tvar windowsCount = 0;\n\t\tvar activeWindowFound = false;\n\t\tvar firstWindow = null;\n\t\tvar firstHtmlLoader = null;\n        for(var i=this.ownedWindows.length-1;i>=0;i--){\n\t\t\tvar item = this.ownedWindows[i];\n            if(item.nativeWindow==this.nativeWindow) continue;\n\t\t\ttry{\n\t\t\t\tif((item.htmlLoader.window.alert+\'\').indexOf(\"[native code]\")<0) continue;\n\t\t\t}catch(e){ continue; }\n\t\t\t\twindowsCount ++;\n\t\t\tfirstWindow = item.nativeWindow;\n\t\t\tfirtHtmlLoader = item.htmlLoader;\n\t\t\tif(item.nativeWindow==this.activeWindow){\n\t\t\t\tactiveWindowFound = true;\n\t\t\t\tthis.activeHtmlLoader = item.htmlLoader;\n\t\t\t}\n           var selected = this.ownedWindows[i].nativeWindow==this.activeWindow;\n\t\t   var label = item.label? \' - \' + item.label : \'\' ;\n           var option = new Option(item.nativeWindow.title + label, i, selected, selected);\n\t\t\toption.innerHTML += \"&nbsp;\";\n           this.windowsDiv.options.add(option);\n        }\n\n\t\tif(windowsCount==0){\n\t\t\tif(air.Introspector.runtime.NativeApplication.nativeApplication.autoExit\n\t\t\t\t\t&&air.Introspector.config.closeIntrospectorOnExit){\t\t\t\n\t\t\t\tair.Introspector.runtime.NativeApplication.nativeApplication.exit();\n\t\t\t\treturn;\n\t\t\t}\n\t\t}\n\t\tif(!activeWindowFound){\n\t\t\tthis.activeWindow = firstWindow;\n\t\t\tif(this.windowsDiv.options.length>0)\n\t\t\t\tthis.windowsDiv.options[0].selected = true;\n\t\t\tthis.activeHtmlLoader = firstHtmlLoader;\n\t\t}\n        this.refreshDomPanel();\n        this.createAssetsTree();\n        this.createSourcesTree();\n\t}else{\n\t\tthis.windowsDiv.style.display=\'none\';\n\t\tthis.refreshActiveWindowButton.value=\'Refresh\';\n\t\t this.refreshDomPanel();\n\t     this.createAssetsTree();\n\t     this.createSourcesTree();\n\t}\n},\n\n\n   refreshDomPanel: function(){\n       this.makeDomDiv();\n       this.makeHtmlDiv();\n   },\n\n completeWindow: function(htmlLoader){\n        \/\/clear dom related to this htmlLoader\n        var self = this;\n        var activeHtmlLoader = this.getActiveHtmlLoader();\n        if(activeHtmlLoader==htmlLoader){\n            this.createAssetsTree();\n            this.createSourcesTree();\n        }else{\n            this.refreshWindows();\n        }\n    },\n\n  setActiveWindowById: function(id){\n\t\tif(isAppSandbox){\n\t\t\tvar selectedWindow = this.ownedWindows[id];\n \t        this.activeWindow = selectedWindow.nativeWindow;\n\t\t\tvar label = selectedWindow.label? \' - \' + selectedWindow.label : \'\' ;\n\t        this.activeHtmlLoader = selectedWindow.htmlLoader;\n\t        if(this.activeWindow!=null){\n\t            try{\n\t               this.logBuffer(\'\"\'+this.activeWindow.title+label+\'\" window selected\');\n\t            }catch(e){\n\t            }\n\t        }\n\t        this.refreshDomPanel();\n\t        this.createAssetsTree();\n\t        this.createSourcesTree();\n\t\t}\n    },\n\n    setActiveWindowByDocument: function(document){\n\t\tif(isAppSandbox){\n\t        for(var i=this.windowsDiv.options.length-1;i>=0;i--){\n\t        \ttry{\n\t        \t\tvar option = this.windowsDiv.options[i];\n\t\t        \tvar windowId = parseFloat(option.value);\n\t\t        \tif(this.ownedWindows[windowId].htmlLoader.window.document==document){\n\t\t        \t   option.selected = true;\n\t\t        \t   this.setActiveWindowById(windowId);\n\t\t        \t   return;\n\t\t        \t}\n\t        \t}catch(e){\n\t        \t}\n\t        }\n\t\t}\n    },\n\n  getActiveHtmlLoader: function(){\n\t\tif(isAppSandbox){\n\t        return this.activeHtmlLoader;\n\t\t}else{\n\t\t\treturn {window: this.activeWindow };\n\t\t}\n    },\n\n\tsetTab: function(tab){\n\t\tthis.activeTab = tab;\n        if(this.isLoaded){\n\t\t\tthis.activateTab();\n\t\t}\n\t\t\n    },\n\tactivateTab: function(){\n        this.clearTabs();\n\t\t\n\t\tvar tabName = this.tabs[this.activeTab];\n\t\tthis.activeTabDiv = this.htmlLoader.window.document.getElementById(tabName+\'Tab\');\n\t\twindow.SearchBox.attachToNode(this.activeTabDiv);\n\t\tthis.htmlLoader.window.document.getElementById(tabName+\'Label\').className = \'selected\';\n        this.activeTabDiv.className = \'selected\';\n\n\t\tif(this.activeTab==0){\n\t\t\tthis.evalConsoleText.focus();\n\t\t}\n\t},\n    setTool: function(toolName){\n        switch(toolName){\n            case \'inspect\':\n              this.toggleInspect();\n            break;\n        }\n    },\n    clearTabs: function(){\n        var child = this.tabLabels.firstChild;\n        while(child!=null){\n            if(child.nodeType==1)  child.className = child.className.replace(\/selected\/, \'\');\n            child=child.nextSibling;\n        }\n        var child = this.tabPages.firstChild;\n        while(child!=null){\n            if(child.nodeType==1) child.className = child.className.replace(\/selected\/, \'\');\n            child=child.nextSibling;\n        }\n    },\n\n  \n\n\ttimerBounce: function(tag){\n\t\tvar bounceTimer = 1300;\n\t\tvar hideBounce = function(){\n\t\t\tif(tag.className.indexOf(\'bounce\')==-1) return;\n\t\t\ttag.className = tag.className.replace(\/bounceOn\/g, \"bounceOff\");\n\t\t\ttag.innerHTML +=\'\'; \/\/force render\n\t\t\tsetTimeout(showBounce, bounceTimer);\n\t\t};\n\t\tvar showBounce = function(){\n\t\t\tif(tag.className.indexOf(\'bounce\')==-1) return;\n\t\t\ttag.className = tag.className.replace(\/bounceOff\/g, \"bounceOn\");\n\t\t\ttag.innerHTML +=\'\';\t\/\/force render\t\t\n\t\t\tsetTimeout(hideBounce, bounceTimer);\t\t\t\n\t\t};\n\t\tsetTimeout(hideBounce, bounceTimer);\n\t},\n\t\n\tbounceTab: function(tab){\n\t\tif(!air.Introspector.config.flashTabLabels)\n\t\t\treturn;\n\t\t\n\t\tif(this.isLoaded&&this.activeTab!=tab){\n\t\t\ttry{\n\t\t\t\tvar tabName = this.tabs[tab];\n\t\t\t\tvar tag =  this.htmlLoader.window.document.getElementById(tabName+\'Label\');\n\t\t\t\tif(tag.className.indexOf(\'bounce\')==-1){\n\t\t\t\t\ttag.className += \' bounceOn\';\n\t\t\t\t\tthis.timerBounce(tag);\n\t\t\t\t}\n\t\t\t}catch(e){\n\t\t\t\t\/\/wrong tab...\n\t\t\t}\n\t\t}\n\t},\n\t\n\t\n\tloadUrl: function(url, callback){\n    \tvar loader = new air.Introspector.runtime.URLLoader();\n        loader.addEventListener(air.Introspector.runtime.Event.COMPLETE, function(e)\n        {\n        \tcallback(loader.data);\n        });\n        loader.load(new air.Introspector.runtime.URLRequest(activeHtmlLoader.location));\n    },\n    \n    \n    \n    closedWindow: function(htmlLoader){\n\t\tvar self = this;\n\t\tthis.htmlLoader.window.setTimeout(function(){\n        \tself.refreshWindows();\n\t\t}, 0);\n    },\n   \n\thideMenu: function(ev){\n\t\tthis.isMenuVisible=false;\n\t\tthis.consoleMenuDiv.style.display = \'\';\n\t\tthis.domMenuDiv.style.display = \'\';\t\t\n\t\tthis.menuHoverDomElement = null;\t\t\n\t},\n\t\n\tshowMenu: function(ev){\n\t\tif(ev.srcElement&&ev.srcElement.nodeName.toLowerCase()==\"input\") return;\n\t\tif(isAppSandbox){\n\t\t\tvar self = this;\n\t\t\tvar menu =  this.activeTab==0?this.consoleMenu:null;\n\n\t\t\tif(this.hoverDomElement){\n\t\t\t\tthis.menuHoverDomElement = this.hoverDomElement;\n\t\t\t\tthis.menuHoverDomElementAttribute = this.hoverDomElementAttribute;\n\t\t\t\tthis.domMenuRemoveAttribute.enabled = this.hoverDomElementAttribute?true:false;\n\t\t\t\tmenu = this.domMenu;\n\t\t\t}\n\t\t\t\t\t\t\n\t\t\tif(menu){\n\t\t\t\tmenu.display(nativeWindow.stage, ev.pageX, ev.pageY);\n\t\t\t\treturn false;\n\t\t\t}\n\t\t\t\n\t\t}else{\t\n\t\t\t\n\t\t\t\n\t\t\tvar menuDiv = this.activeTab==0?this.consoleMenuDiv:null;\n\t\t\t\n\t\t\tif(this.hoverDomElement){\n\t\t\t\tthis.menuHoverDomElement = this.hoverDomElement;\n\t\t\t\tmenuDiv = this.domMenuDiv;\n\t\t\t}\n\t\t\t\n\t\t\tif(this.hoverDomElementAttribute){\n\t\t\t\tthis.menuHoverDomElementAttribute=this.hoverDomElementAttribute;\n\t\t\t\tthis.domRemoveAttributeMenuDiv.style.display = \'\'; \t\t\t\t\n\t\t\t}else{\n\t\t\t\tthis.domRemoveAttributeMenuDiv.style.display = \'none\'; \n\t\t\t}\n\t\t\t\n\t\t\tif(menuDiv){\n\t\t\t\tthis.isMenuVisible = true;\n\t\t\t\tmenuDiv.style.left=(ev.pageX-4)+\'px\';\n\t\t\t\tmenuDiv.style.top=(ev.pageY-4)+\'px\';\t\t\t\n\t\t\t\tmenuDiv.style.display = \'block\';\n\t\t\t\treturn false;\n\t\t\t}\n\t\t\t\n\t\t}\n\n\t\treturn true;\t\n\t},\n\t\n\t\n\n\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/DebugWindow.ASSETS.js\n\t\t\/\/=======================================================================================================================================================\n\t\tcreateLinksTree: function(document){\n\treturn function(){\n\t\tvar alreadyAdded = {};\n    \tthis.items = [];\n    \tfor(var i=0;i<document.links.length;i++){\n    \t\tif(alreadyAdded[document.links[i].href]) continue;\n    \t\tif(document.links[i].name&&document.links[i].name.length!=0){\n    \t\t     var node = new air.Introspector.tree.node(document.links[i].name, { nodeLabel2: document.links[i].href, openable:false});\n    \t\t}else{\n    \t\t\t var node = new air.Introspector.tree.node(document.links[i].href, { nodeLabel2: \'\', openable:false});\n    \t\t}\n    \t\tthis.items.push(node);\n    \t\talreadyAdded[document.links[i].href]=true;\n    \t}\n\t};\n},\n\ncreateImagesTree: function(document){\n    return function(){\n    \tvar alreadyAdded = {};\n        this.items = [];\n        for(var i=0;i<document.images.length;i++){\n        \tif(alreadyAdded[document.images[i].src]) continue;\n\t\t\tvar title = document.images[i].src;\n\t\t\tif(isAppSandbox){\n\t\t\t\ttry{\n\t\t\t\tvar file = new runtime.flash.filesystem.File(document.images[i].src);\n\t\t\t\tif(file.exists){\n\t\t\t\t\ttitle += \' [size: \'+(Math.round(file.size\/10.24)\/100)+\' KB]\';\n\t\t\t\t}else{\n\t\t\t\t\ttitle += \' [NOT FOUND]\';\n\t\t\t\t}\n\t\t\t\t}catch(e){\n\t\t\t\t\t\/\/ we\'ll be here if the image src is not correct\n\t\t\t\t\t\/\/ just ignore it; \n\t\t\t\t}\n\t\t\t}\n            var node = new air.Introspector.tree.node(title, {\n                src : document.images[i].src,\n                onshow:function(){\n                    if(this.items.length==0){\n                           this.items = [ new air.Introspector.tree.imageDownloadNode(this.src) ];\n                    }       \n                }, onhide: function(){\n                \t\/\/this.items.length = 0;\n\t\t\t\t\tthis.clearItems();\n                }\n            });\n            this.items.push(node);\n            alreadyAdded[document.images[i].src] = true;\n        }\n    };\n},\n\n\ncreateCssTree: function(document){\n    return function(){\n        this.items = [];\n        for(var i=0;i<document.styleSheets.length;i++){\n    \t\tvar node;\n            if(document.styleSheets[i].href){\n            \tnode = new air.Introspector.tree.node (document.styleSheets[i].href, {\n                    src : document.styleSheets[i].href,\n                    onshow:function(){\n                        if(this.items.length==0)\n                              this.items = [ new air.Introspector.tree.textDownloadNode(this.src) ];       \n                    }\n                });\n            }else{\n            \t node = new air.Introspector.tree.node (\'<style>\', {\n                    text: document.styleSheets[i].ownerNode.innerHTML,\n                    onshow:function(){\n                        if(this.items.length==0)\n                            this.items = [ new air.Introspector.tree.textNode(this.text) ];        \n                    }\n                }); \n            }\n            this.items.push(node);\n        }\n    };\n},\n\n\ncreateProjectTree: function(){\n\tif(isAppSandbox){\n\t\tvar self = this;\n    \tvar formats = air.Introspector.formats;\n    \tvar extendTree = function(parentNode, file){\n            this.items = [];\n            var files = file.getDirectoryListing();\n            for(var i=0;i<files.length;i++){\n                var node;\n                if(files[i].isDirectory){\n                     node = new air.Introspector.tree.node (files[i].name  , {\n                        src : files[i],\n                        onshow:function(){\n                            extendTree(this, this.src);\n                        },\n                        onhide:function(){\n                        \t\/\/this.items.length=0;\n\t\t\t\t\t\t\tthis.clearItems();\n                        }\n                    });\n                }else{\n\t\t\t\t\t\tvar title = files[i].name;\n\t\t\t\t\t\tvar format = formats[files[i].extension];\n\n\t\t\t\t\t\tif(isAppSandbox && format==1){\n\t\t\t\t\t\t\tvar file = files[i];\n\t\t\t\t\t\t\tif(file.exists){\n\t\t\t\t\t\t\t\ttitle += \' [size: \'+(Math.round(file.size\/10.24)\/100)+\' KB]\';\n\t\t\t\t\t\t\t}else{\n\t\t\t\t\t\t\t\ttitle += \' [NOT FOUND]\';\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}\t\t\t\t\t\t\n\t\t\t\t\t\t\n                \t    node = new air.Introspector.tree.node (title , {\n\t                        src : \'file:\/\/\/\'+files[i].nativePath,\n\t\t\t\t\t\t\tformat: formats[files[i].extension], \n\t                        onshow:function(){\n\t                            if(this.items.length==0){\n\t                            \tif(this.format==0){\n\t                                  this.items = [ new air.Introspector.tree.textDownloadNode(this.src) ];\n\t                            \t}else if(this.format==1){\n\t                            \t   this.items = [ new air.Introspector.tree.imageDownloadNode(this.src) ];\n\t                            \t}else if(this.format==2){\n\t\t\t\t\t\t\t\t\t\tif(this.items.length==0){\n\t\t\t\t\t\t\t\t\t\t\tvar selfNode = this;\n\t\t\t\t\t\t\t\t\t\t\tif(isAppSandbox){ \/\/can not be something else\n\t\t\t\t\t\t\t\t\t        \tvar scriptLoader = new air.Introspector.runtime.URLLoader();\n\t\t\t\t\t\t\t\t\t\t        scriptLoader.addEventListener(air.Introspector.runtime.Event.COMPLETE, function(e){\n\t\t\t\t\t\t\t\t\t\t\t\t\t\tvar domParser = new DOMParser();\n\t\t\t\t\t\t\t\t\t\t\t\t\t\ttry{\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tvar xmlData = domParser.parseFromString(scriptLoader.data, \"text\/xml\");\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tself.extendDom2TreeNode(selfNode, xmlData, true);\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t}catch(e){\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\ttry{\n\t\t\t\t\t                                  \t\t\tselfNode.items = [ new air.Introspector.tree.textNode(scriptLoader.data) ];\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t}catch(err){\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tselfNode.items = [ new air.Introspector.tree.textNode(err+\'\') ];\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\t\t\t\t\t\t\tselfNode.show();\n\t\t\t\t\t\t\t\t\t\t\t\t\t\tscriptLoader = null;\n\t\t\t\t\t\t\t\t\t\t\t        });\n\t\t\t\t\t\t\t\t\t\t\t\tscriptLoader.addEventListener(air.Introspector.runtime.IOErrorEvent.IO_ERROR, function(e){\n\t\t\t\t\t\t\t\t\t\t\t\t\tselfNode.items = [ new air.Introspector.tree.textNode(\"error loading file \"+e) ];\n\t\t\t\t\t\t\t\t\t\t\t\t\tselfNode.show();\n\t\t\t\t\t\t\t\t\t\t\t\t\tair.Introspector.logError(e);\n\t\t\t\t\t\t\t\t\t\t\t\t});\n\t\t\t\t\t\t\t\t\t\t\t\tscriptLoader.load(new air.Introspector.runtime.URLRequest(this.src));\n\t\t\t\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\t\t\t\tselfNode.items = [ new air.Introspector.tree.textNode(\"Loading...\") ];\n\t\t\t\t\t\t\t\t\t\t}\n\t                            \t}\n\t                            }       \n\t                        }, onhide: function(){ this.clearItems(); }\n\t                    });\n                }\n                parentNode.items.push(node);\n            }\n        };\n    \n        node = new air.Introspector.tree.node (\'Application files\'  , {\n                        src : runtime.flash.filesystem.File.applicationDirectory,\n                        onshow:function(){\n                            extendTree(this, this.src);\n                        },\n                        onhide:function(){\n                            \/\/this.items.length=0;\n\t\t\t\t\t\t\tthis.clearItems();\n                        }\n                    });\n        return node;\n\t}else{\n\t\treturn null;\n\t}\n},\n    \n\ncreateScriptsTree: function(document){\n    return  function(){\n        this.items = [];\n        for(var i=0;i<document.scripts.length;i++){\n\t\t\tvar node;\n        \tif(document.scripts[i].src){\n        \t\tnode = new air.Introspector.tree.node (document.scripts[i].src, {\n        \t\t\tsrc : document.scripts[i].src,\n        \t\t\tonshow:function(){\n\t    \t\t\t\tif(this.items.length==0){\t\n\t\t\t\t\t\t\tif(this.src.length>=3&&this.src.substr(this.src.length-3).toLowerCase()==\'swf\'){\n\t\t\t\t\t\t\t\tthis.items = [ new air.Introspector.tree.textNode(\"SWF file\") ];        \n\t\t\t\t\t\t\t}else{\n\t\t\t\t\t\t\t\tthis.items = [ new air.Introspector.tree.textDownloadNode(this.src) ];\t\t\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}\n        \t\t\t}\n        \t\t});\n        \t}else{\n                node = new air.Introspector.tree.node (\'<script>\', {\n                \ttext: document.scripts[i].innerText,\n                \tonshow:function(){\n                \t\tif(this.items.length==0)\n                            this.items = [ new air.Introspector.tree.textNode(this.text) ];        \n                    }\n                });\t\n        \t}\n            this.items.push(node);       \n        }\n    };\n    \n},    \n\ncreateAssetsTree: function(){\n    this.assetsTabDiv.innerHTML = \'\';\n\tif(this.assetsTabDivNode){\n\t\tthis.assetsTabDivNode.dispose();\n\t\tthis.assetsTabDivNode = null;\n\t}\n    var activeHtmlLoader = this.getActiveHtmlLoader();\n    if(activeHtmlLoader!=null){\n        var document = activeHtmlLoader.window.document;\n\t\tif(!document)return;\n        var self = this;\n        var node = new air.Introspector.tree.node(\"Assets\",{\n            onshow: function(){\n                  this.items = [\n                      new air.Introspector.tree.node(\"Links\",{openable:document.links.length!=0,  onshow:  self.createLinksTree(document) } ),\n                      new air.Introspector.tree.node(\"Images\",{openable:document.images.length!=0, onshow: self.createImagesTree(document) } ),\n                      new air.Introspector.tree.node(\"CSS (\"+document.styleSheets.length+\')\',{openable:document.styleSheets.length!=0, onshow:  self.createCssTree(document) } ),\n                      new air.Introspector.tree.node(\"JS (\"+document.scripts.length+\')\',{openable:document.scripts.length!=0, onshow:  self.createScriptsTree(document) } ),\n                  ];\n            },\n            onhide: function(){\n            \t\/\/this.items.length = 0;\n\t\t\t\tthis.clearItems();\n            }\n        });\n        node.shouldOpen();\n\t\tthis.assetsTabDivNode = node;\n        this.assetsTabDiv.appendChild(node.createDiv(this.htmlLoader.window.document));\n    }\n\n},\n\ncreateSourcesTree: function(){\n    this.sourcesTabDiv.innerHTML = \'\';\n\n\tif(this.sourceTabDivNodes){\n\t\tfor(var i=this.sourceTabDivNodes.length-1;i>=0;i--){\n\t\t\tif(this.sourceTabDivNodes[i]){\n\t\t\t\tthis.sourceTabDivNodes[i].dispose();\n\t\t\t\tthis.sourceTabDivNodes[i] = null;\n\t\t\t}\n\t\t}\n\t\tthis.sourceTabDivNodes = null;\n\t}\n\t\n\tthis.sourceTabDivNodes = [];\n    var activeHtmlLoader = this.getActiveHtmlLoader();\n    if(activeHtmlLoader!=null){\n        var document = activeHtmlLoader.window.document;\n        var self = this;\n        var node = new air.Introspector.tree.node(\"Actual source\",{\n        \tsrc: activeHtmlLoader.window.location, \n            onshow: function(){\n                this.items = [ new air.Introspector.tree.textDownloadNode(this.src) ]; \n            },\n            onhide: function(){\n                \/\/this.items.length = 0;\n\t\t\t\tthis.clearItems();\n            }\n        });\n\t\tthis.sourceTabDivNodes.push(node);\n        this.sourcesTabDiv.appendChild(node.createDiv(this.htmlLoader.window.document));\n        \n        var node = new air.Introspector.tree.node(\"Parsed source\",{\n        \t\/\/text: ,\n            onshow: function(){\n                var parsedSource = [];\n                var node = document.firstChild;\n                while(node) {\n                    var nodeSource = node.outerHTML;\n                    if (typeof nodeSource != \'undefined\') {\n                        parsedSource.push(nodeSource);\n                    }\n                    node = node.nextSibling;\n                }\n                this.items = [ new air.Introspector.tree.textNode(parsedSource.join(\"\\n\")) ];\n            },\n            onhide: function(){\n                \/\/this.items.length = 0;\n\t\t\t\tthis.clearItems();\n            }\n        });\n\t\tthis.sourceTabDivNodes.push(node);\n        this.sourcesTabDiv.appendChild(node.createDiv(this.htmlLoader.window.document));\n\n\t\tif(isAppSandbox){\n            var node = this.createProjectTree();\n\t\t\tthis.sourceTabDivNodes.push(node);\n            this.sourcesTabDiv.appendChild(node.createDiv(this.htmlLoader.window.document));\n\t\t}\n    }\n},\n\n\n\n\n\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/DebugWindow.CONSOLE.js\n\t\t\/\/=======================================================================================================================================================\n\t\t\n\n\n\n\n extendTreeNode: function(node, obj){\n        try{\n            var self = this;\n            var items = [];\n            if(typeof obj==\'undefined\'||obj==null)\n               return;\n            if(air.Introspector.isXMLObject(obj)){\n            \tthis.extendDom2TreeNode(node, obj);\n            \treturn;\n            }  \n            var isItemNative = air.Introspector.isItemNative(obj);\n            var parseArray = air.Introspector.isArrayObject(obj)||air.Introspector.isArgumentsObject(obj)||isItemNative;\n            var parseHash =  !parseArray || isItemNative;\n            if (parseArray){\n\t\t\t\tvar l = obj.length;\n            \tfor(var i=0;i<l;i++){\n                    var value;\n                    try{\n                        value = obj[i];\n                    }catch(e){\n                        value = e;\n                    }\n                    items.push(this.createJsTreeNode(i, value, this.createGetter(obj, i), this.createSetter(obj, i)));             \n                }\n            } \n            if(parseHash) {\n                for(var i in obj){\n                    var value;\n                    try{\n                        value = obj[i];\n                    }catch(e){\n                        value = e;\n                    }\n                    items.push(this.createJsTreeNode(i, value, this.createGetter(obj, i), this.createSetter(obj, i)));\n                }\n            }\n\n\t\t\tif(air.Introspector.config.debugRuntimeObjects && !parseArray){\n\t\t\t\ttry{\n\t\t\t\t\tvar typeDescription = runtime.flash.utils.describeType(obj);\n\t\t\t\t\tif(!this.domParser) this.domParser = new DOMParser();\n\t\t\t\t\tvar typeXml = this.domParser.parseFromString(typeDescription, \"text\/xml\");\n\t\t\t\t\tvar child = typeXml.firstChild.firstChild;\n\t\t\t\t\twhile(child){\n\t\t\t\t\t\tif(child.nodeName==\'accessor\'||child.nodeName==\'constant\'||child.nodeName==\'method\'||child.nodeName==\'variable\'){\n\t\t\t\t\t\t\tvar name = child.getAttribute(\'name\');\n\t\t\t\t\t\t\tif(name!=null && name!=\'prototype\'){\n\t\t\t                    try{\n\t\t\t\t\t\t\t\t\titems.push(this.createJsTreeNode(name, obj[name], this.createGetter(obj, name), this.createSetter(obj, name)));\t\t\n\t\t\t                    }catch(e){\n\t\t\t\t\t\t\t\t\titems.push(this.createJsTreeNode(name, \'\', this.createGetter(obj, name), this.createSetter(obj, name), e));\n\t\t\t                    }\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}\n\t\t\t\t\t\tchild = child.nextSibling;\n\t\t\t\t\t}\n\t\t\t\t}catch(e){\n\t\t\t\t\t\/\/just hide the error\n\t\t\t\t}\n            }\n            items.sort(function(node1, node2){\n            \tvar isNode1Number = parseInt(node1.nodeLabel)==node1.nodeLabel;\n            \tvar isNode2Number = parseInt(node2.nodeLabel)==node2.nodeLabel;\n            \tif(isNode1Number&&isNode2Number){\n            \t\treturn parseInt(node1.nodeLabel)-parseInt(node2.nodeLabel);\n            \t}\n            \tif(isNode1Number){\n            \t\treturn -1;\n            \t}\n            \tif(isNode2Number){\n            \t\treturn 1;\n            \t}\n            \tif(node1.nodeLabel.toLowerCase()==node2.nodeLabel.toLowerCase())\n                   return 0;\n                if(node1.nodeLabel.toLowerCase()<node2.nodeLabel.toLowerCase())\n                   return -1;\n                return 1;\n            });\n        }catch(e){\n            this.logError(e);\n        }\n\t\tnode.items = items;\n    },\n\n    createGetter: function(obj, i){\n        return function(){ \n           try{\n               return obj[i];\n           }catch(e){}\n        }\n    },\n\n    createSetter: function(obj, i){\n        return function(value){ \n           try{\n               obj[i] = value;\n           }catch(e){}\n        }\n    },\n\n\n\n\n\tcreateJsTreeNode : function(stringValue, value, getter, setter, error){\n      var self = this;\n\n      var config = {\n            editable:false,\n              onshow: function(sender){\n\t\t\t\t\tself.extendTreeNode(sender, value);\n              }, \n              onhide: function(){\n\t\t\t\t\tthis.clearItems();\n\/\/                    this.items.length = 0;\n              },\n              onclick: function(){\n              \tif(typeof getter!=\'undefined\'){\n              \t   self.clicked = getter();\n              \t}\n              },\n              getEditValue: function(){\n                  if(typeof getter!=\'undefined\')\n                  {\n                      switch(this.valueType){\n                          case 0:\n                             return getter();\n                          case 1:\n                             return getter().replace(\/\\n\/g, \"\\\\n\");\n                      }\n                  }\n              },\n              setEditValue: function(value)\n              {\n                  if(typeof setter!=\'undefined\'){\n                      switch(this.valueType){\n                          case 0:\n                          var lowerCaseValue = value.toLowerCase();\n                          if(lowerCaseValue==\'true\')\n                            setter(true);\n                          else if(lowerCaseValue==\'false\')\n                            setter(false);\n                          else\n                            setter(parseFloat(value));\n                          break;\n                          case 1:\n                           setter(value.replace(\/\\\\n\/g, \"\\n\"));\n                          break;\n                   }\n                   setTimeout(function(self){\n                      if(typeof getter!=\'undefined\'){\n                              var value = getter();\n                              if(typeof value!=\'undefined\'&&value!=null){\n                                switch(self.valueType){\n                                  case 0:\n                                   self.nodeLabel2 = value;\n                                   break;\n                                  case 1:\n                                   var newValue = value.replace(\/\\n\/g, \"\\\\n\");\n                                   if(newValue.length == 0) newValue = \'[empty string - click to edit]\';\n                                       else newValue = \'\"\'+newValue +\'\"\';\n                                   self.nodeLabel2 = newValue;\n                                   break;\n                                }\n                                self.refreshLabel();\n                              }\n                         }\n                     },0,this);\n                  }\n              }\n          };\n\n      var openable = false;\n      var value2 = \'\';\n\n      if(typeof value==\'undefined\'){\n         openable = false;\n\t\tvalue2 = \'[undefined]\';\n      }else if(value==null){\n          openable = false;\n\t\tvalue2 = \'[null]\';\n      }else{\n\t\t\n\t\t\n          openable = false;\n          for(var i in value){\n              openable=true;\n              break;\n          }\n\t\tvar hasASValues = false;\n\t\tvar isItemNative = air.Introspector.isItemNative(value);\n        var parseArray = air.Introspector.isArrayObject(value)||air.Introspector.isArgumentsObject(value)||isItemNative;\n\t\tif(air.Introspector.config.debugRuntimeObjects && !parseArray){\n\t\t\ttry{\n\t\t\t\tvar typeDescription = runtime.flash.utils.describeType(value);\n\t\t\t\tif(!this.domParser) this.domParser = new DOMParser();\n\t\t\t\tvar typeXml = this.domParser.parseFromString(typeDescription, \"text\/xml\");\n\t\t\t\tvar child = typeXml.firstChild.firstChild;\n\t\t\t\twhile(child){\n\t\t\t\t\tif(child.nodeName==\'accessor\'||child.nodeName==\'method\'||child.nodeName==\'constant\'||child.nodeName==\'variable\'){\n\t\t\t\t\t\tif(child.getAttribute(\'declaredBy\') != \'flash.html::__HTMLScriptFunction\'){\n\t\t\t\t\t\t\topenable = true;\n\t\t\t\t\t\t\thasASValues = true;\t\n\t\t\t\t\t\t}\t\t\t\t\t\t\n\t\t\t\t\t\tbreak;\n\t\t\t\t\t}\n\t\t\t\t\tchild=child.nextSibling;\n\t\t\t\t}\n\t\t\t}catch(e){\n\t\t\t\t\/\/hide the error\n\t\t\t}\n\t\t}\n\tif(typeof error!=\'undefined\'){\n\t\t\n\t}else  if(air.Introspector.isNumberObject(value)){\n              value2 = value;\n              if(typeof setter!=\'undefined\'){\n                  config.editable = true;\n                  config.valueType = 0;\n              }\n              openable = false;\n          }else if(air.Introspector.isStringObject(value)){\n               value2 = value.replace(\/\\n\/g, \"\\\\n\");\n               if(value2.length == 0) value2 = \'[empty string - click to edit]\';\n                   else value2 = \'\"\'+value2 +\'\"\';\n\n              if(typeof setter!=\'undefined\'){\n                  config.editable = true;\n                  config.valueType = 1; \n              }\n              openable = false;\n          }else if(air.Introspector.isArgumentsObject(value)){\n              value2 = \'[Arguments \'+value.length+\']\';\n              openable = true;\n          }else if(air.Introspector.isArrayObject(value)||(air.Introspector.isItemNative(value)&&typeof value.length!=\'undefined\')){\n              value2 = \'[Array \'+value.length+\']\';\n              openable = true;\n          }else if(typeof value==\'function\' && !hasASValues ){\n\t\t\t\topenable = false;\n\t\t\t\tvalue2 = \"[function]\";\n\t\t  }else{\n              try{\n                 value2 = value+\'\';\n              }catch(e){\n                 value2 = e+\'\';\n              }\n          }\n      }\n\n      config.nodeLabel2 = value2;\n      config.openable = openable; \n\n      if(air.Introspector.isStringObject(stringValue)&&stringValue.length==0){\n      \tstringValue = value2;\n      \tconfig.nodeLabel2 = \'\';\n      }\n      return new air.Introspector.tree.node(stringValue,config);\n  },\n\n\n\t    logArguments: function(args, config){\n\t        if(typeof config==\'undefined\') config = {};\n\t        config.buffer=\'\';\n\t        if(args.length==1)\n\t           this.logObject(args[0], config);\n\t        else\n\t           this.logObject(args, config);\n\t    },\n\t\n\t\n\t logObject: function(obj, config){\n\t        if(typeof config==\'undefined\') config = {};\n\t        if(this.isLoaded){\n\t       \t\tif(typeof obj != \'undefined\')\n\t\t\t\t{\n\t        \t\tvar node = this.createJsTreeNode(\'\', obj);\n\t        \t\tvar addElement = true;\n\t        \t\tif(node.openable&&config.buffer==\'\'){\n\t        \t\t\ttry{\n\t\t\t\t\t\t\tif(air.Introspector.isArrayObject(obj)||air.Introspector.isArgumentsObject(obj)){\n\t\t\t\t\t\t\t\tvar l = obj.length;\n\t\t\t\t\t\t\t\tvar str = \'\';\n\t\t\t\t\t\t\t\tfor(var i=0;i<l;i++){\n\t\t\t\t\t\t\t\t\ttry{\n\t\t\t\t\t\t\t\t\t\tif(typeof obj[i]==\'undefined\')\n\t                            \t\t\tstr+= \'[undefined], \';\t\t\t\n\t\t\t\t\t\t\t\t\t\telse if(air.Introspector.isArrayObject(obj[i]))\n\t\t\t\t\t\t\t\t\t\t\tstr+=\'[Array \'+obj[i].length+\'], \';\n\t\t\t\t\t\t\t\t\t\telse if(air.Introspector.isArgumentsObject(obj[i]))\n\t\t\t\t\t\t\t\t\t\t\tstr+=\'[Arguments \'+obj[i].length+\'], \';\n\t\t\t\t\t\t\t\t\t\telse\n\t                            \t\t\tstr+= obj[i]+\', \';\t\t\t\t\t\t\t\t\t\n\t\t\t\t\t\t\t\t\t}catch(e){\n\t\t\t\t\t\t\t\t\t\tstr+=\'[\'+e+\'], \';\n\t\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\tconfig.buffer = str;\n\t\t\t\t\t\t\t}\n\t\t                }catch(e){\n\t\t                }\n\t        \t\t}\n\t        \t\tif(typeof config.buffer==\'undefined\'||config.buffer==\'\'){\n\t        \t\t\ttry{\n\t        \t           config.buffer = obj+\'\';\n\t        \t           addElement = node.openable;\n\t        \t\t\t}catch(e){\n\t        \t\t\t\tconfig.buffer = e;\n\t        \t\t\t}\n\t        \t\t}\n\t        \t\tif(addElement)\n\t        \t\t  config.element = node.createDiv(this.htmlLoader.window.document);\t\n\t        \t}else if(typeof config.fromConsole==\'undefined\'||config.fromConsole==false){\n\t\t\t\t\tvar node = this.createJsTreeNode(\'[undefined]\');\n\t\t\t\t\tconfig.element = node.createDiv(this.htmlLoader.window.document);\t\n\t\t\t\t}\n\t            this.logBuffer(config.buffer, config);\n\t\t\t\tthis.bounceTab(0);\n\t        }else{\n\t           air.Introspector.extend(config, {obj: obj, isObject:true, doNotLog:true});\n\t           this.logLines.push(config);\n\t        }\n\t    },\n\n\t    showLogItem: function(logItem){\n\t        if(this.isLoaded){\n\t            var listItem = this.consoleDiv.ownerDocument.createElement(\'li\');\n\t            var listItemText = this.consoleDiv.ownerDocument.createElement(\'div\');\n\t\t\t\tif(logItem.usePre)\n\t\t\t\t{\n\t\t\t\t\tvar pre = \"<pre>\"+air.Introspector.escapeHtml(logItem.buffer).replace(\/\\n\/g, \'<\/pre><pre>\') + \"<\/pre>\";\n\t            \tlistItemText.innerHTML = logItem.specialBuffer + pre;\n\t\t\t\t}else\n\t            \tlistItemText.innerHTML = logItem.specialBuffer + air.Introspector.escapeHtml(logItem.buffer);\n\t            listItemText.className = \'\tconsoleItemText\';\n\t            listItem.appendChild(listItemText);\n\t            this.consoleDiv.appendChild(listItem);\n\t            if(typeof logItem.element!=\'undefined\'){\n\t                listItem.appendChild(logItem.element);\n\t            }\n\t            if(typeof logItem.type!=\'undefined\'){\n\t                listItem.className += \' \'+logItem.type;\n\t                var listItemType = this.consoleDiv.ownerDocument.createElement(\'div\');\n\t                listItemType.className = \'typeBox\';\n\t                switch(logItem.type){\n\t                    case \'warn\':\n\t                       listItemType.innerHTML = \'!\';\n\t                      break;\n\t                    case \'info\':\n\t                       listItemType.innerHTML = \'i\';\n\t                      break;\n\t                    case \'error\':\n\t                       listItemType.innerHTML = \'x\';\n\t                      break;\n\t                }\n\t                listItem.appendChild(listItemType);\n\t            }\n\t            this.scrollConsole();\n\t\t\t\tlogItem.displayObject = listItem;\n\t        }\n\t    },\n\t    logBuffer: function(buffer, config){\n\t        if(typeof config==\'undefined\') config = {};\n\t        if(typeof buffer==\'undefined\') buffer = \'\';\n\t        var specialBuffer = \'\';\n\t\t\tvar textSpecialBuffer = \'\';\n\t\t\t\n\t        if(air.Introspector.config.showTimestamp){\n\t           var date = typeof config.timestamp==\'undefined\'?new Date():config.timestamp;\n\t\t\t\tvar dateStr = air.Introspector.twoDigits(date.getHours())+\':\'+air.Introspector.twoDigits(date.getMinutes())+\':\'+air.Introspector.twoDigits(date.getSeconds());\n\t           specialBuffer=\'<span class=\"consoleTimestamp\">[\'+dateStr+\']<\/span> \'+specialBuffer;\n\t\t\t\ttextSpecialBuffer=  \'[\'+dateStr+\']\'+textSpecialBuffer;\n\t        }\n\t        if(air.Introspector.config.showSender&&config.htmlLoader){\n\t            try{\n\t\t\t\t  var locationStr = config.htmlLoader.window.location;\n\t              specialBuffer=\'<span class=\"consoleFrom\">[\'+locationStr+\']<\/span>\'+specialBuffer;\n\t\t\t\t  textSpecialBuffer=  \'[\'+locationStr+\']\'+textSpecialBuffer;\n\t            }catch(e){\n\t              \/\/no htmlLoader provided - use current window\n\t              buffer=\'[Debugger]\'+buffer;\n\t            }   \n\t        }\n\t        var logItem = {buffer: buffer, specialBuffer:specialBuffer, textSpecialBuffer:textSpecialBuffer};\n\t        delete config.buffer;\n\t        air.Introspector.extend(logItem, config);\n\t        this.showLogItem(logItem);\n\t        if(typeof config.doNotLog==\'undefined\'){\n\t            this.logLines.push(logItem);\n\t        }\n\t    },\n\t    scrollConsole:function(){\n\t    \tif(!this.scrollDisabled)\n\t        {\n\t        \tvar self = this;\n\t            setTimeout(function(){ self.consoleDiv.scrollTop = self.consoleDiv.scrollHeight; self.scrollDisabled = false; }, 0);\n\t        }\n\t    },\n\t    refreshConsole: function(){\n\t        this.consoleDiv.innerHTML = \'\';\n\t        for(var i=0;i<this.logLines.length;i++){\n\t            if(typeof this.logLines[i].isObject!=\'undefined\'){\n\t                \/\/air.Introspector.is an Object log\n\t                this.logObject(this.logLines[i].obj, this.logLines[i] );\n\t            }else{\n\t                this.showLogItem(this.logLines[i]);\n\t            }\n\t        }\n\t        this.scrollConsole();\n\t    },\n\t\n\t\n\t aireval : function(src, realWindow, result, error){\n\t        var self = this;\n\t\t\tvar myAir = {\n\t                Introspector: { \n\t                   loadedEval: src, \n\t                   mainWindow: realWindow,\n\t                   loaded: function(returnValue){\n\t                       if(typeof result==\'function\'){\n\t                           result.call(self, returnValue);\n\t                       }\n\t                   },\n\t                   error : function(returnValue){\n\t                       if(typeof error==\'function\'){\n\t                           error.call(self, returnValue);\n\t                       }\n\t                   },  \n\t                }\n\t           };\n\n\t\t\tif(isAppSandbox){\n\t               var htmlLoader = new  air.Introspector.runtime.HTMLLoader();\n\t               htmlLoader.addEventListener(air.Introspector.runtime.Event.HTML_DOM_INITIALIZE, function(){\n\t               \t   htmlLoader.window.clicked = self.clicked;\n\/*\t\t\t\t\t   htmlLoader.window.toXML = function(xml){\n\t\t\t\t\t\t\tvar domParser = new DOMParser();\n\t\t\t\t\t\t\treturn domParser.parseFromString(xml, \"text\/xml\");\n\t\t\t\t\t   };*\/\n\t                   htmlLoader.window.air = myAir;\n\t               });\n                   \n\t               if ( typeof htmlLoader.placeLoadStringContentInApplicationSandbox != \'undefined\' ) {\n\t\t              \/\/since AIR1.5 the htmlLoader will not allow string load in app sandbox\n\t\t              \/\/don\'t need to switch this back to false, because the htmlLoader will die after this call\n\t\t              htmlLoader.placeLoadStringContentInApplicationSandbox = true;\n\t               }\n                   \n\t               htmlLoader.loadString(\"<script>var result; var evalSource = air.Introspector.loadedEval; try { result = eval(\'with(air.Introspector.mainWindow){\'+ evalSource +\'}\'); air.Introspector.loaded(result); }catch(e){air.Introspector.error(e)}<\/\"+\"script>\");\n\t\t\t}else{\n\t\t\t\t\/\/we just don\'t need all that stuff - real eval is there (we actually run this peace of code in the same sandbox)\n\t\t\t\tvar resultvalue; \n\t\t\t\tvar evalSource = myAir.Introspector.loadedEval; \n\t\t\t\ttry {\n\t\t\t\t\twith(realWindow){ \n\t\t\t\t\t\tresultvalue=realWindow.eval(evalSource); \n\t\t\t\t\t};\n\t\t\t\t\tmyAir.Introspector.loaded(resultvalue); \n\t\t\t\t}catch(e){  \n\t\t\t\t\tmyAir.Introspector.error(e); \n\t\t\t\t}\n\t\t\t}\n\t    },\n\t\n\t    historyUserInput: function(step){\n\t        if(typeof step!=\'undefined\'){\n\t            if(this.evalHistoryPos==this.evalHistory.length){\n\t                this.evalHistorySaved = this.evalConsoleText.value;\n\t            }\n\t            var newEvalHistoryPos = this.evalHistoryPos + step;\n\t            if(newEvalHistoryPos>=0){\n\t                if(newEvalHistoryPos<this.evalHistory.length){\n\t                  this.evalConsoleText.value = this.evalHistory[newEvalHistoryPos];\n\t                }else if(newEvalHistoryPos==this.evalHistory.length){\n\t                  this.evalConsoleText.value = this.evalHistorySaved;\n\t                }else{\n\t                    return;\n\t                }\n\t                this.evalHistoryPos = newEvalHistoryPos;\n\t                this.evalConsoleText.selectionStart = this.evalConsoleText.value.length;\n\t            }\n\t        }else if(this.evalHistoryPos<this.evalHistory.length&&this.evalConsoleText.value!=this.evalHistory[this.evalHistoryPos]){\n\t            this.evalHistoryPos=this.evalHistory.length;\n\t            this.evalHistorySaved = this.evalConsoleText.value;\n\t        }\n\t    },\n\n\t    evalUserInput: function(){\n\t        var htmlLoader = this.getActiveHtmlLoader();\n\t        if(htmlLoader==null){\n\t            this.logBuffer(null, \'No active window.\');\n\t            return;\n\t        }\n\t        try{\n\t            var userEval = this.evalConsoleText.value;\n\n\t            if(air.Introspector.trim(userEval).length==0)\n\t                return;\n\n\t\t\t\tthis.scrollDisabled = false;\n\n\t            this.evalHistory.push(userEval);\n\t            this.evalHistoryPos = this.evalHistory.length; \n\t\n\t \t\t\tthis.saveConfig(true);\n\n\t            this.aireval(userEval, htmlLoader.window, function(result){\n\t                \/\/if(typeof result!=\'undefined\'){\n\t\t                    this.logObject(result, {htmlLoader:htmlLoader, buffer: \'>>>\'+userEval, fromConsole:true });\n\t                \/\/}\n\t            }, function(error){\n\t                this.logError(error, {htmlLoader:htmlLoader, buffer: \'>>>\'+userEval });\n\t            });\n\n\t            this.evalConsoleText.value = \'\';\n\t        }catch(e){\n\t            this.logError(e, {htmlLoader:htmlLoader, buffer: \'>>>eval console\' });\n\t        }\n\t    },\n\n\t    logError: function(error, config){\n\t        if(typeof config==\'undefined\')\n\t           config = {};\n\t        config.type = \'error\';\n\t        this.logObject(error, config);\n\t    },\n\n\t\tgotoConsoleTab: function(){\n\t    \tthis.setTab(0);\n\t    },\n\n\n\t\tclearConsole: function(){\n\t\t\tthis.logLines = [];\n\t\t\tthis.refreshConsole();\n\t\t},\n\t\t\n\t\t\n\t\t\t\t\n\t\n\tsaveConsole: function(){\n\t\tvar str = \'\';\n\t    for(var i=0;i<this.logLines.length;i++){\n\t\t\tvar logItem = this.logLines[i];\n\t\t\tif(typeof logItem.type!=\'undefined\'){\n\t                switch(logItem.type){\n\t                    case \'warn\':\n\t                       str += \'[warn ]\';\n\t                      break;\n\t                    case \'info\':\n\t                       str += \'[info ]\';\n\t                      break;\n\t                    case \'error\':\n\t                       str += \'[error]\';\n\t                      break;\n\t                }\n\t            }\n\t\t   str+=logItem.textSpecialBuffer+\': \';\n           str+=logItem.buffer+\'\\r\\n\';\n        }\t\n\t\treturn str;\n\t},\n\n\tsaveConsoleToFile: function(){\n\t\ttry{\n\t\t\tair.Introspector.writeConsoleToFile(this.saveConsole());\n\t\t}catch(e){this.logError(e);}\n\t},\n\tsaveConsoleToClipboard: function(){\n\t\ttry{\n\t\t\tair.Introspector.writeConsoleToClipboard(this.saveConsole());\n\t\t}catch(e){this.logError(e);}\t\t\n\t},\n\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/DebugWindow.HTML.js\n\t\t\/\/=======================================================================================================================================================\n\t\tmakeHtmlDiv: function(){\n\t\tif(!this.isLoaded) return;\n       try{\n         var htmlLoader = this.getActiveHtmlLoader();\n         if(htmlLoader!=null){\n\t          \/*this.htmlDiv.innerHTML = \'\';\n\t          var domTreeDiv = this.createDomTreeNode(htmlLoader.window.document).createDiv(this.htmlLoader.window.document);\n\t          this.htmlDiv.appendChild(domTreeDiv);*\/\n\t          this.html2Div.innerHTML = \'\';\n\t\t\t\tif(this.html2Node){\n\t\t\t\t\tthis.html2Node.dispose();\n\t\t\t\t\tthis.html2Node = null;\n\t\t\t\t}\n\n\t          if(htmlLoader.window.document&&htmlLoader.window.document.firstChild){\n\t\t\t\t    var node = this.createDom2TreeNode(htmlLoader.window.document);\n\t                var domTreeDiv = node.createDiv(this.htmlLoader.window.document);\n\t\t\t\t\tthis.html2Node = node;\n\t          \t\tthis.html2Div.appendChild(domTreeDiv);\t\t\t\t\t\n\t          }\n\n         }\n       }catch(e){\n       \tair.Introspector.Console.log(e);\n       }\n   },\n\n\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/DebugWindow.XHR.js\n\t\t\/\/=======================================================================================================================================================\n\t\t\n    \n    logNet: function(xhr, method, url, asyncFlag){\n    \tvar requestLog = this.requestLog;\n    \tfor(var i=requestLog.length-1;i>=0;i--){\n            if(requestLog[i].xhr == xhr){\n                this.refreshRequestObject(requestLog[i]);\n                return;\t\n            } \t\t\n    \t}\n    \tvar request = {\n    \t\txhr: xhr, \n    \t\tmethod: method, \n    \t\turl: url,\n\t\t\tasync: asyncFlag,\n    \t\telement: null\n    \t};\n    \tif(this.isLoaded){\n    \t   this.viewRequestObject(request);\n    \t}\n    \trequestLog.push(request);\n\t\tthis.bounceTab(5);\n    },\n\n\tlogNetSend: function(xhr, obj){\n    \tvar requestLog = this.requestLog;\n    \tfor(var i=requestLog.length-1;i>=0;i--){\n            if(requestLog[i].xhr == xhr){\n\t\t\t\trequestLog[i].obj = obj;\n                this.refreshRequestObject(requestLog[i]);\n\t\t\t\treturn;\t\n            } \t\t\n    \t}\n    \tvar request = {\n    \t\txhr: xhr, \n\t\t\tobj: obj,\n    \t\tmethod: \'unknown\',\n    \t\turl: \'\', \n    \t\telement: null\n    \t};\n    \tif(this.isLoaded){\n    \t   this.viewRequestObject(request);\n    \t}\n    \trequestLog.push(request);\n\t\tthis.bounceTab(5);\n\t},\n    \n    refreshRequestObject: function(request){\n    \tvar self =this;\n    \tvar src = \'\';\n\t\tif(!request.node)\n\t\t\treturn;\n    \t\trequest.node.items = [];\n    \trequest.node.items.push( new air.Introspector.tree.node(\'readyState\' , { openable: false, nodeLabel2:request.xhr.readyState } ));\n    \ttry{\n    \t\trequest.node.items.push( new air.Introspector.tree.node(\'status\' , { openable: false, nodeLabel2:request.xhr.status } ));\n    \t}catch(e){\n    \t}\n\n    \ttry{\n\t\t\tif(typeof request.async==\'undefined\')\n\t\t\t\trequest.async = true;\n    \t\trequest.node.items.push( new air.Introspector.tree.node(\'async\' , { openable: false, nodeLabel2:request.async } ));\n    \t}catch(e){\n    \t}\n\n    \ttry{\n            request.node.items.push( new air.Introspector.tree.node(\'statusText\' , { openable: false, nodeLabel2:request.xhr.statusText } ));\n    \t}catch(e){}\n\n\t\t\ttry{\n\t\t\t\tif(typeof request.obj!=\'undefined\'&&request.obj!=null){\n\t\t\t\t\tif(this.isXMLObject(request.obj)){\n\t\t\t            request.node.items.push( new air.Introspector.tree.node(\'sent\' , { openable: true, \n\t\t\t                src: request.obj.firstChild,\n\t\t\t                onshow: function(){\n\t\t\t                    if(this.items.length==0){\n\t\t\t                        this.items = [self.createDom2TreeNode(this.src)];\n\t\t\t                    }\n\t\t\t            } } ));\n\t\t\t\t\t}else{\n\t\t\t    \t\trequest.node.items.push( new air.Introspector.tree.node(\'sent\' , { openable: true, \n\t\t\t    \t\t\tsrc: request.obj+\'\',\n\t\t\t    \t\t\tonshow: function(){\n\t\t\t\t    \t\t\tif(this.items.length==0){\n\t\t\t\t    \t\t\t\tthis.items = [new air.Introspector.tree.textNode(this.src)];\n\t\t\t\t    \t\t\t}\n\t\t\t    \t\t} } ));\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t}catch(e){\n\t\t\t}\n\n    \ttry{\n    \t\trequest.node.items.push( new air.Introspector.tree.node(\'responseText\' , { openable: true, \n    \t\t\tsrc: request.xhr.responseText,\n    \t\t\tonshow: function(){\n\t    \t\t\tif(this.items.length==0){\n\t    \t\t\t\tthis.items = [new air.Introspector.tree.textNode(this.src)];\n\t    \t\t\t}\n    \t\t} } ));\n    \t}catch(e){\n    \t}\n    \t\n\t\n    \t\n    \ttry{\n\t\t\trequest.node.items.push( new air.Introspector.tree.node(\'responseXml\' , { openable: true, \n                src: request.xhr.responseXML,\n                onshow: function(){\n                    if(this.items.length==0){\n\t\t\t\t\t\tself.extendDom2TreeNode(this, this.src, true);\n\/\/                        this.items = [self.createDom2TreeNode(this.src)];\n                    }\n\t            },\n\t\t\t\tonhide: function(){\n\t\t\t\t\t\/\/this.items.length = 0;\n\t\t\t\t\tthis.clearItems();\n\t\t\t\t}\n \t\t\t} ));\n        }catch(e){\n        }\n    \t\n\t\ttry{\n\t\t\tvar headers = request.xhr.getAllResponseHeaders();\n\t\t\trequest.node.items.push( new air.Introspector.tree.node(\'responseHeaders\' , { openable: true, \n    \t\t\tsrc: headers,\n    \t\t\tonshow: function(){\n\t    \t\t\tif(this.items.length==0){\n\t    \t\t\t\tthis.items = [new air.Introspector.tree.textNode(headers)];\n\t    \t\t\t}\n    \t\t} } ));\n\t\t}catch(e){\n\t\n\t\t}\n\t\t\n\t\n\t\t\t\n    \tif(request.node.opened){\n    \t\trequest.node.refreshChildren();\n    \t}\n    \trequest.element.scrollIntoViewIfNeeded();\n    },\n    \n    viewRequestObject: function(request){\n    \ttry{\n    \t\tvar self = this;\n\t    \tvar requestElement = this.htmlLoader.window.document.createElement(\"div\");\n\t    \trequestElement.className = \"requestElement\";\n\t    \tvar node = new air.Introspector.tree.node (request.method.toUpperCase()+\' \'+request.url, {\n\t    \t\tonshow: function(){\n\t               if(this.items.length==0){\n\t               \t  self.refreshRequestObject(request);\n\t               }    \t\t     \n\t    \t    }, \n\t    \t\tonhide: function(){\n\t    \t\t\t\/\/this.items.length = 0;\n\t\t\t\t\tthis.clearItems();\n\t    \t\t}\n\t    \t});\n\t\/\/    \tnode.shouldOpen();\n\t    \trequest.node = node;\n\t    \trequest.element = requestElement;\n            this.netTabDiv.appendChild(requestElement);\t    \t\n\t    \trequestElement.appendChild(node.createDiv(this.htmlLoader.window.document));\n\n    \t}catch(e){\n    \t\tthis.logError(e);\n    \t}\n    },\n    \n    refreshNetConsole: function(){\n    \tvar requestLog = this.requestLog;\n        for(var i=0;i<requestLog.length;i++){\n            this.viewRequestObject(requestLog[i]);\n        }\n        \n    },\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\t\/\/=======================================================================================================================================================\n\t\t\/\/DebugWindow.XHR.js\n\t\t\/\/=======================================================================================================================================================\t\t\t\n\t\tmakeDomDiv: function(){\n    this.domDiv.innerHTML = \'\';\n\tif(this.domDivNode){\n\t\tthis.domDivNode.dispose();\n\t\tthis.domDivNode = null;\n\t}\n    try{\n        var htmlLoader = this.getActiveHtmlLoader();\n\t\t\n\t\tvar node = this.createJsTreeNode(\'window\', htmlLoader.window);\n        var treeDiv = node.createDiv(this.htmlLoader.window.document);\n\t\tthis.domDivNode = node;\n        this.domDiv.appendChild(treeDiv);\n    }catch(e){\n    }\n},\n\ndom3Event: function(ev){\n\ttry{\n\t\tvar node = ev.srcElement || ev.target;\n\t\tif(!node) return;\n\t\tif(node.nodeType!=Node.DOCUMENT_NODE&&node.nodeType!=Node.ELEMENT_NODE){\n\t\t\tnode = node.parentNode;\n\t\t\tif(!node) return;\n\t\t}\n\t\n\t\tvar i = this.findDomListElement(node);\n\t\tif(i!=-1&&this.domList[i].node2)\n\t\t{\n\t\t\tvar node2 = this.domList[i].node2;\n\t\t\tif(this.domList[i].node2.opened){\n\t\t\t\tnode2.clearItems();\n\t\t\t\tnode2.onshow(node2);\n\t\t\t}\n\t\t\tnode2.refresh();\n\t\t\tev.stopPropagation();\n\t\t\t\n\t\t\tthis.openVisibleDomTags();\n\t\t}\n\n\t}catch(e){\n\t}\t\n},\n\nfindDomListElement: function(element){\n    var list = this.domList;\n    for(var i=list.length-1;i>=0;i--){\n        if(list[i].element==element)\n          return i;\n    }\n    return -1;\n},\n\nremoveDomListElement: function(element){\n    var index =  this.findDomListElement(element);\n    if(index==-1)\n       return;\n    this.domList.splice(index, 1);\n    \n    var child = element.firstChild;\n    while(child)\n    {\n       this.removeDomListElement(child);\n       child = child.nextSibling;\n    }    \n},\n\n\n extendDom2TreeNode: function(node, obj, readOnly){\n\tif(typeof readOnly==\'undefined\'){readOnly = false;}\n    try{\n        var self = this;\n        node.items.length = 0;\n        if(typeof obj==\'undefined\'||obj==null)\n           return;\n         var child = obj.firstChild;\n         while(child)\n         {\n\/\/             \tif(child.nodeType==Node.ELEMENT_NODE||child.nodeType==Node.TEXT_NODE){\n         \t\tif(child.nodeType==Node.TEXT_NODE){\n         \t\t\tvar value = air.Introspector.trim(child.nodeValue);\n         \t\t\tif(value.length==0)\n         \t\t\t{   \n         \t\t\t\tchild = child.nextSibling;\n         \t\t\t    continue;\n         \t\t\t}\n         \t\t}\n                node.items.push(this.createDom2TreeNode(child, readOnly)); \n\/\/             \t}else{\n\/\/\t\t\t\t\tair.Introspector.Console.log(child);\n\/\/\t\t\t\t}\n            child = child.nextSibling;\n         }\n    }catch(e){\n        this.logError(e);\n    }\n},\n\ncreateDom2TreeNode : function(child, readOnly){\n    var self = this;\n    \n    var config = {\n\t\t  readOnly: readOnly,\n          editable:false,\n          openable:false,\n            onshow: function(sender){\n                var domListIndex = self.findDomListElement(child);\n                if(domListIndex!=-1){\n                     self.domList[domListIndex].opened = true;\n                }\n                self.extendDom2TreeNode(sender, child, readOnly);\n            }, \n            onhide: function(sender){\n                self.removeDomListElement(child); \/\/also remove children\n                \/\/this.items.length = 0;\n\t\t\t\tthis.clearItems();\n            }\n        };\n    var node = new air.Introspector.tree.domNode(child, config);\n   \n    var domListIndex = this.findDomListElement(child);\n    if(domListIndex!=-1){\n        this.domList[domListIndex].node2 = node;\n        if(this.domList[domListIndex].opened)\n            node.shouldOpen();\n        if(this.domList[domListIndex].selected)\n            node.shouldSelect();\n    }else{\n        this.domList.push({\n            element:child, \n            node2: node,\n            node: null,\n            opened: false,\n            selected: false\n        });\n    }\n    \n    return node;\n},\n    \ncreateDomElementPath: function(element, first){\n\tvar ret = false;\n    if(typeof first==\'undefined\') first = false;\n    var domListIndex = this.findDomListElement(element);\n    if(domListIndex==-1){\n        this.domList.push({\n            element:element, \n\/\/                node: null, \n            node2 :null,\n            opened: true, \n            selected: first\n        });\n\t\tret=true;\n    }else if(!first){\n\t\tif(this.domList[domListIndex].node2){\n\t\t\tif(!this.domList[domListIndex].node2.opened && this.domList[domListIndex].node2.showElements)\n\t\t\t\tthis.domList[domListIndex].node2.showElements();\n\t\t}else{\n\t\t\tret = true;\n\t\t}\n        this.domList[domListIndex].opened = true;\n    }else{\n        this.domList[domListIndex].selected = true;\n\t\tif(this.domList[domListIndex].node2&&this.domList[domListIndex].node2.select){\n\t\t\tthis.domList[domListIndex].node2.select(false);\n\t\t}else{\n\t\t\tret = true;\n\t\t}\n    }\n    \n    var parentNode = element.parentNode;\n    if(parentNode){\n        ret|=this.createDomElementPath(parentNode);\n    }\n\treturn ret;\n},\n\nshowDomElementPath: function(element){\n    var list = this.domList;\n    for(var i=list.length-1;i>=0;i--){\n        list[i].selected = false;\n\t\tif(list[i].node2&&list[i].node2.unselect){\n\t\t\tlist[i].node2.unselect();\n\t\t}\n    }\n    var shouldRecreate = this.createDomElementPath(element, true);\n    \n    var htmlLoader = this.getActiveHtmlLoader();\n    if(htmlLoader!=null&&htmlLoader.window.document==element.ownerDocument){\n\t\tif(shouldRecreate){\n        \tthis.makeHtmlDiv(); \t\n\t\t}\n    }else{\n    \tthis.setActiveWindowByDocument(element.ownerDocument);\n    }\n    \n   \n\tthis.scrollSelectedDomItemIntoView();\n},\n\n\nputInList: function(element){\n\tvar domListIndex = this.findDomListElement(element);\n    if(domListIndex!=-1){\n\t\tthis.domList[domListIndex].opened = true;\n\t}\n},\n\n\tsetHoverDomElement: function(node){\n\t\tthis.hoverDomElement = node;\n\t},\n\t\n\tclearHoverDomElement: function(){\n\t\tthis.hoverDomElement = null;\n\t\tthis.hoverDomElementAttribute = null;\n\t},\n\n\tsetHoverDomElementAttribute: function(att){\n\t\tthis.hoverDomElementAttribute = att;\n\t},\n\t\n\texpandHoverDomElement: function(){\n\t\tif(this.menuHoverDomElement)\n\t\t\tthis.menuHoverDomElement.expandAll();\n\t},\n\t\n\tcollapseHoverDomElement: function(){\n\t\tif(this.menuHoverDomElement)\n\t\t\tthis.menuHoverDomElement.collapse();\n\t},\n\t\n\taddHoverDomElementAttribute: function(){\n\t\ttry{\n\t\tif(this.menuHoverDomElement){\n\t\t\tvar attributeName = prompt(\'Attribute name\');\n\t\t\tif(attributeName&&attributeName.length){\n\t\t\t\tif(!this.menuHoverDomElement.domNode.getAttribute(attributeName))\n\t\t\t\t\tthis.menuHoverDomElement.domNode.setAttribute(attributeName, \'\');\n\t\t\t\tthis.menuHoverDomElement.refreshAttributes();\n\t\t\t\tthis.menuHoverDomElement.toggleAttribute(attributeName);\n\t\t\t}\n\t\t}}catch(e){\n\t\t\talert(\"Invalid attribute name!\");\n\t\t}\n\t},\n\t\n\tremoveHoverDomElementAttribute: function(){\n\t\tif(this.menuHoverDomElement&&this.menuHoverDomElementAttribute){\n\t\t\t\tthis.menuHoverDomElement.domNode.removeAttribute(this.menuHoverDomElementAttribute.nodeName);\n\t\t\t\tthis.hoverDomElementAttribute = null;\n\t\t\t\tthis.menuHoverDomElement.refreshAttributes();\n\t\t}\n\t},\n\n\topenVisibleDomTags: function(){\n\/\/\t\tthis.scrollSelectedDomItemIntoView();\n\/\/\t\treturn;\n\t\tvar list = this.domList;\n\t\tvar clone = [];\n\t\t for(var i=list.length-1;i>=0;i--)\n\t\t\tclone.push(list[i].element);\n\t\t for(var i=clone.length-1;i>=0;i--){\n\t\t\tvar element = clone[i];\n\t\t\tvar parentNode = element.parentNode;\n\t\t\tif(parentNode){\n\t            this.putInList(parentNode);\n\t\t\t}\n\t\t}\n\t},\n\n    scrollSelectedDomItemIntoView: function(){\n\t\t var list = this.domList;\n\t\t for(var i=list.length-1;i>=0;i--){\n\t            if(list[i].selected){\n\t            \tif(list[i].node2){\n\t\t\t\t\t\tif(list[i].node2.nodeLabelDiv)\n\t\t\t\t\t\t\tlist[i].node2.nodeLabelDiv.scrollIntoViewIfNeeded();\n\t\t\t\t\t\telse if(list[i].node2.element)\n\t            \t\t\tlist[i].node2.element.scrollIntoViewIfNeeded();\n\t            \t\t\/\/air.Introspector.Console.log(list[i].node2);\n\t            \t\t\/\/air.Introspector.Console.log(this.html2Div);\n\t\t\t\t\t\tbreak;\n\t            \t}\n\t            }\n\t        }\n\t},\n\t\n\t\n\tsetInspect: function(value){\n        this.isInspecting = value;\n        if(this.inspectToolLabel){\n            if(value){\n                if((this.inspectToolLabel.className+\'\').indexOf(\'selected\')==-1){\n                  this.inspectToolLabel.className += \' selected\';\n                }\n            }else{\n                this.inspectToolLabel.className = this.inspectToolLabel.className.replace(\' selected\', \'\');\n            }\n        }\n\t\tif(value){\n\t\t\tthis.switchToHtmlTab();\n\t\t}\n\t\n\t\t\n\t\tif(isAppSandbox){\n\t        var ownedWindows = air.Introspector.getHtmlWindows();\n\t        for(var i=ownedWindows.length-1;i>=0;i--){\n\t            try{\n\t              ownedWindows[i].htmlLoader.window.air.Introspector.inspect=value;\n\t              ownedWindows[i].htmlLoader.window.air.Introspector.canClick=false;\n\t            }catch(e){\n\t            }\n\t        }\n\t\t}else{\n\t\t\ttry{\n\t\t\t\tthis.activeWindow.air.Introspector.inspect = value;\n\t\t\t\tthis.activeWindow.air.Introspector.canClick=false;\n\t\t\t}catch(e){}\n\t\t}\n    },\n    toggleInspect: function(){\n        this.isInspecting ^= true;\n        if(this.isInspecting){\n           this.startInspect();\n        }else{\n           this.finishInspect();\n        }\n    },\n    startInspect: function()\n    {\n        this.setInspect(true);\n        this.inspectElement = null;\n    },\n    \n    finishInspect: function(canceled){\n        this.setInspect(false);\n        if(typeof canceled==\'undefined\')\n           canceled = true;\n        if(!canceled&&typeof this.inspectElement!=\'undefined\'&&this.inspectElement){\n           this.showCssElement(this.inspectElement);\t\n           this.showDomElementPath(this.inspectElement);\n        }else{\n\t\t\tif(this.selectedElement){\n\t\t\t\tthis.showDomElementPath(this.selectedElement);\n           \t\tthis.showCssElement(this.selectedElement);\t\t\t\t\t\n\t\t\t}\n\t\t}\n        delete inspectElement;\n    }, \n    \n\tshowCssElement: function(element){\n\t\tif(!element || !element.ownerDocument || !element.ownerDocument.defaultView) return;\n\t\tif(this.selectedElement==element){ return };\n\t\tthis.selectedElement = element;\n\t\tvar self = this;\n\t\tif(this.showCssElementTimer){\n\t\t\tclearTimeout(this.showCssElementTimer);\n\t\t\tthis.showCssElementTimer = null;\n\t\t}\n\t\tthis.clearCssElement();\n\t\tthis.showCssElementTimer = setTimeout(function(){\n\t\t\tself.showCssElementTimer = null;\n\t\t\tself.showCssElementTimeout(element);\n\t\t}, 300);\n\t},\n\tclearCssElement:function(){\n\t\tif(!this.isLoaded) return;\n\t\tthis.css2DomTab.innerHTML = \'\';\n\t\tthis.css2StyleTab.innerHTML = \'\';\n\t\t\n\t\tif(this.cssElementJsNode){\n\t\t\tthis.cssElementJsNode.dispose();\n\t\t\tthis.cssElementJsNode = null;\n\t\t}\n\t},\n\tshowCssElementTimeout: function(element){\n\t\tif(!this.isLoaded) return;\t\t\n\t\tvar value = \'\';\n\/*\t\ttry{\n\t\t\tvalue = \'DOM (\' + element+\')\';\n\t\t}catch(e){}*\/\n\t\tvar jsNode = this.createJsTreeNode(value, element);\n\t\tjsNode.shouldOpen();\n        this.css2DomTab.appendChild(jsNode.createDiv(this.htmlLoader.window.document));\n\t\tthis.cssElementJsNode = jsNode;\n\n\t\tif(this.cssElementCssNode){\n\t\t\tthis.cssElementCssNode.dispose();\n\t\t\tthis.cssElementCssNode = null;\n\t\t}\n\n\t\tvar cssNode = this.createCssTreeNode(\'CSS\', element);\n\t\tcssNode.shouldOpen();\n        this.css2StyleTab.appendChild(cssNode.createDiv(this.htmlLoader.window.document));\n\t\tthis.cssElementCssNode =  cssNode;\t\n\t},\n\t\n\tcreateCssTreeNode: function (stringValue, element){\n\t\tvar self = this;\n\t\tvar config = {\n            editable:false,\n              onshow: function(sender){\n                  self.extendCssTreeNode(sender, element);\n              }, \n              onhide: function(sender){\n                  \/\/this.items.length = 0;\n\t\t\t\t\tthis.clearItems();\n              }\n          };\n\t\t return new air.Introspector.tree.node(stringValue,config);\n\t},\n\n\n\textendCssTreeNode: function(node, element){\n\t\ttry{\n\t\t\tvar self = this;\n\t\t\tif(!element.ownerDocument || !element.ownerDocument.defaultView) return;\n\t\t\tvar obj = element.ownerDocument.defaultView.getComputedStyle(element);\n\t\t\t\n\t\t\tif(obj){\n\t\t\t\tvar cssGroupNames = this.cssGroupNames;\n\t\t\t\tvar cssGroups = this.cssGroups;\t\t\t\t\n\t\t\t\t\n\t\t\t\tvar groupsCount=cssGroupNames.length;\n\n\t\t\t\tfor(var z=0;z<groupsCount;z++){\n\t\t\t\t\tvar groupNode = new air.Introspector.tree.node(cssGroupNames[z], \n\t\t\t\t\t\t\t{\n\t\t\t\t\t\t\t\tgroup: cssGroups[z],\n\t\t\t\t\t\t\t\tonshow: function(){\n\t\t\t\t\t\t\t\t\tif(this.items.length==0){\n\t\t\t\t\t\t\t\t\t\tvar l = this.group.length;\n\t\t\t\t\t\t\t\t\t\tfor(var i=0;i<l;i++){\n\t\t\t\t\t\t\t\t\t\t\tvar name = this.group[i], value;\n\t\t\t\t\t\t\t                try{\n\t\t\t\t\t\t\t                    value = obj[name];\n\t\t\t\t\t\t\t                }catch(e){\n\t\t\t\t\t\t\t                    value = e;\n\t\t\t\t\t\t\t                }\n\t\t\t\t\t\t\t                this.items.push(self.createJsTreeNode(name, value));\n\t\t\t\t\t\t\t            }\n\t\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t});\n\t\t\t\t\tgroupNode.shouldOpen();\n\t\t\t\t\tnode.items.push(groupNode);\n\t\t\t\t}\n\t\t\t}\n\t\t}catch(e){\n\t\t\tthis.logError(e);\n\t\t}\n\t},\n\n\tswitchToHtmlTab: function () {\n\t\tthis.setTab(1);\n\t},\n\n    setInspectElement: function(element){\n        this.inspectElement = element;\n        this.showDomElementPath(this.inspectElement);\n    },\n    highlight: function(domNode){\n    \tvar htmlLoader = this.getActiveHtmlLoader();\n        if(htmlLoader!=null){\n        \ttry{\n        \t\tif(typeof domNode!=\'undefined\'){\n        \t       htmlLoader.window.air.Introspector.highlightElement(domNode);\n\t\t\t\t\n        \t\t}\n        \t    else\n        \t       htmlLoader.window.air.Introspector.hideHighlight();\n        \t}catch(e){\n        \t\t\/\/no air debug highlighter..\n        \t}\n        }\n    \t\n    },\n\n\t  setCssTab: function(tab){\n\t\t\tthis.activeCssTab = tab;\n\t        if(this.isLoaded){\n\t\t\t\tthis.activateCssTab();\n\t\t\t}\n\n\t    },\n\t\tactivateCssTab: function(){\n\t\/\/\t\tif(!this.isLoaded) return;\n\t\t\ttry{\n\t\t        this.clearCssTabs();\n\t\t\t\tvar tabName = this.cssTabs[this.activeCssTab];\n\n\t\t\t\tthis.htmlLoader.window.document.getElementById(tabName+\'Label\').className = \'selected\';\n\t\t        this.htmlLoader.window.document.getElementById(tabName+\'Tab\').className = \'selected\';\n\t\t\t}catch(e){}\n\t\t},\n\n\t \tclearCssTabs: function(){\n\t        var child = this.cssTabLabels.firstChild;\n\t        while(child!=null){\n\t            if(child.nodeType==1)  child.className = child.className.replace(\/selected\/, \'\');\n\t            child=child.nextSibling;\n\t        }\n\t        var child = this.cssTabPages.firstChild;\n\t        while(child!=null){\n\t            if(child.nodeType==1) child.className = child.className.replace(\/selected\/, \'\');\n\t            child=child.nextSibling;\n\t        }\n\t    },\n\t\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\t\/\/convert DebugUI.html\n\n\t\t};\n\t\t\t\t\n\t\n\t\/\/=======================================================================================================================================================\n\t\/\/SearchBox.js\n\t\/\/=======================================================================================================================================================\t\t\t\n\t(function(){\n\t\n\tfunction SearchBox(){ \n\t\tvar that = this;\n\t\tvar m_node = null;\n\t\tvar m_visible = false;\n\t\tvar m_created = false;\n\t\tvar m_search_value = null;\n\t\tvar m_last_highlight = null;\n\t\t\/\/dom elements\n\t\tvar m_text_box = null;\n\t\tvar m_search_box = null;\n\t\t\n\t\tfunction show(){\n\t\t\tif(!m_visible){\n\t\t\t\tif(!m_created) createElements();\n\t\t\t\tm_search_box.style.display = \'block\';\n\t\t\t\tm_visible = true;\n\t\t\t}\n\t\t\tsetTimeout(function()\n\t\t\t{\n\t\t\t\tm_text_box.select();\t\t\t\t\t\t\t\t\n\t\t\t}, 0);\n\n\t\t};\n\t\t\n\t\tfunction hide(){\n\t\t\tif(m_visible&&m_created){\n\t\t\t\tm_search_box.style.display = \'none\';\t\t\t\t\n\t\t\t\thideLastHighlight();\n\t\t\t\tm_visible = false;\t\n\t\t\t}\n\t\t};\n\t\t\n\t\tfunction getSearchValue(){\n\t\t\treturn m_text_box.value.toUpperCase();\n\t\t}\n\t\t\n\t\tfunction hideLastHighlight(){\n\t\t\tif(m_last_highlight&&m_last_highlight.parentNode){          \n\t\t\t\tm_last_highlight.parentNode.className = (m_last_highlight.parentNode.className+\'\').replace(\/searchselect\/g, \'\');\n\t\t\t\tm_last_highlight=null;\n\t\t\t}\n\t\t}\n\t\t\n\t\tfunction searchNext(){\n\t\t\tvar searchedNode = m_node || document.body;\n\t\t\tm_node.className += \' selectable\';\n\t\t\thideLastHighlight();\n\t\t\tvar first = null;\n\t\t\twhile(window.find(getSearchValue(), false, false, true,\n\t\t\t            false, true, false)){\n\t\t\t\ttry{ \n\t\t\t\t\tvar selection = window.getSelection();\n\t\t\t\t\tif(first==null){\n\t\t\t\t\t\tfirst = selection.extentNode;\n\t\t\t\t\t}else if(selection.extentNode===first){\n\t\t\t\t\t\t\/\/don\'t enter an infinite loop\n\t\t\t\t\t\tbreak;\n\t\t\t\t\t}\n\t\t\t\t\tif(!selection.isCollapsed){\n\t\t\t\t\t\tvar range = selection.getRangeAt(0);\n\t\t\t\t\t\tvar  node = range.commonAncestorContainer;\n\t\t\t\t   \t\tif(node&&node.parentNode){\n\t\t\t\t\t\t\tnode.parentNode.className += \' searchselect\'; \n\t\t\t\t\t\t\tm_last_highlight = node;\n\t\t\t\t\t\t}\n\t\t\t\t\t\tbreak;\n\t\t\t\t\t}\n\t\t    \t}catch(e){\n\t\t\t\t}\n\t\t\t}\n\t\t\tm_node.className = m_node.className.replace(\/selectable\/g, \'\');\n\t\t}\n\t\t\n\t\tfunction checkParent(node){\n\t\t\twhile(node){\n\t\t\t\tif(node==m_node) return true;\n\t\t\t\tnode = node.parentNode;\n\t\t\t}\n\t\t\treturn false;\n\t\t}\n\t\t\n\t    \n\t\tfunction createElements(){\n\t\t\tm_search_box = document.createElement(\"div\");\n\t\t\tm_text_box = document.createElement(\"input\");\n\t\t\tm_search_box.appendChild(m_text_box);\t\t\t\n\t\t\tm_text_box.addEventListener(\'keydown\', function (evt){\n\t\t\t\tif(m_visible&&evt.keyCode==13){\n\t\t\t\t\tsearchNext();\n\t\t\t\t}\n\t\t\t});\n\t\t\tm_search_box.setAttribute(\'style\', \'position:absolute;right:50px;bottom:50px;display:none;padding:10px;border:1px solid #cccccc;-webkit-border-radius:5px;background:#cccccc\');\n\t\t\tdocument.body.appendChild(m_search_box);\n\t\t\tm_created = true;\n\t\t}\n\t\t\n\t\tfunction registerKeyboardEvents(){\n\t\t\tdocument.addEventListener(\'keydown\', function(evt){\n\t\t\t\t\tif((evt.ctrlKey||evt.metaKey)==true&&evt.altKey==false&&evt.keyCode==70){\n\t\t\t\t\t\t\/\/ctrl\/cmd - F\n\t\t\t\t\t\tshow();\n\t\t\t\t\t\tevt.preventDefault();\n\t\t\t\t\t}else if( evt.keyCode==27&&(evt.ctrlKey||evt.metaKey||evt.altKey)==false){\n\t\t\t\t\t\t\/\/escape key was pressed\n\t\t\t\t\t\thide();\n\t\t\t\t\t\tevt.preventDefault();\t\t\t\t\t\n\t\t\t\t\t}if( ((evt.ctrlKey||evt.metaKey)==true&&evt.altKey==false&&evt.keyCode==71)\n\t\t\t\t\t\t|| (evt.keyCode==114&&(evt.ctrlKey||evt.metaKey||evt.altKey)==false)){\n\t\t\t\t\t\t\t\/\/ctrl\/cmd - G or F3\n\t\t\t\t\t\tsearchNext();\n\t\t\t\t\t\tevt.preventDefault();\n\t\t\t\t\t}\n\t\t\t\t});\n\t\t}\n\t\t\n\t\tfunction init(){\n\t\t\tregisterKeyboardEvents();\n\t\t}\n\t\t\n\t\tthis.attachToNode = function SearchBox_attachToNode(node){\n\t\t\tif(node) \n\t\t\t\tm_node = node;\n\t\t\telse\n\t\t\t\tm_node = document;\n\t\t}\n\t\t\n\t\tinit();\n\t};\n\t\n\twindow.SearchBox = new SearchBox;\n\t\n}());\n\t\/\/-------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\t\t\t\t\n\t\tair.Introspector.version = \"1.5\";\n\t\n\n})();\n\/\/ ]]>\n<\/script>\n\n<\/head>\n\n<body onload=\'doLoad()\' onunload=\"doUnload()\" oncontextmenu=\'return debugWindow.showMenu(event);\'>\n\t<ul id=\"toolsLabels\">\n\t\tADOBE<span>&copy;<\/span> AIR&trade; Introspector\n\t<\/ul>\n    <ul id=\"toolToggle\">\n<li onclick=\"debugWindow.setTool(\'inspect\')\" id=\'inspectToolLabel\'>Inspect<\/li><\/ul>\n\t<ul id=\"tabLabels\">\n   \t  <li onclick=\"debugWindow.setTab(0)\" id=\'consoleLabel\'>Console<\/li>    \t\n        <li onclick=\"debugWindow.setTab(1)\" id=\'html2Label\'>HTML<\/li>\n\t\t<li onclick=\"debugWindow.setTab(2)\" id=\'domLabel\'>DOM<\/li>\n        <li onclick=\"debugWindow.setTab(3)\" id=\'assetsLabel\'>Assets<\/li>\n        <li onclick=\"debugWindow.setTab(4)\" id=\'sourceLabel\'>Source<\/li>\n\t\t<li onclick=\"debugWindow.setTab(5)\" id=\'netLabel\'>XHR<\/li>\n\t<\/ul>\n    <div id=\"tabPages\">\n<div id=\"consoleTab\">\n        \t<ul id=\"console\"><\/ul>\n            <div id=\"evalConsole\"><span id=\"evalConsoleLabel\">&gt;&gt;&gt;<\/span><input type=\"text\" id=\"evalConsoleText\" value=\"\" onkeyup=\"if(event.keyCode==13){ debugWindow.evalUserInput();return true;} else if(event.keyCode==38){ debugWindow.historyUserInput(-1); return true;} else if(event.keyCode==40){ debugWindow.historyUserInput(1); return true;} else debugWindow.historyUserInput();\" \/><\/div>\n        <\/div>\n\n\t\t<div id=\"html2Tab\">\n          <div id=\"html2Div\">\n\t\t  <\/div>\n\t\t  <div id=\"html2Split\">\n\t\t  <\/div>\n\t\t  <div id=\"css2Div\">\n\t\t\t<ul id=\"css2TabLabels\">\n\t\t    \t<li onclick=\"debugWindow.setCssTab(0)\" id=\'css2DomLabel\'>DOM<\/li>    \t\n\t\t        <li onclick=\"debugWindow.setCssTab(1)\" id=\'css2StyleLabel\'>Computed style<\/li>\n\t\t    <\/ul>\n\t\t\t<div id=\"css2TabPages\">\n\t\t\t\t<div id=\'css2DomTab\'><\/div>    \t\n\t\t        <div id=\'css2StyleTab\'><\/div>\n\t\t\t<\/div>\n\t      <\/div>\n\t\t\t\n        <\/div>    \n    \t<div id=\"domTab\">\n        \t\n        <\/div>\n    \t<div id=\"assetsTab\">\n        \t\n        <\/div> \n        <div id=\"sourceTab\">\n        \t\n        <\/div>\n\t\t<div id=\"netTab\">\n            \n        <\/div>    \n<\/div>\n    \n<div id=\"windowSelector\">\n<input id=\'refreshActiveWindow\' type=\"button\" value=\"Refresh active window:\" onclick=\"debugWindow.refreshWindows()\" \/>\n\t <select id=\"windowList\" onchange=\"debugWindow.setActiveWindowById(parseFloat(this.options[this.options.selectedIndex].value))\"><\/select>\n       \n<\/div>\n\n\n\n<div id=\"menuDiv\">\n\t<div id=\"consoleMenuDiv\" class=\"menu\">\n\t\t<ul>\n\t\t\t<li><a href=\"javascript:void(0)\" onclick=\'debugWindow.clearConsole();debugWindow.hideMenu();\'>Clear console<\/a><\/li>\n\t\t\t<li><a href=\"javascript:void(0)\" onclick=\'debugWindow.saveConsoleToFile();debugWindow.hideMenu();\'>Save console to file...<\/a><\/li>\n\t\t\t<li><a href=\"javascript:void(0)\" onclick=\'debugWindow.saveConsoleToClipboard();debugWindow.hideMenu();\'>Save console to clipboard<\/a><\/li>\t\t\n\t\t<\/ul>\n\t<\/div>\n\t<div id=\"domMenuDiv\" class=\"menu\">\n\t\t<ul>\n\t\t\t\t<li><a href=\"javascript:void(0)\" onclick=\'debugWindow.expandHoverDomElement();debugWindow.hideMenu();\'>Expand all<\/a><\/li>\n\t\t\t\t<li><a href=\"javascript:void(0)\" onclick=\'debugWindow.collapseHoverDomElement();debugWindow.hideMenu();\'>Collapse<\/a><\/li>\n\t\t\t\t<li><a href=\"javascript:void(0)\" onclick=\'debugWindow.addHoverDomElementAttribute();debugWindow.hideMenu();\'>Add attribute<\/a><\/li>\n\t\t\t\t<li id=\"domRemoveAttributeMenuDiv\"><a href=\"javascript:void(0)\" onclick=\'debugWindow.removeHoverDomElementAttribute();debugWindow.hideMenu();\'>Remove attribute<\/a><\/li>\t\t\t\t\n\t\t<\/ul>\n\t<\/div>\n<\/div>\n\n\n<div id=\"searchTextBox\">\n\tSearch filter: <input type=\"text\" id=\'txtSearch\' value=\'\' onkeydown=\'debugWindow.searchKeyDown(event)\' onkeyup=\'debugWindow.doFilter(this.value);\' \/>\n\t\n<\/div>\n  \n<\/body>\n<\/html>\n' 
air.Introspector.register();
