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

# sdk_path=${project_path}/sdk
sdk_path=~/tools  # change to your own sdk path
if ! [ -d "${sdk_path}" ]; then
  mkdir ${sdk_path}
fi

esp_idf_path=${sdk_path}/esp-idf
esp_matter_path=${sdk_path}/esp-matter

# for Apple silicon
if [[ "$OSTYPE" == "darwin"* ]]; then
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:"/opt/homebrew/opt/openssl@3/lib/pkgconfig"
fi

# install esp-idf
cd ${sdk_path}
git clone --recursive https://github.com/espressif/esp-idf.git esp-idf
cd ${esp_idf_path}
git fetch --all --tags
git checkout v5.0.1
git submodule update --init --recursive
bash ./install.sh

# clone esp-matter repository and install connectedhomeip submodules      
cd ${sdk_path}
git clone --depth 1 https://github.com/espressif/esp-matter.git esp-matter
cd ${esp_matter_path}
git reset --hard f8105768252e89dd6bd5b5bb9b0f3f2118b0edff
git submodule update --init --depth 1
cd ./connectedhomeip/connectedhomeip
if [[ "$OSTYPE" == "darwin"* ]]; then
  ./scripts/checkout_submodules.py --platform esp32 darwin --shallow
else
  ./scripts/checkout_submodules.py --platform esp32 linux --shallow
fi

# install esp-matter and chip cores
cd ${esp_matter_path}
bash ./install.sh  # will call connectedhomeip "activate.sh"

source ${esp_idf_path}/export.sh
pip install lark stringcase jinja2

# create symbolic link
cd ${project_path}
if ! [ -d "${project_path}/sdk" ]; then
  mkdir ${project_path}/sdk
fi
ln -s ${esp_idf_path} ${project_path}/sdk/esp-idf
ln -s ${esp_matter_path} ${project_path}/sdk/esp-matter

cd ${cur_path}