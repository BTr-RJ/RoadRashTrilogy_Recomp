#include "scene_capture.h"
#include <stdio.h>
#include <stdlib.h>
static FILE *trace;
static int initialized;
static unsigned records;
static unsigned word(const uint8_t *r,unsigned a){return ((unsigned)r[a]<<8)|r[a+1];}
static unsigned longword(const uint8_t *r,unsigned a){return (word(r,a)<<16)|word(r,a+2);}
static void close_trace(void){if(trace){fclose(trace);trace=NULL;}}
static FILE *jobs;
static int jobs_initialized;
static uint32_t job_pointer,job_sat;
static unsigned job_first;
static uint64_t job_tick;
static void close_jobs(void){if(jobs){fclose(jobs);jobs=NULL;}}
void rr_sprite_job_begin(const char *game,uint64_t tick,uint32_t job,
                         const uint8_t *ram,unsigned sat_count,unsigned sat_base){
    if(!jobs_initialized){jobs_initialized=1;const char *path=getenv("RR_JOB_TRACE");
        if(path&&*path){jobs=fopen(path,"wb");if(jobs){setvbuf(jobs,NULL,_IOFBF,65536);atexit(close_jobs);
            fputs("game,tick,job,x,y,scale,image,flags,first,sat,descriptor,pieces\n",jobs);}}}
    job_pointer=job;job_tick=tick;job_first=word(ram,sat_count);
    job_sat=longword(ram,sat_base);
    if(jobs&&job>=0xff0000&&job<=0xffffe0){unsigned j=job&65535;
        fprintf(jobs,"%s,%llu,%06X,%d,%d,%u,%u,%04X,%u,%06X,",game,(unsigned long long)tick,job,
            (int16_t)word(ram,j),(int16_t)word(ram,j+2),word(ram,j+4),word(ram,j+8),word(ram,j+10),job_first,job_sat);
        for(unsigned i=0;i<20;++i)fprintf(jobs,"%02X",ram[j+i]);
        fputc(',',jobs);
    }
}
void rr_sprite_job_end(const uint8_t *ram,unsigned sat_count){
    if(!jobs||job_pointer<0xff0000||job_pointer>0xffffe0)return;
    unsigned last=word(ram,sat_count);
    if(job_sat>=0xff0000&&job_sat<=0xfffd80&&last<=80&&last>=job_first)
        for(unsigned i=job_first;i<last;++i){unsigned at=(job_sat&65535)+i*8;
            for(unsigned k=0;k<8;++k)fprintf(jobs,"%02X",ram[at+k]);}
    fputc('\n',jobs);job_pointer=0;
}
void rr_scene_capture(const char *game,uint64_t tick,uint32_t actor,uint32_t job,
                      uint32_t camera,const uint8_t *ram,unsigned stride){
    if(!initialized){
        initialized=1;const char *path=getenv("RR_SCENE_TRACE");
        if(!path||!*path)return;
        trace=fopen(path,"wb");if(!trace)return;
        setvbuf(trace,NULL,_IOFBF,65536);atexit(close_trace);
        fputs("game,tick,actor,job,stride,world_z_raw,camera_z_raw,lateral_raw,native_x,native_y,native_scale,image,flags,extent,descriptor\n",trace);
    }
    if(!trace||records>=200000||actor<0xff0000||actor>0xffff80||job<0xff0000||job>0xffffc0)return;
    unsigned a=actor&65535,j=job&65535,c=camera&65535;
    fprintf(trace,"%s,%llu,%06X,%06X,%u,%u,%u,%d,%d,%d,%u,%u,%u,%u,",game,(unsigned long long)tick,
        actor,job,stride,longword(ram,a+4),longword(ram,c),(int16_t)word(ram,a+48),
        (int16_t)word(ram,j),(int16_t)word(ram,j+2),word(ram,j+4),word(ram,j+8),word(ram,j+10),word(ram,j+12));
    for(unsigned i=0;i<stride;++i)fprintf(trace,"%02X",ram[j+i]);
    fputc('\n',trace);++records;
}
