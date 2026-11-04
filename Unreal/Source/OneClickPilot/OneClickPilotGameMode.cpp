// SPDX-License-Identifier: MIT
#include "OneClickPilotGameMode.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "OneClickPilotPawn.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Materials/MaterialInterface.h"
AOneClickPilotGameMode::AOneClickPilotGameMode(){
    DefaultPawnClass = AOneClickPilotPawn::StaticClass();
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    SkyMesh=Sphere.Object;
}
void AOneClickPilotGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    const FSoftClassPath Cockpit(TEXT("/Game/Aircraft/BP_OneClickPilotCockpit.BP_OneClickPilotCockpit_C"));
    if (UClass* CockpitClass = Cockpit.TryLoadClass<AOneClickPilotPawn>())
        DefaultPawnClass = CockpitClass;
    Super::InitGame(MapName, Options, ErrorMessage);
}
void AOneClickPilotGameMode::BeginPlay(){
    Super::BeginPlay();
    bool HasBakedScenery = false;
    for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor) {
        if (Actor->ActorHasTag(TEXT("OneClickPilot.BakedScenery"))) {
            HasBakedScenery = true;
            break;
        }
    }
    if (!HasBakedScenery)
        BuildPracticeScenery(GetWorld());

    if (FParse::Param(FCommandLine::Get(), TEXT("OneClickSceneryPreview"))) {
        const FVector Location(-190000, -55000, 68000);
        const FVector LookAt(-52000, 65000, 0);
        auto* Camera = GetWorld()->SpawnActor<ACameraActor>(Location, (LookAt - Location).Rotation());
        GetWorldTimerManager().SetTimerForNextTick([this, Camera]() {
            if (auto* Controller = GetWorld()->GetFirstPlayerController())
                Controller->SetViewTarget(Camera);
        });
    }

}

