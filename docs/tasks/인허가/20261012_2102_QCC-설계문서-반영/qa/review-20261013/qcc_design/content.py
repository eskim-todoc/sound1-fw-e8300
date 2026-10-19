"""Functional descriptions. Source correspondence belongs in local QA evidence."""
import json
from pathlib import Path
ROOT=Path(__file__).parent
UNITS=json.loads((ROOT/'unit-source-contract.json').read_text(encoding='utf-8'))
TESTS=json.loads((ROOT.parent/'qcc_tests/test-contract.json').read_text(encoding='utf-8'))
TEST_MAP={1:[4],2:[5,4],3:[6],4:[7,8],5:[1],6:[2],7:[3],8:[10,8],9:[9,8],10:[11],11:[12],12:[13],13:[14],14:[15]}
PURPOSES=[
 'QCC의 주변장치와 라이브러리를 정해진 순서로 준비하여 상태 처리와 통신이 유효한 초기 설정에서 시작되도록 한다.',
 'E8300의 연결/종료 신호와 전원/무선 상태를 관리하고 BLE와 SPI 사이에서 명령과 응답을 중계한다.',
 '생산 시 결정한 Standalone 또는 TWS 구성에 따라 오디오와 제어 데이터의 처리 대상을 선택한다.',
 '내부기 부착 상태와 Bluetooth 연결 상태를 연결하여 검색/연결 가능 여부와 외부기 장치명을 관리한다.',
 'QCC의 디지털 입력, 출력과 주변장치 기능을 설정하고 입력 변화를 상태 처리에 전달한다.',
 '배터리 전압과 충전 연결을 잔량/전원 상태로 변환하여 시스템에 전달하고 저전압과 종료 요청을 처리한다.',
 'Application의 비휘발성 데이터를 식별자와 길이로 접근하여 외부기 설정과 제조 정보를 유지한다.',
 'Bluetooth Classic 오디오 연결을 관리하여 수신 오디오는 I2S로 E8300에 전달하고 통화 중 DMIC2 입력은 상대 장치로 송신한다.',
 'BLE 광고와 연결 상태를 관리하여 외부 앱이 장치를 식별하고 인증/데이터 서비스에 접근할 수 있게 한다.',
 'CRA의 Challenge/Response/Confirm/Status 교환을 GATT 특성에 연결하여 인증 계산과 무선 전송을 분리한다.',
 'BLE 메시지의 상태, 주소, 종류와 길이를 검사하여 유효한 제어 데이터를 해당 처리 대상에 전달한다.',
 '리모콘과 매핑 통신의 상대 장치를 CRA로 확인하고 인증 결과와 제한 조건에 따라 데이터 접근을 제어한다.',
 '인증 키 선택, 매핑 키 파생과 인증값 계산 및 NVM 암복호화를 공통 인터페이스로 제공한다.',
 '생산 시 설치한 키와 난수를 정해진 보안 저장 영역에서 읽어 인증과 암복호화에 필요한 데이터로 제공한다.',
]
MODULES=[
 {'id':'SI-ID-QCC-001','title':'시스템 관리 모듈','purpose':'주변장치와 라이브러리를 초기화한 뒤 전원/연결 상태, Standalone/TWS 구성과 Bluetooth 활성 상태를 관리한다. 시스템 상태 유닛을 통해 BLE 명령과 E8300 SPI 명령/응답을 연결한다.','units':[1,2,3,4]},
 {'id':'SI-ID-QCC-002','title':'HW 인터페이스 모듈','purpose':'PIO, 배터리/충전과 비휘발성 저장장치의 접근을 제공한다. 입력 변화와 전원 상태는 시스템 관리에 전달하고, NVM 데이터는 일반 설정 및 보안 처리에 제공한다.','units':[5,6,7]},
 {'id':'SI-ID-QCC-003','title':'블루투스 모듈','purpose':'Classic 오디오 송수신과 BLE 광고/연결 및 메시지 전달을 처리한다. 수신 오디오는 I2S로 E8300에 전달하고 전화 통화 중 DMIC2의 음성은 통화 상대에게 송신한다. AUTH service의 전송과 보안 모듈의 인증 계산은 분리한다.','units':[8,9,10,11]},
 {'id':'SI-ID-QCC-004','title':'보안 모듈','purpose':'CRA를 통해 통신 상대를 확인하고 인증 상태에 따라 보호 데이터 접근을 허용한다. 보안 툴박스의 키 파생/인증값 계산과 보안 NVM의 키 읽기를 사용하며 인증 실패와 시간 제한을 BLE 연결 제어에 반영한다.','units':[12,13,14]},
]
OVERRIDES={
 1:{'input':['SPI 포트/클록과 PIO 방향/기능 설정, NVM 데이터 식별자/길이','I2S 출력 24bit/16kHz, Classic 오디오의 I2S 출력 선택','BLE 특성의 인증 속성 및 AUTH service 설정, 충전케이스 통신 포트/속도'],
 'process':['초기화 전에 주변장치, 통신과 NVM 설정을 준비하고 정상 동작 중 초기 설정을 반복 변경하지 않는다.','시스템 → 내장 라이브러리 → 시스템 스케줄러 → Power → Bluetooth → HW → PIO 순서로 초기화한다. Application은 필요한 하위 기능이 준비된 후 시작한다.','PIO_INTER_DEVICE_CONN_IND, PIO_SPI_COMM_IND, PIO_SHUT_DOWN_IND는 입력으로 설정한다. PIO_I2S_EN_IND, PIO_HW_RESET_CTRL, PIO_TEMP_CHECK_ON은 출력으로 설정하고 SPI 및 온도 감지 핀은 주변장치 기능에 연결한다.','초기화가 끝나면 시스템 상태 처리에 종료/내부기 연결 입력, 배터리/충전 및 Bluetooth 상태를 전달한다.'],
 'impl':['초기 설정은 각 하위 모듈의 실행 전에 완료한다. Classic 수신 오디오는 스피커 출력 대신 I2S 출력 경로를 사용한다.','tdPio_preInit()은 Application 초기화 전에 수행한다. 초기화 완료를 확인한 뒤 상태 처리와 통신을 시작한다.'],
 'gap':'SPI 속도와 실제 QCC 핀 번호/초기값, 초기화 실패 처리/시간 한도, AUTH UUID/권한 및 I2S 24bit/16kHz의 실제 출력 설정과 실행 확인이 필요하다.'},
 2:{'process':['shutdown_ind, inter_dev_conn_ind, 배터리, 충전, Classic 연결과 BLE 연결의 여섯 상태를 저장하고 조회한다.','초기화 후 E8300 준비 신호를 기다린다. 준비 대기 시간을 넘기면 Deep sleep에 진입하고, 정상 동작 중 종료 요청을 받으면 종료 대기 후 정지한다.','내부기 연결 상태가 활성화되면 E8300에 장치 정보와 편이/양이 정보를 요청하고 장치명 및 Classic/BLE 동작을 준비한다.','전원/Bluetooth 상태를 SPI로 E8300에 전달하고 BLE의 E8300 대상 데이터와 응답을 내용 변경 없이 중계한다.','0x34는 잔량 %, charger_connected 0/1과 lid 1/2를 전달한다. 0x35는 LED/갱신 연결, 0x36은 오디오 연결/종류를 전달한다. E8300의 유효 응답은 해당 명령과 성공값 1이다.'],
 'impl':['E8300 QCC_CTRL은 High=정상, Low=종료 요청이다. ISD_CHECK는 High=내부기 연결, Low=미연결이다. 두 핀의 초기값은 Low이다.','BLE 논리 메시지의 type+payload는 최대 20byte이다. E8300 물리 SPI 프레임은 별도 길이 1byte를 포함한 21byte이다. 응답 저장 배열의 index20은 유효 응답 길이를 저장한다.'],
 'gap':'QCC shutdown_ind의 물리 핀/논리 반전과 기동/종료 대기 수치, 마지막 SPI 거래 시점, 내부기 정보 요청/응답 및 실제 QCC 송신 length 필드의 값/위치와 21byte 변환 계약을 확인해야 한다.'},
 3:{'impl':['Standalone은 외부기 1대가 오디오 L+R과 제어 데이터를 처리한다. TWS는 외부기 2대가 L/R 오디오와 데이터를 구분하며 1대 단독 사용도 지원한다.','장치 구성은 생산 시 결정하며 동작 중 변경하지 않는다. TWS 오디오/데이터 전달은 Qualcomm 내장 라이브러리 기능에 연결한다.'],
 'gap':'TWS 구성값과 좌우 배정, peer 연결/해제 후 단독 복귀 동작, 생산 설정과 내부기 정보의 우선순위 및 각 모드의 실제 오디오/데이터 처리 확인이 필요하다.'},
 4:{'impl':['inter_dev_conn_ind=0이면 Bluetooth를 비활성화하고 1이면 활성화한다. 탈착 시 기존 BLE/Classic 연결을 해제하고 재부착 시 검색/연결 기능을 다시 준비한다.','장치명은 TD_L__SOUND_ UserName_ExternalDeviceInfo 형식을 사용하며 L 부분은 내부기 수술 위치 L/R에 대응한다.'],
 'gap':'부착 디바운스 시간, 장치명의 실제 바이트/인코딩/길이와 개인정보 처리, 부착 전 허용되는 BLE 기동/충전/갱신 조건 및 신호 전환 실행 확인이 필요하다.'},
 5:{'impl':['tdPio_preInit()/tdPio_init()으로 방향/기능과 초기값을 준비한다. shutdown_ind와 inter_dev_conn_ind의 상태가 변하면 TD_MSG_PIO_CHANGED를 상태 처리에 전달한다.','I2S enable은 E8300의 I2S 입력 유효 상태와 연계한다. E8300 CFX I2S_FLAG(DIO29)는 Low일 때 입력 유효이다.'],
 'gap':'QCC PIO 번호/전압/논리 반전, I2S enable과 DIO29의 물리 대응, HW reset의 회로 대상과 안전 상태, 온도 감지의 실제 사용 조건/한도 및 측정 결과를 확인해야 한다.'},
 6:{'process':['ADC로 배터리 전압을 주기적으로 읽어 0~100% 잔량과 저전압 상태로 변환하고 시스템 상태에서 읽을 수 있게 한다.','전압[V]→잔량[%]의 기준점은 4.2 이상→100, 3.95→80, 3.7→60, 3.475→40, 3.35→20, 3.0→0이다.','충전 상태는 미연결/연결/충전 중/비충전으로 구분한다. E8300 0x34에는 충전기 연결 여부 0/1을 전달한다.','배터리가 Empty이거나 E8300 종료 요청을 받으면 Deep sleep 진입을 처리한다. 외부 충전 연결 중에는 충전 상태를 유지하는 조건을 함께 적용한다.','전원 인가/wake-up 후에는 시스템 초기화를 수행한다. 충전케이스 통신과 저수준 전원 처리는 내장 라이브러리에 연결한다.'],
 'impl':['잔량 기준점과 실제 ADC/공급 전압은 같은 입력에서 비교한다. Empty의 저전압 진입, 충전 연결/분리와 shutdown_ind 변화는 독립 상태로 확인한다.','전원 상태 정보는 SU-ID-QCC-002가 SPI로 E8300에 전달한다. 저전압 LED/자극 알림의 최종 실행은 E8300 시스템 경로가 담당한다.'],
 'gap':'전압-잔량의 중간 구간 변환 방식/측정 주기, 10% 허용오차의 상대오차 또는 %p 의미, 충전 4상태→binary 변환과 Empty/충전/종료 요청의 우선순위, wake-up source/시간 및 실행 결과를 확인해야 한다.'},
 7:{'impl':['Application PS 영역의 외부기 일련번호는 TD_PS_KEY_EX_DEV_SERIAL_NUNM으로 읽기/쓰기한다. 시험용 번호를 저장한 뒤 같은 ID에서 읽어 길이와 값을 확인한다.','보안 키의 역할/권한/암복호화는 보안 NVM과 툴박스에서 처리한다. 일반 NVM 인터페이스의 존재를 키 보호 완료로 판단하지 않는다.'],
 'gap':'데이터별 실제 PS 식별자/길이/바이트 순서/초기값과 읽기·쓰기 실패 처리, 일반 NVM과 보안 NVM의 키 식별자 대응 및 실행 결과를 확인해야 한다.'},
 8:{'input':['Bluetooth 활성 상태, pair_req/disconnect_req/audio_disconnect_req','상대 Classic 장치의 연결/오디오 연결 요청과 수신 오디오','DMIC2 마이크 입력과 전화 통화 상태'],
 'output':['Classic 및 Audio 연결 상태','수신 오디오의 I2S 24bit/16kHz 출력과 E8300 오디오 상태 통보','DMIC2에서 수음한 음성의 Bluetooth 통화 상향 송신'],
 'process':['Bluetooth 비활성 상태이면 연결/페어링 요청을 처리하지 않는다.','Pairable 상태를 유지하여 상대 장치가 검색/페어링할 수 있게 하고 페어링 완료 또는 제한 시간 경과 후 해제한다.','Classic/Audio 연결이 성립하면 수신 오디오를 I2S 24bit/16kHz로 E8300 CFX에 전달한다.','전화 통화 시 QCC에 연결된 DMIC2의 음성을 수음하여 통화 상대에게 송신한다. DMIC2는 E8300에도 연결되며 QCC→E8300 I2S 하향과 마이크 상향은 별도 경로이다.','Classic 해제 요청은 Audio 연결도 해제한다. Audio만 해제할 때는 Classic 연결 상태를 유지한다.'],
 'impl':['SU-ID-QCC-003의 Standalone/TWS 구성에 따라 수신 오디오 L/R을 처리한다. I2S 시작/중단은 PIO의 입력 유효 표시와 0x36 오디오 상태에 연결한다.','수신→I2S→E8300 처리와 DMIC2→QCC→통화 상대 송신은 SW-FW-RID-69의 서로 다른 방향이다. 시스템 시험 ID69와 ID82는 각 방향의 결과를 별도로 비교한다.'],
 'gap':'DMIC2의 QCC 입력 연결은 확인되었다. QCC 마이크 핀/클록, 상대 장치와 Classic 프로파일/코덱/라이브러리 설정, 페어링 제한 시간, L/R/동시 송수신 조건, I2S 유효 신호/0x36 통보 순서 및 실제 음성/오디오 시험 결과가 필요하다.'},
 9:{'process':['Bluetooth 비활성 상태이면 BLE 검색/연결 요청을 처리하지 않는다.','Advertising에는 Complete Local Name과 Flags를, Scan response에는 Appearance/Flags/Manufacturer specific data를 구성한다.','Appearance는 Cochlear Implant로 설정하고 manufacturer data에는 현재 장치와 TWS peer의 배터리/연결 상태를 포함한다. 잔량 변화에 맞춰 갱신한다.','동시에 BLE 연결 1개를 허용하고 연결 상태를 시스템 상태에 전달한다. 페어링 후 AUTH service와 보안 모듈을 통해 자체 인증을 진행한다.','BLE 데이터 송수신이 3분(180초) 동안 없으면 연결을 해제한다. 제어 명령 미수신 조건과 송수신 감시 조건은 같은 타이머로 임의 치환하지 않는다.'],
 'impl':['연결/해제와 잔량 갱신을 각각 광고/scan response와 장치 상태에 반영한다. TWS peer의 값은 현재 장치 값과 구분한다.','BLE 링크 AES-CCM 보호, CRA 인증값 계산과 저장 데이터 암호화는 서로 다른 보호 경로이다.'],
 'gap':'Advertising/Scan response의 전체 바이트 배치/회사ID/Appearance 값/길이/주기, 링크 암호화와 페어링 전 접근 차단, 180초 감시 대상/시작/reset/허용오차 및 실제 연결/상태 결과를 확인해야 한다.'},
 10:{'impl':['Challenge/Confirm은 client write, Response/Status는 read/notify에 연결한다. auth_type 0x01은 Patient/리모콘, 0x02는 Clinician/매핑이며 다른 값은 invalid로 처리한다.','Status는 실패 0, 성공 1, 진행 중 2, 타임아웃 3을 사용한다. AUTH service는 값을 전달하고 SU-ID-QCC-012가 인증을 판정한다.','단순 전송 경계 시험은 Challenge/Response/Confirm의 12byte 패턴과 Status 1byte 패턴을 비교한다. 이 길이/패턴은 실제 CRA 메시지 전체 길이와 인증 성공을 뜻하지 않는다.'],
 'gap':'실제 서비스/특성 UUID, CRA 메시지 길이/엔디언/유효성 검사와 GATT 권한, Confirm client write와 notify 표기의 정합성, 페어링 전 접근 차단 및 실행 결과를 확인해야 한다.'},
 11:{'impl':['type 1byte와 payload 0~19byte의 논리 메시지를 검사한다. SPI 중계는 별도 길이 byte가 포함된 21byte 프레임에 연결한다.','보호된 기기제어/매핑 데이터는 CRA 성공 이후에 전달한다. 인증 실패/타임아웃에는 허용된 인증 응답과 일반 제어 요청을 구분한다.'],
 'gap':'GAIA 주소와 type별 데이터 필드/단위/길이/CRC/응답, 편이/양이 좌우 및 nn 주소 정의, 인증/권한별 허용 메시지, 실제 송신 length 필드/21byte 변환 및 유효하지 않은 요청 처리 결과를 확인해야 한다.'},
 12:{'process':['BLE 페어링 후 CRA를 시작하고 auth_type에 따라 리모콘 또는 매핑 키를 선택한다. 유효하지 않은 타입은 실패로 처리한다.','클라이언트의 Nonce_A를 받고 장치의 Nonce_B와 인증값 B를 Response로 전달한다. 클라이언트가 B를 확인하여 보낸 Confirm의 인증값 A를 장치 계산값과 비교한다.','Status로 실패 0/성공 1/진행 중 2/타임아웃 3을 알리고 성공일 때 보호된 기기제어/매핑 데이터를 허용한다. 응답에는 인증 비밀을 노출하지 않는다.','30초 동안 인증이 완료되지 않으면 타임아웃 처리 후 연결을 해제한다. 정상 완료 연결은 인증 타임아웃으로 해제하지 않는다.','같은 대상의 CRA 인증이 5회 연속 실패하면 1분(60초) 동안 연결을 제한한다. BLE 무송수신 180초 해제와 인증 30초/실패 제한 60초는 각각 관리한다.','연결 해제 시 현재 인증 상태를 대기 상태로 전환한다. 실패 누적/제한 이력의 초기화는 연결 해제와 별도 정책으로 관리한다.'],
 'impl':['auth_type에 따른 키 선택과 인증값 계산은 SU-ID-QCC-013에 요청하고 AUTH service를 통해 상대에게 전달한다.','인증 상태, 데이터 접근 허용과 연결 해제는 같은 사건에서 비교한다. 12byte 패턴 중계만으로 CRA 계산/제어 허용의 통과를 판정하지 않는다.'],
 'gap':'30초/180초 타이머 시작/갱신/reset/허용오차, 5회 실패 누적 대상과 초기화/재기동/60초 제한 종료 경계, 인증값 알고리즘/직렬화 및 데이터 게이트의 실제 동작을 확인해야 한다.'},
 13:{'process':['리모콘 인증에는 생산 시 설치한 shared_service_key를 사용한다. 매핑에는 clinician_master_key와 제조번호를 사용하여 SHA-256 결과의 앞 16byte를 clinic_service_key로 파생한다.','Nonce_A와 Nonce_B는 각각 16byte이다. 인증값 A/B 계산은 AES-CMAC 인터페이스에 연결하며 서로 다른 nonce 결합 순서를 사용한다.','NVM 암복호화에는 AES-128 인터페이스를 사용한다. BD_ADDR 6byte와 random 10byte를 연결한 16byte를 키 재료로 사용한다.','인증/저장 데이터는 보안 NVM을 통해 읽으며 제조 설치 키와 실행 중 파생 키를 구분한다.'],
 'impl':['SHA-256 키 파생과 AES-CMAC 인증값 계산은 역할이 다르다. 공개 시험키/입력과 독립 벡터로 digest 전체, 앞 16byte 키, 인증값 및 복호화값을 각각 비교한다.','HMAC_A/HMAC_B/ENC 표기는 실제 AES-CMAC primitive와 입력 직렬화를 확인하기 전 별개의 확정 알고리즘으로 사용하지 않는다. NVM AES-128의 모드/IV/무결성과 키 재료 읽기 순서는 별도 계약이다.'],
 'gap':'앱별 인증 계산과 SHA-256 키 파생의 역할 정합성, clinician_master_key/제조번호의 직렬화/길이, HMAC/ENC 표기와 nonce 순서/태그 길이, AES-128 모드/IV/무결성, 난수 품질과 NVM bootstrap 및 검증 벡터가 필요하다.'},
 14:{'input':['보안 PS 데이터 식별자와 읽기 요청','HW NVM 읽기 인터페이스, toolbox 복호화, BD_ADDR/RAND 키 재료'],
 'output':['복호화된 제조 설치 키와 인증/키 파생용 데이터','읽기 결과/길이/실패 상태'],
 'process':['생산 시 설치한 보안 저장값은 PS_USR40의 RAND 10byte, PS_USR43의 clinician_master_key 16byte, PS_USR44의 shared_service_key 16byte로 구분한다.','펌웨어의 보안 NVM 접근은 읽기 인터페이스를 사용한다. 일반 NVM이 실제 PS 영역을 읽고 toolbox가 필요한 저장값을 복호화한다.','리모콘 키와 의료진 master key를 인증/파생 처리에 제공하고 생성된 clinic_service_key와 제조 저장 키를 구분한다.','읽기 전용 펌웨어 인터페이스를 생산 도구/PSTool/디버거의 접근 제한 완료로 확대하지 않는다.'],
 'impl':['TD_PS_KEY_AUTH_SET_RAND와 보안 키의 PS 식별자/길이를 일치시킨다. 시험키의 존재/길이/보호 상태와 복호화 결과를 비교하며 실제 생산 비밀키를 시험 보고서에 노출하지 않는다.','생산 저장 작업과 실행 중 읽기를 분리한다. 읽기 실패/손상 저장값은 인증/데이터 접근의 안전 상태에 연결한다.'],
 'gap':'PS 키 이름/역할의 전체 대응, RAND 저장 보호와 복호화 키 bootstrap, 초기값/손상 검출/읽기 실패/접근 제한 정책, 암호화 저장값/복호화 시험 벡터 및 실제 보호 결과를 확인해야 한다.'},
}
for n,u in enumerate(UNITS,1):
    u['purpose']=PURPOSES[n-1]
    u.update(OVERRIDES.get(n,{}))
    u['tests']=[{'id':'SW-FW-UnitTest-QCC-ID-'+str(t),'title':TESTS['unit'][t-1]['title'],'purpose':TESTS['unit'][t-1]['purpose'],'support':(n==2 and t==4)or(n in[8,9]and t==8)}for t in TEST_MAP[n]]
    u['rids']=[int(v.rsplit('-',1)[1])for v in u['requirement_candidates']]
    if n in [2,4,5]:u['rids']+= [46,67,68]
    if n==2:u['rids']+=[21,22,29,63]
    if n==9:u['rids']+=[47]
    u['rids']=sorted(set(u['rids']))

