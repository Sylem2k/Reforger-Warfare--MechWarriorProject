[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "Ultra autocannon: two consecutive shells per trigger cycle, consuming two rounds.")]
class RW_UltraAutocannonClass : BFA_MechAutocannonModuleComponentClass
{
}
class RW_UltraAutocannon : BFA_MechAutocannonModuleComponent
{
    override protected void FireShot(IEntity instigator)
    {
        // Core has already consumed the first round and accounted for its heat.
        super.FireShot(instigator);
        // Second shot is permitted only when an additional round can be consumed.
        // Using the Core gate also preserves ammo, reload and heat hooks.
        if (ConsumeRound())
            super.FireShot(instigator);
    }
}
