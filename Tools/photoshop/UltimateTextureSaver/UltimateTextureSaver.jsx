function UTSTexture(sourceXml) 
{
    this.TG_COLOR =1;
    this.TG_ALPHA = 2;
    this.TG_R = 4;
    this.TG_G = 8;
    this.TG_B = 16;			
    this.name = undefined;
    this.suffix = undefined;
    this.components = {};
    this.isComposite = false;
    this._constructor = function(srcXml) {
        this.name = srcXml.@name.toString();
        this.suffix = srcXml.@suffix.toString();
        
        
        
        for (nodeIndex = 0; nodeIndex< srcXml.elements()[0].elements().length(); nodeIndex++) 
        {

            var node = srcXml.elements()[0].elements()[nodeIndex];
            var nName= node.@name.toString();
            var newComponent = new Object();
            newComponent.name = nName
            var colour = 'None';
            switch (node.@color.toString().toLocaleLowerCase()){  
                  case 'red': colour = 'Rd  '; break;  
                  case 'orange' : colour = 'Orng'; break;  
                  case 'yellow' : colour = 'Ylw '; break;  
                  case 'yellow' : colour = 'Ylw '; break;  
                  case 'green' : colour = 'Grn '; break;  
                  case 'blue' : colour = 'Bl  '; break;  
                  case 'violet' : colour = 'Vlt '; break;  
                  case 'gray' : colour = 'Gry '; break;  
                  case 'none' : colour = 'None'; break;  
                  default : colour = 'None'; break;  
            }
            newComponent.color = colour;
            
            
            if (node.@alias.toString() != "")
            {
                // component hase aliases available, so we need to analyze them
                var allAliases = node.@alias.toString();
                newComponent.alias = allAliases.split(";");
            }

            this.setTargetAndMapping(newComponent, node);
           this.components[nName] = newComponent;
        }   
    };
     
    this.CreateLayout= function()
    {
        for (var property in this.components) {
            if (this.components.hasOwnProperty(property)) {
                    if(!UltimateTextureSaver.SetExists(property)) {					
                        var newLS = app.activeDocument.layerSets.add();
                        newLS.name = property;
                        var desc33 = new ActionDescriptor();
                        var ref21 = new ActionReference();
                        ref21.putEnumerated( charIDToTypeID( "Lyr " ), charIDToTypeID( "Ordn" ), charIDToTypeID( "Trgt" ) );
                        desc33.putReference( charIDToTypeID( "null" ), ref21 );
                        var desc34 = new ActionDescriptor();
                        desc34.putEnumerated( charIDToTypeID( "Clr " ), charIDToTypeID( "Clr " ), charIDToTypeID( this.components[property].color ) );
                        desc33.putObject( charIDToTypeID( "T   " ), charIDToTypeID( "Lyr " ), desc34 );
                        executeAction(  charIDToTypeID( "setd" ), desc33, DialogModes.NO );

                }	
            }
        }	
    };
		
    this.ValidateComponent = function (compName)
    {
            var currentSet;
            try
            {
                    currentSet = app.activeDocument.layerSets.getByName(compName);
            } 
            catch(error) 
            {
                    return false;
            }

            if(currentSet.artLayers.length == 0 && currentSet.layerSets.length == 0)
                    return false;

            return true;

    };  
     
    this.setTargetAndMapping = function (newComponent, node) {
        newComponent.hasMapping = false;
        if (node.@mapping.toString() != "")
        {
            newComponent.hasMapping = true;
        }
	
			
        var target = node.@target.toString();
        if (target == "color") {
                newComponent.target = this.TG_COLOR;
                if (newComponent.hasMapping) {
                        var mapping = node.@mapping.toString();
                        // jestli mame mapping, tak si ho musimen ejak zapsat.
                        newComponent.mapping = new Object();
                        var tempMapping = [this.TG_R,this.TG_G,this.TG_B]
                        for (var i = 0; i < mapping.length; i++) 
                        { 
                                // kdyby se cirou nahodu stalo ze v mappingu bude vic nofmraci nez  je zahodno.
                                if (i >= newComponent.mapping.length)
                                        break;

                                var mappingNum = this.getConstant(mapping.charAt(i));
                                if (mappingNum != 0)
                                        newComponent.mapping[tempMapping[i]] = mappingNum;
                        }

                }
        } else if (target == "a") {
                newComponent.target = this.TG_ALPHA;
                if (newComponent.hasMapping) {
                        var mapping = node.@mapping.toString();
                        newComponent.mapping =  new Object();
                        var mappingNum = this.getConstant(mapping.charAt(0));
                        if (mappingNum != 0)
                                newComponent.mapping[this.TG_ALPHA] = mappingNum;					
                        // defaultne vezmeme prvni znak, precejen se bavime o alfe	
                }

        } else {
                // tady bude ten mapping malinko slozitejsi..
                // a vlastne o moc ne.. bude to easy. :-D
                newComponent.target = 0;
                newComponent.mapping = new Object();
                var tempMapping = []
                for (var i = 0; i < target.length; i++) {
                        var targetSrc = this.getConstant(target.charAt(i));
                        if(targetSrc == 0)
                                continue;
                        newComponent.target = newComponent.target | targetSrc;
                        if (newComponent.hasMapping) 
                                tempMapping.push(targetSrc);



                }
                if (newComponent.hasMapping) {
                        var mapping = node.@mapping.toString();

                        for (var i = 0; i < tempMapping.length; i++) {
                                var mappingTrg = this.getConstant(mapping.charAt(i));
                                if (mappingTrg != 0)
                                        newComponent.mapping[tempMapping[i]] = mappingTrg;					
                        }		
                }	

        }
        if (newComponent.hasMapping)
                this.isComposite = true;

};


// kazda skupina by vlastne mela umet se sama vysejnovat jako by nic protoze ona sama vi nejlip jak s tim nalozit.
this.Save = function Save(showDialog)
        {
			// create new composite layer to work with later
			var compositeLayer = undefined;
			var saveTexture= false;
			var colorTarget = undefined;
                        
			for(var i in this.components)
			{
				// prijdu vsechny komponenty, ktere se mne katualne tykaji.
				// compozit bych mel asi udelat vzdycky, tak abych mel nakonci nahore vrstvu ktera se savuje a pripadne alfu.. easy pie.
				var actualComp= this.components[i];
                  var compNameToSave = actualComp.name;
				if(!this.ValidateComponent(actualComp.name))
                                {
                                    // validation failed, but maybe component has aliases, so need to check them also
                                    if(actualComp.hasOwnProperty("alias"))
                                    {
                                        var found = false;
                                        for(var i in actualComp.alias)
                                        {
                                            if(this.ValidateComponent(actualComp.alias[i])) {
                                                compNameToSave = actualComp.alias[i];
                                                found = true;
                                                break;
                                            }
                                        }
                                        if(!found)
                                            continue;
                                    } else {
                                        continue;
                                    }
                                    
                                }
				
				// je treba zvalidovat jestli vubec tenhle component jde savovat
				saveTexture = true;
				
				switch(actualComp["target"])
				{
					case this.TG_COLOR:
					{
                                                // if there is default color set to be displayed, we just save its name and during the save it should be unhidden, thus, export shoudl be waaay faster
                                                colorTarget = compNameToSave;
						/*compositeLayer.visible = true;	
						compositeLayer.clear();
						app.activeDocument.activeLayer = compositeLayer;
						this.makeOnlySetVisible(compNameToSave);
						compositeLayer.copy (true);
						
						app.activeDocument.paste();
						compositeLayer.visible = false;*/
						break;
					}
						
					case this.TG_ALPHA:
					{
						// alfa jde do alfy, to by mohlo ybt snadne
						this.makeOnlySetVisible(compNameToSave);
						var currentChannels = app.activeDocument.activeChannels;
						var alfaChannel =  UltimateTextureSaver.GetChannel(this.getChannelName(this.TG_ALPHA));
						var newLayer = app.activeDocument.artLayers.add();
						newLayer.copy (true);
						
						app.activeDocument.activeChannels = [ alfaChannel ];
						app.activeDocument.paste();
						app.activeDocument.activeChannels = currentChannels;
						newLayer.remove();						
						break;
					}
						
					default:
					{						
						/*
							<Component name="detail_diff" target="r"/>
							<Component name="detail_gloss" target="b"/>
							<Component name="detail_normal" target="ga" mapping="rg" /> 						
						*/
						// za a je treba poresit targeting, takze za a je treba vytvorit novy	
						this.makeOnlySetVisible(compNameToSave);
                            if (compositeLayer==undefined)
                            {
                                   compositeLayer = app.activeDocument.artLayers.add();

                                    var fillColor= new SolidColor();
                                    fillColor.rgb.red = 0;
                                    fillColor.rgb.green = 0;
                                    fillColor.rgb.blue = 0;
                                    var currentSel= app.activeDocument.selection;
                                    currentSel.selectAll();
                                    currentSel.fill(fillColor);
                                    currentSel.deselect();

                                    compositeLayer.visible = false;
                            }
   

					
						var possibleChannles = [this.TG_R, this.TG_G, this.TG_B, this.TG_ALPHA];
						for each (var j in possibleChannles) 
						{
							if((actualComp.target & j) == false)
								continue;
							
							// vime, ze mame exportit nejaky kanal, takzej e treba si ho najit
							var currentChannels= app.activeDocument.activeChannels;
							var targetChannel= UltimateTextureSaver.GetChannel(this.getChannelName(j));
							var copyLayer= app.activeDocument.artLayers.add();
							if (actualComp.hasMapping && actualComp.mapping[j] != undefined) {
							 // kdyz mame mapping, tak je treba ziskat obsah, jenom z jednoho kanalu.
								var sourceChannel = UltimateTextureSaver.GetChannel(this.getChannelName(actualComp.mapping[j]));
								app.activeDocument.activeChannels = [ sourceChannel ];
							}
							copyLayer.copy(true);
							copyLayer.remove();
							
							compositeLayer.visible = true;
							app.activeDocument.activeLayer = compositeLayer;
							app.activeDocument.activeChannels = [ targetChannel ];
							app.activeDocument.paste();
							app.activeDocument.activeChannels =currentChannels;
							compositeLayer.visible = false;
							
						}
						
						
						break;
					}
				}
			}
			
			if(!saveTexture)
			{
				alert("There is nothing to save. You are probably missing properly named layersets, or they are empty.")
                                UltimateTextureSaver.Finalize();
				return;
			}
			if(colorTarget != undefined)
                            this.makeOnlySetVisible(colorTarget);
                        else {
                            if(compositeLayer == undefined)
                            {
                                   compositeLayer = app.activeDocument.artLayers.add();
                                    var fillColor= new SolidColor();
                                    fillColor.rgb.red = 0;
                                    fillColor.rgb.green = 0;
                                    fillColor.rgb.blue = 0;
                                    var currentSel= app.activeDocument.selection;
                                    currentSel.selectAll();
                                    currentSel.fill(fillColor);
                                    currentSel.deselect();
                            }
                            compositeLayer.visible = true;
                        }
                            
			
			// tady je na case to zacit savovat, ze.. 
			// zakaldne je treba zjistit jmeno souboru, ktery musime sejvnout.
			var psdName= app.activeDocument.name;
                        var pointPos = psdName.lastIndexOf (".");
                        psdName.substr(0, psdName.length - pointPos);
                        var folderPath = UltimateTextureSaver.GetSavePath();
                        var suffix = "\\";
                        if (!(folderPath.indexOf(suffix, folderPath.length - suffix.length) !== -1))
                        {
                            folderPath += "\\";
                        }
                        var tifName= new File(  folderPath + psdName.substr(0, pointPos) + this.suffix+".tif");
                   //    tifName.changePath (UltimateTextureSaver.GetSavePath())
                        if (!tifName.exists)
                            showDialog = true;

                        if(!tifName.readonly) 
                        {
                           this.saveToCryTiff(tifName, showDialog);                
                        }
                        else
                        {
                          alert("Target TIF file is read only");                
                        }


			UltimateTextureSaver.Finalize();
			
		};
		
		this.saveToCryTiff = function (fileName, showDialog){
                        
                        try {
                            var idsave = app.charIDToTypeID( "save" );
                            var desc2= new ActionDescriptor();
                            var idAs= app.charIDToTypeID( "As  " );
                            var desc6 = new ActionDescriptor();
                            var idCmpr= app.charIDToTypeID( "skpD" );
                            desc6.putBoolean( idCmpr, !showDialog );
                            var idCrytekCryTIFPlugin= app.stringIDToTypeID( "Crytek CryTIFPlugin" );			
                            desc2.putObject( idAs, idCrytekCryTIFPlugin, desc6 );    
                            var idIn = app.charIDToTypeID( "In  " );			
                            desc2.putPath( idIn, fileName );
                            var idDocI= app.charIDToTypeID( "DocI" );
                            desc2.putInteger( idDocI, 35 );
                            var idsaveStage= app.stringIDToTypeID( "saveStage" );
                            var idsaveStageType= app.stringIDToTypeID( "saveStageType" );
                            var idsaveBegin= app.stringIDToTypeID( "saveBegin" );
                            desc2.putEnumerated( idsaveStage, idsaveStageType, idsaveBegin );
                            app.executeAction( idsave, desc2, DialogModes.NO ); 
                            return;
                        } catch(err) {
                            
                        }
			try {
                            var idsave = charIDToTypeID( "save" );
                            var desc2 = new ActionDescriptor();
                            var idAs = charIDToTypeID( "As  " );
                            desc2.putString( idAs, """CryTIFPlugin""" );
                            var idIn = charIDToTypeID( "In  " );
                            desc2.putPath( idIn, fileName );
                            var idDocI = charIDToTypeID( "DocI" );
                            desc2.putInteger( idDocI, 35 );
                            var idCpy = charIDToTypeID( "Cpy " );
                            desc2.putBoolean( idCpy, true );
                            var idsaveStage = stringIDToTypeID( "saveStage" );
                            var idsaveStageType = stringIDToTypeID( "saveStageType" );
                            var idsaveBegin = stringIDToTypeID( "saveBegin" );
                            desc2.putEnumerated( idsaveStage, idsaveStageType, idsaveBegin );
                            executeAction( idsave, desc2, DialogModes.NO );        
                        } catch(err) {
                            alert("Unknown error during call to CryTiff plugin. Textures was not saved, sorry.");   
                        }
		};
		    
    
		this.getChannelName = function(channelNumer)
		{
			switch(channelNumer)
			{			
				case this.TG_G:
				{
					return "Green";
					break;
				}
				case this.TG_B:
				{
					return "Blue";
					break;
				}
				case this.TG_ALPHA:
				{
					return "Alpha 1";
					break;
				}		
				default:
				{
					return "Red";
					break;
				}
			}			
			
		};
		
		this.makeOnlySetVisible = function (setName) {
			for ( var a=0 ;  a< app.activeDocument.layerSets.length; a++) {
				if (app.activeDocument.layerSets[a].name == setName) {
					app.activeDocument.layerSets[a].visible = true;
				} else {
					app.activeDocument.layerSets[a].visible = false;
				}
			}    
		};		
		
		this.getConstant = function(channel){
			switch(channel)
			{
				case "r":
				{
					return this.TG_R;
					break;
				}
				case "g":
				{
					return this.TG_G;
					break;
				}
				case "b":
				{
					return this.TG_B;
					break;
				}
				case "a":
					return this.TG_ALPHA;
					break;
				default:
				{
					return 0;
					break;
				}
			}
		
		
		};
    
    
		this._constructor(sourceXml);	
    
    }


