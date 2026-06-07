#include <pybind11/pybind11.h>
#include "udp_test_class.cpp"

namespace py = pybind11;


PYBIND11_MODULE(udp_module, m)
{
    m.doc() = "test UDP client-server";

    py::class_<UdpServer>(m, "UdpServer")
        .def(py::init<int>())
        .def("receive", &UdpServer::receive)
        .def("receive_full_msg", &UdpServer::receive_full_msg)
        .def("receieve_hello", &UdpServer::receieve_hello)
        .def("send_msg", &UdpServer::send_msg);

    py::class_<UdpClient>(m, "UdpClient")
        .def(py::init<std::string, int>())
        .def("send_msg", &UdpClient::send_msg)
        .def("receive_latest_msg", &UdpClient::receive_latest_msg);
}