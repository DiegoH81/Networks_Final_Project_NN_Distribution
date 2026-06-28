#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "MasterUDP.h"
#include "SlaveUDP.h"

namespace py = pybind11;


PYBIND11_MODULE(udp_module, m)
{ 
    // Master
    py::class_<MasterUDP>(m, "MasterUDP")
        .def(py::init<int, int>(), py::arg("port"), py::arg("expected_slaves"))
        .def("register_slaves", &MasterUDP::Register_Slaves)
        .def("prepare_and_send_dataset", &MasterUDP::prepare_and_send_dataset, py::arg("csv_path"))
        .def("train_layer", &MasterUDP::py_train_layer, py::arg("batch_id"), py::arg("layer_id"), py::arg("current_weights"))
        .def("send_end", &MasterUDP::Send_End_To_All_Slaves);
 
    // Slave
    py::class_<SlaveUDP>(m, "SlaveUDP")
        .def(py::init<std::string, int>(), py::arg("master_ip"), py::arg("master_port"))
        .def("register_slave", &SlaveUDP::Register_Slave_To_Master)
        .def("receive_dataset", &SlaveUDP::receieve_dataset)
        .def("receive_weights", &SlaveUDP::receieve_weights)
        .def("send_weights", &SlaveUDP::send_weights, py::arg("batch_id"), py::arg("layer_id"), py::arg("matrix"));
 
}