// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsDemoCharacter.h"
#include "PhysicsDemoProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// APhysicsDemoCharacter

APhysicsDemoCharacter::APhysicsDemoCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	GrabbedObjectLocation = CreateDefaultSubobject<USceneComponent>(TEXT("GrabbedObjectLocation"));
	GrabbedObjectLocation->SetupAttachment(FP_Gun);
}

void APhysicsDemoCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

}

//////////////////////////////////////////////////////////////////////////
// Input

void APhysicsDemoCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &APhysicsDemoCharacter::OnFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &APhysicsDemoCharacter::EndFire);

	// Bind launch event
	PlayerInputComponent->BindAction("Launch", IE_Pressed, this, &APhysicsDemoCharacter::OnLaunch);

	// Bind Weapon change event
	PlayerInputComponent->BindAction("ShotWeapon", IE_Released, this, &APhysicsDemoCharacter::SetToShotWeapon);
	PlayerInputComponent->BindAction("GravityWeapon", IE_Released, this, &APhysicsDemoCharacter::SetToGravityWeapon);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &APhysicsDemoCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APhysicsDemoCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &APhysicsDemoCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &APhysicsDemoCharacter::LookUpAtRate);
}

void APhysicsDemoCharacter::OnFire()
{
	// try and fire a projectile
	if (!bGravityWeapon && ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			const FRotator SpawnRotation = GetControlRotation();
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
			const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

			//Set Spawn Collision Handling Override
			FActorSpawnParameters ActorSpawnParams;
			ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

			// spawn the projectile at the muzzle
			World->SpawnActor<APhysicsDemoProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
		}
		
		// try and play the sound if specified
		if (FireSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
		}

		// try and play a firing animation if specified
		if (FireAnimation != nullptr)
		{
			// Get the animation object for the arms mesh
			UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
			if (AnimInstance != nullptr)
			{
				AnimInstance->Montage_Play(FireAnimation, 1.f);
			}
		}
	}
	
	else if (bGravityWeapon)
	{
		const FCollisionQueryParams QueryParams("GravityGunTrace", false, this);
		const float TraceRange = 5000.0f;
		const FVector StartTrace = FirstPersonCameraComponent->GetComponentLocation();
		const FVector EndTrace = (FirstPersonCameraComponent->GetForwardVector() * TraceRange) + StartTrace;
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, ECC_Visibility, QueryParams))
		{
			if (UPrimitiveComponent* Prim = Hit.GetComponent())
			{
				if (Prim->IsSimulatingPhysics())
				{
					SetGrabbedObject(Prim);
				}
			}
		}
	}
}

void APhysicsDemoCharacter::EndFire()
{
	if (bGravityWeapon)
	{
		if (GrabbedObject)
		{
			//const float ShotStrength = 5000.f;
			//const FVector ShotVelocity = FirstPersonCameraComponent->GetForwardVector() * ShotStrength;

			GrabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			GrabbedObject->SetSimulatePhysics(true);
			//GrabbedObject->AddImpulse(ShotVelocity, NAME_None, true);

			SetGrabbedObject(nullptr);
		}
		
	}
}

void APhysicsDemoCharacter::OnLaunch()
{
	if (bGravityWeapon && GrabbedObject)
	{
		const float ShotStrength = 5000.f;
		const FVector ShotVelocity = FirstPersonCameraComponent->GetForwardVector() * ShotStrength;

		GrabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		GrabbedObject->SetSimulatePhysics(true);
		GrabbedObject->AddImpulse(ShotVelocity, NAME_None, true);

		SetGrabbedObject(nullptr);
			
		// try and play the sound if specified
		if (FireSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
		}

		// try and play a firing animation if specified
		if (FireAnimation != nullptr)
		{
			// Get the animation object for the arms mesh
			UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
			if (AnimInstance != nullptr)
			{
				AnimInstance->Montage_Play(FireAnimation, 1.f);
			}
		}
	}
}


void APhysicsDemoCharacter::SetGrabbedObject(UPrimitiveComponent* ObjectToGrab)
{
	GrabbedObject = ObjectToGrab;

	if (GrabbedObject)
	{
		GrabbedObject->SetSimulatePhysics(false);
		GrabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		GrabbedObject->AttachToComponent(GrabbedObjectLocation, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}



void APhysicsDemoCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void APhysicsDemoCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void APhysicsDemoCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void APhysicsDemoCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void APhysicsDemoCharacter::SetToShotWeapon()
{
	bGravityWeapon = false;
	UE_LOG(LogTemp, Warning, TEXT("Changing to ShotWeapon"));
}

void APhysicsDemoCharacter::SetToGravityWeapon()
{
	bGravityWeapon = true;
	UE_LOG(LogTemp, Warning, TEXT("Changing to GravityWeapon"));
}



