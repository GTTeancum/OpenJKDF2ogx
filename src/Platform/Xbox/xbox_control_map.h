#ifndef XBOX_CONTROL_MAP_H
#define XBOX_CONTROL_MAP_H

/* Logical XDK analog-button slots -> physical slots. Menu input stays physical. */
#define XBOX_CONTROL_MAP_COUNT 8

static int xboxControlMap_Valid(const int *map)
{
    unsigned int seen = 0;
    int i;
    for (i = 0; i < XBOX_CONTROL_MAP_COUNT; ++i) {
        if (map[i] < 0 || map[i] >= XBOX_CONTROL_MAP_COUNT || (seen & (1u << map[i])))
            return 0;
        seen |= 1u << map[i];
    }
    return 1;
}

static void xboxControlMap_Defaults(int *map)
{
    int i;
    for (i = 0; i < XBOX_CONTROL_MAP_COUNT; ++i) map[i] = i;
}

/* Swap the displaced action so no action loses its binding. */
static int xboxControlMap_Assign(int *map, int logical, int physical)
{
    int i, previous;
    if (logical < 0 || logical >= XBOX_CONTROL_MAP_COUNT || physical < 0
        || physical >= XBOX_CONTROL_MAP_COUNT || !xboxControlMap_Valid(map)) return 0;
    previous = map[logical];
    for (i = 0; i < XBOX_CONTROL_MAP_COUNT; ++i)
        if (map[i] == physical) map[i] = previous;
    map[logical] = physical;
    return 1;
}

static void xboxControlMap_Apply(const int *map, const unsigned char *physical,
                                unsigned char *logical, int gameplay)
{
    int i;
    for (i = 0; i < XBOX_CONTROL_MAP_COUNT; ++i)
        logical[i] = physical[gameplay ? map[i] : i];
}
#endif
