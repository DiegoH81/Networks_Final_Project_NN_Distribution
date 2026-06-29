#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "MasterUDP.h"
#include "SlaveUDP.h"
#include "matrix_UTILS.h"

namespace py = pybind11;


PYBIND11_MODULE(udp_module, m)
{ 
    // Master
    py::class_<MasterUDP>(m, "MasterUDP")
        .def(py::init<int, int, bool>(), py::arg("port"), py::arg("expected_slaves"), py::arg("simulation"))
        .def("register_slaves", &MasterUDP::Register_Slaves)
        .def("prepare_and_send_dataset", &MasterUDP::prepare_and_send_dataset, py::arg("csv_path"))
        .def("send_weights_to_slaves", &MasterUDP::Send_Weight_All_Slaves, py::arg("batch"), py::arg("layer"), py::arg("weights"))
        .def("receive_weights_from_slaves", &MasterUDP::Receive_Weights_From_All_Slaves, py::arg("batch"), py::arg("layer"))
        .def("send_end", &MasterUDP::Send_End_To_All_Slaves);
 
    // Slave
    py::class_<SlaveUDP>(m, "SlaveUDP")
        .def(py::init<std::string, int, bool>(), py::arg("master_ip"), py::arg("master_port"), py::arg("simulation"))
        .def("register_slave", &SlaveUDP::Register_Slave_To_Master)
        .def("receive_dataset", &SlaveUDP::receieve_dataset)
        .def("receive_weights", &SlaveUDP::receieve_weights)
        .def("send_weights", &SlaveUDP::send_weights, py::arg("batch_id"), py::arg("layer_id"), py::arg("matrix"));
 
    m.def("average_weights", &Average_weights, 
        py::arg("server_matrix"), 
        py::arg("slaves_matrix"), 
        py::arg("in_workers"),
        "Average matrix between slaves and master.");
}