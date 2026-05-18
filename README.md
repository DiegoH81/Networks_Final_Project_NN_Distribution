# Final Networks Project

Final project for the Networks course

Students:
- Cuadros Álvarez, Jose Francesco
- Hidalgo Machaca, Diego Alejandro
- Valdivia Castillo, Jose Miguel Mateo
- Valencia Flores, Neymi Arlyz


## Protocol

Each protocol message will have a size of 500 bytes, the padding character will be '#' and the hashing that we will use is SHA-256.
The protocol will have three different types of messages:

### Data Packet (D)

| 1B | 3B | 3B | 3B | 3B | 64B | Size | Remaining size |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| D | Sequence number | Data size | Chunk ID | Total chunks | Hash | Data | Padding |


### Acknowledgment Packet (A)

| 1B | 3B | Remaining size |
| :---: | :---: | :---: |
| A | Sequence number | Padding |


### Negative Acknowledgment Packet (N)

| 1B | 3B | Remaining size |
| :---: | :---: | :---: |
| N | Sequence number | Padding |


## Timeout criteria

To define the timeout, the algorithm used by TCP to calculate the Retransmission Timeout was taken as a reference, as presented in RFC 6298.

### Formulas used
### 1. RTT Estimation
TCP uses an Exponentially Weighted Moving Average (EWMA) of the RTT:
$$\text{EstimatedRTT} = (1 - \alpha) \cdot \text{EstimatedRTT} + \alpha \cdot \text{SampleRTT}$$

**Where:**
* SampleRTT: Measured time from sending the packet until receiving the ACK.
* EstimatedRTT: Previous smoothed average RTT.
* $\alpha = 0.125$

### 2. RTT Variation
To measure temporal network variability:
$$\text{DevRTT} = (1 - \beta) \cdot \text{DevRTT} + \beta \cdot |\text{SampleRTT} - \text{EstimatedRTT}|$$

**Where:**
* `DevRTT`: Represents the previous average deviation.
* $\beta = 0.25$

### 3. Timeout Calculation
$$\text{TimeoutInterval} = \text{EstimatedRTT} + 4 \cdot \text{DevRTT}$$

The factor $4 \cdot \text{DevRTT}$ acts as a safety margin.

### 4. Data Acquisition
### 4.1 Network Context

The project will be executed in a Local Area Network (LAN), specifically in a university environment where the server and clients are located within the same subnet.

This context directly determines the expected RTT values and, consequently, the resulting timeout values.

Since the system is currently in the design phase and real measurements are not yet available, the initial RTT values were obtained from technical networking literature and RFC recommendations.

---

### 4.2 Bibliographic Reference Values

The following sources were considered as references for typical RTT behavior in Ethernet LAN environments:

| Source | Typical LAN RTT |
| :--- | :--- |
| Tanenbaum & Wetherall — *Computer Networks* (5th ed.) | 1 ms – 10 ms |
| Kurose & Ross — *Computer Networking: A Top-Down Approach* (8th ed.) | < 1 ms in local Ethernet networks |
| RFC 6298 | Conservative timeout recommendation |

Based on these references, the following initial value was adopted:

$$SampleRTT_0 = 2 \ ms$$

This value is considered a conservative and representative estimate for a university LAN operating under normal traffic conditions.

---

### 4.3 Initial Parameters

Using the bibliographic reference RTT, the following initial values were defined:

$$EstimatedRTT_0 = 2 \ ms$$

Since there is no previous history during the first iteration, the initial EstimatedRTT is assumed equal to the first SampleRTT.

$$DevRTT_0 = 1 \ ms$$

Applying the TCP formulas:

$$TimeoutInterval = EstimatedRTT + 4 \cdot DevRTT$$

$$TimeoutInterval = 2 + 4(1) = 6 \ ms$$

Therefore:

$$TimeoutInterval = 6 \ ms$$

---

### 4.4 Initial Timeout Selection

Although the formula produces a theoretical timeout of 6 ms, this value is too aggressive for a system without real RTT measurements.

RFC 6298 recommends using:

$$RTO = 1000 \ ms$$

before obtaining the first RTT samples. However, that recommendation was designed for Internet-scale environments, where RTT values are significantly larger than in a LAN.

Using 1000 ms in this project would unnecessarily delay packet loss detection and retransmissions during distributed training.

Therefore, the RFC criterion was adapted proportionally to the LAN environment.

Considering a typical LAN RTT of approximately 2 ms, a conservative safety margin of 100× was adopted:

$$Initial\ Timeout = 100 \times 2 \ ms = 200 \ ms$$

Thus, the selected initial timeout is:

$$TimeoutInitial = 200 \ ms$$

This value:

- Is significantly greater than the theoretical minimum (6 ms)
- Remains well below the RFC Internet recommendation (1000 ms)
- Provides robustness against transient latency spikes
- Avoids unnecessary retransmissions
- Does not significantly slow distributed training

---

### 4.5 Dynamic Runtime Adjustment

The 200 ms timeout is only the initial value.

Once the system starts operating, each ACK received generates a new `SampleRTT` that dynamically updates:

- `EstimatedRTT`
- `DevRTT`
- `TimeoutInterval`

using the EWMA formulas previously defined.

This allows the timeout to automatically converge to the real behavior of the network during execution.

