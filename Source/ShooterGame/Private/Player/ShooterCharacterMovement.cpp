// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "ECustomMovementMode.h"
#include "Kismet/KismetMathLibrary.h"
#include "Player/ShooterCharacterMovement.h"

//----------------------------------------------------------------------//
// UPawnMovementComponent
//----------------------------------------------------------------------//
UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentTeleportCooldown = TeleportCooldown;
	DefaultGravity = GravityScale;
}

bool UShooterCharacterMovement::BeginWallRun()
{
	// Only allow wall running to begin if the required keys are down
	if (WallRunKeyDown == true)
	{
		// Set the movement mode to wall running. UE4 will handle replicating this change to all connected clients.
		SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_WallRunning);
		return true;
	}

	return false;
}

void UShooterCharacterMovement::EndWallRun()
{
	// Set the movement mode back to falling
	WallRunSide = EWallRunSide::kNone;
	SetMovementMode(EMovementMode::MOVE_Falling);
}

bool UShooterCharacterMovement::AreRequiredWallRunKeysDown() const
{
	// Since this function is checking for input, it should only be called for locally controlled character
	if (GetPawnOwner()->IsLocallyControlled() == false)
		return false;

	// Make sure the spring key is down (the player may only wall run if he's hold sprint)
	TArray<FInputActionKeyMapping> sprintKeyMappings;
	UInputSettings::GetInputSettings()->GetActionMappingByName("Run", sprintKeyMappings);
	for (FInputActionKeyMapping& sprintKeyMapping : sprintKeyMappings)
	{
		if (GetPawnOwner()->GetController<APlayerController>()->IsInputKeyDown(sprintKeyMapping.Key))
		{
			return true;
		}
	}

	return false;
}

bool UShooterCharacterMovement::IsNextToWall(float vertical_tolerance)
{
	// Do a line trace from the player into the wall to make sure we're stil along the side of a wall
	FVector crossVector = WallRunSide == EWallRunSide::kLeft ? FVector(0.0f, 0.0f, -1.0f) : FVector(0.0f, 0.0f, 1.0f);
	FVector traceStart = GetPawnOwner()->GetActorLocation() + (WallRunDirection * 20.0f);
	FVector traceEnd = traceStart + (FVector::CrossProduct(WallRunDirection, crossVector) * 100);
	FHitResult hitResult;

	// Create a helper lambda for performing the line trace
	auto lineTrace = [&](const FVector& start, const FVector& end)
	{
		return (GetWorld()->LineTraceSingleByObjectType(hitResult, start, end, ECollisionChannel::ECC_WorldStatic));
	};

	// If a vertical tolerance was provided we want to do two line traces - one above and one below the calculated line
	if (vertical_tolerance > FLT_EPSILON)
	{
		// If both line traces miss the wall then return false, we're not next to a wall
		if (lineTrace(FVector(traceStart.X, traceStart.Y, traceStart.Z + vertical_tolerance / 2.0f), FVector(traceEnd.X, traceEnd.Y, traceEnd.Z + vertical_tolerance / 2.0f)) == false &&
			lineTrace(FVector(traceStart.X, traceStart.Y, traceStart.Z - vertical_tolerance / 2.0f), FVector(traceEnd.X, traceEnd.Y, traceEnd.Z - vertical_tolerance / 2.0f)) == false)
		{
			return false;
		}
	}
	// If no vertical tolerance was provided we just want to do one line trace using the caclulated line
	else
	{
		// return false if the line trace misses the wall
		if (lineTrace(traceStart, traceEnd) == false)
			return false;
	}

	// Make sure we're still on the side of the wall we expect to be on
	EWallRunSide newWallRunSide;
	FindWallRunDirectionAndSide(hitResult.ImpactNormal, WallRunDirection, newWallRunSide);
	if (newWallRunSide != WallRunSide)
	{
		return false;
	}

	return true;
}

