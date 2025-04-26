CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGETS = server client

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f $(TARGETS)

setup:
	@echo "Setting up required directories and permissions..."
	mkdir -p /tmp/fileserver/Manufacturing
	mkdir -p /tmp/fileserver/Distribution
	@echo "Creating groups if they don't exist..."
	-getent group Manufacturing > /dev/null || groupadd Manufacturing
	-getent group Distribution > /dev/null || groupadd Distribution
	@echo "Setting proper permissions..."
	chmod 770 /tmp/fileserver/Manufacturing
	chmod 770 /tmp/fileserver/Distribution
	chgrp Manufacturing /tmp/fileserver/Manufacturing
	chgrp Distribution /tmp/fileserver/Distribution
	@echo "Setup complete."

create_users:
	@echo "Creating test users..."
	-useradd -m -G Manufacturing manufacturing_user1
	-useradd -m -G Manufacturing manufacturing_user2
	-useradd -m -G Distribution distribution_user
	@echo "Setting test passwords..."
	@echo "Please run these commands manually with sudo:"
	@echo "sudo passwd manufacturing_user1"
	@echo "sudo passwd manufacturing_user2"
	@echo "sudo passwd distribution_user"
	@echo "User creation complete."

.PHONY: all clean setup create_users