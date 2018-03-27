@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\EditorQt                             -ts .\zh_TW\editor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\CryDesigner                  -ts .\zh_TW\CryDesigner.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\DependencyGraph              -ts .\zh_TW\DependencyGraph.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\DialogEditor                 -ts .\zh_TW\DialogEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorAnimation              -ts .\zh_TW\EditorAnimation.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorConsole                -ts .\zh_TW\EditorConsole.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorCSharp                 -ts .\zh_TW\EditorCSharp.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorDynamicResponseSystem  -ts .\zh_TW\EditorDynamicResponseSystem.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorEnvironment            -ts .\zh_TW\EditorEnvironment.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorGameSDK                -ts .\zh_TW\EditorGameSDK.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorParticle               -ts .\zh_TW\EditorParticle.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSchematyc              -ts .\zh_TW\EditorSchematyc.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSchematyc2             -ts .\zh_TW\EditorSchematyc2.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSubstance              -ts .\zh_TW\EditorSubstance.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorTrackView              -ts .\zh_TW\EditorTrackView.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\FacialEditorPlugin           -ts .\zh_TW\FacialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\FBXPlugin                    -ts .\zh_TW\FBXPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\LodGeneratorPlugin           -ts .\zh_TW\LodGeneratorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MaterialEditorPlugin         -ts .\zh_TW\MaterialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MeshImporter                 -ts .\zh_TW\MeshImporter.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MFCToolsPlugin               -ts .\zh_TW\MFCToolsPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SamplePlugin                 -ts .\zh_TW\SamplePlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SchematycEditor              -ts .\zh_TW\SchematycEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SmartObjectEditor            -ts .\zh_TW\SmartObjectEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\VehicleEditor                -ts .\zh_TW\VehicleEditor.ts