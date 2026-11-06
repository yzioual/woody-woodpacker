NAME = woody_woodpacker

SRC =   src/main.c \
				src/parser.c

OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))

CC = cc
CFLAGS = -Wall -Wextra -Werror -g

MAKEFLAGS += --silent

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $(NAME)
	echo "build done."

obj/%.o: src/%.c
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

stub:
	gcc -nostdlib -static -ffreestanding -fno-stack-protector -fno-pic -c stub.c -o stub.o
	objcopy -O binary --only-section=.text stub.o stub.bin

victim:
	gcc -nostdlib -static -ffreestanding -fno-stack-protector -fno-pic -fno-asynchronous-unwind-tables -fno-unwind-tables -o victim test/victim.c

sample:
	gcc -no-pie -static -o sample test/sample.c

clean:
	rm -rf obj sample victim woody stub.o stub.bin
	echo "object(s) cleaned."

fclean: clean
	rm -f $(NAME)

re: fclean all


.PHONY: all clean fclean re
