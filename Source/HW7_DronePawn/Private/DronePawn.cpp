
#include "DronePawn.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "DroneController.h"
#include "EnhancedInputComponent.h"
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

	
}

// Called every frame
void ADronePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

	CheckGround();

	if (!bIsFly) 
	{
		if (bIsGrounded)
		{
			Velocity.Z = 0;
		}
		else
		{
			Velocity.Z += Gravity * DeltaTime;
		}

		AddActorWorldOffset(FVector(0.f, 0.f, Velocity.Z * DeltaTime), true);
	}

	bIsFly = false;
}

void ADronePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);


	// Enhanced InputComponent로 캐스팅
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// IA를 가져오기 위해 현재 소유 중인 Controller를 ASpartaPlayerController로 캐스팅
		if (ADroneController* DroneController = Cast<ADroneController>(GetController()))
		{
			if (DroneController->MoveAction)
			{
				EnhancedInput->BindAction(
					DroneController->MoveAction,
					ETriggerEvent::Triggered,
					this,
					&ADronePawn::Move
				);
			}
			
			if (DroneController->LookAction)
			{
				EnhancedInput->BindAction(
					DroneController->LookAction,
					ETriggerEvent::Triggered,
					this,
					&ADronePawn::Look
				);
			}

			if (DroneController->FlyUpDownAction)
			{
				EnhancedInput->BindAction(
					DroneController->FlyUpDownAction,
					ETriggerEvent::Triggered,
					this,
					&ADronePawn::FlyUpDown
				);
			}
			if (DroneController->RollAction)
			{
				EnhancedInput->BindAction(
					DroneController->RollAction,
					ETriggerEvent::Triggered,
					this,
					&ADronePawn::Roll
				);
			}
		}
	}
}

void ADronePawn::Move(const FInputActionValue& Value)
{
	const FVector2D MoveVector = Value.Get<FVector2D>();
	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	float CurrentMoveSpeed = bIsGrounded ? MoveSpeed : MoveSpeed * AirControlRatio;

	const FVector ForwardMovement = FVector::ForwardVector * MoveVector.Y * CurrentMoveSpeed * DeltaTime;
	const FVector RightMovement = FVector::RightVector * MoveVector.X * CurrentMoveSpeed * DeltaTime;

	AddActorLocalOffset(ForwardMovement, true);
	AddActorLocalOffset(RightMovement, true);
}

void ADronePawn::FlyUpDown(const FInputActionValue& Value)
{
	const float UpDownValue = Value.Get<float>();

	if (UpDownValue < 0.0f && bIsGrounded)
	{
		return;
	}

	Velocity.Z = 0;

	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	AddActorLocalOffset(FVector::UpVector * UpDownValue * MoveSpeed * DeltaTime, true);

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
	if (!GetWorld() || !BoxComponent) return;

	const FVector Start = GetActorLocation();
	const float HalfHeight = BoxComponent->GetScaledBoxExtent().Z;
	const float TraceLength = HalfHeight + GroundCheckDistance; 
	const FVector End = Start - FVector(0.f, 0.f, TraceLength);

	FHitResult HitResult;
	FCollisionQueryParams Params(TEXT("GroundCheck"), false, this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	// DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 0.f, 0, 1.f);

	if (bHit && HitResult.ImpactNormal.Z > 0.7f)
	{
		if (!bIsGrounded)
		{
			bIsLanding = true;
		}

		bIsGrounded = true;

		if (Velocity.Z < 0)
		{
			Velocity.Z = 0;
		}
	}
	else
	{
		bIsGrounded = false;
	}
}

