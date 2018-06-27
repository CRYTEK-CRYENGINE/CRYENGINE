@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\EditorQt                             -ts .\zh_CN\editor.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\CryDesigner                  -ts .\zh_CN\CryDesigner.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\DependencyGraph              -ts .\zh_CN\DependencyGraph.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\DialogEditor                 -ts .\zh_CN\DialogEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorAnimation              -ts .\zh_CN\EditorAnimation.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorConsole                -ts .\zh_CN\EditorConsole.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorCSharp                 -ts .\zh_CN\EditorCSharp.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorDynamicResponseSystem  -ts .\zh_CN\EditorDynamicResponseSystem.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorEnvironment            -ts .\zh_CN\EditorEnvironment.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorGameSDK                -ts .\zh_CN\EditorGameSDK.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorParticle               -ts .\zh_CN\EditorParticle.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorSchematyc              -ts .\zh_CN\EditorSchematyc.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorSchematyc2             -ts .\zh_CN\EditorSchematyc2.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorSubstance              -ts .\zh_CN\EditorSubstance.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\EditorTrackView              -ts .\zh_CN\EditorTrackView.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\FacialEditorPlugin           -ts .\zh_CN\FacialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\FBXPlugin                    -ts .\zh_CN\FBXPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\LodGeneratorPlugin           -ts .\zh_CN\LodGeneratorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\MaterialEditorPlugin         -ts .\zh_CN\MaterialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\MeshImporter                 -ts .\zh_CN\MeshImporter.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\MFCToolsPlugin               -ts .\zh_CN\MFCToolsPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\SamplePlugin                 -ts .\zh_CN\SamplePlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\SchematycEditor              -ts .\zh_CN\SchematycEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\SmartObjectEditor            -ts .\zh_CN\SmartObjectEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\..\Code\Sandbox\Plugins\VehicleEditor                -ts .\zh_CN\VehicleEditor.ts