# ScriptExecutorApp

## Overview

ScriptExecutorApp is a desktop application developed for the SD Customization department.

The goal of this application is to provide customers with an intuitive user interface that allows them to connect to KNAPP warehouse servers and execute testing scripts remotely. Since customers do not have direct access to these servers by default, the application acts as a secure bridge for script execution and test automation.

## Features

- Connect to KNAPP warehouse servers
- Execute predefined testing scripts
- Intuitive and user-friendly interface
- Built with Qt Quick (QML) and C++
- Cross-platform architecture

## Dependencies

### Build Dependencies

- CMake >= 3.21
- C++17 compatible compiler
- Qt 6

Required Qt modules:

- Qt6 Core
- Qt6 Gui
- Qt6 Qml
- Qt6 Quick
- Qt6 Quick Controls 2

### Ubuntu/Debian

```bash
cd scripts/

chmod +x dependencyInstaller.sh

./dependencyInstaller.sh
```
(in case of any missing dependencies in the script please update it)

## Contact

In case of any further questions, feel free to contact: Alexander Auer