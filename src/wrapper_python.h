#ifndef WRAPPER_PYTHON_H
#define WRAPPER_PYTHON_H

#include "MasterUDP.h"
#include "SlaveUDP.h"


//==================================================
//         Wrappers para tipos no compatibles
//==================================================

static int G_Master_Socket = -1;
static std::vector<Slave_Info> G_Slave_List;

static int G_Slave_Socket = -1;
static sockaddr_in G_Master_Address, G_Last_Sender_Address;

MasterUDP temp_master;
SlaveUDP temp_slave;

// MASTER

void py_master_init(int Port)
{
    G_Master_Socket = temp_master.Create_UDP_Socket();

    if(G_Master_Socket < 0)
        throw std::runtime_error("[ERROR]: Could not create master socket.");

    if(!temp_master.Bind_UDP_Socket(G_Master_Socket, Port))
    {
        close(G_Master_Socket);
        throw std::runtime_error("[ERROR]: Could not bind master socket.");
    }

    if(!temp_master.Set_Socket_Timeout(G_Master_Socket, TIMEOUT_MS))
    {
        close(G_Master_Socket);
        throw std::runtime_error("[ERROR]: Could not set socket timeout.");
    }

    std::cout << "[OK]: Master listening on port " << Port << "\n";

}

void py_register_slaves()
{
    G_Slave_List = temp_master.Register_Slaves(G_Master_Socket, NUM_SLAVES);
    close(G_Master_Socket);
    G_Master_Socket = -1;

}

std::string py_prepare_and_send_dataset(std::string CSV_Path)
{

    Dataset_Distribution Distribution = temp_master.Prepare_Dataset_Distribution(CSV_Path);

    if(Distribution.Dataset_Columns == 0)
        throw std::runtime_error("[ERROR]: Could not prepare dataset.");

    bool Ok = temp_master.Send_Dataset_To_All_Slaves(G_Slave_List, Distribution.Slave_Data_Blocks);

    if(!Ok)
        throw std::runtime_error("[ERROR]: At least one slave failed receiving dataset.");


    return Distribution.Master_CSV_Block;

}

std::vector<std::vector<double>> py_train_layer(int Batch_ID, int Layer_ID, std::vector<std::vector<double>> Current_Weights)
{
    std::vector<Weights_Result> Results = temp_master.Train_Layer_With_All_Slaves(
        G_Slave_List, Batch_ID, Layer_ID, Current_Weights
    );

    std::vector<std::vector<double>> Averaged = temp_master.Average_Weights(Results);

    if(Averaged.empty())
        throw std::runtime_error("[ERROR]: Could not average weights.");

    return Averaged;

}

void py_send_end()
{
    temp_master.Send_End_To_All_Slaves(G_Slave_List);
}


// SLAVE

void py_slave_init(std::string Master_IP, int Master_Port)
{

    G_Slave_Socket = temp_slave.Create_UDP_Socket();

    if(G_Slave_Socket < 0)
        throw std::runtime_error("[ERROR]: Could not create slave socket.");

    if(!temp_slave.Set_Socket_Timeout(G_Slave_Socket, TIMEOUT_MS))
    {
        close(G_Slave_Socket);
        throw std::runtime_error("[ERROR]: Could not set socket timeout.");
    }

    G_Master_Address = temp_slave.Create_Address(Master_IP, Master_Port);

}

void py_register_slave()
{

    bool Ok = temp_slave.Register_Slave_To_Master(G_Slave_Socket, G_Master_Address);

    if(!Ok)
        throw std::runtime_error("[ERROR]: Could not register slave.");

}

std::string py_receive_dataset()
{
    sockaddr_in Sender;
    std::string Dataset_Block = temp_slave.Receive_Message_With_ACK(G_Slave_Socket, Sender);

    if(Dataset_Block.empty())
        throw std::runtime_error("[ERROR]: Empty dataset block received.");

    return Dataset_Block.substr(ROWS_LENGTH + COLUMNS_LENGTH);

}

std::tuple<int, int, std::vector<std::vector<double>>> py_receive_weights()
{
    sockaddr_in Sender;

    //std::string Message = Receive_Message_With_ACK(G_Slave_Socket, Sender);
    std::string Message = temp_slave.Receive_Message_With_ACK(G_Slave_Socket, G_Last_Sender_Address);

    if(Message == "END")
        return {-1, -1, {}};

    Weights_Message Parsed = temp_slave.Parse_Weights_Message(Message);

    if(!Parsed.Is_Valid)
        throw std::runtime_error("[ERROR]: Invalid weights message received.");

    std::vector<std::vector<double>> Matrix = String_To_Matrix( Parsed.Weights_Data, Parsed.Rows, Parsed.Columns );

    return {Parsed.Batch_ID, Parsed.Layer_ID, Matrix};

}

void py_send_weights(int Batch_ID, int Layer_ID, std::vector<std::vector<double>> Matrix)
{
    int Rows = Matrix.size();
    int Columns = Rows > 0 ? Matrix[0].size() : 0;

    std::string Weights_Data = Matrix_To_String(Matrix);
    std::string Result_Message = temp_slave.Build_Result_Weights_Message( Batch_ID, Layer_ID, Rows, Columns, Weights_Data );

    //bool Ok = Send_Message_To_Master(G_Slave_Socket, 'R', Batch_ID, Result_Message, G_Master_Address);
    bool Ok = temp_slave.Send_Message_To_Master(G_Slave_Socket, 'R', Batch_ID, Result_Message, G_Last_Sender_Address);

    if(!Ok)
        throw std::runtime_error("[ERROR]: Could not send weights to master.");

}

#endif