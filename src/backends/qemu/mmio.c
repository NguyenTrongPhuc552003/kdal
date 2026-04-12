/*
 * MMIO register helpers for emulated backends.
 *
 * A thin wrapper around readl/writel that centralises MMIO access so
 * backends do not scatter iomem casts throughout their code.  These
 * helpers also provide a single instrumentation point for future
 * tracing of register-level activity.
 */

#include <linux/io.h>
#include <linux/types.h>

u32 kdal_mmio_read32(void __iomem *base, u32 offset)
{
	return readl((u8 __iomem *)base + offset);
}

void kdal_mmio_write32(void __iomem *base, u32 offset, u32 value)
{
	writel(value, (u8 __iomem *)base + offset);
}

u16 kdal_mmio_read16(void __iomem *base, u32 offset)
{
	return readw((u8 __iomem *)base + offset);
}

void kdal_mmio_write16(void __iomem *base, u32 offset, u16 value)
{
	writew(value, (u8 __iomem *)base + offset);
}

u8 kdal_mmio_read8(void __iomem *base, u32 offset)
{
	return readb((u8 __iomem *)base + offset);
}

void kdal_mmio_write8(void __iomem *base, u32 offset, u8 value)
{
	writeb(value, (u8 __iomem *)base + offset);
}
