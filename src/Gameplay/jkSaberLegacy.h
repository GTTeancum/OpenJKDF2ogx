/* GLOW3 embeds a second blade in its weapon meshes. Hide only those effect
 * materials while the procedural saber is enabled; retain hands and hilt. */
static int jkSaber_IsLegacyGlowFace(const char *model, const char *material)
{
    char m[64], t[64];
    int i;
    const char *p, *base = model;
    for (p = model; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    for (i = 0; base[i] && i < 63; ++i) m[i] = (base[i] >= 'A' && base[i] <= 'Z') ? base[i] + 32 : base[i];
    m[i] = 0;
    if (strncmp(m, "sabg-", 5) && strncmp(m, "sabv-", 5) && strncmp(m, "sabfan-", 7)) return 0;
    base = material;
    for (p = material; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    for (i = 0; base[i] && i < 63; ++i) t[i] = (base[i] >= 'A' && base[i] <= 'Z') ? base[i] + 32 : base[i];
    t[i] = 0;
    if (!strcmp(t, "white.mat")) return 1;
    if (t[0] != 'd' && t[0] != 'l' && t[0] != 'z') return 0;
    return !strcmp(t+1, "green.mat") || !strcmp(t+1, "yellow.mat") ||
        !strcmp(t+1, "orange.mat") || !strcmp(t+1, "red.mat") ||
        !strcmp(t+1, "purple.mat") || !strcmp(t+1, "blue.mat");
}
