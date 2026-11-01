/* Battle rewind: one in-memory in-battle save per player turn, restorable from the F1 menu.
 * See docs/gameplay-additions.md, "Battle rewind". */
#ifndef PLATFORM_PC_REWIND_H
#define PLATFORM_PC_REWIND_H

extern int g_vhRewind;              /* REWIND toggle (VH_REWIND); default OFF */

void PC_RewindLoadSetting(void);    /* read VH_REWIND once */
void PC_RewindSetEnabled(int on);   /* toggle; turning it off drops every snapshot */

/* Game hooks (src/, under PC_FEAT). */
void PC_RewindIdleTick(void);       /* every free-cursor player-turn frame; snapshots a new turn */
void PC_RewindBattleStart(void);    /* battle setup; keeps the list only across a rewind's reload */
void PC_ApplyRewind(void);          /* top of the main loop; performs a requested restore */

void PC_RewindClear(void);
int  PC_RewindCanOpen(void);        /* enabled, has snapshots, and the cursor is free right now */
int  PC_RewindCount(void);
int  PC_RewindTurn(int i);          /* turn number of snapshot i (ascending) */
int  PC_RewindCurrentTurn(void);
void PC_RewindRequest(int i);       /* deferred to PC_ApplyRewind */

#endif