bool AOneClickPilotGameMode::BakePracticeScenery(UObject* WorldContext)
{
#if WITH_EDITOR
    UWorld* World = Cast<UWorld>(WorldContext);
    if (!World || World->WorldType != EWorldType::Editor)
        return false;
    for (TActorIterator<AActor> Actor(World); Actor; ++Actor) {
        if (Actor->ActorHasTag(TEXT("OneClickPilot.BakedScenery")))
            return false;
    }
    GetDefault<AOneClickPilotGameMode>()->BuildPracticeScenery(World);
    World->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

void AOneClickPilotGameMode::BuildPracticeScenery(UWorld* World) const
{
    auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Box=[&](FVector Location,FVector Scale,FRotator Rotation,const TCHAR* Material, const TCHAR* Label = TEXT("Scenery detail")){
        auto Actor=World->SpawnActor<AStaticMeshActor>(Location,Rotation);
        Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
        Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Actor->GetStaticMeshComponent()->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Material));
        Actor->SetActorScale3D(Scale);
#if WITH_EDITOR
        if (World->WorldType == EWorldType::Editor) {
            Actor->SetActorLabel(Label);
            Actor->SetFolderPath(TEXT("Practice Scenery"));
        }
#endif
    };
    Box(FVector(0,0,-50),FVector(74080,74080,1),FRotator::ZeroRotator,TEXT("/Game/Materials/M_CoastalGrass.M_CoastalGrass"), TEXT("Airfield ground"));
    const FRotator RunwayRotation(0,20,0);
    const FVector Forward=RunwayRotation.Vector();
    Box(Forward*51800+FVector(0,0,4),FVector(1036,18,.02),RunwayRotation,TEXT("/Game/Materials/M_Runway.M_Runway"), TEXT("Runway 02–20"));
    for(int i=1;i<25;++i)Box(Forward*(i*4000)+FVector(0,0,9),FVector(20,.4,.02),RunwayRotation,TEXT("/Game/Materials/M_White.M_White"));
    // Approximate Shoreham landmarks in runway-relative metres.
    const FVector Right = RunwayRotation.RotateVector(FVector::RightVector);
    const TCHAR* Paint = TEXT("/Game/Materials/M_White.M_White");
    const TCHAR* Asphalt = TEXT("/Game/Materials/M_Runway.M_Runway");
    const TCHAR* Water = TEXT("/Game/Materials/M_CoastalWater.M_CoastalWater");
    const TCHAR* Yellow = TEXT("/Game/Materials/M_TaxiLine.M_TaxiLine");
    auto Place = [&](double Along, double Across, double Height, FVector Size, const TCHAR* Material) {
        Box(100 * (Forward * Along + Right * Across) + FVector(0, 0, Height * 100),
            Size, RunwayRotation, Material);
    };
    for (double Side : {-8.6, 8.6})
        Place(518, Side, .09, FVector(1036, .22, .02), Paint);
    for (double Threshold : {20.0, 1016.0}) {
        for (double Stripe : {-6.0, -4.0, -2.0, 2.0, 4.0, 6.0})
            Place(Threshold, Stripe, .09, FVector(8, .8, .02), Paint);
    }

    // Main apron and taxiway to the southern runway end.
    Place(120, -108, .012, FVector(110, 55, .024), Asphalt);
    Place(40, -53, .012, FVector(7, 91, .024), Asphalt);
    Place(83, -98, .012, FVector(88, 7, .024), Asphalt);
    Place(40, -53, .034, FVector(.15, 89, .02), Yellow);
    Place(83, -98, .034, FVector(86, .15, .02), Yellow);
    for (double Offset : {-1.2, -1.6})
        Place(40, -15 + Offset, .034, FVector(6, .14, .02), Yellow);

    // Light-coloured terminal with stepped central tower and nearby hangars.
    Place(118, -144, 3, FVector(45, 14, 6), Paint);
    Place(118, -144, 7.5, FVector(12, 12, 3), Paint);
    Place(118, -144, 9.4, FVector(8, 8, .8), Asphalt);
    for (double Along : {163.0, 198.0})
        Place(Along, -135, 4, FVector(28, 22, 8), Paint);
    Place(165, 25, 2.5, FVector(.12, .12, 5), Paint);
    Place(165, 26.25, 4.8, FVector(.65, 2.5, .55), TEXT("/Game/Materials/M_Windsock.M_Windsock"));

    // Coast to the south and the Adur to the east make the airfield easier to locate.
    Box(FVector(-2070000, 0, 12), FVector(38600, 76000, .02), FRotator::ZeroRotator, Water, TEXT("Coastal water"));
    Box(FVector(-26000, 46000, 150), FVector(9, 65, 3), FRotator::ZeroRotator, Asphalt);
    for (double North : {160000.0, 250000.0, 335000.0})
        Box(FVector(North, -48000, 1), FVector(630, 550, .02), FRotator(0, 12, 0),
            TEXT("/Game/Materials/M_Field.M_Field"));

    // Group repeated town geometry by mesh and material for the mobile renderer.
    auto* Town = World->SpawnActor<AActor>();
#if WITH_EDITOR
    if (World->WorldType == EWorldType::Editor) {
        Town->Tags.Add(TEXT("OneClickPilot.BakedScenery"));
        Town->SetActorLabel(TEXT("Town, river and terrain"));
        Town->SetFolderPath(TEXT("Practice Scenery"));
    }