var UltimateTextureSaver = {
    Textures: {},
    CurrentHistoryState: undefined,
    RelativePath: "",
    Defaults: [],
    LoadTextureDefinitions: function (configPath) {
            UltimateTextureSaver.Defaults= [];
            var config =File(configPath);
            if(!config.exists)
            {
                alert("File does not exist");
                return;  
            }
              
            config.open('r');
            var configXML = config.read();
            config.close();
            var finalXML = XML (configXML);
            
            for (texture in finalXML.Texture) {
                var texXml = finalXML.Texture[texture];

                var newTexture = new UTSTexture(texXml);     
                UltimateTextureSaver.Textures[newTexture.name] = newTexture;              
                if(texXml.@Default.toString() == "true")
                    UltimateTextureSaver.Defaults.push(newTexture.name);
            }
            UltimateTextureSaver.RelativePath = finalXML.@path.toString();
        } ,
    
		SetExists: function(setName)
		{
			var setExists = true;
			try {
				app.activeDocument.layerSets.getByName (setName)
			} catch(e){
				setExists= false;
			} 
			return setExists;
		},
    	
        CreateLayout: function (textureName)
        {
            if(app.documents.length == 0) {

               alert("No Active document");				
               return;
            }

            UltimateTextureSaver.Textures[textureName].CreateLayout();		
        },
        CreateDefaultLayout: function(){
             if(app.documents.length == 0) {

               alert("No Active document");				
               return;
            }
       
            for (var i in UltimateTextureSaver.Defaults) {
                UltimateTextureSaver.Textures[UltimateTextureSaver.Defaults[i]].CreateLayout();		
                
            }
        },
        GetChannel: function (channelName)
		{
			var alfaChannel;	
			try{
				alfaChannel = app.activeDocument.channels.getByName (channelName)
			} catch(e){
				alfaChannel = app.activeDocument.channels.add();
				alfaChannel.name = channelName;
			}
			return alfaChannel;
		},		
    
		DeleteAlpha : function()
		{
			
			try{
				var alfaChannel= app.activeDocument.channels.getByName ("Alpha 1");
				alfaChannel.remove();
			} catch(e){
			
			}
                        // sometime the channel can be without a number, so this is to remove it also
                        try{
				var alfaChannel= app.activeDocument.channels.getByName ("Alpha");
				alfaChannel.remove();
			} catch(e){
			
			}
			
		},		
    
    
		SaveInternal: function (textureName, showDialog)
		{
		
			app.activeDocument.layerComps.add("beforeSaveLayerComp", "Just backing up layers state before messing up with PSD", true, true, true);
			// cleans PSD before save
			UltimateTextureSaver.DeleteAlpha();
			UltimateTextureSaver.HideAllRootLayers();	
           
			UltimateTextureSaver.Textures[textureName].Save(showDialog);
		},    
         
         Evaluator: function(strToEval) {
                   eval(strToEval   );            
         },
    
		Save: function (textureName, showDialog)
		{
             if(app.documents.length == 0) {
                 alert("No Active document");				
                 return;
             }            
            
            try
            {
                app.activeDocument.path
            } catch (e) {
                 alert("Document has not been saved yet");				
                 return;
             }
			
			
			UltimateTextureSaver.CurrentHistoryState = app.activeDocument.activeHistoryState;
			app.activeDocument.artLayers.add();
			var showDlg = (showDialog ? "true" : "false");
              app.activeDocument.suspendHistory ("Saving "+textureName+ " texture", "UltimateTextureSaver.SaveInternal(\""+textureName+"\","+ showDlg+")");   
		
		},
		
		Finalize: function ()
		{
			var oldLayerComp = app.activeDocument.layerComps.getByName ("beforeSaveLayerComp");
			oldLayerComp.apply();
			oldLayerComp.remove();			
			app.activeDocument.activeHistoryState = UltimateTextureSaver.CurrentHistoryState;
		},
		
		
		HideAllRootLayers: function ()
		{
			for (var a=0 ;  a<  app.activeDocument.artLayers.length; a++) {
				app.activeDocument.artLayers[a].visible = false
			}    		
		
		},
    
        DocumentActive:function() {
                var active = true;
                 if(app.documents.length == 0) 
                 {                    
                    active = false;
                 } else {
                    try {
                            app.activeDocument.path
                    } catch (e) {
                        active = false;
                    }            
                 }
                return active;
            
        },
        APIDocumentActive: function()
        {                
                   var xml = '<object>';
                    if(!UltimateTextureSaver.DocumentActive()) 
                    {                    
                            xml += '<property id="active"><false/></property>';
                    } else {
                            xml += '<property id="active"><true/></property>';
                    }
                    xml += '</object>';
                    return xml;
        },
	ChangeSavePath: function(newDestination) {
            if(newDestination == "")
            {
                  app.activeDocument.info.copyrightNotice = "";
            } else {
                var f = new Folder(newDestination);
                var g = new Folder(app.activeDocument.path.fsName);
                app.activeDocument.info.copyrightNotice = f.getRelativeURI(g);
            }
            
         },
        GetSavePath: function()
        {
                    var testPath = new Folder(app.activeDocument.path.fsName);
                    var defaultPath = "";
                    if(app.activeDocument.info.copyrightNotice == "") {
                        defaultPath = UltimateTextureSaver.RelativePath;
                    } else {
                        defaultPath = app.activeDocument.info.copyrightNotice;
                    }
                    testPath.changePath(defaultPath);
                   return testPath.fsName;
            
        },
        
        APIGetSavePath: function()
        {
                   var xml = '<object>';
                    xml += '<property id="path"><string>'+ UltimateTextureSaver.GetSavePath() +'</string></property>';
                    xml += '</object>';
                    return xml;
        },

        APIGetFolderPath: function()
        {
            var f = new Folder(UltimateTextureSaver.GetSavePath());
            var newFolder = f.selectDlg("Choose destination folder for saving textures");
            var chosen = "<true/>";
            if(!newFolder) {
                 chosen = "<false/>"; 
            }
            var xml = '<object>';
            xml += '<property id="chosen">'+chosen+'</property>';
            if(newFolder) {
                xml += '<property id="path"><string>'+ newFolder.fsName +'</string></property>';
            }             
            xml += '</object>';
            return xml;
        }

        
        
};

//UltimateTextureSaver.LoadTextureDefinitions("j:/RB_code/Tools/photoshop/UltimateTextureSaver/UltimateTextureSaver.xml");
//UltimateTextureSaver.Save("DetailMerged", true);