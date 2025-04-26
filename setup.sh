#!/bin/bash
# Setup script for File Transfer System
# Must be run as root

# Check if running as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit 1
fi

echo "Setting up File Transfer System..."

# Create groups
echo "Creating groups..."
groupadd -f Manufacturing
groupadd -f Distribution

# Create users
echo "Creating users..."
useradd -m -G Manufacturing manufacturing_user1 2>/dev/null || echo "User manufacturing_user1 already exists"
useradd -m -G Manufacturing manufacturing_user2 2>/dev/null || echo "User manufacturing_user2 already exists"
useradd -m -G Distribution distribution_user 2>/dev/null || echo "User distribution_user already exists"

# Set passwords (in a production environment, you'd use a more secure method)
echo "Setting passwords..."
echo "manufacturing_user1:password1" | chpasswd
echo "manufacturing_user2:password2" | chpasswd
echo "distribution_user:password3" | chpasswd

# Create directories
echo "Creating directories..."
mkdir -p /tmp/fileserver/Manufacturing
mkdir -p /tmp/fileserver/Distribution

# Set permissions
echo "Setting permissions..."
chmod 770 /tmp/fileserver/Manufacturing
chmod 770 /tmp/fileserver/Distribution
chgrp Manufacturing /tmp/fileserver/Manufacturing
chgrp Distribution /tmp/fileserver/Distribution

echo "Setup complete!"
echo "You may need to log out and log back in for group changes to take effect."
echo ""
echo "Test users created:"
echo "- manufacturing_user1 (password: password1) - Manufacturing group"
echo "- manufacturing_user2 (password: password2) - Manufacturing group"
echo "- distribution_user (password: password3) - Distribution group"