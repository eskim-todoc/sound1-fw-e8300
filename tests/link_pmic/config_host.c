/* Actual configuration module with a RAM file and explicit I/O faults. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ff.h>
#include <tdc_shm.h>
tdc_test_shm_t cfx_cm3_sharedMemoryAll;
static unsigned char s_tdc_disk[4096];
static int s_tdc_exists, s_tdc_fault, s_tdc_opens, s_tdc_writes, s_tdc_seeks, s_tdc_closes;
FRESULT f_open(FIL *f,const char *name,unsigned mode) {
    assert(!strcmp(name,"LINKPARM.TXT")); ++s_tdc_opens;
    if(s_tdc_fault==1 || (s_tdc_fault==10 && s_tdc_opens==2)) return FR_DISK_ERR;
    if(!s_tdc_exists && !(mode&FA_OPEN_ALWAYS)) return FR_NO_FILE;
    s_tdc_exists=1; f->position=0; return FR_OK;
}
FRESULT f_read(FIL *f,void *out,UINT n,UINT *got) {
    assert(f->position+n<=sizeof(s_tdc_disk));
    if(s_tdc_fault==8)return FR_DISK_ERR;
    memcpy(out,s_tdc_disk+f->position,n); *got=s_tdc_fault==9?n-1:n;
    if(s_tdc_fault==11)((unsigned char*)out)[4]^=1;
    f->position+=*got;return FR_OK;
}
FRESULT f_write(FIL *f,const void *in,UINT n,UINT *got) {
    ++s_tdc_writes;assert(n==40 && f->position==0);
    if(s_tdc_fault==4)return FR_DISK_ERR;
    *got=s_tdc_fault==5?n-1:n;memcpy(s_tdc_disk,in,*got);f->position+=*got;return FR_OK;
}
FRESULT f_lseek(FIL *f,unsigned offset) {
    ++s_tdc_seeks;if(s_tdc_fault==2 || (s_tdc_fault==12 && s_tdc_seeks==2))return FR_DISK_ERR;
    assert(offset<=sizeof(s_tdc_disk));f->position=offset;return FR_OK;
}
unsigned f_tell(FIL *f){return s_tdc_fault==3?f->position+1:f->position;}
FRESULT f_sync(FIL *f){(void)f;return s_tdc_fault==6?FR_DISK_ERR:FR_OK;}
FRESULT f_close(FIL *f){(void)f;++s_tdc_closes;return s_tdc_fault==7?FR_DISK_ERR:FR_OK;}
#include "../../src/2__cm3/source/isd/tdc_link_config.c"
static void word(unsigned offset,unsigned value) {
    for(unsigned i=0;i<4;i++)s_tdc_disk[offset+i]=(unsigned char)(value>>(8*i));
}
static void record(unsigned max) {
    word(0,0x11223344);word(4,120);word(8,7);word(12,180);word(16,9);
    word(20,3);word(24,1);word(28,1);word(32,max);word(36,0x44332211);s_tdc_exists=1;
}
int main(int argc,char **argv) {
    int which=argc>1?atoi(argv[1]):0;
    if(which>=1 && which<=12) {
        tdc_link_config_t before=*tdc_link_config_get();
        cfx_cm3_sharedMemoryAll.tdc_backtel_period_100ms=3;s_tdc_fault=which;
        assert(tdc_link_config_set(2,8)==TDC_LINK_CONFIG_IO_ERROR);
        assert(!memcmp(&before,tdc_link_config_get(),sizeof(before)));
        assert(cfx_cm3_sharedMemoryAll.tdc_backtel_period_100ms==3);
        if(which==7){int opens=s_tdc_opens;s_tdc_fault=0;assert(tdc_link_config_set(2,9)==-1);assert(opens==s_tdc_opens);}
    } else if(which==20 || which==21) {
        record(which==21?0:205);assert(tdc_link_config_init()==0);
        const tdc_link_config_t *c=tdc_link_config_get();
        assert(c->min_level==120 && c->mapping_min_level==180 && c->step_up==9);
        assert(c->coin_count==3 && c->mode==1 && c->accel==1 && c->period_100ms==7);
        assert(c->max_level==(which==21?214u:205u));assert(s_tdc_writes==0);
    } else if(which==22) {
        record(180);unsigned char before[4096];memcpy(before,s_tdc_disk,4096);
        assert(tdc_link_config_init()==-2);assert(tdc_link_config_get()->min_level==140);
        assert(!memcmp(before,s_tdc_disk,4096) && s_tdc_writes==0);
    } else {
        assert(tdc_link_config_init()==0);assert(s_tdc_writes==1 && s_tdc_disk[0]==0x44 && s_tdc_disk[39]==0x44);
        const tdc_link_config_t *c=tdc_link_config_get();
        assert(c->min_level==140 && c->mapping_min_level==172 && c->period_100ms==3 && c->max_level==214);
        const unsigned invalid[][2]={{3,111},{3,214},{4,214},{16,172},{16,215},{2,0},{2,256},{5,0},{5,41},{7,101},{9,2},{10,2},{17,0},{99,0}};
        int writes=s_tdc_writes;
        for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)assert(tdc_link_config_set(invalid[i][0],invalid[i][1])==-2);
        assert(writes==s_tdc_writes);
        assert(tdc_link_config_set(3,112)==0 && c->min_level==112);
        assert(tdc_link_config_set(2,255)==0 && cfx_cm3_sharedMemoryAll.tdc_backtel_period_100ms==255);
        assert(tdc_link_config_set(7,0)==0 && c->coin_count==0);
        assert(!tdc_link_config_valid(NULL));
    }
    printf("CONFIG_HOST_PASS scenario=%d opens=%d writes=%d closes=%d\n",which,s_tdc_opens,s_tdc_writes,s_tdc_closes);
    return 0;
}
