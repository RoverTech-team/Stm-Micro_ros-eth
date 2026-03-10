---
title: micro-ROS Overview
parent: micro-ROS
nav_order: 1
---

# micro-ROS Overview
{: .no_toc }

The firmware implements a micro-ROS **XRCE-DDS client** over UDP/Ethernet.

---

## Data Flow

The system communicates with a host agent residing on the same subnet (`192.168.50.x`).

```mermaid
sequenceDiagram
    participant S as STM32H7 (CM7)
    participant A as micro-ROS Agent
    participant D as microk3 Dashboard
    
    Note over S: 192.168.50.2
    Note over A: 192.168.50.1
    
    S->>A: UDP Connect (8888)
    A->>S: Session Ack
    loop Publishing
        S->>A: XRCE Data Message
        A->>D: ROS 2 Topic update
    end
```

---

## RMW Configuration

The Client library is tuned for STM32 memory constraints.

| Parameter | Value |
|---|---|
| Max nodes | 1 |
| Max publishers | 5 |
| Max subscriptions | 5 |
| Max services | 1 |
| Max history | 4 |
| Transport | Custom (UDP over LwIP) |
| Serial profile | Disabled |

---

## Client Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Init
    Init --> AgentDiscovery: Ping Agent
    AgentDiscovery --> SessionEstablish: Connect
    SessionEstablish --> EntityCreation: Create Nodes/Pubs
    EntityCreation --> Spin: Executor Loop
    Spin --> Spin: Process Callbacks
    Spin --> [*]: Error/Shutdown
```
