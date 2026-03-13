#include "rootless_layout.h"

int rootless_layout_host_width(int screen_width,
                               int left_edge,
                               int logical_width,
                               bool widen_for_host_menu)
{
    int available_width;

    if (!widen_for_host_menu || logical_width <= 0 || screen_width <= 0)
    {
        return logical_width;
    }

    if (left_edge < 0)
    {
        left_edge = 0;
    }

    if (left_edge >= screen_width)
    {
        return logical_width;
    }

    available_width = screen_width - left_edge;
    if (available_width > logical_width)
    {
        return available_width;
    }

    return logical_width;
}
