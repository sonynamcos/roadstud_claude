# 작업 기록 - 센서 ID 설정 기능 구현

## 날짜: 2026-01-28

## 프로젝트 경로
- **Flutter 앱**: `C:\Flutter Dev\flutter_roadstud\flutter_roadstud`
- **센서 펌웨어**: `C:\Users\TopIt\workspace_ccstheia\dmm_154sensor_remote_display_app_CC1352P1_LAUNCHXL_tirtos7_ticlang`
- **컬렉터 펌웨어**: `C:\Users\TopIt\workspace_ccstheia\dmm_154collector_remote_display_app_CC1352P1_LAUNCHXL_tirtos7_ticlang`
- **SDK 경로**: `C:\ti\simplelink_cc13xx_cc26xx_sdk_8_31_00_11`

---

## 완료된 작업: Flutter 앱에서 센서에 ID 부여 기능

### 목표
Flutter 앱으로 BLE 연결하여 센서에 ID(차선타입, 방향, 노드ID)를 부여하고 NV 메모리에 저장

### 프로토콜 설계
- **컬렉터 명령**: `[0xAA, laneMask, scenarioId, interval]` (4바이트)
- **센서 설정**: `[0xBB, laneType, direction, nodeId]` (4바이트)
  - laneType: 0(황색), 1(흰색-안), 2(흰색-밖)
  - direction: 0(정방향), 1(역방향)
  - nodeId: 1~15

### 수정된 파일

#### 1. Flutter - protocol.dart
**경로**: `C:\Flutter Dev\flutter_roadstud\flutter_roadstud\lib\models\protocol.dart`
**변경**: `createSetup()` 함수에 0xBB 헤더 추가 (3바이트 → 4바이트)
```dart
static Uint8List createSetup(int laneType, int direction, int nodeId) {
  return Uint8List.fromList([
    0xBB,       // Header: 센서 설정 명령
    laneType,   // 0:황색, 1:흰색(안), 2:흰색(밖)
    direction,  // 0:정방향, 1:역방향
    nodeId,     // 1 ~ 15번
  ]);
}
```

#### 2. 센서 펌웨어 - ssf.c
**경로**: `C:\ti\simplelink_cc13xx_cc26xx_sdk_8_31_00_11\source\ti\ti154stack\apps\sensor\ssf.c`
**변경**:
- NV Item ID 추가: `#define SSF_NV_SENSOR_ID_INFO_ID  0x0009`
- 함수 추가:
  - `Ssf_setSensorIdInfo(uint8_t laneType, uint8_t direction, uint8_t nodeId)` - NV에 저장
  - `Ssf_getSensorIdInfo(uint8_t *pLaneType, uint8_t *pDirection, uint8_t *pNodeId)` - NV에서 읽기

#### 3. 센서 펌웨어 - ssf.h
**경로**: `C:\ti\simplelink_cc13xx_cc26xx_sdk_8_31_00_11\source\ti\ti154stack\apps\sensor\ssf.h`
**변경**: 함수 선언 추가
```c
extern void Ssf_setSensorIdInfo(uint8_t laneType, uint8_t direction, uint8_t nodeId);
extern bool Ssf_getSensorIdInfo(uint8_t *pLaneType, uint8_t *pDirection, uint8_t *pNodeId);
```

#### 4. 센서 펌웨어 - sensor.c
**경로**: `C:\ti\simplelink_cc13xx_cc26xx_sdk_8_31_00_11\source\ti\ti154stack\apps\sensor\sensor.c`
**변경**:
- 전역 변수 추가:
```c
uint8_t Sensor_laneType = 0;    // 0:Yellow, 1:White-In, 2:White-Out
uint8_t Sensor_direction = 0;   // 0:Forward, 1:Reverse
uint8_t Sensor_nodeId = 0;      // 1~15
```
- `Sensor_init()` 함수에서 부팅 시 NV에서 ID 로드:
```c
if(Ssf_getSensorIdInfo(&Sensor_laneType, &Sensor_direction, &Sensor_nodeId))
{
    uartPrintf("Sensor ID loaded: Lane=%d Dir=%d ID=%d\r\n",
               Sensor_laneType, Sensor_direction, Sensor_nodeId);
}
```

