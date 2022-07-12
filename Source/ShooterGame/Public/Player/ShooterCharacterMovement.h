// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Movement component meant for use with Pawns.
 */

#pragma once
#include "EWallRunSide.h"
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
	uint8 SavedWantsToWallJump : 1;
	uint8 SavedWantsToWallRun : 1;
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

	/** Distance of the Teleport */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter Character Movement", Meta = (AllowPrivateAcces = "true"))
	float TeleportDistance = 500.0f;

	/** Base cooldown for the Teleport ability */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter Character Movement", Meta = (AllowPrivateAccess = "true"))
	float TeleportCooldown = 1.0f;

	// The player's velocity while wall running
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunSpeed = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunTargetGravity = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunOffJumpForceXY = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunOffJumpForceZ = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float LineTraceVerticalTolerance = 50.0f;


protected:
	virtual void BeginPlay() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:


	virtual float GetMaxSpeed() const override;

	/** Set the TeleportKeyDown to bTeleport
	 * @param bTeleport bool to set the TeleportKeyDown
	 */
	void SetTeleportKeyDown(bool bTeleport);

	void SetWallJumpKeyDown(bool bWallJump);

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

	virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;

	void WallRunUpdate();

	void WallRunEndVectors(FVector& Right, FVector& Left) const;

	bool WallRunMovement(FVector StartLineTrace, FVector& EndLineTrace, float InWallRunDirection);

	bool IsValidWallRunVector(const FVector& InVector) const;

	FVector ForwardVelocityOnWall(const float InWallRunDirection) const;

	void WallRunEnd();

	void StartWallRunCooldown();

	void ReEnableWallRunAfterCooldown();

	void CameraTick() const;

	void CameraTilt(const float TargetXRoll) const;

	void WallRunJump();

	bool CanWallRunJump() const;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	UFUNCTION()
	void OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	void PhysWallRunning(float deltaTime, int32 Iterations);
	UFUNCTION(BlueprintCallable, Category = "Custom Character Movement")
		bool BeginWallRun();
	// Ends the character's wall run
	UFUNCTION(BlueprintCallable, Category = "Custom Character Movement")
		void EndWallRun();
	// Returns true if the required wall run keys are currently down
	bool AreRequiredWallRunKeysDown() const;
	// Returns true if the player is next to a wall that can be wall ran
	bool IsNextToWall(float vertical_tolerance = 0.0f);
	// Finds the wall run direction and side based on the specified surface normal
	void FindWallRunDirectionAndSide(const FVector& surface_normal, FVector& direction, EWallRunSide& side) const;
	// Helper function that returns true if the specified surface normal can be wall ran on
	bool CanSurfaceBeWallRan(const FVector& surface_normal) const;
	// Returns true if the movement mode is custom and matches the provided custom movement mode
	bool IsCustomMovementMode(uint8 custom_movement_mode) const;

private:

	/* If True the flags will be saved to send to the client the will to Teleport */
	uint8 WantsToTeleport : 1;

	/** Setted by SetTeleport() */
	bool TeleportKeyDown = false;

	/** Cooldown timer used for CanTeleport() */
	float CurrentTeleportCooldown = 0.0f;

	float DefaultGravity = 0.0f;

	FVector WallRunNormal;

	bool IsWallRunning = false;

	bool IsWallRunningR = false;

	bool IsWallRunningL = false;

	bool IsWallRunOnCooldown = false;

	FTimerHandle WallRunTimerHandle;

	bool WallJumpKeyDown = false;

	uint8 WantsToWallJump : 1;

	uint8 WantsToWallRun : 1;

	bool WallRunKeyDown = false;

	// The direction the character is currently wall running in
	FVector WallRunDirection;
	// The side of the wall the player is running on.
	EWallRunSide WallRunSide;
};

