import numpy as np


def matrix_to_string(matrix):

    arr = np.asarray(matrix)

    rows, cols = arr.shape

    data = ";".join(
        ",".join(str(float(x)) for x in row)
        for row in arr
    )

    return f"{rows}|{cols}|{data}"


def string_to_matrix(text):

    rows_str, cols_str, data = text.split("|", 2)

    rows = int(rows_str)
    cols = int(cols_str)

    matrix = []

    for row in data.split(";"):

        matrix.append(
            [float(x) for x in row.split(",")]
        )

    matrix = np.array(matrix)

    if matrix.shape != (rows, cols):
        raise ValueError(
            f"Dimensiones inválidas. Esperado {(rows, cols)} "
            f"pero llegó {matrix.shape}"
        )

    return matrix

X = np.array([
    [6,148,72,35,0,33.6,0.627,50,1,0,0,0,0,0],
    [1,85,66,29,0,26.6,0.351,31,0,1,0,0,0,0],
    [8,183,64,0,0,23.3,0.672,32,0,0,1,0,0,0]
])

s = matrix_to_string(X)

print(s)

m = string_to_matrix(s)

print(m)
