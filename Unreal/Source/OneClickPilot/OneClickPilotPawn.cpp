// SPDX-License-Identifier: MIT
#include "OneClickPilotPawn.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include <cmath>
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FlightDiagnostics.h"
#include "UnrealClient.h"
AOneClickPilotPawn::AOneClickPilotPawn(){
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Airframe"));
    CockpitCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
    CockpitCamera->SetupAttachment(RootComponent);CockpitCamera->SetRelativeLocation(FVector(-50,-20,80));CockpitCamera->FieldOfView=75;
    CockpitCamera->SetRelativeRotation(FRotator(-5, 0, 0));
    AutoPossessPlayer=EAutoReceiveInput::Player0;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PanelMaterial(TEXT("/Game/Materials/M_Panel.M_Panel"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeedleMaterial(TEXT("/Game/Materials/M_White.M_White"));
    auto Panel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));Panel->SetupAttachment(RootComponent);Panel->SetStaticMesh(Cube.Object);Panel->SetMaterial(0,PanelMaterial.Object);Panel->SetRelativeLocation(FVector(65,0,35));Panel->SetRelativeScale3D(FVector(.08,1.1,.5));Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for(int i=0;i<9;++i){
        const double GaugeY = i >= 6 ? 41 - 19*(i-6) : (i%3-1)*22;
        const double GaugeZ = i >= 6 ? 15 : 25+(i/3)*23;
        auto Gauge=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Gauge%d"),i));Gauge->SetupAttachment(RootComponent);Gauge->SetStaticMesh(Cylinder.Object);Gauge->SetMaterial(0,NeedleMaterial.Object);Gauge->SetRelativeLocation(FVector(60,GaugeY,GaugeZ));Gauge->SetRelativeRotation(FRotator(90,0,0));Gauge->SetRelativeScale3D(FVector(.16,.16,.008));Gauge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        auto Needle=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Needle%d"),i));Needle->SetupAttachment(RootComponent);Needle->SetStaticMesh(Cube.Object);Needle->SetMaterial(0,PanelMaterial.Object);Needle->SetRelativeLocation(FVector(59,GaugeY,GaugeZ));Needle->SetRelativeScale3D(FVector(.008,.12,.008));Needle->SetCollisionEnabled(ECollisionEnabled::NoCollision);Needles.Add(Needle);
    }
    // Put reference marks at descent, level flight, and climb.
    for (int Mark = -1; Mark <= 1; ++Mark) {
        auto* TickMark = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("VerticalSpeedMark%d"), Mark + 1));
        TickMark->SetupAttachment(RootComponent);
        TickMark->SetStaticMesh(Cube.Object);
        TickMark->SetMaterial(0, PanelMaterial.Object);
        const FRotator Angle(0, 0, 90 * Mark);
        TickMark->SetRelativeLocation(FVector(59, 0, 48) + Angle.RotateVector(FVector(0, -6.5, 0)));
        TickMark->SetRelativeRotation(Angle);
        TickMark->SetRelativeScale3D(FVector(.008, .018, .006));
        TickMark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    Needles[4]->SetRelativeScale3D(FVector(.008, .05, .008));

    auto* AttitudeReference = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttitudeReference"));
    AttitudeReference->SetupAttachment(RootComponent);
    AttitudeReference->SetStaticMesh(Cube.Object);
    AttitudeReference->SetMaterial(0, PanelMaterial.Object);
    AttitudeReference->SetRelativeLocation(FVector(58, 0, 25));
    AttitudeReference->SetRelativeScale3D(FVector(.008, .025, .025));
    AttitudeReference->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadingBug = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadingBug"));
    HeadingBug->SetupAttachment(RootComponent);
    HeadingBug->SetStaticMesh(Cube.Object);
    HeadingBug->SetMaterial(0, PanelMaterial.Object);
    HeadingBug->SetRelativeScale3D(FVector(.01, .025, .025));
    HeadingBug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* FlapTrack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlapTrack"));
    FlapTrack->SetupAttachment(RootComponent);
    FlapTrack->SetStaticMesh(Cube.Object);
    FlapTrack->SetMaterial(0, NeedleMaterial.Object);
    FlapTrack->SetRelativeLocation(FVector(59, 41, 37));
    FlapTrack->SetRelativeScale3D(FVector(.01, .05, .24));
    FlapTrack->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FlapIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlapIndicator"));
    FlapIndicator->SetupAttachment(RootComponent);
    FlapIndicator->SetStaticMesh(Cube.Object);
    FlapIndicator->SetMaterial(0, PanelMaterial.Object);
    FlapIndicator->SetRelativeScale3D(FVector(.01, .04, .025));
    FlapIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for (int Tank = 0; Tank < 2; ++Tank) {
        auto* Bar = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("FuelBar%d"), Tank));
        Bar->SetupAttachment(RootComponent);
        Bar->SetStaticMesh(Cube.Object);
        Bar->SetMaterial(0, NeedleMaterial.Object);
        Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FuelBars.Add(Bar);
    }
    AlternatorWarning = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AlternatorWarning"));
    AlternatorWarning->SetupAttachment(RootComponent);
    AlternatorWarning->SetStaticMesh(Cube.Object);
    AlternatorWarning->SetMaterial(0, NeedleMaterial.Object);
    AlternatorWarning->SetRelativeLocation(FVector(59, 30, 54));
    AlternatorWarning->SetRelativeScale3D(FVector(.015, .06, .03));
    AlternatorWarning->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AlternatorWarning->SetVisibility(false);
    auto MakeBeam = [this](const TCHAR* Name, FVector Location, float Pitch,
                           float Intensity, float Range, float Cone) {
        auto* Beam = CreateDefaultSubobject<USpotLightComponent>(Name);
        Beam->SetupAttachment(RootComponent);
        Beam->SetMobility(EComponentMobility::Movable);
        Beam->SetRelativeLocation(Location);
        Beam->SetRelativeRotation(FRotator(Pitch, 0, 0));
        Beam->SetIntensityUnits(ELightUnits::Lumens);
        Beam->SetIntensity(Intensity);
        Beam->SetAttenuationRadius(Range);
        Beam->SetOuterConeAngle(Cone);
        Beam->CastShadows = false;
        Beam->SetVisibility(false);
        return Beam;
    };
    LandingLight = MakeBeam(TEXT("LandingLight"), FVector(100, 0, 0), -4, 30000, 16000, 12);
    TaxiLight = MakeBeam(TEXT("TaxiLight"), FVector(100, 0, -10), -12, 9000, 6500, 28);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> LampMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto MakeLamp = [&](const TCHAR* Name, FVector Location) {
        auto* Lamp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Lamp->SetupAttachment(RootComponent);
        Lamp->SetStaticMesh(LampMesh.Object);
        Lamp->SetRelativeLocation(Location);
        Lamp->SetRelativeScale3D(FVector(.065));
        Lamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Lamp->SetCastShadow(false);
        Lamp->SetVisibility(false);
        return Lamp;
    };
    LeftNavLight = MakeLamp(TEXT("LeftNavLight"), FVector(-25, -548, 95));
    RightNavLight = MakeLamp(TEXT("RightNavLight"), FVector(-25, 548, 95));
    BeaconLight = MakeLamp(TEXT("BeaconLight"), FVector(-430, 0, 155));
    StrobeLights.Add(MakeLamp(TEXT("LeftStrobe"), FVector(-32, -548, 95)));
    StrobeLights.Add(MakeLamp(TEXT("RightStrobe"), FVector(-32, 548, 95)));

}
void AOneClickPilotPawn::BeginPlay(){
    Super::BeginPlay();
    BaseCockpitRotation = CockpitCamera->GetRelativeRotation();
    const auto* AirframeMesh = FindComponentByClass<USkeletalMeshComponent>();
    const bool bImportedCockpit = AirframeMesh &&
        AirframeMesh->ComponentHasTag(TEXT("OneClickPilot.ImportedCockpit"));
    if (bImportedCockpit)
        CockpitCamera->SetRelativeLocation(FVector(-43, -27, 61));
    if (bImportedCockpit) {
        TArray<UStaticMeshComponent*> Meshes;
        GetComponents(Meshes);
        for (auto* Mesh : Meshes) {
            const FString Name = Mesh->GetName();
            if (Name.StartsWith(TEXT("Panel")) || Name.StartsWith(TEXT("Gauge")) ||
                Name.StartsWith(TEXT("Needle")) || Name.StartsWith(TEXT("VerticalSpeedMark")) ||
                Name.StartsWith(TEXT("AttitudeReference")) || Name.StartsWith(TEXT("HeadingBug")) ||
                Name.StartsWith(TEXT("Flap")) || Name.StartsWith(TEXT("FuelBar")) ||
                Name.StartsWith(TEXT("AlternatorWarning")))
                Mesh->SetHiddenInGame(true);
        }
    }
    auto* LampMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AircraftLamp.M_AircraftLamp"));
    if (LampMaterial) {
        auto SetLampColor = [this, LampMaterial](UStaticMeshComponent* Lamp, FLinearColor Color) {
            auto* Instance = UMaterialInstanceDynamic::Create(LampMaterial, this);
            Instance->SetVectorParameterValue(TEXT("LampColor"), Color);
            Lamp->SetMaterial(0, Instance);
        };
        SetLampColor(LeftNavLight, FLinearColor(4, .02f, .02f));
        SetLampColor(RightNavLight, FLinearColor(.02f, 4, .02f));
        SetLampColor(BeaconLight, FLinearColor(6, .02f, .02f));
        for (UStaticMeshComponent* Lamp : StrobeLights) SetLampColor(Lamp, FLinearColor(6, 6, 6));
    }

    bDiagnostics=FParse::Param(FCommandLine::Get(),TEXT("OneClickDiagnostics"));
    ocp::FlightAPI API;
#if PLATFORM_IOS
    // UE's generic GetDllHandle is intentionally unimplemented on iOS.
    // These C imports link to the separately embedded dynamic framework.
    API={ocp_fdm_abi,ocp_fdm_create,ocp_fdm_destroy,ocp_fdm_step,ocp_fdm_set,ocp_fdm_get,ocp_fdm_state,ocp_fdm_error};
#else
    const FString Library=FPaths::Combine(FPaths::ProjectDir(),TEXT("ThirdParty/Mac/OneClickFlightDynamics.framework/OneClickFlightDynamics"));
    FlightLibrary=FPlatformProcess::GetDllHandle(*Library);
    if(!FlightLibrary){UE_LOG(LogTemp,Error,TEXT("Cannot load flight dynamics framework: %s"),*Library);return;}
#define OCP_LOAD(Member) API.Member=reinterpret_cast<decltype(API.Member)>(FPlatformProcess::GetDllExport(FlightLibrary,TEXT("ocp_fdm_" #Member)))
    OCP_LOAD(abi);OCP_LOAD(create);OCP_LOAD(destroy);OCP_LOAD(step);OCP_LOAD(set);OCP_LOAD(get);OCP_LOAD(state);OCP_LOAD(error);
#undef OCP_LOAD
#endif
    const FString Resources=IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("FlightModels")));
    try{Flight=std::make_unique<ocp::FlightSession>(API);Flight->start(TCHAR_TO_UTF8(*Resources));}
    catch(const std::exception& Error){UE_LOG(LogTemp,Error,TEXT("Flight startup: %s"),UTF8_TO_TCHAR(Error.what()));Flight.reset();}
}
void AOneClickPilotPawn::Tick(float Seconds){
    Super::Tick(Seconds);if(!Flight)return;
    try{Flight->tick(Seconds);}catch(const std::exception& Error){UE_LOG(LogTemp,Error,TEXT("Flight step: %s"),UTF8_TO_TCHAR(Error.what()));Flight.reset();return;}
    UpdateAircraftLights();
    const auto& S=Flight->state;
    // Local tangent-plane approximation for the initial flat practice scenery.
    constexpr double Radians=3.14159265358979323846/180;
    const double North=(S.latitude_deg-50.83138094)*Radians*6378137.0;
    const double East=(S.longitude_deg+.29991299)*Radians*6378137.0*std::cos(50.83138094*Radians);
    SetActorLocationAndRotation(FVector(North*100,East*100,(S.altitude_ft-7)*30.48),FRotator(S.pitch_deg,S.heading_deg,S.roll_deg));
    auto& P=Flight->properties;
#if !UE_BUILD_SHIPPING
    const auto Phase = Flight->instructorPhase();
    if (Phase != ocp::CircuitInstructor::Phase::WaitingForBrakeRelease &&
        S.elapsed_seconds >= NextInstructorTraceTime) {
        NextInstructorTraceTime = S.elapsed_seconds + .5;
        UE_LOG(LogTemp, Display, TEXT("InstructorTrack t=%.2f phase=%d lat=%.8f lon=%.8f alt_ft=%.2f ias_kt=%.2f heading=%.2f pitch=%.2f bank=%.2f vsi_fps=%.2f throttle=%.3f elevator=%.3f aileron=%.3f rudder=%.3f"),
               S.elapsed_seconds, static_cast<int>(Phase), S.latitude_deg, S.longitude_deg,
               S.altitude_ft, S.airspeed_kts, S.heading_deg, S.pitch_deg, S.roll_deg, S.vertical_speed_fps,
               P.getDoubleValue("/controls/engines/current-engine/throttle"),
               P.getDoubleValue("/controls/flight/elevator"),
               P.getDoubleValue("/controls/flight/aileron"),
               P.getDoubleValue("/controls/flight/rudder"));
    }
#endif
    CockpitCamera->SetRelativeRotation(BaseCockpitRotation + FRotator(P.getDoubleValue("/sim/current-view/pitch-offset-deg"),P.getDoubleValue("/sim/current-view/heading-offset-deg"),P.getDoubleValue("/sim/current-view/roll-offset-deg")));
    const double IndicatedAltitude = Flight->properties.getDoubleValue("/instrumentation/altimeter/indicated-altitude-ft");
    const double VerticalSpeed = P.getDoubleValue("/instrumentation/vertical-speed-indicator/indicated-speed-fpm");
    const double VerticalSpeedAngle = FMath::Clamp(VerticalSpeed / 2000, -1.0, 1.0) * 90;
    const double Values[]={S.airspeed_kts*2,-S.roll_deg,IndicatedAltitude*.36,S.heading_deg,VerticalSpeedAngle,S.rpm*.1,
                           FMath::Clamp(P.getDoubleValue("/engines/engine[0]/oil-pressure-psi"), 0.0, 100.0)*2.7,
                           FMath::Clamp((P.getDoubleValue("/engines/engine[0]/oil-temperature-degf")-50)/200, 0.0, 1.0)*270,
                           FMath::Clamp((P.getDoubleValue("/instrumentation/outside-air-temperature/degc")+40)/90, 0.0, 1.0)*270};
    for(int i=0;i<Needles.Num();++i)Needles[i]->SetRelativeRotation(FRotator(0,0,Values[i]));
    Needles[4]->SetRelativeLocation(FVector(58.5, 0, 48) + FRotator(0, 0, VerticalSpeedAngle).RotateVector(FVector(0, -2.5, 0)));
    // The horizon falls as the nose rises and stays inside the instrument face.
    const double HorizonHeight = FMath::Clamp(-S.pitch_deg * .15, -4.0, 4.0);
    Needles[1]->SetRelativeLocation(FVector(59, 0, 25 + HorizonHeight));
    // Keep the selected heading at the rim of the rotating heading instrument.
    const double HeadingOffset = FMath::FindDeltaAngleDegrees(
        S.heading_deg, P.getDoubleValue("/autopilot/settings/heading-bug-deg"));
    const FVector BugOffset = FRotator(0, 0, HeadingOffset).RotateVector(FVector(0, 0, 7));
    HeadingBug->SetRelativeLocation(Needles[3]->GetRelativeLocation() + FVector(-1, 0, 0) + BugOffset);
    const double FlapPosition = FMath::Clamp(P.getDoubleValue("/fdm/jsbsim/fcs/flap-pos-norm"), 0.0, 1.0);
    FlapIndicator->SetRelativeLocation(FVector(58, 41, 47 - 20 * FlapPosition));
    // Each tank in the bundled C172 flight model has a capacity of 126 lb.
    constexpr double TankCapacityLb = 126;
    for (int Tank = 0; Tank < FuelBars.Num(); ++Tank) {
        const std::string Path = "/consumables/fuel/tank[" + std::to_string(Tank) + "]/level-lbs";
        const double Level = FMath::Clamp(P.getDoubleValue(Path.c_str()) / TankCapacityLb, 0.0, 1.0);
        FuelBars[Tank]->SetVisibility(Level > 0);
        FuelBars[Tank]->SetRelativeLocation(FVector(59, -48 + 7 * Tank, 26 + 11 * Level));
        FuelBars[Tank]->SetRelativeScale3D(FVector(.01, .04, .22 * Level));
    }
    const bool AlternatorWarningOn = P.getBoolValue("/systems/electrical/main-bus-powered") &&
                                     !P.getBoolValue("/systems/electrical/alternator-online");
    AlternatorWarning->SetVisibility(AlternatorWarningOn);
    P.setBoolValue("/instrumentation/annunciators/alternator-warning", AlternatorWarning->IsVisible());
    if(bDiagnostics && ++DiagnosticFrames%120==0){
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Diagnostics"));
        IFileManager::Get().MakeDirectory(*Directory,true);
        if (!WriteFlightDiagnostics(TCHAR_TO_UTF8(*FPaths::Combine(Directory, TEXT("flight-status.json"))), DiagnosticFrames, S))
            UE_LOG(LogTemp, Warning, TEXT("Could not write the flight diagnostics snapshot"));
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,TEXT("frame.png")),false,false);
    }
}
void AOneClickPilotPawn::UpdateAircraftLights()
{
    auto& Properties = Flight->properties;
    const bool Powered = Properties.getBoolValue("/systems/electrical/main-bus-powered");
    const auto Enabled = [&](const char* Switch) {
        return Powered && Properties.getBoolValue(Switch);
    };
    LandingLight->SetVisibility(Enabled("/controls/lighting/landing-lights"));
    TaxiLight->SetVisibility(Enabled("/controls/lighting/taxi-light"));
    const bool Navigation = Enabled("/controls/lighting/nav-lights");
    LeftNavLight->SetVisibility(Navigation);
    RightNavLight->SetVisibility(Navigation);
    const double Time = Flight->state.elapsed_seconds;
    BeaconLight->SetVisibility(Enabled("/controls/lighting/beacon") && std::fmod(Time, 1.2) < .2);
    const double StrobePhase = std::fmod(Time, 1.5);
    const bool StrobeFlash = StrobePhase < .08 || (StrobePhase >= .2 && StrobePhase < .28);
    for (UStaticMeshComponent* Lamp : StrobeLights) Lamp->SetVisibility(Enabled("/controls/lighting/strobe") && StrobeFlash);
    Properties.setBoolValue("/sim/oneclickpilot/lights/landing", LandingLight->IsVisible());
    Properties.setBoolValue("/sim/oneclickpilot/lights/taxi", TaxiLight->IsVisible());
    Properties.setBoolValue("/sim/oneclickpilot/lights/navigation", LeftNavLight->IsVisible());
    Properties.setBoolValue("/sim/oneclickpilot/lights/beacon", BeaconLight->IsVisible());
    Properties.setBoolValue("/sim/oneclickpilot/lights/strobe", StrobeLights[0]->IsVisible());
}

void AOneClickPilotPawn::EndPlay(const EEndPlayReason::Type Reason){
    Flight.reset();if(FlightLibrary){FPlatformProcess::FreeDllHandle(FlightLibrary);FlightLibrary=nullptr;}Super::EndPlay(Reason);
}
