// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneModeBase.h"
#include "DronePawn.h"
#include "DroneController.h"

ADroneModeBase::ADroneModeBase()
{
	DefaultPawnClass = ADronePawn::StaticClass();
	PlayerControllerClass = ADroneController::StaticClass();
}
