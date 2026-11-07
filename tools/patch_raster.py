"""Replace frozen HV counters and coarse Z80 bus arbitration in pinned runtime.

Horizontal timing remains an NTSC approximation; retain this limitation in QA.
"""
from pathlib import Path
import difflib
import zipfile

ROOT = Path(__file__).resolve().parents[1]
archive = ROOT / ".tools/downloads/segagenesisrecomp-252535d.zip"


def pristine(relative):
    with zipfile.ZipFile(archive) as file:
        name = next(n for n in file.namelist() if n.endswith("/" + relative))
        return file.read(name).decode("utf-8").replace("\r\n", "\n")


def save(relative, original, modified, name):
    (ROOT / ".tools/engine" / relative).write_text(modified, encoding="utf-8")
    (ROOT / "patches" / name).write_text("".join(difflib.unified_diff(
        original.splitlines(True), modified.splitlines(True),
        fromfile="a/" + relative, tofile="b/" + relative)), encoding="utf-8")


relative = "runner/video/genesis_vdp.c"
original = pristine(relative)
before = '''    /* Approximate: V counter in the high byte, H counter low. Refined during
     * raster-timing validation. */
    return (uint16_t)(((v->scanline & 0xFF) << 8) | (v->in_hblank ? 0x00 : 0x80));'''
after = '''    /* RR: NTSC 224-line vertical discontinuity; the old 0/0x80 horizontal
     * value cannot satisfy Road Rash's horizontal-counter polling loops.
     * Approximate the horizontal phase from the 68K cycle axis. The H40
     * irregular pixel clock and HV latch still need hardware-level validation. */
    extern uint32_t g_audio_cycle_counter;
    unsigned vertical = (unsigned)v->scanline;
    if (vertical > 0xEAu) vertical -= 6u;
    unsigned phase = ((g_audio_cycle_counter % 488u) * 7u) % 3420u;
    unsigned count, horizontal;
    if (v->reg[12] & 1u) {
        count = phase * 211u / 3420u;
        horizontal = count <= 0xB6u ? count : count + 0x2Du;
    } else {
        count = phase * 171u / 3420u;
        horizontal = count <= 0x93u ? count : count + 0x55u;
    }
    return (uint16_t)(((vertical & 0xFFu) << 8) | (horizontal & 0xFFu));'''
if original.count(before) != 1:
    raise ValueError("HV patch context mismatch")
modified = original.replace(before, after)
modified = modified.replace('((REG_SPRITE_NT(v) & 0x7F) << 9)', '((REG_SPRITE_NT(v) & (h40 ? 0x7E : 0x7F)) << 9)')
modified = modified.replace('(vram_read_word(v, e) & 0x3FF) - (im2 ? 256 : 128)', '(vram_read_word(v, e) & (im2 ? 0x3FF : 0x1FF)) - (im2 ? 256 : 128)')
modified = modified.replace('vram_read_word(v, (uint16_t)(e + 6)) & 0x3FF', 'vram_read_word(v, (uint16_t)(e + 6)) & 0x1FF')
modified = modified.replace('    int total = gvdp_active_width(v);', '    int total = w; /* RR: replay evaluates authentic sprite status. */')
modified = modified.replace('static int s_ws_extra = 0;', 'static int s_ws_extra = 0;')
modified = modified.replace('void gvdp_set_ws_extra(int extra_px)',
    'static int rr_road_profile;\nvoid gvdp_set_road_profile(int profile){rr_road_profile=profile;}\n\nvoid gvdp_set_ws_extra(int extra_px)')
