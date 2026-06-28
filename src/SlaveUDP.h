#ifndef SLAVEUDP_H
#define SLAVEUDP_H

#include <map>
#include <tuple>

#include "UDP_base.h"
#include "matrix_UTILS.h"

struct Weights_Message
{
    int Batch_ID;
    int Layer_ID;
    int Rows;
    int Columns;
    std::string Weights_Data;
    bool Is_Valid;
};

class SlaveUDP: public UDP_BASE
{
public:

    SlaveUDP(std::string in_server_ip, int in_port) :
        UDP_BASE(in_port)
    {
        priv_socket = Create_UDP_Socket();
        //Set_Socket_Timeout(priv_socket, TIMEOUT_MS);
        master_Address = Create_Address(in_server_ip, connection_port);
    }

    std::string receieve_dataset()
    {
        sockaddr_in Sender;
        std::string Dataset_Block = Receive_Message_With_ACK(priv_socket, Sender);

        if(Dataset_Block.empty())
            throw std::runtime_error("[ERROR]: Empty dataset block received.");

        return Dataset_Block.substr(ROWS_LENGTH + COLUMNS_LENGTH);
    }

    bool Register_Slave_To_Master()
    {
        std::vector<std::string> Register_Fragments = Fragment_Message('L', 0, "");

        if(Register_Fragments.empty())
        {
            std::cout << "[ERROR]: Could not build register packet.\n";
            return false;
        }

        bool Send_Result = Send_UDP_Packet(priv_socket, Register_Fragments[0], master_Address);

        if(!Send_Result)
        {
            std::cout << "[ERROR]: Could not send register packet.\n";
            return false;
        }

        sockaddr_in Sender_Address;

        std::string ACK_Packet = Receive_UDP_Packet(priv_socket, Sender_Address);

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

    std::tuple<int, int, std::vector<std::vector<double>>> receieve_weights()
    {
        std::string Message = Receive_Message_With_ACK(priv_socket, last_sender_Address);

        if(Message == "END")
            return {-1, -1, {}};

        Weights_Message Parsed = Parse_Weights_Message(Message);

        if(!Parsed.Is_Valid)
            throw std::runtime_error("[ERROR]: Invalid P message received.");

        std::vector<std::vector<double>> Matrix = String_To_Matrix( Parsed.Weights_Data, Parsed.Rows, Parsed.Columns );

        return {Parsed.Batch_ID, Parsed.Layer_ID, Matrix};
    }

    void send_weights(int Batch_ID, int Layer_ID, std::vector<std::vector<double>> Matrix)
    {
        int Rows = Matrix.size();
        int Columns = Rows > 0 ? Matrix[0].size() : 0;

        std::string Weights_Data = Matrix_To_String(Matrix);
        std::string Result_Message = Build_Result_Weights_Message( Batch_ID, Layer_ID, Rows, Columns, Weights_Data );

        bool Ok = Send_Message_To_Master(priv_socket, 'R', Batch_ID, Result_Message, last_sender_Address);

        if(!Ok)
            throw std::runtime_error("[ERROR]: Could not send weights to master.");
    }
private:
    sockaddr_in master_Address;
    sockaddr_in last_sender_Address;

    Weights_Message Parse_Weights_Message(std::string Weights_Message_Text)
    {
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

    std::string Receive_Message_With_ACK(int Slave_Socket, sockaddr_in& Message_Sender_Address)
    {
        std::map<int, std::string> Received_Fragments;

        int Expected_Fragments = -1;
        int Expected_Seq_Msg = -1;

        while(true)
        {
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

        for (const auto& item : Received_Fragments)
            Ordered_Fragments.push_back(item.second);

        std::string Full_Data = Reassemble_Message(Ordered_Fragments);

        return Full_Data;
    }

    std::string Build_Result_Weights_Message(int Batch_ID, int Layer_ID, int Rows, int Columns, std::string Updated_Weights_Data)
    {
        std::string Message = "";

        Message += Int_to_String(Batch_ID, BATCH_ID_LENGTH);
        Message += Int_to_String(Layer_ID, LAYER_ID_LENGTH);
        Message += Int_to_String(Rows, ROWS_LENGTH);
        Message += Int_to_String(Columns, COLUMNS_LENGTH);
        Message += Updated_Weights_Data;

        return Message;
    }

    bool Send_Message_To_Master(int Socket_Master, char Message_Type, int Seq_Num_Msg, std::string Full_Data, sockaddr_in Master_Address)
    {
        std::vector<std::string> Fragments_List = Fragment_Message(Message_Type, Seq_Num_Msg, Full_Data);

        std::cout << "[INFO]: The message [" << Seq_Num_Msg << "] has " << Fragments_List.size() << " fragments.\n"; 

        for(size_t i = 0; i < Fragments_List.size(); i++)
        {
            bool Send_Result = Send_Packet_With_ACK(Socket_Master, Fragments_List[i], Master_Address);

            if(!Send_Result)
            {
                std::cout << "[ERROR]: Could not send fragment [" << i << "] of message {" << Seq_Num_Msg << "}.\n";
                return false;
            }
        }

        std::cout << "[OK]: Full message " << Seq_Num_Msg << " sent correctly.\n";

        return true;
    }
};

#endif