# План реализации HDLC codec для DLMS/COSEM

## 1. Назначение документа

Документ описывает план реализации переносимой кроссплатформенной библиотеки C++11 для кодирования и декодирования HDLC-кадров формата **Frame Format Type 3** в составе будущего DLMS/COSEM-фреймворка.

Библиотека является базовым транспортно-канальным модулем и должна использоваться совместно с будущими модулями:

```text
HDLC codec
LLC codec
WRAPPER codec
APDU codec
HDLC session layer
```

В первой версии реализуется только **HDLC codec layer**, без полноценной session state machine.

---

## 2. Зафиксированные проектные решения

| Вопрос | Решение |
|---|---|
| Ошибки | Только status-коды |
| Exceptions | Не используются |
| Роль библиотеки | Универсальная: client + server |
| HDLC segmentation | Поддержать полностью |
| Byte stuffing | Не используется |
| Граница кадра | По `Frame Format.length`, flag используется как маркер начала/конца |
| Payload с `0x7E` | Допускается |
| Closing flag | Обязателен всегда |
| Frame set | Минимальный набор для клиента и сервера |
| Session layer | В v1 не реализуется, но требования фиксируются |
| C ABI | Закладывается отдельным стабильным слоем |
| CMake | Минимум 3.16 |
| Tests | GoogleTest |
| Limits | Настраиваемые limits + будущая negotiation |
| C++ API buffers | Оба варианта: `std::vector` и caller-provided buffers |
| Noise before first flag | Настраиваемая policy |

---

## 3. Цель v1

Реализовать библиотеку, которая умеет:

```text
кодировать HDLC frame format type 3
декодировать HDLC frame format type 3
работать без exceptions
возвращать ошибки через status-коды
поддерживать client + server сценарии
разбирать и кодировать HDLC address field
разбирать и кодировать HDLC control field
проверять HCS/FCS
декодировать поток по Frame Format.length
поддерживать payload с байтом 0x7E
требовать closing flag
выполнять segmentation/reassembly
предоставлять C++11 API
предоставлять стабильный C ABI
собираться через CMake 3.16
иметь обязательные GoogleTest-тесты
```

---

## 4. Границы v1

### 4.1. Входит в v1

```text
HDLC frame format type 3
HDLC address codec
HDLC control field codec
HCS/FCS
HDLC frame encoder
HDLC frame decoder
HDLC stream decoder by Frame Format.length
HDLC segmentation/reassembly
C++11 API
C ABI wrapper
GoogleTest coverage
CMake 3.16
```

### 4.2. Не входит в v1

```text
HDLC session state machine
timeouts
retransmission
SNRM/UA negotiation state
transport UART/TCP
LLC implementation
WRAPPER implementation
APDU implementation
xDLMS APDU block transfer
security/ciphering
```

---

## 5. Архитектурная позиция HDLC codec в DLMS/COSEM-фреймворке

Для HDLC-based profile будущий стек должен выглядеть так:

```text
+-----------------------------+
| APDU codec                  |
+-----------------------------+
| LLC codec                   |
+-----------------------------+
| HDLC session                |
+-----------------------------+
| HDLC codec                  |
+-----------------------------+
| Transport: UART/TCP/etc.    |
+-----------------------------+
```

Для WRAPPER-профиля будет отдельная ветка:

```text
+-----------------------------+
| APDU codec                  |
+-----------------------------+
| WRAPPER codec               |
+-----------------------------+
| Transport: TCP/UDP          |
+-----------------------------+
```

HDLC codec не должен знать про APDU. Его `information` field — это opaque byte buffer. В DLMS/COSEM там обычно будет LLC PDU.

---

## 6. Рассмотренные подходы

### 6.1. Подход A — только frame encode/decode

Плюс: минимальный объём работ.  
Минус: segmentation и потоковая обработка останутся снаружи.  
Риск: session layer потом начнёт дублировать parsing logic.

### 6.2. Подход B — frame codec + stream decoder + segmentation

Плюс: полноценная база для DLMS/COSEM HDLC session.  
Минус: больше тестов и аккуратнее API.  
Риск: можно случайно начать реализовывать session logic.

### 6.3. Подход C — codec + session сразу

Плюс: быстрее получить рабочий обмен со счётчиком.  
Минус: v1 раздуется.  
Риск: слои смешаются, особенно SNRM/UA, sequence counters и retry.

### 6.4. Выбор

