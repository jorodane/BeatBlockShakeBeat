// Copyright Epic Games, Inc. All Rights Reserved.

#include "BBSB.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "IPythonScriptPlugin.h"
#endif

class FBBSBGameModule : public FDefaultGameModuleImpl
{
#if WITH_EDITOR
 FDelegateHandle VisibilityPreparation;
public:
 virtual void StartupModule() override
 {
  FDefaultGameModuleImpl::StartupModule();
  VisibilityPreparation=FEditorDelegates::PreBeginPIE.AddLambda([](bool)
  {
   if(IPythonScriptPlugin::Get()->IsPythonAvailable())
    IPythonScriptPlugin::Get()->ExecPythonCommand(TEXT("import BBSB.view_visibility as _bbsb_view; _bbsb_view.prepare_scene()"));
  });
 }
 virtual void ShutdownModule() override
 {
  FEditorDelegates::PreBeginPIE.Remove(VisibilityPreparation);FDefaultGameModuleImpl::ShutdownModule();
 }
#endif
};
IMPLEMENT_PRIMARY_GAME_MODULE( FBBSBGameModule, BBSB, "BBSB" );

DEFINE_LOG_CATEGORY(LogBBSB)
