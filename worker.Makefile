include Makefile.defs

TARGET = worker

CXXSRCS = \
	worker.cpp \
	modbus_tools/Master.cpp \
	modbus_tools/SerialPort.cpp \
	modbus_tools/crc.cpp \
	modbus_tools/json.cpp

include Makefile.rules
