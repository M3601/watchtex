CC = g++
CFLAGS =
CFLAGS += -Wall -Wextra
CFLAGS += -std=c++20
CFLAGS += -Iinclude
LDFLAGS =
LDFLAGS += -lrt
LDFLAGS += -lm
LDFLAGS += -lc
DBGFLAGS =
DBGFLAGS += -O0 -g3
DBGFLAGS += -fsanitize=address,undefined
DBGFLAGS += -fstack-protector-strong
DBGFLAGS += -DDEBUG
DBGFLAGS += -lfmt
RLSFLAGS =
RLSFLAGS += -O3
RLSFLAGS += -static
RLSFLAGS += -DFMT_HEADER_ONLY

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
NAME = main
RLSNAME = watchtex


debug: CFLAGS += $(DBGFLAGS)
debug: all
	./$(NAME)

release: CFLAGS += $(RLSFLAGS)
release: all
	mv $(NAME) $(RLSNAME)

all: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(OBJ)

%.o: %.cpp
	$(CC) $< $(CFLAGS) -c -o $@

src/clone3.o: src/clone3.cpp src/clone3.s
	$(CC) src/clone3.cpp $(CFLAGS) -c -o $@.1
	$(CC) src/clone3.s $(CFLAGS) -c -o $@.2
	ld -r $@.1 $@.2 -o $@
	rm $@.1 $@.2

format:
	clang-format -i src/*.cpp include/*.hpp

install: release
	cp $(RLSNAME) /usr/local/bin/$(RLSNAME)