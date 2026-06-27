#ifndef CSV_TEMP_H
#define CSV_TEMP_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

//==================================================
//               Parsers and Dividers
//==================================================

inline int Count_CSV_Columns(std::string Line){

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

inline std::vector<std::vector<std::string>> Read_CSV_Partitions(std::string Path, int Num_Partitions, int& Dataset_Columns){

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

inline std::string Join_CSV_Rows(std::vector<std::string> Rows){

    std::string CSV_Block = "";

    for(size_t i = 0; i < Rows.size(); i++){

        CSV_Block += Rows[i];

        if(i < Rows.size() - 1){

            CSV_Block += "\n";

        }

    }

    return CSV_Block;

}

#endif