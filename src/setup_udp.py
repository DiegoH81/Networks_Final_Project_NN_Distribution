from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "udp_module",
        sources=["bindings_udp.cpp"], 
        include_dirs=[ pybind11.get_include()],
        language='c++'
    ),
]

setup(
    name="udp_module",
    version="0.1",
    ext_modules=ext_modules,
    install_requires=['pybind11>=2.10.0'],
    python_requires=">=3.6",
)