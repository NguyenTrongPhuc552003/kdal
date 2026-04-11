/*
 * SoC-specific glue layer for future board adaptation.
 *
 * The purpose of this module is to hold any SoC-specific clock gating,
 * pin-mux, or reset-controller logic that must run before the generic
 * platform backend enumerates devices.  On QEMU this is a no-op.
 *
 * When targeting the CIX P1 SoC on the Radxa Orion O6 this file will
 * configure the peripheral clocks and IOMMU for the devices that KDAL
 * manages.
 */

#include <linux/errno.h>

int kdal_soc_glue_init(void)
{
	/* Future: SoC clock/reset/pinmux initialisation */
	return 0;
}

void kdal_soc_glue_exit(void)
{
	/* Future: SoC cleanup */
}