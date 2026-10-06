# RemoteOps Design Diary

## Project Overview

RemoteOps is a remote system monitoring and management tool developed in C using TCP/IP socket programming. The Agent acts as the server and the Controller acts as the client.

## Development Progress

### Step 1 – Basic TCP Communication
The project was initially created with a basic TCP client-server communication model. The Agent listens on the personalised TCP port 9461 and accepts Controller connections.

### Step 2 – Authentication
Token-based authentication was implemented using the personalised token `OPS-0812`. The Controller must authenticate before accessing other commands.

### Step 3 – System and Process Information
The `SYSINFO` command was implemented to provide CPU usage, memory usage, uptime, and Session ID information. `LISTPROC` was then added to provide a snapshot of running processes.

### Step 4 – Restricted Command Execution
The `EXEC` command was implemented with a whitelist containing only:
- DATE
- UPTIME
- DISKFREE
- HOSTNAME
- WHOAMI

Other commands such as `EXEC ls` are rejected.

### Step 5 – File Transfer
PUT and GET functionality was implemented for uploading and downloading files. Uploaded files are stored in the personalised directory:

`./agentfiles/IT24610812/`

Filename validation and a maximum file size of 10 MB were also added.

### Step 6 – UDP Monitoring
UDP monitoring was implemented using `MONITOR START` and `MONITOR STOP`.
 Monitoring reports include CPU usage, memory usage, uptime, and SID.

### Step 7 – Logging
Timestamped logging was added to record Controller connections,
 authentication, commands, file transfers, monitoring operations, and disconnections.

### Step 8 – Concurrency
A thread-per-Controller model using POSIX threads was implemented.
 This allows multiple Controllers to connect to the Agent independently.
The implementation was tested using five simultaneous Controller connections.

### Step 9 – TCP Stream Handling
TCP command reception was improved by implementing newline-based command framing. 
This avoids assuming that one `recv()` call always contains exactly one complete command.

### Step 10 – Optional Extension
PUT and GET throughput measurement was added using timing calculations.
 File integrity was also verified using `cmp` and SHA-256 hashing.

## Final Design Decisions

TCP was selected for reliable command and file transfer, while UDP was selected for periodic monitoring.
 POSIX threads were selected for concurrent Controller handling.
 A command whitelist was used to reduce security risks, and personalised storage was used to satisfy the assignment requirements.
