CC=gcc
CFLAGS=-Wall -g -std=c99 -I$(IDIR)

IDIR=include
SDIR=src
BDIR=build

TARGET=simulador_arquivos

SOURCES=$(wildcard $(SDIR)/*.c)
OBJECTS=$(patsubst $(SDIR)/%.c, $(BDIR)/%.o, $(SOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^

$(BDIR)/%.o: $(SDIR)/%.c
	@mkdir -p build
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -rf $(BDIR) $(TARGET) dados/meu_so.disk

re: clean all

run: all
	./$(TARGET)

.PHONY: all clean re run
