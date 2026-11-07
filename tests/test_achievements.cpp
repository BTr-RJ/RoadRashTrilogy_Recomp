#include "md_memory.h"
#include "ra_service.h"
#include "rc_runtime.h"
#include "rc_hash.h"
#include "rc_consoles.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); std::exit(1); } } while(0)
static std::array<uint8_t,65536> memory;
static unsigned awards;
static uint32_t read_ram(uint32_t address,uint8_t *out,uint32_t bytes,void*) {
    return rr_md_ra_read(memory.data(),address,out,bytes);
}
static uint32_t peek(uint32_t address,uint32_t bytes,void*) {
    uint8_t out[4]{}; CHECK(bytes<=4);
    CHECK(rr_md_ra_read(memory.data(),address,out,bytes)==bytes);
    uint32_t value=0;for(unsigned i=0;i<bytes;++i)value|=uint32_t(out[i])<<(8*i);
    return value;
}
static void event(const rc_runtime_event_t *e) {
    if(e->type==RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED) ++awards;
}
int main() {
    memory[0]=0x12;memory[1]=0x34;memory[2]=0x56;memory[3]=0x78;
    uint8_t out[8]{};
    CHECK(rr_md_ra_read(memory.data(),0,out,4)==4);
    CHECK(out[0]==0x34&&out[1]==0x12&&out[2]==0x78&&out[3]==0x56);
    CHECK(peek(0,2,nullptr)==0x1234);
    CHECK(rr_md_ra_read(memory.data(),1,out,2)==2&&out[0]==0x12&&out[1]==0x78);
    CHECK(rr_md_ra_read(memory.data(),65535,out,8)==1);
    CHECK(rr_md_ra_read(memory.data(),65536,out,8)==0);
    CHECK(rr_md_ra_read(memory.data(),0xffffffffu,out,8)==0);
    CHECK(rr_md_ra_read(nullptr,0,out,1)==0);
    rc_runtime_t runtime;rc_runtime_init(&runtime);
    CHECK(rc_runtime_activate_achievement(&runtime,1,"0xH0000=5",nullptr,0)==RC_OK);
    rc_runtime_do_frame(&runtime,event,read_ram,nullptr,nullptr); // false arms trigger
    CHECK(awards==0);memory[1]=5;
    rc_runtime_do_frame(&runtime,event,read_ram,nullptr,nullptr);CHECK(awards==1);
    rc_runtime_do_frame(&runtime,event,read_ram,nullptr,nullptr);CHECK(awards==1);
    rc_runtime_destroy(&runtime);
    const uint8_t synthetic[]={0,1,2,3,4,5,6,7};char hash[33]{};
    rc_hash_iterator_t it{};rc_hash_initialize_iterator(&it,"test.md",synthetic,sizeof synthetic);
    CHECK(rc_hash_generate(hash,RC_CONSOLE_MEGA_DRIVE,&it));
    CHECK(std::strcmp(hash,"3677509751ccf61539174d2b9635a7bf")==0);
    rc_hash_destroy_iterator(&it);
    RRRa *service=rr_ra_create(L"build/validation/no-such-credentials.auth",nullptr,nullptr);
    CHECK(service&&!rr_ra_logged_in(service)&&!rr_ra_loaded(service));
    CHECK(rr_ra_restore_login(service)==0);rr_ra_bind_rom(service,synthetic,sizeof synthetic);
    for(int i=0;i<3;++i)rr_ra_update(service,1);
    CHECK(!rr_ra_loaded(service));rr_ra_destroy(service);
    std::puts("Achievements: byte lanes, boundaries, trigger, one-shot award, MD5, offline lifecycle passed");
}
