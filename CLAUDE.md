# Heritage

First-person investigation/puzzle game by a 9-person student studio. UE 5.9 Preview, C++ + Blueprint.
Engine (with source) is at `C:\Users\Chiip03\Epic Games\UE_5.9` — read engine source there instead of guessing 5.9 APIs.

## Where the team's truth lives

- Notion hub "Heritage" (page id `405b228e10738098a654c27a5d75860e`): GDD "===Heritage. CN===", Task Board, Milestone tracker, Workflow Guidelines.
- FigJam "Heritage-Brainstorm" (file key `7ox9x12lhGS2dhS0NfGEl2`): references, flow diagrams, noun/verb diagram, prototype notes.
- Re-read these when a task depends on current design; they change often. Docs are in Chinese; talk to the user in Chinese.

## Phase and how to work

The team follows ideation → preproduction → full production with Scrum. Current phase: **ideation**.
Design written in the docs is creative reference, not a spec. Do not build the full game loop from the GDD.

Work is one of two kinds:

1. **Toy prototype** (now): one level, one idea, throwaway. Before building, state the one question it answers
   (yes/no or A-vs-B), the player verbs it studies, and the experience it should produce. Afterwards record the conclusion.
   Greybox only; placeholder art and audio. Speed over architecture, no shared systems between prototypes.
   Prototypes are the user's personal work, used to show ideas to the team; teammates build their own. No collaboration rules apply.
   - **Default medium: Three.js**, one self-contained `Prototypes/<name>/index.html` (libraries from CDN, no build step) so it
     runs by opening the file and can be shared as a link. Good for verbs, rules, timeline/gap logic, UI flows, camera ideas.
   - **Use UE instead** when the answer depends on the engine itself: Lumen lighting/exposure, post-process look, Chaos physics
     feel, Sequencer playback, or anything meant to be carried into the vertical slice. UE prototypes live in
     `Content/Developers/<name>/<PrototypeName>/` with their own map.
   - Say which medium and why before starting.
2. **Precise single feature** (vertical slice and later): scoped to exactly what was asked, production quality.

## Game direction (reference only)

Player is a historian commissioned to catalogue a derelict manor. Damaged film reels let them revisit the past:
watch intact footage, enter the damaged gaps, infer and act out what happened, then compose the result in the Archive.
Pillars: environment interaction; reflective revisit (edit film / reconstruct). Core stream: observe → infer → act and verify → understand.
Keep text minimal (not a visual novel). All player behaviour reduces to a few fixed verbs (grab, drop, throw, run, jump, use, photograph…);
the environment responds to verbs, the player does not pick from option menus. No inventory; the Archive records things.

## Project layout

- `Source/Heritage/` — first-person template code (`HeritageCharacter`, `HeritagePlayerController`, …) plus template
  `Variant_Horror` and `Variant_Shooter`. The variants are template leftovers, not game design.
- `Content/` is still the stock template layout.
- `Prototypes/` — web prototypes, one folder each.
- The Notion Workflow Guidelines (folder layout, naming prefixes) are for the future production project. Ignore them for prototypes.

## Driving the editor

The editor exposes an MCP server (`unreal-mcp`, `http://127.0.0.1:8000/mcp`, configured in `.mcp.json`). It only exists while the
editor is open. It uses tool search: `list_toolsets` → `describe_toolsets` → `describe_tools` → `call_tool`.
Prefer it for assets, Blueprints, levels, PIE, input injection and screenshots; edit C++ and config as files.
Verify a prototype by running PIE and capturing the viewport rather than assuming it works.

Build from the command line only when the editor is closed (otherwise use Live Coding):
`"C:\Users\Chiip03\Epic Games\UE_5.9\Engine\Build\BatchFiles\Build.bat" HeritageEditor Win64 Development -Project="<abs path>\Heritage.uproject" -WaitMutex`

Version control is a local git repo with Git LFS for `.uasset`/`.umap` and media (personal, no remote). Commit only when asked.
