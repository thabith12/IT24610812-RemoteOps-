# RemoteOps – Remote System Monitoring and Management Tool

## 1. Project Overview

RemoteOps is a remote system monitoring and management tool developed in C using TCP/IP socket programming.

The system consists of:

* **Agent** – TCP server that runs on the remote system.
* **Controller** – TCP client used to communicate with the Agent.
* **UDP Monitoring** – Periodically sends system monitoring information to the Controller.

## 2. Personalised Configuration

| Item                 | Value                      |
| -------------------- | -------------------------- |
| Registration Number  | IT24610812                 |
| TCP Port             | 9461                       |
| UDP Monitoring Port  | 9462                       |
| Source File Suffix   | 812                        |
| Session ID           | 2180                       |
| Authentication Token | OPS-0812                   |
| Storage Directory    | `./agentfiles/IT24610812/` |
| Log File             | `remoteops_812.log`        |

## 3. Main Features

* Token-based authentication
* System information retrieval
* Running process listing
* Restricted command execution
* File upload using PUT
* File download using GET
* UDP-based system monitoring
* Timestamped logging
* Graceful Controller disconnection
* Multiple simultaneous Controller connections using POSIX threads
* PUT/GET throughput measurement
* File integrity verification using `cmp` and SHA-256

## 4. Allowed EXEC Commands

Only the following commands are permitted:

```text
DATE
UPTIME
DISKFREE
HOSTNAME
WHOAMI
```

Other commands are rejected by the Agent.

## 5. Build Instructions

Compile the project using the provided Makefile:

```bash
make -f Makefile_812 clean
make -f Makefile_812
```

Or compile manually:

```bash
gcc -Wall -Wextra -pthread -o agent_812 agent_812.c
gcc -Wall -Wextra -pthread -o controller_812 controller_812.c
```

## 6. Running the Application

### Start the Agent

```bash
./agent_812
```

The Agent listens for TCP connections on port `9461`.

### Start the Controller

Open another terminal and run:

```bash
./controller_812
```

The Controller connects to the Agent and authenticates using:

```text
OPS-0812
```

## 7. File Transfer

Uploaded files are stored in:

```text
./agentfiles/IT24610812/
```

PUT and GET operations use exact byte counts to maintain file integrity.

File transfers were verified using:

```bash
cmp original_file transferred_file
```

and:

```bash
sha256sum original_file transferred_file
```

## 8. UDP Monitoring

The Controller can start monitoring using:

```text
MONITOR START
```

The Agent periodically sends:

```text
CPU
MEM
UPTIME
SID
```

Monitoring can be stopped using:

```text
MONITOR STOP
```

## 9. Concurrency

The Agent uses a **thread-per-Controller** concurrency model implemented using POSIX threads.

The implementation was tested with five simultaneous Controller connections, with each Controller successfully establishing and authenticating a TCP session.

## 10. Logging

Runtime activities are recorded in:

```text
remoteops_812.log
```

The log records connections, authentication, commands, file transfers, monitoring operations, and disconnections with timestamps.

## 11. Testing

The final implementation was tested for:

* TCP connectivity
* Authentication
* SYSINFO
* LISTPROC
* All permitted EXEC commands
* Rejection of invalid EXEC commands
* PUT and GET file transfer
* File integrity using `cmp` and SHA-256
* UDP monitoring
* MONITOR STOP
* Logging
* Graceful disconnection
* Five simultaneous Controller connections
* PUT/GET throughput measurement

## 12. Development

The project was developed incrementally using Git with descriptive commits for the major implementation stages, including authentication, SYSINFO, LISTPROC, EXEC, GET, UDP monitoring, logging, PUT, TCP stream handling, Makefile creation, and throughput measurement.

## 13. Project Files

```text
RemoteOps/
├── agent_812.c
├── controller_812.c
├── Makefile_812
├── design_diary.md
├── prompt_log.md
├── reflection.md
├── agentfiles/
│   └── IT24610812/
└── remoteops_812.log
```
