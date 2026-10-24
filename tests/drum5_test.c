#define main hostsim_main
#include "hostsim.c"
#undef main
static double bright(int32_t *b,int n){double d=0,s=0;for(int i=1;i<n;i++){d+=llabs(b[i]-b[i-1]);s+=llabs(b[i]);}return s?d/s:0;}
int main(void){
    track_t *t=&trk[0]; voice_t *v=&t->v[0]; vmod_t m={0};
    memset(trk,0,sizeof trk); host_tracks_init();
    uint32_t ei=0; for(uint32_t i=0;i<NENGINES;i++) if(ENGINES[i]==&ENG_DIGITAL) ei=i;
    const char*nm[]={"SYN","BD","SD","CH","OH","CYM","TOM"};
    int pre[]={0,8,9,10,11,12,13}; int notes[]={36,36,50,84,84,72,48};
    for(int c=0;c<7;c++){
        host_preset(t,ei,pre[c]);
        int note=notes[c];
        m.inc=pitch_inc(note*16); m.amp0=m.amp1=m.envq15=32767; m.shape=64<<8; m.pitch16=note*16;
        v->vel=96; v->note=note; v->gate=1; v->pitch16=note*16;
        digital_note_on(t,v);
        static int32_t buf[CTL*300]; memset(buf,0,sizeof buf);
        int32_t pk=0; double rms[6]={0};
        for(int k=0;k<300;k++) digital_render(t,v,buf+k*CTL,CTL,&m);
        for(int w=0;w<6;w++){ double s=0; for(int i=0;i<CTL*50;i++){double x=buf[w*CTL*50+i]; s+=x*x;} rms[w]=sqrt(s/(CTL*50)); }
        for(int i=0;i<CTL*300;i++) if(abs(buf[i])>pk) pk=abs(buf[i]);
        printf("%-4s mach %d peak %6d bright %.2f rms/50blk:",nm[c],t->p[P_E7],pk,bright(buf,CTL*100));
        for(int w=0;w<6;w++) printf(" %6.0f",rms[w]); printf("\n");
    }
    return 0;}
