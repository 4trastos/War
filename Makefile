NAME = War
CC = gcc
AS = nasm
CFLAGS = -Wall -Werror -Wextra -g3 #-fsanitize=address
ASFLAGS = -f elf64
RM = rm -f

SRC = src/main.c src/infection.c src/protection.c src/auxiliar.c
OBJT = $(SRC:.c=.o)

PAYLOAD_SRC = src/payload.s
PAYLOAD_OBJ = src/payload.o
PAYLOAD_BIN = src/payload.bin
PAYLOAD_HDR = src/payload.h

all: $(NAME)

$(NAME): $(OBJT) $(PAYLOAD_BIN)
	$(CC) $(CFLAGS) $(OBJT) -o $(NAME)

$(PAYLOAD_OBJ): $(PAYLOAD_SRC)
	$(AS) $(ASFLAGS) $(PAYLOAD_SRC) -o $(PAYLOAD_OBJ)

$(PAYLOAD_BIN): $(PAYLOAD_OBJ)
	objcopy -O binary $(PAYLOAD_OBJ) $(PAYLOAD_BIN)

$(PAYLOAD_HDR): $(PAYLOAD_BIN)
	xxd -i $(PAYLOAD_BIN) > $(PAYLOAD_HDR)

src/infection.o: src/infection.c $(PAYLOAD_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJT) $(PAYLOAD_OBJ) $(PAYLOAD_BIN) $(PAYLOAD_HDR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re