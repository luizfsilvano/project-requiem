// Copyright Project Requiem. All Rights Reserved.

#include "Editor/RequiemConfigureLayeredRecoveryCommandlet.h"

#if WITH_EDITOR

#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimationGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/EditorAssetSubsystem.h"

namespace RequiemLayeredRecoverySetup
{
constexpr TCHAR AnimationBlueprintPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Locomotion/")
	TEXT("ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion");
const FName DefaultSlotName(TEXT("DefaultSlot"));
const FName UpperBodySlotName(TEXT("SwordRecoveryUpperBody"));
const FName UpperBodySlotGroupName(TEXT("SwordRecoveryGroup"));
const FName UpperBodyRootBoneName(TEXT("spine_01"));
constexpr TCHAR LocomotionCacheName[] = TEXT("PR_LocomotionBase");

template <typename TNode>
TNode* SpawnAnimNode(UAnimationGraph* Graph, const int32 X, const int32 Y)
{
	FGraphNodeCreator<TNode> NodeCreator(*Graph);
	TNode* Node = NodeCreator.CreateNode(false);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	NodeCreator.Finalize();
	return Node;
}

TArray<UEdGraphPin*> FindPosePins(
	UEdGraphNode* Node,
	const EEdGraphPinDirection Direction)
{
	TArray<UEdGraphPin*> Result;
	if (!Node)
	{
		return Result;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin
			&& Pin->Direction == Direction
			&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& Pin->PinType.PinSubCategoryObject == FPoseLink::StaticStruct())
		{
			Result.Add(Pin);
		}
	}
	return Result;
}

bool ConnectPose(
	UAnimationGraph* Graph,
	UEdGraphNode* OutputNode,
	const int32 OutputIndex,
	UEdGraphNode* InputNode,
	const int32 InputIndex)
{
	const TArray<UEdGraphPin*> Outputs = FindPosePins(OutputNode, EGPD_Output);
	const TArray<UEdGraphPin*> Inputs = FindPosePins(InputNode, EGPD_Input);
	return Outputs.IsValidIndex(OutputIndex)
		&& Inputs.IsValidIndex(InputIndex)
		&& Graph->GetSchema()->TryCreateConnection(
			Outputs[OutputIndex],
			Inputs[InputIndex]);
}

bool IsPoseConnected(
	UEdGraphNode* OutputNode,
	const int32 OutputIndex,
	UEdGraphNode* InputNode,
	const int32 InputIndex)
{
	const TArray<UEdGraphPin*> Outputs = FindPosePins(OutputNode, EGPD_Output);
	const TArray<UEdGraphPin*> Inputs = FindPosePins(InputNode, EGPD_Input);
	return Outputs.IsValidIndex(OutputIndex)
		&& Inputs.IsValidIndex(InputIndex)
		&& Outputs[OutputIndex]->LinkedTo.Contains(Inputs[InputIndex]);
}

