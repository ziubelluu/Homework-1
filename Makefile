CC = gcc
CFLAGS = -Wall -Wextra -g
INCLUDES = -I./include
OBJDIR = obj
BINDIR = bin
TARGET = $(BINDIR)/myPreCompiler

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRC))

all: $(TARGET)

$(TARGET): $(OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR):
	mkdir $(OBJDIR)

$(BINDIR):
	mkdir $(BINDIR)

clean:
	rmdir /s /q $(OBJDIR) $(BINDIR)

.PHONY: all clean 