Выбирается **Подход B**:

```text
frame codec
+ stream decoder
+ segmentation/reassembly
- session state machine
```

Это самый простой вариант, который не создаёт технический долг сразу на старте.

---

## 7. Структура проекта

```text
dlms-cpp/
 ├── CMakeLists.txt
 ├── include/
 │   └── dlms/
 │       └── hdlc/
 │           ├── hdlc_types.hpp
 │           ├── hdlc_error.hpp
 │           ├── hdlc_address.hpp
 │           ├── hdlc_control.hpp
 │           ├── hdlc_crc.hpp
 │           ├── hdlc_frame.hpp
 │           ├── hdlc_codec.hpp
 │           ├── hdlc_stream_decoder.hpp
 │           ├── hdlc_segmentation.hpp
 │           └── hdlc_c_api.h
 ├── src/
 │   └── hdlc/
 │       ├── hdlc_address.cpp
 │       ├── hdlc_control.cpp
 │       ├── hdlc_crc.cpp
 │       ├── hdlc_codec.cpp
 │       ├── hdlc_stream_decoder.cpp
 │       ├── hdlc_segmentation.cpp
 │       └── hdlc_c_api.cpp
 ├── test/
 │   ├── CMakeLists.txt
 │   └── hdlc/
 │       ├── test_hdlc_address.cpp
 │       ├── test_hdlc_control.cpp
 │       ├── test_hdlc_crc.cpp
 │       ├── test_hdlc_codec.cpp
 │       ├── test_hdlc_stream_decoder.cpp
 │       ├── test_hdlc_segmentation.cpp
 │       ├── test_hdlc_c_api.cpp
 │       └── test_hdlc_vectors.cpp
 └── docs/
     ├── 00_hdlc_requirements.md
     ├── 01_hdlc_codec_api.md
     ├── 02_hdlc_c_api.md
     ├── 03_hdlc_segmentation.md
     └── 04_hdlc_session_requirements.md
```

---

## 8. Базовые требования к ошибкам

### 8.1. Общий принцип

```text
Ни одна функция библиотеки не бросает исключения.
Ни одна функция библиотеки не вызывает abort/assert в runtime path.
Ошибки возвращаются только через HdlcStatus.
```

### 8.2. Status enum

```cpp
enum class HdlcStatus
{
  Ok = 0,

  NeedMoreData,
  OutputBufferTooSmall,

  InvalidArgument,
  InvalidFlag,
  InvalidFrameFormat,
  InvalidFrameType,
  InvalidFrameLength,

  InvalidAddress,
  InvalidControlField,

  InvalidHeaderChecksum,
  InvalidFrameChecksum,

  FrameTooLarge,
  SegmentationError,
  SegmentationIncomplete,
  SegmentationOverflow,

  UnsupportedFrame,
  UnsupportedAddress,
  UnsupportedFeature,

  InternalError
};
```

---

## 9. Buffer policy

Так как exceptions запрещены, но C++ API должен быть удобным, используются два уровня API.

### 9.1. High-level C++ API

Может использовать `std::vector`:

```cpp
HdlcStatus EncodeFrame(
  const HdlcFrame& frame,
  std::vector<std::uint8_t>& output);
```

Риск: allocation внутри `std::vector` потенциально может привести к `std::bad_alloc`. Поэтому этот API считается удобным, но не strict no-allocation API.

### 9.2. Strict no-allocation API

Использует caller-provided buffers:

```cpp
HdlcStatus EncodeFrameToBuffer(
  const HdlcFrame& frame,
  std::uint8_t* output,
  std::size_t outputSize,
  std::size_t& writtenSize);
```

Именно этот вариант должен использоваться в C ABI и в окружениях с жёсткими требованиями к предсказуемости.

---

## 10. HDLC frame model

### 10.1. Lightweight view

```cpp
struct HdlcFrame
{
  bool segmented;

  HdlcAddress destination;
  HdlcAddress source;

  HdlcControl control;

  const std::uint8_t* informationData;
  std::size_t informationSize;
};
```

### 10.2. Owning container

```cpp
struct HdlcFrameBuffer
{
  bool segmented;
  HdlcAddress destination;
  HdlcAddress source;
  HdlcControl control;
  std::vector<std::uint8_t> information;
};
```

Назначение:

```text
HdlcFrame       — lightweight view для encode/decode без лишнего владения памятью
HdlcFrameBuffer — owning container для stream decoder, reassembler и тестов
```