void UShooterCharacterMovement::FindWallRunDirectionAndSide(const FVector& surface_normal, FVector& direction, EWallRunSide& side) const
{
	FVector crossVector;

	if (FVector2D::DotProduct(FVector2D(surface_normal), FVector2D(GetPawnOwner()->GetActorRightVector())) > 0.0)
	{
		side = EWallRunSide::kRight;
		crossVector = FVector(0.0f, 0.0f, 1.0f);
	}
	else
	{
		side = EWallRunSide::kLeft;
		crossVector = FVector(0.0f, 0.0f, -1.0f);
	}

	// Find the direction parallel to the wall in the direction the player is moving
	direction = FVector::CrossProduct(surface_normal, crossVector);
}

bool UShooterCharacterMovement::CanSurfaceBeWallRan(const FVector& surface_normal) const
{
	// Return false if the surface normal is facing down
	if (surface_normal.Z < -0.05f)
		return false;

	FVector normalNoZ = FVector(surface_normal.X, surface_normal.Y, 0.0f);
	normalNoZ.Normalize();

	// Find the angle of the wall
	float wallAngle = FMath::Acos(FVector::DotProduct(normalNoZ, surface_normal));

	// Return true if the wall angle is less than the walkable floor angle
	return wallAngle < GetWalkableFloorAngle();
}

bool UShooterCharacterMovement::IsCustomMovementMode(uint8 custom_movement_mode) const
{
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == custom_movement_mode;
}

void UShooterCharacterMovement::OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_WallRunning))
		return;

	// Make sure we're falling. Wall running can only begin if we're currently in the air
	if (IsFalling() == false)
		return;

	// Make sure the surface can be wall ran based on the angle of the surface that we hit
	if (CanSurfaceBeWallRan(Hit.ImpactNormal) == false)
		return;

	// Update the wall run direction and side
	FindWallRunDirectionAndSide(Hit.ImpactNormal, WallRunDirection, WallRunSide);

	// Make sure we're next to a wall
	if (IsNextToWall() == false)
		return;

	BeginWallRun();
}

