ifndef OBJ_DIR
OBJ_DIR = ${PWD}/obj
export OBJ_DIR
endif

ifndef DST_DIR
DST_DIR = ${PWD}/dst
export DST_DIR
endif

all: client.Makefile worker.Makefile AMQP-CPP/Makefile
	make -C AMQP-CPP
	mkdir -p ${OBJ_DIR}/AMQP-CPP
	make PREFIX=${OBJ_DIR}/AMQP-CPP install -C AMQP-CPP
	make -f client.Makefile
	make -f worker.Makefile

install: client.Makefile worker.Makefile AMQP-CPP/Makefile
	make -C AMQP-CPP
	mkdir -p ${OBJ_DIR}/AMQP-CPP
	make PREFIX=${OBJ_DIR}/AMQP-CPP install -C AMQP-CPP
	make PREFIX=${DST_DIR} install -C AMQP-CPP
	make -f client.Makefile install
	make -f worker.Makefile install

clean: client.Makefile worker.Makefile AMQP-CPP/Makefile
	make -f client.Makefile clean
	make -f worker.Makefile clean
	make clean -C AMQP-CPP

purge:
	rm $(OBJ_DIR) -rf
