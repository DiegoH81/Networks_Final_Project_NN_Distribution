#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "wrapper_python.h"

namespace py = pybind11;



PYBIND11_MODULE(udp_module, m)
{ 
    // Master
    m.def("master_init", &py_master_init, py::arg("port"));
    m.def("register_slaves", &py_register_slaves);
    m.def("prepare_and_send_dataset", &py_prepare_and_send_dataset, py::arg("csv_path"));
    m.def("train_layer", &py_train_layer, py::arg("batch_id"), py::arg("layer_id"), py::arg("current_weights"));
    m.def("send_end", &py_send_end);
 
    // Slave
    m.def("slave_init", &py_slave_init, py::arg("master_ip"), py::arg("master_port"));
    m.def("register_slave", &py_register_slave);
    m.def("receive_dataset", &py_receive_dataset);
    m.def("receive_weights", &py_receive_weights);
    m.def("send_weights", &py_send_weights, py::arg("batch_id"), py::arg("layer_id"), py::arg("matrix"));
 
}