---

## 11. HDLC address codec

### 11.1. Требования

Поддержать:

```text
1-byte address
2-byte address
4-byte address
client address
server address
logical + physical server addressing
```

Reserved/broadcast/no-station handling нужно оформить явно в требованиях до реализации совместимости с конкретными счётчиками.

### 11.2. API

```cpp
class HdlcAddress
{
public:
  HdlcAddress();

  static HdlcStatus FromBytes(
    const std::uint8_t* data,
    std::size_t size,
    HdlcAddress& address,
    std::size_t& consumedSize);

  static HdlcStatus FromRaw(
    std::uint32_t rawValue,
    std::size_t encodedSize,
    HdlcAddress& address);

  HdlcStatus Encode(
    std::uint8_t* output,
    std::size_t outputSize,
    std::size_t& writtenSize) const;

  std::uint32_t RawValue() const;
  std::size_t EncodedSize() const;

private:
  std::uint32_t rawValue_;
  std::size_t encodedSize_;
};
```

### 11.3. DLMS helper

```cpp
class DlmsHdlcAddress
{
public:
  static HdlcStatus MakeClientAddress(
    std::uint8_t clientAddress,
    HdlcAddress& output);

  static HdlcStatus MakeServerAddress(
    std::uint16_t logicalDeviceAddress,
    std::uint16_t physicalDeviceAddress,
    HdlcAddress& output);
};
```

---

## 12. HDLC control field

### 12.1. Минимальный набор для клиента и сервера

Обязательно поддержать:

```text
I-frame
RR
RNR
SNRM
UA
DISC
DM
FRMR
UI
```

Также желательно сразу распознавать:

```text
REJ
SREJ
```

Codec должен уметь их кодировать/декодировать, даже если session layer в v1 отсутствует.

### 12.2. API

```cpp
enum class HdlcFrameKind
{
  Information,
  Supervisory,
  Unnumbered
};

enum class HdlcSupervisoryKind
{
  ReceiveReady,
  ReceiveNotReady,
  Reject,
  SelectiveReject
};

enum class HdlcUnnumberedKind
{
  Snrm,
  Ua,
  Disc,
  Dm,
  Frmr,
  Ui
};

class HdlcControl
{
public:
  HdlcControl();

  static HdlcStatus Decode(
    std::uint8_t value,
    HdlcControl& control);

  HdlcStatus Encode(std::uint8_t& value) const;

  HdlcFrameKind FrameKind() const;

  bool PollFinal() const;

  std::uint8_t SendSequence() const;
  std::uint8_t ReceiveSequence() const;

private:
  std::uint8_t rawValue_;
};
```

---

## 13. HCS/FCS

### 13.1. Требования

Реализовать:

```text
CRC calculation
HCS validation
FCS validation
header-only frame case
frame with information field
```

### 13.2. API

```cpp
std::uint16_t CalculateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size);

HdlcStatus ValidateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size,
  std::uint16_t expected);
```

### 13.3. Важное требование

Нужны тестовые векторы из реальных DLMS/COSEM кадров:

```text
SNRM
UA
DISC
I-frame with LLC payload
RR
```

Без реальных векторов CRC легко сделать почти правильно, но несовместимо с настоящими устройствами.

---

## 14. Frame encoder

### 14.1. Формат кадра

Encoder собирает кадр:

```text
Flag
Frame Format type 3
Destination address
Source address
Control
HCS if information exists
Information
FCS
Flag
```

### 14.2. Byte stuffing

Byte stuffing не используется:

```text
encoder не выполняет escape
decoder не выполняет unescape
```

Флаги `0x7E` всё равно добавляются как начало и конец frame.

### 14.3. Frame Format.length

```text
Length field содержит длину кадра без opening flag и closing flag.
```

Тестировать обязательно:

```text
empty information
non-empty information
maximum length
segmented frame
payload containing 0x7E
```

---

## 15. Frame decoder

Decoder обязан:

1. Найти opening flag `0x7E`.
2. Прочитать `Frame Format`.
3. Проверить format type `0b1010`.
4. Извлечь segmentation bit.
5. Извлечь length.
6. Дочитать ровно `length` байт.
7. Проверить closing flag `0x7E`.
8. Разобрать destination address.
9. Разобрать source address.
10. Разобрать control field.
11. Проверить HCS, если есть information.
12. Проверить FCS.
13. Вернуть `HdlcFrameBuffer` или status-code error.

