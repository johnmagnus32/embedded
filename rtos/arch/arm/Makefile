obj-$(CONFIG_CPU_CORTEX_M4)     += startup_m4.o
obj-$(CONFIG_CPU_CORTEX_M4)     += irq_dispatch.o
obj-$(CONFIG_CPU_CORTEX_M0PLUS) += startup_m0plus.o
obj-$(CONFIG_SYSTICK)           += systick_m4.o
obj-$(CONFIG_SYSTICK)           += systick_m0plus.o
obj-$(CONFIG_SMP)               += smp_rp2040.o
obj-$(CONFIG_MPU)               += mpu.o
# fault.o holds the HardFault handler (always needed — a HardFault can occur
# regardless of MPU); the MemManage handler inside it is #ifdef CONFIG_MPU.
obj-$(CONFIG_CPU_CORTEX_M4)     += fault.o
