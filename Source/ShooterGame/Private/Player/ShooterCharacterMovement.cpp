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
}

bool UShooterCharacterMovement::BeginWallRun()
{
	if (WallRunKeyDown == true)
	{
		GetWorld()->GetTimerManager().SetTimer(WallRunTimerHandle, this, &UShooterCharacterMovement::EndWallRun, WallRunTimeMax);
		SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_WallRunning);
		return true;
	}

	return false;
}

void UShooterCharacterMovement::EndWallRun()
{
	if (WallRunTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(WallRunTimerHandle);
	}
	WallSide = EWallRunSide::kNone;
	SetMovementMode(EMovementMode::MOVE_Falling);
	StartWallRunCooldown();
}

bool UShooterCharacterMovement::CanWallRun() const
{
	if (GetPawnOwner()->IsLocallyControlled() == false || IsWallRunOnCooldown == true)
	{
		return false;
	}

	TArray<FInputActionKeyMapping> SprintKeysMapping;
	UInputSettings::GetInputSettings()->GetActionMappingByName("Run", SprintKeysMapping);
	for (const FInputActionKeyMapping& SprintKeyMapping : SprintKeysMapping)
	{
		if (GetPawnOwner()->GetController<APlayerController>()->IsInputKeyDown(SprintKeyMapping.Key))
		{
			return true;
		}
	}

	return false;
}

bool UShooterCharacterMovement::IsNextToWall(const float VerticalTolerance)
{
	// Do a line trace from the player into the wall to make sure we're stil along the side of a wall
	const FVector CrossVector = WallSide == EWallRunSide::kLeft ? FVector(0.0f, 0.0f, -1.0f) : FVector(0.0f, 0.0f, 1.0f);
	const FVector TraceStart = GetPawnOwner()->GetActorLocation() + (WallRunDirection * 20.0f);
	const FVector TraceEnd = TraceStart + (FVector::CrossProduct(WallRunDirection, CrossVector) * 100);
	FHitResult HitResult;

	auto LineTrace = [&](const FVector& Start, const FVector& End)
	{
		return (GetWorld()->LineTraceSingleByObjectType(HitResult, Start, End, ECollisionChannel::ECC_WorldStatic));
	};

	if (VerticalTolerance > FLT_EPSILON)
	{
		if (LineTrace(FVector(TraceStart.X, TraceStart.Y, TraceStart.Z + VerticalTolerance / 2.0f), FVector(TraceEnd.X, TraceEnd.Y, TraceEnd.Z + VerticalTolerance / 2.0f)) == false &&
			LineTrace(FVector(TraceStart.X, TraceStart.Y, TraceStart.Z - VerticalTolerance / 2.0f), FVector(TraceEnd.X, TraceEnd.Y, TraceEnd.Z - VerticalTolerance / 2.0f)) == false)
		{
			return false;
		}
	}
	else
	{
		if (LineTrace(TraceStart, TraceEnd) == false)
			return false;
	}

	EWallRunSide NewWallRunSide;
	FindWallRunDirectionAndSide(HitResult.ImpactNormal, WallRunDirection, NewWallRunSide);
	if (NewWallRunSide != WallSide)
	{
		return false;
	}
	WallJumpNormal = HitResult.ImpactNormal;
	return true;
}

void UShooterCharacterMovement::FindWallRunDirectionAndSide(const FVector& SurfaceNormal, FVector& Direction, EWallRunSide& Side) const
{
	FVector CrossVector;

	if (FVector2D::DotProduct(FVector2D(SurfaceNormal), FVector2D(GetPawnOwner()->GetActorRightVector())) > 0.0)
	{
		Side = EWallRunSide::kRight;
		CrossVector = FVector(0.0f, 0.0f, 1.0f);
	}
	else
	{
		Side = EWallRunSide::kLeft;
		CrossVector = FVector(0.0f, 0.0f, -1.0f);
	}

	// Find the direction parallel to the wall in the direction the player is moving
	Direction = FVector::CrossProduct(SurfaceNormal, CrossVector);
}

bool UShooterCharacterMovement::CanSurfaceBeWallRan(const FVector& SurfaceNormal) const
{
	// Return false if the surface normal is facing down
	if (SurfaceNormal.Z < -0.05f)
		return false;

	FVector NormalNoZ = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0.0f);
	NormalNoZ.Normalize();

	// Find the angle of the wall
	const float WallAngle = FMath::Acos(FVector::DotProduct(NormalNoZ, SurfaceNormal));

	// Return true if the wall angle is less than the walkable floor angle
	return WallAngle < GetWalkableFloorAngle();
}

bool UShooterCharacterMovement::IsCustomMovementMode(const uint8 InCustomMovementMode) const
{
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

void UShooterCharacterMovement::OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_WallRunning))
		return;

	if (IsFalling() == false)
		return;

	if (CanSurfaceBeWallRan(Hit.ImpactNormal) == false)
		return;

	FindWallRunDirectionAndSide(Hit.ImpactNormal, WallRunDirection, WallSide);

	if (IsNextToWall() == false)
		return;

	BeginWallRun();
}

