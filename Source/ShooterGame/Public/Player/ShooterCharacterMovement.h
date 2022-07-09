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

	/** Distance of the teleport */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter Character Movement", Meta = (AllowPrivateAcces = "true"))
	float TeleportDistance = 500.0f;

	/** Base cooldown for the teleport ability */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter Character Movement", Meta = (AllowPrivateAccess = "true"))
	float TeleportCooldown = 0.5f;

public:

	virtual float GetMaxSpeed() const override;

	/** Set the TeleportKeyDown to bTeleport
	 * @param bTeleport bool to set the TeleportKeyDown
	 */
	void SetTeleport(bool bTeleport);

	/** Teleport the chracter based on TeleportDistance */
	void Teleport();

	/** Check if CurrentTeleportCooldown has elapsed
	 * @return True if CurrentTeleportCooldown is less than 0 otherwise it substract the deltatime to CurrentTeleportCooldown and then return false
	 */
	bool CanTeleport();

	/** Reset CurrentTeleportCooldown to TeleportCooldown value */
	void ResetTeleportTimer();

	/** Override version of UpdateFromCompressedFlags to add custom ability */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	/** Override version of GetPredictionData_Client to return custom FNetworkPredictionData_ShooterClient
	 * @return Custom FNetworkPredictionData_ShooterClient that includes custom ability
	 */
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	/** Override TickComponent to use custom ability client side and make a prediction */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	/* If True the flags will be saved to send to the client the will to Teleport */
	uint8 WantsToTeleport : 1;

	/** Setted by SetTeleport() */
	bool TeleportKeyDown = false;

	/** Cooldown timer used for CanTeleport() */
	float CurrentTeleportCooldown = 0.0f;
};

