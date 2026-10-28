/****************************/
/*    NEW GAME - MAIN 		*/
/* (c)2000 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"
#include "miscscreens.h"
#include "network.h"
#include <SDL3/SDL.h>


/****************************/
/*    PROTOTYPES            */
/****************************/

static void InitArea(void);
static void CleanupLevel(void);
static bool PlayArea(void);
static int PlayGame(void);
static int PlayGame_LocalMultiplayer(void);
static int PlayGame_Practice(void);
static int PlayGame_Tournament(void);
static void UpdateGameModeSpecifics(void);

static bool TallyTokens(void);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	MAX_RETRIES		3

/****************************/
/*    VARIABLES             */
/****************************/

Byte				gDebugMode = 0;				// 0 == none, 1 = fps, 2 = all
Boolean				gIsInGame = false;

uint32_t				gAutoFadeStatusBits;

OGLSetupOutputType	*gGameView = nil;

PrefsType			gGamePrefs;



OGLVector3D			gWorldSunDirection = { 1, -.2, 1};		// also serves as lense flare vector


OGLColorRGBA		gFillColor1 = { 1.0, 1.0, 1.0, 1.0 };
OGLColorRGBA		gFillColor2 = { .5, .5, .5, 1.0 };

uint32_t				gGameFrameNum = 0;

Boolean				gNoCarControls = false;						// set if player has no control (for starting light et al)
Boolean				gGameOver = false;
Boolean				gTrackCompleted = false;
float				gTrackCompletedCoolDownTimer = 0;

int					gGameMode,gTheAge,gTrackNum;


			/* BATTLE MODE VARS */

short				gWhoIsIt, gWhoWasIt;
float				gReTagTimer;							// timer to keep players from spaztic tagging each other

short				gCapturedFlagCount[2];					// # flags captured for both teams
short				gNumPlayersEliminated;

short				gTotalTokens;
short				gNumRetriesRemaining;


Boolean				gIsSelfRunningDemo = false;
float				gSelfRunningDemoTimer;


//======================================================================================
//======================================================================================
//======================================================================================


/****************** TOOLBOX INIT  *****************/

void ToolBoxInit(void)
{
OSErr		iErr;

#if _DEBUG											// in debug builds, boot with debug mode (you can turn it off with F8)
	gDebugMode = 1;
#endif


		/* FIRST VERIFY SYSTEM BEFORE GOING TOO FAR */

	InitPrefsFolder(false);


			/* BOOT OGL */

	OGL_Boot();


			/* MAKE FSSPEC FOR DATA FOLDER */

	SetDefaultDirectory();							// be sure to get the default directory

	FSSpec bogus;
	iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Skeletons", &bogus);
	if (iErr)
	{
		DoAlert("Cannot find Data folder.");
		CleanQuit();
	}

			/********************/
			/* INIT PREFERENCES */
			/********************/

	InitDefaultPrefs();
	LoadPrefs();										// attempt to read from prefs file
	SetFullscreenMode(true);

	LoadLocalizedStrings(gGamePrefs.language);

	LoadScoreboardFile();

	SetDefaultPhysics();								// set all physics to defaults
}


/********** INIT DEFAULT PREFS ******************/

void InitDefaultPrefs(void)
{
	SDL_memset(&gGamePrefs, 0, sizeof(gGamePrefs));

	SetDefaultPlayerSaveData();

	gGamePrefs.language				= GetBestLanguageIDFromSystemLocale();
	gGamePrefs.difficulty			= DIFFICULTY_MEDIUM;
	gGamePrefs.splitScreenMode2P	= SPLITSCREEN_MODE_2P_TALL;
	gGamePrefs.splitScreenMode3P	= SPLITSCREEN_MODE_3P_TALL;
	gGamePrefs.displayNumMinus1		= 0;			// main monitor by default
	gGamePrefs.fullscreen			= true;
	gGamePrefs.tagDuration 			= 3;
	gGamePrefs.musicVolumePercent	= 60;			// careful to set these two volumes to one of the
	gGamePrefs.sfxVolumePercent		= 60;			// the predefined values allowed in the settings menu

	SDL_memcpy(&gGamePrefs.bindings, kDefaultInputBindings, sizeof(kDefaultInputBindings));
}





#pragma mark -

/******************** PLAY GAME ************************/
//
// Return true if user bailed before game started
//
// NOTE:  	at this point the network connections have already been established between Host and Clients for
//			network games.
//

static int PlayGame(void)
{
	static enum { Setup, Gather, Playing } state;
	if (state == Setup)
	{
		UnlockPlayerGamepadMapping();
		if (gNetGameInProgress)
			SetDefaultPhysics();
		state = Gather;
	}
	if (state == Gather)
	{
		if (!gIsSelfRunningDemo && gNumLocalPlayers > 1)
		{
			int result = DoLocalGatherScreen();
			if (result == kScreenPending)
				return kScreenPending;
			if (result == kScreenAborted)
			{
				state = Setup;
				return result;
			}
			LockPlayerGamepadMapping();
		}
		state = Playing;
	}
	int result;
	switch (gGameMode)
	{
	case GAME_MODE_PRACTICE: result = PlayGame_Practice(); break;
	case GAME_MODE_TOURNAMENT: result = PlayGame_Tournament(); break;
	default: result = PlayGame_LocalMultiplayer(); break;
	}
	if (result == kScreenPending)
		return result;
	UnlockPlayerGamepadMapping();
	state = Setup;
	if (result == kScreenDone && !gIsSelfRunningDemo)
		SavePlayerFile();
	return result;
}