#### 5. 센서 펌웨어 - sensor.h
**경로**: `C:\ti\simplelink_cc13xx_cc26xx_sdk_8_31_00_11\source\ti\ti154stack\apps\sensor\sensor.h`
**변경**: extern 선언 추가
```c
extern uint8_t Sensor_laneType;
extern uint8_t Sensor_direction;
extern uint8_t Sensor_nodeId;
```

#### 6. BLE - remote_display.c
**경로**: `C:\ti\simplelink_cc13xx_cc26xx_sdk_8_31_00_11\source\ti\dmm\apps\common\ti154stack\source\ble_remote_display\app\remote_display.c`
**변경**: `RemoteDisplay_processRDCharValueChangeEvt()` 함수에 0xBB 처리 로직 추가
```c
#ifdef DMM_SENSOR
if(newReportInterval[0] == 0xBB)
{
    Ssf_setSensorIdInfo(newReportInterval[1], newReportInterval[2], newReportInterval[3]);
    // CUI 출력...
    break;
}
#endif
```

---

## 데이터 흐름

```
[Flutter 앱] → BLE Write [0xBB, lane, dir, id]
       ↓
[remote_display.c] → 0xBB 감지
       ↓
[Ssf_setSensorIdInfo()] → NV 저장
       ↓
(센서 재부팅)
       ↓
[Sensor_init()] → Ssf_getSensorIdInfo() → 전역변수 로드
```

---

## BLE GATT UUID 정보
- **Write Characteristic**: `f0001182-0451-4000-b000-000000000000` (UUID: 0x1182)
- **Read Characteristic**: `f0001184-0451-4000-b000-000000000000` (UUID: 0x1184)

---

## 추가 완료: 센서 → Flutter 응답 전송

### 센서 (remote_display.c)
ID 설정 후 응답 패킷 전송:
```c
uint8_t response[4];
response[0] = 0xBB;  // 헤더
response[1] = laneType;
response[2] = direction;
response[3] = nodeId;
RemoteDisplay_SetParameter(RDPROFILE_SENSOR_DATA_CHAR, sizeof(response), response);
```

### Flutter (sensor_setup_screen.dart)
응답 파싱 및 상세 로그 출력:
```dart
if (response.length >= 4 && response[0] == 0xBB) {
  final laneNames = ['황색', '흰색(안)', '흰색(밖)'];
  final dirNames = ['정방향', '역방향'];
  _addLog("차선: ${laneNames[response[1]]}");
  _addLog("방향: ${dirNames[response[2]]}");
  _addLog("ID: ${response[3]}");
}
```

---

## 추가 완료: BLE 페어링 팝업 비활성화

### 센서 프로젝트 syscfg 파일
**경로**: `C:\Users\TopIt\workspace_ccstheia\dmm_154sensor_remote_display_app_CC1352P1_LAUNCHXL_tirtos7_ticlang\dmm_154sensor_remote_display.syscfg`

추가된 설정:
```javascript
// BLE 페어링 비활성화 (팝업 없이 연결)
ble.bondManager.bondPairing         = "GAPBOND_PAIRING_MODE_NO_PAIRING";
ble.bondManager.bondMITMProtection  = false;
```

### 효과
- 변경 전: 연결 시 페어링 팝업 표시
- 변경 후: 팝업 없이 바로 연결

---

## 추가 완료: 효과음 기능

### pubspec.yaml
```yaml
audioplayers: ^6.1.0

assets:
  - assets/sounds/
```

### sensor_setup_screen.dart
```dart
final AudioPlayer _audioPlayer = AudioPlayer();

Future<void> _playSuccessSound() async {
  try {
    await _audioPlayer.play(AssetSource('sounds/success.mp3'));
  } catch (e) {
    // 사운드 파일이 없으면 무시
  }
}

Future<void> _playFailureSound() async {
  try {
    await _audioPlayer.play(AssetSource('sounds/failure.mp3'));
  } catch (e) {
    // 사운드 파일이 없으면 무시
  }
}
```

### 필요한 사운드 파일
- `assets/sounds/success.mp3` - 성공 효과음
- `assets/sounds/failure.mp3` - 실패 효과음

---

## 추가 완료: 햅틱 피드백

