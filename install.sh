#!/bin/bash

# Define functions
function error() {
  echo -e "$(tput setaf 1)$(tput bold)$1$(tput sgr 0)"
  exit 1
}

function warning() {
  echo -e "$(tput setaf 1)$(tput bold)$1$(tput sgr 0)"
  sleep 2
  true
}



# Install depends if not already installed
read -p "Install depends wget and gnupg2 (y/n)?" choice
case "$choice" in
  y|Y ) install-depends;;
  n|N ) echo "Need depends to install! Exiting..." && exit 1;;
  * ) echo "invalid";;
esac

# Install repo
wget -qO- https://github.com/mobilegmYT/mcpi-packages-extended/raw/main/debs/KEY.gpg | sudo apt-key add - || error "Failed to download and add key!"
wget -qO- https://github.com/mobilegmYT/mcpi-packages-extended/raw/main/debs/mcpi-revival.list | sudo tee /etc/apt/sources.list.d/mcpi-reborn-extended.list || error "Failed to download 'mcpi-revival.list'!"

sudo apt update --allow-releaseinfo-change || warning "Failed to run 'sudo apt update'! Please run that command manually"

# Nuke vanilla reborn if installed
if [ $(dpkg-query -W -f='${Status}' minecraft-pi-reborn-client 2>/dev/null | grep -c "ok installed") -eq 0 ];
then
  sudo apt-get uninstall -y minecraft-pi-reborn-client && sudo apt-get install minecraft-pi-reborn-client || warning "Could not reinstall reborn to switch to extended version! Please do it manually."
fi

# Install modified python lib
read -p "Install MCPI python API (y/n)?" choice
case "$choice" in 
  y|Y ) install-api;;
  n|N ) break;;
  * ) echo "invalid";;
esac

# Install sound
wget https://archive.org/download/libminecraftpe0.6.1/libminecraftpe06%2B08.so || warning "Failed to download sound files!"
mkdir -p ~/.minecraft-pi/overrides
mv libminecraftpe06+08.so ~/.minecraft-pi/overrides/libminecraftpe.so

echo "Installation success! Enjoy the game!"
