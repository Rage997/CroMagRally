//
// miscscreens.h
//

#pragma once

#include "menu.h"

void DoWarmUpScreen(void);
int DisplayPicture(const char* picturePath, float timeout);
int DoPaused(void);
int ShowAgePicture(int age);

int DoMainMenuScreen(void);

int DoFailedMenu(const char* headerString);
int DoTitleScreen(void);
int DoAgeConqueredScreen(void);

int DoCharacterSelectScreen(short whichPlayer, Boolean allowAborting);
int DoCreditsScreen(void);
int DoTournamentWinScreen(void);

int DoHelpScreen(void);
int SelectSingleTrack(void);

int DoMultiPlayerVehicleSelections(void);
int DoVehicleSelectScreen(short whichPlayer, Boolean allowAborting);

int DoLocalGatherScreen(void);

int DoPhysicsEditor(void);

void RegisterSettingsMenu(const MenuItem* junk);

