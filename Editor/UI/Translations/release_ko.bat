@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lrelease .\ko\editor.ts
%QTDIR%\lrelease .\ko\CryDesigner.ts
%QTDIR%\lrelease .\ko\DependencyGraph.ts
%QTDIR%\lrelease .\ko\DialogEditor.ts
%QTDIR%\lrelease .\ko\EditorAnimation.ts
%QTDIR%\lrelease .\ko\EditorConsole.ts
%QTDIR%\lrelease .\ko\EditorCSharp.ts
%QTDIR%\lrelease .\ko\EditorDynamicResponseSystem.ts
%QTDIR%\lrelease .\ko\EditorEnvironment.ts
%QTDIR%\lrelease .\ko\EditorGameSDK.ts
%QTDIR%\lrelease .\ko\EditorParticle.ts
%QTDIR%\lrelease .\ko\EditorSchematyc.ts
%QTDIR%\lrelease .\ko\EditorSchematyc2.ts
%QTDIR%\lrelease .\ko\EditorSubstance.ts
%QTDIR%\lrelease .\ko\EditorTrackView.ts
%QTDIR%\lrelease .\ko\FacialEditorPlugin.ts
%QTDIR%\lrelease .\ko\FBXPlugin.ts
%QTDIR%\lrelease .\ko\LodGeneratorPlugin.ts
%QTDIR%\lrelease .\ko\MaterialEditorPlugin.ts
%QTDIR%\lrelease .\ko\MeshImporter.ts
%QTDIR%\lrelease .\ko\MFCToolsPlugin.ts
%QTDIR%\lrelease .\ko\SamplePlugin.ts
%QTDIR%\lrelease .\ko\SchematycEditor.ts
%QTDIR%\lrelease .\ko\SmartObjectEditor.ts
%QTDIR%\lrelease .\ko\VehicleEditor.ts