/******************** PLAY GAME: PRACTICE ************************/
//
// return true if user aborted before game started
//

static int PlayGame_Practice(void)
{
	static enum { Track, Character, Vehicle, Load, Race, Fade } state;
	int result;
	switch (state)
	{
	case Track:
		gMyNetworkPlayerNum = 0;
		if (gIsSelfRunningDemo)
		{
			int ages = GetNumAgesCompleted();
			gTrackNum = RandomRange(0, ages == 0 ? 2 : ages == 1 ? 5 : 7);
		}
		else
		{
			result = SelectSingleTrack();
			if (result != kScreenDone)
				return result;
		}
		InitPlayerInfo_Game();
		state = Character;
		return kScreenPending;
	case Character:
		if (gIsSelfRunningDemo)
		{
			int ages = GetNumAgesCompleted();
			gPlayerInfo[0].sex = MyRandomLong() & 1;
			gPlayerInfo[0].vehicleType = RandomRange(0, ages == 0 ? 5 : ages == 1 ? 7 : 9);
			gAutoPilot = true;
			gSelfRunningDemoTimer = 50;
			state = Load;
		}
		else
		{
			result = DoCharacterSelectScreen(0, true);
			if (result == kScreenPending)
				return result;
			state = result == kScreenAborted ? Track : Vehicle;
		}
		return kScreenPending;
	case Vehicle:
		result = DoVehicleSelectScreen(0, true);
		if (result != kScreenPending)
			state = result == kScreenAborted ? Character : Load;
		return kScreenPending;
	case Load:
		InitArea();
		state = Race;
		return kScreenPending;
	case Race:
		if (PlayArea())
			state = Fade;
		return kScreenPending;
	case Fade:
		if (!FadeOutArea())
			return kScreenPending;
		CleanupLevel();
		gAutoPilot = false;
		gIsSelfRunningDemo = false;
		state = Track;
		return kScreenDone;
	}
	return kScreenPending;
}


/******************** PLAY GAME: TOURNAMENT ************************/
//
// Game loop for 1-player tournament mode.
//
//
// return true if user aborted before game started
//

static int PlayGame_Tournament(void)
{
	static enum { Setup, Character, Age, Vehicle, Load, Race, Tally, Failure, Progress, Fade, Conquered, Win } state;
	static int stage, placeToWin;
	static bool canAbort, objectiveCompleted, retry;
	int result;
	switch (state)
	{
	case Setup:
		canAbort = true;
		placeToWin = gGamePrefs.difficulty <= DIFFICULTY_EASY ? 2 : 0;
		gMyNetworkPlayerNum = 0;
		InitPlayerInfo_Game();
		state = Character;
		// fall through
	case Character:
		result = DoCharacterSelectScreen(0, true);
		if (result == kScreenPending)
			return result;
		if (result == kScreenAborted)
		{
			state = Setup;
			return result;
		}
		state = Age;
		return kScreenPending;
	case Age:
		if (ShowAgePicture(gTheAge) == kScreenPending)
			return kScreenPending;
		gNumRetriesRemaining = MAX_RETRIES;
		stage = GetNumAgesCompleted() >= NUM_AGES || gGamePrefs.difficulty >= DIFFICULTY_HARD
			|| gTheAge < GetNumAgesCompleted() ? 0 : GetNumStagesCompletedInAge();
		gTrackNum = GetTrackNumFromAgeStage(gTheAge, stage);
		state = Vehicle;
		return kScreenPending;
	case Vehicle:
		result = DoVehicleSelectScreen(0, canAbort);
		if (result == kScreenPending)
			return result;
		if (result == kScreenAborted)
		{
			state = Setup;
			return result;
		}
		state = Load;
		return kScreenPending;
	case Load:
		gTrackNum = GetTrackNumFromAgeStage(gTheAge, stage);
		InitArea();
		retry = false;
		state = Race;
		return kScreenPending;
	case Race:
		if (!PlayArea())
			return kScreenPending;
		objectiveCompleted = gPlayerInfo[0].place <= placeToWin
			&& gTotalTokens >= MAX_TOKENS && gPlayerInfo[0].raceComplete;
		state = gPlayerInfo[0].place <= placeToWin && gPlayerInfo[0].raceComplete ? Tally : Failure;
		return kScreenPending;
	case Tally:
		if (TallyTokens())
			state = Failure;
		return kScreenPending;
	case Failure:
		if (!objectiveCompleted)
		{
			if (gNumRetriesRemaining > 0)
			{
				result = DoFailedMenu(Localize(STR_1_TRY_REMAINING + gNumRetriesRemaining - 1));
				if (result == kScreenPending)
					return kScreenPending;
				retry = result != 0;
				if (retry)
					gNumRetriesRemaining--;
				else
					gGameOver = true;
			}
			else
				gGameOver = true;
		}
		state = Progress;
		// fall through
	case Progress:
		if (objectiveCompleted && !gGameOver && gTrackNum + 1 > GetNumTracksCompletedTotal())
		{
			SDL_memcpy(gGamePrefs.tournamentProgression.tournamentLapTimes[gTrackNum],
				gPlayerInfo->lapTimes, sizeof(gPlayerInfo->lapTimes));
			SetPlayerProgression(gTrackNum + 1);
			SavePlayerFile();
		}
		state = Fade;
		return kScreenPending;
	case Fade:
		if (!FadeOutArea())
			return kScreenPending;
		CleanupLevel();
		if (gGameOver)
		{
			state = Setup;
			return kScreenDone;
		}
		if (!retry)
			stage++;
		state = stage < TRACKS_PER_AGE ? Load : Conquered;
		return kScreenPending;
	case Conquered:
		if (DoAgeConqueredScreen() == kScreenPending)
			return kScreenPending;
		canAbort = false;
		state = ++gTheAge <= IRON_AGE ? Age : Win;
		return kScreenPending;
	case Win:
		if (DoTournamentWinScreen() == kScreenPending)
			return kScreenPending;
		state = Setup;
		return kScreenDone;
	}
	return kScreenPending;
}

