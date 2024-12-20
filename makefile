EXE=server
SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
DEP=$(SRC:.c=.d)
FLAGS=-Wall -g

CLIEN_EXE=client
CLIENT_SRC=$(wildcard src/client/*.c)
CLIENT_OBJ=$(CLIENT_SRC:.c=.o)
CLIENT_DEP=$(CLIENT_SRC:.c=.d)


all: $(EXE) $(CLIEN_EXE)

-include $(DEP)

$(EXE): $(OBJ)
	$(CC) $(FLAGS) $^ -o $@

CLIEN_EXE: $(CLIENT_OBJ)
	$(CC) $(FLAGS) $^ -o $@

%.o: %.c
	gcc -c $(FLAGS) -MMD $< -o $@


$(CLIEN_EXE): $(CLIENT_OBJ) 
	$(CC) $(FLAGS) $^ -o $@


clean:
	rm -f $(OBJ) $(EXE) $(DEP) $(CLIEN_EXE) $(CLIENT_OBJ)  $(CLIENT_DEP)

.PNONY: all clean