//
// window.h
//

#pragma once

void InitWindowStuff(void);
ObjNode* MakeFadeEvent(Boolean fadeIn);
bool OGL_FadeOutScene(void (*drawCall)(void), void (*moveCall)(void));
void Enter2D(Boolean pauseDSp);
void Exit2D(void);
int GetNumDisplays(void);
void MoveToPreferredDisplay(void);
void SetFullscreenMode(bool enforceDisplayPref);