anchor='''        fetch_plane_pixel(v, &cache_b, base_b, wt, ht,
                          (x - hs_b), (line + vs_b), im2, &b_idx, &b_op, &b_hi);
#endif'''
replacement='''        fetch_plane_pixel(v, &cache_b, base_b, wt, ht,
                          (x - hs_b), (line + vs_b), im2, &b_idx, &b_op, &b_hi);
        /* RR_ROAD_STRIPS: RR2/3's 1024px plane is a finite road bitmap,
         * not a periodically tiled world. Native road generators $14822 /
         * $1A028 offset their edges by 672. Expanding a modulo-1024 fetch
         * wraps the opposite edge back into view in strong curves.
         * Preserve signed source coordinates, discard the repeated asphalt,
         * and continue the authored ground tile at its 8px phase. Sky and
         * native columns keep the hardware sampling rules. */
        int raw_a=(int16_t)vram_read_word(v,(uint16_t)(hbase+hidx*4));
        int raw_b=(int16_t)vram_read_word(v,(uint16_t)(hbase+hidx*4+2));
        if(rr_road_profile==2 && wt==128 && base_a==0x4000 && base_b==0x6000 &&
           hmode==3 && !im2 && (x<0 || x>=w) && line<152 &&
           raw_a>=-1024 && raw_a<=-352 && raw_b>=512 && raw_b<=1088) {
            int ax=x-raw_a,bx=x-raw_b+1024;
            if(ax<320 || ax>=704){a_idx=0;a_op=0;a_hi=0;}
            if(bx<320){b_idx=0;b_op=0;b_hi=0;}
            else if(bx>=672)fetch_plane_pixel(v,&cache_b,base_b,wt,ht,
                640+(bx&7),line+vs_b,im2,&b_idx,&b_op,&b_hi);
        }
#endif'''
if modified.count(anchor)!=1:raise ValueError('Road strip sampling context mismatch')
modified=modified.replace(anchor,replacement)
# Extra columns are presentation only. Evaluate the authentic width first;
# preserve its pixels and hardware status flags, then draw the side margins.
# Wider sprite budgets must never change native dropout/collision or gameplay.
modified = modified.replace('int gvdp_render_scanline(GVDP *v, int line, uint8_t *out)',
                            'static int rr_render_scanline(GVDP *v, int line, uint8_t *out)')
modified += '''
/* RR_WIDE_CANON: extra drawing cannot become the hardware-status oracle. */
int gvdp_render_scanline(GVDP *v, int line, uint8_t *out) {
    int extra=s_ws_extra, canvas=s_ws_canvas_w;
    if(extra<=0 && canvas<=0)return rr_render_scanline(v,line,out);
    uint8_t native[GVDP_MAX_WIDTH];
    s_ws_extra=0;s_ws_canvas_w=0;
    int width=rr_render_scanline(v,line,native);
    uint8_t collision=v->sprite_collision,overflow=v->sprite_overflow;
    s_ws_extra=extra;s_ws_canvas_w=canvas;
    int total=rr_render_scanline(v,line,out);
    memcpy(out+(total-width)/2,native,(size_t)width);
    v->sprite_collision=collision;v->sprite_overflow=overflow;
    return total;
}
'''
save(relative, original, modified, "engine-hv-counter.patch")

relative = "runner/video/genesis_machine.c"
original = pristine(relative)
modified = original.replace("    uint32_t done = m->z80_cycle_debt;", "    uint32_t slice_master_start = s_z80_off; /* RR sub-scanline arbitration */\n    uint32_t done = m->z80_cycle_debt;")
modified = modified.replace("        s_z80_off = done * MASTER_PER_Z80;", "        s_z80_off = slice_master_start + done * MASTER_PER_Z80;")
start = modified.index("        /* Advance the recompiled 68K ~one scanline")
end = modified.index("        /* Deliver interrupts to the 68K", start)
modified = modified[:start] + '''        /* RR: arbitrate in eight bounded slices. The sound driver repeatedly
         * releases/reacquires BUSREQ inside one scanline; a whole-line slice
         * may always observe it stopped and starve the Z80 indefinitely. */
        for (unsigned slice = 0; slice < 8; ++slice) {
            s_z80_off = slice * MASTER_PER_LINE / 8u;
            glue_run_game_chunk((slice + 1u) * M68K_PER_LINE / 8u - slice * M68K_PER_LINE / 8u);
            glue_own_vint_service_latched(&m->vdp);
            step_z80(m, (slice + 1u) * Z80_PER_LINE / 8u - slice * Z80_PER_LINE / 8u);
        }

''' + modified[end:]
save(relative, original, modified, "engine-z80-arbitration.patch")
print("Applied NTSC HV progression and sub-scanline CPU/Z80 arbitration")
