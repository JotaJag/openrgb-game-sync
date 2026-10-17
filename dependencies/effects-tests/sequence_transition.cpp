#include <QApplication>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "Effects/Sequence/Sequence.h"

// Capture the real effect's output without accessing any physical LED device.
static RGBColor captured;
static unsigned int writes;
void ControllerZone::SetAllZoneLEDs(RGBColor color, int, int, int)
{
    captured = color;
    ++writes;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    bool passed = true;
    ControllerZone zone(nullptr, 0, false, 100, true, false);
    for (unsigned int fade : {1u, 5u})
    {
        Sequence effect;
        effect.SetFPS(60);
        effect.SetSpeed(4);
        effect.SetSlider2Val(fade);
        effect.LoadCustomSettings({{"colors", {ToRGBColor(255, 0, 0), ToRGBColor(0, 0, 255)}}});
        RGBColor previous = 0;
        unsigned int max_jump = 0;
        const unsigned int frames = 60 * (8 + 2 * fade);
        for (unsigned int frame = 0; frame < frames; ++frame)
        {
            writes = 0;
            effect.StepEffect({&zone});
            if (writes != 1) return 2;
            if (frame)
            {
                for (int shift : {0, 8, 16})
                {
                    const unsigned int jump = std::abs(int((captured >> shift) & 255) - int((previous >> shift) & 255));
                    max_jump = std::max(max_jump, jump);
                }
            }
            previous = captured;
        }
        std::cout << "fade=" << fade << ": maximum channel jump=" << max_jump << "/255\n";
        passed &= max_jump <= 10;
    }
    std::cout << (passed ? "PASS: continuous transitions\n" : "FAIL: transition flash detected\n");
    return passed ? 0 : 1;
}
