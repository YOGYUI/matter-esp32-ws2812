# Matter Light Example (ESP32 + WS2812)
Matter 조명(Light) 예제 프로젝트<br>
조명과 관련된 다음 3종류의 Matter 클러스터들에 대한 코드 구현 방법을 알아본다
- OnOff
- LevelControl
- ColorControl

SDK
---
- esp-idf: [v5.0.1](https://github.com/espressif/esp-idf/tree/v5.0.1)
- esp-matter: [f8105768252e89dd6bd5b5bb9b0f3f2118b0edff](https://github.com/espressif/esp-matter/commit/f8105768252e89dd6bd5b5bb9b0f3f2118b0edff)
- connectedhomeip: [04b2fdfc6f7259116a50170b0d38dab9507e20d3](https://github.com/project-chip/connectedhomeip/commit/04b2fdfc6f7259116a50170b0d38dab9507e20d3)
  - Matter 1.1 released (2023.05.18)

Helper Script
---
SDK 클론 및 설치
```shell
source ./scripts/install_sdk.sh
```
SDK (idf.py) 준비
```shell
source ./scripts/prepare_sdk.sh
```

Flash
---
idf.py 준비된 상태에서 sdkconfig 파일 생성 (프로젝트 폴더 내 sdkconfig.defaults 파일을 기반으로 생성)
```shell
idf.py set-target esp32
```
DAC Provider 플래시
connectedhomeip의 예제 Attestation 중 Vendor ID 0x**FFF2**, Product ID 0x**8001**에 대한 DAC Provider Factory 바이너리 파일을 ESP32에 플래시해준다
```shell
source ./scripts/flash_factory_dac_provider.h
```
소스코드 빌드 및 플래시
```shell
idf.py -p {시리얼포트명} flash monitor
```

QR Code for commisioning
---
![qrcode.png](./resource/DACProvider/qrcode.png)

References
---
[Matter::OnOff 클러스터 예제 (ESP32)](https://yogyui.tistory.com/entry/PROJ-MatterOn-Off-Light-%ED%81%B4%EB%9F%AC%EC%8A%A4%ED%84%B0-%EC%98%88%EC%A0%9C-ESP32)<br>
[Matter::LevelControl 클러스터 예제 (ESP32)](https://yogyui.tistory.com/entry/PROJ-MatterLevelControl-%ED%81%B4%EB%9F%AC%EC%8A%A4%ED%84%B0-%EA%B0%9C%EB%B0%9C-%EC%98%88%EC%A0%9C-ESP32)<br>
[Matter::ColorControl 클러스터 예제 (ESP32)](https://yogyui.tistory.com/entry/PROJ-MatterColorControl-%ED%81%B4%EB%9F%AC%EC%8A%A4%ED%84%B0-%EA%B0%9C%EB%B0%9C-%EC%98%88%EC%A0%9C-ESP32)<br>
