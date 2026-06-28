#ifndef MASTERUDP_H
#define MASTERUDP_H

#include <vector>
#include <map>
#include <thread>

#include <arpa/inet.h>
#include <unistd.h>

#include "UDP_base.h"
#include "CSV_utils.h"
#include "matrix_UTILS.h"

//==================================================
//               Project Structs
//==================================================

struct Slave_Info
{
    int Slave_ID;
    sockaddr_in Slave_Address;
};

struct Dataset_Distribution
{
    std::string Master_CSV_Block;
    int Master_Rows;
    int Dataset_Columns;
    std::vector<std::string> Slave_Data_Blocks;
};

struct Weights_Result
{
    int Batch_ID;
    int Layer_ID;
    int Rows;
    int Columns;
    std::string Weights_Data;
    bool Is_Valid;
};


class MasterUDP: public UDP_BASE
{
public:
    MasterUDP(int in_port, int in_expected_slaves):
        UDP_BASE(), port(in_port), all_slaves(), expected_slaves(in_expected_slaves)
    {
        priv_socket = Create_UDP_Socket();

        if(!Bind_UDP_Socket(priv_socket, port))
        {
            close(priv_socket);
            throw std::runtime_error("[ERROR]: Could not bind master socket.");
        }
        
        if(!Set_Socket_Timeout(priv_socket, TIMEOUT_MS))
        {
            close(priv_socket);
            throw std::runtime_error("[ERROR]: Could not set socket timeout.");
        }

        std::cout << "[OK]: Master listening on port " << port << "\n";
    }

    void Register_Slaves()
    {
        std::cout << "[INFO]: Waiting for: "  << expected_slaves << " slaves...\n";

        while((int)all_slaves.size() < expected_slaves)
        {
            sockaddr_in Sender_Address;

            std::string Register_Packet = Receive_UDP_Packet(priv_socket, Sender_Address);

            if(Register_Packet == "")
            {
                std::cout << "[WARNING]: Invalid register packet received.\n";
                continue;
            }

            if(!Verify_Packet_Hash(Register_Packet))
            {
                std::cout << "[WARNING]: Register Packet hash invalid.\n";
                continue;
            }

            std::string Payload = Register_Packet.substr(HEADER_LENGTH, PAYLOAD_LENGTH);

            char Message_Type = Payload[0];

            if(Message_Type != 'L')
            {
                std::cout << "[WARNING]: Packet ignored. Expected register type 'L'\n";
                continue;
            }

            bool Already_Registered = false;

            std::string New_Address_Key = Address_To_String(Sender_Address);

            // Check REGISTERED
            for(size_t i = 0; i < all_slaves.size(); i++)
            {
                std::string Current_Address = Address_To_String(all_slaves[i].Slave_Address);

                if(Current_Address == New_Address_Key){

                    Already_Registered = true;
                    break;

                }
            }
            
            int Seq_Frag_Num = std::stoi(Register_Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH, SEQ_NUM_FRAG_LENGTH));

            int Seq_Msg_Num = std::stoi(Register_Packet.substr(HASH_LENGTH + CTRL_FRAG_LENGTH + SEQ_NUM_FRAG_LENGTH, SEQ_NUM_MSG_LENGTH));

            std::string ACK_Packet = Build_ACK_NACK_Packet(Seq_Frag_Num, Seq_Msg_Num, 'A');

            Send_UDP_Packet(priv_socket, ACK_Packet, Sender_Address);
            
            if(Already_Registered)
            {
                std::cout << "[INFO]: Slave already registered: " << New_Address_Key << "\n";
                continue;
            }

            Slave_Info New_Slave;

            New_Slave.Slave_ID = all_slaves.size() + 1;
            New_Slave.Slave_Address = Sender_Address;

            all_slaves.push_back(New_Slave);

            std::cout << "[OK]: Slave " << New_Slave.Slave_ID << " registered from " << Address_To_String(New_Slave.Slave_Address) << " .\n";  
        }
    }

    std::string prepare_and_send_dataset(std::string CSV_path)
    {
        Dataset_Distribution Distribution = Prepare_Dataset_Distribution(CSV_path);

        if(Distribution.Dataset_Columns == 0)
            throw std::runtime_error("[ERROR]: Could not prepare dataset.");

        bool Ok = Send_Dataset_To_All_Slaves(all_slaves, Distribution.Slave_Data_Blocks);

        if(!Ok)
            throw std::runtime_error("[ERROR]: At least one slave failed receiving dataset.");

        return Distribution.Master_CSV_Block;
    }

    std::vector<std::vector<double>> py_train_layer(int Batch_ID, int Layer_ID, std::vector<std::vector<double>> Current_Weights)
    {
        std::vector<Weights_Result> Results = Train_Layer_With_All_Slaves( all_slaves, Batch_ID, Layer_ID, Current_Weights );

        std::vector<std::vector<double>> Averaged = Average_Weights(Results);

        if(Averaged.empty())
            throw std::runtime_error("[ERROR]: Could not average weights.");

        return Averaged;

    }

    bool Send_End_To_All_Slaves()
    {
        std::vector<int> Results(NUM_SLAVES, 0);
        std::vector<std::thread> Thread_List;

        for(int i = 0; i < NUM_SLAVES; i++)
            Thread_List.push_back( std::thread(&MasterUDP::Send_Message_To_Slave_Thread, this, all_slaves[i], 'E', 9000 + i, "END", std::ref(Results[i])) );

        for(int i = 0; i < NUM_SLAVES; i++)
            Thread_List[i].join();

        bool All_Ok = true;

        for(int i = 0; i < NUM_SLAVES; i++)
        {
            if(!Results[i])
                All_Ok = false;

        }

        return All_Ok;
    }

