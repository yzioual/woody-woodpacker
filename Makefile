NAME = woody_woodpacker
SRC =   src/main.c \
        src/chacha20.c \
				src/parser.c

OBJ = $(SRC:.c=.o)
CXX = cc
CXXFLAGS = -Wall -Wextra -Werror -g
MAKEFLAGS += --silent -j

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $(NAME)
	echo "build done."

clean:
	@rm -f $(OBJ)
	echo "object(s) cleaned."

fclean: clean
	@rm -f $(NAME)

re:
	$(MAKE) fclean
	$(MAKE) all
	$(MAKE) clean

.PHONY: all clean fclean re