void UShooterCharacterMovement::BeginPlay()
{
	Super::BeginPlay();

	// We don't want simulated proxies detecting their own collision
	if (GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
	{
		// Bind to the OnActorHot component so we're notified when the owning actor hits something (like a wall)
		GetPawnOwner()->OnActorHit.AddDynamic(this, &UShooterCharacterMovement::OnActorHit);
	}
}

void UShooterCharacterMovement::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (GetPawnOwner() != nullptr && GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
	{
		// Unbind from all events
		GetPawnOwner()->OnActorHit.RemoveDynamic(this, &UShooterCharacterMovement::OnActorHit);
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UShooterCharacterMovement::SetTeleportKeyDown(bool bTeleport)
{
	TeleportKeyDown = bTeleport;
}

void UShooterCharacterMovement::SetWallJumpKeyDown(bool bWallJump)
{
	WallJumpKeyDown = bWallJump;
}

void UShooterCharacterMovement::Teleport()
{
	const bool bLimitRotation = (CharacterOwner->GetCharacterMovement()->IsMovingOnGround() || CharacterOwner->GetCharacterMovement()->IsFalling());
	const FRotator Rotation = bLimitRotation ? CharacterOwner->GetActorRotation() : CharacterOwner->Controller->GetControlRotation();
	const FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
	CharacterOwner->AddActorWorldOffset(Direction * TeleportDistance, true, nullptr, ETeleportType::TeleportPhysics);
}

bool UShooterCharacterMovement::CanTeleport()
{
	if (CurrentTeleportCooldown < 0.0f)
	{
		return true;
	}
	CurrentTeleportCooldown -= GetWorld()->DeltaTimeSeconds;
	return false;
}

void UShooterCharacterMovement::ResetTeleportTimer()
{
	CurrentTeleportCooldown = TeleportCooldown;

}

void UShooterCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	WantsToTeleport = ((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
	WantsToWallJump = ((Flags & FSavedMove_Character::FLAG_Custom_1) != 0);
	WallRunKeyDown = ((Flags & FSavedMove_Character::FLAG_Custom_2) != 0);
	if (CharacterOwner->GetLocalRole() == ROLE_Authority)
	{
		if (WantsToTeleport == true)
		{
			Teleport();
		}
		if(WantsToWallJump)
		{
			WallRunJump();
		}
	}
}

FNetworkPredictionData_Client* UShooterCharacterMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UShooterCharacterMovement* MutableThis = const_cast<UShooterCharacterMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ShooterCharacter(*this);
	}
	return ClientPredictionData;
}

void UShooterCharacterMovement::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	if (GetPawnOwner()->IsLocallyControlled())
	{
		CameraTick();
		if (CanTeleport() && TeleportKeyDown == true)
		{
			ResetTeleportTimer();
			Teleport();
			WantsToTeleport = true;
		}
		else
		{
			WantsToTeleport = false;
		}
		if(CanWallRunJump() && WallJumpKeyDown == true)
		{
			WallRunJump();
			WantsToWallJump = true;
		}
		else
		{
			WantsToWallJump = false;
		}
		WallRunKeyDown = AreRequiredWallRunKeysDown();
	}
	//if(CharacterOwner->GetLocalRole() > ROLE_SimulatedProxy && !IsWallRunOnCooldown)
	//{
	//	WallRunUpdate();
	//}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UShooterCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (MovementMode == MOVE_Custom)
	{
		switch (CustomMovementMode)
		{
			// Did we just start wall running?
		case ECustomMovementMode::CMOVE_WallRunning:
		{
			// Stop current movement and constrain the character to only horizontal movement
			StopMovementImmediately();
			bConstrainToPlane = true;
			SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 1.0f));
		}
		break;
		}
	}

	if (PreviousMovementMode == MOVE_Custom)
	{
		switch (PreviousCustomMode)
		{
			// Did we just finish wall running?
		case ECustomMovementMode::CMOVE_WallRunning:
		{
			// Unconstrain the character from horizontal movement
			bConstrainToPlane = false;
		}
		break;
		}
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UShooterCharacterMovement::PhysCustom(float deltaTime, int32 Iterations)
{
	// Phys* functions should only run for characters with ROLE_Authority or ROLE_AutonomousProxy. However, Unreal calls PhysCustom in
	// two seperate locations, one of which doesn't check the role, so we must check it here to prevent this code from running on simulated proxies.
	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
		return;

	switch (CustomMovementMode)
	{
	case ECustomMovementMode::CMOVE_WallRunning:
	{
		PhysWallRunning(deltaTime, Iterations);
		break;
	}
	}

	// Not sure if this is needed
	Super::PhysCustom(deltaTime, Iterations);
}

void UShooterCharacterMovement::PhysWallRunning(float deltaTime, int32 Iterations)
{
	// IMPORTANT NOTE: This function (and all other Phys* functions) will be called on characters with ROLE_Authority and ROLE_AutonomousProxy
	// but not ROLE_SimulatedProxy. All movement should be performed in this function so that is runs locally and on the server. UE4 will handle
	// replicating the final position, velocity, etc.. to the other simulated proxies.

	// Make sure the required wall run keys are still down
	if (WallRunKeyDown == false)
	{
		EndWallRun();
		return;
	}

	// Make sure we're still next to a wall. Provide a vertial tolerance for the line trace since it's possible the the server has
	// moved our character slightly since we've began the wall run. In the event we're right at the top/bottom of a wall we need this
	// tolerance value so we don't immiedetly fall of the wall 
	if (IsNextToWall(LineTraceVerticalTolerance) == false)
	{
		EndWallRun();
		return;
	}

	// Set the owning player's new velocity based on the wall run direction
	FVector newVelocity = WallRunDirection;
	newVelocity.X *= WallRunSpeed;
	newVelocity.Y *= WallRunSpeed;
	newVelocity.Z *= 0.0f;
	Velocity = newVelocity;

	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
}

void UShooterCharacterMovement::ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, RemainingTime, Iterations);
	//WallRunEnd();
	//ReEnableWallRunAfterCooldown();
	WallRunSide = EWallRunSide::kNone;
	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_WallRunning))
	{
		EndWallRun();
	}
}

