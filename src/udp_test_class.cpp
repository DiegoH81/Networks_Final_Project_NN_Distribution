#include <pybind11/pybind11.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <cstring>

#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <chrono>


class UdpServer
{
private:
    int socketFD;
    bool is_running;

    std::map<std::string, struct sockaddr_in> clients;

    std::vector<std::string> message_buffer;

public:
    UdpServer(int port):
        is_running(true)
    {
        socketFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        struct sockaddr_in stSockAddr;
        memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
        stSockAddr.sin_family = AF_INET;
        stSockAddr.sin_port = htons(port);
        stSockAddr.sin_addr.s_addr = INADDR_ANY;

        bind(socketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in));

        std::thread(&UdpServer::receive, this).detach();
    }

    ~UdpServer()
    {
        close(socketFD);
        is_running = false;
    }

    void send_msg(std::string msg)
    {
        if (msg.length() > 100) // Crop a 100 caracteres
            msg = msg.substr(0, 100);
        else if (msg.length() < 100) // Relleno con '#'   
            msg.append(100 - msg.length(), '#');
        
        for (auto &c : clients)
            sendto(socketFD, msg.c_str(), 100, 0, (const struct sockaddr *)&c.second, sizeof(c.second));
    }

    void receive()
    {
        while (is_running)
        {
            char buffer[101]; // 100 + null terminator
            memset(buffer, 0, sizeof(buffer));
            struct sockaddr_in sender_addr;
            socklen_t addr_size = sizeof(sender_addr);
            
            ssize_t recv_size = recvfrom(socketFD, buffer, 100, 0, (struct sockaddr*)&sender_addr, &addr_size);
            if (recv_size > 0)
            {
                buffer[recv_size] = '\0';

                std::string ip = inet_ntoa(sender_addr.sin_addr);
                int port = ntohs(sender_addr.sin_port);
                std::string client_id = ip + ":" + std::to_string(port);

                if (clients.find(client_id) == clients.end())
                    clients[client_id] = sender_addr;


                std::string to_push(buffer);

                
                message_buffer.push_back(to_push);

                std::cout << "Receieved msg " << message_buffer.size() << "\n";
            }
        }
        
    }

    std::string receive_full_msg()
    {
        while (message_buffer.size() < 1 && is_running)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::string to_return;
        for (auto& msg : message_buffer)
            to_return += msg;
        

        message_buffer.clear();
        
        return to_return;
    }

    std::string receieve_hello()
    {
        while (message_buffer.size() < 1 && is_running)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::string to_return = message_buffer.front();
        message_buffer.clear();
        
        return to_return;
    }
};


class UdpClient
{
private:
    int socketFD;
    bool is_running;
    struct sockaddr_in serverAddr;
    std::string last_message_receieved;
public:
    UdpClient(const std::string& ip, int port) :
        is_running(true), last_message_receieved()
    {
        socketFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        memset(&serverAddr, 0, sizeof(struct sockaddr_in));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());

        std::thread(&UdpClient::receive, this).detach();
    }

    ~UdpClient()
    {
        close(socketFD);
        is_running = false;
    }

    // Recibe string de Python, lo formatea a 100 bytes y lo manda por socket
    void send_msg(std::string msg)
    {
        if (msg.length() > 100) // Crop a 100 caracteres
            msg = msg.substr(0, 100);
        else if (msg.length() < 100) // Relleno con '#'   
            msg.append(100 - msg.length(), '#');
        
        sendto(socketFD, msg.c_str(), 100, 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
    }

    std::string receive_latest_msg()
    {
        while (last_message_receieved.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::string to_return = last_message_receieved;
        last_message_receieved.clear();
        return to_return;
    }

    void receive()
    {
        char buffer[101];
        memset(buffer, 0, sizeof(buffer));
        struct sockaddr_in from_addr;
        socklen_t addr_size = sizeof(from_addr);
        
        while (is_running)
        {
            ssize_t recv_size = recvfrom(socketFD, buffer, 100, 0, (struct sockaddr*)&from_addr, &addr_size);
    
            if (recv_size > 0)
            {
                buffer[recv_size] = '\0';
                std::string to_print(buffer);

                std::string ending_str = "exit";
                ending_str.resize(100, '#');

                if (to_print == ending_str)
                    return;

                last_message_receieved = to_print;
                std::cout << to_print << "\n";
            }
        }
    }
};