// Attribute Sample - game mode

#include "AttributeSampleGameMode.h"

#include "AttributeSampleCameraPawn.h"
#include "AttributeSamplePlayerController.h"

AAttributeSampleGameMode::AAttributeSampleGameMode()
{
	DefaultPawnClass = AAttributeSampleCameraPawn::StaticClass();
	PlayerControllerClass = AAttributeSamplePlayerController::StaticClass();
}
