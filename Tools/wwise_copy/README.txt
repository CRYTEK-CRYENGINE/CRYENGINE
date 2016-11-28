Wwise SDK installation

To compile Wwise dll support you must copy Wwise SDK folder to 
Code\SDKs\Audio\wwise\SDK folder.
Just drag & drop your "C:\Program Files (x86)\Audiokinetic\Wwise 2016.2.0.5972"
folder over dragndrop_wwise_here.bat to copy the needed files.

The version will depends on which CRYENGINE engine version you are using.
To check which wwise version you are running, use the next console command in 
the editor or game console:

    s_DrawAudioDebug a
	
Use the same version accordingly to compile your own wwise dll.
Remember that you must run cry_waf or cmake again to detect wwise sources and
add the project to your visual studio solution.
Also your banks should be generated for the exact same wwise version. You can
check which wwise version you are using via wwise launcher.