/******************** LOCAL MULTIPLAYER MODES ************************/

static int PlayGame_LocalMultiplayer(void)
{
	static enum { Setup, Selection, Load, Race, Fade } state;
	switch (state)
	{
	case Setup:
		InitPlayerInfo_Game();
		if (gGameMode == GAME_MODE_CAPTUREFLAG)
			gCapturedFlagCount[0] = gCapturedFlagCount[1] = 0;
		state = Selection;
		// fall through
	case Selection:
	{
		int result = DoMultiPlayerVehicleSelections();
		if (result == kScreenPending)
			return result;
		if (result == kScreenAborted)
		{
			state = Setup;
			return result;
		}
		state = Load;
		return kScreenPending;
	}
	case Load:
		InitArea();
		if (gGameMode == GAME_MODE_SURVIVAL || gGameMode == GAME_MODE_TAG1 || gGameMode == GAME_MODE_TAG2)
			gNumPlayersEliminated = 0;
		if (gGameMode == GAME_MODE_TAG1 || gGameMode == GAME_MODE_TAG2)
		{
			ChooseTaggedPlayer();
			gWhoWasIt = gWhoIsIt;
		}
		state = Race;
		return kScreenPending;
	case Race:
		if (PlayArea())
			state = Fade;
		return kScreenPending;
	case Fade:
		if (!FadeOutArea())
			return kScreenPending;
		CleanupLevel();
		state = Setup;
		return kScreenDone;
	}
	return kScreenPending;
}

#pragma mark -


/********************** TALLY TOKENS **************************/
//
// Called @ end of tournament game to do token tally effect
//

static bool TallyTokens(void)
{
	static bool active;
	static Byte stage;
	static float stageTimer;
	static int tokensTallied;
	static ObjNode *bigArrowhead, *counterText;
	char s[32];
	bool done = false;
	if (!active)
	{
		active = true;
		stage = 0;
		stageTimer = 0;
		tokensTallied = 0;
		bigArrowhead = counterText = NULL;

	if (gFinalPlaceObj)
	{
		DeleteObject(gFinalPlaceObj);
		gFinalPlaceObj = nil;
	}

	}
			/************************/
			/* SEE IF DO NEXT STAGE */
			/************************/

		stageTimer -= gFramesPerSecondFrac;
		if (stageTimer <= 0.0f)
		{

			switch(stage++)
			{
					/* SHOW BIG ARROWHEAD ICON */

				case	0:
				{
						stageTimer = 1.0;										// reset stage timer

						PlayEffect_Parms(EFFECT_GETPOW, 2 * FULL_CHANNEL_VOLUME, 2 * FULL_CHANNEL_VOLUME, NORMAL_CHANNEL_RATE * (0.7));

						NewObjectDefinitionType def =
						{
							.group		= SPRITE_GROUP_INFOBAR,
							.type		= INFOBAR_SObjType_Token_Arrowhead,
							.coord		= {-64, 0, 0},
							.slot 		= SPRITE_SLOT,
							.scale		= 1.5,
							.flags		= STATUS_BIT_KEEPBACKFACES,
						};
						bigArrowhead = MakeSpriteObject(&def);
						MakeTwitch(bigArrowhead, kTwitchPreset_MenuSelect);
						break;
				}


					/* START COUNTER */

				case	1:
				{
						stageTimer = 1.0;										// reset stage timer

								/* MAKE X */

						PlayEffect_Parms(EFFECT_GETPOW, 2 * FULL_CHANNEL_VOLUME, 2 * FULL_CHANNEL_VOLUME, NORMAL_CHANNEL_RATE * (0.8));

						NewObjectDefinitionType def =
						{
							.group		= SPRITE_GROUP_INFOBAR,
							.type		= INFOBAR_SObjType_WeaponX,
							.coord		= {0, 0, 0},
							.slot		= SPRITE_SLOT,
							.scale		= 1.5,
						};
						ObjNode* bigX = MakeSpriteObject(&def);
						MakeTwitch(bigX, kTwitchPreset_MenuSelect);
						break;
				}

					/* TALLY */

				case	2:
				{
						stageTimer = 1.0f;

								/* MAKE NUMBER */

						PlayEffect_Parms(EFFECT_GETPOW, 2 * FULL_CHANNEL_VOLUME, 2 * FULL_CHANNEL_VOLUME, NORMAL_CHANNEL_RATE * (0.9));

						NewObjectDefinitionType def =
						{
							.coord 		= {64, 0, 0},
							.scale		= 1.5,
							.slot 		= SPRITE_SLOT,
						};

						counterText = TextMesh_New("0", kTextMeshAlignCenter, &def);
						MakeTwitch(counterText, kTwitchPreset_MenuSelect);
						break;
				}

				case	3:
				{
						stageTimer = .15f;

								/* MAKE NUMBER */

						PlayEffect_Parms(EFFECT_GETPOW, 2 * FULL_CHANNEL_VOLUME, 2 * FULL_CHANNEL_VOLUME, NORMAL_CHANNEL_RATE * (1.0f + 0.15f * tokensTallied));

						tokensTallied++;

						SDL_snprintf(s, sizeof(s), "%d", tokensTallied);

						TextMesh_Update(s, kTextMeshAlignCenter, counterText);

						MakeTwitch(counterText, kTwitchPreset_ItemGain);

						if (tokensTallied < gTotalTokens)		// replay this stage
						{
							stage = 3;
						}
						else
						{
							stageTimer = 0;
						}

						break;
				}

					/* SHOW COUNTER */

				case	4:
				{
						stageTimer = 4.0;										// reset stage timer

						if (gTotalTokens == MAX_TOKENS)
						{
							PlayAnnouncerSound(EFFECT_COMPLETED, true, .3);
							MakeTwitch(bigArrowhead, kTwitchPreset_GiantArrowheadSpin);
						}
						else
							PlayAnnouncerSound(EFFECT_INCOMPLETE, true, .3);

						break;
				}

				case	5:
						done = true;
						break;

			}
		}


			/* MOVE STUFF */

		MoveEverything();

			/* DRAW STUFF */

		CalcFramesPerSecond();
		ReadKeyboard();
		DoPlayerTerrainUpdate();							// need to call this to keep supertiles active
		OGL_DrawScene(DrawTerrain);




	if (done)
		active = false;
	return done;
}


