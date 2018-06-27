@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lrelease .\ja\editor.ts
%QTDIR%\lrelease .\ja\CryDesigner.ts
%QTDIR%\lrelease .\ja\DependencyGraph.ts
%QTDIR%\lrelease .\ja\DialogEditor.ts
%QTDIR%\lrelease .\ja\EditorAnimation.ts
%QTDIR%\lrelease .\ja\EditorConsole.ts
%QTDIR%\lrelease .\ja\EditorCSharp.ts
%QTDIR%\lrelease .\ja\EditorDynamicResponseSystem.ts
%QTDIR%\lrelease .\ja\EditorEnvironment.ts
%QTDIR%\lrelease .\ja\EditorGameSDK.ts
%QTDIR%\lrelease .\ja\EditorParticle.ts
%QTDIR%\lrelease .\ja\EditorSchematyc.ts
%QTDIR%\lrelease .\ja\EditorSchematyc2.ts
%QTDIR%\lrelease .\ja\EditorSubstance.ts
%QTDIR%\lrelease .\ja\EditorTrackView.ts
%QTDIR%\lrelease .\ja\FacialEditorPlugin.ts
%QTDIR%\lrelease .\ja\FBXPlugin.ts
%QTDIR%\lrelease .\ja\LodGeneratorPlugin.ts
%QTDIR%\lrelease .\ja\MaterialEditorPlugin.ts
%QTDIR%\lrelease .\ja\MeshImporter.ts
%QTDIR%\lrelease .\ja\MFCToolsPlugin.ts
%QTDIR%\lrelease .\ja\SamplePlugin.ts
%QTDIR%\lrelease .\ja\SchematycEditor.ts
%QTDIR%\lrelease .\ja\SmartObjectEditor.ts
%QTDIR%\lrelease .\ja\VehicleEditor.ts