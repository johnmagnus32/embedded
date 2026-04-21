# Select startup and systick based on CPU type
obj-$(CONFIG_CPU_CORTEX_M4)     += startup.o
obj-$(CONFIG_CPU_CORTEX_M0PLUS) += startup_m0plus.o

# Both compile but #ifdef guards ensure only one has code
obj-$(CONFIG_SYSTICK)           += systick.o
obj-$(CONFIG_SYSTICK)           += systick_m0plus.o
