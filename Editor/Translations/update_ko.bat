@IF "%QTDIR%" == "" (
    echo QTDIR environment variable is not set!
    EXIT /B 1
)

%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\EditorQt                             -ts .\ko\editor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\CryDesigner                  -ts .\ko\CryDesigner.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\DependencyGraph              -ts .\ko\DependencyGraph.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\DialogEditor                 -ts .\ko\DialogEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorAnimation              -ts .\ko\EditorAnimation.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorConsole                -ts .\ko\EditorConsole.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorCSharp                 -ts .\ko\EditorCSharp.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorDynamicResponseSystem  -ts .\ko\EditorDynamicResponseSystem.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorEnvironment            -ts .\ko\EditorEnvironment.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorGameSDK                -ts .\ko\EditorGameSDK.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorParticle               -ts .\ko\EditorParticle.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSchematyc              -ts .\ko\EditorSchematyc.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSchematyc2             -ts .\ko\EditorSchematyc2.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorSubstance              -ts .\ko\EditorSubstance.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\EditorTrackView              -ts .\ko\EditorTrackView.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\FacialEditorPlugin           -ts .\ko\FacialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\FBXPlugin                    -ts .\ko\FBXPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\LodGeneratorPlugin           -ts .\ko\LodGeneratorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MaterialEditorPlugin         -ts .\ko\MaterialEditorPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MeshImporter                 -ts .\ko\MeshImporter.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\MFCToolsPlugin               -ts .\ko\MFCToolsPlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SamplePlugin                 -ts .\ko\SamplePlugin.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SchematycEditor              -ts .\ko\SchematycEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\SmartObjectEditor            -ts .\ko\SmartObjectEditor.ts
%QTDIR%\lupdate -noobsolete ..\..\Code\Sandbox\Plugins\VehicleEditor                -ts .\ko\VehicleEditor.ts