#include <nds.h>
#include <stdio.h>

const int CurveCount = 256;
const int CurveStep = 4;
const int Iterations = 256;
const float Pi = 3.1415926535;
const float Ang1inc = CurveStep * 2 * Pi / 235;
const float Ang2inc = CurveStep;

int main(void)
{
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
        dmaFillWords(0, buffer, 256*196*2);

        float time = (float)frame / 1000;
        float ang1Start = time;
        float ang2Start = time;

        for (int i = 0; i < CurveCount; i += CurveStep)
        {
            float x = 0, y = 0;
            for (int j = 0; j < Iterations; ++j)
            {
                float a = ang1Start + x;
                float b = ang2Start + y;
                x = sinLerp(a * DEGREES_IN_CIRCLE / (2*Pi)) / (float)(1<<12) + sinLerp(b * DEGREES_IN_CIRCLE / (2*Pi)) / (float)(1<<12);
                y = cosLerp(a * DEGREES_IN_CIRCLE / (2*Pi)) / (float)(1<<12) + cosLerp(b * DEGREES_IN_CIRCLE / (2*Pi)) / (float)(1<<12);
                int pX = (int)(x * 48) + 128;
                int pY = (int)(y * 48) + 98;
                buffer[pY * 256 + pX] = 0xFFFF;
            }

            ang1Start += Ang1inc;
            ang2Start += Ang2inc;
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