#endif
    auto* TownRoot = NewObject<USceneComponent>(Town, TEXT("SceneryRoot"), RF_Transactional);
    Town->AddInstanceComponent(TownRoot);
    Town->SetRootComponent(TownRoot);
    TownRoot->RegisterComponent();
    // Follow a smooth, variable-width river through the floodplain.
    const FVector RiverPoints[] = {
        {-1460, 570, 0}, {-850, 560, 0}, {-260, 460, 0}, {120, 435, 0},
        {600, 520, 0}, {1250, 660, 0}, {2100, 520, 0}, {3400, 770, 0}
    };
    constexpr int RiverPointCount = UE_ARRAY_COUNT(RiverPoints);
    auto RiverCentre = [&](int Segment, double T) {
        const FVector Start = RiverPoints[Segment];
        const FVector End = RiverPoints[Segment + 1];
        const FVector Before = RiverPoints[FMath::Max(0, Segment - 1)];
        const FVector After = RiverPoints[FMath::Min(RiverPointCount - 1, Segment + 2)];
        return FMath::CubicInterp(Start, (End - Before) * .5, End, (After - Start) * .5, T);
    };
    auto RiverSurface = [&](double LeftSign, double LeftOffset, double RightSign, double RightOffset, double Height, const TCHAR* Material) {
        auto* Mesh = NewObject<UProceduralMeshComponent>(Town, NAME_None, RF_Transactional);
        Town->AddInstanceComponent(Mesh);
        Mesh->SetupAttachment(TownRoot);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        Mesh->RegisterComponent();
        TArray<FVector> Vertices, Normals;
        TArray<FLinearColor> Colors;
        TArray<int32> Triangles;
        constexpr int Subdivisions = 24;
        constexpr int Last = (RiverPointCount - 1) * Subdivisions;
        for (int Sample = 0; Sample <= Last; ++Sample) {
            const int Segment = FMath::Min(Sample / Subdivisions, RiverPointCount - 2);
            const double T = double(Sample - Segment * Subdivisions) / Subdivisions;
            const FVector Centre = RiverCentre(Segment, T);
            const FVector Tangent = RiverCentre(Segment, T + .001) - RiverCentre(Segment, T - .001);
            const FVector Side = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
            const double HalfWidth = 17 + 3.5 * FMath::Sin(Centre.X / 220);
            for (double Offset : {LeftSign * HalfWidth + LeftOffset, RightSign * HalfWidth + RightOffset}) {
                Vertices.Add((Centre + Side * Offset + FVector(0, 0, Height)) * 100);
                Normals.Add(FVector::UpVector);
                Colors.Add(FLinearColor(.09, .13, .039));
            }
            if (Sample < Last) {
                const int I = 2 * Sample;
                Triangles.Append({I, I + 1, I + 2, I + 1, I + 3, I + 2});
            }
        }
        Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, {}, Colors, {}, false);
        Mesh->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, Material));
    };
    // Keep the banks in separate strips so they do not compete with the water in the depth buffer.
    const TCHAR* Bank = TEXT("/Game/Materials/M_RidgeTerrain.M_RidgeTerrain");
    RiverSurface(-1, -5, -1, 0, .12, Bank);
    RiverSurface(1, 0, 1, 5, .12, Bank);
    RiverSurface(-1, 0, 1, 0, .12, Water);

    auto Batch = [Town](UStaticMesh* Mesh, const TCHAR* Material) {
        auto* Instances = NewObject<UInstancedStaticMeshComponent>(Town, NAME_None, RF_Transactional);
        Town->AddInstanceComponent(Instances);
        Instances->SetupAttachment(Town->GetRootComponent());
        Instances->SetStaticMesh(Mesh);
        Instances->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, Material));
        Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Instances->RegisterComponent();
        return Instances;
    };
    auto* Brick = Batch(Cube, TEXT("/Game/Materials/M_Brick.M_Brick"));
    auto* Plaster = Batch(Cube, Paint);
    auto* Tile = Batch(Cube, TEXT("/Game/Materials/M_Roof.M_Roof"));
    auto* Slate = Batch(Cube, TEXT("/Game/Materials/M_Slate.M_Slate"));
    auto* Windows = Batch(Cube, TEXT("/Game/Materials/M_Glass.M_Glass"));
    auto* Roads = Batch(Cube, Asphalt);
    auto* Bark = Batch(Cube, TEXT("/Game/Materials/M_Bark.M_Bark"));
    auto* LightLeaves = Batch(SkyMesh, TEXT("/Game/Materials/M_LeavesLight.M_LeavesLight"));
    auto* DarkLeaves = Batch(SkyMesh, TEXT("/Game/Materials/M_LeavesDark.M_LeavesDark"));
    auto* Paving = Batch(Cube, TEXT("/Game/Materials/M_Paving.M_Paving"));
    auto* Pasture = Batch(Cube, TEXT("/Game/Materials/M_Pasture.M_Pasture"));
    auto* Crops = Batch(Cube, TEXT("/Game/Materials/M_Crop.M_Crop"));
    auto* Hedges = Batch(Cube, TEXT("/Game/Materials/M_Hedge.M_Hedge"));
    auto Add = [](UInstancedStaticMeshComponent* Group, FVector Metres, FVector Size,
                  FRotator Rotation = FRotator::ZeroRotator) {
        Group->AddInstance(FTransform(Rotation, Metres * 100, Size));
    };

    FRandomStream Random(20261103);
    auto PlantTree = [&](FVector Ground, double Height) {
        const double Width = Height * Random.FRandRange(.5, .8);
        Add(Bark, Ground + FVector(0, 0, Height * .27), FVector(Height * .045, Height * .045, Height * .54));
        auto* Crown = Random.RandRange(0, 2) ? DarkLeaves : LightLeaves;
        Add(Crown, Ground + FVector(0, 0, Height * .69), FVector(Width, Width * .85, Height * .63));
        const double Angle = Random.FRandRange(0, 360);
        const FVector Branch = FRotator(0, Angle, 0).RotateVector(FVector(Width * .29, 0, Height * .56));
        Add(Crown, Ground + Branch, FVector(Width * .64, Width * .58, Height * .43), FRotator(0, Angle, 0));
    };
    // A compact street grid on the east bank, with an open waterfront.
    for (int Block = 0; Block <= 8; ++Block)
        Add(Roads, FVector(-1260 + Block * 145, 1080, .035), FVector(8, 910, .05));
    for (int Street = 0; Street <= 7; ++Street)
        Add(Roads, FVector(-680, 630 + Street * 130, .035), FVector(1160, 8, .05));
    for (int Block = 0; Block < 8; ++Block) {
        for (int Street = 0; Street < 7; ++Street) {
            const double X = -1260 + Block * 145;
            const double Y = 630 + Street * 130;
            if (Block == 3 && Street == 3) {
                // A recognizable civic square with a paved cross and a clock-tower silhouette.
                Add(Paving, FVector(X + 72, Y + 65, .055), FVector(96, 78, .09));
                Add(Plaster, FVector(X + 72, Y + 87, 6), FVector(43, 14, 12));
                Add(Slate, FVector(X + 72, Y + 87, 12.4), FVector(46, 16, .8));
                Add(Brick, FVector(X + 72, Y + 82, 13), FVector(8, 8, 26));
                Add(Slate, FVector(X + 72, Y + 82, 26.6), FVector(10, 10, 1.2));
                for (double Offset : {-42.0, 42.0}) {
                    PlantTree(FVector(X + 72 + Offset, Y + 34, .1), 8);
                    PlantTree(FVector(X + 72 + Offset, Y + 94, .1), 10);
                }
                continue;
            }
            if ((Block + 3 * Street) % 11 == 0) {
                for (int Tree = 0; Tree < 14; ++Tree)
                    PlantTree(FVector(X + Random.FRandRange(18, 127), Y + Random.FRandRange(16, 114), 0),
                              Random.FRandRange(5, 11));
                continue;
            }
            if (Block >= 2 && Block <= 4 && Street >= 2 && Street <= 4) {
                for (int Row = 0; Row < 3; ++Row) {
                    const double Height = Random.FRandRange(9, 17);
                    const FVector Centre(X + 26 + Row * 44, Y + 43 + (Row % 2) * 37, Height / 2);
                    Add(Row == 1 ? Brick : Plaster, Centre, FVector(28, 25, Height));
                    Add(Slate, Centre + FVector(0, 0, Height / 2 + .2), FVector(29, 26, .4));
                    for (int Floor = 1; Floor * 3 < Height; ++Floor)
                        Add(Windows, FVector(Centre.X, Centre.Y - 12.52, Floor * 3),
                            FVector(24, .08, 1.2));
                }
                continue;
            }
            for (int Lot = 0; Lot < 7; ++Lot) {
                for (double Side : {20.0, 110.0}) {
                    const double Width = Random.FRandRange(9, 14);
                    const double Depth = Random.FRandRange(10, 16);
                    const double Height = Random.FRandRange(4.5, 8.5);
                    const FVector Footprint(X + 16 + Lot * 18 + Random.FRandRange(-2, 2), Y + Side + Random.FRandRange(-4, 4), 0);
                    Add(Random.RandRange(0, 2) ? Brick : Plaster, Footprint + FVector(0, 0, Height / 2),
                        FVector(Width, Depth, Height));
                    auto* Roof = Random.RandRange(0, 2) ? Tile : Slate;
                    for (double Slope : {-1.0, 1.0})
                        Add(Roof, Footprint + FVector(0, Slope * Depth / 4, Height + Depth * .12),
                            FVector(Width + .8, Depth * .57, .32), FRotator(0, 0, Slope * 26));
                    for (double Eave : {-1.0, 1.0})
                        Add(Slate, Footprint + FVector(0, Eave * Depth * .51, Height + .08),
                            FVector(Width + .8, .18, .18));
                    const double Front = Side < 65 ? -1 : 1;
                    for (double Offset : {-Width / 4, Width / 4})
                        Add(Windows, Footprint + FVector(Offset, Front * (Depth / 2 + .02), Height * .68),
                            FVector(1.5, .08, 1.4));
                    const double YardLength = FMath::Abs(Footprint.Y - (Y + (Front < 0 ? 5 : 125))) - Depth / 2;
                    const FVector Entrance = Footprint + FVector(0, Front * (Depth + YardLength) / 2, .04);
                    Add(Paving, Entrance, FVector(2.4, YardLength, .06));
                    Add(Hedges, Footprint + FVector(0, -Front * (Depth / 2 + 11), .55), FVector(Width + 2, .8, 1.1));
                    if (Random.RandRange(0, 2) != 0)
                        PlantTree(Footprint + FVector(5, -Front * 14, 0), Random.FRandRange(4, 8));
                    if (Random.RandRange(0, 2) == 0)
                        Add(Brick, Footprint + FVector(Width * .29, 0, Height + Depth * .22),
                            FVector(.8, .75, 1.6));
                }
            }
        }
    }
    for (int Street = 0; Street <= 7; ++Street) {
        const double East = 630 + Street * 130;
        for (double Side : {-5.0, 5.0})
            Add(Paving, FVector(-680, East + Side, .07), FVector(1160, 1.6, .1));
    }
    Add(Roads, FVector(-260, 545, .055), FVector(8, 185, .08));
    Add(Roads, FVector(-680, 596, .035), FVector(1160, 6, .05));
    Add(Plaster, FVector(-1310, 1090, .22), FVector(7, 920, .44));
    // Waterfront warehouses, a prominent tower, beach and distant wooded hills.
    for (int Pier = 0; Pier < 4; ++Pier) {
        const double East = 720 + Pier * 160;
        Add(Roads, FVector(-1415, East, .6), FVector(95, 12, 1));
        Add(Plaster, FVector(-1325, East, 4), FVector(32, 24, 8));
        Add(Slate, FVector(-1325, East, 8.2), FVector(34, 26, .4));
    }
    // A sheltered marina adds small-scale cues near the harbour mouth.
    Add(Roads, FVector(-1490, 1050, .7), FVector(12, 420, 1.4));
    Add(Roads, FVector(-1450, 1260, .65), FVector(80, 12, 1.3));
    for (int Slip = 0; Slip < 12; ++Slip) {
        const double East = 870 + Slip * 31;
        Add(Paving, FVector(-1474, East, .55), FVector(24, 2.2, .6));
        if (Slip % 4 == 0)
            continue;
        const double Length = Random.FRandRange(6, 11);
        const FVector Boat(-1469, East + 4, .55);
        Add(Plaster, Boat, FVector(Length, 2.6, .9));
        Add(Slate, Boat + FVector(0, 0, .65), FVector(Length * .43, 1.7, .5));
    }
    Add(Plaster, FVector(-900, 1040, 12), FVector(10, 10, 24));
    Add(Slate, FVector(-900, 1040, 25), FVector(12, 12, 2));
    Box(FVector(-140000, 0, 15), FVector(34, 7500, .02), FRotator::ZeroRotator,
        TEXT("/Game/Materials/M_Sand.M_Sand"));
    // A continuous upland mesh blends into the flat practice area at its boundaries.
    auto RidgeHeight = [](double North, double East) {
        const double Envelope = FMath::SmoothStep(0.0, 800.0,
            FMath::Min(FMath::Min(North - 1750, 7100 - North), 3900 - FMath::Abs(East)));
        const double Noise = FMath::PerlinNoise2D(FVector2D(North, East) / 1100);
        const double Detail = FMath::PerlinNoise2D(FVector2D(North, East) / 290);
        return -2 + Envelope * (55 + 125 * FMath::Square(Noise + .6) + 18 * Detail);
    };
    auto* Ridge = NewObject<UProceduralMeshComponent>(Town, NAME_None, RF_Transactional);
    Town->AddInstanceComponent(Ridge);
    Ridge->SetupAttachment(TownRoot);
    Ridge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Ridge->RegisterComponent();
    TArray<FVector> Vertices, Normals;
    TArray<int32> Triangles;
    TArray<FLinearColor> Colors;
    constexpr int Columns = 101;
    constexpr int Rows = 70;
    for (int Row = 0; Row < Rows; ++Row) {
        for (int Col = 0; Col < Columns; ++Col) {
            const double North = 1750 + 80 * Row;
            const double East = -4000 + 80 * Col;
            const double Height = RidgeHeight(North, East);
            Vertices.Add(100 * FVector(North, East, Height));
            Normals.Add(FVector(RidgeHeight(North - 1, East) - RidgeHeight(North + 1, East),
                               RidgeHeight(North, East - 1) - RidgeHeight(North, East + 1), 2).GetSafeNormal());
            const float Variation = .5f + .5f * FMath::PerlinNoise2D(FVector2D(North, East) / 360);
            Colors.Add(FMath::Lerp(FLinearColor(.038, .085, .026), FLinearColor(.16, .21, .075), Variation));
            if (Row + 1 < Rows && Col + 1 < Columns) {
                const int I = Row * Columns + Col;
                Triangles.Append({I, I + 1, I + Columns, I + 1, I + Columns + 1, I + Columns});
            }
        }
    }
    Ridge->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, {}, Colors, {}, false);
    Ridge->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_RidgeTerrain.M_RidgeTerrain")));

    // Farm parcels and hedgerows break up the low ground on the inland side.
    for (int Parcel = 0; Parcel < 15; ++Parcel) {
        const double North = 280 + (Parcel % 5) * 295;
        const double East = -1550 + (Parcel / 5) * 345;
        const FRotator Bearing(0, -13 + (Parcel / 5) * 9, 0);
        Add(Parcel % 3 ? Pasture : Crops, FVector(North, East, .06), FVector(270, 310, .08), Bearing);
        for (double Side : {-153.0, 153.0}) {
            const FVector Boundary = Bearing.RotateVector(FVector(0, Side, 0));
            Add(Hedges, FVector(North, East, .75) + Boundary, FVector(265, 2.1, 1.5), Bearing);
        }
        if (Parcel % 3 == 0) {
            for (int Row = -10; Row <= 10; ++Row) {
                const FVector Offset = Bearing.RotateVector(FVector(0, Row * 13, 0));
                Add(Pasture, FVector(North, East, .12) + Offset, FVector(252, 1.1, .025), Bearing);
            }
        }
    }
    for (int Grove = 0; Grove < 12; ++Grove) {
        const FVector Centre(Random.FRandRange(1650, 4100), Random.FRandRange(-2400, -420), 0);
        for (int Tree = 0; Tree < 28; ++Tree) {
            FVector Ground = Centre + FVector(Random.FRandRange(-105, 105), Random.FRandRange(-95, 95), 0);
            Ground.Z = FMath::Max(0.0, RidgeHeight(Ground.X, Ground.Y));
            PlantTree(Ground, Random.FRandRange(5, 13));
        }
    }

    auto Sun=World->SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-35,-45,0));
    Sun->GetLightComponent()->SetIntensity(3.2f);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1, .91f, .79f));
    auto* Haze = World->SpawnActor<AExponentialHeightFog>();
    auto* Fog = Haze->GetComponent();
    Fog->SetFogDensity(.012f);
    Fog->SetFogHeightFalloff(.23f);
    Fog->SetStartDistance(35000);
    Fog->SetFogInscatteringColor(FLinearColor(.42f, .59f, .70f));
    auto Dome=World->SpawnActor<AStaticMeshActor>();
    Dome->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Dome->GetStaticMeshComponent()->SetStaticMesh(SkyMesh);
    Dome->GetStaticMeshComponent()->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Sky.M_Sky")));
    Dome->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Dome->GetStaticMeshComponent()->SetCastShadow(false);
    Dome->SetActorScale3D(FVector(200000));
    auto Sky=World->SpawnActor<ASkyLight>();Sky->GetLightComponent()->SetIntensity(.48f);Sky->GetLightComponent()->RecaptureSky();
}
