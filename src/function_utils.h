#ifndef FUNCTION_UTILS_H
#define FUNCTION_UTILS_H

#include <vector>
#include <string>
#include <iostream>
#include <arpa/inet.h>


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


//==================================================
//               Auxiliary Functions
//==================================================

inline std::string Int_to_String(unsigned long long Integer_Number, int Size){

    std::string Number_String = std::to_string(Integer_Number);   

    if (Number_String.length() > Size) {

        std::cout << "[ERROR]: Number exceeds field length.\n";
        return "";

    }

    while(Number_String.length() < Size){

        Number_String = "0" + Number_String;

    }

    return Number_String;

}

inline std::string Float_to_String(float Decimal_Number){

    std::string Float_String = std::to_string(Decimal_Number);

    while(!Float_String.empty() && Float_String.back() == '0'){

        Float_String.pop_back();

    }

    if(!Float_String.empty() && Float_String.back() == '.'){

        Float_String.pop_back();

    }

    return Float_String;

}

inline std::string Add_Payload_Padding(std::string Payload, char Char_Padding = '#'){

    if(Payload.length() > PAYLOAD_LENGTH){

        std::cout << "[ERROR]: The Packet Content exceeds payload length.\n";
        return "";

    }

    while(Payload.length() < PAYLOAD_LENGTH){

        Payload += Char_Padding;

    }

    return Payload;

}

inline std::string Remove_Padding(std::string Packet_Content, char Char_Padding = '#'){

    while(!Packet_Content.empty() && Packet_Content.back() == Char_Padding){

        Packet_Content.pop_back();

    }

    return Packet_Content;

}

inline char Get_Padding_Char(std::string Control_Frag, int Seq_Num_Frag){

    if(Control_Frag == "11" && Seq_Num_Frag == 0){

        return '@';

    }

    return '#';

}

inline std::string Address_To_String(sockaddr_in Address){

    std::string IP_Address = inet_ntoa(Address.sin_addr);
    int Port = ntohs(Address.sin_port);

    return IP_Address + ":" + std::to_string(Port);

}

//==================================================
//                 Hash Function
//==================================================

inline unsigned int Calculate_CRC32(std::string Payload){

    unsigned int CRC32_Value = 0xFFFFFFFF;

    for(unsigned char Current_Byte : Payload){

        CRC32_Value = CRC32_Value ^ Current_Byte;

        for(int Bit_Index = 0; Bit_Index < 8; Bit_Index++){

            if(CRC32_Value & 1){ 

                CRC32_Value = (CRC32_Value >> 1)^0xEDB88320; 

            }

            else{

                CRC32_Value = CRC32_Value >> 1;

            }

        }

    }

    return CRC32_Value ^ 0xFFFFFFFF;

}

inline bool Verify_Packet_Hash(std::string Packet_Content){

    if(Packet_Content.length() != PACKET_LENGTH){

        std::cout << "[ERROR]: Invalid packet length.\n";
        return false;

    }

    std::string Received_Hash = Packet_Content.substr(0,HASH_LENGTH);
    std::string Payload = Packet_Content.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

    unsigned int Hash = Calculate_CRC32(Payload);
    std::string Calculated_Hash = Int_to_String(Hash, HASH_LENGTH);

    return Received_Hash == Calculated_Hash;

}

//==================================================
//              Packet Construction
//==================================================

inline std::string Build_Header(std::string Hash_CRC32, std::string Control_Frag, int Seq_Num_Frag, int Seq_Num_Msg){

    std::string Header_Content = "";

    Header_Content += Hash_CRC32;
    Header_Content += Control_Frag;
    Header_Content += Int_to_String(Seq_Num_Frag, SEQ_NUM_FRAG_LENGTH);
    Header_Content += Int_to_String(Seq_Num_Msg, SEQ_NUM_MSG_LENGTH);

    return Header_Content;

}

inline std::string Build_Packet(std::string Control_Frag, int Seq_Num_Frag, int Seq_Num_Msg, std::string Payload){

    if (Payload.length() > PAYLOAD_LENGTH){

        std::cout << "[ERROR]: Payload exceeds payload length.\n";
        return "";

    }

    std::string Packet_Content = "";

    char Char_Padding = Get_Padding_Char(Control_Frag, Seq_Num_Frag);
    Payload = Add_Payload_Padding(Payload, Char_Padding);

    unsigned int Hash = Calculate_CRC32(Payload);
    std::string Hash_CRC32 = Int_to_String(Hash, HASH_LENGTH);

    std::string Header = Build_Header(Hash_CRC32, Control_Frag, Seq_Num_Frag, Seq_Num_Msg);

    Packet_Content += Header;
    Packet_Content += Payload;

    if(Packet_Content.length() != PACKET_LENGTH){

        std::cout << "[ERROR]: Final build packet length is invalid.\n";
        return "";

    }

    return Packet_Content;

}

