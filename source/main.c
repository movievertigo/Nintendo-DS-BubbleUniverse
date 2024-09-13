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

s32 SinTable[SINTABLEENTRIES];
u16 ColourTable[ITERATIONS*CURVECOUNT/CURVESTEP];
u16 WidthTable[SCREENHEIGHT];

void ExpandSinTable()
{
    for (int i = 0; i < SINTABLEENTRIES/4; ++i)
    {
        SinTable[i] = SinTable[SINTABLEENTRIES/2 - i - 1] = compactsintable[i];
        SinTable[SINTABLEENTRIES/2 + i] = SinTable[SINTABLEENTRIES - i - 1] = -compactsintable[i];
    }
    for (int i = 0; i < SINTABLEENTRIES; ++i)
    {
        *((s16*)(SinTable+i)+1) = *(s16*)(SinTable+((i+SINTABLEENTRIES/4)%SINTABLEENTRIES));
    }
}

void InitColourTable()
{
    int colourIndex = 0;
    for (int i = 0; i < CURVECOUNT; i += CURVESTEP)
    {
        const s32 red = (i >> 3)|BIT(15);
        for (int j = 0; j < ITERATIONS; ++j)
        {
            const s32 green = j >> 3;
            const s32 blue = (62-(red+green))>>1;
            ColourTable[colourIndex++] = red + (green<<5) + (blue<<10);
        }
    }
}

void InitWidthTable()
{
    for (int y = 0; y < SCREENHEIGHT; ++y)
    {
        int widthSquared =  (SIZE+2)*(SIZE+2) - ((SCREENHEIGHT/2)-y)*((SCREENHEIGHT/2)-y);
        WidthTable[y] = widthSquared > 0 ? sqrt32(widthSquared)*4: 0;
    }
}

int main(void)
{
    ExpandSinTable();
    InitColourTable();
    InitWidthTable();

	videoSetMode(MODE_5_2D); 
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);

	consoleDemoInit();


	int bgId = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bool phase = false;
    u16* buffer1 = bgGetGfxPtr(bgId);
    u16* buffer2 = bgGetGfxPtr(bgId) + 256*256;

    int frame = 0;
    cpuStartTiming(0);
	while(true)
	{
		swiWaitForVBlank();
        phase = !phase;
        bgSetMapBase(bgId, phase ? 0 : 8);
        u16* buffer = phase ? buffer2 : buffer1;

        u16* row = buffer;
        for (u32 y = 0; y < SCREENHEIGHT; ++y)
        {
            if (WidthTable[y] > 0)
            {
                dmaFillWords(0, row + (SCREENWIDTH*2 - WidthTable[y])/4, WidthTable[y]);

            }
            row += SCREENWIDTH;
        }

        s32 time = frame * (1<<SINTABLEPOWER) / 1000;
        s32 ang1Start = time;
        s32 ang2Start = time;

        u16* screenCentre = buffer + (SCREENWIDTH + SCREENWIDTH*SCREENHEIGHT) / 2;

        u16* colourPtr = ColourTable;
        for (u32 i = 0; i < CURVECOUNT; i += CURVESTEP)
        {
            s32 x = 0, y = 0;
            for (u32 j = 0; j < ITERATIONS; ++j)
            {
                const s32 values1 = SinTable[(ang1Start + x)&(SINTABLEENTRIES-1)];
                const s32 values2 = SinTable[(ang2Start + y)&(SINTABLEENTRIES-1)];
                x = (s32)(s16)values1 + (s32)(s16)values2;
                y = (values1>>16) + (values2>>16);
                const s32 pX = (x * SCALEMUL) >> SINTABLEPOWER;
                const s32 pY = (y * SCALEMUL) >> SINTABLEPOWER;
                screenCentre[pY*SCREENWIDTH + pX] = *colourPtr++;
            }

            ang1Start += ANG1INC;
            ang2Start += ANG2INC;
        }

        ++frame;
        if ((frame%60) == 0)
        {
            iprintf("%ld\n", (u32)(cpuGetTiming()/33.514f/frame));
        }
	}
}
