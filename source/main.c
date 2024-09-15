#include <nds.h>
#include <stdio.h>
#include "sintable.h"

#include "font.h"

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

#define PRINTCHAR(x) *textCursor++ = (x)-32
#define PRINTDIGIT(x) *textCursor++ = (x)+16

bool trails = false;
s32 speed = 32;
s32 oldSpeed = 0;

s32 SinTable[SINTABLEENTRIES];
u16 ColourTable[ITERATIONS*CURVECOUNT/CURVESTEP];
u16 WidthTable[SCREENHEIGHT];

PrintConsole* console;
u16* textBase;
u16* textCursor;
u16* speedCursorPos;
u16* statsCursorPos;
u16* vsyncCursorPos;

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

void InitConsole()
{
	videoSetModeSub(MODE_0_2D);	
	vramSetBankC(VRAM_C_SUB_BG); 

	console = consoleInit(0,0, BgType_Text8bpp, BgSize_T_256x256, 8, 0, false, false);

	ConsoleFont font;
	font.gfx = (u16*)fontTiles;
	font.pal = (u16*)fontPal;
	font.numChars = 256-32;
	font.numColors = fontPalLen / 2;
	font.bpp = 8;
	font.asciiOffset = 32;
	font.convertSingleColor = false;
	
	consoleSetFont(console, &font);

    textBase = console->fontBgMap;
    textCursor = textBase;
}

void printNumber(s32 number)
{
    if (number < 0)
    {
        PRINTCHAR('-');
        number = -number;
    }

    u32 divisor = 1;
    while (number >= divisor*10)
    {
        divisor *= 10;
    }
    
    while (divisor > 0)
    {
        PRINTDIGIT(number/divisor);
        number %= divisor;
        divisor /= 10;
    }
}

int main(void)
{
    ExpandSinTable();
    InitColourTable();
    InitWidthTable();
    InitConsole();

	videoSetMode(MODE_5_2D); 
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);

    keysSetRepeat(16, 1);

    for (int i = 128; i < 224; ++i)
    {
    	iprintf("%c",i);
    }

	iprintf(
        "\n"
        "     D-pad : Speed control\n"
        "         A : Pause/Unpause\n"
        "         X : Toggle trails\n"
        "\n\n"
        " Particles : %d\n"
        "     Speed : %ld\n"
        "\n"
        "     Frame :\n"
        "     Vsync :\n"
        "\n\n\n\n\n\n"
        "        By Movie Vertigo\n"
        "    youtube.com/movievertigo\n"
        "    twitter.com/movievertigo",
        ITERATIONS * CURVECOUNT / CURVESTEP,
        speed
    );
    speedCursorPos = textBase + 32*10  + 13;
    statsCursorPos = textBase + 32*12 + 13;
    vsyncCursorPos = textBase + 32*13 + 13;

	int bgId = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bool phase = false;
    u16* buffer1 = bgGetGfxPtr(bgId);
    u16* buffer2 = bgGetGfxPtr(bgId) + 256*256;

    s32 animationTime = 0;
    u32 startTime = 0;
    cpuStartTiming(0);
	while(true)
	{
        phase = !phase;
        bgSetMapBase(bgId, phase | trails ? 0 : 8);
        u16* buffer = phase & !trails ? buffer2 : buffer1;

        if (!trails)
        {
            u16* row = buffer + (SCREENWIDTH>>1);
            for (u32 y = 0; y < SCREENHEIGHT; ++y)
            {
                dmaFillWords(0, row - (WidthTable[y]>>2), WidthTable[y]);
                row += SCREENWIDTH;
            }
        }

        s32 ang1Start = animationTime;
        s32 ang2Start = animationTime;

        u16* screenCentre = buffer + ((SCREENWIDTH + SCREENWIDTH*SCREENHEIGHT)>>1);

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

        u32 usec = timerTicks2usec(cpuGetTiming()-startTime);
		swiWaitForVBlank();
        u32 usecvsync = timerTicks2usec(cpuGetTiming()-startTime);
        startTime = cpuGetTiming();

        textCursor = speedCursorPos;
        printNumber(speed);
        PRINTCHAR(' ');
        textCursor = statsCursorPos;
        printNumber(usec/1000); PRINTCHAR('m'); PRINTCHAR('s'); PRINTCHAR(' ');
        printNumber((1000000+(usec>>1))/usec); PRINTCHAR('f'); PRINTCHAR('p'); PRINTCHAR('s');
        textCursor = vsyncCursorPos;
        printNumber(usecvsync/1000); PRINTCHAR('m'); PRINTCHAR('s'); PRINTCHAR(' ');
        printNumber((1000000+(usecvsync>>1))/usecvsync); PRINTCHAR('f'); PRINTCHAR('p'); PRINTCHAR('s');

		scanKeys();
		int pressed = keysDownRepeat();
		if(pressed & (KEY_RIGHT|KEY_UP)) speed += 1;
		if(pressed & (KEY_LEFT|KEY_DOWN)) speed -= 1;
        pressed = keysDown();
        if(pressed & KEY_A)
        {
            if (speed)
            {
                oldSpeed = speed;
                speed = 0;
            }
            else
            {
                speed = oldSpeed;
            }
        }
        if(pressed & KEY_X)
        {
            trails = !trails;
            phase = false;
        }

        animationTime += speed;
	}
}