bool HasExpectedLayeredTopology(
	UAnimGraphNode_Root* RootNode,
	UAnimGraphNode_Slot* DefaultSlotNode,
	UAnimGraphNode_SaveCachedPose* LocomotionCacheNode,
	UAnimGraphNode_Slot* UpperBodySlotNode,
	UAnimGraphNode_LayeredBoneBlend* LayeredNode,
	const TArray<UAnimGraphNode_UseCachedPose*>& UseCachedPoseNodes)
{
	if (!RootNode
		|| !DefaultSlotNode
		|| !LocomotionCacheNode
		|| !UpperBodySlotNode
		|| !LayeredNode
		|| UseCachedPoseNodes.Num() != 2
		|| DefaultSlotNode->Node.SlotName != DefaultSlotName
		|| UpperBodySlotNode->Node.SlotName != UpperBodySlotName
		|| LocomotionCacheNode->CacheName != LocomotionCacheName
		|| LocomotionCacheNode->Node.CachePoseName != LocomotionCacheName
		|| LayeredNode->Node.BlendMode != ELayeredBoneBlendMode::BranchFilter
		|| LayeredNode->Node.LayerSetup.Num() != 1
		|| LayeredNode->Node.LayerSetup[0].BranchFilters.Num() != 1
		|| LayeredNode->Node.BlendWeights.Num() != 1
		|| !FMath::IsNearlyEqual(LayeredNode->Node.BlendWeights[0], 1.0f)
		|| !LayeredNode->Node.bMeshSpaceRotationBlend
		|| !LayeredNode->Node.bBlendRootMotionBasedOnRootBone)
	{
		return false;
	}

	const FBranchFilter& UpperBodyFilter =
		LayeredNode->Node.LayerSetup[0].BranchFilters[0];
	if (UpperBodyFilter.BoneName != UpperBodyRootBoneName
		|| UpperBodyFilter.BlendDepth != 2
		|| !IsPoseConnected(DefaultSlotNode, 0, LocomotionCacheNode, 0)
		|| !IsPoseConnected(UpperBodySlotNode, 0, LayeredNode, 1)
		|| !IsPoseConnected(LayeredNode, 0, RootNode, 0))
	{
		return false;
	}

	UAnimGraphNode_UseCachedPose* BasePoseNode = nullptr;
	UAnimGraphNode_UseCachedPose* UpperSourceNode = nullptr;
	for (UAnimGraphNode_UseCachedPose* UseCachedPoseNode : UseCachedPoseNodes)
	{
		if (!UseCachedPoseNode
			|| UseCachedPoseNode->SaveCachedPoseNode.Get() != LocomotionCacheNode)
		{
			return false;
		}
		if (IsPoseConnected(UseCachedPoseNode, 0, LayeredNode, 0))
		{
			BasePoseNode = UseCachedPoseNode;
		}
		if (IsPoseConnected(UseCachedPoseNode, 0, UpperBodySlotNode, 0))
		{
			UpperSourceNode = UseCachedPoseNode;
		}
	}

	return BasePoseNode
		&& UpperSourceNode
		&& BasePoseNode != UpperSourceNode;
}
}

