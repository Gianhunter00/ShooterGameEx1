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

	/** The player's velocity while wall running */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunSpeed = 700.0f;

	/** WallRun Jumping force for the X and Y */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunOffJumpForceXY = 300.0f;

	/** WallRun Jumping force for the Z */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunOffJumpForceZ = 800.0f;

	/** WallRun Cooldown time after finishing a WallRun, called from EndWallRun() */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunCooldownAfterFall = 0.35f;

	/** Max time to WallRun before ending it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunTimeMax = 3.0f;

	/** Offset on Z when performing raycast to check if next to a Wall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float LineTraceVerticalTolerance = 50.0f;

	/** Wall Jumping force for the X */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Jump", Meta = (AllowPrivateAccess = "true"))
	float WallJumpOffJumpForceX = 400.0f;

	/** Wall Jumping force for the X */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Jump", Meta = (AllowPrivateAccess = "true"))
	float WallJumpOffJumpForceY = 400.0f;

	/** Wall Jumping force for the Z */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Jump", Meta = (AllowPrivateAccess = "true"))
	float WallJumpOffJumpForceZ = 500.0f;

	/** Wall Jumping force for the Z */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter Character Movement|Wall Jump", 
		Meta = (ToolTip = "0 is full side force, 90 is full forward force", ClampMin = 0, ClampMax = 90, AllowPrivateAccess = "true"))
	float WallJumpDirection = 30.0f;

protected:
	virtual void BeginPlay() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:

	/** Added to the ActorOnHit during BeginPlay */
	UFUNCTION()
	void OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/** Start the WallRun */
	UFUNCTION(BlueprintCallable, Category = "Custom Character Movement")
	bool BeginWallRun();

	/** End the WallRun and reset all */
	UFUNCTION(BlueprintCallable, Category = "Custom Character Movement")
	void EndWallRun();

	virtual float GetMaxSpeed() const override;

	/** Set the TeleportKeyDown to bTeleport
	 * @param bTeleport bool to set the TeleportKeyDown
	 */
	void SetTeleportKeyDown(bool bTeleport);

	/** Set the WallJumpKeyDown to bWallJump
	 * @param bWallJump bool to set the WallJumpKeyDown
	 */
	void SetWallJumpKeyDown(bool bWallJump);

	/** Teleport the character based on TeleportDistance */
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

	/** Override ProcessLanded, added WallRun control */
	virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;

	FVector ForwardVelocityOnWall(const float InWallRunDirection) const;

	/** Start the cooldown for the wall run, Cooldown time is equal to WallRunCooldownAfterFall */
	void StartWallRunCooldown();

	/** When the timer ends it enable WallRun again with this function */
	void ReEnableWallRunAfterCooldown();

	/** Calls CameraTilt based on which side of the wall we are running */
	void CameraTick() const;

	/** Twist the roll of the Controller to have more vision during WallRun */
	void CameraTilt(const float TargetXRoll) const;

	/** Jump during a WallRun, it use WallRunOffJumpForceXY and WallRunOffJumpForceZ */
	void WallRunJump();

	/** Check if you can WallRun Jump */
	bool CanWallRunJump() const;

	/** Override OnMovementModeChanged to constrain character to not fall on Z and reset it when no WallRunning */
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** Override to call our PhysWallRunning if Mode is CMOVE_WallRunning */
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	/** WallRunning Physics function */
	void PhysWallRunning(float deltaTime, int32 Iterations);

	/** Check if you can WallRun
	 * @return True if WallRunKey is down and WallRun is not on cooldown otherwise False
	 */
	bool CanWallRun() const;

	/** Returns true if the player is next to a wall that can be wall ran */
	bool IsNextToWall(const float VerticalTolerance = 0.0f);

	/** Finds the wall run direction and side based on the specified surface normal
	 * @param SurfaceNormal Normal of the Wall
	 * @param Direction Will be set after calculation using SurfaceNormal
	 * @param Side Wall be set after calculation using SurfaceNormal
	 */
	void FindWallRunDirectionAndSide(const FVector& SurfaceNormal, FVector& Direction, EWallRunSide& Side) const;

	/** Check if the Wall normal can be ran
	 * @param SurfaceNormal Normal of the Wall
	 * @return True if the specified surface normal can be wall ran on, otherwise False
	 */
	bool CanSurfaceBeWallRan(const FVector& SurfaceNormal) const;

	/** Check if the CustomMovementMode is the one passed
	 * @param InCustomMovementMode Custom Mode to check
	 * @return True if MovementMode is set to Custom and CustomMovementMode is equal to InCustomMovementMode otherwise False
	 */
	bool IsCustomMovementMode(const uint8 InCustomMovementMode) const;

private:

	/* If True the flags will be saved for the CompressedFlags */
	uint8 WantsToTeleport : 1;

	/** Setted by SetTeleport() */
	bool TeleportKeyDown = false;

	/** Cooldown timer used for CanTeleport() */
	float CurrentTeleportCooldown = 0.0f;

	/** Normal of the Wall to WallRun */
	FVector WallRunNormal;

	/** True if WallRun is on cooldown otherwise false */
	bool IsWallRunOnCooldown = false;

	/** Timer Handle for the WallRunCooldown */
	FTimerHandle WallRunCooldownTimerHandle;

	/** Setted by SetWallJumpKeyDown() */
	bool WallJumpKeyDown = false;

	/** if True the flags will be saved for the CompressedFlags */
	uint8 WantsToWallJump : 1;

	/** if true the flags will be saved for the CompressedFlags */
	uint8 WantsToWallRun : 1;

	/** Has to be set using the CanWallRun() function */
	bool WallRunKeyDown = false;

	/** The direction the character is currently wall running in */
	FVector WallRunDirection;

	/** The side of the wall the player hit */
	EWallRunSide WallSide;

	/** Timer Handle for the WallRun Max Time */
	FTimerHandle WallRunTimerHandle;

	/** Normal of the plane hit (point outside), change WallJumpDirection to adjust the vector */
	FVector WallJumpNormal;
};

