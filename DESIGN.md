---
name: Road Rash Trilogy Recomp
description: A sober native Windows launcher centered on the three classic covers.
colors:
  primary: "#ffc925"
  primary-pressed: "#d69d13"
  neutral-bg: "#101315"
  panel: "#1a1d1f"
  cover-surface: "#171a1c"
  cover-selected: "#262a2b"
  button: "#212527"
  button-hover: "#313535"
  ink: "#f4f0e2"
  muted: "#a6a9a3"
  dialog-ink: "#e8eae9"
  primary-ink: "#111416"
  divider: "#3e4241"
  cover-rule: "#484d4c"
  hover-border: "#a8aba5"
typography:
  display:
    fontFamily: "Barlow Condensed, Segoe UI, sans-serif"
    fontSize: "69px"
    fontWeight: 700
  headline:
    fontFamily: "Barlow Condensed, Segoe UI, sans-serif"
    fontSize: "49px"
    fontWeight: 700
  title:
    fontFamily: "Barlow Condensed, Segoe UI, sans-serif"
    fontSize: "25px"
    fontWeight: 700
  body:
    fontFamily: "Barlow, Segoe UI, sans-serif"
    fontSize: "18px"
    fontWeight: 400
  dialog:
    fontFamily: "Barlow, Segoe UI, sans-serif"
    fontSize: "16px"
    fontWeight: 400
  label:
    fontFamily: "Barlow, Segoe UI, sans-serif"
    fontSize: "14px"
    fontWeight: 400
rounded:
  square: "0px"
spacing:
  cover-inset: "10px"
  action-inset: "18px"
  dialog-inset: "24px"
  cover-gap: "28px"
  shell-inset: "42px"
components:
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.primary-ink}"
    rounded: "{rounded.square}"
    height: "58px"
    width: "284px"
  button-primary-active:
    backgroundColor: "{colors.primary-pressed}"
    textColor: "{colors.primary-ink}"
    rounded: "{rounded.square}"
  button-secondary:
    backgroundColor: "{colors.button}"
    textColor: "{colors.ink}"
    rounded: "{rounded.square}"
    height: "48px"
    width: "284px"
  button-secondary-hover:
    backgroundColor: "{colors.button-hover}"
    textColor: "{colors.ink}"
    rounded: "{rounded.square}"
  cover-card:
    backgroundColor: "{colors.cover-surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.square}"
    width: "264px"
    height: "414px"
  cover-card-selected:
    backgroundColor: "{colors.cover-selected}"
    textColor: "{colors.primary}"
    rounded: "{rounded.square}"
  installer-field:
    backgroundColor: "{colors.panel}"
    textColor: "{colors.dialog-ink}"
    typography: "{typography.dialog}"
    height: "29px"
---

# Design System: Road Rash Trilogy Recomp

## Overview

**Creative North Star: "The Trilogy Shelf"**

This descriptive label captures the incumbent system: three recognizable classic
covers against a sober charcoal background, with a clear route into the selected
game. It records the implemented direction rather than a separately approved
concept or visual exploration.

The launcher is a native Win32 application drawn with GDI+, with standard Windows
controls in its utility dialogs. Condensed headings give the games presence;
regular body text keeps configuration and installation readable. The original
motorcycle masthead supports the covers without becoming the primary attraction.
The user-confirmed commitments are the dark background and classic covers.

**Key Characteristics:**

- Classic cover art leads the visual hierarchy.
- Warm text and a restrained yellow accent sit on charcoal surfaces.
- Square, flat controls keep the interface direct.
- Native Windows interaction remains visible and usable.
- English labels describe local actions and actual game readiness.

## Colors

The palette uses one warm accent and quiet charcoal neutrals; the frontmatter is
the normative token list extracted from the native source.

### Primary

- **Ride Yellow** (`primary`): selected-cover border, underline and title; the
  Play / Install game action; the small trilogy descriptor.
- **Pressed Yellow** (`primary-pressed`): primary action while pressed.

### Neutral

- **Road Charcoal** (`neutral-bg`): the main client background.
- **Utility Charcoal** (`panel`): dialog backgrounds and installer edits.
- **Shelf Charcoal** (`cover-surface`): cover cards and the masthead base.
- **Selected Charcoal** (`cover-selected`): selected cover card.
- **Action Charcoal** (`button`) and **Raised Tone** (`button-hover`): secondary
  actions at rest and under hover or focus.
- **Warm Paper** (`ink`): main text; **Quiet Grey** (`muted`): metadata and hints.
- **Dialog Paper** (`dialog-ink`): controls and installer text.
- **Action Ink** (`primary-ink`): text on the yellow primary action.
- **Rule Grey** (`divider`, `cover-rule`): shell separators and cover underlines.
- **Hover Grey** (`hover-border`): unselected cover hover and focus borders.

**The Selection Rule.** Yellow identifies the chosen game and the main action.
Keep settings and supporting copy visually quieter.

## Typography

**Display Font:** Barlow Condensed Bold, with Segoe UI Bold fallback.
**Body Font:** Barlow Regular, with Segoe UI fallback.
**Accent Font:** Barlow Condensed Bold Italic, privately bundled under the same
OFL license, for the yellow RECOMP wordmark and centered primary action.

