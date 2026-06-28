#ifndef UDP_BASE_H
#define UDP_BASE_H

#include <arpa/inet.h>
#include <string>
#include <cstring>
#include <iostream>

#include "definitions.h"
#include "function_utils.h"

class UDP_BASE
{
public:
    UDP_BASE(): priv_socket(0) {}

    int Create_UDP_Socket()
    {
        int to_return_skt = socket(AF_INET, SOCK_DGRAM, 0);

        if(to_return_skt < 0){

            std::cout << "[ERROR]: Could not create UDP Socket.\n";
            return -1; 

        }

        return to_return_skt;

    }

    bool Set_Socket_Timeout(int Socket_Master, int Timeout_Miliseconds)
    {
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
protected:
    int priv_socket;

    bool Send_UDP_Packet(int Socket_Master, std::string Packet, sockaddr_in Destination_Address)
    {
        if(Packet.length() != PACKET_LENGTH) {

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

    std::string Receive_UDP_Packet(int Socket_Master, sockaddr_in& Sender_Address)
    {
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

    bool Send_Packet_With_ACK(int in_socket, std::string Packet, sockaddr_in Destination_Address)
    {
        std::string Seq_Frag = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);
        std::string Seq_Msg = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH);

        int Retry_Count = 0;

        while(Retry_Count < MAX_RETRIES)
        {
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

    std::string Add_Payload_Padding(std::string Payload, char Char_Padding = '#')
    {
        if(Payload.length() > PAYLOAD_LENGTH){

            std::cout << "[ERROR]: The Packet Content exceeds payload length.\n";
            return "";

        }

        while(Payload.length() < PAYLOAD_LENGTH)
            Payload += Char_Padding;

        return Payload;
    }

    std::string Remove_Padding(std::string Packet_Content, char Char_Padding = '#')
    {
        while(!Packet_Content.empty() && Packet_Content.back() == Char_Padding)
            Packet_Content.pop_back();

        return Packet_Content;
    }

    char Get_Padding_Char(std::string Control_Frag, int Seq_Num_Frag)
    {
        if(Control_Frag == "11" && Seq_Num_Frag == 0)
            return '@';

        return '#';
    }

    bool Verify_Packet_Hash(std::string Packet_Content)
    {
        if(Packet_Content.length() != PACKET_LENGTH){

            std::cout << "[ERROR]: Invalid packet length.\n";
            return false;

        }

        std::string Received_Hash = Packet_Content.substr(0, HASH_LENGTH);
        std::string Payload = Packet_Content.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

        unsigned int Hash = Calculate_CRC32(Payload);
        std::string Calculated_Hash = Int_to_String(Hash, HASH_LENGTH);

        return Received_Hash == Calculated_Hash;
    }

    //==================================================
    //              Packet Construction
    //==================================================

    std::string Build_Header(std::string Hash_CRC32, std::string Control_Frag, int Seq_Num_Frag, int Seq_Num_Msg)
    {
        std::string Header_Content = "";

        Header_Content += Hash_CRC32;
        Header_Content += Control_Frag;
        Header_Content += Int_to_String(Seq_Num_Frag, SEQ_NUM_FRAG_LENGTH);
        Header_Content += Int_to_String(Seq_Num_Msg, SEQ_NUM_MSG_LENGTH);

        return Header_Content;
    }

    std::string Build_Packet(std::string Control_Frag, int Seq_Num_Frag, int Seq_Num_Msg, std::string Payload)
    {
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

    std::vector<std::string> Fragment_Message(char Message_Type, int Seq_Num_Msg, std::string Full_Data)
    {
        std::vector<std::string> Fragment_List;

        size_t Total_Data_Size = Full_Data.length();

        std::string Data_Size_String = Int_to_String(Total_Data_Size, DATA_SIZE_LENGTH);

        if(Full_Data.length() <= FIRST_PAYLOAD_DATA_SIZE)
        {
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

        while(Current_Position < Total_Data_Size)
        {
            size_t Remaining_Data = Total_Data_Size - Current_Position;
            size_t Current_Data_Size = std::min(Remaining_Data, (size_t)NORMAL_PAYLOAD_DATA_SIZE);

            std::string Data_Fragment = Full_Data.substr(Current_Position, Current_Data_Size);

            Current_Position += Current_Data_Size;

            std::string Control_Frag = "";

            if(Current_Position >= Total_Data_Size)
                Control_Frag = "11";
            else
                Control_Frag = "00";

            std::string Packet = Build_Packet(Control_Frag, Current_Frag_Num, Seq_Num_Msg, Data_Fragment);

            Fragment_List.push_back(Packet);

            Current_Frag_Num++;
        }

        return Fragment_List;
    }

    //==================================================
    //              Reassembly Logic
    //==================================================

    std::string Reassemble_Message(std::vector<std::string> Fragment_List)
    {
        std::string Full_Data = "";

        unsigned long long Expected_Data_Size = 0;
        char Message_Type = '\0';

        for(size_t i = 0; i < Fragment_List.size(); i++)
        {
            std::string Packet = Fragment_List[i];

            if(!Verify_Packet_Hash(Packet))
            {
                std::cout << "[ERROR]: Invalid hash in fragment " << i << ".\n";
                return "";
            }

            std::string Control_Frag = Packet.substr(HASH_LENGTH, CTRL_FRAG_LENGTH);
            std::string Seq_Frag_String = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);

            int Seq_Frag = std::stoi(Seq_Frag_String);

            std::string Payload = Packet.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

            if(i == 0)
            {
                Message_Type = Payload[0];

                std::string Data_Size_String = Payload.substr(TYPE_LENGTH, DATA_SIZE_LENGTH);

                Expected_Data_Size = std::stoull(Data_Size_String);

                std::string First_Data = Payload.substr(TYPE_LENGTH + DATA_SIZE_LENGTH);

                Full_Data += First_Data;
            }
            else
            {
                if(Control_Frag == "11")
                {
                    char Char_Padding = Get_Padding_Char(Control_Frag, Seq_Frag);

                    std::string Clean_Payload = Remove_Padding(Payload, Char_Padding);
                    Full_Data += Clean_Payload;
                }
                else
                    Full_Data += Payload;              
            }
        }

        if(Full_Data.length() > Expected_Data_Size)
            Full_Data = Full_Data.substr(0, Expected_Data_Size);

        if(Full_Data.length() != Expected_Data_Size)
        {
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
    
    std::string Build_ACK_NACK_Packet(int Seq_Num_Frag, int Seq_Num_Msg, char ACK_Type)
    {
        std::string Payload = "";

        Payload += ACK_Type;

        std::string Packet = Build_Packet("11", Seq_Num_Frag, Seq_Num_Msg, Payload);

        return Packet;
    }
};
#endif