#include "RGTypes.h"

URGClassCatalog::URGClassCatalog()
{
    for (int32 I = 0; I < 7; ++I)
    {
        FRGClassDefinition D;
        D.ClassID = static_cast<ERGClass>(I);
        D.DisplayName = FText::FromString(RG::ClassName(D.ClassID));
        switch (D.ClassID)
        {
        case ERGClass::Worker:
            D.MaxHealth = 120;
            D.AttackDamage = 20;
            D.MoveSpeed = 575;
            break;
        case ERGClass::Knight:
            D.MaxHealth = 180;
            D.MoveSpeed = 520;
            D.AttackDamage = 32;
            D.AttackRate = .65f;
            D.WoodCost = 20;
            D.OreCost = 40;
            break;
        case ERGClass::Priest:
            D.MaxHealth = 105;
            D.AttackDamage = 19;
            D.AttackRange = 850;
            D.WoodCost = 40;
            D.OreCost = 20;
            break;
        case ERGClass::Mage:
            D.MaxHealth = 90;
            D.AttackDamage = 34;
            D.AttackRange = 1450;
            D.AttackRate = .85f;
            break;
        case ERGClass::Archer:
            D.MaxHealth = 100;
            D.AttackDamage = 26;
            D.AttackRange = 1900;
            D.AttackRate = .7f;
            D.WoodCost = 25;
            D.OreCost = 35;
            break;
        case ERGClass::Ninja:
            D.MaxHealth = 85;
            D.AttackDamage = 21;
            D.AttackRate = .35f;
            D.MoveSpeed = 700;
            D.AbilityCooldown = 4;
            break;
        default:
            break;
        }
        Classes.Add(D);
    }
}
const FRGClassDefinition& URGClassCatalog::Get(ERGClass Class) const
{
    for (const FRGClassDefinition& D : Classes)
        if (D.ClassID == Class)
            return D;
    check(Classes.Num());
    return Classes[0];
}