Так как byte stuffing не используется, наличие `0x7E` внутри payload не должно завершать frame, если `Frame Format.length` корректный.

---

## 16. Stream decoder

### 16.1. Назначение

`HdlcStreamDecoder` принимает произвольные куски данных и извлекает из них полные HDLC-кадры.

```cpp
class HdlcStreamDecoder
{
public:
  explicit HdlcStreamDecoder(const HdlcStreamDecoderOptions& options);

  HdlcStatus Push(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<HdlcFrameBuffer>& frames);

  void Reset();
};
```

### 16.2. Состояния decoder

```text
WAIT_FLAG
READ_FORMAT_FIELD
READ_FRAME_BODY_BY_LENGTH
READ_CLOSING_FLAG
DECODE_FRAME
```

### 16.3. Политика мусора до первого flag

Поведение настраиваемое:

```cpp
enum class HdlcNoisePolicy
{
  IgnoreBeforeOpeningFlag,
  ReportErrorBeforeOpeningFlag
};
```

### 16.4. Критичное правило

Нельзя завершать кадр по первому встреченному `0x7E` после начала, потому что byte stuffing отключён и `0x7E` может находиться внутри payload.

Кадр завершается только после чтения количества байт, указанного в `Frame Format.length`, и проверки обязательного closing flag.

---

## 17. Limits и negotiation

### 17.1. Codec-level limits

В v1 должны быть настраиваемые limits:

```cpp
struct HdlcCodecLimits
{
  std::size_t maximumFrameSize;
  std::size_t maximumInformationFieldSize;
  std::size_t maximumReassembledInformationSize;
};
```

### 17.2. Defaults

Нужны безопасные значения по умолчанию. Конкретные числа должны быть зафиксированы перед реализацией или вынесены в отдельный config header.

### 17.3. Future negotiation

Negotiation относится к будущему session layer.

Session layer должен будет обновлять limits codec/reassembler на основе согласованных параметров SNRM/UA.

Codec не должен сам выполнять negotiation.

---

## 18. Segmentation

Segmentation поддерживается полностью, но реализуется отдельным компонентом:

```text
hdlc_segmentation
```

Низкоуровневый `DecodeFrame` должен просто возвращать один валидный frame. Сборка segmented payload — задача reassembler.

---

## 19. Encoder-side segmentation

### 19.1. API

```cpp
struct HdlcSegmentationOptions
{
  std::size_t maximumInformationFieldSize;
};

class HdlcSegmenter
{
public:
  explicit HdlcSegmenter(const HdlcSegmentationOptions& options);

  HdlcStatus SegmentInformation(
    const HdlcFrame& baseFrame,
    const std::uint8_t* information,
    std::size_t informationSize,
    std::vector<HdlcFrameBuffer>& outputFrames);
};
```

### 19.2. Поведение

```text
если payload помещается в один frame:
  segmented = false

если payload не помещается:
  все промежуточные frames:
    segmented = true

  последний frame:
    segmented = false
```

---

## 20. Decoder-side reassembly

### 20.1. API

```cpp
class HdlcReassembler
{
public:
  HdlcReassembler();

  HdlcStatus PushFrame(
    const HdlcFrameBuffer& frame,
    HdlcFrameBuffer& completedFrame,
    bool& hasCompletedFrame);

  void Reset();
};
```

### 20.2. Поведение

```text
segmented = true:
  сохранить information
  вернуть SegmentationIncomplete

segmented = false после накопленных частей:
  добавить information
  вернуть completedFrame

segmented = false без накопленных частей:
  вернуть frame как completed
```

### 20.3. Проверки reassembler

Reassembler должен проверять:

```text
source address не изменился
destination address не изменился
frame type совместим
накопленный размер не превышает limit
нельзя начать новую segmented sequence до завершения старой
```

Sequence validation полноценно относится к session layer. Reassembler не должен превращаться в session state machine.

---

## 21. C ABI

### 21.1. Требования

C ABI должен быть отдельным слоем поверх C++.

Файлы:

```text
include/dlms/hdlc/hdlc_c_api.h
src/hdlc/hdlc_c_api.cpp
```

### 21.2. Принципы C ABI

```text
extern "C"
никаких C++ типов в ABI
никаких exceptions
только opaque handles
только фиксированные integer-типы
caller-provided buffers
стабильные enum values
```

