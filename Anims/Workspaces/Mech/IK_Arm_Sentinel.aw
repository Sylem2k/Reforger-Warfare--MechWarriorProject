BaseSource {
 AnimSetTemplate "{1A88BB06EFB65207}Anims/Workspaces/Mech/IK_Arm.ast"
 AnimSetInstances {
  "{E51088DDC47AD41A}Anims/Workspaces/Mech/IK_Arm.asi"
 }
 AnimGraph "{087ADD06D87386F4}Anims/Workspaces/Mech/IK_Arm.agr"
 PreviewModels {
  AnimSrcWorkspacePreviewModel "{60FF2DAF20718368}" {
   Model "{5A53334BFCF3E8F0}Common/Models/Sen_Right_Updated.xob"
   HiddenMaterials {
    "{C14DDB7EF1DB32BC}Common/Models/Data/Sen_Right_Leg.emat"
   }
   AutoEnable {
    "{E51088DDC47AD41A}Anims/Workspaces/Mech/IK_Arm.asi"
   }
  }
 }
 AudioTesting AnimSrcWorkspaceAudioTesting "{60EEB14350250D42}" {
 }
 IkTesting AnimSrcWorkspaceIkTesting "{60EEB14350250DB8}" {
  IkTargets {
   AnimSrcWorkspaceIkTargetTest "{610E8909FC796CEE}" {
    Enabled 1
    BindingName "IK_Target_Mech"
    Position 0 0 0
    Rotation 0 0 0
   }
  }
 }
}