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

#define UNROLLCOUNT 4

bool trails = false;
s32 speed = 8;
s32 oldSpeed = 0;
bool justReset = false;
s32 scaleMul = SCALEMUL;
s32 xPan = 0;
s32 yPan = 0;


s32 SinTable[SINTABLEENTRIES];
DTCM_BSS u16 ColourTable[ITERATIONS*CURVECOUNT/CURVESTEP/UNROLLCOUNT];

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
        for (int j = 0; j < ITERATIONS; j += UNROLLCOUNT)
        {
            const s32 green = j >> 3;
            const s32 blue = (62-(red+green))>>1;
            ColourTable[colourIndex++] = red + (green<<5) + (blue<<10);
        }
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

#define UNROLL \
    values1 = SinTable[(ang1Start + x)&(SINTABLEENTRIES-1)]; \
    values2 = SinTable[(ang2Start + y)&(SINTABLEENTRIES-1)]; \
    x = (s32)(s16)values1 + (s32)(s16)values2; \
    y = (values1>>16) + (values2>>16); \
    pX = ((x * scaleMul) >> SINTABLEPOWER) + xPan; \
    pY = ((y * scaleMul) >> SINTABLEPOWER) + yPan; \
    if (pX >= -(SCREENWIDTH>>1) && pY >= -(SCREENHEIGHT>>1) && pX < (SCREENWIDTH>>1) && pY < (SCREENHEIGHT>>1)) \
    { \
        screenCentre[pY*SCREENWIDTH + pX] = *colourPtr; \
    } \

int main(void)
{
    ExpandSinTable();
    InitColourTable();
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
        "     D-pad : Move around\n"
        "       A/B : Zoom in/out\n"
        "       A+B : Reset view\n"
        "       X/Y : Speed inc/dec\n"
        "       L/R : Fast move + zoom\n"
        "     Start : Pause/Unpause\n"
        "    Select : Toggle trails\n"
        "\n\n"
        " Particles : %d\n"
        "     Speed : %ld\n"
        "\n"
        "     Frame :\n"
        "     Vsync :\n"
        "\n\n"
        "        By Movie Vertigo\n"
        "    youtube.com/movievertigo\n"
        "    twitter.com/movievertigo",
        ITERATIONS * CURVECOUNT / CURVESTEP,
        speed
    );
    speedCursorPos = textBase + 32*14 + 13;
    statsCursorPos = textBase + 32*16 + 13;
    vsyncCursorPos = textBase + 32*17 + 13;

	int bgId = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bool phase = false;
    u16* buffer1 = bgGetGfxPtr(bgId);
    u16* buffer2 = bgGetGfxPtr(bgId) + 256*256;

    s32 animationTime = 0;
//    animationTime = 21989; speed = 0; // Slow frame test
    u32 startTime = 0;
    cpuStartTiming(0);
	while(true)
	{
        phase = !phase;
        bgSetMapBase(bgId, phase | trails ? 0 : 8);
        u16* buffer = phase & !trails ? buffer2 : buffer1;

        if (!trails)
        {
            dmaFillWords(0, buffer, SCREENWIDTH*SCREENHEIGHT*2);
        }

        s32 ang1Start = animationTime;
        s32 ang2Start = animationTime;

        u16* screenCentre = buffer + ((SCREENWIDTH + SCREENWIDTH*SCREENHEIGHT)>>1);

        u16* colourPtr = ColourTable;
        for (u32 i = 0; i < CURVECOUNT; i += CURVESTEP)
        {
            s32 x = 0, y = 0;
            for (u32 j = 0; j < ITERATIONS/UNROLLCOUNT; ++j)
            {
                s32 values1, values2, pX, pY;

                UNROLL; UNROLL; UNROLL; UNROLL;

                colourPtr++;
            }

            ang1Start += ANG1INC;
            ang2Start += ANG2INC;
        }

        u32 usec = timerTicks2usec(cpuGetTiming()-startTime);
		swiWaitForVBlank();
        u32 usecvsync = timerTicks2usec(cpuGetTiming()-startTime);
        startTime = cpuGetTiming();

        textCursor = speedCursorPos;
        printNumber(speed); PRINTCHAR(' ');
        textCursor = statsCursorPos;
        printNumber(usec/1000); PRINTCHAR('m'); PRINTCHAR('s'); PRINTCHAR(' ');
        printNumber((1000000+(usec>>1))/usec); PRINTCHAR('f'); PRINTCHAR('p'); PRINTCHAR('s');
        textCursor = vsyncCursorPos;
        printNumber(usecvsync/1000); PRINTCHAR('m'); PRINTCHAR('s'); PRINTCHAR(' ');
        printNumber((1000000+(usecvsync>>1))/usecvsync); PRINTCHAR('f'); PRINTCHAR('p'); PRINTCHAR('s');

		scanKeys();
		int pressed = keysHeld();
        int modSpeed = (pressed & KEY_L ? 4 : 1) + (pressed & KEY_R ? 8 : 0);
		if(pressed & KEY_LEFT) xPan += modSpeed;
		if(pressed & KEY_RIGHT) xPan -= modSpeed;
		if(pressed & KEY_UP) yPan += modSpeed;
		if(pressed & KEY_DOWN) yPan -= modSpeed;
		if((pressed & KEY_A) && !justReset) scaleMul += modSpeed;
		if((pressed & KEY_B) && !justReset) scaleMul -= modSpeed;
        if(!(pressed & (KEY_A|KEY_B))) justReset = false;
		pressed = keysDownRepeat();
        if((pressed & KEY_A) && (pressed & KEY_B)) { scaleMul = SCALEMUL; xPan = yPan = 0; justReset = true; }
		if(pressed & KEY_X) speed += modSpeed;
		if(pressed & KEY_Y) speed -= modSpeed;
        pressed = keysDown();
        if(pressed & KEY_START)
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
        if(pressed & KEY_SELECT)
        {
            trails = !trails;
            phase = false;
        }

        animationTime += speed;
	}
}