### 21.3. Пример C status enum

```c
typedef enum dlms_hdlc_status_t
{
  DLMS_HDLC_STATUS_OK = 0,
  DLMS_HDLC_STATUS_NEED_MORE_DATA = 1,
  DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL = 2,
  DLMS_HDLC_STATUS_INVALID_ARGUMENT = 3,
  DLMS_HDLC_STATUS_INVALID_FRAME_FORMAT = 4,
  DLMS_HDLC_STATUS_INVALID_FRAME_LENGTH = 5,
  DLMS_HDLC_STATUS_INVALID_ADDRESS = 6,
  DLMS_HDLC_STATUS_INVALID_CONTROL_FIELD = 7,
  DLMS_HDLC_STATUS_INVALID_HEADER_CHECKSUM = 8,
  DLMS_HDLC_STATUS_INVALID_FRAME_CHECKSUM = 9,
  DLMS_HDLC_STATUS_FRAME_TOO_LARGE = 10,
  DLMS_HDLC_STATUS_SEGMENTATION_ERROR = 11
} dlms_hdlc_status_t;
```

### 21.4. Opaque handles

```c
typedef struct dlms_hdlc_decoder_t dlms_hdlc_decoder_t;
typedef struct dlms_hdlc_reassembler_t dlms_hdlc_reassembler_t;
```

### 21.5. C API examples

```c
dlms_hdlc_status_t dlms_hdlc_encode_frame(
  const dlms_hdlc_frame_t* frame,
  uint8_t* output,
  size_t output_size,
  size_t* written_size);
```

```c
dlms_hdlc_status_t dlms_hdlc_decode_frame(
  const uint8_t* input,
  size_t input_size,
  dlms_hdlc_frame_t* frame,
  uint8_t* information_buffer,
  size_t information_buffer_size,
  size_t* information_size);
```

---

## 22. Требования к будущему HDLC session layer

В v1 session не реализуется, но codec обязан быть готов к нему.

Будущий `hdlc_session` должен будет отвечать за:

```text
client/server role
SNRM generation
UA parsing
negotiated max information field length
window size
I-frame send sequence N(S)
I-frame receive sequence N(R)
Poll/Final handling
RR/RNR
DISC/UA close
timeouts
retransmission
duplicate frame handling
segmentation usage policy
```

Codec не должен:

```text
сам увеличивать sequence numbers
сам решать, когда слать RR
сам выполнять retransmission
сам знать про timeout
сам открывать/закрывать соединение
сам выполнять SNRM/UA negotiation
```

---

## 23. CMake

```cmake
cmake_minimum_required(VERSION 3.16)

project(dlms_cpp
  VERSION 0.1.0
  LANGUAGES C CXX)

option(DLMS_BUILD_TESTS "Build tests" ON)
option(DLMS_BUILD_C_API "Build C ABI wrapper" ON)
option(DLMS_USE_SYSTEM_GTEST "Use system GoogleTest" OFF)

add_library(dlms_hdlc
  src/hdlc/hdlc_address.cpp
  src/hdlc/hdlc_control.cpp
  src/hdlc/hdlc_crc.cpp
  src/hdlc/hdlc_codec.cpp
  src/hdlc/hdlc_stream_decoder.cpp
  src/hdlc/hdlc_segmentation.cpp)

target_compile_features(dlms_hdlc PUBLIC cxx_std_11)

target_include_directories(dlms_hdlc
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(DLMS_BUILD_C_API)
  target_sources(dlms_hdlc
    PRIVATE
      src/hdlc/hdlc_c_api.cpp)
endif()

if(DLMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
```

---

## 24. GoogleTest integration

### 24.1. Рассмотренные варианты

| Вариант | Плюс | Минус | Риск |
|---|---|---|---|
| system package | просто | не везде есть | CI/offline ломается |
| FetchContent | удобно | нужен интернет | offline ломается |
| vendored gtest | воспроизводимо | больше репозиторий | надо обновлять вручную |

### 24.2. Выбор

Базовый вариант:

```text
по умолчанию FetchContent
при DLMS_USE_SYSTEM_GTEST=ON использовать системный GoogleTest
```

Для offline-среды можно позже добавить vendored-режим.

---

## 25. Тестовая стратегия

### 25.1. CRC tests

