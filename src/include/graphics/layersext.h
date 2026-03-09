#ifndef GRAPHICS_LAYERSEXT_H
#define GRAPHICS_LAYERSEXT_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef GRAPHICS_LAYERS_H
#include <graphics/layers.h>
#endif

#ifndef GRAPHICS_CLIP_H
#include <graphics/clip.h>
#endif

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

/* Tags for CreateLayerTagList */
#define LA_ShapeRegion  (TAG_USER + 34)
#define LA_ShapeHook    (TAG_USER + 35)
#define LA_InFrontOf    (TAG_USER + 36)
#define LA_Hidden       (TAG_USER + 41)

#define LA_Dummy        (TAG_USER + 1024)
#define LA_BackfillHook (LA_Dummy + 1)
#define LA_TransRegion  (LA_Dummy + 2)
#define LA_TransHook    (LA_Dummy + 3)
#define LA_WindowPtr    (LA_Dummy + 4)
#define LA_SuperBitMap  (LA_Dummy + 5)

#define LA_AROS         (TAG_USER + 1234)
#define LA_Behind       (LA_AROS + 3)
#define LA_ChildOf      (LA_AROS + 4)

#ifndef LAYERS_BASE_NAME
#define LAYERS_BASE_NAME LayersBase
#endif

#define CreateLayerTagList(li, bm, x0, y0, x1, y1, flags, tagList) \
LP8(246, struct Layer *, CreateLayerTagList, struct Layer_Info *, li, a0, \
    struct BitMap *, bm, a1, LONG, x0, d0, LONG, y0, d1, LONG, x1, d2, \
    LONG, y1, d3, LONG, flags, d4, struct TagItem *, tagList, a2, \
    , LAYERS_BASE_NAME)

#define AddLayerInfoTag(li, tag, data) \
LP3(252, BOOL, AddLayerInfoTag, struct Layer_Info *, li, a0, Tag, tag, d0, \
    ULONG, data, d1, , LAYERS_BASE_NAME)

#endif
