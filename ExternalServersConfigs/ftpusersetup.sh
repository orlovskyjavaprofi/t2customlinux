#!/bin/bash

# Configuration
USER="filehoster"
PASS="temporarypassword" # Replace with your desired throwaway password
FTP_DIR="/mnt/t2-trunk/ftp"
FTP_DIR2="/home/filehoster/ftp"
# 1. Create the user if it doesn't exist
if ! id "$USER" &>/dev/null; then
    useradd -m "$USER"
    echo "$USER:$PASS" | chpasswd
    echo "User $USER created."
else
    echo "User $USER already exists, skipping user creation."
fi

# 2. Setup the FTP directory
# If you want to use the home dir instead, change FTP_DIR to "/home/$USER/ftp"
mkdir -p "$FTP_DIR"
mkdir -p "$FTP_DIR2"

# 3. Apply permissions (nobody:nogroup is standard for restricted FTP)
chown nobody:nogroup "$FTP_DIR"
chmod a-w "$FTP_DIR"

chown nobody:nogroup "$FTP_DIR2"
chmod a-w "$FTP_DIR2"

# 4. Verify
echo "FTP directory setup complete at $FTP_DIR"
ls -la "$FTP_DIR"
ls -la "$FTP_DIR2"