```text
CalculateHdlcCrc_empty
CalculateHdlcCrc_knownSnrm
CalculateHdlcCrc_knownUa
ValidateHcs_valid
ValidateHcs_invalid
ValidateFcs_valid
ValidateFcs_invalid
```

### 25.2. Address tests

```text
DecodeAddress_oneByte
DecodeAddress_twoBytes
DecodeAddress_fourBytes
DecodeAddress_truncated
DecodeAddress_invalidExtensionBit
EncodeAddress_oneByte
EncodeAddress_twoBytes
EncodeAddress_fourBytes
MakeClientAddress_valid
MakeServerAddress_logicalPhysical
```

### 25.3. Control tests

```text
DecodeControl_iFrame
DecodeControl_rr
DecodeControl_rnr
DecodeControl_rej
DecodeControl_srej
DecodeControl_snrm
DecodeControl_ua
DecodeControl_disc
DecodeControl_dm
DecodeControl_frmr
DecodeControl_ui
EncodeControl_roundtrip
```

### 25.4. Frame codec tests

```text
EncodeFrame_snrm
EncodeFrame_ua
EncodeFrame_disc
EncodeFrame_rr
EncodeFrame_iFrameWithoutSegmentation
EncodeFrame_iFrameWithSegmentation
DecodeFrame_validSnrm
DecodeFrame_validUa
DecodeFrame_validIFrame
DecodeFrame_invalidFormatType
DecodeFrame_invalidLength
DecodeFrame_invalidHcs
DecodeFrame_invalidFcs
DecodeFrame_payloadContainsFlagByte
DecodeFrame_missingClosingFlag
```

Особенно важные тесты:

```text
DecodeFrame_payloadContainsFlagByte
DecodeFrame_missingClosingFlag
```

Первый нужен потому, что byte stuffing не используется. Второй нужен потому, что closing flag обязателен всегда.

### 25.5. Stream decoder tests

```text
Push_fullFrame
Push_byteByByte
Push_multipleFrames
Push_frameWithPayloadFlagByte
Push_invalidLength
Push_frameTooLarge
Push_noiseBeforeFlag_ignorePolicy
Push_noiseBeforeFlag_errorPolicy
Push_missingClosingFlag
Push_resetAfterError
```

### 25.6. Segmentation tests

```text
SegmentInformation_singleFrame
SegmentInformation_multipleFrames
SegmentInformation_exactBoundary
Reassemble_singleFrame
Reassemble_multipleFrames
Reassemble_addressMismatch
Reassemble_controlMismatch
Reassemble_overflow
Reassemble_newSequenceBeforeCompletion
```

### 25.7. C API tests

```text
CApi_encodeFrame
CApi_decodeFrame
CApi_outputBufferTooSmall
CApi_streamDecoderCreateDestroy
CApi_reassemblerCreateDestroy
CApi_noCrashOnNullArguments
```

### 25.8. Real DLMS vector tests

```text
Vector_snrm
Vector_ua
Vector_disc
Vector_rr
Vector_iFrame_llcPayload
Vector_segmented_iFrames
```

---

## 26. Реализационные фазы

### Фаза 0. Документы требований

Результат:

```text
docs/00_hdlc_requirements.md
docs/01_hdlc_codec_api.md
docs/02_hdlc_c_api.md
docs/03_hdlc_segmentation.md
docs/04_hdlc_session_requirements.md
```

Критерий готовности:

```text
все спорные параметры явно зафиксированы
нет неявных решений в коде
```

---

### Фаза 1. Каркас проекта

Результат:

```text
CMakeLists.txt
include/dlms/hdlc/*.hpp
src/hdlc/*.cpp
test/CMakeLists.txt
```

Критерий готовности:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

---

### Фаза 2. Status/error model

Результат:

```text
hdlc_error.hpp
```

Критерий готовности:

```text
единый HdlcStatus
нет exceptions в публичном API
все функции имеют предсказуемый error path
```

---

### Фаза 3. CRC

Результат:

```text
hdlc_crc.hpp
hdlc_crc.cpp
test_hdlc_crc.cpp
```

Критерий готовности:

```text
known vectors проходят
bad HCS/FCS обнаруживаются
```

---

### Фаза 4. Address codec

Результат:

```text
hdlc_address.hpp
hdlc_address.cpp
test_hdlc_address.cpp
```

Критерий готовности:

```text
1/2/4-byte addresses проходят
client/server helpers проходят
ошибочные адреса отклоняются
```

---

