// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Player/ShooterCharacterMovement.h"

//----------------------------------------------------------------------//
// UPawnMovementComponent
//----------------------------------------------------------------------//
UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentTeleportCooldown = TeleportCooldown;
}

void UShooterCharacterMovement::SetTeleport(bool bTeleport)
{
	TeleportKeyDown = bTeleport;
}

void UShooterCharacterMovement::Teleport()
{
	const bool bLimitRotation = (CharacterOwner->GetCharacterMovement()->IsMovingOnGround() || CharacterOwner->GetCharacterMovement()->IsFalling());
	const FRotator Rotation = bLimitRotation ? CharacterOwner->GetActorRotation() : CharacterOwner->Controller->GetControlRotation();
	const FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
	CharacterOwner->AddActorWorldOffset(Direction * TeleportDistance,true,nullptr,ETeleportType::TeleportPhysics);
}

bool UShooterCharacterMovement::CanTeleport()
{
	if(CurrentTeleportCooldown < 0.0f)
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
	if (CharacterOwner->GetLocalRole() == ROLE_Authority)
	{
		if (WantsToTeleport == true)
		{
			Teleport();
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
	if(GetPawnOwner()->IsLocallyControlled())
	{
		if(CanTeleport() && TeleportKeyDown == true)
		{
			ResetTeleportTimer();
			Teleport();
			WantsToTeleport = true;
		}
		else
		{
			WantsToTeleport = false;
		}
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
}

uint8 FSavedMove_ShooterCharacter::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (SavedWantsToTeleport)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

bool FSavedMove_ShooterCharacter::CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const
{
	const FSavedMove_ShooterCharacter* NewMove = static_cast<const FSavedMove_ShooterCharacter*>(NewMovePtr.Get());

	// As an optimization, check if the engine can combine saved moves.
	if (SavedWantsToTeleport != NewMove->SavedWantsToTeleport)
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
	}
}

void FSavedMove_ShooterCharacter::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	UShooterCharacterMovement* DefaultCharacterMovement = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (DefaultCharacterMovement)
	{
		DefaultCharacterMovement->WantsToTeleport = SavedWantsToTeleport;
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