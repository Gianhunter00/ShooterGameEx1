// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Movement component meant for use with Pawns.
 */

#pragma once
#include "GameFramework/CharacterMovementComponent.h"
#include "ShooterCharacterMovement.generated.h"

class FSavedMove_ShooterCharacter : public FSavedMove_Character
{

public:

	uint8 GetCompressedFlags() const override;
	void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	void Clear() override;

	bool bPressedTeleport : 1;
	
};

class FNetworkPredictionData_Client_ShooterCharacter : public FNetworkPredictionData_Client_Character
{

public:
	virtual FSavedMovePtr AllocateNewMove() override;
};

UCLASS()
class UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY() 
public:
	/*If true try to teleport Player to the new location*/
	UPROPERTY(Category = "Character Movement (General Settings)", VisibleInstanceOnly, BlueprintReadOnly)
	uint8 bWantsToTeleport:1;

public:
	virtual float GetMaxSpeed() const override;

	virtual void UpdateFromCompressedFlags(uint8 InFlags) override;
};

