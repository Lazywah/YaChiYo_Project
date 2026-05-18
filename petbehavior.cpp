#include "petbehavior.h"

#include <QRandomGenerator>

PetBehavior::Decision PetBehavior::decideOnGround(double positionRatio)
{
    Decision d;
    lastActionRoll = QRandomGenerator::global()->bounded(100);

    if (lastActionRoll < 50)            // ZH: 50% 散步 | EN: 50% walk
    {
        double rightProb = 1.0 - positionRatio;     // ZH: 越靠右越傾向往左走 | EN: Tend left when near right edge
        double dirRoll   = QRandomGenerator::global()->generateDouble();
        int direction    = (dirRoll < rightProb) ? 1 : -1;

        d.action          = Decision::Walk;
        d.targetVelocityX = direction * walkSpeed;
        d.walkSteps       = QRandomGenerator::global()->bounded(120) + 90;  // ZH: 90~210 步 | EN: 90~210 steps
    }
    else if (lastActionRoll < 60)       // ZH: 10% 起飛 | EN: 10% take off
    {
        d.action = Decision::Fly;
    }
    else                                // ZH: 40% 站定 | EN: 40% stand still
    {
        d.action = Decision::Stand;
    }

    return d;
}

PetBehavior::Decision PetBehavior::decideInAir()
{
    Decision d;
    lastActionRoll = QRandomGenerator::global()->bounded(100);

    if (lastActionRoll < 50)            // ZH: 50% 繼續浮空 | EN: 50% keep hovering
        d.action = Decision::HoverStay;
    else if (lastActionRoll < 80)       // ZH: 30% 飛去新位置 | EN: 30% fly to new position
        d.action = Decision::HoverFly;
    else                                // ZH: 20% 落地 | EN: 20% land
        d.action = Decision::HoverLand;

    return d;
}
