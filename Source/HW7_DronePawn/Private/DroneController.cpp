// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "DronePawn.h"

ADroneController::ADroneController()
    : DefaultMappingContext(nullptr),
    MoveAction(nullptr),
    LookAction(nullptr),
    FlyUpDownAction(nullptr),
    RollAction(nullptr)
{
}

void ADroneController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* DronePawn = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = DronePawn->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}