#pragma mark -

/**************** PLAY AREA ************************/

static bool PlayArea(void)
{
	static enum { Setup, Racing, Paused } state;
	if (state == Paused)
	{
		if (DoPaused() == kScreenPending)
			return false;
		state = Racing;
		if (gGameOver)
			goto complete;
		return false;
	}
	if (state == Setup)
	{
	/* IF DOING NET GAME THEN WAIT FOR SYNC */
	//
	// Need to wait for other players to finish loading art and when
	// everyone is ready, then we can start all at once.
	//


	if (gIsNetworkHost)
		HostWaitForPlayersToPrepareLevel();
	else
	if (gIsNetworkClient)
		ClientTellHostLevelIsPrepared();


			/* PREP STUFF */

	ReadKeyboard();
	ResetFrameTimer();
	gNoCarControls = true;									// no control when starting light is going
	gDisableHiccupTimer = true;
	gIsInGame = true;

		/******************/
		/* MAIN GAME LOOP */
		/******************/

	MakeFadeEvent(true);

		state = Racing;
	}
	{
				/******************************************/
				/* GET CONTROL INFORMATION FOR THIS FRAME */
				/******************************************/
				//
				// Also gathers frame rate info for the net clients.
				//

				/* NETWORK CLIENT */

		if (gIsNetworkClient)
		{
			ClientReceive_ControlInfoFromHost();			// read all player's control info back from the Host once he's gathered it all
		}

				/* HOST OR NON-NET */
		else
		{
			ReadKeyboard();									// read local keys
			GetLocalKeyState();								// build a control state bitfield

				/* NETWORK HOST*/

			if (gIsNetworkHost)
			{
				HostSend_ControlInfoToClients();			// now send everyone's key states to all clients
			}
		}


				/****************/
				/* MOVE OBJECTS */
				/****************/

		MoveEverything();

				/* DO GAME MODE SPECIFICS */

		UpdateGameModeSpecifics();


			/* UPDATE THE TERRAIN */

		DoPlayerTerrainUpdate();




			/****************************/
			/* SEND NET CLIENT KEY INFO */
			/****************************/
			//
			// We can do this anytime AFTER this frame's key control info is no longer needed.
			// Since this will change the control bits, we MUST BE SURE that the bits are not
			// used again until the next frame!
			//
			// For best performance, we do this before the render function.  That way there is
			// time for this send to get to the host while we're still waiting for the render to
			// complete - we essentially get this send for free!
			//

		if (gIsNetworkClient)
		{
			ReadKeyboard();									// read local client keys
			GetLocalKeyState();								// build a control state bitfield
			ClientSend_ControlInfoToHost();					// send this info to the host to be used the next frame
		}



			/***************/
			/* DRAW IT ALL */
			/***************/

		OGL_DrawScene(DrawTerrain);




			/************************************/
			/* NET HOST RECEIVE CLIENT KEY INFO */
			/************************************/
			//
			// Odds are that the clients have all sent their control into to the Host by now,
			// so the Host can read all of the client key info for use on the next frame.
			//

		if (gIsNetworkHost)
		{
			HostReceive_ControlInfoFromClients();		// get client info
		}


			/**************/
			/* MISC STUFF */
			/**************/

			/* SEE IF PAUSED */

		if (!gIsSelfRunningDemo && (GetNewNeedStateAnyP(kNeed_UIPause) || IsCmdQPressed()))
		{
			state = Paused;
			return false;
		}

			/* CHECK CHEATS */

		if (IsCheatKeyComboDown())						// win race cheat
		{
			if (!gPlayerInfo[0].raceComplete)
			{
				gPlayerInfo[0].cheated = true;
				PlayerCompletedRace(0);
				gPlayerInfo[0].place = 0;
				gPlayerInfo[0].raceComplete = true;
				gPlayerInfo[0].numTokens = gTotalTokens = MAX_TOKENS;
			}
		}

		if (GetKeyState(SDL_SCANCODE_L) &&
			GetKeyState(SDL_SCANCODE_A) &&
			GetNewKeyState(SDL_SCANCODE_P))
		{
			gPlayerInfo[0].cheated = true;
			NextLap(0);
		}

		if (GetNewKeyState(SDL_SCANCODE_SCROLLLOCK))		// hide/show infobar
		{
			gHideInfobar = !gHideInfobar;
		}



		if (!gIsNetworkClient)						// clients dont need to calc frame rate since its passed to them from host.
			CalcFramesPerSecond();

		gGameFrameNum++;


				/* SEE IF TRACK IS COMPLETED */

		if (gGameOver)													// if we need immediate abort, then bail now
			goto complete;

		if (gTrackCompleted)
		{
			gTrackCompletedCoolDownTimer -= gFramesPerSecondFrac;		// game is done, but wait for cool-down timer before bailing
			if (gTrackCompletedCoolDownTimer <= 0.0f)
				goto complete;
		}

		gDisableHiccupTimer = false;									// reenable this after the 1st frame

			/* UPDATE SELF-RUNNING DEMO */

		if (gIsSelfRunningDemo)
		{
			if (UserWantsOut())											// stop SRD if any key is pressed
				goto complete;

			gSelfRunningDemoTimer -= gFramesPerSecondFrac;
			if (gSelfRunningDemoTimer <= 0.0f)							// or stop of timer runs out
			{
				goto complete;
			}
		}
	}
	return false;

complete:
	state = Setup;

	gIsInGame = false;
	return true;
}


