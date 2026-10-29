#include "../frame_window.h"

#include <cassert>
#include <limits>

int main()
{
    FrameWindow window;
    for (int i = 0; i < 60; ++i)
        {
            const bool accepted = window.Add(1000.0 / 60.0);
            assert(accepted);
        }
    assert(window.Fps() == 60);

    for (int i = 0; i < 30; ++i)
        {
            const bool accepted = window.Add(1000.0 / 30.0);
            assert(accepted);
        }
    assert(window.Fps() == 30);

    assert(!window.Add(0.0));
    assert(!window.Add(-10.0));
    assert(!window.Add(1500.0));
    assert(!window.Add(std::numeric_limits<double>::quiet_NaN()));

    FrameWindow high;
    FrameWindow low;
    for (int i = 0; i < 200; ++i)
    {
        high.Add(1000.0 / 120.0);
        low.Add(1000.0 / 30.0);
    }
    assert(high.Fps() == 120 && low.Fps() == 30);

    FrameWindow bounded;
    for (int i = 0; i < 5000; ++i)
        bounded.Add(0.01);
    assert(bounded.Samples() == 1024);
}
