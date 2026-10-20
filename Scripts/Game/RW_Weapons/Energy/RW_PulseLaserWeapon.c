// MW pulse lasers share the Core fire-rate/ammo/heat pipeline with standard lasers.
// Higher fire rate and lower damage per pulse are configured on the weapon prefab.
[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "Rapid-firing MW pulse laser. Configure shots/minute, heat and damage in the prefab.")]
class RW_PulseLaserWeaponClass : RW_LaserWeaponClass
{
}

class RW_PulseLaserWeapon : RW_LaserWeapon
{
}
