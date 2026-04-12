/*
 * SoC-specific glue layer for board adaptation.
 *
 * The purpose of this module is to hold any SoC-specific clock gating,
 * pin-mux, or reset-controller logic that must run before the generic
 * platform backend enumerates devices.  On QEMU (no Device Tree node
 * named "kdal,soc-glue") the functions are no-ops.
 *
 * When targeting the CIX P1 SoC on the Radxa Orion O6 this file
 * configures the peripheral clocks and reset controller for the
 * devices that KDAL manages.
 */

#include <linux/errno.h>

#ifdef CONFIG_OF
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/reset.h>

static struct clk *soc_peri_clk;
static struct reset_control *soc_reset;
static struct device_node *soc_np;
#endif /* CONFIG_OF */

int kdal_soc_glue_init(void)
{
#ifdef CONFIG_OF
	int ret;

	soc_np = of_find_compatible_node(NULL, NULL, "kdal,soc-glue");
	if (!soc_np) {
		/* No SoC glue DT node - expected on QEMU */
		return 0;
	}

	soc_peri_clk = of_clk_get(soc_np, 0);
	if (IS_ERR(soc_peri_clk)) {
		ret = PTR_ERR(soc_peri_clk);
		soc_peri_clk = NULL;
		goto err_put_node;
	}

	ret = clk_prepare_enable(soc_peri_clk);
	if (ret)
		goto err_put_clk;

	soc_reset = of_reset_control_get(soc_np, NULL);
	if (IS_ERR(soc_reset)) {
		soc_reset = NULL;
		/* Reset controller is optional - continue without it */
	} else {
		reset_control_deassert(soc_reset);
	}

	pr_info("kdal: SoC glue initialised (clk + reset)\n");
	return 0;

err_put_clk:
	clk_put(soc_peri_clk);
	soc_peri_clk = NULL;
err_put_node:
	of_node_put(soc_np);
	soc_np = NULL;
	return ret;
#else
	return 0;
#endif /* CONFIG_OF */
}

void kdal_soc_glue_exit(void)
{
#ifdef CONFIG_OF
	if (soc_reset) {
		reset_control_assert(soc_reset);
		reset_control_put(soc_reset);
		soc_reset = NULL;
	}

	if (soc_peri_clk) {
		clk_disable_unprepare(soc_peri_clk);
		clk_put(soc_peri_clk);
		soc_peri_clk = NULL;
	}

	if (soc_np) {
		of_node_put(soc_np);
		soc_np = NULL;
	}

	pr_info("kdal: SoC glue cleaned up\n");
#endif /* CONFIG_OF */
}
