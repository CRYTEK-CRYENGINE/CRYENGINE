@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lrelease .\zh_TW\editor.ts
%QTDIR%\lrelease .\zh_TW\CryDesigner.ts
%QTDIR%\lrelease .\zh_TW\DependencyGraph.ts
%QTDIR%\lrelease .\zh_TW\DialogEditor.ts
%QTDIR%\lrelease .\zh_TW\EditorAnimation.ts
%QTDIR%\lrelease .\zh_TW\EditorConsole.ts
%QTDIR%\lrelease .\zh_TW\EditorCSharp.ts
%QTDIR%\lrelease .\zh_TW\EditorDynamicResponseSystem.ts
%QTDIR%\lrelease .\zh_TW\EditorEnvironment.ts
%QTDIR%\lrelease .\zh_TW\EditorGameSDK.ts
%QTDIR%\lrelease .\zh_TW\EditorParticle.ts
%QTDIR%\lrelease .\zh_TW\EditorSchematyc.ts
%QTDIR%\lrelease .\zh_TW\EditorSchematyc2.ts
%QTDIR%\lrelease .\zh_TW\EditorSubstance.ts
%QTDIR%\lrelease .\zh_TW\EditorTrackView.ts
%QTDIR%\lrelease .\zh_TW\FacialEditorPlugin.ts
%QTDIR%\lrelease .\zh_TW\FBXPlugin.ts
%QTDIR%\lrelease .\zh_TW\LodGeneratorPlugin.ts
%QTDIR%\lrelease .\zh_TW\MaterialEditorPlugin.ts
%QTDIR%\lrelease .\zh_TW\MeshImporter.ts
%QTDIR%\lrelease .\zh_TW\MFCToolsPlugin.ts
%QTDIR%\lrelease .\zh_TW\SamplePlugin.ts
%QTDIR%\lrelease .\zh_TW\SchematycEditor.ts
%QTDIR%\lrelease .\zh_TW\SmartObjectEditor.ts
%QTDIR%\lrelease .\zh_TW\VehicleEditor.ts