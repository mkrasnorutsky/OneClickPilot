// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OneClickPilotGameMode.generated.h"
class UStaticMesh;
class ULevelStreamingDynamic;
UCLASS()
class ONECLICKPILOT_API AOneClickPilotGameMode : public AGameModeBase {
    GENERATED_BODY()
public:
    AOneClickPilotGameMode();
    void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    void BeginPlay() override;
    UFUNCTION(BlueprintCallable, Category="OneClickPilot|Editor")
    static bool BakePracticeScenery(UObject* WorldContext);
private:
    void UpdateRuralTiles();
    TMap<FIntPoint, TWeakObjectPtr<ULevelStreamingDynamic>> RuralTiles;
    FTimerHandle RuralTileTimer;
    void BuildPracticeScenery(UWorld* World) const;
    UPROPERTY() TObjectPtr<UStaticMesh> SkyMesh;
};
