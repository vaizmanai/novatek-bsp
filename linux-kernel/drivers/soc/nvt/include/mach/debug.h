#ifndef __MACH_NVT72668_DEBUG_H
#define __MACH_NVT72668_DEBUG_H

#ifdef CONFIG_SUSPEND

#if 1
#define CONFIG_NVT_SUSPEND_MICOM_IPC
#define CONFIG_NVT_SUSPEND_IPC_AUTO_POWERON
#define CONFIG_NVT_SUSPEND_IPC_SLEEP
#endif

#define CONFIG_NVT_SUSPEND_MICOM_UART

#endif

#endif
