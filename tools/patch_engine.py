"""Apply narrowly-scoped integration fixes to the pinned engine copy.

The pristine source ZIP remains cached for inspection. No generated C is edited.
"""
from pathlib import Path
import difflib

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / ".tools/engine/runner/glue.c"
original = path.read_text(encoding="utf-8")
if "RR_GAME_IRQ_INTEGRATION" in original:
    import zipfile
    archive = ROOT / ".tools/downloads/segagenesisrecomp-252535d.zip"
    with zipfile.ZipFile(archive) as source:
        name = next(n for n in source.namelist() if n.endswith("/runner/glue.c"))
        original = source.read(name).decode("utf-8").replace("\r\n", "\n")
modified = original


def replace(before, after):
    global modified
    if modified.count(before) != 1:
        raise ValueError("Pinned engine patch context does not match")
    modified = modified.replace(before, after)


replace("static void yield_to_main(GlueYieldSite site)\n", "/* RR_GAME_IRQ_INTEGRATION: service device-polling ISRs on the game fiber. */\nstatic int rr_hint_latched;\nstatic void rr_service_pending_irq(void);\nstatic void yield_to_main(GlueYieldSite site)\n")
replace("    fiber_switch(s_main_fiber);\n}\n\n/* Engine-owned game fiber stack", "    fiber_switch(s_main_fiber);\n    rr_service_pending_irq();\n}\n\n/* Engine-owned game fiber stack")
replace("if (interleave_irq_on() && s_game_yielded_vblank) {", "if (1) { /* Every Road Rash IRQ must allow VDP/Z80 time to advance. */")
replace("    } else if (level == 4 && imask < 4) {\n", "    } else if (level == 4 && imask < 4) {\n        if (s_pending_irq < 4) s_pending_irq = 4;\n        s_game_yielded_vblank = 0;\n        return; /* handled at the suspended game fiber's next resume */\n")
replace("    if (s_irq_in_progress) { if (level == 6) s_own_vint_latched = 1; return; }", "    if (s_irq_in_progress) { if (level == 6) s_own_vint_latched = 1; else if(level==4) rr_hint_latched=1; return; }\n    if(level==4 && ((g_cpu.SR>>8)&7)>=4) rr_hint_latched=1;")
replace("    s_own_vint_latched = 0;\n    own_deliver_vint(vdp);\n    return 1;", "    s_own_vint_latched = 0;\n    s_pending_irq = 6;\n    s_game_yielded_vblank = 0;\n    return 1;")
# Interleaved handlers advance across real scanlines. Never force VBlank or
# restore an obsolete flag: that made active-display palette/VRAM transfers
# use blanking bandwidth and resurrected VBlank after the raster reached 0.
irq_start = modified.index("static void own_run_handler_interleaved(int level, GVDP *vdp)\n{")
irq_end = modified.index("/* Own-backend interrupt delivery", irq_start)
irq_body = modified[irq_start:irq_end]
assert irq_body.count("    uint8_t saved_vb = vdp->in_vblank; vdp->in_vblank = 1;\n") == 1
irq_body = irq_body.replace("    uint8_t saved_vb = vdp->in_vblank; vdp->in_vblank = 1;\n", "    /* RR_REAL_RASTER: scheduler owns VBlank throughout the ISR. */\n")
irq_body = irq_body.replace("    vdp->in_vblank = saved_vb;\n", "")
modified = modified[:irq_start] + irq_body + modified[irq_end:]
anchor = "/* Own-backend interrupt delivery"
if modified.count(anchor) != 1:
    raise ValueError("IRQ helper anchor mismatch")
helper = """/* Called only after the game fiber resumes from a scheduler yield. Unlike
 * atomic ISR execution, this permits the HV counter and Z80 mailbox to change
 * while the ISR polls them. Existing execution state/snapshots already carry
 * s_pending_irq and s_irq_in_progress; no new machine state is introduced. */
static void rr_service_pending_irq(void)
{
    if (!s_pending_irq && rr_hint_latched && !s_irq_in_progress) {
        s_pending_irq=4; rr_hint_latched=0;
    }
    if (s_pending_irq && !s_irq_in_progress && !s_in_vblank_service) {
        int level = s_pending_irq;
        s_pending_irq = 0;
        if (((g_cpu.SR >> 8) & 7) < level)
            own_run_handler_interleaved(level, &g_machine.vdp);
        else if (level == 6)
            s_own_vint_latched = 1;
        else if (level == 4)
            rr_hint_latched = 1;
    }
}

"""
modified = modified.replace(anchor, helper + anchor)
patch = "".join(difflib.unified_diff(original.splitlines(True), modified.splitlines(True),
                                    fromfile="a/runner/glue.c", tofile="b/runner/glue.c"))
(ROOT / "patches").mkdir(exist_ok=True)
(ROOT / "patches/engine-game-irqs.patch").write_text(patch, encoding="utf-8")
path.write_text(modified, encoding="utf-8")
print("Applied game-fiber IRQ integration; saved reviewable patch")

# The pinned GameSpec advertises frame callbacks but sim_step never calls them.
step_path = ROOT / ".tools/engine/runner/sim_step.c"
step_original = step_path.read_text(encoding="utf-8")
if "RR_FRAME_CALLBACKS" not in step_original:
    step = step_original.replace('#include "sim_step.h"', '#include "sim_step.h"\n#include "game_spec.h" /* RR_FRAME_CALLBACKS */')
    step = step.replace('    machine_run_frame(hooks->sink, hooks->sink_user);',
                        '    if (g_game_spec.on_frame_pre) g_game_spec.on_frame_pre(s_ticks);\n'
                        '    machine_run_frame(hooks->sink, hooks->sink_user);\n'
                        '    if (g_game_spec.on_frame_post) g_game_spec.on_frame_post(s_ticks);')
    step_path.write_text(step, encoding="utf-8")
    (ROOT / "patches/engine-frame-callbacks.patch").write_text("".join(difflib.unified_diff(
        step_original.splitlines(True), step.splitlines(True), fromfile="a/runner/sim_step.c",
        tofile="b/runner/sim_step.c")), encoding="utf-8")

main_path = ROOT / ".tools/engine/runner/main.c"
main_original = main_path.read_text(encoding="utf-8")
if "RR_OUTPUT_SIZE" not in main_original:
    before = '    /* Scripted launcher tests must stay hidden after the handoff to gameplay. */'
    after = '''    /* RR_OUTPUT_SIZE: explicit output dimensions selected in the trilogy launcher.
     * Native rendering remains at console resolution until its custom renderer
     * is enabled; the SDL target controls output scaling and letterboxing. */
    {
        const char *rw=getenv("RR_OUTPUT_WIDTH"), *rh=getenv("RR_OUTPUT_HEIGHT");
        if (rw && rh) {
            int w=atoi(rw), h=atoi(rh);
            if (w>=320 && w<=7680 && h>=224 && h<=4320) { win_w=w; win_h=h; }
        }
    }
''' + before
    if main_original.count(before)!=1: raise ValueError("Output-size patch context mismatch")
    main=main_original.replace(before,after)
    main_path.write_text(main,encoding="utf-8")
    (ROOT / "patches/engine-output-size.patch").write_text("".join(difflib.unified_diff(
        main_original.splitlines(True),main.splitlines(True),fromfile="a/runner/main.c",tofile="b/runner/main.c")),encoding="utf-8")
