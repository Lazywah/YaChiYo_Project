#ifndef PETBEHAVIOR_H
#define PETBEHAVIOR_H

// ZH: 行為 AI 模組，負責隨機決策邏輯（不含計時器，計時器由 MainWindow 管理）
// EN: Behavior AI module — random decision logic (timer is owned by MainWindow)
class PetBehavior
{
public:
    // ZH: 可調整參數 | EN: Adjustable parameters
    double walkSpeed = 2.0;

    // ZH: 最後一次的隨機決策值（供 Developer 監控顯示）| EN: Last action roll (for developer monitor)
    int lastActionRoll = 0;

    // ZH: 決策結果 | EN: Decision result
    struct Decision
    {
        enum Action { Walk, Fly, Stand, HoverStay, HoverFly, HoverLand } action = Stand;
        double targetVelocityX = 0;
        int    walkSteps       = 0;
    };

    // ZH: 地面狀態決策（需傳入當前位置比例 0.0=最左 ~ 1.0=最右）
    // EN: Ground decision (pass position ratio: 0.0=leftmost ~ 1.0=rightmost)
    Decision decideOnGround(double positionRatio);

    // ZH: 空中狀態決策（Hovering / Flying）| EN: Airborne decision (Hovering / Flying)
    Decision decideInAir();
};

#endif // PETBEHAVIOR_H