### Фаза 5. Control codec

Результат:

```text
hdlc_control.hpp
hdlc_control.cpp
test_hdlc_control.cpp
```

Критерий готовности:

```text
I/S/U frames поддержаны
минимальный client/server набор поддержан
roundtrip проходит
```

---

### Фаза 6. Frame encoder

Результат:

```text
hdlc_codec.hpp
hdlc_codec.cpp
```

Критерий готовности:

```text
SNRM/UA/DISC/RR/I-frame кодируются
length считается корректно
HCS/FCS добавляются корректно
payload с 0x7E допускается
```

---

### Фаза 7. Frame decoder

Критерий готовности:

```text
valid frames декодируются
invalid format/length/HCS/FCS отклоняются
payload с 0x7E не ломает decoding
missing closing flag отклоняется
```

---

### Фаза 8. Stream decoder

Критерий готовности:

```text
decoder читает кадр по Frame Format.length
поддерживает chunked input
поддерживает несколько кадров подряд
не зависит от byte stuffing
поддерживает настраиваемую noise policy
требует closing flag
```

---

### Фаза 9. Segmentation/reassembly

Критерий готовности:

```text
большой information payload режется на frames
segmentation bit выставляется корректно
reassembler собирает исходный payload
ошибки последовательности обнаруживаются
limits соблюдаются
```

---

### Фаза 10. C ABI

Критерий готовности:

```text
C API компилируется C-компилятором
не содержит C++ типов
не бросает exceptions
покрыт тестами
использует caller-provided buffers
```

---

### Фаза 11. Real DLMS vectors

Критерий готовности:

```text
реальные SNRM/UA/I/RR/DISC hex-векторы проходят
codec совместим с DLMS/COSEM HDLC profile
```

---

### Фаза 12. Doxygen public API documentation

Критерий готовности:

```text
каждая функция публичного API документирована
каждый публичный метод документирован
каждое публичное поле структуры документировано
C ABI header самодокументируемый и компилируется C-компилятором
комментарии описывают ownership, limits, buffer policy и error statuses
```

---

## 27. Основные риски

### 27.1. Неправильная адресация

Контроль:

```text
отдельный address codec
реальные test vectors
client/server/gateway cases
```

### 27.2. Смешивание HDLC segmentation и xDLMS block transfer

Контроль:

```text
HDLC segmentation только в hdlc_segmentation
APDU block transfer только в будущем APDU layer
```

### 27.3. Ошибка в HCS/FCS

Контроль:

```text
known vectors
реальные DLMS frames
negative tests with one-byte corruption
```

### 27.4. Неправильная обработка `0x7E` внутри payload

Контроль:

```text
length-based decoder
обязательный test DecodeFrame_payloadContainsFlagByte
```

### 27.5. Слишком ранняя session logic

Контроль:

```text
codec не управляет sequence numbers
codec не выполняет retry
codec не знает timeout
codec не выполняет negotiation
```

---

## 28. Итоговый milestone v1

```text
M1: Portable C++11 DLMS/COSEM HDLC Type 3 Codec
```

Состав:

```text
CMake 3.16
C++11
no exceptions
status-code API
client + server frame support
HDLC type 3 format field
address codec
control codec
HCS/FCS
frame encode/decode
stream decode by Frame Format.length
payload with 0x7E support
mandatory closing flag
full segmentation/reassembly
configurable limits
future negotiation support boundary
stable C ABI
GoogleTest
real DLMS test vectors
Doxygen-documented public API
```

Не входит:

```text
HDLC session
transport
LLC
WRAPPER
APDU
timeouts
retransmission
negotiation implementation
```

---

## 29. Следующий практический шаг (v1 — выполнено)

Фазы 0–12 реализованы. Библиотека соответствует требованиям v1. Следующий
этап — реализация v2 session layer. См. раздел 30.

---

## 30. v2 Session Layer — план реализации

Источники требований: IEC 62056-46, Green Book Ed. 8.3, DLMS UA 1001-3
(Yellow Book). Детальные спецификации — в `docs/04_hdlc_session_requirements.md`
разделы 7–12.

---

### Фаза 13. SNRM/UA parameter negotiation

**Цель:** сессия корректно разбирает Information field SNRM/UA и применяет
согласованные параметры.

Критерий готовности:

