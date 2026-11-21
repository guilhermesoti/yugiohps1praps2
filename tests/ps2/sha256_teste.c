/* Teste do SHA-256 em pedaços (src/pc/guest/sha256.c) no PC:
 *   gcc -std=c99 -Wall -fsanitize=address,undefined -I. -o /tmp/s tests/ps2/sha256_teste.c src/pc/guest/sha256.c && /tmp/s */
#include "src/pc/guest/sha256.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void){ static unsigned char b[100000]; for(int i=0;i<100000;i++) b[i]=(unsigned char)(i*131+7);
 size_t lens[]={0,1,55,56,63,64,65,119,120,4096,99999,100000};
 for(unsigned k=0;k<sizeof lens/sizeof*lens;k++){ unsigned char a[32],c[32]; Sha256(b,lens[k],a);
  Sha256Ctx x; Sha256Begin(&x); size_t d=0; while(d<lens[k]){ size_t t=1+rand()%5000; if(t>lens[k]-d)t=lens[k]-d; Sha256Add(&x,b+d,t); d+=t;} Sha256End(&x,c);
  if(memcmp(a,c,32)){printf("FALHOU %zu\n",lens[k]);return 1;} }
 unsigned char e[32]; Sha256("abc",3,e); for(int i=0;i<4;i++)printf("%02x",e[i]); printf(" (abc deve comecar ba7816bf)\nok\n"); return 0;}
