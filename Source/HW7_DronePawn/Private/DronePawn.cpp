
#include "DronePawn.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetSystemLibrary.h"

ADronePawn::ADronePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	SetRootComponent(BoxComponent);
	BoxComponent->SetSimulatePhysics(false);

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(RootComponent);
	SkeletalMeshComponent->SetSimulatePhysics(false);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(RootComponent);
	SpringArmComponent->TargetArmLength = 250.0f;
	SpringArmComponent->SocketOffset = FVector(0.0f, 0.0f, 50.0f);
	SpringArmComponent->bUsePawnControlRotation = false;

	SpringArmComponent->bDoCollisionTest = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;

	Velocity = FVector::ZeroVector;
	bIsGrounded = false;
	bIsFly = false; 
	bIsLanding = false;
}

// Called when the game starts or when spawned
void ADronePawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

// Called every frame
void ADronePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckGround();

	if (bIsLanding)
	{
		const FRotator CurrentRotation = GetActorRotation();
		const FRotator TargetRotation(0.f, CurrentRotation.Yaw, 0.f);

		if (CurrentRotation.Equals(TargetRotation, 1.0f))
		{
			bIsLanding = false;
		}
		else
		{
			FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 8.0f);
			SetActorRotation(NewRotation);
		}
	}

	if (!bIsFly) 
	{
		if (bIsLanding)
		{
			Velocity.Z = 0;
		}
		else if (bIsGrounded)
		{
			if (Velocity.Z < 0) Velocity.Z = 0;
		}
		else
		{
			Velocity.Z += Gravity * DeltaTime; 
		}
	}
	FVector DeltaLocation = Velocity * DeltaTime;
	AddActorWorldOffset(DeltaLocation, true);
	bIsFly = false;
}

void ADronePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADronePawn::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADronePawn::StopMove);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADronePawn::Look);
		EnhancedInputComponent->BindAction(FlyUpDownAction, ETriggerEvent::Triggered, this, &ADronePawn::FlyUpDown);
		EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Triggered, this, &ADronePawn::Roll);
	}
}

void ADronePawn::Move(const FInputActionValue& Value)
{
	const FVector2D MoveVector = Value.Get<FVector2D>();
	float CurrentMoveSpeed = bIsGrounded ? MoveSpeed : MoveSpeed * AirControlRatio;

	const FVector ForwardDirection = GetActorForwardVector() * MoveVector.Y;
	const FVector RightDirection = GetActorRightVector() * MoveVector.X;

	FVector WorldMoveDirection = (ForwardDirection + RightDirection).GetSafeNormal();

	Velocity.X = WorldMoveDirection.X * CurrentMoveSpeed;
	Velocity.Y = WorldMoveDirection.Y * CurrentMoveSpeed;
}

void ADronePawn::FlyUpDown(const FInputActionValue& Value)
{
	const float UpDownValue = Value.Get<float>();

	Velocity.Z = UpDownValue * MoveSpeed;

	bIsFly = true;
}

void ADronePawn::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	if (!bIsGrounded)
	{
		FRotator PitchRotation(LookAxisVector.Y * RotationSpeed * DeltaTime * -1.0f, 0.0f, 0.0f);
		AddActorLocalRotation(PitchRotation, true);
	}

	FRotator YawRotation(0.0f, LookAxisVector.X * RotationSpeed * DeltaTime, 0.0f);
	AddActorLocalRotation(YawRotation, true);
}

void ADronePawn::Roll(const FInputActionValue& Value)
{
	if (bIsGrounded)
	{
		return;
	}

	const float RollValue = Value.Get<float>();
	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	FRotator RollRotation(0.0f, 0.0f, RollValue * RotationSpeed * DeltaTime);
	AddActorLocalRotation(RollRotation, true);
}

void ADronePawn::CheckGround()
{
	FVector Start = GetActorLocation();

	const float LandingProximity = 100.0f;
	FVector End = Start - FVector(0, 0, LandingProximity);

	FHitResult HitResult;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	bool bHit = UKismetSystemLibrary::LineTraceSingle(
		GetWorld(),
		Start,
		End,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		true
	);

	if (bHit && HitResult.bBlockingHit)
	{
		const float GroundDistance = (Start - HitResult.ImpactPoint).Size();
		const bool bIsActuallyTouchingGround = GroundDistance <= (BoxComponent->GetScaledBoxExtent().Z + 5.0f);

		if (bIsActuallyTouchingGround)
		{
			if (!bIsGrounded)
			{
				bIsLanding = true;
			}
			bIsGrounded = true;
			if (Velocity.Z < 0) Velocity.Z = 0;
		}
		else
		{
			bIsGrounded = false;
		}
	}
	else
	{
		bIsGrounded = false;
	}
}

void ADronePawn::StopMove(const FInputActionValue& Value)
{
	Velocity.X = 0;
	Velocity.Y = 0;
}