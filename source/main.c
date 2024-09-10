#include <nds.h>
#include <stdio.h>
#include "sintable.h"

#define CURVECOUNT 256
#define CURVESTEP 4
#define ITERATIONS 256
#define SIZE 96
#define SCREENWIDTH 256
#define SCREENHEIGHT 192
#define PI 3.1415926535897932384626433832795
#define SINTABLEPOWER 14
#define SINTABLEENTRIES (1<<SINTABLEPOWER)
#define ANG1INC (s32)((CURVESTEP * SINTABLEENTRIES) / 235)
#define ANG2INC (s32)((CURVESTEP * SINTABLEENTRIES) / (2*PI))
#define SCALEMUL (s32)(SIZE*PI)

s16 SinTable[SINTABLEENTRIES + SINTABLEENTRIES/4];
s16* CosTable = SinTable + SINTABLEENTRIES/4;

void ExpandSinTable()
{
    for (int i = 0; i < SINTABLEENTRIES/4; ++i)
    {
        SinTable[i] = SinTable[SINTABLEENTRIES/2 - i - 1] = SinTable[SINTABLEENTRIES + i] = compactsintable[i];
        SinTable[SINTABLEENTRIES/2 + i] = SinTable[SINTABLEENTRIES - i - 1] = -compactsintable[i];
    }
}

int main(void)
{
    ExpandSinTable();

	videoSetMode(MODE_5_2D); 
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);

	consoleDemoInit();


	int bgId = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bool phase = true;
    u16* buffer1 = bgGetGfxPtr(bgId);
    u16* buffer2 = bgGetGfxPtr(bgId) + 256*256;
    u16* buffer = phase ? buffer2 : buffer1;

    int frame = 0;
    cpuStartTiming(0);
	while(true)
	{
        dmaFillWords(0, buffer, SCREENWIDTH*SCREENHEIGHT*2);

        s32 time = frame * (1<<SINTABLEPOWER) / 1000;
        s32 ang1Start = time;
        s32 ang2Start = time;

        for (int i = 0; i < CURVECOUNT; i += CURVESTEP)
        {
            s32 x = 0, y = 0;
            for (int j = 0; j < ITERATIONS; ++j)
            {
                const s32 sinoffset1 = (ang1Start + x)&(SINTABLEENTRIES-1);
                const s32 sinoffset2 = (ang2Start + y)&(SINTABLEENTRIES-1);
                x = SinTable[sinoffset1] + SinTable[sinoffset2];
                y = CosTable[sinoffset1] + CosTable[sinoffset2];
                const s32 pX = ((x * SCALEMUL) >> SINTABLEPOWER) + (SCREENWIDTH/2);
                const s32 pY = ((y * SCALEMUL) >> SINTABLEPOWER) + (SCREENHEIGHT/2);
                buffer[pY*SCREENWIDTH + pX] = 0xFFFF;
            }

            ang1Start += ANG1INC;
            ang2Start += ANG2INC;
        }

		swiWaitForVBlank();
        phase = !phase;
        bgSetMapBase(bgId, phase ? 0 : 8);
        buffer = phase ? buffer2 : buffer1;

        ++frame;
        if ((frame%60) == 0)
        {
            iprintf("%ld\n", cpuGetTiming()/33514/frame);
        }
	}
}
