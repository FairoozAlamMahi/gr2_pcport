# Let's inspect the AMD GCN CIK register offsets around 0x28000 (0xA000)
# In Linux kernel drivers/gpu/drm/amd/include/asic_reg/cik/cik_regs.h or mesa
# Let's search online or calculate offsets

# Common CIK Context Registers:
# 0x28000: DB_RENDER_CONTROL
# 0x28004: DB_COUNT_CONTROL
# 0x28008: DB_DEPTH_VIEW
# 0x2800C: DB_RENDER_OVERRIDE
# 0x28010: DB_RENDER_OVERRIDE2
# 0x28014: DB_HTILE_DATA_BASE
# ...
# Wait, let's verify exact CIK register offsets!
