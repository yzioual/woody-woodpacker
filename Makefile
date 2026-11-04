NAME = woody_woodpacker

SRC =   src/main.c \
        src/chacha20.c

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

clean:
	rm -rf obj
	echo "object(s) cleaned."

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re