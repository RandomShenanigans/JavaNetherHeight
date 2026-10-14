#include "dimension_hook.h"

#include "hook.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#define BLC_ALIGNOF(T) __alignof(T)
#else
#define BLC_ALIGNOF(T) _Alignof(T)
#endif

typedef struct DimensionArguments {
    int32_t dim_id;
    DimensionHeightRange height_range;
} DimensionArguments;

static HookInline dimension_hook;

static bool height_range_valid(DimensionHeightRange range)
{
    int16_t hi = range.max;
    int16_t lo = range.min;

    return lo >= -2048 && lo <= 2048 &&
           hi >= -2048 && hi <= 2048 &&
           lo % 16 == 0 && hi % 16 == 0 &&
           hi > lo;
}

static DimensionHeightRange apply_direct_limits(int32_t dim_id, DimensionHeightRange range)
{
    if (dim_id == 1) {
        range.min = 0;
        range.max = 256;
    }
    return range;
}

static bool dimension_arguments_valid(const DimensionArguments *args)
{
    return args->dim_id >= 0 && args->dim_id <= 2 &&
           height_range_valid(args->height_range);
}

static DimensionArguments *find_dimension_arguments(uint8_t *region, size_t region_size, size_t offset)
{
    const size_t k_size = sizeof(DimensionArguments);
    const uintptr_t k_align_mask = (uintptr_t)BLC_ALIGNOF(DimensionArguments) - 1u;

    if (!region || region_size < k_size || offset > region_size - k_size)
        return NULL;

    for (size_t i = offset; i <= region_size - k_size; ++i) {
        uint8_t *candidate_ptr = region + i;
        DimensionArguments candidate;

        if (((uintptr_t)candidate_ptr & k_align_mask) != 0u)
            continue;

        memcpy(&candidate, candidate_ptr, k_size);
        if (dimension_arguments_valid(&candidate))
            return (DimensionArguments *)candidate_ptr;
    }

    return NULL;
}

typedef void (BLC_CALL *DimensionV2Original)(void *self, void *dimension_arguments);
typedef void (BLC_CALL *DimensionV1Original)(void *self, void *level, int32_t dim_id,
                                             int32_t height_range, void *callback_context, void *name);
typedef void (BLC_CALL *DimensionV0Original)(void *self, void *level, int32_t dim_id,
                                             int16_t height_max, void *callback_context, void *name);

static void BLC_CALL dimension_v2(void *self, void *dimension_arguments)
{
    DimensionArguments *args = find_dimension_arguments((uint8_t *)dimension_arguments, 256, 70);

    if (args) {
        DimensionHeightRange range = args->height_range;
        args->height_range = apply_direct_limits(args->dim_id, range);
    }

    if (dimension_hook.original)
        ((DimensionV2Original)dimension_hook.original)(self, dimension_arguments);
}

static void BLC_CALL dimension_v1(void *self, void *level, int32_t dim_id,
                                  int32_t height_range, void *callback_context, void *name)
{
    DimensionHeightRange range;

    range.raw = height_range;
    range = apply_direct_limits(dim_id, range);

    if (dimension_hook.original)
        ((DimensionV1Original)dimension_hook.original)(self, level, dim_id, range.raw, callback_context, name);
}

static void BLC_CALL dimension_v0(void *self, void *level, int32_t dim_id,
                                  int16_t height_max, void *callback_context, void *name)
{
    if (dim_id == 1)
        height_max = 256;

    if (dimension_hook.original)
        ((DimensionV0Original)dimension_hook.original)(self, level, dim_id, height_max, callback_context, name);
}

void setup_hook(void *func, int hook_version)
{
    static void *dimension_hooks[] = {
        (void *)&dimension_v0,
        (void *)&dimension_v1,
        (void *)&dimension_v2,
    };

    if (!func || hook_version < 0 || hook_version >= (int)(sizeof(dimension_hooks) / sizeof(dimension_hooks[0])))
        return;

    dimension_hook = hook_inline_create(func, dimension_hooks[hook_version], 0);
}
