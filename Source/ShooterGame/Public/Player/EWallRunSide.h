#pragma once


#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum class EWallRunSide : uint8
{
	kNone   UMETA(DisplayName = "None", ToolTip = "Not Running along any side"),
	kLeft 	UMETA(DisplayName = "Left", ToolTip = "Running along the left side of a wall"),
	kRight	UMETA(DisplayName = "Right", ToolTip = "Running along the right side of a wall"),
};