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

struct Weights_Message{

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

    int Socket_Slave = socket(AF_INET, SOCK_DGRAM, 0);

    if(Socket_Slave < 0){

        std::cout << "[ERROR]: Could not create UDP Socket.\n";
        return -1; 

    }

    return Socket_Slave;

}

sockaddr_in Create_Address(std::string IP_Address, int Port){

    sockaddr_in Address;
    std::memset(&Address, 0, sizeof(Address));

    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = inet_addr(IP_Address.c_str());

    return Address;

}

bool Send_UDP_Packet(int Socket_Slave, std::string Packet, sockaddr_in Destination_Address){

    if(Packet.length() != PACKET_LENGTH){

        std::cout << "[ERROR]: Cannot send packet with invalid length.\n";
        return false;

    }

    int Bytes_Sent = sendto(Socket_Slave, Packet.c_str(), PACKET_LENGTH, 0, (sockaddr*)&Destination_Address, sizeof(Destination_Address));

    if(Bytes_Sent != PACKET_LENGTH){

        std::cout << "[ERROR]: Could not send complete UDP Packet.\n";
        return false;

    }

    return true;

}

std::string Receive_UDP_Packet(int Socket_Slave, sockaddr_in& Sender_Address){

    char Buffer[PACKET_LENGTH];

    socklen_t Sender_Length = sizeof(Sender_Address);

    int Bytes_Received = recvfrom(Socket_Slave, Buffer, PACKET_LENGTH, 0, (sockaddr*)&Sender_Address, &Sender_Length);

    if(Bytes_Received <= 0){

        return "";

    }


    if(Bytes_Received != PACKET_LENGTH){

        std::cout << "[ERROR]: Could not receive UDP Packet.\n";
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

        std::cout << "[Timeout]: Could not set socket timeout.\n";
        return false;

    }

    return true;

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


std::vector<std::vector<double>> Update_Weights_For_Test(std::vector<std::vector<double>> Weights){

    for(size_t i = 0; i < Weights.size(); i++){

        for(size_t j = 0; j < Weights[i].size(); j++){

            Weights[i][j] += 0.01;

        }

    }

    return Weights;

}



//==================================================
//                  Slave Logic
//==================================================

bool Register_Slave_To_Master(int Slave_Socket, sockaddr_in Master_Address){

    std::vector<std::string> Register_Fragments = Fragment_Message('L', 0, "");

    if(Register_Fragments.empty()){

        std::cout << "[ERROR]: Could not build register packet.\n";
        return false;

    }

    bool Send_Result = Send_UDP_Packet(Slave_Socket, Register_Fragments[0], Master_Address);

    if(!Send_Result){

        std::cout << "[ERROR]: Could not send register packet.\n";
        return false;

    }

    sockaddr_in Sender_Address;

    std::string ACK_Packet = Receive_UDP_Packet(Slave_Socket, Sender_Address);

    if(ACK_Packet == ""){

        std::cout << "[ERROR]: No ACK received for register packet.\n";
        return false;

    }

    if(!Verify_Packet_Hash(ACK_Packet)){

        std::cout << "[ERROR]: Register ACK packet hash invalid.\n";
        return false;

    }

    std::string ACK_Payload = ACK_Packet.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

    if(ACK_Payload[0] != 'A'){

        std::cout << "[ERROR]: Register was declined by the master.\n";
        return false;

    }

    std::cout << "[OK]: Slave registered correctly.\n";
    return true;

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

Weights_Message Parse_Weights_Message(std::string Weights_Message_Text){

    Weights_Message Result;

    Result.Batch_ID = -1;
    Result.Layer_ID = -1;
    Result.Rows = 0;
    Result.Columns = 0;
    Result.Weights_Data = "";
    Result.Is_Valid = false;

    int Position = 0;

    if(Weights_Message_Text.length() < BATCH_ID_LENGTH + LAYER_ID_LENGTH + ROWS_LENGTH + COLUMNS_LENGTH){

        std::cout << "[ERROR]: Weights message is too short.\n";
        return Result;

    }

    Result.Batch_ID = std::stoi(Weights_Message_Text.substr(Position, BATCH_ID_LENGTH));

    Position += BATCH_ID_LENGTH;

    Result.Layer_ID = std::stoi(Weights_Message_Text.substr(Position, LAYER_ID_LENGTH));

    Position += LAYER_ID_LENGTH;

    Result.Rows = std::stoi(Weights_Message_Text.substr(Position, ROWS_LENGTH));

    Position += ROWS_LENGTH;

    Result.Columns = std::stoi(Weights_Message_Text.substr(Position, COLUMNS_LENGTH));

    Position += COLUMNS_LENGTH;

    Result.Weights_Data = Weights_Message_Text.substr(Position);

    try{

        std::vector<std::vector<double>> Matrix = String_To_Matrix(Result.Weights_Data, Result.Rows, Result.Columns);

        Result.Is_Valid = true;

    }

    catch(std::exception& Error){

        std::cout << "[ERROR]: Invalid weights matrix in P message.\n";
        std::cout << Error.what() << "\n";
        Result.Is_Valid = false;

    }

    return Result;

}

std::string Receive_Message_With_ACK(int Slave_Socket, sockaddr_in& Message_Sender_Address){

    std::map<int, std::string> Received_Fragments;

    int Expected_Fragments = -1;
    int Expected_Seq_Msg = -1;

    while(true){

        sockaddr_in Sender_Address;

        std::string Packet = Receive_UDP_Packet(Slave_Socket, Sender_Address);

        if(Packet == ""){

            //std::cout << "[WARNING]: Empty packet received.\n";
            continue;

        }

        Message_Sender_Address = Sender_Address;

        std::string Control_Frag = Packet.substr(HASH_LENGTH, CTRL_FRAG_LENGTH);

        std::string Seq_Frag_String = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);

        std::string Seq_Msg_String = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH);

        int Seq_Frag_Num = std::stoi(Seq_Frag_String);
        int Seq_Msg_Num = std::stoi(Seq_Msg_String);

        bool Hash_Ok = Verify_Packet_Hash(Packet);

        char ACK_Type = Hash_Ok ? 'A' : 'N';

        std::string ACK_NACK_Packet = Build_ACK_NACK_Packet(Seq_Frag_Num, Seq_Msg_Num, ACK_Type);

        Send_UDP_Packet(Slave_Socket, ACK_NACK_Packet, Sender_Address);

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

bool Send_Packet_With_ACK(int Slave_Socket, std::string Packet, sockaddr_in Destination_Address){

    std::string Seq_Frag = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH);
    std::string Seq_Msg = Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH);

    int Retry_Count = 0;

    while(Retry_Count < MAX_RETRIES){

        bool Send_Result = Send_UDP_Packet(Slave_Socket, Packet, Destination_Address);

        if(!Send_Result){

            std::cout << "[ERROR]: Packet send failed. Retry: " << Retry_Count << ".\n";
            Retry_Count++;

            continue;
            
        }

        sockaddr_in Sender_Address;

        std::string Response_Packet = Receive_UDP_Packet(Slave_Socket, Sender_Address);

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

bool Send_Message_To_Master(int Socket_Master, char Message_Type, int Seq_Num_Msg, std::string Full_Data, sockaddr_in Master_Address){

    std::vector<std::string> Fragments_List = Fragment_Message(Message_Type, Seq_Num_Msg, Full_Data);

    std::cout << "[INFO]: The message [" << Seq_Num_Msg << "] has " << Fragments_List.size() << " fragments.\n"; 

    for(size_t i = 0; i < Fragments_List.size(); i++){

        bool Send_Result = Send_Packet_With_ACK(Socket_Master, Fragments_List[i], Master_Address);

        if(!Send_Result){

            std::cout << "[ERROR]: Could not send fragment [" << i << "] of message {" << Seq_Num_Msg << "}.\n";
            return false;

        }

    }

    std::cout << "[OK]: Full message " << Seq_Num_Msg << " sent correctly.\n";

    return true;

}

bool Process_Weights_Message(int Slave_Socket, sockaddr_in Master_Address, std::string Weights_Message_Text){

    Weights_Message Message = Parse_Weights_Message(Weights_Message_Text);

    if(!Message.Is_Valid){

        std::cout << "[ERROR]: Invalid P message received.\n";
        return false;
        
    }

    std::cout << "[OK]: P received.\n"
              << "Batch -> " << Message.Batch_ID << "\n"
              << "Layer -> " << Message.Layer_ID << "\n"
              << "Rows -> " << Message.Rows << "\n"
              << "Columns -> " << Message.Columns << "\n";

    std::vector<std::vector<double>> Weights = String_To_Matrix(Message.Weights_Data, Message.Rows, Message.Columns);

    //Cambiar por pesos entrenados aqui.

    std::vector<std::vector<double>> Updated_Weights = Update_Weights_For_Test(Weights);

    std::string Updated_Weights_Data = Matrix_To_String(Updated_Weights);

    std::string Result_Message = Build_Result_Weights_Message(Message.Batch_ID, Message.Layer_ID, Message.Rows, Message.Columns, Updated_Weights_Data);

    bool Send_Ok = Send_Message_To_Master(Slave_Socket, 'R', Message.Batch_ID, Result_Message, Master_Address);

    if(!Send_Ok){

        std::cout << "[ERROR]: Could not send R to master.\n";
        return false;

    }

    std::cout << "[OK]: R sent to master.\n";

    return true;

}


//==================================================
//                     Main
//==================================================

int main(){

    std::string Master_IP = "127.0.0.1";
    int Master_Port = 5000;

    int Slave_Socket = Create_UDP_Socket();

    if(Slave_Socket < 0){

        return 1;

    }

    if(!Set_Socket_Timeout(Slave_Socket, TIMEOUT_MS)){

        close(Slave_Socket);
        return 1;

    }

    sockaddr_in Master_Address = Create_Address(Master_IP, Master_Port);

    if(!Register_Slave_To_Master(Slave_Socket, Master_Address)){

        close(Slave_Socket);
        return 1;

    }

    std::cout << "[INFO]: Waiting for dataset block from master...\n";

    sockaddr_in Dataset_Sender_Address;

    std::string Dataset_Block = Receive_Message_With_ACK(Slave_Socket, Dataset_Sender_Address);

    if(Dataset_Block == ""){

        std::cout << "[ERROR]: Dataset block could not be received.\n";
        close(Slave_Socket);
        return 1;

    }

    int Rows = std::stoi(
        Dataset_Block.substr(0, ROWS_LENGTH)
    );

    int Columns = std::stoi(
        Dataset_Block.substr(ROWS_LENGTH, COLUMNS_LENGTH)
    );

    std::string CSV_Block = Dataset_Block.substr(
        ROWS_LENGTH + COLUMNS_LENGTH
    );

    std::cout << "[OK]: Dataset block received.\n";
    std::cout << "Rows: " << Rows << "\n";
    std::cout << "Columns: " << Columns << "\n";
    std::cout << "CSV block size: " << CSV_Block.length() << "\n";

std::cout << "[INFO]: Waiting for training messages from master...\n";

while(true){

    sockaddr_in Message_Sender_Address;

    std::string Message_Text = Receive_Message_With_ACK(
        Slave_Socket,
        Message_Sender_Address
    );

    if(Message_Text == ""){

        std::cout << "[WARNING]: Empty training message received.\n";
        continue;

    }

    /*
        Por ahora Receive_Message_With_ACK devuelve solo el contenido lógico,
        no devuelve el tipo P/E directamente.

        Entonces para terminar de forma limpia necesitamos una convención:
        - Si Message_Text == "END", terminamos.
        - Si no, lo tratamos como mensaje de pesos P.
    */

    if(Message_Text == "END"){

        std::cout << "[OK]: End message received. Closing slave.\n";
        break;

    }

    bool Process_OK = Process_Weights_Message(
        Slave_Socket,
        Message_Sender_Address,
        Message_Text
    );

    if(!Process_OK){

        std::cout << "[ERROR]: Could not process weights message.\n";
        continue;

    }

}

close(Slave_Socket);

return 0;

}