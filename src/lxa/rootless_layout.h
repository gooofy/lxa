#ifndef HAVE_ROOTLESS_LAYOUT_H
#define HAVE_ROOTLESS_LAYOUT_H

#include <stdbool.h>

int rootless_layout_host_width(int screen_width,
                               int left_edge,
                               int logical_width,
                               bool widen_for_host_menu);

#endif
