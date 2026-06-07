#include <iostream>
#include <vector>
#include <string>
#include <sstream>

std::string matrixToString(
    const std::vector<std::vector<double>>& matrix
)
{
    if (matrix.empty())
        return "0|0|";

    int rows = matrix.size();
    int cols = matrix[0].size();

    std::stringstream ss;

    ss << rows << "|" << cols << "|";

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            ss << matrix[i][j];

            if (j < cols - 1)
                ss << ",";
        }

        if (i < rows - 1)
            ss << ";";
    }

    return ss.str();
}

std::vector<std::vector<double>>
stringToMatrix(const std::string& text)
{
    std::vector<std::vector<double>> matrix;

    size_t p1 = text.find('|');
    size_t p2 = text.find('|', p1 + 1);

    if (p1 == std::string::npos ||
        p2 == std::string::npos)
    {
        throw std::runtime_error("Formato invalido");
    }

    int rows = std::stoi(text.substr(0, p1));
    int cols = std::stoi(
        text.substr(
            p1 + 1,
            p2 - p1 - 1
        )
    );

    std::string data =
        text.substr(p2 + 1);

    std::stringstream rowStream(data);

    std::string rowStr;

    while (std::getline(rowStream, rowStr, ';'))
    {
        std::vector<double> row;

        std::stringstream colStream(rowStr);

        std::string value;

        while (std::getline(colStream, value, ','))
        {
            row.push_back(
                std::stod(value)
            );
        }

        matrix.push_back(row);
    }

    if (matrix.size() != rows)
        throw std::runtime_error(
            "Numero de filas incorrecto"
        );

    for (auto& row : matrix)
    {
        if (row.size() != cols)
        {
            throw std::runtime_error(
                "Numero de columnas incorrecto"
            );
        }
    }

    return matrix;
}

int main()
{
    std::vector<std::vector<double>> original = {
        {6, 148, 72, 35, 0, 33.6, 0.627, 50, 1, 0, 0, 0, 0, 0} ,
        {1, 85, 66, 29, 0, 26.6, 0.351, 31, 0, 1, 0, 0, 0, 0},
        {8, 183, 64, 0, 0, 23.3, 0.672, 32, 0, 0, 1, 0, 0, 0}
    };

    std::string s =
        matrixToString(original);

    std::cout << s << std::endl;

    auto recovered =
        stringToMatrix(s);

    for (auto& row : recovered)
    {
        for (double x : row)
            std::cout << x << " ";

        std::cout << std::endl;
    }

    return 0;
}
