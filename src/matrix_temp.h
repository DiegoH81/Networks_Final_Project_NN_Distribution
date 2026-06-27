#ifndef MATRIX_TEMP_H
#define MATRIX_TEMP_H

#include <string>
#include <vector>
#include <sstream>

//==================================================
//               Parsers and Dividers
//==================================================

inline std::string Matrix_To_String(std::vector<std::vector<double>> Matrix){

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

inline std::vector<std::vector<double>> String_To_Matrix(std::string Text, int Rows, int Columns){

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


inline std::vector<std::vector<double>> Update_Weights_For_Test(std::vector<std::vector<double>> Weights){

    for(size_t i = 0; i < Weights.size(); i++){

        for(size_t j = 0; j < Weights[i].size(); j++){

            Weights[i][j] += 0.01;

        }

    }

    return Weights;

}



#endif