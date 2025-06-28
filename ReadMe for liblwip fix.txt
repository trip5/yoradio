from https://4pda.to/forum/index.php?showtopic=1010378&st=17940#entry132965307 :

Для тех, кто хочет слушать еРадио с aac320, flac без рывков
(проблемы и начальные фиксы были описаны здесь liblwip fix
Скомпилирована новая версия фиксов библиотек esp-idf v5.1 которые используется в Ардуино с ядром 3.0.5 ( работает с 3.0.X ядрами)

Я проверял компиляцию на последнем MOD от Maleksm
Спасибо Maleksm за тесты на ESP32S3 , ESP32 WROOM/WROVER
.

Для использования новых библиотек в Arduino с ядром 3.0.5, надо взять из liblwip_20241019_3.0.5.zip liblwip.a и libesp_netif.a и заменить ими в директории:
Для ESP32S3:
C:\Users\User\AppData\Local\Arduino15\packages\esp32\tools\esp32-arduino-libs\idf-release_v5.1-33fbade6\esp32s3\lib\

Для ESP32:
C:\Users\User\AppData\Local\Arduino15\packages\esp32\tools\esp32-arduino-libs\idf-release_v5.1-33fbade6\esp32\lib\

(директорий idf-release_v5.1-33fbade6 может у вас отличаться )

Если у вас ядро 2.0.17 - используйте первую версию liblw_fix.zip из liblwip_fix

---

For those who want to listen to eRadio with aac320, flac without jerks
(problems and initial fixes are described here liblwip fix
A new version of the esp-idf v5.1 fix libraries has been compiled, used in Arduino with 3.0.5 cores (works with 3.0.X cores)

I checked the compilation of the latest mod from Maleksm
Thanks to Maleksm for tests on ESP32S3, ESP32 WROOM/WROVER
.

Arduino with 3.0.5 cores, you need to take liblwip.a and libesp_netif.a from liblwip_20241019_3.0.5.zip and replace them in the directory:
For ESP32S3:
C:\Users\User\AppData\Local\Arduino15\packages\esp32\tools\esp32-arduino-libs\idf-release_v5.1-33fbade6\esp32s3\lib\

For ESP32:
C:\Users\User\AppData\Local\Arduino15\packages\esp32\tools\esp32-arduino-libs\idf-release_v5.1-33fbade6\esp32\lib\

(the idf-release_v5.1-33fbade6 directory may support you)

If you have 2.0.17 - feed the first version of liblw_fix.zip from liblwip_fix

---

Should allow higher bitrate streams to play without warbling when compiling with Arduino IDE

Trip5 Note:

Mixed results in Platformio on Core 5.2 compiling for ESP32-S3

May cause a failure in ESPFileUpdater that looks like:

[194675][E][WiFiClient.cpp:282] connect(): Setsockopt 'SO_SNDTIMEO'' on fd 58 failed. errno: 22, "Invalid argument"

Then either restore the original files or delete the platform folder and re-download.  You can try the fix again or not... not sure if it has any effect.