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

### 4. Initial Value
As stated in RFC 6298, the RT0 should be 1000 ms.
