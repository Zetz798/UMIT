# Исправление Android-сборки на Windows, 6 октября 2026

Сбой происходил до сборки кода проекта. В цепочке CMake/Ninja/cmd/NDK
путь с `Админ` обрабатывался некорректно; отдельно qmlimportscanner не мог
прочитать QML из этого пути. Прямой вызов компилятора из PowerShell работал,
но это не исправляло сборку через CMake. Замена Ninja проблему не устранила.

Созданы копии, оригинальные папки не удалены:

```text
Проект: C:\Dev\ticket-mvp
SDK:    C:\Android\Sdk
NDK:    C:\Android\Sdk\ndk\27.2.12479018
Сборка: C:\Dev\ticket-mvp-build
```

Именно копия в `C:\Dev\ticket-mvp` используется для дальнейшей Android-сборки.
Не редактируй параллельно обе копии: они не синхронизируются автоматически.

В Qt Creator:

1. Открыть Preferences → SDKs → Android. В некоторых версиях раздел
   называется Devices → Android.
2. Установить Android SDK location в `C:\Android\Sdk`. Проверить, что NDK
   27.2.12479018 и компилятор в Android kit тоже указывают в эту папку.
3. Закрыть старый проект и открыть `C:\Dev\ticket-mvp\CMakeLists.txt`.
4. Для Redmi A3 выбрать **Qt 6.12.0 for Android armeabi-v7a / armv7** и Debug.
   Проверенная папка: `C:\Dev\ticket-mvp\build\android-armv7-debug`.
   Kit arm64-v8a не подходит для 32-битной системы этого телефона.
   Старую папку сборки в Documents не использовать: в кеше остались старые пути.
5. Установить CMake-параметры:

   ```text
   TICKET_BUILD_MOBILE=ON
   TICKET_BUILD_CLI=OFF
   BUILD_TESTING=OFF
   ```

6. Configure Project, затем собрать `ticket_mobile` / Build Android APK.

[Официальная настройка Android в Qt Creator](https://doc.qt.io/qtcreator/creator-how-to-set-android-preferences.html).

Проверено: конфигурация CMake и компиляция `ticket_mobile` завершились с
кодом 0. Получен `C:\Dev\ticket-mvp-build\mobile\libticket_mobile_arm64-v8a.so`.
В автоматически сгенерированном Qt-коде было одно предупреждение о variadic
macro; оно не остановило сборку. Теперь также упакован 32-битный APK с Phantom
`../apk/ticket-mvp-redmi-a3-phantom-debug.apk`. Подпись проверена, приложение на
телефоне ещё не запускалось. Переустановка Qt/NDK и переименование Windows-пользователя
для устранения этой ошибки не потребовались.

Если Gradle сообщает `Unable to establish loopback connection`, в окружении
сборки используйте существующий JDK 21 и оба параметра временного каталога:

```powershell
$env:JAVA_HOME='C:\Users\Админ\AppData\Local\Programs\Eclipse Adoptium\jdk-21.0.12.101-hotspot'
$env:JAVA_TOOL_OPTIONS='-Djava.io.tmpdir=C:/Dev/ticket-mvp/build/android-armv7-debug/tmp -Djdk.net.unixdomain.tmpdir=C:/Dev/ticket-mvp/build/android-armv7-debug/tmp'
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build C:/Dev/ticket-mvp/build/android-armv7-debug --target apk --parallel 4
```

Каталог `tmp` должен существовать. Эти переменные действуют в текущем процессе
PowerShell; глобальные настройки Java не менялись.
