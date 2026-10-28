/* Screen pictures from the emulator, for checking layouts without the browser.
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -Iweb/emu -o build/emu_shot tools/emu_shot.c -lm
 *   build/emu_shot tap:0 hold:9 enc:4,1 rel a.ppm   (tap:/hold: button number (panel.c B_*), rel, key:n, enc:role,steps,
 *   keyd:n (held) keyu, wav:F (2 s of the left channel, float32), playing? (prints), ms:n, any other word = save the screen there as PPM) */
#include "felucca_web.c"
static void run(double ms){ uint32_t n=(uint32_t)(ms*44.1), k; for(k=0;k<n;k+=128) web_render(128); }
static void tap(uint32_t b){ web_buttons(1u<<b); run(80); web_buttons(0); run(150); }
static void dump(const char *fn){ FILE *f=fopen(fn,"wb"); fprintf(f,"P6 240 240 255\n"); for(int i=0;i<240*240;i++){ uint16_t c=web_fb[i]; c=(uint16_t)((c>>8)|(c<<8)); fputc((c>>11)<<3,f); fputc(((c>>5)&63)<<2,f); fputc((c&31)<<3,f);} fclose(f);}
int main(int argc,char**argv){
  web_nor_erase(); web_boot(); run(4000);
  for(int a=1;a<argc;a++){ char *s=argv[a];
    if(!strncmp(s,"tap:",4)) tap((uint32_t)atoi(s+4));
    else if(!strncmp(s,"ms:",3)) run(atof(s+3));
    else if(!strncmp(s,"hold:",5)) { web_buttons(1u<<atoi(s+5)); run(80);} 
    else if(!strcmp(s,"rel")) { web_buttons(0); run(150);} 
    else if(!strncmp(s,"key:",4)) { web_keys(1u<<atoi(s+4)); run(80); web_keys(0); run(100);} 
    else if(!strncmp(s,"keyd:",5)) { web_keys(1u<<atoi(s+5)); run(100);} 
    else if(!strcmp(s,"keyu")) { web_keys(0); run(100);} 
    else if(!strncmp(s,"wav:",4)) { FILE *f=fopen(s+4,"wb"); for(int b=0;b<689;b++){ web_render(128); fwrite(out_l,4,128,f);} fclose(f);} 
    else if(!strcmp(s,"playing?")) printf("playing=%u\n", web_playing());
    else if(!strncmp(s,"enc:",4)) { int r,n; sscanf(s+4,"%d,%d",&r,&n); web_enc(r,n); run(100);} 
    else dump(s);
  }
  return 0; }