bool FadeOutArea(void)
{
			/* FADE OUT */

	gGameView->fadeSound = true;
	gGameView->fadeOutDuration = .3f;
	return OGL_FadeOutScene(DrawTerrain, DoPlayerTerrainUpdate);
}

/******************** MOVE EVERYTHING ************************/

void MoveEverything(void)
{

	MoveObjects();
	MoveSplineObjects();
	MoveParticleGroups();
	UpdateCameras(false, false);				// update cameras for ALL players
	CalcPlayerPlaces();							// determinw who is in what place
	UpdateSkidMarks();							// update skid marks
	UpdateLiquidAnimation();					// update liquid animation

			/* DO TRACK SPECIFICS */

	switch(gTrackNum)
	{
		case	TRACK_NUM_ICE:
		case	TRACK_NUM_RAMPS:
				MakeSnow();
				break;

	}
}


/***************** INIT AREA ************************/

static void InitArea(void)
{
OGLSetupInputType	viewDef;
short				numPanes;


	switch(gTrackNum)
	{
		case	TRACK_NUM_DESERT:
				PlaySong(SONG_DESERT, true);
				break;

		case	TRACK_NUM_JUNGLE:
		case	TRACK_NUM_AZTEC:
				PlaySong(SONG_JUNGLE, true);
				break;

		case	TRACK_NUM_ATLANTIS:
		case	TRACK_NUM_TARPITS:
				PlaySong(SONG_ATLANTIS, true);
				break;

		case	TRACK_NUM_CHINA:
		case	TRACK_NUM_SPIRAL:
				PlaySong(SONG_CHINA, true);
				break;

		case	TRACK_NUM_EGYPT:
				PlaySong(SONG_EGYPT, true);
				break;

		case	TRACK_NUM_CRETE:
		case	TRACK_NUM_CELTIC:
		case	TRACK_NUM_COLISEUM:
				PlaySong(SONG_CRETE, true);
				break;

		case	TRACK_NUM_ICE:
		case	TRACK_NUM_RAMPS:
				PlaySong(SONG_ICE, true);
				break;

		case	TRACK_NUM_EUROPE:
		case	TRACK_NUM_STONEHENGE:
				PlaySong(SONG_EUROPE, true);
				break;

		case	TRACK_NUM_SCANDINAVIA:
				PlaySong(SONG_VIKING, true);
				break;

		default:
				PlaySong(SONG_DESERT, true);
	}



			/* INIT SOME PRELIM STUFF */

	if ((!gIsSelfRunningDemo) && (gGameMode != GAME_MODE_PRACTICE))				// dont reset random seed for SRD - we want variety!
		InitMyRandomSeed();
	InitControlBits();

	gGameFrameNum 		= 0;
	gGameOver 			= false;
	gTrackCompleted 	= false;
	gTotalTokens 		= 0;
	gReTagTimer 		= 0;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		gPlayerInfo[i].objNode = nil;
	}


			/*************/
			/* MAKE VIEW */
			/*************/

	if (gNetGameInProgress)
		numPanes = 1;
	else
		numPanes = gNumRealPlayers;


			/* SETUP VIEW DEF */

	OGL_NewViewDef(&viewDef);

	viewDef.camera.hither 			= 50;
	viewDef.camera.yon 				= (SUPERTILE_ACTIVE_RANGE * SUPERTILE_SIZE * TERRAIN_POLYGON_SIZE);
	viewDef.camera.fov 				= GAME_FOV;

	viewDef.view.clearColor.r 		= 0;
	viewDef.view.clearColor.g 		= 0;
	viewDef.view.clearColor.b		= 0;
	viewDef.view.clearBackBuffer	= true;
	viewDef.view.numPanes			= numPanes;
	viewDef.view.pillarboxRatio		= PILLARBOX_RATIO_FULLSCREEN;
	viewDef.view.fontName			= "rockfont";

	viewDef.styles.useFog			= false;
	viewDef.styles.fogStart			= viewDef.camera.yon * .1f;
	viewDef.styles.fogEnd			= viewDef.camera.yon;


			/* SET LIGHTS */

	switch(gTrackNum)
	{
		case	TRACK_NUM_ICE:
				viewDef.lights.numFillLights 	= 1;

				viewDef.lights.ambientColor.r 		= .7;
				viewDef.lights.ambientColor.g 		= .7;
				viewDef.lights.ambientColor.b 		= .7;
				viewDef.lights.fillDirection[0].x 	= 1.0;
				viewDef.lights.fillDirection[0].y 	= -.1;
				viewDef.lights.fillDirection[0].z 	= 1.0;
				viewDef.lights.fillColor[0].r 		= 1.0;
				viewDef.lights.fillColor[0].g 		= 1.0;
				viewDef.lights.fillColor[0].b 		= 1.0;
				break;

		case	TRACK_NUM_ATLANTIS:
				viewDef.lights.numFillLights 		= 1;

				viewDef.lights.ambientColor.r 		= .5;
				viewDef.lights.ambientColor.g 		= .5;
				viewDef.lights.ambientColor.b 		= .7;
				viewDef.lights.fillDirection[0].x 	= 0;
				viewDef.lights.fillDirection[0].y 	= -1.0;
				viewDef.lights.fillDirection[0].z 	= 0;
				viewDef.lights.fillColor[0].r 		= .9;
				viewDef.lights.fillColor[0].g 		= .9;
				viewDef.lights.fillColor[0].b 		= 1.0;
				break;

		default:
				viewDef.lights.numFillLights 		= 1;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.ambientColor.r 		= .6;
				viewDef.lights.ambientColor.g 		= .6;
				viewDef.lights.ambientColor.b 		= .6;
				viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
				viewDef.lights.fillColor[0] 		= gFillColor1;
				viewDef.lights.fillDirection[1].x 	= -gWorldSunDirection.x;
				viewDef.lights.fillDirection[1].y 	= gWorldSunDirection.y;
				viewDef.lights.fillDirection[1].z 	= -gWorldSunDirection.z;
				viewDef.lights.fillColor[1]			= gFillColor2;
	}

			/* SET CLEAR COLOR */
			//
			// Our crappy sky dome is a cylinder with a big hole, so pick a color to kinda blend it in
			//

	switch(gTrackNum)
	{

		case		TRACK_NUM_DESERT:
					viewDef.view.clearColor.r = 153.0/255.0;
					viewDef.view.clearColor.g = 171.0/255.0;
					viewDef.view.clearColor.b = 237.0/255.0;
					break;

		case		TRACK_NUM_JUNGLE:
					viewDef.view.clearColor.r = 82.0/255.0;
					viewDef.view.clearColor.g = 148.0/255.0;
					viewDef.view.clearColor.b = 198.0/255.0;
					break;

		case		TRACK_NUM_ICE:
		case		TRACK_NUM_RAMPS:
					viewDef.view.clearColor.r = 115.0/255.0;
					viewDef.view.clearColor.g = 198.0/255.0;
					viewDef.view.clearColor.b = 255.0/255.0;
					break;

		case		TRACK_NUM_SPIRAL:
		case		TRACK_NUM_CRETE:
		case		TRACK_NUM_CELTIC:
		case		TRACK_NUM_MAZE:
					viewDef.view.clearColor.r = 44.0/255.0;
					viewDef.view.clearColor.g = 73.0/255.0;
					viewDef.view.clearColor.b = 195.0/255.0;
					break;

		case		TRACK_NUM_CHINA:
					viewDef.view.clearColor.r = 179.0/255.0;
					viewDef.view.clearColor.g = 153.0/255.0;
					viewDef.view.clearColor.b = 91.0/255.0;
					break;

		case		TRACK_NUM_EGYPT:
		case		TRACK_NUM_TARPITS:
					viewDef.view.clearColor.r = 222.0/255.0;
					viewDef.view.clearColor.g = 181.0/255.0;
					viewDef.view.clearColor.b = 99.0/255.0;
					break;

		case		TRACK_NUM_EUROPE:
					viewDef.view.clearColor.r = 16.0/255.0;
					viewDef.view.clearColor.g = 16.0/255.0;
					viewDef.view.clearColor.b = 74.0/255.0;
					break;

		case		TRACK_NUM_SCANDINAVIA:
		case		TRACK_NUM_STONEHENGE:
					viewDef.view.clearColor.r = 74.0/255.0;
					viewDef.view.clearColor.g = 90.0/255.0;
					viewDef.view.clearColor.b = 148.0/255.0;
					break;

		case		TRACK_NUM_ATLANTIS:
					viewDef.view.clearColor.r = 5.0/255.0;
					viewDef.view.clearColor.g = 160.0/255.0;
					viewDef.view.clearColor.b = 190.0/255.0;
					break;


		case		TRACK_NUM_AZTEC:
					viewDef.view.clearColor.r = 82.0/255.0;
					viewDef.view.clearColor.g = 148.0/255.0;
					viewDef.view.clearColor.b = 198.0/255.0;
					break;

		case		TRACK_NUM_COLISEUM:
					viewDef.view.clearColor.r = 61.0/255.0;
					viewDef.view.clearColor.g = 87.0/255.0;
					viewDef.view.clearColor.b = 198.0/255.0;
					break;

	}


	OGL_SetupGameView(&viewDef);


			/* SET AUTO-FADE INFO */

	gAutoFadeStartDist	= gGameView->yon * .80f;
	gAutoFadeEndDist	= gGameView->yon * .92f;
	gAutoFadeRange_Frac	= 1.0 / (gAutoFadeEndDist - gAutoFadeStartDist);

	if (gAutoFadeStartDist != 0.0f)
		gAutoFadeStatusBits = STATUS_BIT_AUTOFADE;
	else
		gAutoFadeStatusBits = 0;





			/**********************/
			/* LOAD ART & TERRAIN */
			/**********************/
			//
			// NOTE: only call this *after* draw context is created!
			//

	LoadLevelArt();
	InitInfobar();


			/* INIT OTHER MANAGERS */

	InitEffects();
	InitItemsManager();
	InitSkidMarks();


		/* INIT THE PLAYERS & RELATED STUFF */

	InitPlayersAtStartOfLevel();
	InitCurrentScrollSettings();

	PrimeSplines();
	PrimeFences();

			/* PRINT TRACK NAME */

	MakeIntroTrackName();

			/* INIT CAMERAS */

	InitCameras();
 }


