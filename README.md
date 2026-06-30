# Final Networks Project

Final project for the Networks course

Students:
- Cuadros Álvarez, Jose Francesco
- Hidalgo Machaca, Diego Alejandro
- Valdivia Castillo, Jose Miguel Mateo
- Valencia Flores, Neymi Arlyz



## UDP Communication Protocol

### General packet structure

| HASH | CONTROL | SEQ_FRAG | SEQ_MSG | PAYLOAD |
| :---: | :---: | :---: | :---: | :---: |
| 10B | 2B | 4B | 4B | 480B |

- **HASH**: CRC32 computed over the payload, represented as an ASCII decimal number. Used to validate packet integrity.
- **CONTROL**: indicates the fragment type.

| Value | Meaning |
| :---: | :--- |
| 01 | First fragment |
| 00 | Intermediate fragment |
| 11 | Last fragment (or single-fragment message) |

- **SEQ_FRAG** (4B): fragment number within the message, used to reorder fragments.
- **SEQ_MSG** (4B): logical message identifier.

### Payload of the first fragment (or single-packet message)

| TYPE | DATA_SIZE | DATA |
| :---: | :---: | :---: |
| 1B | 10B | up to 469B |

Intermediate and final fragments use the full payload (480B) directly for data.

### Message types (TYPE field)

| Type | Meaning |
| :---: | :--- |
| L | Slave login |
| B | Dataset block |
| P | Weights sent by the Master |
| R | Result sent by the Slave |
| E | End |
| A | ACK |
| N | NACK |

---

### Message L — Slave registration

| TYPE | DATA |
| :---: | :---: |
| L | empty |

---

### Message B — Dataset block

| ROWS | COLUMNS | CSV_BLOCK |
| :---: | :---: | :---: |
| 6B | 4B | CSV data assigned to the Slave |

Each Slave receives its own block and stores it locally.

---

### Message P — Weights sent by the Master

| BATCH_ID | LAYER_ID | ROWS | COLUMNS | WEIGHTS_DATA |
| :---: | :---: | :---: | :---: | :---: |
| 5B | 3B | 6B | 4B | matrix serialized as text |

---

### Message R — Slave result

Same logical structure as `P`:

| BATCH_ID | LAYER_ID | ROWS | COLUMNS | UPDATED_WEIGHTS_DATA |
| :---: | :---: | :---: | :---: | :---: |
| 5B | 3B | 6B | 4B | matrix serialized as text |

---

### Message E — Termination

| TYPE | DATA |
| :---: | :---: |
| E | "END" |

---

### ACK and NACK

| TYPE | PADDING |
| :---: | :---: |
| 1B | rest of the payload |

---

### Padding

| Character | Use |
| :---: | :--- |
| # | Normal padding |
| @ | Padding for single-fragment messages |
