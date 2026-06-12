#include "petphysics.h"

#include <QtMath>
#include <QRandomGenerator>

void PetPhysics::applyGravity(int &posY)
{
    if (!isGrounded)
        velocityY += gravity;
    posY += static_cast<int>(velocityY);
}

bool PetPhysics::resolveGroundCollision(int &posY, int petHeight, const QRect &screenRect)
{
    bool wasGrounded = isGrounded;
    int groundY = screenRect.bottom() - petHeight;

    if (posY >= groundY)
    {
        if (qAbs(velocityY) > 1.5)
        {
            velocityY *= bounceFactor;
            posY = groundY - 1;     // ZH: 稍微抬起，避免卡地板 | EN: Lift slightly to avoid floor clipping
        }
        else
        {
            posY = groundY;
            velocityY = 0;
            isGrounded = true;
        }
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
}

void PetPhysics::initFly(const QRect &screenRect, int petWidth, int petHeight)
{
    flyTargetX = QRandomGenerator::global()->bounded(screenRect.left(), screenRect.right() - petWidth);
    flyTargetY = QRandomGenerator::global()->bounded(screenRect.top() + 50, screenRect.bottom() - petHeight - 100);
    velocityY  = 0;
    isGrounded = false;
}

void PetPhysics::resetVelocities()
{
    velocityY        = 0;
    currentVelocityX = 0;
    isGrounded       = false;
}