/**************** CLEANUP LEVEL **********************/

static void CleanupLevel(void)
{
	EndNetworkGame();
	StopAllEffectChannels();
 	EmptySplineObjectList();
	DisposeInfobar();
	DeleteAllObjects();
	FreeAllSkeletonFiles(-1);
	DisposeSuperTileMemoryList();
	DisposeTerrain();
	DeleteAllParticleGroups();
	DisposeFences();

	DisposeAllBG3DContainers();

	OGL_DisposeGameView();	// do this last!
	DisposeSoundBank(SOUNDBANK_LEVELSPECIFIC);

	gNumRealPlayers = 1;					// reset at end of level to be safe
	gNumLocalPlayers = 1;
	gActiveSplitScreenMode = SPLITSCREEN_MODE_NONE;
}


/*************** UPDATE GAME MODE SPECIFICS **********************/

static void UpdateGameModeSpecifics(void)
{
short	i,t,winner;

	if (gTrackCompleted)										// if track is done then dont do anything
		return;

	switch(gGameMode)
	{
				/****************************/
				/* TAG - DONT WANT TO BE IT */
				/****************************/

		case	GAME_MODE_TAG1:
				gReTagTimer -= gFramesPerSecondFrac;						// dec this timer

				if (gStartingLightTimer <= 1.0f)
					gPlayerInfo[gWhoIsIt].tagTimer -= gFramesPerSecondFrac;		// dec the tagged player's timer

				if (gPlayerInfo[gWhoIsIt].tagTimer <= 0.0f)					// if done, then eliminate this player from the battle
				{
					gPlayerInfo[gWhoIsIt].tagTimer = 0;
					gPlayerInfo[gWhoIsIt].isEliminated = true;

					gNumPlayersEliminated++;
					if (gNumPlayersEliminated == (gNumTotalPlayers-1))		// if all players eliminated except 1 then we're done!
					{
							/* GAME IS DONE */

						gTrackCompleted = true;
						gTrackCompletedCoolDownTimer = TRACK_COMPLETE_COOLDOWN_TIME;

						for (winner = 0; winner < gNumTotalPlayers; winner++)
							if (!gPlayerInfo[winner].isEliminated)			// scan for the winner
								break;

						for (i = 0; i < gNumTotalPlayers; i++)				// see which player Won (was not eliminated)
						{
							if (gPlayerInfo[i].isEliminated)
							{
								ShowWinLose(i, 2, winner);					// lost
							}
							else
								ShowWinLose(i, 1, winner);					// won!
						}
					}

							/* ELIMINATE THIS PLAYER AND CHOOSE ANOTHER */
					else
					{
						ShowWinLose(gWhoIsIt, 0, 0);						// this player is eliminated
						ChooseTaggedPlayer();								// tagged guy is gone, so choose a new one
					}

				}
				UpdateTagMarker();
				break;


				/**************************/
				/* TAG - DO WANT TO BE IT */
				/**************************/

		case	GAME_MODE_TAG2:
				gReTagTimer -= gFramesPerSecondFrac;						// dec this timer

				if (gStartingLightTimer <= 1.0f)
					gPlayerInfo[gWhoIsIt].tagTimer -= gFramesPerSecondFrac;		// update the "it" player for tagged mode

				if (gPlayerInfo[gWhoIsIt].tagTimer <= 0.0f)					// if this player's timer is done then this player has won
				{
					gPlayerInfo[gWhoIsIt].tagTimer = 0;
					gTrackCompleted = true;
					gTrackCompletedCoolDownTimer = TRACK_COMPLETE_COOLDOWN_TIME;

					for (i = 0; i < gNumTotalPlayers; i++)					// see which player Won (was not eliminated)
					{
						if (i == gWhoIsIt)
							ShowWinLose(i, 1, gWhoIsIt);
						else
							ShowWinLose(i, 2, gWhoIsIt);
					}

				}
				UpdateTagMarker();
				break;


				/************/
				/* SURVIVAL */
				/************/

		case	GAME_MODE_SURVIVAL:
				for (i = 0; i < gNumTotalPlayers; i++)					// dec impact timers
				{
					gPlayerInfo[i].impactResetTimer -= gFramesPerSecondFrac;
				}

				if (gNumPlayersEliminated == (gNumTotalPlayers-1))		// if all players eliminated except 1 then we're done!
				{
						/* GAME IS DONE */

					gTrackCompleted = true;
					gTrackCompletedCoolDownTimer = TRACK_COMPLETE_COOLDOWN_TIME;

					for (winner = 0; winner < gNumTotalPlayers; winner++)
						if (!gPlayerInfo[winner].isEliminated)			// scan for the winner
							break;


								/* NOBODY WON */

					if (winner >= gNumTotalPlayers)
					{
						for (i = 0; i < gNumTotalPlayers; i++)			// all were eliminated
							ShowWinLose(i, 2, -1);						// lost
					}

							/* SHOW WINNER/LOSERS */
					else
					{
						for (i = 0; i < gNumTotalPlayers; i++)				// see which player Won (was not eliminated)
						{
							if (gPlayerInfo[i].isEliminated)
							{
								ShowWinLose(i, 2, winner);					// lost
							}
							else
								ShowWinLose(i, 1, winner);					// won!
						}
					}
				}
				break;


				/********************/
				/* CAPTURE THE FLAG */
				/********************/

		case	GAME_MODE_CAPTUREFLAG:
				for (t = 0; t < 2; t++)											// check both teams
				{
					if (gCapturedFlagCount[t] >= NUM_FLAGS_TO_GET) 				// see if team t got all of their flags
					{
						gTrackCompleted = true;
						gTrackCompletedCoolDownTimer = TRACK_COMPLETE_COOLDOWN_TIME;

						for (i = 0; i < gNumTotalPlayers; i++)					// see which players Won (was not eliminated)
						{
							if (gPlayerInfo[i].team == t)
								ShowWinLose(i, 1, 0);								// won!
							else
								ShowWinLose(i, 2, 0);								// lost
						}
						break;
					}
				}
				break;

	}
}