//==================================================
//              Fragmentation Logic
//==================================================

inline std::vector<std::string> Fragment_Message(char Message_Type, int Seq_Num_Msg, std::string Full_Data){

    std::vector<std::string> Fragment_List;

    size_t Total_Data_Size = Full_Data.length();

    std::string Data_Size_String = Int_to_String(Total_Data_Size, DATA_SIZE_LENGTH);

    if(Full_Data.length() <= FIRST_PAYLOAD_DATA_SIZE){

        std::string Payload = "";

        Payload += Message_Type;
        Payload += Data_Size_String;
        Payload += Full_Data;

        std::string Packet = Build_Packet("11", 0, Seq_Num_Msg, Payload);

        Fragment_List.push_back(Packet);

        return Fragment_List;

    }

    std::string First_Data = Full_Data.substr(0, FIRST_PAYLOAD_DATA_SIZE);

    std::string First_Payload = "";

    First_Payload += Message_Type;
    First_Payload += Data_Size_String;
    First_Payload += First_Data;

    std::string First_Packet = Build_Packet("01", 0, Seq_Num_Msg, First_Payload);

    Fragment_List.push_back(First_Packet);

    size_t Current_Position = FIRST_PAYLOAD_DATA_SIZE;
    int Current_Frag_Num = 1;

    while(Current_Position < Total_Data_Size){

        size_t Remaining_Data = Total_Data_Size - Current_Position;
        size_t Current_Data_Size = std::min(Remaining_Data, (size_t)NORMAL_PAYLOAD_DATA_SIZE);

        std::string Data_Fragment = Full_Data.substr(Current_Position, Current_Data_Size);

        Current_Position += Current_Data_Size;

        std::string Control_Frag = "";

        if(Current_Position >= Total_Data_Size){ Control_Frag = "11"; }
        else { Control_Frag = "00"; }

        std::string Packet = Build_Packet(Control_Frag, Current_Frag_Num, Seq_Num_Msg, Data_Fragment);

        Fragment_List.push_back(Packet);

        Current_Frag_Num++;

    }

    return Fragment_List;

}

//==================================================
//              Reassembly Logic
//==================================================

inline std::string Reassemble_Message(std::vector<std::string> Fragment_List){

    std::string Full_Data = "";

    unsigned long long Expected_Data_Size = 0;
    char Message_Type = '\0';

    for(size_t i = 0; i < Fragment_List.size(); i++){

        std::string Packet = Fragment_List[i];

        if(!Verify_Packet_Hash(Packet)){

            std::cout << "[ERROR]: Invalid hash in fragment " << i << ".\n";
            return "";

        }

        std::string Control_Frag = Packet.substr(HASH_LENGTH, CTRL_FRAG_LENGTH);
        std::string Seq_Frag_String = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);

        int Seq_Frag = std::stoi(Seq_Frag_String);

        std::string Payload = Packet.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

        if(i == 0){

            Message_Type = Payload[0];

            std::string Data_Size_String = Payload.substr(TYPE_LENGTH, DATA_SIZE_LENGTH);

            Expected_Data_Size = std::stoull(Data_Size_String);

            std::string First_Data = Payload.substr(TYPE_LENGTH + DATA_SIZE_LENGTH);

            Full_Data += First_Data;
        }
        else{

            if(Control_Frag == "11"){

                char Char_Padding = Get_Padding_Char(Control_Frag, Seq_Frag);

                std::string Clean_Payload = Remove_Padding(Payload, Char_Padding);
                Full_Data += Clean_Payload;

            }

            else{

                Full_Data += Payload;

            }
            
        }

    }

    if(Full_Data.length() > Expected_Data_Size){

        Full_Data = Full_Data.substr(0, Expected_Data_Size);

    }

    if(Full_Data.length() != Expected_Data_Size){

        std::cout << "[ERROR]: Reassembled data size is invalid.\n";
        std::cout << "Expected: " << Expected_Data_Size << "\n";
        std::cout << "Received: " << Full_Data.length() << "\n";
        return "";

    }

    return Full_Data;

}


//==================================================
//              ACK / NACK Logic
//==================================================

inline std::string Build_ACK_NACK_Packet(int Seq_Num_Frag, int Seq_Num_Msg, char ACK_Type){

    std::string Payload = "";

    Payload += ACK_Type;

    std::string Packet = Build_Packet("11", Seq_Num_Frag, Seq_Num_Msg, Payload);

    return Packet;

}

#endif