### 적용된 파일
- **main.dart**: 메인 메뉴 버튼
- **collector_screen.dart**: 시나리오 버튼, 차선 선택, 장치 선택, 다시 검색
- **sensor_setup_screen.dart**: ID 쓰기 버튼, 검색 버튼, 차선 선택, 방향 스위치

### 사용된 햅틱 타입
```dart
import 'package:flutter/services.dart';

HapticFeedback.mediumImpact();    // 버튼 클릭
HapticFeedback.lightImpact();     // 가벼운 액션
HapticFeedback.selectionClick();  // 선택 변경 (드롭다운, 스위치)
```

---

## 추가 완료: 컬렉터 연결 효과음

### collector_screen.dart
BLE 연결 성공/실패 시 효과음 재생:
```dart
onTap: () async {
  HapticFeedback.mediumImpact();
  Navigator.pop(context);
  await _ble.connectToDevice(r.device, _addLog);
  if (_ble.connectedDevice != null) {
    await _playSuccessSound();
  } else {
    await _playFailureSound();
  }
  setState(() {});
},
```

---

## 미완료/추후 작업
- [ ] 사운드 파일 추가 (success.mp3, failure.mp3)
- [ ] 센서 프로젝트 빌드 및 테스트
- [ ] 센서가 컬렉터에 데이터 전송 시 ID 포함하여 전송
- [ ] Flutter 앱에서 센서 Join/Leave BLE 알림 수신 확인

---

## 참고사항
- 트래킹 주기가 1시간 이상으로 설정되어 있어 센서 disconnect 감지가 느림
- DMM 환경에서 BLE와 15.4가 RF 공유하므로 타이밍 충돌 가능
- SDK 파일을 직접 수정했으므로, 다른 프로젝트에도 영향 있음

---

## 추가 완료: DMM BLE Status Notification (센서 Join/Leave 알림)

### 문제 상황
- BLE가 연결된 상태에서 센서 Association 시 Status 235 (noBeacon) 에러 발생
- `RemoteDisplay_sendStatusToBle()` 함수가 154 스택을 블로킹하여 RF 동작 방해

### 원인 분석
1. **컨텍스트 문제**: 154 스택 컨텍스트에서 BLE 함수(`linkDB_NumActive()`) 직접 호출 시 시스템 멈춤
2. **타이밍 문제**: Association Response 직후 BLE 알림 전송 시 Config Request 블로킹

### 해결 방법: 이벤트 큐 + 타이머 방식

#### 1. collector.c - 비동기 이벤트 처리
```c
/* BLE 상태 알림을 위한 변수 */
typedef struct {
    uint8_t type;       // 0x01=joined, 0x02=left
    uint16_t addr;      // 센서 주소
    uint8_t count;      // 총 센서 개수
    bool pending;       // 보내야 할 알림이 있는지
} BleStatusNotify_t;
STATIC BleStatusNotify_t pendingBleStatus = {0, 0, 0, false};

/* BLE 상태 알림 딜레이 타이머 */
STATIC ClockP_Struct bleStatusClkStruct;
STATIC ClockP_Handle bleStatusClkHandle;
#define BLE_STATUS_DELAY_MS  5000  // 5초 딜레이
```

#### 2. collector.c - dataCnfCB()에서 타이머 시작
Config Request TX 완료 후 타이머 시작:
```c
if(pDataCnf->status == ApiMac_status_success)
{
    Collector_statistics.configReqRequestSent++;
    /* Config Request TX 완료 - BLE 타이머 시작 */
    Collector_startBleStatusTimer();
}
```

#### 3. remote_display.c - 이벤트 큐 방식
154 스택에서 직접 BLE 함수 호출하지 않고 이벤트 큐 사용:
```c
#define RD_BLE_STATUS_NOTIFY_EVT  23

typedef struct {
    uint8_t type;
    uint16_t addr;
    uint8_t count;
} BleStatusData_t;

void RemoteDisplay_sendStatusToBle(uint8_t type, uint16_t addr, uint8_t count)
{
    BleStatusData_t *pData = ICall_malloc(sizeof(BleStatusData_t));
    if(pData) {
        pData->type = type;
        pData->addr = addr;
        pData->count = count;
        RemoteDisplay_enqueueMsg(RD_BLE_STATUS_NOTIFY_EVT, pData);
    }
}
```