void UShooterCharacterMovement::WallRunUpdate()
{
	FVector Right;
	FVector Left;
	WallRunEndVectors(Right, Left);
	if(WallRunMovement(CharacterOwner->GetActorLocation(), Right, -1.0f))
	{
		IsWallRunning = IsWallRunningR = true;
		IsWallRunningL = false;
		GravityScale = FMath::FInterpTo(GravityScale, WallRunTargetGravity, GetWorld()->DeltaTimeSeconds, 10.0f);
		
	}
	else if(!IsWallRunningR)
	{
		if (WallRunMovement(CharacterOwner->GetActorLocation(), Left, 1.0f))
		{
			IsWallRunning = IsWallRunningL = true;
			IsWallRunningR = false;
			GravityScale = FMath::FInterpTo(GravityScale, WallRunTargetGravity, GetWorld()->DeltaTimeSeconds, 10.0f);
		}
		else
		{
			WallRunEnd();
			StartWallRunCooldown();
		}
	}
	else
	{
		WallRunEnd();
		StartWallRunCooldown();
	}
}

void UShooterCharacterMovement::WallRunEndVectors(FVector& Right, FVector& Left) const
{
	const FVector CharacterLocation = CharacterOwner->GetActorLocation();
	const bool bLimitRotation = (CharacterOwner->GetCharacterMovement()->IsMovingOnGround() || CharacterOwner->GetCharacterMovement()->IsFalling());
	const FRotator Rotation = bLimitRotation ? CharacterOwner->GetActorRotation() : CharacterOwner->Controller->GetControlRotation();
	const FVector ForwardDirection = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X).GetSafeNormal();
	const FVector RightDirection = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y).GetSafeNormal();
	Right = CharacterLocation + RightDirection * 100 + ForwardDirection * -50;
	Left = CharacterLocation + RightDirection * -100 + ForwardDirection * -50;
}

bool UShooterCharacterMovement::WallRunMovement(FVector StartLineTrace, FVector& EndLineTrace, float InWallRunDirection)
{
	FHitResult Hit;
	const FName TraceTag("MyTraceTag");

	GetWorld()->DebugDrawTraceTag = TraceTag;

	FCollisionQueryParams CollisionParams;
	CollisionParams.TraceTag = TraceTag;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, StartLineTrace, EndLineTrace, ECollisionChannel::ECC_WorldStatic, CollisionParams))
	{
		if(IsValidWallRunVector(Hit.Normal) && IsFalling())
		{
			WallRunNormal = Hit.Normal;
			CharacterOwner->LaunchCharacter(ForwardVelocityOnWall(InWallRunDirection), true, true);
			return true;
		}
	}
	return false;
}

bool UShooterCharacterMovement::IsValidWallRunVector(const FVector& InVector) const
{
	if (InVector.Z < -0.05f)
		return false;

	FVector NormalNoZ = FVector(InVector.X, InVector.Y, 0.0f);
	NormalNoZ.Normalize();

	// Find the angle of the wall
	const float WallAngle = FMath::Acos(FVector::DotProduct(NormalNoZ, InVector));

	// Return true if the wall angle is less than the walkable floor angle
	return WallAngle < GetWalkableFloorAngle();
}

FVector UShooterCharacterMovement::ForwardVelocityOnWall(const float InWallRunDirection) const
{
	const FVector CurrentForwardOnWall = FVector::CrossProduct(WallRunNormal, FVector::UpVector);
	return CurrentForwardOnWall * ((WallRunSpeed) * InWallRunDirection);
}

void UShooterCharacterMovement::WallRunEnd()
{
	IsWallRunning = IsWallRunningR = IsWallRunningL = false;
	GravityScale = DefaultGravity;
}

void UShooterCharacterMovement::StartWallRunCooldown()
{
	IsWallRunOnCooldown = true;
	GetWorld()->GetTimerManager().SetTimer(WallRunTimerHandle, this, &UShooterCharacterMovement::ReEnableWallRunAfterCooldown, 0.35f);
}

void UShooterCharacterMovement::ReEnableWallRunAfterCooldown()
{
	if (WallRunTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(WallRunTimerHandle);
	}
	IsWallRunOnCooldown = false;
}

void UShooterCharacterMovement::CameraTick() const
{
	if(WallRunSide == EWallRunSide::kRight)
	{
		CameraTilt(15.0f);
	}
	else if(WallRunSide == EWallRunSide::kLeft)
	{
		CameraTilt(-15.0f);
	}
	else
	{
		CameraTilt(0.0f);
	}
}

