// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include <memory>
#include "Core/FlightSession.h"
#include "OneClickPilotPawn.generated.h"
class UCameraComponent;
class UStaticMeshComponent;
class USpotLightComponent;
UCLASS()
class ONECLICKPILOT_API AOneClickPilotPawn : public APawn {
    GENERATED_BODY()
public:
    AOneClickPilotPawn();
    void BeginPlay() override;
    void Tick(float Seconds) override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(VisibleAnywhere, Category="Cockpit") TObjectPtr<UCameraComponent> CockpitCamera;
    FRotator BaseCockpitRotation = FRotator::ZeroRotator;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Needles;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HeadingBug;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FlapIndicator;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> AlternatorWarning;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FuelBars;
    UPROPERTY() TObjectPtr<USpotLightComponent> LandingLight;
    UPROPERTY() TObjectPtr<USpotLightComponent> TaxiLight;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftNavLight;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightNavLight;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> BeaconLight;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StrobeLights;

    void UpdateAircraftLights();
    std::unique_ptr<ocp::FlightSession> Flight;
    void* FlightLibrary=nullptr;
    uint64 DiagnosticFrames=0;
    double NextInstructorTraceTime = 0.0;
    bool bDiagnostics=false;
};
