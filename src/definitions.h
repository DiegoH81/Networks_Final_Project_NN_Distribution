#ifndef DEFINITIONS_H
#define DEFINITIONS_H


//==================================================
//               Project Constants
//==================================================

#define PACKET_LENGTH 500

#define HASH_LENGTH 10
#define CTRL_FRAG_LENGTH 2
#define SEQ_NUM_FRAG_LENGTH 4
#define SEQ_NUM_MSG_LENGTH 4
#define TYPE_LENGTH 1
#define DATA_SIZE_LENGTH 10

#define BATCH_ID_LENGTH 5
#define LAYER_ID_LENGTH 3
#define ROWS_LENGTH 6
#define COLUMNS_LENGTH 4

#define TIMEOUT_MS 500
#define MAX_RETRIES 5

#define NUM_SLAVES 3
#define NUM_LAYERS 4

//==================================================
//                Protocol Lengths
//==================================================

#define HEADER_LENGTH (HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH + SEQ_NUM_MSG_LENGTH)

#define PAYLOAD_LENGTH (PACKET_LENGTH - HEADER_LENGTH)

#define FIRST_PAYLOAD_DATA_SIZE (PAYLOAD_LENGTH - TYPE_LENGTH - DATA_SIZE_LENGTH)
#define NORMAL_PAYLOAD_DATA_SIZE PAYLOAD_LENGTH
#define ACK_NACK_PADDING_SIZE (PAYLOAD_LENGTH - TYPE_LENGTH)

#endif