/************************************************************/
/******************** PROGRAM MAIN ENTRY  *******************/
/************************************************************/


void GameMain(void)
{
	static enum { Boot, DirectRace, DirectFade, Warmup, Title, MenuStart, Menu, Playing } state;
	if (state == Boot)
	{
				/**************/
				/* BOOT STUFF */
				/**************/

	ToolBoxInit();



			/* INIT SOME OF MY STUFF */

	InitSpriteManager();
	InitBG3DManager();
    InitNetworkManager();
 	InitInput();
	InitWindowStuff();
	InitTerrainManager();
	InitSkeletonManager();
	InitSoundTools();
	InitTwitchSystem();


			/* INIT MORE MY STUFF */

	InitObjectManager();

	{
		unsigned long someLong;
		GetDateTime(&someLong);		// init random seed
		SetMyRandomSeed(someLong);
	}

//	HideCursor();


	if (gCommandLine.bootToTrack != 0)
	{
		gGameMode = GAME_MODE_PRACTICE;
		gTrackNum = gCommandLine.bootToTrack - 1;
		InitPlayerInfo_Game();

		if (gCommandLine.car)
		{
			gPlayerInfo[0].vehicleType = gCommandLine.car - 1;
		}

		InitArea();
		state = DirectRace;
		return;
	}
	state = Warmup;
	}
	if (state == DirectRace)
	{
		if (PlayArea())
			state = DirectFade;
		return;
	}
	if (state == DirectFade)
	{
		if (!FadeOutArea())
			return;
		CleanupLevel();
		gCommandLine.bootToTrack = 0;
		state = Warmup;
		return;
	}
	if (state == Warmup)
	{

	DoWarmUpScreen();
	PreloadGameArt();
	PlaySong(SONG_THEME, true);


		/* SHOW TITLE SCREEN */

		state = gDebugMode == 0 ? Title : MenuStart;
	}
	if (state == Title)
	{
		if (DoTitleScreen() == kScreenPending)
			return;
		state = MenuStart;
	}
	if (state == MenuStart)
	{
		PlaySong(SONG_THEME, true);
		state = Menu;
	}
	if (state == Menu)
	{
		if (DoMainMenuScreen() != kScreenPending)
			state = Playing;
		return;
	}
	if (state == Playing && PlayGame() != kScreenPending)
		state = MenuStart;
}




