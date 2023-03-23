#! /usr/sh
# install_sdk.sh
# author: seung hee, lee
# purpose: get 'esp-idf' and 'esp-matter' repository from github
# reference: https://docs.espressif.com/projects/esp-matter/en/main/esp32/developing.html#development-setup

cur_path=${PWD}
if [[ "$OSTYPE" == "darwin"* ]]; then
    project_path=$(dirname $(dirname $(realpath $0)))
else 
    project_path=$(dirname $(dirname $(realpath $BASH_SOURCE)))
fi
esp_idf_path=${project_path}/sdk/esp-idf
esp_matter_path=${project_path}/sdk/esp-matter

# for Apple silicon
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:"/opt/homebrew/opt/openssl@3/lib/pkgconfig"

# prepare esp-idf
cd ${esp_idf_path}
git submodule update --init --recursive
bash ./install.sh

# prepare esp-matter and CHIP
cd ${esp_matter_path}
git submodule update --init
cd connectedhomeip/connectedhomeip

# optional: checkout another CHIP commit
git fetch --all --tags
git checkout tags/v1.0.0.2

. ./scripts/activate.sh

cd ${esp_matter_path}
bash ./install.sh

# maybe obsolete anymore...
#source ${esp_idf_path}/export.sh
#pip install lark stringcase

cd ${cur_path}