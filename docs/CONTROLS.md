# Controls

Open **Controls** in the launcher. Select Player 1 or Player 2, choose keyboard,
gamepad, both or disabled, and choose an assigned gamepad slot. Slots follow SDL
connection order. Two active players using gamepads need different slots.
Player 2 starts disabled. Road Rash II / 3 use their original split-screen mode;
Road Rash 1 retains its original player limit.

Click or keyboard-activate a binding. Press a keyboard key, a controller button,
or move an axis in the intended direction. Release the controller before
capture begins. Escape cancels capture, Delete clears a binding and capture
times out after 15 seconds. Duplicate bindings within a player swap positions.
**Save controls** persists both players. **Cancel** and the window close button
discard the draft. **Reset player** restores only the selected player's defaults.

The same profile is written to the game at launch. Button names show common
Xbox / PlayStation labels; SDL-compatible controllers use the same logical map.
Select a 3- or 6-button Genesis pad and adjust deadzone as needed. **Left stick
also steers** is optional; turn it off for fully custom axis mappings.

## Player 1 defaults

| Genesis input | Keyboard | Gamepad |
|---|---|---|
| Directions | Arrow keys | D-pad; optional left stick |
| A | Z | B / Circle |
| B | X | A / Cross |
| C | C | X / Square |
| Start | Enter | Start / Menu |
| X / Y / Z | A / S / D | Y / Triangle; left / right shoulder |
| Mode | Backspace | Back / View |

Function keys, Tab and Escape remain reserved for runtime/window commands.
F6 opens local achievements. The launcher profile disables the engine's fixed
shoulder-button snapshot shortcuts and Back-button turbo shortcut, allowing
those buttons to be used as regular mapped inputs.

## Campaign checkpoint (0.60)

After a single-player campaign race ends, the game automatically replaces its
previous campaign checkpoint. Press **F1** or the assigned Player 1 controller's
**R2 / RT** to load it, including after restarting the game. Holding the trigger
loads once; release it before loading again. This shortcut uses the latest
automatic checkpoint, independently of the engine's numbered manual slots.
Player 1's R2 / RT axis is reserved for this shortcut; choose another control
for a Genesis action previously assigned to it. Player 2 mappings are unchanged.

Each game has its own file in `.local/saves/rrN-campaign.state` under the launcher
root, or `saves/` beside the executable when launched directly. Saving is atomic;
a failed save retains the previous checkpoint. Files are tied to the exact
local build and ROM; an incompatible or damaged file is rejected. Untouched
attract demonstrations and split-screen sessions do not replace campaign saves.
Existing achievements, control profiles and original ROMs are independent.

## Launcher navigation

Tab moves through native controls; Shift+Tab moves back. Arrow keys or the
gamepad D-pad select a game, Enter on a game card or Play starts the selected
title, and Enter on a settings button opens that page. Space activates focused
native buttons. Gamepad A starts the selected title and Y opens Video & CRT.
Keyboard/controller remapping applies to gameplay, not Windows navigation.

Controllers tested by the automated backend use SDL virtual devices. Specific
physical controllers, Bluetooth connection behavior and drivers need hardware
verification; compatibility is not guaranteed for every device.