#### 4. remote_display.c - BLE 태스크에서 처리
```c
case RD_BLE_STATUS_NOTIFY_EVT:
{
    BleStatusData_t *pStatusData = (BleStatusData_t*)(pMsg->pData);
    uint8_t numActive = linkDB_NumActive();
    if(numActive > 0) {
        uint8_t statusMsg[5] = {0xCC, pStatusData->type,
            (uint8_t)(pStatusData->addr & 0xFF),
            (uint8_t)(pStatusData->addr >> 8),
            pStatusData->count};
        RemoteDisplay_SetParameter(RDPROFILE_SENSOR_ADDR_CHAR, sizeof(statusMsg), statusMsg);
    }
    break;
}
```

### 동작 순서
1. 센서 Association 완료 → `cllcDeviceJoiningCB()`에서 `pendingBleStatus` 저장
2. Config Request TX 완료 → `dataCnfCB()`에서 5초 타이머 시작
3. 5초 후 → `COLLECTOR_BLE_STATUS_EVT` 발생
4. `RemoteDisplay_sendStatusToBle()` → 이벤트 큐에 추가 (블로킹 없음)
5. BLE 태스크에서 GATT 알림 전송

### BLE 알림 프로토콜
| Byte | 내용 | 값 |
|------|------|-----|
| 0 | Header | 0xCC |
| 1 | Type | 0x01=joined, 0x02=left |
| 2 | Address Low | addr & 0xFF |
| 3 | Address High | addr >> 8 |
| 4 | Count | 연결된 센서 수 |

### 수정된 파일
- **collector.c**: 타이머, 이벤트 처리, `dataCnfCB()`에서 타이머 시작
- **collector.h**: `COLLECTOR_BLE_STATUS_EVT` (0x0020), `Collector_startBleStatusTimer()`
- **cllc.c**: 디버그 로그 추가
- **remote_display.c**: 이벤트 큐 방식으로 변경, `RD_BLE_STATUS_NOTIFY_EVT` 핸들러
- **remote_display_gatt_profile.c**: CCC 값 디버그 로그

### 테스트 결과
```
[COLLECTOR] Config Request TX SUCCESS - starting BLE timer
[COLLECTOR] Starting BLE status timer (5000ms after Config Request)
[BLE_STATUS] Queueing status notify event
[BLE_STATUS] Processing in BLE task, numActive=1
[GATT] CCC value: 0x0001 (0x0001=notify enabled)
[BLE_STATUS] SetParameter result: 0
```

### 핵심 교훈
1. **DMM 환경에서는 스택 간 직접 함수 호출 금지** - 반드시 이벤트 큐 사용
2. **TX 완료 콜백 활용** - `dataCnfCB()`에서 실제 전송 완료 시점 확인
3. **충분한 딜레이 필요** - RF 작업 완료 후 여유있게 5초 대기

---

## 2026-01-29: SK6812 LED + DMM 통합 시도 (미해결)

### 목표
컬렉터에서 0xAA 시나리오 명령 수신 시 SK6812 LED 애니메이션 실행

### 문제 현상
1. SK6812 코드 활성화 시 **센서가 비콘을 수신하지 못함** (네트워크 조인 실패)
2. SK6812 코드 비활성화 시 정상 동작
3. Scan Request API Status: 252 (`MAC_SCAN_IN_PROGRESS`) 에러 발생

### 시도한 해결책들

#### 1. SPI open/close 방식 (실패)
- 매 프레임마다 SPI_open → transfer → SPI_close
- **결과**: 오버헤드가 크고, RF 타이밍 여전히 방해

#### 2. Event 기반 LED 태스크 (부분 성공)
- Task_sleep 대신 Event_pend 사용
- Clock이 50ms마다 Event_post → LED 태스크가 Event_pend로 대기
- **구조**:
```c
Clock(50ms) → Event_post(LED_TICK_EVENT)
              ↓
LED Task: Event_pend() → scenarioTick() → return
```
- **결과**: 구조는 올바르나 비콘 수신 문제 미해결

#### 3. dataIndCB 콜백 최적화 (구현됨)
- **문제**: MAC 콜백에서 UART 20줄+ 출력 + SK6812_startTask() 직접 호출
- **해결**: 콜백에서는 파싱 + Event만, Sensor_process()에서 LED 처리
```c
// dataIndCB에서:
g_ledCmd.valid = 1;
g_ledCmd.scenarioId = ...;
Util_setEvent(&Sensor_events, SENSOR_LED_CMD_EVT);
return;  // 즉시 리턴

// Sensor_process에서:
if(Sensor_events & SENSOR_LED_CMD_EVT) {
    SK6812_setScenario(g_ledCmd.scenarioId);
    SK6812_startTask();
}
```

