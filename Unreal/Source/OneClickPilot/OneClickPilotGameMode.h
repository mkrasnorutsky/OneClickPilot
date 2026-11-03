// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OneClickPilotGameMode.generated.h"
class UStaticMesh;
UCLASS()
class ONECLICKPILOT_API AOneClickPilotGameMode : public AGameModeBase {
    GENERATED_BODY()
public:
    AOneClickPilotGameMode();
    void BeginPlay() override;
private:
    UPROPERTY() TObjectPtr<UStaticMesh> SkyMesh;
};
