CC=/usr/local/cs/bin/g++
CFLAGS=-Wall -Wextra -g -static-libstdc++
NAME=http-get
UID=805167986
FILES=$(NAME).cpp README Makefile

default: $(NAME).cpp
	$(CC) $(CFLAGS) $(NAME).cpp -o $(NAME)

dist: $(FILES)
	tar -czvf $(UID).tar.gz $(FILES)

clean:
	rm -f $(NAME)
	rm -f $(UID).tar.gz
