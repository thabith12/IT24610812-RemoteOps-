# RemoteOps Prompt Log

This document records the main types of prompts used during the development of the RemoteOps assignment. AI assistance was used for explanation, debugging guidance, documentation support, and understanding networking concepts. The final implementation was tested and verified by the developer.

## Prompt 1 – Assignment Understanding

Asked for an explanation of the RemoteOps assignment requirements, including Agent, Controller, TCP, UDP, authentication, file transfer, monitoring, concurrency, and logging.

## Prompt 2 – TCP/IP Implementation

Asked for guidance on implementing TCP socket communication between the Agent and Controller in C.

## Prompt 3 – Authentication

Asked for help understanding and implementing token-based authentication before allowing other commands.

## Prompt 4 – EXEC Whitelist

Asked how to restrict the EXEC command to the required commands:
DATE, UPTIME, DISKFREE, HOSTNAME, and WHOAMI.

## Prompt 5 – File Transfer

Asked for guidance on implementing PUT and GET operations with exact byte counts and personalised file storage.

## Prompt 6 – UDP Monitoring

Asked for help implementing periodic UDP monitoring messages containing CPU usage, memory usage, uptime, and Session ID.

## Prompt 7 – Concurrent Controllers

Asked for an explanation of thread-per-Controller concurrency and how to test multiple simultaneous TCP connections.

## Prompt 8 – TCP Stream Handling

Asked for help handling TCP command boundaries correctly instead of assuming one recv() call contains one complete command.

## Prompt 9 – Testing and Validation

Asked for guidance on validating file integrity using `cmp` and SHA-256 and checking the required RemoteOps functionality.

## Prompt 10 – Documentation

Asked for assistance in organising the technical report, testing evidence, Git development history, and conclusion.

## AI Usage Statement

AI assistance was used as a supporting tool for understanding concepts, debugging approaches, documentation, and testing ideas. The implementation was compiled, executed, tested, and verified manually in the development environment.
