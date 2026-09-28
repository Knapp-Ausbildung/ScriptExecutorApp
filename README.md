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

## Design Language

- Member Variables
  Make sure to mark them with "m_name".
  This makes it easier to be distinguished from local variables.
  Example:
  ```cpp
  class Player
  {
  private:
  string m_name;
  int m_score;
  };
  ```
  
- Memory Management
  In general, try to prefer stack allocation.
  It is up to the developer if he wants to use references or pointers, but it is recommended to use Smart Pointers to prevent memory leaks.
  Also make sure to use "ptr" in the name of pointers.
  Example:
  ```cpp
  int *ptr = new int(10);
  std::unique_ptr<Player> ptrPlayer = std::make_unique<Player>("Name", "100");
  ```

- Naming conventions
  Classes and Structs use PascalCase: z.B. LoginService 
  Method Names and local variables use camelCase: z.B. currentValue, sshKey
  Constants are all caps: z.B. SSH_PORT

- Use const when appropriate
  Mark parameters or the return values of methods as constants to make it obvious they are not meant to be modified in any way.
  Example:
    ```cpp
  void printSession(const Session& session):
  std::string& getSesssionName() const;
  ```

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
