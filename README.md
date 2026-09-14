# CODM Guest Account ImGui

Target package: `com.garena.game.codm`

An in-game ImGui frontend that saves/restores `shared_prefs` into 8 guest account slots. Root filesystem operations are executed by a Zygisk companion process, not by the game UID.

## UI
Each slot has:
- Save Current
- Switch
- Delete

Switch flow:
1. Force-stop CODM.
2. Replace active `shared_prefs` with the selected slot.
3. Fix UID ownership and SELinux context.
4. Launch CODM again.
5. Relog manually if Garena/MSDK requires it.

Slots live at:
`/data/local/codm/accounts/slot1` ... `slot8`

## Build
Push the project to GitHub and run the included Actions workflow. The artifact is `CODM-Guest-ImGui.zip`.

## Notes
- arm64-v8a only.
- Requires Zygisk-compatible root environment.
- This intentionally switches only `shared_prefs`; it does not copy WebView, anti-cheat, databases, or MSDK binary state.
- If the overlay renders but touch does not work on a specific CODM build, the input hook can be swapped for the game's actual input path without changing the account-slot companion code.