#### 4. SPI 열고 닫기 타이밍 조절 (실패)
- 부팅 시: SPI open → LED 끄기 → SPI close
- 네트워크 조인 중: SPI closed (RF 보호)
- LED 명령 수신 후: SPI re-open
- **결과**: 여전히 비콘 수신 실패

### 핵심 발견 사항

#### SPI와 RF 충돌 가능성
- CONFIG_SPI_0은 NVS 외장 플래시와 공유됨
- SPI DMA 채널: UDMA_CHAN_SSI0_TX, UDMA_CHAN_SSI0_RX
- SPI_open만 해도 RF에 영향을 주는 것으로 보임

#### GPT 권장 구조 (참고용)
```
"한 프레임씩" = scenarioTick()이 1번 호출될 때:
1. LED 버퍼 1번 만들고
2. SPI_transfer 1번 하고
3. 즉시 return
(for 루프, delay, printf 금지)
```

### 수정된 파일

#### sk6812_spi.c
**경로**: `dmm_154sensor_remote_display_app_CC1352P1_LAUNCHXL_tirtos7_ticlang/application/sk6812_spi.c`
- SPI 한번 open 유지 방식으로 변경
- CPUdelay/busy-wait 제거
- uartPrintf 제거 (LED_DEBUG=0)
- Event 기반 태스크 구현

#### sk6812_spi.h
**경로**: `dmm_154sensor_remote_display_app_CC1352P1_LAUNCHXL_tirtos7_ticlang/application/sk6812_spi.h`
- 시나리오 정의 (1~7)
- 차선 타입 정의 (LANE_YELLOW, LANE_WHITE_IN, LANE_WHITE_OUT)

#### sensor.c
**경로**: `C:\Users\TopIt\workspace_ccstheia\application\sensor\sensor.c`
- SENSOR_LED_CMD_EVT 이벤트 추가 (0x0800)
- g_ledCmd 구조체 추가 (콜백→프로세스 전달용)
- dataIndCB() 최적화 (UART 폭탄 제거)
- Sensor_process()에 LED 명령 처리 추가

#### main.c
**경로**: `C:\Users\TopIt\workspace_ccstheia\application\sensor\main.c`
- SK6812_createTask() 부팅 시 호출 제거 (지연 생성으로 변경)

### 미해결 문제
- **SK6812 코드 활성화 시 비콘 수신 불가**
- SPI_open 자체가 RF/MAC과 충돌하는 것으로 추정
- NVS 외장 플래시와 SPI 공유 문제 가능성

### 다음 단계 (추후 시도)
1. SPI를 완전히 다른 핀으로 이동 (GPIO bit-bang 또는 다른 SPI 인스턴스)
2. SK6812 초기화를 BIOS_start() 이후로 완전히 지연
3. NVS 외장 플래시 비활성화 테스트
4. TI E2E 포럼에서 SPI+RF 충돌 사례 검색

### 참고: 작동했던 이전 코드 특징
- SPI를 한번 열고 계속 유지 (g_spi 전역)
- 별도 LED 태스크 없음
- 단순한 blocking transfer
- **차이점**: DMM 환경이 아니었을 가능성

---

## 2026-01-30: 하이젠버그 해결 및 중요 파일 백업

### 하이젠버그(Heisenbug) 해결

#### 문제 현상
- 센서가 컬렉터에 조인 시도 시 Association 실패
- Status 240 (TRANSACTION_OVERFLOW), Status 233 (NO_BEACON) 에러 발생
- DMM 환경에서 BLE와 15.4 RF 타이밍 충돌

#### 해결 방법: UART 디버그 로그 추가
**핵심**: cllc.c에 UART 디버그 로그를 추가하면 암묵적 딜레이가 생겨 RF 타이밍 충돌 해소

