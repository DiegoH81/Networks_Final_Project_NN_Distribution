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

### 4. Final value

The project will be executed in a Local Area Network (LAN), specifically in a university environment where the server and clients are within the same subnet, this context directly determines the expected RTT values and the resulting timeout values.

### Reference values

The following sources were considered as references for typical RTT behavior in Ethernet LAN environments:

| Source | Typical LAN RTT |
| :--- | :--- |
| Tanenbaum & Wetherall — *Computer Networks* (5th ed.) | 1 ms – 10 ms |
| Kurose & Ross — *Computer Networking: A Top-Down Approach* (8th ed.) | < 1 ms in local Ethernet networks |
| RFC 6298 | Conservative timeout recommendation |

Based on these references, the following initial value was adopted:

$$\text{SampleRTT}_0 = 2 \ ms$$



### Initial Parameters

Using the bibliographic reference RTT, the following initial values were defined:

$$\text{EstimatedRTT}_0 = 2 \ ms$$
$$\text{DevRTT}_0 = 1 \ ms$$

Applying the TCP formulas:

$$\text{TimeoutInterval} = \text{EstimatedRTT} + 4 \cdot \text{DevRTT}$$
$$\text{TimeoutInterval} = 2 + 4(1) = 6 \ ms$$

Then:

$$\text{TimeoutInterval} = 6 \ ms$$



### Initial timeout

Although the formula produces a theoretical timeout of 6 ms, this value is too aggressive for a system without real RTT measurements.

RFC 6298 recommends using: $\text{RTO} = 1000 \ ms$

Using 1000 ms in this project would delay packet loss detection and retransmissions during distributed training, so we adjusted it to fit a LAN environment, Considering a typical LAN RTT of approximately 2 ms, and a safe margin of 100x then:

$$\text{Initial\ Timeout} = 100 \times 2 \ ms = 200 \ ms$$

Once the system starts operating the 200 ms timeout value will change to its final value thanks to the formula presented before.
