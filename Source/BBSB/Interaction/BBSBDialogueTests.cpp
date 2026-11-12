#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Interaction/BBSBInteraction.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBDialogueTurnsTest,"BBSB.Interaction.Dialogue.SpeakersAndLegacyCompatibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBDialogueTurnsTest::RunTest(const FString&)
{
 if(!GEditor || !GEditor->GetEditorWorldContext().World())return false;
 UWorld* World=GEditor->GetEditorWorldContext().World();FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
 auto* NPC=World->SpawnActor<ABBSBInteractionNPC>(ABBSBInteractionNPC::StaticClass(),FVector(100000,100000,100000),FRotator::ZeroRotator,Params);
 if(!TestNotNull(TEXT("Dialogue fixture spawns"),NPC))return false;
 ON_SCOPE_EXIT { World->DestroyActor(NPC,false,false); };
 auto* Dialogue=NewObject<UBBSBInteractionComponent>();Dialogue->bDialogueActive=true;Dialogue->ActiveNPC=NPC;NPC->DisplayName=FText::FromString(TEXT("상인"));
 NPC->Lines={FText::FromString(TEXT("열어줄게.")),FText::FromString(TEXT("레파라도: 알겠어요.")),FText::FromString(TEXT("Bunniar: 준비됐어!"))};
 TestEqual(TEXT("Existing unstructured NPC lines remain usable"),NPC->DialogueLineCount(),3);
 TestEqual(TEXT("An unprefixed line belongs to the partner"),Dialogue->DisplayedSpeakerName().ToString(),FString(TEXT("상인")));
 TestEqual(TEXT("Legacy turns default to the neutral expression"),Dialogue->CurrentExpression(),EBBSBDialogueExpression::Neutral);
 Dialogue->PresentationElapsed=Dialogue->PortraitStaggerSeconds*.5f;
 TestTrue(TEXT("The partner arrives first"),Dialogue->PortraitEntranceProgress(EBBSBDialogueSpeaker::Partner)>0);
 TestEqual(TEXT("Bunniar waits a short beat"),Dialogue->PortraitEntranceProgress(EBBSBDialogueSpeaker::Bunniar),0.f);
 Dialogue->PresentationElapsed=Dialogue->PortraitStaggerSeconds*1.5f;
 TestTrue(TEXT("Bunniar follows the partner"),Dialogue->PortraitEntranceProgress(EBBSBDialogueSpeaker::Bunniar)>0);
 TestEqual(TEXT("Reparado follows Bunniar"),Dialogue->PortraitEntranceProgress(EBBSBDialogueSpeaker::Reparado),0.f);
 Dialogue->BubbleRevealElapsed=10;
 TestEqual(TEXT("Even a queued balloon waits for the last portrait"),Dialogue->BubbleRevealProgress(),0.f);
 Dialogue->AdvanceDialogue();TestEqual(TEXT("A press during entry reveals the current line without skipping it"),Dialogue->LineIndex,0);
 TestEqual(TEXT("That press finishes the balloon entrance"),Dialogue->BubbleRevealProgress(),1.f);
 // These fixtures exercise turn data after each presentation has settled.
 auto Next=[&] { Dialogue->PresentationElapsed=10;Dialogue->BubbleRevealElapsed=10;Dialogue->AdvanceDialogue(); };
 Next();TestEqual(TEXT("Legacy Reparado turn resolves its speaker"),Dialogue->CurrentSpeaker(),EBBSBDialogueSpeaker::Reparado);
 TestEqual(TEXT("A new speaker reopens their balloon"),Dialogue->BubbleRevealProgress(),0.f);
 TestEqual(TEXT("Changing speaker does not replay the cut-in entrance"),Dialogue->PortraitEntranceProgress(EBBSBDialogueSpeaker::Reparado),1.f);
 TestEqual(TEXT("Legacy prefix is not repeated in the displayed sentence"),Dialogue->DisplayedLine(),FString(TEXT("알겠어요.")));
 Next();TestEqual(TEXT("Legacy Bunniar turn resolves its speaker"),Dialogue->CurrentSpeaker(),EBBSBDialogueSpeaker::Bunniar);
 Next();TestFalse(TEXT("A legacy conversation ends after its last line"),Dialogue->bDialogueActive);
 NPC->Conversation.Reset();
 for(const auto Speaker:{EBBSBDialogueSpeaker::Partner,EBBSBDialogueSpeaker::Reparado,EBBSBDialogueSpeaker::Bunniar})
 {
  auto& Line=NPC->Conversation.AddDefaulted_GetRef();Line.Speaker=Speaker;Line.Text=FText::FromString(TEXT("새 대화\n둘째 줄"));
 }
 NPC->Lines.Add(FText::FromString(TEXT("unused legacy turn")));
 NPC->Conversation[1].Expression=EBBSBDialogueExpression::Annoyed;NPC->Conversation[2].Expression=EBBSBDialogueExpression::Warm;
 TestEqual(TEXT("Structured dialogue owns its own length"),NPC->DialogueLineCount(),3);
 Dialogue->bDialogueActive=true;Dialogue->ActiveNPC=NPC;Dialogue->LineIndex=0;NPC->bFaceCovered=true;
 TestEqual(TEXT("A covered partner is still muffled"),Dialogue->DisplayedLine(),FString(TEXT("....\n....")));
 Next();TestEqual(TEXT("Covering the partner does not muffle Reparado"),Dialogue->DisplayedLine(),FString(TEXT("새 대화\n둘째 줄")));
 TestEqual(TEXT("The line selects Reparado's expression"),Dialogue->CurrentExpression(),EBBSBDialogueExpression::Annoyed);
 TestEqual(TEXT("Reparado's cut-in remembers that expression"),Dialogue->ExpressionForSpeaker(EBBSBDialogueSpeaker::Reparado),EBBSBDialogueExpression::Annoyed);
 TestEqual(TEXT("The Reparado speaker label follows the line"),Dialogue->DisplayedSpeakerName().ToString(),FString(TEXT("레파라도")));
 Next();TestEqual(TEXT("Covering the partner does not muffle Bunniar"),Dialogue->DisplayedLine(),FString(TEXT("새 대화\n둘째 줄")));
 TestEqual(TEXT("The next speaker uses their own expression"),Dialogue->ExpressionForSpeaker(EBBSBDialogueSpeaker::Bunniar),EBBSBDialogueExpression::Warm);
 TestEqual(TEXT("An inactive speaker keeps their last expression"),Dialogue->ExpressionForSpeaker(EBBSBDialogueSpeaker::Reparado),EBBSBDialogueExpression::Annoyed);
 UTexture2D* Neutral=Dialogue->BunniarExpressions.FindRef(EBBSBDialogueExpression::Neutral);
 if(TestNotNull(TEXT("The neutral illustrated Bunniar asset loads"),Neutral))
 {
  Dialogue->BunniarExpressions.Remove(EBBSBDialogueExpression::Warm);
  TestEqual(TEXT("An omitted expression safely falls back to neutral"),Dialogue->PortraitForSpeaker(EBBSBDialogueSpeaker::Bunniar),Neutral);
 }
 UTexture2D* Override=NewObject<UTexture2D>();NPC->PortraitExpressions.Add(EBBSBDialogueExpression::Neutral,Override);
 TestEqual(TEXT("A per-NPC portrait override takes precedence over the default mannequin"),Dialogue->PortraitForSpeaker(EBBSBDialogueSpeaker::Partner),Override);
 TestEqual(TEXT("The Bunniar speaker label follows the line"),Dialogue->DisplayedSpeakerName().ToString(),FString(TEXT("버니아")));
 Next();TestFalse(TEXT("Structured dialogue closes without reading unused legacy turns"),Dialogue->bDialogueActive);
 TestNull(TEXT("Closing clears the active partner"),Dialogue->ActiveNPC.Get());
 TestEqual(TEXT("Closing clears the next entrance clock"),Dialogue->PresentationElapsed,0.f);
 return !HasAnyErrors();
}
#endif
