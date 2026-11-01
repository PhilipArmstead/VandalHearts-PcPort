/* Battle rewind. Each player turn's first free-cursor frame stores a CardFileData_InBattleSave in
 * memory; a restore replays the retail "load in-battle save" rebuild from it. See
 * docs/gameplay-additions.md, "Battle rewind". */
#include <stdlib.h>
#include <string.h>
#include "common.h"        /* gSignal1/2, gPlayerControlSuppressed, gClearSavedPadState */
#include "state.h"         /* gState, STATE_LOAD_IN_BATTLE_SAVE */
#include "card.h"          /* CardFileData_InBattleSave, gInBattleSaveDataPtr, Card_* */
#include "audio.h"         /* AUDIO_CMD_STOP_ALL */
#include "battle.h"        /* gIsEnemyTurn */
#include "object.h"        /* Obj_ResetFromIdx10 */
#include "pc_balance.h"    /* PC_SyncBalance */
#include "pc_rewind.h"

/* audio.h only carries a commented-out declaration; the real signature is in src/core/audio.c. */
extern void PerformAudioCommand(s16 cmd);

#define REWIND_MAX 128

typedef struct {
    s16 turn;
    CardFileData_InBattleSave save;
} RewindSnap;

int g_vhRewind = 0;

static RewindSnap s_snap[REWIND_MAX];
static int s_count = 0;
static int s_lastSnapTurn = -1;
static int s_idleValid = 0;
static s32 s_idleFrame = 0;          /* gState.frameCounter of the last free-cursor tick */
static int s_pending = -1;
static int s_rewinding = 0;          /* set across a rewind's reload so battle setup keeps the list */

void PC_RewindLoadSetting(void) {
    static int loaded = 0;
    const char *e;
    if (loaded) return;
    loaded = 1;
    e = getenv("VH_REWIND");
    g_vhRewind = (e && e[0] == '1') ? 1 : 0;
}

void PC_RewindClear(void) {
    s_count = 0;
    s_lastSnapTurn = -1;
    s_pending = -1;
    s_rewinding = 0;
}

void PC_RewindSetEnabled(int on) {
    g_vhRewind = on ? 1 : 0;
    if (!g_vhRewind) PC_RewindClear();
}

void PC_RewindIdleTick(void) {
    s16 turn = gState.turn;
    PC_RewindLoadSetting();
    s_idleValid = 1;
    s_idleFrame = gState.frameCounter;
    if (!g_vhRewind || turn == s_lastSnapTurn) return;

    /* A turn number at or below an existing entry means play diverged; that entry is stale. */
    while (s_count > 0 && s_snap[s_count - 1].turn >= turn) s_count--;
    if (s_count == REWIND_MAX) {
        memmove(&s_snap[0], &s_snap[1], sizeof(s_snap[0]) * (REWIND_MAX - 1));
        s_count--;
    }
    Card_PopulateInBattleSave();
    s_snap[s_count].turn = turn;
    memcpy(&s_snap[s_count].save, gInBattleSaveDataPtr, sizeof(CardFileData_InBattleSave));
    s_count++;
    s_lastSnapTurn = turn;
}

void PC_RewindBattleStart(void) {
    s_idleValid = 0;
    if (s_rewinding) { s_rewinding = 0; return; }
    PC_RewindClear();
}

int PC_RewindCanOpen(void) {
    s32 age;
    PC_RewindLoadSetting();
    if (!g_vhRewind || s_count == 0 || !s_idleValid || s_pending >= 0) return 0;
    if (gIsEnemyTurn) return 0;
    /* The F1 event arrives from VSync, after this frame's Obj_Execute ran the idle tick. */
    age = gState.frameCounter - s_idleFrame;
    return age >= 0 && age <= 2;
}

int PC_RewindCount(void) { return s_count; }
int PC_RewindTurn(int i) { return (i >= 0 && i < s_count) ? s_snap[i].turn : 0; }
int PC_RewindCurrentTurn(void) { return gState.turn; }

void PC_RewindRequest(int i) {
    if (i >= 0 && i < s_count) s_pending = i;
}

void PC_ApplyRewind(void) {
    int i = s_pending;
    if (i < 0) return;
    s_pending = -1;
    if (i >= s_count) return;

    PerformAudioCommand(AUDIO_CMD_STOP_ALL);
    /* Skips Card_LoadInBattleSave on purpose: it re-reads card slot 3 into gRegularSave, and the
     * in-memory battle-start copy is already the right one. */
    memcpy(gInBattleSaveDataPtr, &s_snap[i].save, sizeof(CardFileData_InBattleSave));
    Card_LoadInBattleSaveFromBuf();
    Obj_ResetFromIdx10();

    gSignal1 = 0;
    gSignal2 = 0;
    gIsEnemyTurn = 0;
    gState.inEvent = 0;
    gState.primary = STATE_LOAD_IN_BATTLE_SAVE;
    gState.secondary = 0;
    gState.state3 = 0;
    gState.state4 = 0;

    s_count = i + 1;
    s_lastSnapTurn = s_snap[i].turn;
    s_idleValid = 0;
    s_rewinding = 1;
    PC_SyncBalance();
}