AUDIO='오디오 입력은 DMIC1/DMIC2→E8300의 마이크 경로와 QCC→E8300의 I2S 경로로 구분한다. DMIC2는 QCC에도 연결되며 전화 통화 시 QCC가 수음한 음성을 통화 상대에게 송신한다. QCC의 Classic 수신 오디오는 I2S 24bit/16kHz로 CFX에 전달한다. E8300 PCM1은 slave, 24bit, MSB 우선/정렬, 상승 에지, 2word 설정이며 16표본 블록을 처리한다. 16kHz 표본율과 0~8kHz 오디오 대역은 구분한다. CFX는 일반 I2S 게인 1과 마이크 Gain A, 크래들 I2S Gain B와 마이크 Gain A, 매핑 연결 중 두 게인 1을 적용하며 혼합 후 2로 나누지 않는다.'
AUDIO_GAP='[QCC 자료 입력] DMIC2의 QCC 입력 연결은 확인되었다. QCC 마이크 핀/클록, 프로파일/코덱별 송수신과 동시 동작, I2S 클록/프레임/좌우 배정 및 시작/중단 유효 신호의 실제 설정/파형과 오디오 시험 결과를 확인해야 한다.'
SCOPE='외부기의 QCC5181은 시스템 관리, HW 인터페이스, 블루투스와 보안의 4개 기능 모듈 및 14개 유닛으로 구성한다. QCC는 무선 송수신과 상태/명령 중계, PIO/전원/NVM 접근 및 CRA 인증을 처리한다. E8300은 CFX/HEAR의 오디오 처리와 CM3의 내부기 제어/자극 실행을 담당하며 두 칩은 SPI, I2S와 제어 신호로 연결한다.'
BOUNDARY=[['경계','담당 기능','연계 경로'],['외부기 / E8300','CFX/HEAR 오디오 처리, CM3 상태/명령 처리와 내부기 제어/자극','DMIC1/DMIC2→E8300, QCC→CM3 SPI, QCC→CFX I2S'],['외부기 / QCC','시스템 관리, HW 인터페이스, 블루투스, 보안','DMIC2→QCC→통화 상대, BLE/Classic, SPI/PIO/NVM'],['주변 장치','리모콘/매핑 프로그램, Classic 상대 장치와 충전케이스','BLE AUTH/GAIA, Classic 오디오 송수신, 충전케이스 통신'],['내부기','E8300의 명령/PCM 출력에 따른 자극 실행','E8300의 내부기 제어/자극 경로']]

def test_description(u):
    return ' '.join(t['id']+'에서는 '+t['purpose']+'를 '+('연계 지원으로 ' if t['support']else '')+'확인한다.'for t in u['tests'])
def trace_description(u):return '상위 요구사항: '+', '.join('SW-FW-RID-'+str(n)for n in u['rids'])+'. '+test_description(u)
