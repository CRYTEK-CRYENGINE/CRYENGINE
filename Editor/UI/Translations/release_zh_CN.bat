@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lrelease .\zh_CN\editor.ts
%QTDIR%\lrelease .\zh_CN\CryDesigner.ts
%QTDIR%\lrelease .\zh_CN\DependencyGraph.ts
%QTDIR%\lrelease .\zh_CN\DialogEditor.ts
%QTDIR%\lrelease .\zh_CN\EditorAnimation.ts
%QTDIR%\lrelease .\zh_CN\EditorConsole.ts
%QTDIR%\lrelease .\zh_CN\EditorCSharp.ts
%QTDIR%\lrelease .\zh_CN\EditorDynamicResponseSystem.ts
%QTDIR%\lrelease .\zh_CN\EditorEnvironment.ts
%QTDIR%\lrelease .\zh_CN\EditorGameSDK.ts
%QTDIR%\lrelease .\zh_CN\EditorParticle.ts
%QTDIR%\lrelease .\zh_CN\EditorSchematyc.ts
%QTDIR%\lrelease .\zh_CN\EditorSchematyc2.ts
%QTDIR%\lrelease .\zh_CN\EditorSubstance.ts
%QTDIR%\lrelease .\zh_CN\EditorTrackView.ts
%QTDIR%\lrelease .\zh_CN\FacialEditorPlugin.ts
%QTDIR%\lrelease .\zh_CN\FBXPlugin.ts
%QTDIR%\lrelease .\zh_CN\LodGeneratorPlugin.ts
%QTDIR%\lrelease .\zh_CN\MaterialEditorPlugin.ts
%QTDIR%\lrelease .\zh_CN\MeshImporter.ts
%QTDIR%\lrelease .\zh_CN\MFCToolsPlugin.ts
%QTDIR%\lrelease .\zh_CN\SamplePlugin.ts
%QTDIR%\lrelease .\zh_CN\SchematycEditor.ts
%QTDIR%\lrelease .\zh_CN\SmartObjectEditor.ts
%QTDIR%\lrelease .\zh_CN\VehicleEditor.ts