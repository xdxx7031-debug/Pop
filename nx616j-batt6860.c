/*
 * NX616J Battery Design Capacity KPM
 *
 * Intercepts only fg_psy_get_property(CHARGE_FULL_DESIGN) and overwrites
 * the returned userspace-reporting value with 6860000 uAh.
 *
 * This does NOT change fuel-gauge learning, SOC/OCV, charging limits,
 * charge voltage/current, JEITA, thermal protections, or cutoff voltage.
 *
 * Target assumptions are pinned to LineageOS/android_kernel_nubia_sdm845
 * branch lineage-22.2:
 *   - callback symbol: fg_psy_get_property
 *   - POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN enum value: 22
 *
 * Experimental: symbol availability and APatch/KernelPatch hook behavior
 * must be verified on the target device before enabling persistent loading.
 */
#include <log.h>
#include <compiler.h>
#include <kpmodule.h>
#include <hook.h>

#define NX616J_PROP_CHARGE_FULL_DESIGN 22
#define NX616J_DESIGN_CAPACITY_UAH     6860000

KPM_NAME("nx616j-batt6860");
KPM_VERSION("0.1.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("OpenAI");
KPM_DESCRIPTION("NX616J: report battery design capacity as 6860mAh only");

static void *g_target;
static int g_hooked;

static void after_fg_psy_get_property(hook_fargs3_t *args, void *udata)
{
    int prop = (int)args->arg1;
    int rc = (int)args->ret;
    int *intval;

    /* Leave every property except CHARGE_FULL_DESIGN untouched. */
    if (prop != NX616J_PROP_CHARGE_FULL_DESIGN || rc != 0)
        return;

    intval = (int *)(unsigned long)args->arg2;
    if (!intval)
        return;

    *intval = NX616J_DESIGN_CAPACITY_UAH;
}

static long nx616j_batt6860_init(const char *args, const char *event,
                                  void *reserved)
{
    hook_err_t err;

    g_target = (void *)kallsyms_lookup_name("fg_psy_get_property");
    if (!g_target) {
        logke("nx616j-batt6860: symbol fg_psy_get_property not found; no hook installed\n");
        return -1;
    }

    err = hook_wrap3(g_target, 0, after_fg_psy_get_property, 0);
    if (err != HOOK_NO_ERR) {
        logke("nx616j-batt6860: hook_wrap3 failed: %d\n", (int)err);
        g_target = 0;
        return -2;
    }

    g_hooked = 1;
    logkd("nx616j-batt6860: hook active; design capacity report override = %d uAh\n",
          NX616J_DESIGN_CAPACITY_UAH);
    return 0;
}

static long nx616j_batt6860_exit(void *reserved)
{
    if (g_hooked && g_target) {
        hook_unwrap(g_target, 0, after_fg_psy_get_property);
        g_hooked = 0;
        g_target = 0;
        logkd("nx616j-batt6860: hook removed\n");
    }
    return 0;
}

KPM_INIT(nx616j_batt6860_init);
KPM_EXIT(nx616j_batt6860_exit);
