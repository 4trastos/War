NAME = War
CC = gcc
CFLAGS = -Wall -Werror -Wextra
RM = rm -f

SRC = src/main.c src/infection.c src/protection.c
OBJT = $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJT)
	$(CC) $(CFLAGS) $(OBJT) -o $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJT)

fclean: clean
	$(RM) $(NAME)

re: fclean all

PHONY: all clean fclean re
