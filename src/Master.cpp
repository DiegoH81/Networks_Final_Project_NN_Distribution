#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <cctype>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <stdexcept>

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
//               Project Structs
//==================================================

struct Slave_Info{

    int Slave_ID;
    sockaddr_in Slave_Address;

};

struct Dataset_Distribution{

    std::string Master_CSV_Block;
    int Master_Rows;
    int Dataset_Columns;
    std::vector<std::string> Slave_Data_Blocks;

};

struct Weights_Result{

    int Batch_ID;
    int Layer_ID;
    int Rows;
    int Columns;
    std::string Weights_Data;
    bool Is_Valid;

};

//==================================================
//               Auxiliary Functions
//==================================================

std::string Int_to_String(unsigned long long Integer_Number, int Size){

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

std::string Float_to_String(float Decimal_Number){

    std::string Float_String = std::to_string(Decimal_Number);

    while(!Float_String.empty() && Float_String.back() == '0'){

        Float_String.pop_back();

    }

    if(!Float_String.empty() && Float_String.back() == '.'){

        Float_String.pop_back();

    }

    return Float_String;

}

std::string Add_Payload_Padding(std::string Payload, char Char_Padding = '#'){

    if(Payload.length() > PAYLOAD_LENGTH){

        std::cout << "[ERROR]: The Packet Content exceeds payload length.\n";
        return "";

    }

    while(Payload.length() < PAYLOAD_LENGTH){

        Payload += Char_Padding;

    }

    return Payload;

}

std::string Remove_Padding(std::string Packet_Content, char Char_Padding = '#'){

    while(!Packet_Content.empty() && Packet_Content.back() == Char_Padding){

        Packet_Content.pop_back();

    }

    return Packet_Content;

}

char Get_Padding_Char(std::string Control_Frag, int Seq_Num_Frag){

    if(Control_Frag == "11" && Seq_Num_Frag == 0){

        return '@';

    }

    return '#';

}

std::string Address_To_String(sockaddr_in Address){

    std::string IP_Address = inet_ntoa(Address.sin_addr);
    int Port = ntohs(Address.sin_port);

    return IP_Address + ":" + std::to_string(Port);

}

//==================================================
//                 Hash Function
//==================================================

unsigned int Calculate_CRC32(std::string Payload){

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

bool Verify_Packet_Hash(std::string Packet_Content){

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

std::string Build_Header(std::string Hash_CRC32, std::string Control_Frag, int Seq_Num_Frag, int Seq_Num_Msg){

    std::string Header_Content = "";

    Header_Content += Hash_CRC32;
    Header_Content += Control_Frag;
    Header_Content += Int_to_String(Seq_Num_Frag, SEQ_NUM_FRAG_LENGTH);
    Header_Content += Int_to_String(Seq_Num_Msg, SEQ_NUM_MSG_LENGTH);

    return Header_Content;

}

std::string Build_Packet(std::string Control_Frag, int Seq_Num_Frag, int Seq_Num_Msg, std::string Payload){

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

std::vector<std::string> Fragment_Message(char Message_Type, int Seq_Num_Msg, std::string Full_Data){

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

std::string Reassemble_Message(std::vector<std::string> Fragment_List){

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

std::string Build_ACK_NACK_Packet(int Seq_Num_Frag, int Seq_Num_Msg, char ACK_Type){

    std::string Payload = "";

    Payload += ACK_Type;

    std::string Packet = Build_Packet("11", Seq_Num_Frag, Seq_Num_Msg, Payload);

    return Packet;

}

//==================================================
//              UDP Socket Functions
//==================================================

int Create_UDP_Socket(){

    int Socket_Master = socket(AF_INET, SOCK_DGRAM, 0);

    if(Socket_Master < 0){

        std::cout << "[ERROR]: Could not create UDP Socket.\n";
        return -1; 

    }

    return Socket_Master;

}

bool Bind_UDP_Socket(int Socket_Master, int Port){

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

sockaddr_in Create_Address(std::string IP_Address, int Port){

    sockaddr_in Address;
    std::memset(&Address, 0, sizeof(Address));

    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = inet_addr(IP_Address.c_str());

    return Address;

}

bool Send_UDP_Packet(int Socket_Master, std::string Packet, sockaddr_in Destination_Address){

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

std::string Receive_UDP_Packet(int Socket_Master, sockaddr_in& Sender_Address){

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

bool Set_Socket_Timeout(int Socket_Master, int Timeout_Miliseconds){

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

//==================================================
//                  Master Logic
//==================================================

bool Send_Packet_With_ACK(int Socket_Master, std::string Packet, sockaddr_in Destination_Address){

    std::string Seq_Frag = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);
    std::string Seq_Msg = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH);


    int Retry_Count = 0;

    while(Retry_Count < MAX_RETRIES){

        bool Send_Result = Send_UDP_Packet(Socket_Master, Packet, Destination_Address);

        if(!Send_Result){

            std::cout << "[ERROR]: Packet send failed. Retry : " << Retry_Count << " .\n";
            Retry_Count++;
            continue; 

        }

        sockaddr_in Sender_Address;
        std::string Response_Packet = Receive_UDP_Packet(Socket_Master, Sender_Address);

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

std::vector<Slave_Info> Register_Slaves(int Master_Socket, int Expected_Slaves){

    std::vector<Slave_Info> Slave_List;

    std::cout << "[INFO]: Waiting for: "  << Expected_Slaves << " slaves...\n";

    while((int)Slave_List.size() < Expected_Slaves){

        sockaddr_in Sender_Address;

        std::string Register_Packet = Receive_UDP_Packet(Master_Socket, Sender_Address);

        if(Register_Packet == ""){

            std::cout << "[WARNING]: Invalid register packet received.\n";
            continue;

        }

        if(!Verify_Packet_Hash(Register_Packet)){

            std::cout << "[WARNING]: Register Packet hash invalid.\n";
            continue;

        }

        std::string Payload = Register_Packet.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

        char Message_Type = Payload[0];

        if(Message_Type != 'L'){

            std::cout << "[WARNING]: Packet ignored. Expected register type 'L'\n";
            continue;

        }

        bool Already_Registered = false;

        std::string New_Address_Key = Address_To_String(Sender_Address);

        for(size_t i = 0; i < Slave_List.size(); i++){

            std::string Current_Address = Address_To_String(Slave_List[i].Slave_Address);

            if(Current_Address == New_Address_Key){

                Already_Registered = true;
                break;

            }

        }
        
        int Seq_Frag_Num = std::stoi(Register_Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH));

        int Seq_Msg_Num = std::stoi(Register_Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH));

        std::string ACK_Packet = Build_ACK_NACK_Packet(Seq_Frag_Num,Seq_Msg_Num,'A');

        Send_UDP_Packet(Master_Socket, ACK_Packet, Sender_Address);
        
        if(Already_Registered){

            std::cout << "[INFO]: Slave already registered: " << New_Address_Key << "\n";
            continue;

        }

        Slave_Info New_Slave;

        New_Slave.Slave_ID = Slave_List.size() + 1;
        New_Slave.Slave_Address = Sender_Address;

        Slave_List.push_back(New_Slave);

        std::cout << "[OK]: Slave " << New_Slave.Slave_ID << " registered from " << Address_To_String(New_Slave.Slave_Address) << " .\n";  

    }

    return Slave_List;

}

bool Send_Message_To_Slave(int Socket_Master, char Message_Type, int Seq_Num_Msg, std::string Full_Data, sockaddr_in Slave_Address){

    std::vector<std::string> Fragments_List = Fragment_Message(Message_Type, Seq_Num_Msg, Full_Data);

    std::cout << "[INFO]: The message [" << Seq_Num_Msg << "] has " << Fragments_List.size() << " fragments.\n"; 

    for(size_t i = 0; i < Fragments_List.size(); i++){

        bool Send_Result = Send_Packet_With_ACK(Socket_Master, Fragments_List[i], Slave_Address);

        if(!Send_Result){

            std::cout << "[ERROR]: Could not send fragment [" << i << "] of message {" << Seq_Num_Msg << "}.\n";
            return false;

        }

    }

    std::cout << "[OK]: Full message " << Seq_Num_Msg << " sent correctly.\n";

    return true;

}

void Send_Message_To_Slave_Thread(Slave_Info Current_Slave, char Message_Type, int Seq_Num_Msg, std::string Full_Data, int& Result_Flag){

    int Thread_Socket = Create_UDP_Socket();

    if(Thread_Socket < 0){

        Result_Flag = 0;
        return;

    }

    if(!Set_Socket_Timeout(Thread_Socket, TIMEOUT_MS)){

        close(Thread_Socket);
        Result_Flag = 0;
        return;

    }  

    std::cout << "[INFO]: Thread Sending to slave " << Current_Slave.Slave_ID << " at " << Address_To_String(Current_Slave.Slave_Address) << "\n";
    
    int Send_Result = Send_Message_To_Slave(Thread_Socket, Message_Type, Seq_Num_Msg, Full_Data, Current_Slave.Slave_Address);

    Result_Flag = Send_Result ? 1 : 0;

    close(Thread_Socket);

}

//==================================================
//               Parsers and Dividers
//==================================================

std::string Matrix_To_String(std::vector<std::vector<double>> Matrix){

    std::stringstream String_Stream;

    for(size_t i = 0; i < Matrix.size(); i++){

        for(size_t j = 0; j < Matrix[i].size(); j++){

            String_Stream << Matrix[i][j];

            if(j < Matrix[i].size() - 1){

                String_Stream << ",";

            }

        }

        if(i < Matrix.size() - 1){

                String_Stream << ";";

        }

    }

    return String_Stream.str();

}

std::vector<std::vector<double>> String_To_Matrix(std::string Text, int Rows, int Columns){

    std::vector<std::vector<double>> Matrix;

    std::stringstream Row_Stream(Text);
    std::string Row_String;

    while(std::getline(Row_Stream, Row_String, ';')){

        if(Row_String.empty()){

            continue;

        }

        std::vector<double> Row;

        std::stringstream Column_Stream(Row_String);
        std::string Value;

        while(std::getline(Column_Stream, Value, ',')){

            if(Value.empty()){

                continue;

            }

            Row.push_back(std::stod(Value));

        }

        Matrix.push_back(Row);

    }

    if((int)Matrix.size() != Rows){

        throw std::runtime_error("Invalid number of rows");

    }

    for(size_t i = 0; i < Matrix.size(); i++){

        if((int)Matrix[i].size() != Columns){

            throw std::runtime_error("Invalid number of columns.");

        }

    }

    return Matrix;

}

int Count_CSV_Columns(std::string Line){

    if(Line.empty()){

        return 0;

    }

    int Columns = 1;

    for(char Current_Char : Line){

        if(Current_Char == ','){

            Columns++;

        }

    }

    return Columns;

}

std::vector<std::vector<std::string>> Read_CSV_Partitions(std::string Path, int Num_Partitions, int& Dataset_Columns){

    std::vector<std::vector<std::string>> Partitions(Num_Partitions);

    std::ifstream File(Path);

    if(!File.is_open()){

        std::cout << "[ERROR]: Cannot open file " << Path << "\n";
        Dataset_Columns = 0;
        return Partitions;

    }

    std::string Line;

    if(std::getline(File, Line)){

        Dataset_Columns = Count_CSV_Columns(Line);

    }

    else{

        Dataset_Columns = 0;
        return Partitions;

    }

    int Current_Partition = 0;

    while(std::getline(File, Line)){

        if(!Line.empty()){

            Partitions[Current_Partition].push_back(Line);

            Current_Partition++;

            if(Current_Partition == Num_Partitions){

                Current_Partition = 0;

            }

        }

    }

    File.close();

    return Partitions;

}

std::string Join_CSV_Rows(std::vector<std::string> Rows){

    std::string CSV_Block = "";

    for(size_t i = 0; i < Rows.size(); i++){

        CSV_Block += Rows[i];

        if(i < Rows.size() - 1){

            CSV_Block += "\n";

        }

    }

    return CSV_Block;

}

//==================================================
//              Training Message Logic
//==================================================

std::string Build_Dataset_Message(int Rows, int Columns, std::string CSV_Block){

    std::string Message = "";

    Message += Int_to_String(Rows, ROWS_LENGTH);
    Message += Int_to_String(Columns, COLUMNS_LENGTH);
    Message += CSV_Block;

    return Message;

}

std::string Build_Weights_Message(int Batch_ID, int Layer_ID, int Rows, int Columns, std::string Weights_Data){

    std::string Message = "";

    Message += Int_to_String(Batch_ID, BATCH_ID_LENGTH);
    Message += Int_to_String(Layer_ID, LAYER_ID_LENGTH);
    Message += Int_to_String(Rows, ROWS_LENGTH);
    Message += Int_to_String(Columns, COLUMNS_LENGTH);
    Message += Weights_Data;

    return Message;

}

std::string Build_Result_Weights_Message(int Batch_ID, int Layer_ID, int Rows, int Columns, std::string Updated_Weights_Data){

    std::string Message = "";

    Message += Int_to_String(Batch_ID, BATCH_ID_LENGTH);
    Message += Int_to_String(Layer_ID, LAYER_ID_LENGTH);
    Message += Int_to_String(Rows, ROWS_LENGTH);
    Message += Int_to_String(Columns, COLUMNS_LENGTH);
    Message += Updated_Weights_Data;

    return Message;

}

Dataset_Distribution Prepare_Dataset_Distribution(std::string Dataset_Path){

    Dataset_Distribution Distribution;

    Distribution.Master_CSV_Block = "";
    Distribution.Master_Rows = 0;
    Distribution.Dataset_Columns = 0;

    int Total_Workers = NUM_SLAVES + 1;

    std::vector<std::vector<std::string>> Dataset_Partitions = Read_CSV_Partitions(Dataset_Path, Total_Workers, Distribution.Dataset_Columns);

    if(Distribution.Dataset_Columns == 0){

        std::cout << "[ERROR]: Dataset columns coul not be 0.\n";
        return Distribution;

    }

    Distribution.Master_CSV_Block = Join_CSV_Rows(Dataset_Partitions[0]);
    Distribution.Master_Rows = Dataset_Partitions[0].size();
    
    std::cout << "[INFO]: Dataset block for master where:\n"
          << "Rows -> " << Distribution.Master_Rows << "\n"
          << "Columns -> " << Distribution.Dataset_Columns << "\n"
          << "Size in bytes -> " << Distribution.Master_CSV_Block.length() << ".\n";

    for(int i = 0; i < NUM_SLAVES; i++){

        std::string CSV_Block = Join_CSV_Rows(Dataset_Partitions[i + 1]);

        int Dataset_Rows = Dataset_Partitions[i + 1].size();
        
        std::string Dataset_Message = Build_Dataset_Message(Dataset_Rows, Distribution.Dataset_Columns, CSV_Block);

        Distribution.Slave_Data_Blocks.push_back(Dataset_Message);

        std::cout << "[INFO]: Dataset block for slave " << i + 1
                  << " -> Rows: " << Dataset_Rows
                  << ", Columns: " << Distribution.Dataset_Columns
                  << ", Bytes: " << Dataset_Message.length() << "\n";

    }

    return Distribution;

}

bool Send_Dataset_To_All_Slaves(std::vector<Slave_Info> Slave_List, std::vector<std::string> Data_Blocks){

    std::vector<int> Results(NUM_SLAVES, 0);
    std::vector<std::thread> Thread_List;

    for(int i = 0; i < NUM_SLAVES; i++){

        Thread_List.push_back(
            std::thread(Send_Message_To_Slave_Thread, Slave_List[i], 'B', i + 1, Data_Blocks[i], std::ref(Results[i]))
        );

    }

    for(int i = 0; i < NUM_SLAVES; i++){

        Thread_List[i].join();

    }

    bool All_Ok = true;

    for(int i = 0; i < NUM_SLAVES; i++){

        std::cout << "Slave " << i + 1 << " dataset result: "
                  << Results[i] << "\n"; 

        if(!Results[i]){

            All_Ok = false;

        }

    }

    return All_Ok;

}

std::string Receive_Message_With_ACK(int Master_Socket){

    std::map<int, std::string> Received_Fragments;

    int Expected_Fragments = -1;
    int Expected_Seq_Msg = -1;

    while(true){

        sockaddr_in Sender_Address;

        std::string Packet = Receive_UDP_Packet(Master_Socket, Sender_Address);

        if(Packet == ""){

            //std::cout << "[WARNING]: Empty packet received.\n";
            continue;

        }

        std::string Control_Frag = Packet.substr(HASH_LENGTH, CTRL_FRAG_LENGTH);

        std::string Seq_Frag_String = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);

        std::string Seq_Msg_String = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH);

        int Seq_Frag_Num = std::stoi(Seq_Frag_String);
        int Seq_Msg_Num = std::stoi(Seq_Msg_String);

        bool Hash_Ok = Verify_Packet_Hash(Packet);

        char ACK_Type = Hash_Ok ? 'A' : 'N';

        std::string ACK_NACK_Packet = Build_ACK_NACK_Packet(Seq_Frag_Num, Seq_Msg_Num, ACK_Type);

        Send_UDP_Packet(Master_Socket, ACK_NACK_Packet, Sender_Address);

        if(!Hash_Ok){

            std::cout << "[NACK]: Fragment " << Seq_Frag_Num << " was rejected.\n";
            continue;

        }

        if(Expected_Seq_Msg == -1){

            Expected_Seq_Msg = Seq_Msg_Num;

        }

        if(Seq_Msg_Num != Expected_Seq_Msg){

            std::cout << "[WARNING]: Fragment belongs to another message. Ignored.\n";
            continue;

        }

        if(Received_Fragments.find(Seq_Frag_Num) == Received_Fragments.end()){

            Received_Fragments[Seq_Frag_Num] = Packet;

        }

        else{

            std::cout << "[INFO]: Duplicate fragment " << Seq_Frag_Num << ". Ignored.\n";

        }

        if(Control_Frag == "11"){

            Expected_Fragments = Seq_Frag_Num + 1;

        }

        if(Expected_Fragments != -1 && Expected_Fragments == (int)Received_Fragments.size()){

            std::cout << "[OK]: All fragments received.\n";
            break;

        }

    }


    std::vector<std::string> Ordered_Fragments;

    for(auto Iterator = Received_Fragments.begin(); Iterator != Received_Fragments.end(); Iterator++){

        Ordered_Fragments.push_back(Iterator->second);

    }

    std::string Full_Data = Reassemble_Message(Ordered_Fragments);

    return Full_Data;

}

Weights_Result Parse_Result_Weights_Message(std::string Result_Message){

    Weights_Result Result;

    Result.Batch_ID = -1;
    Result.Layer_ID = -1;
    Result.Rows = 0;
    Result.Columns = 0;
    Result.Weights_Data = "";
    Result.Is_Valid = false;

    int Position = 0;

    if(Result_Message.length() < BATCH_ID_LENGTH + LAYER_ID_LENGTH + ROWS_LENGTH + COLUMNS_LENGTH){

        std::cout << "[ERROR]: Result message is too short.\n";
        return Result;

    }

    Result.Batch_ID = std::stoi(Result_Message.substr(Position, BATCH_ID_LENGTH));

    Position += BATCH_ID_LENGTH;

    Result.Layer_ID = std::stoi(Result_Message.substr(Position, LAYER_ID_LENGTH));

    Position += LAYER_ID_LENGTH;

    Result.Rows = std::stoi(Result_Message.substr(Position, ROWS_LENGTH));

    Position += ROWS_LENGTH;

    Result.Columns = std::stoi(Result_Message.substr(Position, COLUMNS_LENGTH));

    Position += COLUMNS_LENGTH;

    Result.Weights_Data = Result_Message.substr(Position);

    try{

        std::vector<std::vector<double>> Matrix = String_To_Matrix(Result.Weights_Data, Result.Rows, Result.Columns);

        Result.Is_Valid = true;

    }

    catch(std::exception& Error){

        std::cout << "[ERROR]: Invalid weights matrix in R message.\n";
        std::cout << Error.what() << "\n";
        Result.Is_Valid = false;

    }

    return Result;

}

std::vector<std::vector<double>> Average_Weights(std::vector<Weights_Result> Weight_Results){

    std::vector<std::vector<double>> Average_Matrix;

    int Valid_Results = 0;
    int Rows = 0;
    int Columns = 0;

    for(size_t i = 0; i < Weight_Results.size(); i++){

        if(Weight_Results[i].Is_Valid){

            Rows = Weight_Results[i].Rows;
            Columns = Weight_Results[i].Columns;
            break;

        }

    }

    if(Rows == 0 || Columns == 0){

        std::cout << "[ERROR]: No valid weight results to average.\n";
        return Average_Matrix;

    }

    Average_Matrix.resize(Rows);

    for(int i = 0; i < Rows; i++){

        Average_Matrix[i].resize(Columns, 0.0);

    }

    for(size_t Result_Index = 0; Result_Index < Weight_Results.size(); Result_Index++){

        if(!Weight_Results[Result_Index].Is_Valid){

            continue;

        }

        std::vector<std::vector<double>> Current_Matrix;

        try{

            Current_Matrix = String_To_Matrix( Weight_Results[Result_Index].Weights_Data, Weight_Results[Result_Index].Rows,
                                               Weight_Results[Result_Index].Columns
            );

        }
        catch(std::exception& Error){

            std::cout << "[ERROR]: Could not parse weights for average.\n";
            std::cout << Error.what() << "\n";
            continue;

        }

        if(Weight_Results[Result_Index].Rows != Rows ||
           Weight_Results[Result_Index].Columns != Columns){

            std::cout << "[ERROR]: Weight matrix size mismatch.\n";
            continue;

        }

        for(int i = 0; i < Rows; i++){

            for(int j = 0; j < Columns; j++){

                Average_Matrix[i][j] += Current_Matrix[i][j];

            }

        }

        Valid_Results++;

    }

    if(Valid_Results == 0){

        std::cout << "[ERROR]: No valid matrices were averaged.\n";
        Average_Matrix.clear();
        return Average_Matrix;

    }

    for(int i = 0; i < Rows; i++){

        for(int j = 0; j < Columns; j++){

            Average_Matrix[i][j] = Average_Matrix[i][j] / Valid_Results;

        }

    }

    return Average_Matrix;

}

void Train_Layer_With_Slave_Thread(Slave_Info Current_Slave, int Batch_ID, int Layer_ID,
    std::vector<std::vector<double>> Current_Weights, Weights_Result& Slave_Result, int& Result_Flag){

    Result_Flag = 0;

    Slave_Result.Batch_ID = -1;
    Slave_Result.Layer_ID = -1;
    Slave_Result.Rows = 0;
    Slave_Result.Columns = 0;
    Slave_Result.Weights_Data = "";
    Slave_Result.Is_Valid = false;

    int Thread_Socket = Create_UDP_Socket();

    if(Thread_Socket < 0){

        return;

    }

    if(!Set_Socket_Timeout(Thread_Socket, TIMEOUT_MS)){

        close(Thread_Socket);
        return;

    }

    int Rows = Current_Weights.size();
    int Columns = 0;

    if(Rows > 0){

        Columns = Current_Weights[0].size();

    }

    std::string Weights_Data = Matrix_To_String(Current_Weights);

    std::string Weights_Message = Build_Weights_Message(Batch_ID, Layer_ID, Rows, Columns, Weights_Data); 


    std::cout << "[INFO]: Sending P to slave " << Current_Slave.Slave_ID << "\n"
              << "Batch -> " << Batch_ID << "\n"
              << "Layer -> "<< Layer_ID << "\n";

    bool Send_Ok = Send_Message_To_Slave(Thread_Socket, 'P', Batch_ID, Weights_Message, Current_Slave.Slave_Address);

    if(!Send_Ok){

        std::cout << "[ERROR]: Could not send P to slave " << Current_Slave.Slave_ID << ".\n";
        close(Thread_Socket);
        return;

    }

    std::cout << "[INFO]: Waiting R from slave " << Current_Slave.Slave_ID << ".\n";

    std::string Result_Message = Receive_Message_With_ACK(Thread_Socket);

    if(Result_Message == ""){

        std::cout << "[ERROR]: Empty R received from slave " << Current_Slave.Slave_ID << ".\n";
        close(Thread_Socket);
        return;

    } 

    Weights_Result Parsed_Result = Parse_Result_Weights_Message(Result_Message);

    if(!Parsed_Result.Is_Valid){

        std::cout << "[ERROR]: Invalid R received from slave " << Current_Slave.Slave_ID << ".\n";
        close(Thread_Socket);
        return;

    }
    
    if(Parsed_Result.Batch_ID != Batch_ID){

        std::cout << "[ERROR]: R Batch_ID does not match.\n";
        close(Thread_Socket);
        return;

    }

    if(Parsed_Result.Layer_ID != Layer_ID){

        std::cout << "[ERROR]: R Layer_ID does not match.\n";
        close(Thread_Socket);
        return;

    }

    Slave_Result = Parsed_Result;
    Result_Flag = 1;

    std::cout << "[OK]: R Received correctly from slave " << Current_Slave.Slave_ID << ".\n";

    close(Thread_Socket);

}

std::vector<Weights_Result> Train_Layer_With_All_Slaves(std::vector<Slave_Info> Slave_List, int Batch_ID, int Layer_ID,
                                                        std::vector<std::vector<double>> Current_Weights){

    std::vector<Weights_Result> Slave_Results(NUM_SLAVES);
    std::vector<int> Result_Flags(NUM_SLAVES, 0);
    std::vector<std::thread> Thread_List;
    
    for(int i = 0; i < NUM_SLAVES; i++){

        Thread_List.push_back(
            std::thread(Train_Layer_With_Slave_Thread, Slave_List[i], Batch_ID, Layer_ID, Current_Weights, 
                        std::ref(Slave_Results[i]), std::ref(Result_Flags[i]))
        );

    }

    for(int i = 0; i < NUM_SLAVES; i++){

        Thread_List[i].join();

    }

    for(int i = 0; i < NUM_SLAVES; i++){

        if(Result_Flags[i]){

            std::cout << "[OK]: Slave " << i + 1 << " returned valid weights.\n";

        }

        else{

            std::cout << "[ERROR]: Slave " << i + 1 << " failed returning weights.\n";

        }

    }

    return Slave_Results;

}                                                    

bool Send_End_To_All_Slaves(std::vector<Slave_Info> Slave_List){

    std::vector<int> Results(NUM_SLAVES, 0);
    std::vector<std::thread> Thread_List;

    for(int i = 0; i < NUM_SLAVES; i++){

        Thread_List.push_back(
            std::thread(Send_Message_To_Slave_Thread, Slave_List[i], 'E', 9000 + i, "END", std::ref(Results[i]))
        );

    }

    for(int i = 0; i < NUM_SLAVES; i++){

        Thread_List[i].join();

    }

    bool All_Ok = true;

    for(int i = 0; i < NUM_SLAVES; i++){

        if(!Results[i]){

            All_Ok = false;

        }

    }

    return All_Ok;

}


//==================================================
//                     Main
//==================================================

int main(){

    int Master_Port = 5000;

    int Master_Socket = Create_UDP_Socket();

    if(Master_Socket < 0){

        return 1;

    }

    if(!Bind_UDP_Socket(Master_Socket, Master_Port)){

        close(Master_Socket);
        return 1;

    }

    std::cout << "[OK]: Master listening on port "
              << Master_Port << "\n";

    std::vector<Slave_Info> Slave_List = Register_Slaves(
        Master_Socket,
        NUM_SLAVES
    );

    close(Master_Socket);

    std::cout << "[OK]: All slaves registered.\n";

    Dataset_Distribution Distribution = Prepare_Dataset_Distribution(
        "dataset.csv"
    );

    if(Distribution.Dataset_Columns == 0){

        return 1;

    }

    bool Dataset_Send_OK = Send_Dataset_To_All_Slaves(
        Slave_List,
        Distribution.Slave_Data_Blocks
    );

    if(!Dataset_Send_OK){

        std::cout << "[ERROR]: At least one slave failed receiving dataset.\n";
        return 1;

    }

    std::cout << "[OK]: All slaves received their dataset blocks correctly.\n";

std::vector<std::vector<std::vector<double>>> Global_Weights(NUM_LAYERS);

    //Cambiar a matrices traidas del python. 

    Global_Weights[0] = {
        {0.1, 0.2},
        {0.3, 0.4}
    };

    Global_Weights[1] = {
        {0.5, 0.6},
        {0.7, 0.8}
    };

    Global_Weights[2] = {
        {0.9, 1.0},
        {1.1, 1.2}
    };

    Global_Weights[3] = {
        {1.3, 1.4},
        {1.5, 1.6}
    };

    int Total_Batches = 3;

    for(int Batch_ID = 1; Batch_ID <= Total_Batches; Batch_ID++){

        std::cout << "\n========== BATCH "
                << Batch_ID
                << " ==========\n";

        for(int Layer_Index = 0; Layer_Index < NUM_LAYERS; Layer_Index++){

            int Layer_ID = Layer_Index + 1;

            std::cout << "\n[INFO]: Training Layer "
                    << Layer_ID
                    << " of Batch "
                    << Batch_ID
                    << "\n";

            std::vector<Weights_Result> Weight_Results = Train_Layer_With_All_Slaves(
                Slave_List,
                Batch_ID,
                Layer_ID,
                Global_Weights[Layer_Index]
            );

            /*
                Master promedia los R recibidos.
            */
            std::vector<std::vector<double>> Averaged_Weights = Average_Weights(
                Weight_Results
            );

            if(Averaged_Weights.empty()){

                std::cout << "[ERROR]: Could not average weights for Layer "
                        << Layer_ID
                        << " in Batch "
                        << Batch_ID
                        << ".\n";

                continue;

            }

            Global_Weights[Layer_Index] = Averaged_Weights;

            std::cout << "[OK]: Global weights updated for Layer "
                    << Layer_ID
                    << ".\n";

            std::cout << "New global weights: "
                    << Matrix_To_String(Global_Weights[Layer_Index])
                    << "\n";

        }

    }

    bool End_OK = Send_End_To_All_Slaves(
        Slave_List
    );

    if(End_OK){

        std::cout << "[OK]: End message sent to all slaves.\n";

    }
    else{

        std::cout << "[WARNING]: Some slaves did not receive end message.\n";

    }

    return 0;

}