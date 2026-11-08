"""Portable guest continuations for Road Rash checkpoints, never host stacks."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def edit(relative,old,new):
    p=ROOT/'.tools/engine'/relative
    text=p.read_text(encoding='utf-8')
    if new in text:return
    if text.count(old)!=1:raise ValueError(f'Campaign patch context: {relative}')
    p.write_text(text.replace(old,new,1),encoding='utf-8')

# Only expose the existing, exact static-entry lookup. Cold continuation uses
# the interpreter between entries and the unchanged native code for routines.
edit('external/m68k-recomp-core/profiles/genesis/code_generator.c',
     '"static FuncPtr recomp_find_func(uint32_t addr)',
     '"FuncPtr recomp_find_func(uint32_t addr)')
edit('external/m68k-recomp-core/profiles/genesis/code_generator.c',
     '        if (s_reverse_debug) {\n            if (has_aliases) {',
     '''        /* RR_STATE_ENTRY: serialize only at a native instruction boundary. */
        fprintf(f_func, "  extern int rr_state_requested; extern int rr_state_park(uint32_t);\\n");
        if (has_aliases)
            fprintf(f_func, "  if(rr_state_requested) rr_state_park(_entry);\\n");
        else
            fprintf(f_func, "  if(rr_state_requested) rr_state_park(0x%06Xu);\\n",func_addr);
        if (s_reverse_debug) {\n            if (has_aliases) {''')
edit('external/m68k-recomp-core/profiles/genesis/code_generator.c',
     'fprintf(f, "    glue_yield_for_interrupt_poll(); goto label_%06X;\\n",\n                            instr->target_addr);',
     'fprintf(f, "    if(!rr_state_requested || !rr_state_park(0x%06Xu)) glue_yield_for_interrupt_poll(); goto label_%06X;\\n",\n                            instr->target_addr, instr->target_addr);')
edit('external/m68k-recomp-core/profiles/genesis/code_generator.c',
     'fprintf(f, "    glue_yield_for_interrupt_poll();\\n");',
     'fprintf(f, "    if(!rr_state_requested || !rr_state_park(0x%06Xu)) glue_yield_for_interrupt_poll();\\n",instr->target_addr);')

edit('runner/glue.c','/* Game fiber entry point. */',r'''/* RR_CAMPAIGN_CONTINUATION: portable PC/register/guest-stack continuation.
 * No fiber bytes, return addresses into the executable, or C locals on disk.
 * Native routines use the established A7-neutral RTS ABI. The floor executes
 * the short caller continuations, then native entry lookup takes over again. */
extern RecompFuncPtr recomp_find_func(uint32_t addr);
static void rr_continue_guest(void)
{
    rr_service_pending_irq(); /* same resume edge as the original scheduler yield */
    for (;;) {
        uint32_t pc=g_cpu.PC & 0xFFFFFFu;
        RecompFuncPtr fn=recomp_find_func(pc);
        if (fn) {
            recomp_call_func(fn);
            g_rte_pending=0; g_split_sp_popped=0;
            g_cpu.PC=m68k_read32(g_cpu.A[7]) & 0xFFFFFFu;
            g_cpu.A[7]+=4;
        } else {
            M68kiStatus st=m68k_interp_step();
            if (st!=M68KI_OK) {
                fprintf(stderr,"[RR STATE] continuation halted status=%d PC=%06X\n",st,pc);
                for (;;) yield_to_main(GLUE_YIELD_HALT);
            }
        }
    }
}

/* Game fiber entry point. */''')
edit('runner/glue.c','        s_game_fiber_resume_pc = 0;\n        g_cpu.A[7] = g_game_layout.initial_ssp;',
     '        s_game_fiber_resume_pc = 0;\n        if (resume_pc==0xFFFFFEu) { rr_continue_guest(); return; }\n        g_cpu.A[7] = g_game_layout.initial_ssp;')
edit('runner/glue.c','int glue_state_boundary_ready(void) { return s_state_parked; }',r'''int glue_state_boundary_ready(void) { return s_state_parked; }
static int s_pending_irq;
extern int rr_adapter_atomic;
int rr_state_park(uint32_t pc)
{
    if (!s_state_requested || s_state_parked || s_irq_in_progress || s_in_vblank_service ||
        rr_adapter_atomic || !s_main_fiber) return 0;
    g_cpu.PC=pc; s_state_parked=1;
    yield_to_main(GLUE_YIELD_IRQPOLL);
    return 1;
}''')
edit('runner/glue.c','static int s_state_requested,s_state_parked;',
     'int rr_state_requested;\n#define s_state_requested rr_state_requested\nstatic int s_state_parked;')
edit('runner/glue.c','    int32_t  state_requested, state_parked;',
     '    int32_t  state_requested, state_parked, rr_hint_latched;')
edit('runner/glue.c','    s.state_parked         = s_state_parked;',
     '    s.state_parked         = s_state_parked;\n    s.rr_hint_latched      = rr_hint_latched;')
edit('runner/glue.c','    s_state_parked              = s.state_parked;',
     '    s_state_parked              = s.state_parked;\n    rr_hint_latched             = s.rr_hint_latched;')
edit('runner/glue.c','/* EXEC: [u64 tail-frame head]',r'''/* Restart the host stack while retaining the serialized scheduler clocks.
 * The saved PC is at a pre-instruction game hook, outside any interrupt. */
void rr_state_resume(void)
{
    GlueRbSched saved;
    glue_rb_sched_save(&saved,sizeof saved);
    glue_restart_game_fiber(0xFFFFFEu);
    recomp_tail_frame_set(NULL);
    glue_rb_sched_load(&saved,sizeof saved);
    s_state_requested=s_state_parked=0;
    s_in_floor=0; /* the serialized interpreter C frame was replaced */
    s_game_fiber_resume_pc=0xFFFFFEu;
    s_game_running=1; s_game_yielded_vblank=0;
    s_yield_site=GLUE_YIELD_IRQPOLL;
}

/* EXEC: [u64 tail-frame head]''')
edit('runner/m68k_interp.c','static int interp_game_hook(uint32_t pc)\n{',
     'static int interp_game_hook(uint32_t pc)\n{\n    extern int rr_state_requested; extern int rr_state_park(uint32_t);\n    if(rr_state_requested)rr_state_park(pc); /* RR_INTERPRETED_STATE_BOUNDARY */')

edit('runner/main.c','        if (g_game_spec.state_size) host_state_tick();',
     '        { extern void rr_campaign_tick(void); rr_campaign_tick(); }\n        if (g_game_spec.state_size) host_state_tick();')
edit('runner/main.c','        if (!running) break; /* Close must not execute another game/render tick. */',
     '        { extern void rr_campaign_input(void); rr_campaign_input(); }\n        if (!running) break; /* Close must not execute another game/render tick. */')
edit('runner/main.c','                if(!ev.key.repeat && rr_game_key((int)ev.key.keysym.sym)) continue;',
     '                if(ev.key.repeat && ev.key.keysym.sym==SDLK_F1) continue;\n                if(!ev.key.repeat && rr_game_key((int)ev.key.keysym.sym)) continue;')
edit('runner/host_state.h','static void host_state_notice(const char *message)\n{',
     'static void host_state_notice(const char *message)\n{\n    extern void rr_campaign_notice(const char *); rr_campaign_notice(message);')
print('Applied portable Road Rash campaign continuation')