private:
    int port, expected_slaves;
    std::vector<Slave_Info> all_slaves;

    bool Send_Message_To_Slave(int Socket_Master, char Message_Type, int Seq_Num_Msg, std::string Full_Data, sockaddr_in Slave_Address)
    {
        std::vector<std::string> Fragments_List = Fragment_Message(Message_Type, Seq_Num_Msg, Full_Data);

        std::cout << "[INFO]: The message [" << Seq_Num_Msg << "] has " << Fragments_List.size() << " fragments.\n"; 

        for(size_t i = 0; i < Fragments_List.size(); i++)
        {
            bool Send_Result = Send_Packet_With_ACK(Socket_Master, Fragments_List[i], Slave_Address);

            if(!Send_Result)
            {
                std::cout << "[ERROR]: Could not send fragment [" << i << "] of message {" << Seq_Num_Msg << "}.\n";
                return false;
            }
        }

        std::cout << "[OK]: Full message " << Seq_Num_Msg << " sent correctly.\n";

        return true;
    }

    void Send_Message_To_Slave_Thread(Slave_Info Current_Slave, char Message_Type, int Seq_Num_Msg, std::string Full_Data, int& Result_Flag)
    {
        int Thread_Socket = Create_UDP_Socket();

        if(Thread_Socket < 0)
        {
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
    //              Training Message Logic
    //==================================================

    std::string Build_Dataset_Message(int Rows, int Columns, std::string CSV_Block)
    {
        std::string Message = "";

        Message += Int_to_String(Rows, ROWS_LENGTH);
        Message += Int_to_String(Columns, COLUMNS_LENGTH);
        Message += CSV_Block;

        return Message;
    }

    std::string Build_Weights_Message(int Batch_ID, int Layer_ID, int Rows, int Columns, std::string Weights_Data)
    {
        std::string Message = "";

        Message += Int_to_String(Batch_ID, BATCH_ID_LENGTH);
        Message += Int_to_String(Layer_ID, LAYER_ID_LENGTH);
        Message += Int_to_String(Rows, ROWS_LENGTH);
        Message += Int_to_String(Columns, COLUMNS_LENGTH);
        Message += Weights_Data;

        return Message;
    }

    Dataset_Distribution Prepare_Dataset_Distribution(std::string Dataset_Path)
    {
        Dataset_Distribution Distribution;

        Distribution.Master_CSV_Block = "";
        Distribution.Master_Rows = 0;
        Distribution.Dataset_Columns = 0;

        int Total_Workers = NUM_SLAVES + 1;

        std::vector<std::vector<std::string>> Dataset_Partitions = Read_CSV_Partitions(Dataset_Path, Total_Workers, Distribution.Dataset_Columns);

        if(Distribution.Dataset_Columns == 0)
        {
            std::cout << "[ERROR]: Dataset columns can not be 0.\n";
            return Distribution;
        }

        Distribution.Master_CSV_Block = Join_CSV_Rows(Dataset_Partitions[0]);
        Distribution.Master_Rows = Dataset_Partitions[0].size();
        
        std::cout << "[INFO]: Dataset block for master where:\n"
                  << "Rows -> " << Distribution.Master_Rows << "\n"
                  << "Columns -> " << Distribution.Dataset_Columns << "\n"
                  << "Size in bytes -> " << Distribution.Master_CSV_Block.length() << ".\n";

        for(int i = 0; i < NUM_SLAVES; i++)
        {
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

    bool Send_Dataset_To_All_Slaves(std::vector<Slave_Info> Slave_List, std::vector<std::string> Data_Blocks)
    {
        std::vector<int> Results(NUM_SLAVES, 0);
        std::vector<std::thread> Thread_List;

        for(int i = 0; i < NUM_SLAVES; i++)
        {
            Thread_List.push_back(
                std::thread(&MasterUDP::Send_Message_To_Slave_Thread, this, Slave_List[i], 'B', i + 1, Data_Blocks[i], std::ref(Results[i]))
            );
        }

        for(int i = 0; i < NUM_SLAVES; i++)
            Thread_List[i].join();


        bool All_Ok = true;

        for(int i = 0; i < NUM_SLAVES; i++)
        {
            std::cout << "Slave " << i + 1 << " dataset result: "
                      << Results[i] << "\n"; 

            if(!Results[i])
                All_Ok = false;
        }

        return All_Ok;
    }

    std::string Receive_Message_With_ACK(int Master_Socket)
    {
        std::map<int, std::string> Received_Fragments;

        int Expected_Fragments = -1;
        int Expected_Seq_Msg = -1;

        while(true)
        {
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

            if(Received_Fragments.find(Seq_Frag_Num) == Received_Fragments.end())
                Received_Fragments[Seq_Frag_Num] = Packet;
            else
                std::cout << "[INFO]: Duplicate fragment " << Seq_Frag_Num << ". Ignored.\n";


            if(Control_Frag == "11")
                Expected_Fragments = Seq_Frag_Num + 1;

            if(Expected_Fragments != -1 && Expected_Fragments == (int)Received_Fragments.size())
            {
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

    Weights_Result Parse_Result_Weights_Message(std::string Result_Message)
    {
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

    std::vector<std::vector<double>> Average_Weights(std::vector<Weights_Result> Weight_Results)
    {
        std::vector<std::vector<double>> Average_Matrix;

        int Valid_Results = 0;
        int Rows = 0;
        int Columns = 0;

        for(size_t i = 0; i < Weight_Results.size(); i++)
        {
            if(Weight_Results[i].Is_Valid){

                Rows = Weight_Results[i].Rows;
                Columns = Weight_Results[i].Columns;
                break;

            }
        }

        if(Rows == 0 || Columns == 0)
        {
            std::cout << "[ERROR]: No valid weight results to average.\n";
            return Average_Matrix;
        }

        Average_Matrix.resize(Rows);

        for(int i = 0; i < Rows; i++)
            Average_Matrix[i].resize(Columns, 0.0);
        

        for(size_t Result_Index = 0; Result_Index < Weight_Results.size(); Result_Index++)
        {
            if(!Weight_Results[Result_Index].Is_Valid)
                continue;

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

            if(Weight_Results[Result_Index].Rows != Rows || Weight_Results[Result_Index].Columns != Columns)
            {
                std::cout << "[ERROR]: Weight matrix size mismatch.\n";
                continue;
            }

            for(int i = 0; i < Rows; i++)
            {
                for(int j = 0; j < Columns; j++)
                    Average_Matrix[i][j] += Current_Matrix[i][j];

            }

            Valid_Results++;
        }

        if(Valid_Results == 0){

            std::cout << "[ERROR]: No valid matrices were averaged.\n";
            Average_Matrix.clear();
            return Average_Matrix;

        }

        for(int i = 0; i < Rows; i++)
        {
            for(int j = 0; j < Columns; j++)         
                Average_Matrix[i][j] = Average_Matrix[i][j] / Valid_Results;
        }

        return Average_Matrix;
    }

    void Train_Layer_With_Slave_Thread(Slave_Info Current_Slave, int Batch_ID, int Layer_ID,
                                       std::vector<std::vector<double>> Current_Weights, Weights_Result& Slave_Result,
                                       int& Result_Flag)
    {

        Result_Flag = 0;

        Slave_Result.Batch_ID = -1;
        Slave_Result.Layer_ID = -1;
        Slave_Result.Rows = 0;
        Slave_Result.Columns = 0;
        Slave_Result.Weights_Data = "";
        Slave_Result.Is_Valid = false;

        int Thread_Socket = Create_UDP_Socket();

        if(Thread_Socket < 0)
            return;

        if(!Set_Socket_Timeout(Thread_Socket, TIMEOUT_MS)){

            close(Thread_Socket);
            return;

        }

        int Rows = Current_Weights.size();
        int Columns = 0;

        if(Rows > 0)
            Columns = Current_Weights[0].size();

        std::string Weights_Data = Matrix_To_String(Current_Weights);

        std::string Weights_Message = Build_Weights_Message(Batch_ID, Layer_ID, Rows, Columns, Weights_Data); 


        std::cout << "[INFO]: Sending P to slave " << Current_Slave.Slave_ID << "\n"
                  << "Batch -> " << Batch_ID << "\n"
                  << "Layer -> "<< Layer_ID << "\n";

        bool Send_Ok = Send_Message_To_Slave(Thread_Socket, 'P', Batch_ID, Weights_Message, Current_Slave.Slave_Address);

        if(!Send_Ok)
        {
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
                                                            std::vector<std::vector<double>> Current_Weights)
    {
        std::vector<Weights_Result> Slave_Results(NUM_SLAVES);
        std::vector<int> Result_Flags(NUM_SLAVES, 0);
        std::vector<std::thread> Thread_List;
        
        for(int i = 0; i < NUM_SLAVES; i++)
        {
            Thread_List.push_back( std::thread(&MasterUDP::Train_Layer_With_Slave_Thread, this, Slave_List[i], Batch_ID, Layer_ID, Current_Weights, 
                                               std::ref(Slave_Results[i]), std::ref(Result_Flags[i])) );
        }

        for(int i = 0; i < NUM_SLAVES; i++)
            Thread_List[i].join();

        for(int i = 0; i < NUM_SLAVES; i++)
        {

            if(Result_Flags[i])
                std::cout << "[OK]: Slave " << i + 1 << " returned valid weights.\n";
            else
                std::cout << "[ERROR]: Slave " << i + 1 << " failed returning weights.\n";

        }

        return Slave_Results;
    }

    bool Bind_UDP_Socket(int Socket_Master, int Port)
    {
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
};

#endif