URequiemConfigureLayeredRecoveryCommandlet::URequiemConfigureLayeredRecoveryCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 URequiemConfigureLayeredRecoveryCommandlet::Main(const FString& Params)
{
	using namespace RequiemLayeredRecoverySetup;

	UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(
		nullptr,
		AnimationBlueprintPath);
	if (!AnimBlueprint || !AnimBlueprint->TargetSkeleton)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[ProjectRequiem.LayeredRecovery] Missing animation Blueprint or skeleton: %s"),
			AnimationBlueprintPath);
		return 1;
	}

	USkeleton* Skeleton = AnimBlueprint->TargetSkeleton;
	if (Skeleton->GetReferenceSkeleton().FindBoneIndex(UpperBodyRootBoneName) == INDEX_NONE)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[ProjectRequiem.LayeredRecovery] Skeleton %s has no %s bone"),
			*Skeleton->GetPathName(),
			*UpperBodyRootBoneName.ToString());
		return 1;
	}

	TArray<UEdGraph*> AllGraphs;
	AnimBlueprint->GetAllGraphs(AllGraphs);
	UAnimationGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetFName() == TEXT("AnimGraph"))
		{
			AnimGraph = Cast<UAnimationGraph>(Graph);
			break;
		}
	}
	if (!AnimGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[ProjectRequiem.LayeredRecovery] AnimGraph not found"));
		return 1;
	}

	TArray<UAnimGraphNode_Root*> RootNodes;
	TArray<UAnimGraphNode_Slot*> SlotNodes;
	TArray<UAnimGraphNode_LayeredBoneBlend*> LayeredNodes;
	TArray<UAnimGraphNode_SaveCachedPose*> CacheNodes;
	TArray<UAnimGraphNode_UseCachedPose*> UseCachedPoseNodes;
	AnimGraph->GetNodesOfClass(RootNodes);
	AnimGraph->GetNodesOfClass(SlotNodes);
	AnimGraph->GetNodesOfClass(LayeredNodes);
	AnimGraph->GetNodesOfClass(CacheNodes);
	AnimGraph->GetNodesOfClass(UseCachedPoseNodes);

	UAnimGraphNode_Root* RootNode = RootNodes.Num() == 1 ? RootNodes[0] : nullptr;
	UAnimGraphNode_Slot* DefaultSlotNode = nullptr;
	UAnimGraphNode_Slot* UpperBodySlotNode = nullptr;
	for (UAnimGraphNode_Slot* SlotNode : SlotNodes)
	{
		if (SlotNode && SlotNode->Node.SlotName == DefaultSlotName)
		{
			DefaultSlotNode = SlotNode;
		}
		else if (SlotNode && SlotNode->Node.SlotName == UpperBodySlotName)
		{
			UpperBodySlotNode = SlotNode;
		}
	}
	UAnimGraphNode_SaveCachedPose* LocomotionCacheNode = nullptr;
	for (UAnimGraphNode_SaveCachedPose* CacheNode : CacheNodes)
	{
		if (CacheNode && CacheNode->CacheName == LocomotionCacheName)
		{
			LocomotionCacheNode = CacheNode;
			break;
		}
	}
	UAnimGraphNode_LayeredBoneBlend* LayeredNode =
		LayeredNodes.Num() == 1 ? LayeredNodes[0] : nullptr;

	if (!RootNode || !DefaultSlotNode)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[ProjectRequiem.LayeredRecovery] Expected one root and the existing DefaultSlot"));
		return 1;
	}

	const bool bAnyLayeredSetupExists =
		UpperBodySlotNode
		|| LocomotionCacheNode
		|| LayeredNodes.Num() > 0
		|| UseCachedPoseNodes.Num() > 0;
	const bool bCompleteLayeredSetupExists =
		HasExpectedLayeredTopology(
			RootNode,
			DefaultSlotNode,
			LocomotionCacheNode,
			UpperBodySlotNode,
			LayeredNode,
			UseCachedPoseNodes);
	if (bAnyLayeredSetupExists && !bCompleteLayeredSetupExists)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[ProjectRequiem.LayeredRecovery] Refusing to overwrite a partial/custom layered graph"));
		return 1;
	}

	if (!bCompleteLayeredSetupExists)
	{
		if (!IsPoseConnected(DefaultSlotNode, 0, RootNode, 0))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[ProjectRequiem.LayeredRecovery] Refusing to replace a non-default AnimGraph"));
			return 1;
		}

		AnimBlueprint->Modify();
		AnimGraph->Modify();
		const TArray<UEdGraphPin*> RootInputs = FindPosePins(RootNode, EGPD_Input);
		if (RootInputs.Num() != 1)
		{
			UE_LOG(LogTemp, Error, TEXT("[ProjectRequiem.LayeredRecovery] Unexpected root pose pins"));
			return 1;
		}
		RootInputs[0]->BreakAllPinLinks(true);

		const int32 RootX = RootNode->NodePosX;
		const int32 RootY = RootNode->NodePosY;
		LocomotionCacheNode = SpawnAnimNode<UAnimGraphNode_SaveCachedPose>(
			AnimGraph,
			RootX - 850,
			RootY - 280);
		LocomotionCacheNode->CacheName = LocomotionCacheName;
		LocomotionCacheNode->Node.CachePoseName = FName(LocomotionCacheName);

		UAnimGraphNode_UseCachedPose* BasePoseNode =
			SpawnAnimNode<UAnimGraphNode_UseCachedPose>(AnimGraph, RootX - 520, RootY - 120);
		BasePoseNode->SaveCachedPoseNode = LocomotionCacheNode;
		UAnimGraphNode_UseCachedPose* UpperSourceNode =
			SpawnAnimNode<UAnimGraphNode_UseCachedPose>(AnimGraph, RootX - 820, RootY + 180);
		UpperSourceNode->SaveCachedPoseNode = LocomotionCacheNode;

		UpperBodySlotNode = SpawnAnimNode<UAnimGraphNode_Slot>(
			AnimGraph,
			RootX - 540,
			RootY + 180);
		UpperBodySlotNode->Node.SlotName = UpperBodySlotName;

		LayeredNode = SpawnAnimNode<UAnimGraphNode_LayeredBoneBlend>(
			AnimGraph,
			RootX - 250,
			RootY);
		LayeredNode->Node.BlendMode = ELayeredBoneBlendMode::BranchFilter;
		LayeredNode->Node.LayerSetup[0].BranchFilters.Reset();
		FBranchFilter UpperBodyFilter;
		UpperBodyFilter.BoneName = UpperBodyRootBoneName;
		UpperBodyFilter.BlendDepth = 2;
		LayeredNode->Node.LayerSetup[0].BranchFilters.Add(UpperBodyFilter);
		LayeredNode->Node.BlendWeights[0] = 1.0f;
		LayeredNode->Node.bMeshSpaceRotationBlend = true;
		LayeredNode->Node.bBlendRootMotionBasedOnRootBone = true;

		if (!ConnectPose(AnimGraph, DefaultSlotNode, 0, LocomotionCacheNode, 0)
			|| !ConnectPose(AnimGraph, BasePoseNode, 0, LayeredNode, 0)
			|| !ConnectPose(AnimGraph, UpperSourceNode, 0, UpperBodySlotNode, 0)
			|| !ConnectPose(AnimGraph, UpperBodySlotNode, 0, LayeredNode, 1)
			|| !ConnectPose(AnimGraph, LayeredNode, 0, RootNode, 0))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[ProjectRequiem.LayeredRecovery] Failed to connect layered recovery graph"));
			return 1;
		}

		UseCachedPoseNodes = {BasePoseNode, UpperSourceNode};
		if (!HasExpectedLayeredTopology(
				RootNode,
				DefaultSlotNode,
				LocomotionCacheNode,
				UpperBodySlotNode,
				LayeredNode,
				UseCachedPoseNodes))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[ProjectRequiem.LayeredRecovery] Created graph failed topology validation"));
			return 1;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	}

	if (Skeleton->GetSlotGroupName(UpperBodySlotName) != UpperBodySlotGroupName)
	{
		Skeleton->Modify();
		Skeleton->SetSlotGroupName(UpperBodySlotName, UpperBodySlotGroupName);
		Skeleton->MarkPackageDirty();
	}
	if (Skeleton->GetSlotGroupName(UpperBodySlotName)
		== Skeleton->GetSlotGroupName(DefaultSlotName))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[ProjectRequiem.LayeredRecovery] Upper-body and locomotion slots must use separate groups"));
		return 1;
	}

	FCompilerResultsLog CompilerResults;
	FKismetEditorUtilities::CompileBlueprint(
		AnimBlueprint,
		EBlueprintCompileOptions::None,
		&CompilerResults);
	if (AnimBlueprint->Status != BS_UpToDate
		|| CompilerResults.NumErrors > 0
		|| CompilerResults.NumWarnings > 0)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[ProjectRequiem.LayeredRecovery] Animation Blueprint compile failed: status=%d errors=%d warnings=%d"),
			static_cast<int32>(AnimBlueprint->Status),
			CompilerResults.NumErrors,
			CompilerResults.NumWarnings);
		return 1;
	}

	UEditorAssetSubsystem* AssetSubsystem =
		GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	TArray<UObject*> AssetsToSave;
	AssetsToSave.Add(Skeleton);
	AssetsToSave.Add(AnimBlueprint);
	if (!AssetSubsystem
		|| !AssetSubsystem->SaveLoadedAssets(AssetsToSave, true))
	{
		UE_LOG(LogTemp, Error, TEXT("[ProjectRequiem.LayeredRecovery] Failed to save configured assets"));
		return 1;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[ProjectRequiem.LayeredRecovery] Configured DefaultSlot locomotion + %s above %s"),
		*UpperBodySlotName.ToString(),
		*UpperBodyRootBoneName.ToString());
	return 0;
}

#else

URequiemConfigureLayeredRecoveryCommandlet::URequiemConfigureLayeredRecoveryCommandlet()
{
}

int32 URequiemConfigureLayeredRecoveryCommandlet::Main(const FString& Params)
{
	return 1;
}

#endif
