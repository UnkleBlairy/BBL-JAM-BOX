/* Screen pictures from the emulator, for checking layouts without the browser.
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -Iweb/emu -o build/emu_shot tools/emu_shot.c -lm
 *   build/emu_shot tap:0 hold:9 enc:4,1 rel a.ppm   (tap:/hold: button number (panel.c B_*), rel, key:n, enc:role,steps,
 *   keyd:n (held) keyu, wav:F (2 s of the left channel, float32), playing? (prints), state?, tapms:button,ms, ms:n, any other word = save the screen there as PPM) */
#include "felucca_web.c"
static void run(double ms){ uint32_t n=(uint32_t)(ms*44.1), k; for(k=0;k<n;k+=128) web_render(128); }
static void tap(uint32_t b){ web_buttons(1u<<b); run(80); web_buttons(0); run(150); }
static void dump(const char *fn){ FILE *f=fopen(fn,"wb"); fprintf(f,"P6 240 240 255\n"); for(int i=0;i<240*240;i++){ uint16_t c=web_fb[i]; c=(uint16_t)((c>>8)|(c<<8)); fputc((c>>11)<<3,f); fputc(((c>>5)&63)<<2,f); fputc((c&31)<<3,f);} fclose(f);}
int main(int argc,char**argv){
  web_nor_erase();
  if(argc>1 && !strncmp(argv[1],"nor:",4)) { FILE *f=fopen(argv[1]+4,"rb"); if(f){ fread(nor,1,sizeof nor,f); fclose(f);} argv++; argc--; }   /* the flash of an earlier run */
  web_boot(); run(4000);
  for(int a=1;a<argc;a++){ char *s=argv[a];
    if(!strncmp(s,"tap:",4)) tap((uint32_t)atoi(s+4));
    else if(!strncmp(s,"ms:",3)) run(atof(s+3));
    else if(!strncmp(s,"hold:",5)) { web_buttons(1u<<atoi(s+5)); run(80);} 
    else if(!strncmp(s,"both:",5)) { int a2,b2; sscanf(s+5,"%d,%d",&a2,&b2); web_buttons(1u<<a2); run(120); web_buttons((1u<<a2)|(1u<<b2)); run(100); web_buttons(1u<<a2); run(80); web_buttons(0); run(150);}
    else if(!strcmp(s,"rel")) { web_buttons(0); run(150);} 
    else if(!strncmp(s,"key:",4)) { web_keys(1u<<atoi(s+4)); run(80); web_keys(0); run(100);} 
    else if(!strncmp(s,"keym:",5)) { web_keys((unsigned)strtoul(s+5,0,0)); run(10);}   /* the key bits as they are (several held) */
    else if(!strncmp(s,"keyd:",5)) { web_keys(1u<<atoi(s+5)); run(100);} 
    else if(!strcmp(s,"keyu")) { web_keys(0); run(100);} 
    else if(!strncmp(s,"wavx:",5)) { char fn[256]; int r,k; sscanf(s+5,"%255[^,],%d,%d",fn,&r,&k); FILE *f=fopen(fn,"wb"); for(int b=0;b<689;b++){ if(b%10==0 && k){ web_enc(r,k<0?-1:1); k+=k<0?1:-1; } run(0); web_render(128); fwrite(out_l,4,128,f);} fclose(f);}   /* 2 s, the encoder turned a step every 10 blocks */
    else if(!strncmp(s,"wav:",4)) { FILE *f=fopen(s+4,"wb"); for(int b=0;b<689;b++){ web_render(128); fwrite(out_l,4,128,f);} fclose(f);} 
    else if(!strcmp(s,"state?")) printf("sgrid=%u page=%u ly=%u lyt0=%u tm=%u hint=%u msg=%u grid=%u\n", ui.sgrid, ui.page, ui.ly, ui.ly_t0 & 15u, tm_dn, gb_hint, ui.msg_t, song.grid);
    else if(!strncmp(s,"tapms:",6)) { int b,d; sscanf(s+6,"%d,%d",&b,&d); web_buttons(1u<<b); run(d); web_buttons(0); run(120);} 
    else if(!strcmp(s,"playing?")) printf("playing=%u\n", web_playing());
    else if(!strncmp(s,"encx:",5)) { int r,n; sscanf(s+5,"%d,%d",&r,&n); for(int i=0;i<(n<0?-n:n);i++){ web_enc(r,n<0?-1:1); run(30);} }
    else if(!strcmp(s,"acid?")) { acid_t *a=acid_of(song.sel); uint32_t k=song.sel; printf("play=%u len=%u dir=%u sel=%u go=%u st=%u pul=%u nx=%u seq_n=%u note=%u hold=%u page=%s sgrid=%u grid=%u\n", a->play,a->stages,a->dir,acid_sel[k],acid_rt[k].go,acid_rt[k].st,acid_rt[k].pul,acid_rt[k].nx,trk[k].seq_n,trk[k].seq_notes[0],trk[k].seq_hold,PAGES[ui.page].title,ui.sgrid,song.grid); }
    else if(!strcmp(s,"vox?")) { track_t *t=TSEL; for(int i=0;i<NVOICE;i++) if(t->v[i].active) printf(" v%d n%u g%u st%u pc%d p%d", i, t->v[i].note, t->v[i].gate, t->v[i].stage, t->v[i].pitch_cur, t->v[i].pitch16); printf("\n"); }
    else if(!strcmp(s,"idx?")) { printf("mst=%u/%u fx=%u:", mst_step, mst_len(), fxl_pos); for(int i=0;i<NTRK;i++) printf(" %u(%u)", trk[i].seq_idx, trk[i].seq_pos); printf("\n"); }
    else if(!strcmp(s,"nt?")) { for(int i=0;i<NTRK;i++){ printf(" T%d:", i+1); for(int j=0;j<lastc[i];j++) printf("%u,", lastn[i][j]); } printf("\n"); }
    else if(!strcmp(s,"p?")) { for(int i=0;i<P_COUNT;i++) printf("%d:%d ",i,trk[song.sel].p[i]); printf("\n"); }   /* the selected track's values */
    else if(!strcmp(s,"mach?")) { for(int i=0;i<NTRK;i++) printf("%s%s ", i==song.sel?"*":"", MACHINES[trk_mach(&trk[i])].pill); printf(" pools:"); for(int q=0;q<GB_NPOOL;q++){ printf(" %s=",GB_POOL_NAME[q]); for(int k=0;k<16;k++) printf("%d",gbs_used(q,k)?1:0);} printf(" gbsv=%u\n",gbsv); }
    else if(!strncmp(s,"norsave:",8)) { FILE *f=fopen(s+8,"wb"); fwrite(nor,1,sizeof nor,f); fclose(f); }
    else if(!strcmp(s,"bpm?")) printf("bpm=%d sel=%u kitkeys=%u motion=%u\n", song.g[G_BPM], song.sel, (unsigned)({int c=0; for(int a=0;a<NTRK;a++) for(int b=0;b<16;b++) c+=kit[a][b].n; c;}), motion.count);
    else if(!strcmp(s,"all?")) { for(int k=0;k<NTRK;k++){ track_t *t=&trk[k]; printf("T%d:",k+1); for(int i=0;i<NSTEP;i++){ step_t *st=&t->step[i]; if(st->time==ST_NOTE&&st->n) printf(" %d(nd%d)", i, (int)step_nudge(st)); } printf("\n"); } }
    else if(!strcmp(s,"steps?")) { track_t *t=TSEL; printf("len=%d bank=%u qntz=%d idx=%u:", t->p[P_SLEN], ui.bank, t->p[P_SQNT], t->seq_idx); for(int i=0;i<NSTEP;i++){ step_t *st=&t->step[i]; if(st->time==ST_NOTE&&st->n) printf(" %d[n%u nd%d tc%u]", i, st->n, (int)step_nudge(st), gb_tc(st)); } printf("\n"); }
    else if(!strcmp(s,"m1?")) printf("m1 src=%d dst=%d amt=%d tm_dn=%u\n", TSEL->p[P_M1SRC], TSEL->p[P_M1DST], TSEL->p[P_M1AMT], tm_dn);
    else if(!strcmp(s,"fx?")) printf("sel=%u fxt=%u page=%s amp=%d L1=%d/%d/%d/%d L2=%d/%d/%d/%d dly=%d trig=%llx len=%d lk0=%x v0amp=%d ov=%u pos=%u\n", song.sel, fxl_sel, PAGES[ui.page].title, song.g[G_AMP], song.g[G_F1RATE], song.g[G_F1WAVE], song.g[G_F1DEST], song.g[G_F1AMT], song.g[G_F2RATE], song.g[G_F2WAVE], song.g[G_F2DEST], song.g[G_F2AMT], TSEL->p[P_DLY], (unsigned long long)fxl.trig, fxl.len, fxl.lk[0], fxl.v[0][11], mg_ov_n, fxl_pos);
    else if(!strncmp(s,"enc:",4)) { int r,n; sscanf(s+4,"%d,%d",&r,&n); web_enc(r,n); run(100);} 
    else dump(s);
  }
  return 0; }
