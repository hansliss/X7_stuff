#ifndef X7_DISCOVERY_CONFIG_H
#define X7_DISCOVERY_CONFIG_H
#if PROBE_DISCOVERY == 1
#define DISCOVERY_NAME "commonui"
#elif PROBE_DISCOVERY == 2
#define DISCOVERY_NAME "fusion"
#elif PROBE_DISCOVERY == 3
#define DISCOVERY_NAME "style"
#elif PROBE_DISCOVERY == 4
#define DISCOVERY_NAME "apconfig"
#elif PROBE_DISCOVERY == 5
#define DISCOVERY_NAME "all"
#else
#error Invalid discovery variant
#endif
#define DISCOVERY_LOG "/mnt/card/appprobe-default-" DISCOVERY_NAME ".txt"
#define DISCOVERY_START_LOG "/mnt/card/appprobe-default-" DISCOVERY_NAME "-start-error.txt"
#endif
