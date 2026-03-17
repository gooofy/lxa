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

int rootless_layout_host_height(int top_edge,
                                int logical_height)
{
    if (logical_height <= 0)
    {
        return logical_height;
    }

    if (top_edge > 0)
    {
        return logical_height + top_edge;
    }

    return logical_height;
}

int rootless_layout_host_origin_y(int top_edge)
{
    if (top_edge > 0)
    {
        return 0;
    }

    return top_edge;
}

void rootless_layout_screen_coords(int window_left,
                                    int window_top,
                                    int local_x,
                                    int local_y,
                                    int *screen_x,
                                    int *screen_y)
{
    int host_origin_y = rootless_layout_host_origin_y(window_top);

    if (screen_x)
    {
        *screen_x = window_left + local_x;
    }

    if (screen_y)
    {
        *screen_y = host_origin_y + local_y;
    }
}