```text
HdlcSnrmParameters — структура, описывающая параметры из info field
ParseSnrmParameters(data, size) — парсер TLV-структуры 81/80/...
EncodeSnrmParameters(params, output) — кодировщик для UA ответа
BuildConnectRequest — может включать info field с предложенными параметрами
BuildConnectResponse — включает согласованные параметры в UA
ReceiveFrame — парсит info field SNRM на стороне сервера и выбирает min()
Codec limits обновляются после обмена SNRM/UA
Тесты: Session_SnrmWithMaxInfoFieldLength_ServerNegotiates
       Session_SnrmWithWindowSize_ServerNegotiates
       Session_UaCarriesNegotiatedValues
       Session_CodecLimitsUpdatedAfterNegotiation
       Session_SnrmWithUnrecognisedParameter_ServerRejectsDm
Реальные векторы из Green Book §8.4.5.3.2 добавлены в test_hdlc_real_vectors
Conformance tests HDLC_NDM2NRM_P1 и HDLC_NDM2NRM_P2 проходят
```

---

### Фаза 14. Window size > 1 (sliding window)

**Цель:** сессия поддерживает window size 1–7 согласно согласованному значению.

Критерий готовности:

```text
V(A) — acknowledge state variable добавлен в HdlcSession
CanSendInformationFrame() — проверяет V(S) - V(A) < window_size
AcknowledgeSequence() — возвращает V(A)
BuildInformationFrame — блокируется когда окно заполнено
ReceiveSupervisoryFrame (RR) — обновляет V(A) до N(R)
ReceiveInformationFrame — обновляет V(A) через piggyback N(R)
Тесты: Session_WindowSize1_CannotSendSecondFrameBeforeRr
       Session_WindowSize3_CanSendThreeFramesBeforeRr
       Session_RrAdvancesAcknowledgeSequence
       Session_WindowExhaustedBlocksSend
```

---

### Фаза 15. User_Information в SNRM и DISC

**Цель:** верхний уровень (COSEM-OPEN/RELEASE) может передавать opaque payload
через Information field SNRM/DISC.

Критерий готовности:

```text
BuildConnectRequest(userInfo, size, output) — перегрузка с user_information
BuildDisconnectRequest(userInfo, size, output) — перегрузка с user_information
ReceiveFrame — возвращает user_information из SNRM/DISC caller-у
Тесты: Session_SnrmWithUserInformation_ServerReceivesPayload
       Session_DiscWithUserInformation_PeerReceivesPayload
v1 перегрузки без userInfo остаются рабочими
```

---

### Фаза 16. Обновление тестовых векторов

**Цель:** покрыть реальными байтами все новые сценарии сессии.

Критерий готовности:

```text
Добавить в test_hdlc_real_vectors.cpp:
  - SNRM с info field (negotiation params из Green Book §8.4.5.3.2)
  - UA с negotiated params
  - I-frame с N(S)=0, N(R)=0 в 3-frame window sequence
Все существующие векторы продолжают проходить
```

---

### Фаза 17. Conformance test gap check

**Цель:** проверить, что все тест-группы Yellow Book для HDLC layer проходят.

Критерий готовности:

```text
HDLC_FRAME_P     — проходит (v1)
HDLC_ADDRESS_P1  — проходит (v1)
HDLC_ADDRESS_N1  — проходит (v1)
HDLC_ADDRESS_N4  — проходит (v1)
HDLC_ADDRESS_N6  — проходит (v1)
HDLC_ADDRESS_N7  — проходит (v1)
HDLC_NDM2NRM_P1  — проходит (фаза 13)
HDLC_NDM2NRM_P2  — проходит (фаза 13+14)
HDLC_INFO_P1     — проходит (v1)
HDLC_INFO_N1     — проходит (v1)
HDLC_INFO_N2     — проходит (v1)
HDLC_INFO_N3     — проходит (v1)
```

---

## 31. Не входит в v2

```text
timeouts (принадлежат транспортному адаптеру или приложению)
retransmission scheduling
полное duplicate frame detection
LLC codec
WRAPPER codec
APDU codec
security/ciphering
```

---

## 32. Milestone v2

```text
M2: DLMS/COSEM HDLC Session Layer — conformance-ready
```

Состав:

```text
SNRM/UA parameter negotiation (max_info, window_size)
Sliding window (window_size 1–7)
User_Information passthrough в SNRM/DISC
Обновлённые тестовые векторы с negotiation examples
Все HDLC conformance test groups из Yellow Book покрыты
```
