# Final Networks Project

Final project for the Networks course

Students:
- Cuadros Álvarez, Jose Francesco
- Hidalgo Machaca, Diego Alejandro
- Valdivia Castillo, Jose Miguel Mateo
- Valencia Flores, Neymi Arlyz


## Protocol

Each protocol message will have a size of 500 bytes, and the hashing that we will use is SHA-256.
The protocol will have three different types of messages:

## Data Packet (D)

| 1B | 3B | 3B | 3B | 3B | 64B | Size | Remaining size |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| D | Sequence number | Data size | Chunk ID | Total chunks | Hash | Data | Padding |


## Acknowledgment Packet (A)

| 1B | 3B | Remaining size |
| :---: | :---: | :---: |
| A | Sequence number | Padding |


## Negative Acknowledgment Packet (N)

| 1B | 3B | Remaining size |
| :---: | :---: | :---: |
| N | Sequence number | Padding |
