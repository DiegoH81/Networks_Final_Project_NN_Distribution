#ifndef SLAVE_H
#define SLAVE_H

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

#include "function_utils.h"
#include "UDP_utils.h"
#include "matrix_temp.h"

//==================================================
//               Project Structs
//==================================================

struct Weights_Message {

    int Batch_ID;
    int Layer_ID;
    int Rows;
    int Columns;
    std::string Weights_Data;
    bool Is_Valid;

};


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

/*
int main()
{

    std::string Master_IP = "127.0.0.1";
    int Master_Port = 45000;

    int Slave_Socket = Create_UDP_Socket();

    if(Slave_Socket < 0)
        return 1;
        

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

    while(true)
    {

        sockaddr_in Message_Sender_Address;

        std::string Message_Text = Receive_Message_With_ACK(
            Slave_Socket,
            Message_Sender_Address
        );

        if(Message_Text == ""){

            std::cout << "[WARNING]: Empty training message received.\n";
            continue;

        }

        
            Por ahora Receive_Message_With_ACK devuelve solo el contenido lógico,
            no devuelve el tipo P/E directamente.

            Entonces para terminar de forma limpia necesitamos una convención:
            - Si Message_Text == "END", terminamos.
            - Si no, lo tratamos como mensaje de pesos P.
        

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
*/
#endif