**cllc.c 패치 내용:**
```c
// 1. uart_debug.h 포함
#include "application/uart_debug.h"

// 2. updateState() 함수에 디버그 로그
static void updateState(Cllc_states_t state)
{
    uartPrintf("\r\n[COLLECTOR] ========== STATE CHANGE ==========\r\n");
    uartPrintf("[COLLECTOR] Previous State: %d\r\n", coordInfoBlock.currentCllcState);
    uartPrintf("[COLLECTOR] New State: %d\r\n", state);
    // ... 상태 이름 출력 등
}

// 3. assocIndCb() 함수에 디버그 로그
static void assocIndCb(ApiMac_mlmeAssociateInd_t *pData)
{
    uartPrintf("\r\n[COLLECTOR] ========== ASSOC IND CB ==========\r\n");
    uartPrintf("[COLLECTOR] Device Addr: 0x%04X\r\n", pData->deviceAddress);
    // ... 기타 로그
}
```

#### 왜 동작하는가?
- UART 출력이 시리얼 통신을 위해 짧은 대기 시간 생성
- 이 대기 시간이 BLE와 15.4 RF 작업 사이의 충돌 완화
- 로그 제거 시 다시 실패 → "하이젠버그" (관측하면 사라지는 버그)

---

### GitHub 커밋 완료

**커밋**: `c506aef` - Add critical source files with Heisenbug fix

#### 새 프로젝트 구조 (application 폴더)
```
C:\Users\TopIt\workspace_ccstheia\application\
├── cllc/
│   ├── cllc.c          ← 하이젠버그 패치 적용 (핵심!)
│   └── cllc.h
├── collector/
│   ├── collector.c
│   ├── collector.h
│   ├── csf.c
│   └── csf.h
├── jdllc/
│   ├── jdllc.c
│   └── jdllc.h
├── sensor/
│   ├── main.c
│   ├── sensor.c
│   ├── sensor.h
│   ├── ssf.c
│   └── ssf.h
├── ble_remote_display/
│   ├── remote_display.c
│   ├── remote_display_gatt_profile.c
│   └── remote_display_gatt_profile.h
└── uart_debug/
    ├── uart_debug.c    ← 하이젠버그 해결 필수!
    └── uart_debug.h
```

---

### 로컬 백업 폴더

**경로**: `C:\roadstud_joinsuccess\`

**백업된 파일 (26개):**
| 파일 | 설명 |
|------|------|
| cllc.c, cllc.h | 하이젠버그 패치 적용 (핵심) |
| collector.c/h | 컬렉터 메인 |
| csf.c/h | 컬렉터 서포트 함수 |
| sensor.c/h | 센서 메인 |
| ssf.c/h | 센서 서포트 함수 (NV 저장) |
| jdllc.c/h | 센서 LLC |
| remote_display.c/h | BLE 처리 |
| remote_display_gatt_profile.c | GATT 프로필 |
| uart_debug.c/h | UART 디버그 (필수!) |
| sk6812_spi.c/h | LED 드라이버 |
| rf_switch_override.c | RF 스위치 |
| dmm_154collector_remote_display.syscfg | 컬렉터 설정 |
| dmm_154sensor_remote_display.syscfg | 센서 설정 |
| sensor_local.c, ssf_local.c | 로컬 복사본 |

---

### 핵심 교훈

1. **DMM 환경에서 RF 타이밍 민감** - BLE와 15.4가 RF 공유하므로 충돌 가능
2. **디버그 로그가 타이밍 변경** - UART 출력이 암묵적 딜레이 생성
3. **중요 파일은 반드시 백업** - Git + 로컬 폴더 이중 백업
4. **SDK 파일 수정 시 주의** - application 폴더에 로컬 복사본 유지

---

### 현재 상태 (2026-01-30)

- ✅ 센서-컬렉터 Association 정상 동작
- ✅ 하이젠버그 해결 (UART 디버그 로그 유지)
- ✅ GitHub 커밋 완료 (`c506aef`)
- ✅ 로컬 백업 완료 (`C:\roadstud_joinsuccess\`)
- ⏸️ SK6812 LED 기능 - 보류 (SPI+RF 충돌 문제)

### 다음 작업

- [ ] SK6812 LED 기능 재시도 (SPI 핀 변경 또는 GPIO bit-bang)
- [ ] 센서 데이터에 ID 포함하여 컬렉터 전송
- [ ] Flutter 앱에서 센서 Join/Leave 알림 수신 테스트
