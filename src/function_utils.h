#ifndef FUNCTION_UTILS_H
#define FUNCTION_UTILS_H

#include <vector>
#include <string>
#include <iostream>
#include <arpa/inet.h>

#include "definitions.h"

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

inline std::string Address_To_String(sockaddr_in Address){

    std::string IP_Address = inet_ntoa(Address.sin_addr);
    int Port = ntohs(Address.sin_port);

    return IP_Address + ":" + std::to_string(Port);

}

//==================================================
//                 Hash Function
//==================================================

inline unsigned int Calculate_CRC32(std::string Payload)
{
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

inline sockaddr_in Create_Address(std::string IP_Address, int Port)
{
    sockaddr_in Address;
    std::memset(&Address, 0, sizeof(Address));

    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = inet_addr(IP_Address.c_str());

    return Address;

}

#endif