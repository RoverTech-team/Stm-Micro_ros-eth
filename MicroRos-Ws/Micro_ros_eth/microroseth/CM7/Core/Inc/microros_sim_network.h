#ifndef MICROROS_SIM_NETWORK_H
#define MICROROS_SIM_NETWORK_H

/*
 * Simulation-oriented network defaults for the Renode + host TAP path.
 * Override any of these macros from the build system if a different lab
 * network is needed.
 */

#ifndef MICROROS_AGENT_IP
#define MICROROS_AGENT_IP "192.168.50.1"
#endif

#ifndef MICROROS_DEVICE_IP_A
#define MICROROS_DEVICE_IP_A 192
#endif
#ifndef MICROROS_DEVICE_IP_B
#define MICROROS_DEVICE_IP_B 168
#endif
#ifndef MICROROS_DEVICE_IP_C
#define MICROROS_DEVICE_IP_C 50
#endif
#ifndef MICROROS_DEVICE_IP_D
#define MICROROS_DEVICE_IP_D 2
#endif

#ifndef MICROROS_GATEWAY_IP_A
#define MICROROS_GATEWAY_IP_A 192
#endif
#ifndef MICROROS_GATEWAY_IP_B
#define MICROROS_GATEWAY_IP_B 168
#endif
#ifndef MICROROS_GATEWAY_IP_C
#define MICROROS_GATEWAY_IP_C 50
#endif
#ifndef MICROROS_GATEWAY_IP_D
#define MICROROS_GATEWAY_IP_D 1
#endif

#ifndef MICROROS_NETMASK_A
#define MICROROS_NETMASK_A 255
#endif
#ifndef MICROROS_NETMASK_B
#define MICROROS_NETMASK_B 255
#endif
#ifndef MICROROS_NETMASK_C
#define MICROROS_NETMASK_C 255
#endif
#ifndef MICROROS_NETMASK_D
#define MICROROS_NETMASK_D 0
#endif

#endif
