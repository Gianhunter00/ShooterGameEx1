// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Movement component meant for use with Pawns.
 */

#pragma once
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Actor.h"
#include "ShooterCharacterMovement.generated.h"

class FSavedMove_ShooterCharacter : public FSavedMove_Character
{

public:
	
	typedef FSavedMove_Character Super;

	/** Resets all saved variables */
	virtual void Clear() override;
	/** Store input commands in the compressed flags */
	virtual uint8 GetCompressedFlags() const override;
	/** Check if two move can be combined into one */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const override;
	/** Set up the move before sending it to the server */
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	/** Sets variables on character movement component before making a predictive correction */
	virtual void PrepMoveFor(class ACharacter* Character) override;

private:
	uint8 SavedWantsToTeleport : 1;
};

class FNetworkPredictionData_Client_ShooterCharacter : public FNetworkPredictionData_Client_Character
{
public:

	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_ShooterCharacter(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};

UCLASS(BlueprintType)
class UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

		friend class FSavedMove_ShooterCharacter;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter Character Movement", Meta = (AllowPrivateAcces = "true"))
	float TeleportDistance = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter Character Movement", Meta = (AllowPrivateAccess = "true"))
	float TeleportCooldown = 0.5f;
public:
	virtual float GetMaxSpeed() const override;
	void SetTeleport(bool bTeleport);
	void Teleport();
	bool CanTeleport();
	void ResetTeleportTimer();

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/*If true try to teleport Player to the new location*/
	uint8 WantsToTeleport : 1;
	bool TeleportKeyDown = false;
	float CurrentTeleportCooldown = 0.0f;
};

