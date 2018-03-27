@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\EditorQt                             -ts .\ja\editor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\CryDesigner                  -ts .\ja\CryDesigner.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\DependencyGraph              -ts .\ja\DependencyGraph.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\DialogEditor                 -ts .\ja\DialogEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorAnimation              -ts .\ja\EditorAnimation.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorConsole                -ts .\ja\EditorConsole.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorCSharp                 -ts .\ja\EditorCSharp.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorDynamicResponseSystem  -ts .\ja\EditorDynamicResponseSystem.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorEnvironment            -ts .\ja\EditorEnvironment.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorGameSDK                -ts .\ja\EditorGameSDK.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorParticle               -ts .\ja\EditorParticle.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSchematyc              -ts .\ja\EditorSchematyc.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSchematyc2             -ts .\ja\EditorSchematyc2.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSubstance              -ts .\ja\EditorSubstance.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorTrackView              -ts .\ja\EditorTrackView.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\FacialEditorPlugin           -ts .\ja\FacialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\FBXPlugin                    -ts .\ja\FBXPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\LodGeneratorPlugin           -ts .\ja\LodGeneratorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MaterialEditorPlugin         -ts .\ja\MaterialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MeshImporter                 -ts .\ja\MeshImporter.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MFCToolsPlugin               -ts .\ja\MFCToolsPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SamplePlugin                 -ts .\ja\SamplePlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SchematycEditor              -ts .\ja\SchematycEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SmartObjectEditor            -ts .\ja\SmartObjectEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\VehicleEditor                -ts .\ja\VehicleEditor.ts