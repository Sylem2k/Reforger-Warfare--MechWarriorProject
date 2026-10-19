BaseSource {
 AnimSetTemplate "{2638477A226BE56E}Anims/Workspaces/Mech/IK_Titan_Arm.ast"
 AnimSetInstances {
  "{A9F566D32EEE75E4}Anims/Workspaces/Mech/IK_Titan_Arm.asi"
 }
 AnimGraph "{F8BD36FA8416C4A6}Anims/Workspaces/Mech/IK_Titan_Arm.agr"
 PreviewModels {
  AnimSrcWorkspacePreviewModel "{60FF2DAF20718368}" {
   Model "{2E0A7C1F57556FC1}Common/Models/Titan/Titan_Left_Arm_Updated.xob"
   AutoEnable {
    "{A9F566D32EEE75E4}Anims/Workspaces/Mech/IK_Titan_Arm.asi"
   }
  }
 }
 AudioTesting AnimSrcWorkspaceAudioTesting "{60EEB14350250D42}" {
 }
 IkTesting AnimSrcWorkspaceIkTesting "{60EEB14350250DB8}" {
  IkTargets {
   AnimSrcWorkspaceIkTargetTest "{610E8909FC796CEE}" {
    Enabled 1
    BindingName "IK_Target_Arm"
    Position 0 0 0
    Rotation 0 0 0
   }
  }
 }
}