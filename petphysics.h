#ifndef PETPHYSICS_H
#define PETPHYSICS_H

#include <QRect>

// ZH: 物理引擎模組，負責重力、碰撞偵測與各移動狀態的位置計算
// EN: Physics engine module — gravity, collision detection, position updates
class PetPhysics
{
public:
    // ZH: 可調整參數 | EN: Adjustable parameters
    double gravity = 0.8;

    // ZH: 物理狀態 | EN: Physics state
    double velocityY = 0;
    bool isGrounded = false;
    double currentVelocityX = 0;
    double targetVelocityX = 0;

    // ZH: 物理常數 | EN: Physics constants
    const double bounceFactor    = -0.5;
    const double wallBounceFactor = -1.0;
    const double acceleration    =  0.2;
    const double friction        =  0.15;

    // ZH: 浮空 (Hovering) 狀態 | EN: Hovering state
    double hoverPhase = 0.0;
    int    hoverBaseY = 0;
    const double hoverAmplitude = 8.0;
    const double hoverSpeed     = 0.08;

    // ZH: 飛行 (Flying) 狀態 | EN: Flying state
    int flyTargetX = 0;
    int flyTargetY = 0;
    const double flySpeed = 1.5;

    // ZH: 重力計算，修改 posY | EN: Apply gravity, modifies posY
    void applyGravity(int &posY);

    // ZH: 地面碰撞，修改 posY / velocityY / isGrounded | EN: Ground collision, modifies posY / velocityY / isGrounded
    void resolveGroundCollision(int &posY, int petHeight, const QRect &screenRect);

    // ZH: 左右邊界碰撞，修改 posX / velocityX | EN: Boundary collision, modifies posX / velocityX
    void resolveBoundaryCollision(int &posX, int petWidth, const QRect &screenRect);

    // ZH: 行走速度計算（加速 or 摩擦減速）| EN: Walk velocity update (accelerate or friction)
    void updateWalkVelocity(bool hasSteps);

    // ZH: 浮空位置計算，回傳新 Y | EN: Hover position, returns new Y
    int calcHoverY();

    // ZH: 飛行步進結果 | EN: Fly step result
    struct FlyStep { int newX; int newY; bool arrived; };
    FlyStep calcFlyStep(int posX, int posY);

    // ZH: 初始化浮空基準 Y | EN: Initialise hover base Y
    void initHover(int currentY);

    // ZH: 初始化飛行目標位置 | EN: Initialise fly target position
    void initFly(const QRect &screenRect, int petWidth, int petHeight);

    // ZH: 重置速度（落地後、切換狀態時使用）| EN: Reset velocities
    void resetVelocities();
};

#endif // PETPHYSICS_H