The bundled fonts are loaded privately. Sizes below are GDI+ logical drawing
pixels before the main canvas scale, not CSS pixels or fixed physical sizes.
Native dialog fonts scale independently with their dialog DPI factor.

- **Display:** ROAD RASH TRILOGY uses the display token. Yellow RECOMP uses the
  accent face at 34px, positioned 14 logical units after the title's visible ink.
- **Headline:** the game-selection heading uses the headline token.
- **Title:** cover names use the title token; the Settings heading uses (27px).
- **Body:** action labels and supporting text use regular Barlow; game
  descriptions use (19px). The primary action uses the accent face at (32px),
  centered horizontally and vertically using the actual glyph bounds.
- **Dialog:** utility labels and controls use the dialog token.
- **Label:** help, version and build status use the label token; cover metadata
  uses (13px), and achievement status uses (15px).

**The Legibility Rule.** Condensed bold belongs to short headings and game names.
Keep instructions, field values and longer text in regular Barlow.

## Layout

The main window lays out a centered (1280 by 800) logical canvas. Its uniform
scale is the smaller of client width / 1280 and client height / 800; extra space
is letterboxed in the background color. The minimum tracked outer window is
(920 by 600) logical units multiplied by Windows DPI scale. It does not switch
to a mobile layout.

The cover shelf starts at (42, 240), with three (264 by 414) cards advancing by
(292) units. A rule at x=(924) separates the shelf from the (284)-unit settings
rail at x=(952). The primary action anchors that rail below the status text.
Keep all three covers visible at the minimum size.

Controls and installation use separate native modal windows. They scale from
the parent DPI and are capped to the work-area height. Preserve their labeled
rows, aligned columns, bottom actions and readable status area. Long installer
output belongs in its scrollable log control.

## Elevation & Depth

Depth comes from tonal layering and thin rules. The custom launcher controls
have no drop shadows, glass or decorative lifting animation. Windows supplies
the non-client frame and the appearance of standard dialog controls; their
white buttons, combo boxes and blue slider handles are part of the observed
native presentation rather than additional brand tokens.

## Shapes

Custom controls are rectangular with square corners. Selected covers combine a
thin outer border with a stronger bottom rule; focused cards increase border
weight. Main actions use an inset focus rectangle. Native dialog controls keep
their operating-system shapes and focus treatments.

## Components

### Buttons

The primary action is a broad yellow rectangle labeled **PLAY** when the selected
game is ready, otherwise **INSTALL GAME**. It uses dark text and a darker pressed
fill. Secondary actions use charcoal with a lighter hover/focus tone. Focus is
visible as an inset (2px) rectangle, dark on the primary action and yellow on
secondary actions. Native Save, Cancel, Browse and Close buttons retain Windows
behavior and appearance.

### Cover selection

Each full card is an owner-drawn, tab-accessible Windows button. Fit each classic
cover inside its available area using its original aspect ratio; do not crop or
stretch it. Selection changes the panel tone, title and border to yellow and
thickens the underline to (3px). Keep year and Ready / Not installed text beneath
the title. Focus and selection remain distinct state information.

### Navigation

Tab and Shift+Tab traverse native controls. Left and Right change the selected
game, including when a launcher button has focus. Enter on a cover launches the
currently selected game; Enter on a settings action activates that action.
Standard button Space behavior remains available. Controller D-pad selection
and A activation are implemented through SDL; physical devices require their
own verification.

### Inputs / Fields

Use real combo boxes, edits, checkboxes and trackbars. Installer path edits have
dark fills and thin native borders. Controls mapping presents Genesis action,
keyboard binding and gamepad binding in aligned columns. During capture, replace
the chosen binding with **Waiting for input...** and explain Escape to cancel
and Delete to clear. Capture times out after (15 seconds). Duplicate bindings
within a player swap. Changes apply on Save controls; Cancel restores the draft.

### Installer and status

Keep one ROM-folder row per game and one installation-folder row, with adjacent
Browse actions. Installation status, progress and a scrollable log stay visible
together. While work runs, disable editable paths, present Stop installation,
and restore a clear retry or completion action. Successful completion offers
Open launcher. Show validation and failure guidance where the player can act.

### Artwork

Use the three supplied classic covers, the subordinate original motorcycle
masthead and the RR application icon. Covers and bundled fonts have recorded
URLs and hashes in `assets/launcher/sources.json`; generated brand assets use
their companion provenance records. Preserve that traceability when replacing
an asset. A screenshot does not establish redistribution rights.

## Do's and Don'ts

### Do:

- **Do** preserve the three-cover hierarchy and original cover proportions.
- **Do** reserve yellow emphasis for selection and the primary action.
- **Do** retain visible keyboard focus and native dialog navigation.
- **Do** keep the motorcycle masthead subordinate to the games.
- **Do** write English labels that explain the current local state.

### Don't:

- **Don't** replace classic covers with generated reinterpretations.
- **Don't** stretch or crop a cover to fill its card.
- **Don't** introduce decorative shadows or rounded cards into the flat shell.
- **Don't** let artwork obscure headings, status or controls.
- **Don't** imply network play, official service unlocks or complete campaign validation.