void UShooterCharacterMovement::CameraTilt(const float TargetXRoll) const
{
	FRotator ControllerRotation = CharacterOwner->Controller->GetControlRotation();
	ControllerRotation.Roll = TargetXRoll;
	CharacterOwner->Controller->SetControlRotation(FMath::RInterpTo(CharacterOwner->Controller->GetControlRotation(), ControllerRotation, GetWorld()->DeltaTimeSeconds, 10.0f));
}

bool UShooterCharacterMovement::CanWallRunJump() const
{
	if (GetPawnOwner()->IsLocallyControlled())
	{
		return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_WallRunning;
	}
	return false;
}

void UShooterCharacterMovement::WallRunJump()
{
	if(IsCustomMovementMode(ECustomMovementMode::CMOVE_WallRunning))
	{
		EndWallRun();
		/*StartWallRunCooldown();*/
		const FVector WallRunJumpLaunch = FVector(WallRunDirection.X * WallRunOffJumpForceXY, WallRunDirection.Y * WallRunOffJumpForceXY, WallRunOffJumpForceZ);
		CharacterOwner->LaunchCharacter(WallRunJumpLaunch, false, true);
	}
}

float UShooterCharacterMovement::GetMaxSpeed() const
{
	float MaxSpeed = Super::GetMaxSpeed();

	const AShooterCharacter* ShooterCharacterOwner = Cast<AShooterCharacter>(PawnOwner);
	if (ShooterCharacterOwner)
	{
		if (ShooterCharacterOwner->IsTargeting())
		{
			MaxSpeed *= ShooterCharacterOwner->GetTargetingSpeedModifier();
		}
		if (ShooterCharacterOwner->IsRunning())
		{
			MaxSpeed *= ShooterCharacterOwner->GetRunningSpeedModifier();
		}
	}

	return MaxSpeed;
}

void FSavedMove_ShooterCharacter::Clear()
{
	Super::Clear();

	// Clear all values
	SavedWantsToTeleport = 0;
	SavedWantsToWallJump = 0;
	SavedWantsToWallRun = 0;
}

uint8 FSavedMove_ShooterCharacter::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (SavedWantsToTeleport)
	{
		Result |= FLAG_Custom_0;
	}
	if(SavedWantsToWallJump)
	{
		Result |= FLAG_Custom_1;
	}
	if(SavedWantsToWallRun)
	{
		Result |= FLAG_Custom_2;
	}
	return Result;
}

bool FSavedMove_ShooterCharacter::CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const
{
	const FSavedMove_ShooterCharacter* NewMove = static_cast<const FSavedMove_ShooterCharacter*>(NewMovePtr.Get());

	// As an optimization, check if the engine can combine saved moves.
	if (SavedWantsToTeleport != NewMove->SavedWantsToTeleport ||
		SavedWantsToWallJump != NewMove->SavedWantsToWallJump ||
		SavedWantsToWallRun != NewMove->SavedWantsToWallRun)
	{
		return false;
	}
	return Super::CanCombineWith(NewMovePtr, Character, MaxDelta);

}

void FSavedMove_ShooterCharacter::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	const UShooterCharacterMovement* DefaultCharacterMovement = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (DefaultCharacterMovement)
	{
		/** Copy value into saved mode */
		SavedWantsToTeleport = DefaultCharacterMovement->WantsToTeleport;
		SavedWantsToWallJump = DefaultCharacterMovement->WantsToWallJump;
		SavedWantsToWallRun = DefaultCharacterMovement->WallRunKeyDown;
	}
}

void FSavedMove_ShooterCharacter::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	UShooterCharacterMovement* DefaultCharacterMovement = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (DefaultCharacterMovement)
	{
		DefaultCharacterMovement->WantsToTeleport = SavedWantsToTeleport;
		DefaultCharacterMovement->WantsToWallJump = SavedWantsToWallJump;
		DefaultCharacterMovement->WallRunKeyDown = SavedWantsToWallRun;
	}
}

FNetworkPredictionData_Client_ShooterCharacter::FNetworkPredictionData_Client_ShooterCharacter(
	const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_ShooterCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_ShooterCharacter());
}