void UShooterCharacterMovement::BeginPlay()
{
	Super::BeginPlay();

	if (GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
	{
		GetPawnOwner()->OnActorHit.AddDynamic(this, &UShooterCharacterMovement::OnActorHit);
	}
}

void UShooterCharacterMovement::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (GetPawnOwner() != nullptr && GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
	{
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
		WallRunKeyDown = CanWallRun();
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UShooterCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (MovementMode == MOVE_Custom)
	{
		if(CustomMovementMode == ECustomMovementMode::CMOVE_WallRunning)
		{
			StopMovementImmediately();
			bConstrainToPlane = true;
			SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 1.0f));
		}
	}

	if (PreviousMovementMode == MOVE_Custom)
	{
		if (PreviousCustomMode == ECustomMovementMode::CMOVE_WallRunning)
		{
			bConstrainToPlane = false;
		}
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UShooterCharacterMovement::PhysCustom(float deltaTime, int32 Iterations)
{
	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}

	if(CustomMovementMode == ECustomMovementMode::CMOVE_WallRunning)
	{
		PhysWallRunning(deltaTime, Iterations);
	}

	Super::PhysCustom(deltaTime, Iterations);
}

void UShooterCharacterMovement::PhysWallRunning(float deltaTime, int32 Iterations)
{
	if (WallRunKeyDown == false)
	{
		EndWallRun();
		return;
	}

	if (IsNextToWall(LineTraceVerticalTolerance) == false)
	{
		EndWallRun();
		return;
	}

	FVector newVelocity = WallRunDirection;
	newVelocity.X *= WallRunSpeed;
	newVelocity.Y *= WallRunSpeed;
	newVelocity.Z *= 0.0f;
	Velocity = newVelocity;

	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	if(!SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit))
	{
		EndWallRun();
	}
}

void UShooterCharacterMovement::ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, RemainingTime, Iterations);
	WallSide = EWallRunSide::kNone;
	if (IsCustomMovementMode(ECustomMovementMode::CMOVE_WallRunning))
	{
		EndWallRun();
	}
}

FVector UShooterCharacterMovement::ForwardVelocityOnWall(const float InWallRunDirection) const
{
	const FVector CurrentForwardOnWall = FVector::CrossProduct(WallRunNormal, FVector::UpVector);
	return CurrentForwardOnWall * ((WallRunSpeed) * InWallRunDirection);
}

void UShooterCharacterMovement::StartWallRunCooldown()
{
	IsWallRunOnCooldown = true;
	GetWorld()->GetTimerManager().SetTimer(WallRunCooldownTimerHandle, this, &UShooterCharacterMovement::ReEnableWallRunAfterCooldown, WallRunCooldownAfterFall);
}

void UShooterCharacterMovement::ReEnableWallRunAfterCooldown()
{
	if (WallRunCooldownTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(WallRunCooldownTimerHandle);
	}
	IsWallRunOnCooldown = false;
}

void UShooterCharacterMovement::CameraTick() const
{
	if(!IsCustomMovementMode(ECustomMovementMode::CMOVE_WallRunning) || WallSide == EWallRunSide::kNone)
	{
		CameraTilt(0.0f);
		return;
	}
	if(WallSide == EWallRunSide::kRight)
	{
		CameraTilt(15.0f);
	}
	else
	{
		CameraTilt(-15.0f);
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
		return (MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_WallRunning) || (MovementMode == MOVE_Falling);
	}
	return false;
}

void UShooterCharacterMovement::WallRunJump()
{
	if(IsCustomMovementMode(ECustomMovementMode::CMOVE_WallRunning))
	{
		EndWallRun();
		const FVector WallRunJumpLaunch = FVector(WallRunDirection.X * WallRunOffJumpForceXY, WallRunDirection.Y * WallRunOffJumpForceXY, WallRunOffJumpForceZ);
		CharacterOwner->LaunchCharacter(WallRunJumpLaunch, false, true);
	}
	else if(MovementMode == MOVE_Falling)
	{
		if(IsNextToWall())
		{
			FVector NewNormal;
			if (WallSide == EWallRunSide::kLeft)
			{
				NewNormal = WallJumpNormal.RotateAngleAxis(WallJumpDirection, FVector::UpVector);
			}
			else if(WallSide == EWallRunSide::kRight)
			{
				NewNormal = WallJumpNormal.RotateAngleAxis(-WallJumpDirection, FVector::UpVector);
			}
			else
			{
				return;
			}
			const FVector WallRunJumpLaunch = FVector(NewNormal.X * WallJumpOffJumpForceX, NewNormal.Y * WallJumpOffJumpForceY, WallJumpOffJumpForceZ);
			CharacterOwner->LaunchCharacter(WallRunJumpLaunch, true, true);
		}
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