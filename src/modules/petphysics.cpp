#include "petphysics.h"

#include <QtMath>
#include <QRandomGenerator>

void PetPhysics::applyGravity(int &posY)
{
    // ZH: 空氣墊子停頓中 — 凍結不下落 | EN: during air-cushion pause — frozen, no falling
    if (landPause > 0)
    {
        landPause--;
        return;
    }
    if (!isGrounded)
        velocityY += gravity;
    posY += static_cast<int>(velocityY);
}

bool PetPhysics::resolveGroundCollision(int &posY, int petHeight, const QRect &screenRect)
{
    bool wasGrounded = isGrounded;
    int groundY = screenRect.bottom() - petHeight;

    double dist = groundY - posY;

    // ZH: 落地前緩速 — 進入螢幕底算 20% 高度即開始；弧形反曲線：高處幾乎不減速、減速集中在接近地面才急遽增加。
    //     以「接近地面程度 c」的平方計算 → 上段平緩、近地陡升。停頓前才減速；空氣墊停頓後恢復正常重力。
    // EN: ease within bottom 20%; arc curve — almost no decel high up, decel concentrated near the ground (closeness²)
    double landEaseDist = screenRect.height() * landEaseRatio;
    if (!landPaused && velocityY > 0.0 && dist > 0.0 && dist < landEaseDist)
    {
        double c = 1.0 - (dist / landEaseDist);   // ZH: 0=區域頂端(高) 1=接近地面(低) | EN: 0 = top (high), 1 = near ground (low)
        double factor = 1.0 - (1.0 - landEaseMinKeep) * (c * c);  // ZH: c² → 近地才急遽減速 | EN: c² → sharp decel only near ground
        velocityY *= factor;
    }

    // ZH: 空氣墊子 — 快觸地時先停頓一下 (速度歸零)，再完全落下 | EN: air cushion — brief pause (v=0) just before landing
    if (!isGrounded && !landPaused && landPause == 0 && velocityY > 0.0 && dist > 0.0 && dist < landPauseTriggerDist)
    {
        velocityY = 0.0;
        landPause = landPauseDuration;
        landPaused = true;
    }

    if (posY >= groundY)
    {
        // ZH: 直接落地，不彈跳 | EN: settle on the ground, no bounce
        posY = groundY;
        velocityY = 0;
        isGrounded = true;
        landPaused = false;   // ZH: 落地完成，重置供下次下落 | EN: landed — reset for next descent
    }
    else
    {
        isGrounded = false;
    }

    // ZH: 由空中轉為落地的瞬間視為觸地事件 | EN: Transition airborne -> grounded is a landing event
    return !wasGrounded && isGrounded;
}

bool PetPhysics::resolveBoundaryCollision(int &posX, int petWidth, const QRect &screenRect)
{
    int leftWall  = screenRect.left();
    int rightWall = screenRect.right() - petWidth;

    if (posX <= leftWall || posX >= rightWall)
    {
        posX = qBound(leftWall, posX, rightWall);
        currentVelocityX *= wallBounceFactor;
        targetVelocityX  *= wallBounceFactor;
        return true;
    }
    return false;
}

void PetPhysics::updateWalkVelocity(bool hasSteps)
{
    if (hasSteps)
    {
        if (currentVelocityX < targetVelocityX)
            currentVelocityX = qMin(targetVelocityX, currentVelocityX + acceleration);
        else if (currentVelocityX > targetVelocityX)
            currentVelocityX = qMax(targetVelocityX, currentVelocityX - acceleration);
    }
    else
    {
        if (currentVelocityX > 0)
            currentVelocityX = qMax(0.0, currentVelocityX - friction);
        else if (currentVelocityX < 0)
            currentVelocityX = qMin(0.0, currentVelocityX + friction);
    }
}

int PetPhysics::calcHoverY()
{
    hoverPhase += hoverSpeed;
    if (hoverPhase > 2 * M_PI)
        hoverPhase -= 2 * M_PI;
    return hoverBaseY + static_cast<int>(qSin(hoverPhase) * hoverAmplitude);
}

PetPhysics::FlyStep PetPhysics::calcFlyStep(int posX, int posY)
{
    double dx   = flyTargetX - posX;
    double dy   = flyTargetY - posY;
    double dist = qSqrt(dx * dx + dy * dy);

    if (dist < 5.0)
        return { posX, posY, true };

    // ZH: 飛行單純朝目標直線移動 (不套 Sinwave；Sinwave 僅用於懸浮原地上下浮動)
    // EN: fly straight toward target (no sinwave; sinwave is only for in-place hovering)
    return {
        posX + static_cast<int>((dx / dist) * flySpeed),
        posY + static_cast<int>((dy / dist) * flySpeed),
        false
    };
}

void PetPhysics::initHover(int currentY)
{
    hoverBaseY       = currentY;
    hoverPhase       = 0.0;
    velocityY        = 0;
    currentVelocityX = 0;
    isGrounded       = false;
    landPause = 0; landPaused = false;   // ZH: 離地，重置空氣墊子 | EN: leaving ground — reset air cushion
}

void PetPhysics::initFly(const QRect &screenRect, int petWidth, int petHeight)
{
    flyTargetX = QRandomGenerator::global()->bounded(screenRect.left(), screenRect.right() - petWidth);
    flyTargetY = QRandomGenerator::global()->bounded(screenRect.top() + 50, screenRect.bottom() - petHeight - 100);
    velocityY  = 0;
    isGrounded = false;
    landPause = 0; landPaused = false;
}

void PetPhysics::resetVelocities()
{
    velocityY        = 0;
    currentVelocityX = 0;
    isGrounded       = false;
    landPause = 0; landPaused = false;
}
