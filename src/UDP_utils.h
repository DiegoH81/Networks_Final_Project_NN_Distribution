#ifndef UDP_UTILS_H
#define UDP_UTILS_H

#include <arpa/inet.h>
#include <string>
#include <cstring>

#include "function_utils.h"

//==================================================
//              UDP Socket Functions
//==================================================

inline int Create_UDP_Socket(){

    int Socket_Master = socket(AF_INET, SOCK_DGRAM, 0);

    if(Socket_Master < 0){

        std::cout << "[ERROR]: Could not create UDP Socket.\n";
        return -1; 

    }

    return Socket_Master;

}

inline bool Bind_UDP_Socket(int Socket_Master, int Port){

    sockaddr_in Local_Address;
    std::memset(&Local_Address, 0, sizeof(Local_Address));

    Local_Address.sin_family = AF_INET;
    Local_Address.sin_port = htons(Port);
    Local_Address.sin_addr.s_addr = INADDR_ANY;

    int Bind_Result = bind(Socket_Master, (sockaddr*)&Local_Address, sizeof(Local_Address));

    if(Bind_Result < 0){

        std::cout << "[ERROR]: Could not bind UDP socket.\n";
        return false;

    }

    return true;

}

inline sockaddr_in Create_Address(std::string IP_Address, int Port){

    sockaddr_in Address;
    std::memset(&Address, 0, sizeof(Address));

    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = inet_addr(IP_Address.c_str());

    return Address;

}

inline bool Send_UDP_Packet(int Socket_Master, std::string Packet, sockaddr_in Destination_Address){

    if(Packet.length() != PACKET_LENGTH){

        std::cout << "[ERROR]: Cannot send packet with invalid length.\n";
        return false;

    }

    int Bytes_Sent = sendto(Socket_Master, Packet.c_str(), PACKET_LENGTH, 0, (sockaddr*)&Destination_Address, sizeof(Destination_Address));

    if(Bytes_Sent != PACKET_LENGTH){

        std::cout << "[ERROR]: Could not send complete UDP Packet.\n";
        return false;

    }

    return true;

}

inline std::string Receive_UDP_Packet(int Socket_Master, sockaddr_in& Sender_Address){

    char Buffer[PACKET_LENGTH];

    socklen_t Sender_Length = sizeof(Sender_Address);

    int Bytes_Received = recvfrom(Socket_Master, Buffer, PACKET_LENGTH, 0, (sockaddr*)&Sender_Address, &Sender_Length);

    if(Bytes_Received <= 0){

        return "";

    }

    if(Bytes_Received != PACKET_LENGTH){

        std::cout << "[ERROR]: Incomplete UDP Packet received.\n";
        return "";

    }

    std::string Packet(Buffer, Bytes_Received);

    return Packet;

}

inline bool Set_Socket_Timeout(int Socket_Master, int Timeout_Miliseconds){

    timeval Timeout_Value;

    Timeout_Value.tv_sec = Timeout_Miliseconds / 1000;
    Timeout_Value.tv_usec = (Timeout_Miliseconds % 1000) * 1000;

    int Result = setsockopt(Socket_Master, SOL_SOCKET, SO_RCVTIMEO, &Timeout_Value, sizeof(Timeout_Value));

    if(Result < 0){

        std::cout << "[ERROR]: Could not set socket timeout.\n";
        return false;

    }

    return true;

}

inline bool Send_Packet_With_ACK(int in_socket, std::string Packet, sockaddr_in Destination_Address){

    std::string Seq_Frag = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);
    std::string Seq_Msg = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH);


    int Retry_Count = 0;

    while(Retry_Count < MAX_RETRIES){

        bool Send_Result = Send_UDP_Packet(in_socket, Packet, Destination_Address);

        if(!Send_Result){

            std::cout << "[ERROR]: Packet send failed. Retry : " << Retry_Count << " .\n";
            Retry_Count++;
            continue; 

        }

        sockaddr_in Sender_Address;
        std::string Response_Packet = Receive_UDP_Packet(in_socket, Sender_Address);

        if(Response_Packet == ""){

            std::cout << "[TIMEOUT]: No ACK/NACK received. Retrying...\n";
            Retry_Count++;
            continue;

        }

        if(!Verify_Packet_Hash(Response_Packet)){

            std::cout << "[ERROR]: ACK/NACK hash invalid. Retrying...\n";
            Retry_Count++;
            continue;

        }

        std::string Response_Seq_Frag = Response_Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);
        std::string Response_Seq_Msg = Response_Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH);
        std::string Response_Payload = Response_Packet.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

        char Response_Type = Response_Payload[0];

        if(Response_Seq_Frag != Seq_Frag || Response_Seq_Msg != Seq_Msg){

            std::cout << "[WARNING]: ACK/NACK does not match current packet. Retrying...\n";
            Retry_Count++;
            continue;

        } 

        if(Response_Type == 'A'){

            std::cout << "[OK]: ACK received for fragment " << Seq_Frag << ".\n";
            return true;

        }

        if(Response_Type == 'N'){

            std::cout << "[NACK]: Fragment " << Seq_Frag << " rejected. Retrying...\n";
            Retry_Count++;
            continue;

        }


        std::cout << "[ERROR]: Unknown ACK/NACK type. Retrying...\n";
        Retry_Count++;

    }

    std::cout << "[ERROR]: Max retries reached for fragment " << Seq_Frag << ".\n";
    return false;

}

#endif