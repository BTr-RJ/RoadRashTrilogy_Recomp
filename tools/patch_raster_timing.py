"""Road Rash changes its palette/scroll in HBlank: raster order matters."""
from pathlib import Path
import difflib
ROOT=Path(__file__).resolve().parents[1]
def save(name,old,new):
    p=ROOT/'.tools/engine/runner/video'/name
    p.write_text(new,encoding='utf-8')
    (ROOT/'patches'/('raster-'+name+'.patch')).write_text(''.join(difflib.unified_diff(
        old.splitlines(True),new.splitlines(True),fromfile='a/runner/video/'+name,tofile='b/runner/video/'+name)),encoding='utf-8')
p=ROOT/'.tools/engine/runner/video/genesis_machine.c'
old=p.read_text(encoding='utf-8')
if 'RR_RASTER_PHASE' not in old:
    new=old.replace('void machine_run_frame(','''/* RR_RASTER_PHASE: physical raster time, independent of codegen accounting. */
extern uint32_t g_audio_cycle_counter;
static uint32_t rr_slice_cpu, rr_slice_phase;
uint32_t rr_raster_phase(void) {
    extern uint32_t g_audio_cycle_counter;
    return rr_slice_phase + (g_audio_cycle_counter-rr_slice_cpu)*7u;
}
void machine_run_frame(''')
    start=new.index('        /* RR: arbitrate in eight bounded slices.')
    end=new.index('        /* Deliver interrupts to the 68K',start)
    new=new[:start]+new[end:]
    # Move IRQs and rendering into a bounded slice loop. Active pixels are
    # committed BEFORE HBlank palette/scroll writes for the following row.
    anchor='        /* Deliver interrupts to the 68K (and the Z80 at vblank). */'
    new=new.replace(anchor,'''        for (unsigned slice=0;slice<8;++slice) {
        rr_slice_phase=slice*MASTER_PER_LINE/8u;
        rr_slice_cpu=g_audio_cycle_counter;
        m->vdp.in_hblank=slice>=6;
        if(slice==0 || slice==6) {
'''+anchor)
    new=new.replace('        for (unsigned slice=0;slice<8;++slice) {',
        '        uint16_t rr_latched_vsram[GVDP_VSRAM_ENTRIES];\n        memcpy(rr_latched_vsram,m->vdp.vsram,sizeof rr_latched_vsram);\n        for (unsigned slice=0;slice<8;++slice) {')
    new=new.replace('if (irq & GVDP_IRQ_HBLANK) glue_own_interrupt(4, &m->vdp);','if (slice==6 && (irq & GVDP_IRQ_HBLANK)) glue_own_interrupt(4, &m->vdp);')
    new=new.replace('if (irq & GVDP_IRQ_VBLANK) {','if (slice==0 && (irq & GVDP_IRQ_VBLANK)) {')
    new=new.replace('        /* Render + emit active scanlines.', '''        }
        if(slice==6) {
        /* Render + emit active scanlines.''')
    new=new.replace('        m->master_cycle += MASTER_PER_LINE;', '''        }
        s_z80_off=rr_slice_phase;
        glue_run_game_chunk((slice+1u)*M68K_PER_LINE/8u-slice*M68K_PER_LINE/8u);
        glue_own_vint_service_latched(&m->vdp);
        step_z80(m,(slice+1u)*Z80_PER_LINE/8u-slice*Z80_PER_LINE/8u);
        }
        m->master_cycle += MASTER_PER_LINE;''')
    new=new.replace('                int n = gvdp_render_scanline(&m->vdp, row, idxbuf);',
        '                uint16_t rr_live_vsram[GVDP_VSRAM_ENTRIES];\n'
        '                memcpy(rr_live_vsram,m->vdp.vsram,sizeof rr_live_vsram);\n'
        '                memcpy(m->vdp.vsram,rr_latched_vsram,sizeof rr_latched_vsram);\n'
        '                int n = gvdp_render_scanline(&m->vdp, row, idxbuf);\n'
        '                memcpy(m->vdp.vsram,rr_live_vsram,sizeof rr_live_vsram);')
    save('genesis_machine.c',old,new)
p=ROOT/'.tools/engine/runner/video/genesis_vdp.c'
old=p.read_text(encoding='utf-8')
if 'RR_PHYSICAL_HV' not in old:
    new=old.replace('    v->in_hblank ^= 1;', '    /* RR: status reads must not manufacture HBlank edges. */')
    new=new.replace('    extern uint32_t g_audio_cycle_counter;\n    unsigned vertical = (unsigned)v->scanline;',
                    '    extern uint32_t rr_raster_phase(void); /* RR_PHYSICAL_HV */\n    unsigned phase=rr_raster_phase();\n    unsigned vertical=(unsigned)v->scanline+phase/3420u;\n    phase%=3420u;')
    new=new.replace('    unsigned phase = ((g_audio_cycle_counter % 488u) * 7u) % 3420u;\n','')
    save('genesis_vdp.c',old,new)
print('Applied active pixels before HBlank and physical HV phase')
